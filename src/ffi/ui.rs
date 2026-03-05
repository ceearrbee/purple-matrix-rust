use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{send_event, events::FfiEvent};

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ui_action_poll(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let event = FfiEvent::PollCreationRequested {
            user_id: user_id_str,
            room_id: room_id_str,
        };
        send_event(event);
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ui_show_dashboard(_room_id: *const c_char) {
    // Dashboard not yet implemented
}
