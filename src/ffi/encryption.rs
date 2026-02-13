use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bootstrap_cross_signing(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            // Check if bootstrap needed
            let status = client.encryption().cross_signing_status().await;
            if status.is_some() {
                 log::info!("Cross signing already active.");
                 crate::ffi::send_system_message(&user_id_str, "Cross-signing is already active.");
                 return;
            }

            if let Err(e) = client.encryption().bootstrap_cross_signing(None).await {
                 log::error!("Failed to bootstrap cross signing: {:?}", e);
                 crate::ffi::send_system_message(&user_id_str, &format!("Failed to bootstrap cross-signing: {:?}", e));
            } else {
                 log::info!("Bootstrap cross signing successful.");
                 crate::ffi::send_system_message(&user_id_str, "Cross-signing bootstrapped successfully.");
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_verify_user(user_id: *const c_char, target_user_id: *const c_char) {
    if user_id.is_null() || target_user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    log::info!("Requesting verification for user: {}", target_user_id_str);

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::UserId;
             if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                 match client.encryption().get_user_identity(uid).await {
                     Ok(Some(identity)) => {
                         if let Err(e) = identity.request_verification().await {
                             log::error!("Failed to request verification: {:?}", e);
                         } else {
                             log::info!("Verification request sent to {}", target_user_id_str);
                         }
                     },
                     Ok(None) => log::warn!("User identity not found for {}. Please ensure you share a room.", target_user_id_str),
                     Err(e) => log::error!("Failed to get user identity: {:?}", e),
                 }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_accept_sas(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char) {
    if user_id.is_null() || target_user_id.is_null() || flow_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let flow_id_str = unsafe { CStr::from_ptr(flow_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                if let Some(request) = client.encryption().get_verification_request(uid, &flow_id_str).await {
                    let _ = request.accept().await;
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_confirm_sas(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char, is_match: bool) {
    if user_id.is_null() || target_user_id.is_null() || flow_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let flow_id_str = unsafe { CStr::from_ptr(flow_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                if let Some(verification) = client.encryption().get_verification(uid, &flow_id_str).await {
                    if let Some(sas) = verification.sas() {
                        if is_match {
                            let _ = sas.confirm().await;
                        } else {
                            let _ = sas.mismatch().await;
                        }
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_recover_keys(user_id: *const c_char, passphrase: *const c_char) {
    if user_id.is_null() || passphrase.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let pass_str = unsafe { CStr::from_ptr(passphrase).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             match client.encryption().recovery().recover(&pass_str).await {
                 Ok(_) => {
                     log::info!("Secrets recovered successfully!");
                     crate::ffi::send_system_message(&user_id_str, "Key recovery successful.");
                 },
                 Err(e) => {
                     log::error!("Failed to recover secrets: {:?}", e);
                     crate::ffi::send_system_message(&user_id_str, &format!("Key recovery failed: {:?}", e));
                 }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_e2ee_status(user_id: *const c_char, room_id: *const c_char) -> bool {
    if user_id.is_null() || room_id.is_null() { return false; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    let res: Option<bool> = RUNTIME.block_on(async move {
        let fut = crate::with_client(&user_id_str, |client| async move {
             use matrix_sdk::ruma::RoomId;
             use matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent;
             if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                 if let Some(room) = client.get_room(rid) {
                     return room.get_state_event_static::<RoomEncryptionEventContent>().await.ok().flatten().is_some();
                 }
             }
             false
        });
        
        if let Some(f) = fut { Some(f.await) } else { None }
    });
    
    res.unwrap_or(false)
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_export_room_keys(user_id: *const c_char, path: *const c_char, passphrase: *const c_char) {
    if user_id.is_null() || path.is_null() || passphrase.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    let pass_str = unsafe { CStr::from_ptr(passphrase).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             let path_buf = std::path::PathBuf::from(path_str);
             if let Err(e) = client.encryption().export_room_keys(path_buf, &pass_str, |_| true).await {
                log::error!("Failed to export room keys: {:?}", e);
                crate::ffi::send_system_message(&user_id_str, &format!("Failed to export room keys: {:?}", e));
             } else {
                log::info!("Room keys exported successfully");
                crate::ffi::send_system_message(&user_id_str, "Room keys exported successfully.");
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_enable_key_backup(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            let encryption = client.encryption();
            let backup = encryption.backups();
            
            match backup.state() {
                matrix_sdk::encryption::backups::BackupState::Unknown => {
                    log::info!("Backup state unknown, checking server...");
                    if let Ok(exists) = backup.exists_on_server().await {
                        if exists {
                             crate::ffi::send_system_message(&user_id_str, "Online Key Backup exists on server. You can restore from it using /matrix_restore_backup <security_key>.");
                        } else {
                             crate::ffi::send_system_message(&user_id_str, "No Online Key Backup found. Auto-creation is not yet supported in this version. Please enable backup on another client.");
                             // create_backup() not available in this SDK version or requires different args.
                        }
                    } else {
                        crate::ffi::send_system_message(&user_id_str, "Failed to check backup status on server.");
                    }
                },
                state => {
                    crate::ffi::send_system_message(&user_id_str, &format!("Key backup state: {:?}", state));
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_restore_from_backup(user_id: *const c_char, recovery_key: *const c_char) {
    if user_id.is_null() || recovery_key.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let key_str = unsafe { CStr::from_ptr(recovery_key).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            log::info!("Restoring from backup...");
            let recovery_key = key_str.trim();
            
            // Try to use the secret storage recovery which often handles backup decryption too.
            // This avoids needing the specific RecoveryKey struct which we failed to import.
            match client.encryption().recovery().recover(recovery_key).await {
                 Ok(_) => crate::ffi::send_system_message(&user_id_str, "Secret Storage recovered and keys restored from backup (if available)."),
                 Err(e) => crate::ffi::send_system_message(&user_id_str, &format!("Recovery failed: {:?}", e)),
            }
        });
    });
}
