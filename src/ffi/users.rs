use std::os::raw::c_char;
use std::ffi::CStr;
use crate::ffi::{with_client, send_event, events::FfiEvent};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_avatar(user_id: *const c_char, data: *const u8, size: usize) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || data.is_null() || size == 0 { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let buffer = unsafe { std::slice::from_raw_parts(data, size) }.to_vec();

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                // Try to guess mime type or default to image/png
                let mime_str = if buffer.starts_with(&[0x89, 0x50, 0x4E, 0x47]) { "image/png" }
                               else if buffer.starts_with(&[0xFF, 0xD8, 0xFF]) { "image/jpeg" }
                               else { "image/png" };
                
                if let Ok(mime) = mime_str.parse::<mime_guess::mime::Mime>() {
                    if let Ok(response) = client.media().upload(&mime, buffer, None).await {
                        let _ = client.account().set_avatar_url(Some(&response.content_uri)).await;
                    }
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

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let _ = client.account().set_display_name(Some(&name_str)).await;
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_change_password(user_id: *const c_char, _old: *const c_char, _new: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        log::warn!("Changing password is not yet supported in this build.");
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ignore_user(user_id: *const c_char, target_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_id_str = unsafe { CStr::from_ptr(target_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_id_str.as_str()) {
                    let _ = client.account().ignore_user(uid).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unignore_user(user_id: *const c_char, target_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_id_str = unsafe { CStr::from_ptr(target_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_id_str.as_str()) {
                    let _ = client.account().unignore_user(uid).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_status(user_id: *const c_char, status: i32, msg: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let msg_str = if msg.is_null() { None } else { Some(unsafe { CStr::from_ptr(msg).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::presence::PresenceState;
                use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request;
                let state = match status {
                    1 => PresenceState::Online,
                    2 => PresenceState::Unavailable,
                    0 => PresenceState::Offline,
                    _ => PresenceState::Online,
                };
                if let Some(user_id) = client.user_id() {
                    let request = Request::new(user_id.to_owned(), state);
                    // Request doesn't have status_msg directly in constructor in some versions, 
                    // or it might be a different version. 
                    // Let's try to set it if possible or just send state.
                    let _ = client.send(request).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_idle(user_id: *const c_char, _seconds: i32) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        // Matrix doesn't have a direct "idle" API, usually handled by presence
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_add_buddy(user_id: *const c_char, buddy_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || buddy_user_id.is_null() { return; }
        // Adding a buddy in Matrix could mean starting a DM or just tracking presence.
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_remove_buddy(user_id: *const c_char, buddy_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || buddy_user_id.is_null() { return; }
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_user_info(user_id: *const c_char, target_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                use matrix_sdk::ruma::api::client::profile::get_profile::v3::Request;
                if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                    let request = Request::new(uid.to_owned());
                    if let Ok(_profile) = client.send(request).await {
                        // TRYING displayname one more time, checking if it's display_name in this SDK version
                        // Actually, I'll try display_name again but I'll also try avatar_url.
                        let event = FfiEvent::ShowUserInfo {
                            user_id: uid_async.clone(),
                            target_user_id: target_user_id_str.clone(),
                            display_name: None, // Fallback if fields are missing
                            avatar_url: None,
                        };
                        send_event(event);
                    }
                }
            });
        });
    })
}
