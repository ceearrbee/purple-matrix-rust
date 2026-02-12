use crate::with_client;
use crate::RUNTIME;
use std::ffi::CStr;
use std::os::raw::c_char;

// Callback for reporting backup status/recovery key?
// For now, we reuse send_system_message to print to chat until we have a dedicated UI.

#[no_mangle]
pub extern "C" fn purple_matrix_rust_enable_key_backup(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        let user_id = user_id_str.clone();
        RUNTIME.spawn(async move {
            log::info!("Checking key backup status for {}", user_id);
            let encryption = client.encryption();
            let backup = encryption.backups();
            
            // Check if backup exists on server
            match backup.exists_on_server().await {
                Ok(exists) => {
                    log::info!("Backup exists on server: {}", exists);
                    if exists {
                        // If it exists, we should try to enable it (connect to it).
                        // This usually requires the recovery key if we don't have it locally.
                         let msg = "Online Key Backup exists on the server. If you haven't verified this session, you may need to verify or enter your recovery key.";
                         crate::ffi::send_system_message(&user_id, msg);
                    } else {
                        // Create a new backup?
                        // This suggests we should generate a recovery key.
                        /*
                        match backup.create_backup().await {
                            Ok(res) => {
                                let msg = format!("Created new Online Key Backup. Version: {}", res.version);
                                crate::ffi::send_system_message(&user_id, &msg);
                            },
                            Err(e) => {
                                let msg = format!("Failed to create backup: {:?}", e);
                                crate::ffi::send_system_message(&user_id, &msg);
                            }
                        }
                        */
                        let msg = "No Online Key Backup found. Creating one is not yet fully supported in this plugin version.";
                        crate::ffi::send_system_message(&user_id, msg);
                    }
                },
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
pub extern "C" fn purple_matrix_rust_restore_from_backup(user_id: *const c_char, recovery_key: *const c_char) {
    if user_id.is_null() || recovery_key.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let _recovery_key = unsafe { CStr::from_ptr(recovery_key).to_string_lossy().into_owned() };

    with_client(&user_id_str, |_client| {
        let user_id = user_id_str.clone();
        RUNTIME.spawn(async move {
            log::info!("Attempting to restore from backup for {}", user_id);
            // Experimental stub
            crate::ffi::send_system_message(&user_id, "Restore from backup triggered. Feature pending full implementation.");
        });
    });
}
