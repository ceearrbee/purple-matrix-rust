use crate::with_client;
use crate::RUNTIME;
use matrix_sdk::ruma::api::client::backup::get_latest_backup_info;
use ruma::api::client::ErrorKind as ApiErrorKind;
use matrix_sdk::Client;
use matrix_sdk::encryption::backups::keys::decryption::{BackupDecryptionKey, DecodeError};
use crate::ffi;
use std::ffi::CStr;
use std::os::raw::c_char;

fn parse_backup_key(input: &str) -> Result<BackupDecryptionKey, DecodeError> {
    BackupDecryptionKey::from_base58(input).or_else(|_| BackupDecryptionKey::from_base64(input))
}

async fn restore_keys_from_backup(
    client: Client,
    user_id: String,
    recovery_key: String,
) {
    let mut msg = "Key backup restored successfully.".to_string();

    match parse_backup_key(&recovery_key) {
        Ok(decryption_key) => {
            let version_opt = match client
                .send(get_latest_backup_info::v3::Request::new())
                .await
            {
                Ok(resp) => Some(resp.version),
                Err(e) => {
                    if let Some(kind) = e.client_api_error_kind() {
                        if kind == &ApiErrorKind::NotFound {
                            None
                        } else {
                            msg =
                                format!("Unable to inspect backups: {:?}. Please retry later.", e);
                            crate::ffi::send_system_message(&user_id, &msg);
                            return;
                        }
                    } else {
                        msg =
                            format!("Unable to inspect backups: {:?}. Please retry later.", e);
                        crate::ffi::send_system_message(&user_id, &msg);
                        return;
                    }
                }
            };

            if let Some(version) = version_opt {
                match client.olm_machine().await {
                    Some(olm_machine) => {
                        if let Err(e) = olm_machine
                            .backup_machine()
                            .save_decryption_key(Some(decryption_key.clone()), Some(version.clone()))
                            .await
                        {
                            msg = format!("Failed to persist backup key locally: {:?}", e);
                        } else {
                            let joined_rooms = client.joined_rooms();
                            let backups = client.encryption().backups();
                            let mut count = 0;
                            for room in joined_rooms {
                                if let Err(e) =
                                    backups.download_room_keys_for_room(room.room_id()).await
                                {
                                    log::error!(
                                        "Failed to download keys for {}: {:?}",
                                        room.room_id(),
                                        e
                                    );
                                } else {
                                    count += 1;
                                }
                            }
                            msg = format!(
                                "Downloaded room keys from backup for {} rooms (version {}).",
                                count, version
                            );
                        }
                    }
                    None => {
                        msg = "Unable to access encryption storage to save backup key.".to_string();
                    }
                }
            } else {
                msg = "No key backup found on the server for this account.".to_string();
            }
        }
        Err(e) => {
            msg = format!("Invalid recovery key: {}", e);
        }
    }

    ffi::send_system_message(&user_id, &msg);
}

// Callback for reporting backup status/recovery key?
#[no_mangle]
pub extern "C" fn purple_matrix_rust_enable_key_backup(user_id: *const c_char) {
    if user_id.is_null() {
        return;
    }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        let user_id = user_id_str.clone();
        RUNTIME.spawn(async move {
            log::info!("Checking key backup status for {}", user_id);
            let encryption = client.encryption();
            let backup = encryption.backups();

            match backup.exists_on_server().await {
                Ok(exists) => {
                    let msg = if exists {
                        "Online Key Backup exists. Use recovery key to restore or verify session."
                    } else {
                        "No Online Key Backup found yet. Enable a backup from another verified client."
                    };
                    crate::ffi::send_system_message(&user_id, msg);
                }
                Err(e) => {
                    log::error!("Failed to check backup status: {:?}", e);
                    let msg = format!("Failed to check backup status: {:?}", e);
                    crate::ffi::send_system_message(&user_id, &msg);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_restore_from_backup(
    user_id: *const c_char,
    recovery_key: *const c_char,
) {
    if user_id.is_null() || recovery_key.is_null() {
        return;
    }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let recovery_key_str =
        unsafe { CStr::from_ptr(recovery_key).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), move |client| {
        let user_id = user_id_str.clone();
        let recovery_key = recovery_key_str.clone();
        RUNTIME.spawn(async move {
            log::info!("Attempting to restore from backup for {}", user_id);
            restore_keys_from_backup(client, user_id, recovery_key).await;
        });
    });
}
