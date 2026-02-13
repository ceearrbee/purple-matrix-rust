use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bootstrap_cross_signing(user_id: *const c_char, password: *const c_char) {
    if user_id.is_null() || password.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let password_str = unsafe { CStr::from_ptr(password).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {

            
            // This is a complex operation. Simplified for this FFI call.
            // Ideally should check recovery status, etc.
            
            // Check if bootstrap needed
            let status = client.encryption().cross_signing_status().await;
            if status.is_some() {
                 log::info!("Cross signing already active.");
                 return;
            }

            // Requires UIAA, so we need password.
            use matrix_sdk::ruma::api::client::uiaa::{AuthData, Password, UserIdentifier};
            
            let auth_data = if let Some(uid) = client.user_id() {
                 Some(AuthData::Password(Password::new(
                     UserIdentifier::UserIdOrLocalpart(uid.to_string()),
                     password_str.clone()
                 )))
            } else {
                 None
            };
            
            // Bootstrap
            if let Err(e) = client.encryption().bootstrap_cross_signing(auth_data).await {
                 log::error!("Failed to bootstrap cross signing: {:?}", e);
            } else {
                 log::info!("Bootstrap cross signing successful.");
            }
        });
    });
}

// ... code ...

#[no_mangle]
pub extern "C" fn purple_matrix_rust_e2ee_status(user_id: *const c_char, room_id: *const c_char) -> bool {
    if user_id.is_null() || room_id.is_null() { return false; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    // Explicit type for res
    let res: Option<bool> = RUNTIME.block_on(async move {
        // Use with_client (synchronous wrapper that returns immediate value)
        // But we need async client interaction.
        // We can't easily bubble async from with_client closure.
        // But with_client returns Option<R>.
        // If R is a Future, it returns Option<Future>.
        
        let fut = crate::with_client(&user_id_str, |client| async move {
             use matrix_sdk::ruma::RoomId;
             use matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent;
             if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                 if let Some(room) = client.get_room(rid) {
                     // Check if state event exists
                     return room.get_state_event_static::<RoomEncryptionEventContent>().await.ok().flatten().is_some();
                 }
             }
             false
        });
        
        if let Some(f) = fut {
            Some(f.await)
        } else {
            None
        }
    });
    
    res.unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_export_room_keys(user_id: *const c_char, path: *const c_char, passphrase: *const c_char) {
    if user_id.is_null() || path.is_null() || passphrase.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    // let passphrase_str = unsafe { CStr::from_ptr(passphrase).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             // Export keys is complex and signature varies. Stubbed for refactor stability.
             log::warn!("export_room_keys not fully implemented in this version.");
             let msg = "Export room keys not implemented in this build.".to_string();
             crate::ffi::send_system_message(&user_id_str, &msg);
             
             // Placeholder for future implementation:
             // client.encryption().export_room_keys(...)
        });
    });
}
