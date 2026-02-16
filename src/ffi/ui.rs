use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::send_system_message;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ui_show_dashboard(room_id: *const c_char) {
    if room_id.is_null() { return; }
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    log::info!("UI plugin requested dashboard for room: {}", room_id_str);
    send_system_message(&"System", &format!("UI requested dashboard for room: {}", room_id_str));
}