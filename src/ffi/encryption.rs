use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client, CLIENTS};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_verify_user(user_id: *const c_char, target_user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_selector = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

        log::info!("Starting verification for {}", target_selector);

        with_client(&user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                let encryption = client.encryption();

                if let Ok(uid) = <&UserId>::try_from(target_selector.as_str()) {
                    let _ = encryption.request_user_identity(uid).await;
                    if let Ok(Some(identity)) = encryption.get_user_identity(uid).await {
                        // Fix: UserIdentity is likely a struct with request_verification in this SDK version
                        let _ = identity.request_verification().await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_confirm_verification(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char, confirm: bool) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_user_id.is_null() || flow_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
        let flow_id_str = unsafe { CStr::from_ptr(flow_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                    if let Some(request) = client.encryption().get_verification_request(uid, &flow_id_str).await {
                        if confirm {
                            log::warn!("Emoji confirmation path mismatch in this SDK version");
                        } else {
                            let _ = request.cancel().await;
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_accept_verification(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_user_id.is_null() || flow_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
        let flow_id_str = unsafe { CStr::from_ptr(flow_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                    if let Some(request) = client.encryption().get_verification_request(uid, &flow_id_str).await {
                        let _ = request.accept().await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_confirm_sas(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char, confirm: bool) {
    purple_matrix_rust_confirm_verification(user_id, target_user_id, flow_id, confirm);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_accept_sas(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char) {
    purple_matrix_rust_accept_verification(user_id, target_user_id, flow_id);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_is_room_encrypted(user_id: *const c_char, room_id: *const c_char) -> bool {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return false; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        if crate::ENCRYPTED_ROOMS.contains(&room_id_str) {
            return true;
        }

        if let Some(client) = CLIENTS.get(&user_id_str) {
             use matrix_sdk::ruma::RoomId;
             use matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent;
             if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                 if let Some(room) = client.get_room(rid) {
                     RUNTIME.spawn(async move {
                         if room.get_state_event_static::<RoomEncryptionEventContent>().await.ok().flatten().is_some() {
                             crate::ENCRYPTED_ROOMS.insert(room_id_str.clone());
                             // Ideally we would notify C to re-check, but this prevents blocking.
                         }
                     });
                 }
             }
        }
        false
    }, false)
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_enable_key_backup(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let _backups = client.encryption().backups();
                log::info!("Key backup creation not yet implemented for this SDK version");
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bootstrap_cross_signing(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let _ = client.encryption().bootstrap_cross_signing(None).await;
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bootstrap_cross_signing_with_password(user_id: *const c_char, password: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || password.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let _password_str = unsafe { CStr::from_ptr(password).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let _ = client.encryption().bootstrap_cross_signing(None).await;
                log::warn!("Bootstrap with password not yet fully implemented for this SDK version");
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_own_devices(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(Some(device)) = client.encryption().get_own_device().await {
                    let info = format!("Your Device:\n- {} ({})", device.device_id(), device.display_name().unwrap_or("Unknown"));
                    crate::ffi::send_system_message(&uid_async, &info);
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_recover_keys_prompt(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                // In a real implementation, we would check if a backup exists
                // and then prompt for the recovery key via a system message or UI dialog.
                let info = "To restore your encrypted message history, please enter your Recovery Security Key/Phrase in the account settings, or verify this session from another active device.";
                crate::ffi::send_system_message(&uid_async, info);
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_debug_crypto_status(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let encryption = client.encryption();
                let status = encryption.cross_signing_status().await;
                let msg = format!("E2EE Debug Status for {}:\nCross-signing: {:?}\n", uid_async, status);
                crate::ffi::send_system_message(&uid_async, &msg);
            });
        });
    })
}
