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
pub extern "C" fn purple_matrix_rust_bootstrap_cross_signing_with_password(user_id: *const c_char, password: *const c_char) {
    if user_id.is_null() || password.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let pass_str = unsafe { CStr::from_ptr(password).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            let status = client.encryption().cross_signing_status().await;
            if status.is_some() {
                 log::info!("Cross signing already active.");
                 crate::ffi::send_system_message(&user_id_str, "Cross-signing is already active.");
                 return;
            }
            
            use matrix_sdk::ruma::api::client::uiaa::{AuthData, Password, UserIdentifier};
            
            let auth_data = if let Some(uid) = client.user_id() {
                 Some(AuthData::Password(Password::new(
                     UserIdentifier::UserIdOrLocalpart(uid.to_string()),
                     pass_str
                 )))
            } else {
                 None
            };

            if let Err(e) = client.encryption().bootstrap_cross_signing(auth_data).await {
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
                 let encryption = client.encryption();
                 
                 // Try to request identity first to ensure it's tracked
                 let _ = encryption.request_user_identity(uid).await;

                 match encryption.get_user_identity(uid).await {
                     Ok(Some(identity)) => {
                         log::info!("Identity found for {}, requesting verification...", target_user_id_str);
                         match identity.request_verification().await {
                             Ok(_) => {
                                 log::info!("Verification request sent to {}", target_user_id_str);
                                 crate::ffi::send_system_message(&user_id_str, &format!("Verification request sent to {}. Please check your other devices.", target_user_id_str));
                             },
                             Err(e) => {
                                 log::error!("Failed to request verification: {:?}", e);
                                 crate::ffi::send_system_message(&user_id_str, &format!("Verification failed to start: {:?}", e));
                             }
                         }
                     },
                     Ok(None) => {
                         log::warn!("User identity not found for {}. Checking for other devices...", target_user_id_str);
                         // If it's self-verification and no identity found, maybe try verifying against another device directly?
                         // For now, just report failure.
                         crate::ffi::send_system_message(&user_id_str, &format!("Could not find a valid Matrix identity for {}. Ensure they share a room with you.", target_user_id_str));
                     },
                     Err(e) => {
                         log::error!("Failed to get user identity: {:?}", e);
                         crate::ffi::send_system_message(&user_id_str, &format!("Error retrieving identity: {:?}", e));
                     },
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
pub extern "C" fn purple_matrix_rust_resync_room_keys(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                let encryption = client.encryption();
                let backups = encryption.backups();
                
                log::info!("Manually requesting key sync for room {}...", room_id_str);
                
                if backups.are_enabled().await {
                    crate::ffi::send_system_message(&user_id_str, "Downloading room keys from backup...");
                    if let Err(e) = backups.download_room_keys_for_room(rid).await {
                        log::error!("Failed to download room keys: {:?}", e);
                        crate::ffi::send_system_message(&user_id_str, &format!("Failed to download keys: {:?}", e));
                    } else {
                        crate::ffi::send_system_message(&user_id_str, "Room keys updated! Try viewing threads again.");
                    }
                } else {
                    crate::ffi::send_system_message(&user_id_str, "Key backup is not enabled. Other devices must be online to share keys.");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_debug_crypto_status(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            let encryption = client.encryption();
            let mut msg = format!("<b>Crypto Status for {}:</b><br/>", user_id_str);
            
            if let Ok(Some(device)) = encryption.get_own_device().await {
                msg.push_str(&format!("- Device ID: {}<br/>", device.device_id()));
                msg.push_str(&format!("- Locally Trusted: {}<br/>", device.is_locally_trusted()));
                msg.push_str(&format!("- Verified: {}<br/>", device.is_verified()));
            } else {
                msg.push_str("- Own device not found!<br/>");
            }

            if let Some(status) = encryption.cross_signing_status().await {
                msg.push_str(&format!("- Cross-signing: Active<br/>"));
                msg.push_str(&format!("  - Master Key: {}<br/>", status.has_master));
                msg.push_str(&format!("  - Self-signing Key: {}<br/>", status.has_self_signing));
                msg.push_str(&format!("  - User-signing Key: {}<br/>", status.has_user_signing));
            } else {
                msg.push_str("- Cross-signing: Not initialized<br/>");
            }
            
            let backups = encryption.backups();
            msg.push_str(&format!("- Key Backup: {}<br/>", backups.are_enabled().await));

            crate::ffi::send_system_message(&user_id_str, &msg);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_export_keys(user_id: *const c_char, path: *const c_char, passphrase: *const c_char) {
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
pub extern "C" fn purple_matrix_rust_restore_backup(user_id: *const c_char, recovery_key: *const c_char) {
    if user_id.is_null() || recovery_key.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let key_str = unsafe { CStr::from_ptr(recovery_key).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            let encryption = client.encryption();
            let recovery = encryption.recovery();
            
            log::info!("Attempting to restore keys from backup for {}...", user_id_str);
            
            // 1. Recover the secrets (this imports the recovery key into the store)
            match recovery.recover(&key_str).await {
                Ok(_) => {
                    log::info!("Successfully imported recovery key for {}", user_id_str);
                    
                    // 2. Trigger key download from backup for all joined rooms
                    let backups = encryption.backups();
                    if backups.are_enabled().await {
                         crate::ffi::send_system_message(&user_id_str, "Recovery key accepted! Downloading keys from backup...");
                         
                         let rooms = client.joined_rooms();
                         for room in rooms {
                             let _ = backups.download_room_keys_for_room(room.room_id()).await;
                         }
                         crate::ffi::send_system_message(&user_id_str, "Key download complete. Historical threads should now be visible.");
                    } else {
                         crate::ffi::send_system_message(&user_id_str, "Recovery key accepted, but online backup is not enabled on this account.");
                    }
                },
                Err(e) => {
                    log::error!("Failed to recover keys: {:?}", e);
                    crate::ffi::send_system_message(&user_id_str, &format!("Failed to recover keys: {:?}", e));
                }
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
