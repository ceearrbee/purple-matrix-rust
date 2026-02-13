use crate::{ffi, with_client, RUNTIME};
use matrix_sdk::Client;
use std::ffi::CStr;
use std::os::raw::c_char;

async fn restore_keys_from_backup(_client: Client, user_id: String, recovery_key: String) {
    log::warn!(
        "Key backup restoration requested for {} but the feature is currently unavailable.",
        user_id
    );

    if !recovery_key.is_empty() {
        log::debug!("Received recovery key (first 8 chars): {}", &recovery_key[..8.min(recovery_key.len())]);
    }

    let msg = "Key backup restoration is not supported in this build. Use a full Matrix client to restore keys.";
    ffi::send_system_message(&user_id, msg);
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
