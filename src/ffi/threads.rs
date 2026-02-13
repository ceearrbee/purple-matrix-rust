use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_threads(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
           // Matrix-sdk doesn't have a direct "list threads" easy API that returns a list of root events with metadata readily?
           // Actually, it maintains thread state in `BaseClient`.
           // But access via `Room` involves iterating events or using `threads()` if available (experimental features).
           
           // Placeholder logic:
           // We would iterate recent events and find thread roots, then callback.
           
           // Log for now
           log::info!("Listing threads for {} (Not fully implemented in FFI)", room_id_str);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_thread_list_callback(cb: ThreadListCallback) {
    let mut guard = THREAD_LIST_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
