use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ui_show_dashboard(room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if room_id.is_null() { return; }
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        log::info!("Dashboard requested for room: {}", room_id_str);
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ui_action_poll(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        log::info!("UI action requested: Poll in room {}", room_id_str);
        
        with_client(&user_id_str, move |_client: Client| {
            // Hint to the UI to open a poll creation dialog
        });
    })
}
