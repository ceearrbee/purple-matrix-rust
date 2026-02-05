use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::path::PathBuf;
use matrix_sdk::Client;
use crate::ffi::{CONNECTED_CALLBACK, LOGIN_FAILED_CALLBACK, SSO_CALLBACK};
use crate::{RUNTIME, GLOBAL_CLIENT, DATA_PATH};
use crate::sync_logic;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_finish_sso(token: *const c_char) {
    if token.is_null() { return; }
    let token_str = unsafe { CStr::from_ptr(token).to_string_lossy().into_owned() };
    
    RUNTIME.spawn(async move {
        let client_opt = {
            let guard = GLOBAL_CLIENT.lock().unwrap();
            guard.clone()
        };
        
        if let Some(client) = client_opt {
             log::info!("Finishing SSO login with token...");
             match client.matrix_auth().login_token(&token_str).initial_device_display_name("Pidgin (Rust)").await {
                 Ok(_) => {
                     log::info!("SSO Login Successful! Persisting session...");
                     finish_login_success(client).await;
                 },
                 Err(e) => {
                     let err_msg = format!("SSO Login Failed: {:?}", e);
                     log::error!("{}", err_msg);
                     
                     if err_msg.contains("Mismatched") || err_msg.contains("Session") {
                         log::warn!("SSO Mismatch detected. Wiping data and retrying login...");
                         
                         // 1. Preserve config
                         let hs_url = client.homeserver().to_string();
                         
                         // 2. Drop Client
                         {
                             let mut guard = GLOBAL_CLIENT.lock().unwrap();
                             *guard = None;
                         }
                         drop(client); // Release DB lock
                         
                         // 3. Wipe Data
                         let path_opt = {
                              let guard = DATA_PATH.lock().unwrap();
                              guard.clone()
                         };
                         
                         if let Some(path) = path_opt {
                             log::info!("Deleting data directory: {:?}", path);
                             let _ = std::fs::remove_dir_all(&path);
                             let _ = std::fs::create_dir_all(&path);
                             
                             // 4. Rebuild Client
                             match build_client(&hs_url, &path).await {
                                 Ok(new_client) => {
                                     log::info!("Client rebuilt. Retrying login...");
                                     
                                     // Update Global
                                     {
                                         let mut guard = GLOBAL_CLIENT.lock().unwrap();
                                         *guard = Some(new_client.clone());
                                     }
                                     
                                     // 5. Retry Login
                                     match new_client.matrix_auth().login_token(&token_str).initial_device_display_name("Pidgin (Rust)").await {
                                         Ok(_) => {
                                             log::info!("Retry SSO Login Successful!");
                                             // Notify connected callback
                                             {
                                                 let guard = CONNECTED_CALLBACK.lock().unwrap();
                                                 if let Some(cb) = *guard {
                                                      cb();
                                                 }
                                             }
                                             // Finish setup
                                             finish_login_success(new_client).await;
                                         },
                                         Err(e2) => {
                                             let msg = format!("Retry failed: {:?}", e2);
                                             log::error!("{}", msg);
                                             // Fallback report
                                             let guard = LOGIN_FAILED_CALLBACK.lock().unwrap();
                                             if let Some(cb) = *guard {
                                                 let c_err = CString::new(msg).unwrap_or_default();
                                                 cb(c_err.as_ptr());
                                             }
                                         }
                                     }
                                 },
                                 Err(e_build) => {
                                     log::error!("Failed to rebuild client for retry: {:?}", e_build);
                                 }
                             }
                         }
                     } else {
                         // Notify user of non-mismatch error
                         let guard = LOGIN_FAILED_CALLBACK.lock().unwrap();
                         if let Some(cb) = *guard {
                             let c_err = CString::new(err_msg).unwrap_or_default();
                             cb(c_err.as_ptr());
                         }
                     }
                 }
             }
        } else {
            log::error!("Finish SSO called but no client instance found!");
        }
    });
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
    
    // Disable SSL verification to rule out cert issues causing load failures
    let builder = builder.disable_ssl_verification();
    
    builder.sqlite_store(data_path, None).build().await
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
             let _ = std::fs::remove_dir_all(&data_path);
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
    // Attempt to load from session.json
    {
         let session_path = data_path.join("session.json");
         if session_path.exists() {
              if let Ok(json) = std::fs::read_to_string(&session_path) {
                  if let Ok(saved) = serde_json::from_str::<SavedSession>(&json) {
                       log::info!("Found saved session for {}, restoring...", saved.user_id);
                       
                       use matrix_sdk::authentication::matrix::MatrixSession;
                       use matrix_sdk::authentication::SessionTokens; 
                       use matrix_sdk::ruma::{UserId, DeviceId};
                       
                       // Construct MatrixSession
                       if let Ok(u) = <&UserId>::try_from(saved.user_id.as_str()) {
                           let d = <&DeviceId>::from(saved.device_id.as_str()); {
                               let session = MatrixSession {
                                   meta: matrix_sdk::SessionMeta {
                                       user_id: u.to_owned(),
                                       device_id: d.to_owned(),
                                   },
                                   tokens: SessionTokens {
                                       access_token: saved.access_token,
                                       refresh_token: None,
                                   },
                               };
                               
                               // RoomLoadSettings is required in 0.16.0
                               let load_settings = matrix_sdk::store::RoomLoadSettings::default(); 
                               
                               if let Err(e) = client.matrix_auth().restore_session(session, load_settings).await {
                                   log::error!("Failed to restore session from file: {:?}", e);
                                   let _ = std::fs::remove_file(&session_path);
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
    }

    if client.user_id().is_some() {
         log::info!("Restored existing session for {}", client.user_id().unwrap());
         finish_login_success(client).await;
         return;
    }

    log::warn!("No existing session found (user_id is None). Checking for stale data...");
    if data_path.exists() {
         let db_file = data_path.join("matrix-sdk-state.sqlite");
         if db_file.exists() {
             log::warn!("Database file exists at {:?} but session was not loaded.", db_file);
             log::warn!("Configured Homeserver: {}", client.homeserver());
             // This implies a potential mismatch or corrupted store.
         } else {
             log::info!("Data directory exists but no database found.");
         }
    }
    
    // SAFETY WIPE: 
    // If we are here, 'client' failed to load session. 
    // If the data directory contains a database, 'login' will fail with "MismatchedAccount".
    // We MUST wipe to allow login to proceed.
    // This sacrifices persistence (which just failed anyway) for login ability.
    if data_path.exists() {
        let has_files = data_path.read_dir().map(|mut i| i.next().is_some()).unwrap_or(false);
        if has_files {
             log::info!("Data directory present but no active session. Proceeding with fresh login flow...");
        }
    }
    
    // Unused variable removed: db_file
    
    {
         let mut guard = GLOBAL_CLIENT.lock().unwrap();
         *guard = Some(client.clone());
    }

    if !password.is_empty() {
        log::info!("Attempting Password Login...");
        match client.matrix_auth().login_username(&username, &password).initial_device_display_name("Pidgin (Rust)").await {
            Ok(_) => {
                log::info!("Login Succeeded!");
                finish_login_success(client).await;
            },
            Err(e) => {
                // If error is mismatch, we should wipe and retry
                let err_str = format!("{:?}", e);
                if err_str.contains("Mismatched") || err_str.contains("Session") {
                    log::warn!("Login failed due to Store Mismatch. Wiping and retrying...");
                    drop(client);
                    let _ = std::fs::remove_dir_all(&data_path);
                    let _ = std::fs::create_dir_all(&data_path);
                    
                    // Recursive retry (safe because we wiped)
                    // Note: ensure we don't infinite loop. One retry is enough.
                    // Ideally call perform_login again but it's async recursion.
                    // Just report failure for now, asking user to retry.
                    report_login_failure("Session conflict detected. data wiped. Please retry login.".to_string());
                } else {
                    report_login_failure(format!("Login failed: {:?}", e));
                }
            }
        }
    } else {
        log::info!("Password empty, checking login discovery...");
        
        let client_clone = client.clone();
        RUNTIME.spawn(async move {
            match client_clone.matrix_auth().get_login_types().await {
                Ok(types) => {
                    log::info!("Supported login types: {:?}", types);
                    // Check for SSO/OIDC
                    // types is a Ruma Response which contains a list of LoginType
                    // (Actually in 0.16.0 it might be a bit different)
                },
                Err(e) => {
                    log::warn!("Failed to query login types: {:?}. Assuming SSO.", e);
                }
            }
            start_sso_flow(client_clone);
        });
    }
}

#[derive(serde::Serialize, serde::Deserialize)]
struct SavedSession {
    user_id: String,
    device_id: String,
    access_token: String,
}

async fn finish_login_success(client: Client) {
    // 1. Persist Session
    if let Some(session) = client.session() {
         let user_id = client.user_id().map(|u| u.to_string()).unwrap_or_default();
         let device_id = client.device_id().map(|d| d.to_string()).unwrap_or_default();
         let access_token = session.access_token().to_string();

         let data = SavedSession {
             user_id,
             device_id,
             access_token, 
         };
         
         // data_path logic needs to be robust. using DATA_PATH global.
         let path_opt = {
              let guard = DATA_PATH.lock().unwrap();
              guard.clone()
         };
         
         if let Some(mut path) = path_opt {
             path.push("session.json");
             if let Ok(json) = serde_json::to_string(&data) {
                 if let Err(e) = std::fs::write(&path, json) {
                     log::error!("Failed to save session.json: {:?}", e);
                 } else {
                     log::info!("Session successfully saved to {:?}", path);
                 }
             }
         }
    }

    {
         let mut guard = GLOBAL_CLIENT.lock().unwrap();
         *guard = Some(client.clone());
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
    let rt = tokio::runtime::Handle::current();
        
    rt.spawn_blocking(move || {
            // Helper to find free port
            let listener = match tiny_http::Server::http("127.0.0.1:0") {
                Ok(l) => l,
                Err(e) => {
                    log::error!("Failed to start local HTTP server: {}", e);
                    return;
                }
            };
            
            let port = listener.server_addr().to_ip().unwrap().port();
            let redirect_url = format!("http://localhost:{}/login", port);
            log::info!("Listening for SSO callback on {}", redirect_url);

            // Manual SSO URL Construction
            let hs_url = client.homeserver();
            let base = hs_url.as_str().trim_end_matches('/');
            let encoded_redirect: String = url::form_urlencoded::byte_serialize(redirect_url.as_bytes()).collect();
            let sso_url = format!("{}/_matrix/client/v3/login/sso/redirect?redirectUrl={}", base, encoded_redirect);
            
            log::info!("SSO URL generated: {}", sso_url);
            
            // Emit Callback
            {
                let guard = SSO_CALLBACK.lock().unwrap();
                if let Some(cb) = *guard {
                    let c_url = CString::new(sso_url).unwrap();
                    cb(c_url.as_ptr());
                }
            }
            
            // Wait for request
            if let Ok(request) = listener.recv() {
                let url_string = request.url().to_string();
                if let Some(pos) = url_string.find("loginToken=") {
                        let token_part = &url_string[pos + 11..];
                        let token = token_part.split('&').next().unwrap_or("").to_string(); 
                        log::info!("Captured SSO token from callback!");
                        
                        let response = tiny_http::Response::from_string("Login successful! You can close this window.");
                        let _ = request.respond(response);
                        
                        // Finish SSO on Runtime
                        purple_matrix_rust_finish_sso(CString::new(token).unwrap().as_ptr());
                }
            }
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_logout() {
    log::info!("Logging out and clearing global client...");
    let mut guard = GLOBAL_CLIENT.lock().unwrap();
    
    if let Some(client) = guard.take() {
        RUNTIME.spawn(async move {
            if let Err(e) = client.logout().await {
                log::error!("Failed to logout from homeserver: {:?}", e);
            } else {
                log::info!("Session invalidated on homeserver.");
            }
            drop(client);
            log::info!("Client dropped safely within Tokio context.");
        });
    }
}