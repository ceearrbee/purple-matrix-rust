use std::os::raw::c_char;
use crate::send_system_message;

#[no_mangle]

pub extern "C" fn purple_matrix_rust_ui_show_dashboard(room_id: *const c_char) {

    if room_id.is_null() { return; }

    let room_id_str = unsafe { std::ffi::CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    

    log::info!("UI plugin requested dashboard for room: {}", room_id_str);

    

    // In a production build, we might open a native window here if we had a UI handle.

    // Since we are headless in Rust, we just provide feedback to the C side.

    send_system_message(&"System", &format!("Matrix Dashboard for room {} is active. Use /matrix_help for more commands.", room_id_str));

}
