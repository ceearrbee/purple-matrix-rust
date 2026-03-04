use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_status(user_id: *const c_char, status: i32, msg: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let msg_str = if !msg.is_null() {
            unsafe { CStr::from_ptr(msg).to_string_lossy().into_owned() }
        } else {
            String::new()
        };

        log::info!("Setting status for {} to: {} ('{}')", user_id_str, status, msg_str);

        with_client(&user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::presence::PresenceState;
                let presence = match status {
                    0 => PresenceState::Online,
                    1 => PresenceState::Unavailable,
                    2 => PresenceState::Offline,
                    _ => PresenceState::Online,
                };

                use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request as PresenceRequest;
                
                if let Ok(uid) = <&matrix_sdk::ruma::UserId>::try_from(user_id_str.as_str()) {
                    let mut request = PresenceRequest::new(uid.to_owned(), presence.clone());
                    request.status_msg = Some(msg_str);
                    let _ = client.send(request).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_display_name(user_id: *const c_char, name: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || name.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let name_str = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };
        log::info!("Setting display name for {} to: {}", user_id_str, name_str);

        with_client(&user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                let _ = client.account().set_display_name(Some(&name_str)).await;
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_avatar(user_id: *const c_char, path: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || path.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
        log::info!("Setting avatar for {} from file: {}", user_id_str, path_str);

        with_client(&user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                let path_buf = std::path::PathBuf::from(path_str);
                if path_buf.exists() {
                    let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
                    if let Ok(data) = std::fs::read(&path_buf) {
                        if let Ok(response) = client.media().upload(&mime, data, None).await {
                            let _ = client.account().set_avatar_url(Some(&response.content_uri)).await;
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_change_password(user_id: *const c_char, old_pw: *const c_char, new_pw: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || old_pw.is_null() || new_pw.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let old_pw_str = unsafe { CStr::from_ptr(old_pw).to_string_lossy().into_owned() };
        let new_pw_str = unsafe { CStr::from_ptr(new_pw).to_string_lossy().into_owned() };

        with_client(&user_id_str.clone(), |_client: Client| {
            RUNTIME.spawn(async move {
                log::info!("Requesting password change...");
                let _ = (old_pw_str, new_pw_str); 
                crate::ffi::send_system_message(&user_id_str, "Password change is not yet fully implemented in this build.");
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_add_buddy(account_user_id: *const c_char, buddy_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || buddy_user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let buddy_user_id_str = unsafe { CStr::from_ptr(buddy_user_id).to_string_lossy().into_owned() };

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(buddy_user_id_str.as_str()) {
                    log::info!("Adding buddy (Creating DM) for {}", buddy_user_id_str);
                    if let Err(e) = client.create_dm(uid).await {
                        log::error!("Failed to create DM for buddy {}: {:?}", buddy_user_id_str, e);
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_remove_buddy(account_user_id: *const c_char, buddy_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || buddy_user_id.is_null() { return; }
        let _account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let buddy_user_id_str = unsafe { CStr::from_ptr(buddy_user_id).to_string_lossy().into_owned() };
        log::info!("Removed buddy from local list: {}", buddy_user_id_str);
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_idle(user_id: *const c_char, seconds: i32) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::presence::PresenceState;
                let state = if seconds > 300 { PresenceState::Unavailable } else { PresenceState::Online };
                use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request as PresenceRequest;
                
                if let Ok(uid) = <&matrix_sdk::ruma::UserId>::try_from(user_id_str.as_str()) {
                    let request = PresenceRequest::new(uid.to_owned(), state);
                    let _ = client.send(request).await;
                }
            });
        });
    })
}

// Get User Info
#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_user_info(account_user_id: *const c_char, user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        log::info!("Fetching user info for: {} (on account {})", user_id_str, account_user_id_str);

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(user_id_str.as_str()) {
                    match client.account().fetch_user_profile_of(uid).await {
                        Ok(profile) => {
                            // Fix: profile response fields
                            let display_name = profile.get("displayname").and_then(|v| v.as_str().map(String::from));
                            let avatar_url = profile.get("avatar_url").and_then(|v| v.as_str().map(String::from));
                            let is_online = false; 

                            let event = crate::ffi::FfiEvent::ShowUserInfo {
                                user_id: account_user_id_str,
                                target_user_id: user_id_str,
                                display_name,
                                avatar_url,
                                is_online,
                            };
                            let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                        },
                        Err(e) => {
                            log::error!("Failed to fetch profile for {}: {:?}", user_id_str, e);
                            let event = crate::ffi::FfiEvent::ShowUserInfo {
                                user_id: account_user_id_str,
                                target_user_id: user_id_str.clone(),
                                display_name: None,
                                avatar_url: None,
                                is_online: false,
                            };
                            let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_my_profile(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(profile) = client.account().fetch_user_profile().await {
                     // Fix: profile response fields
                     let display_name = profile.get("displayname").and_then(|v| v.as_str().map(String::from)).unwrap_or_else(|| "N/A".to_string());
                     let avatar_url = profile.get("avatar_url").and_then(|v| v.as_str().map(String::from)).unwrap_or_else(|| "None".to_string());
                     crate::ffi::send_system_message(&user_id_str, &format!("My Profile:\nDisplay Name: {}\nAvatar URL: {}", display_name, avatar_url));
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ignore_user(account_user_id: *const c_char, target_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || target_user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                    log::info!("Ignoring user {}", target_user_id_str);
                    if let Err(e) = client.account().ignore_user(uid).await {
                        log::error!("Failed to ignore user {}: {:?}", target_user_id_str, e);
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unignore_user(account_user_id: *const c_char, target_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || target_user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                    log::info!("Unignoring user {}", target_user_id_str);
                    if let Err(e) = client.account().unignore_user(uid).await {
                        log::error!("Failed to unignore user {}: {:?}", target_user_id_str, e);
                    }
                }
            });
        });
    })
}
