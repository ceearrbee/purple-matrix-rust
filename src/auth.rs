use std::ffi::{CStr, CString};
use std::io::Write;
use std::os::raw::c_char;
#[cfg(unix)]
use std::os::unix::fs::OpenOptionsExt;
use std::path::PathBuf;
use std::sync::Mutex;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use matrix_sdk::Client;
use crate::ffi::{CONNECTED_CALLBACK, LOGIN_FAILED_CALLBACK, SSO_CALLBACK};
use crate::{RUNTIME, DATA_PATH, CLIENTS};
use crate::sync_logic;

static SSO_IN_PROGRESS: Mutex<bool> = Mutex::new(false);
const SSO_CALLBACK_TIMEOUT_SECS: u64 = 180;

fn set_sso_in_progress(value: bool) {
    let mut guard = SSO_IN_PROGRESS.lock().unwrap();
    *guard = value;
}

fn try_begin_sso() -> bool {
    let mut guard = SSO_IN_PROGRESS.lock().unwrap();
    if *guard {
        return false;
    }
    *guard = true;
    true
}

fn extract_login_token(input: &str) -> Option<String> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return None;
    }
    if trimmed.starts_with("http://")
        || trimmed.starts_with("https://")
        || trimmed.contains('?')
        || trimmed.contains("loginToken=")
    {
        extract_query_param(trimmed, "loginToken")
    } else {
        // Backward compatibility: allow raw token pasted directly.
        if trimmed.chars().any(|c| c.is_whitespace()) {
            None
        } else {
            Some(trimmed.to_string())
        }
    }
}

fn extract_query_param(input: &str, key: &str) -> Option<String> {
    let query = input.split('?').nth(1).unwrap_or(input);
    for (k, v) in url::form_urlencoded::parse(query.as_bytes()) {
        if k == key && !v.is_empty() {
            return Some(v.into_owned());
        }
    }
    None
}

fn generate_sso_state() -> String {
    let nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .unwrap_or_default()
        .as_nanos();
    format!("{:x}{:x}", nanos, std::process::id())
}

#[no_mangle]
async fn finish_sso_internal(client: Client, token: String) {
    log::info!("Finishing SSO login with token...");
    match client.matrix_auth().login_token(&token).initial_device_display_name("Pidgin (Rust)").await {
        Ok(_) => {
            log::info!("SSO Login Successful! Persisting session...");
            set_sso_in_progress(false);
            finish_login_success(client).await;
        },
        Err(e) => {
            let err_msg = format!("SSO Login Failed: {:?}", e);
            log::error!("{}", err_msg);
            
            if err_msg.contains("Mismatched") || err_msg.contains("Session") {
                log::warn!("SSO mismatch detected. Wiping data and restarting SSO flow...");
                
                // 1. Preserve config
                let hs_url = client.homeserver().to_string();
                
                // 2. Drop Client - Implicitly dropped as we overwrite `client` or just ensure we don't use it.
                // We need to drop the client instance to release DB lock if it holds it.
                // `client` logic in `perform_login` retry handled this.
                drop(client); 
                
                // 3. Wipe Data
                let path_opt = {
                        let guard = DATA_PATH.lock().unwrap();
                        guard.clone()
                };
                
                if let Some(path) = path_opt {
                    log::info!("Deleting data directory: {:?}", path);
                    safe_wipe_data_dir(&path);
                    let _ = std::fs::create_dir_all(&path);
                    
                    // 4. Rebuild Client
                    match build_client(&hs_url, &path).await {
                        Ok(new_client) => {
                            // Login tokens are one-time; after a mismatched-account
                            // attempt, retrying with the same token can fail with
                            // "Invalid login token". Start a fresh SSO round.
                            log::info!("Client rebuilt. Requesting a fresh SSO token...");
                            set_sso_in_progress(false);
                            start_sso_flow(new_client);
                        },
                        Err(e_build) => {
                            set_sso_in_progress(false);
                            log::error!("Failed to rebuild client for retry: {:?}", e_build);
                            report_login_failure(format!("Failed to rebuild client during SSO recovery: {:?}", e_build));
                        }
                    }
                }
            } else {
                set_sso_in_progress(false);
                // Notify user of non-mismatch error
                let guard = LOGIN_FAILED_CALLBACK.lock().unwrap();
                if let Some(cb) = *guard {
                    let c_err = CString::new(err_msg).unwrap_or_default();
                    cb(c_err.as_ptr());
                }
            }
        }
    }
}
#[no_mangle]
pub extern "C" fn purple_matrix_rust_login(
    username: *const c_char,
    password: *const c_char,
    homeserver: *const c_char,
    data_dir: *const c_char,
) -> i32 {
    if username.is_null() || homeserver.is_null() || data_dir.is_null() {
        log::error!("Login failed: One or more arguments are NULL");
        return 0;
    }

    let username = unsafe { CStr::from_ptr(username).to_string_lossy().into_owned() };
    let password = if password.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(password).to_string_lossy().into_owned() }
    };
    let homeserver = unsafe { CStr::from_ptr(homeserver).to_string_lossy().into_owned() };
    let data_dir = unsafe { CStr::from_ptr(data_dir).to_string_lossy().into_owned() };

    log::info!("Attempting login for {} at {}", username, homeserver);
    
    // Spawn the login process on the runtime to avoid blocking the UI
    // We return '2' (Pending) immediately.
    RUNTIME.spawn(async move {
        perform_login(username, password, homeserver, data_dir).await;
    });

    return 2; // Pending
}

// Helper to build client to reduce code duplication
async fn build_client(homeserver: &str, data_path: &PathBuf) -> Result<Client, matrix_sdk::ClientBuildError> {
    let client_builder = Client::builder();
    let builder = if homeserver.starts_with("http") {
          if let Ok(url) = url::Url::parse(homeserver) {
              if let Some(host) = url.host_str() {
                  if url.port().is_none() && (url.path() == "/" || url.path().is_empty()) {
                      match matrix_sdk::ruma::ServerName::parse(host) {
                          Ok(sn) => client_builder.server_name(&sn),
                          Err(_) => client_builder.homeserver_url(homeserver)
                      }
                  } else {
                      client_builder.homeserver_url(homeserver)
                  }
              } else {
                  client_builder.homeserver_url(homeserver)
              }
          } else {
              client_builder.homeserver_url(homeserver)
          }
    } else {
         match matrix_sdk::ruma::ServerName::parse(homeserver) {
             Ok(sn) => client_builder.server_name(&sn),
             Err(_) => client_builder.homeserver_url(homeserver)
         }
    };
    
    // Secure by default. Allow explicit local override for debugging.
    let insecure_ssl = std::env::var("MATRIX_RUST_INSECURE_SSL")
        .map(|v| matches!(v.as_str(), "1" | "true" | "TRUE" | "yes" | "YES"))
        .unwrap_or(false);
    let builder = if insecure_ssl {
        log::warn!("MATRIX_RUST_INSECURE_SSL enabled: TLS certificate verification is disabled");
        builder.disable_ssl_verification()
    } else {
        builder
    };
    
    builder.sqlite_store(data_path, None).build().await
}

fn get_keyring_entry(username: &str) -> keyring::Entry {
    keyring::Entry::new("purple-matrix-rust", username).unwrap()
}

fn write_session_json_secure(path: &std::path::Path, json: &str, username: &str) -> std::io::Result<()> {
    // 1. Save to keyring
    let entry = get_keyring_entry(username);
    if let Err(e) = entry.set_password(json) {
        log::warn!("Failed to save session to keyring: {:?}", e);
    } else {
        log::info!("Session saved securely to keyring.");
    }

    // 2. Save a dummy/metadata file to the data dir so we know a session exists
    let mut opts = std::fs::OpenOptions::new();
    opts.write(true).create(true).truncate(true);
    #[cfg(unix)]
    {
        opts.mode(0o600);
    }
    let mut file = opts.open(path)?;
    file.write_all(b"{\"keyring\": true}")
}

fn load_session_json_secure(path: &std::path::Path, username: &str) -> Option<String> {
    // 1. Try to load from keyring
    let entry = get_keyring_entry(username);
    if let Ok(json) = entry.get_password() {
        log::info!("Session loaded from keyring.");
        return Some(json);
    }

    // 2. Fallback to file (for migration or if keyring failed)
    if path.exists() {
        if let Ok(json) = std::fs::read_to_string(path) {
            if !json.contains("\"keyring\": true") {
                 return Some(json);
            }
        }
    }
    None
}

fn safe_wipe_data_dir(path: &std::path::Path) {
    let path_str = path.to_string_lossy();
    // Safety check: Ensure we are only wiping directories that look like our expected data path.
    // It must contain 'matrix_rust_data' and must not be a top-level system directory.
    if path_str.contains("matrix_rust_data") && path.components().count() > 3 {
        log::warn!("Safety wipe: Deleting directory {:?}", path);
        let _ = std::fs::remove_dir_all(path);
    } else {
        log::error!("Safety wipe BLOCKED: Path {:?} looks suspicious or too high-level.", path);
    }
}

async fn perform_login(username: String, password: String, homeserver: String, data_dir: String) {
    let data_path = PathBuf::from(&data_dir);
    
    // update global data path
    {
        let mut guard = DATA_PATH.lock().unwrap();
        *guard = Some(data_path.clone());
    }

    if !data_path.exists() {
        let _ = std::fs::create_dir_all(&data_path);
    }

    log::info!("Initializing Store at {:?}", data_path);

    // 1. Build Client
    let client = match build_client(&homeserver, &data_path).await {
        Ok(c) => c,
        Err(e) => {
             log::error!("Failed to build client: {:?}. Wiping and retrying...", e);
             safe_wipe_data_dir(&data_path);
             let _ = std::fs::create_dir_all(&data_path);
             match build_client(&homeserver, &data_path).await {
                 Ok(c2) => c2,
                 Err(e2) => {
                     report_login_failure(format!("Client build failed after retry: {:?}", e2));
                     return;
                 }
             }
        }
    };

    log::info!("Client built successfully. Homeserver: {}", client.homeserver());

    // 2. Check existing session
    // Attempt to load from secure storage
    {
         let session_path = data_path.join("session.json");
         if let Some(json) = load_session_json_secure(&session_path, &username) {
              if let Ok(saved) = serde_json::from_str::<SavedSession>(&json) {
                       log::info!("Found saved session for {}, restoring...", saved.user_id);
                       
                       use matrix_sdk::authentication::matrix::MatrixSession;
                       use matrix_sdk::authentication::SessionTokens; 
                       use matrix_sdk::ruma::{UserId, DeviceId};
                       
                       if let Ok(u) = <&UserId>::try_from(saved.user_id.as_str()) {
                           let d = <&DeviceId>::from(saved.device_id.as_str()); {
                               let session = MatrixSession {
                                   meta: matrix_sdk::SessionMeta {
                                       user_id: u.to_owned(),
                                       device_id: d.to_owned(),
                                   },
                                   tokens: SessionTokens {
                                       access_token: saved.access_token,
                                       refresh_token: saved.refresh_token,
                                   },
                               };
                               
                               let load_settings = matrix_sdk::store::RoomLoadSettings::default(); 
                               
                               if let Err(e) = client.matrix_auth().restore_session(session, load_settings).await {
                                   log::error!("Failed to restore session: {:?}", e);
                                   // Don't delete metadata yet, maybe it was a transient error.
                               } else {
                                   log::info!("Session successfully restored!");
                                   finish_login_success(client).await;
                                   return;
                               }
                           }
                       }
              }
         }
    }

    if let Some(user) = client.user_id() {
         log::info!("Restored existing session for {}", user);
         finish_login_success(client).await;
         return;
    }

    log::warn!("No existing session found (user_id is None). Wiping potentially stale data to prevent MismatchedAccount errors...");
    if data_path.exists() {
         safe_wipe_data_dir(&data_path);
         let _ = std::fs::create_dir_all(&data_path);
         
         // Re-build client after wipe to ensure fresh state
         let client = match build_client(&homeserver, &data_path).await {
             Ok(c) => c,
             Err(e) => {
                 report_login_failure(format!("Failed to rebuild client after safety wipe: {:?}", e));
                 return;
             }
         };
         
         {
              let mut clients = crate::CLIENTS.lock().unwrap();
              clients.insert(username.clone(), client.clone());
         }
         
         proceed_with_login(client, username, password).await;
    } else {
         proceed_with_login(client, username, password).await;
    }
}

async fn proceed_with_login(client: Client, username: String, password: String) {
    if !password.is_empty() {
        log::info!("Attempting Password Login...");
        match client.matrix_auth().login_username(&username, &password).initial_device_display_name("Pidgin (Rust)").await {
            Ok(_) => {
                log::info!("Login Succeeded!");
                finish_login_success(client).await;
            },
            Err(e) => {
                report_login_failure(format!("Login failed: {:?}", e));
            }
        }
    } else {
        log::info!("Password empty, checking login discovery...");
        start_sso_flow(client);
    }
}

#[derive(serde::Serialize, serde::Deserialize)]
struct SavedSession {
    user_id: String,
    device_id: String,
    access_token: String,
    refresh_token: Option<String>,
}

async fn finish_login_success(client: Client) {
    let username = client.user_id().map(|u| u.to_string()).unwrap_or_default();
    
    // 1. Persist Session
    if let Some(session) = client.session() {
         let user_id = username.clone();
         let device_id = client.device_id().map(|d| d.to_string()).unwrap_or_default();
         let access_token = session.access_token().to_string();
         let refresh_token = session.get_refresh_token().map(|t: &str| t.to_string());

         let data = SavedSession {
             user_id,
             device_id,
             access_token, 
             refresh_token,
         };
         
         let path_opt = {
              let guard = DATA_PATH.lock().unwrap();
              guard.clone()
         };
         
         if let Some(mut path) = path_opt {
             path.push("session.json");
             if let Ok(json) = serde_json::to_string(&data) {
                 if let Err(e) = write_session_json_secure(&path, &json, &username) {
                     log::error!("Failed to save session securely: {:?}", e);
                 } else {
                     log::info!("Session successfully saved securely.");
                 }
             }
         }
    }

    {
         let mut clients = crate::CLIENTS.lock().unwrap();
         if let Some(user_id) = client.user_id() {
             clients.insert(user_id.to_string(), client.clone());
         }
    }
    {
         let guard = CONNECTED_CALLBACK.lock().unwrap();
         if let Some(cb) = *guard {
              cb();
         }
    }
    sync_logic::start_sync_loop(client).await;
}

fn report_login_failure(msg: String) {
    log::error!("{}", msg);
    let guard = LOGIN_FAILED_CALLBACK.lock().unwrap();
    if let Some(cb) = *guard {
        let c_err = CString::new(msg).unwrap_or_default();
        cb(c_err.as_ptr());
    }
}

fn start_sso_flow(client: Client) {
    if !try_begin_sso() {
        log::warn!("SSO flow requested while one is already in progress; ignoring duplicate request.");
        return;
    }

    let rt = tokio::runtime::Handle::current();
        
    rt.spawn_blocking(move || {
            // Helper to find free port
            let listener = match tiny_http::Server::http("127.0.0.1:0") {
                Ok(l) => l,
                Err(e) => {
                    set_sso_in_progress(false);
                    log::error!("Failed to start local HTTP server: {}", e);
                    report_login_failure(format!("Failed to start local SSO callback server: {}", e));
                    return;
                }
            };
            
            let port = match listener.server_addr().to_ip() {
                Some(addr) => addr.port(),
                None => {
                    set_sso_in_progress(false);
                    report_login_failure("Failed to bind local SSO callback listener to an IP address.".to_string());
                    return;
                }
            };
            let state = generate_sso_state();
            let redirect_url = format!("http://localhost:{}/login?state={}", port, state);
            log::info!("Listening for SSO callback on {}", redirect_url);

            // Native SSO URL Construction
            let sso_url = match RUNTIME.block_on(client.matrix_auth().get_sso_login_url(&redirect_url, None)) {
                Ok(u) => u,
                Err(e) => {
                    set_sso_in_progress(false);
                    log::error!("Failed to generate SSO URL: {:?}", e);
                    report_login_failure(format!("Failed to start SSO login: {:?}", e));
                    return;
                }
            };
            
            log::info!("SSO URL generated (Native): {}", sso_url);
            
            // Emit Callback
            {
                let guard = SSO_CALLBACK.lock().unwrap();
                if let Some(cb) = *guard {
                    let c_url = CString::new(sso_url.replace('\0', "")).unwrap_or_default();
                    cb(c_url.as_ptr());
                }
            }
            
            let deadline = Instant::now() + Duration::from_secs(SSO_CALLBACK_TIMEOUT_SECS);

            // Wait for callback requests until we get loginToken.
            loop {
                if Instant::now() >= deadline {
                    set_sso_in_progress(false);
                    report_login_failure(format!(
                        "SSO timed out after {} seconds. Please try login again.",
                        SSO_CALLBACK_TIMEOUT_SECS
                    ));
                    break;
                }

                match listener.recv_timeout(Duration::from_secs(1)) {
                    Ok(Some(request)) => {
                        let url_string = request.url().to_string();
                        if let Some(token) = extract_login_token(&url_string) {
                            let returned_state = extract_query_param(&url_string, "state");
                            if returned_state.as_deref() != Some(state.as_str()) {
                                let response = tiny_http::Response::from_string(
                                    "SSO verification failed (invalid state). Please retry login.");
                                let _ = request.respond(response);
                                continue;
                            }
                            log::info!("Captured SSO token from callback.");
                            let response = tiny_http::Response::from_string("Login successful! You can close this window.");
                            let _ = request.respond(response);
                            // Finish SSO on Runtime
                            RUNTIME.spawn(finish_sso_internal(client, token));
                            break;
                        } else {
                            let response = tiny_http::Response::from_string(
                                "Waiting for login completion. Please finish login in the original browser tab.");
                            let _ = request.respond(response);
                        }
                    }
                    Ok(None) => {
                        continue;
                    }
                    Err(e) => {
                        set_sso_in_progress(false);
                        report_login_failure(format!("SSO callback server failed: {}", e));
                        break;
                    }
                }
            }
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_finish_sso(token: *const c_char) {
    if token.is_null() {
        return;
    }
    let raw = unsafe { CStr::from_ptr(token).to_string_lossy().into_owned() };
    let Some(token_str) = extract_login_token(&raw) else {
        report_login_failure("SSO completion token was empty or invalid.".to_string());
        return;
    };

    let pending_client = {
        let clients = CLIENTS.lock().unwrap();
        clients
            .values()
            .find(|c| c.user_id().is_none())
            .cloned()
            .or_else(|| clients.values().next().cloned())
    };

    if let Some(client) = pending_client {
        RUNTIME.spawn(finish_sso_internal(client, token_str));
    } else {
        report_login_failure("SSO completion requested but no pending Matrix client is available.".to_string());
    }
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_deactivate_account(erase_data: bool) {
    let client_opt = {
        let mut clients = CLIENTS.lock().unwrap();
        if let Some((key, _)) = clients.iter().next() {
            let key_owned = key.clone();
            clients.remove(&key_owned)
        } else {
            None
        }
    };

    if let Some(client) = client_opt {
        RUNTIME.spawn(async move {
            if let Err(e) = client.logout().await {
                log::error!("Failed to logout during deactivation: {:?}", e);
            }
        });
    }

    if erase_data {
        let path_opt = {
            let guard = DATA_PATH.lock().unwrap();
            guard.clone()
        };
        if let Some(path) = path_opt {
            safe_wipe_data_dir(&path);
        }
    }
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_logout(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    log::info!("Disconnecting {}...", user_id_str);
    let mut clients = crate::CLIENTS.lock().unwrap();
    
    // Just drop the client to stop the sync loop.
    if let Some(client) = clients.remove(&user_id_str) {
         log::info!("Dropping global client instance for disconnect.");
         // Ensure client is dropped within the Tokio runtime context to prevent
         // "there is no reactor running" panics from internal components (e.g. deadpool).
         RUNTIME.block_on(async move {
             drop(client);
         });
    }
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_destroy_session(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    log::info!("Explicit Logout Requested for {}. Invalidating session...", user_id_str);
    let mut clients = crate::CLIENTS.lock().unwrap();
    
    if let Some(client) = clients.remove(&user_id_str) {
        RUNTIME.spawn(async move {
            // 1. Invalidate on server
            if let Err(e) = client.logout().await {
                log::error!("Failed to logout from homeserver: {:?}", e);
            } else {
                log::info!("Session invalidated on homeserver.");
            }
            
            // 2. Remove Session File
            let path_opt = {
                 let guard = DATA_PATH.lock().unwrap();
                 guard.clone()
            };
            if let Some(mut path) = path_opt {
                 path.push("session.json");
                 if path.exists() {
                     let _ = std::fs::remove_file(path);
                     log::info!("session.json deleted.");
                 }
            }
        });
    }
}
// End of file anchor
