use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_status(user_id: *const c_char, status: i32, msg: *const c_char) {
     if user_id.is_null() { return; }
     let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
     
     let msg_str = if !msg.is_null() {
        unsafe { CStr::from_ptr(msg).to_string_lossy().into_owned() }
    } else {
        String::new()
    };
    
    log::info!("Setting status for {} to: {} ('{}')", user_id_str, status, msg_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             let presence = match status {
                 0 => matrix_sdk::ruma::presence::PresenceState::Offline,
                 1 => matrix_sdk::ruma::presence::PresenceState::Online,
                 2 => matrix_sdk::ruma::presence::PresenceState::Unavailable,
                 _ => matrix_sdk::ruma::presence::PresenceState::Online,
             };
             
             // Presence
             use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request as PresenceRequest;
             use matrix_sdk::ruma::UserId;

             if let Ok(uid) = <&UserId>::try_from(user_id_str.as_str()) {
                 let mut request = PresenceRequest::new(uid.to_owned(), presence.clone());
                 request.status_msg = Some(msg_str);
                 
                 if let Err(e) = client.send(request).await {
                     log::error!("Failed to set presence: {:?}", e);
                 } else {
                     log::info!("Set presence to {:?}", presence);
                 }
             } else {
                 log::error!("Invalid user_id for presence: {}", user_id_str);
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_display_name(user_id: *const c_char, name: *const c_char) {
    if user_id.is_null() || name.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let name_str = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };
    log::info!("Setting display name for {} to: {}", user_id_str, name_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             if let Err(e) = client.account().set_display_name(Some(&name_str)).await {
                 log::error!("Failed to set display name: {:?}", e);
             } else {
                 log::info!("Display name set successfully");
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_avatar(user_id: *const c_char, path: *const c_char) {
    if user_id.is_null() || path.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    log::info!("Setting avatar for {} from file: {}", user_id_str, path_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            let path_buf = std::path::PathBuf::from(&path_str);
            if !path_buf.exists() {
                log::error!("Avatar file not found: {}", path_str);
                return;
            }

            let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
            // Read file content
            let data = match std::fs::read(&path_buf) {
                Ok(d) => d,
                Err(e) => {
                    log::error!("Failed to read avatar file: {:?}", e);
                    return;
                }
            };

            // Upload to media repo first
            let response = match client.media().upload(&mime, data, None).await {
                 Ok(r) => r,
                 Err(e) => {
                     log::error!("Failed to upload avatar image: {:?}", e);
                     return;
                 }
            };

            // Set the avatar URL
            if let Err(e) = client.account().set_avatar_url(Some(&response.content_uri)).await {
                log::error!("Failed to set avatar URL: {:?}", e);
            } else {
                log::info!("Avatar set successfully");
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_change_password(user_id: *const c_char, old_pw: *const c_char, new_pw: *const c_char) {
    if user_id.is_null() || old_pw.is_null() || new_pw.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let old_pw_str = unsafe { CStr::from_ptr(old_pw).to_string_lossy().into_owned() };
    let new_pw_str = unsafe { CStr::from_ptr(new_pw).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            log::info!("Requesting password change...");
            
            use matrix_sdk::ruma::api::client::uiaa::{AuthData, Password, UserIdentifier};
            
            let auth_data = if let Some(user_id) = client.user_id() {
                 Some(AuthData::Password(Password::new(
                     UserIdentifier::UserIdOrLocalpart(user_id.to_string()),
                     old_pw_str
                 )))
            } else {
                 None
            };

            if let Err(e) = client.account().change_password(&new_pw_str, auth_data).await {
                log::error!("Failed to change password: {:?}", e);
            } else {
                log::info!("Password changed successfully.");
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_add_buddy(account_user_id: *const c_char, buddy_user_id: *const c_char) {
    if account_user_id.is_null() || buddy_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let buddy_user_id_str = unsafe { CStr::from_ptr(buddy_user_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(buddy_user_id_str.as_str()) {
                log::info!("Adding buddy (Creating DM) for {}", buddy_user_id_str);
                if let Err(e) = client.create_dm(uid).await {
                    log::error!("Failed to create DM for added buddy {}: {:?}", buddy_user_id_str, e);
                } else {
                    log::info!("DM created/ensured for buddy {}", buddy_user_id_str);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_remove_buddy(account_user_id: *const c_char, buddy_user_id: *const c_char) {
    if account_user_id.is_null() || buddy_user_id.is_null() { return; }
    let _account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let buddy_user_id_str = unsafe { CStr::from_ptr(buddy_user_id).to_string_lossy().into_owned() };
    log::info!("Removed buddy from local list: {}", buddy_user_id_str);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_idle(user_id: *const c_char, seconds: i32) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::presence::PresenceState;
            use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request as PresenceRequest;
            
            if let Some(uid) = client.user_id() {
                 let presence = if seconds > 0 { PresenceState::Unavailable } else { PresenceState::Online };
                 let request = PresenceRequest::new(uid.to_owned(), presence);
                 if let Err(e) = client.send(request).await {
                      log::error!("Failed to set idle presence: {:?}", e);
                 }
            }
        });
    });
}

// Get User Info
#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_user_info(account_user_id: *const c_char, user_id: *const c_char) {
    if account_user_id.is_null() || user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    
    log::info!("Fetching user info for: {} (on account {})", user_id_str, account_user_id_str);

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            
            if let Ok(uid) = <&UserId>::try_from(user_id_str.as_str()) {
                match client.account().fetch_user_profile_of(uid).await {
                    Ok(profile) => {
                        let display_name = profile.get("displayname")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string())
                            .unwrap_or_else(|| user_id_str.clone());
                        let avatar_url = profile.get("avatar_url")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string())
                            .unwrap_or_default();
                        
                        let is_online = false; 

                        let c_user_id = CString::new(user_id_str).unwrap_or_default();
                        let c_display_name = CString::new(display_name).unwrap_or_default();
                        let c_avatar_url = CString::new(avatar_url).unwrap_or_default();
                        
                        let guard = SHOW_USER_INFO_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_display_name.as_ptr(), c_avatar_url.as_ptr(), is_online);
                        }
                    },
                    Err(e) => {
                        log::error!("Failed to fetch profile for {}: {:?}", user_id_str, e);
                        // Fallback
                        let c_user_id = CString::new(user_id_str.clone()).unwrap_or_default();
                        let c_display_name = CString::new(user_id_str).unwrap_or_default();
                        let c_avatar_url = CString::new("").unwrap_or_default();
                        let guard = SHOW_USER_INFO_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_display_name.as_ptr(), c_avatar_url.as_ptr(), false);
                        }
                    }
                }
            }
        });
    });
}
#[no_mangle]
pub extern "C" fn purple_matrix_rust_ignore_user(account_user_id: *const c_char, target_user_id: *const c_char) {
    if account_user_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                log::info!("Ignoring user {}", target_user_id_str);
                if let Err(e) = client.account().ignore_user(uid).await {
                    log::error!("Failed to ignore user: {:?}", e);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unignore_user(account_user_id: *const c_char, target_user_id: *const c_char) {
    if account_user_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                log::info!("Unignoring user {}", target_user_id_str);
                if let Err(e) = client.account().unignore_user(uid).await {
                    log::error!("Failed to unignore user: {:?}", e);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_avatar_bytes(user_id: *const c_char, data: *const u8, len: usize) {
    if user_id.is_null() || data.is_null() || len == 0 { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let bytes = unsafe { std::slice::from_raw_parts(data, len).to_vec() };

    log::info!("Setting avatar bytes for {} ({} bytes)", user_id_str, len);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            // Assume PNG/JPEG/OctetStream. Mime guess from bytes is harder without magic library
            // Default to application/octet-stream or image/png
            let mime = "image/png".parse().unwrap(); 
            
            match client.media().upload(&mime, bytes, None).await {
                Ok(response) => {
                     if let Err(e) = client.account().set_avatar_url(Some(&response.content_uri)).await {
                         log::error!("Failed to set avatar URL from bytes: {:?}", e);
                     } else {
                         log::info!("Avatar set successfully from bytes");
                     }
                },
                Err(e) => log::error!("Failed to upload avatar bytes: {:?}", e),
            }
        });
    });
}
