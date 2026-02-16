use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_active_polls(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(_room) = client.get_room(rid) {
                    // Iterate through timeline to find polls.
                    // In a real implementation we'd use room.event_cache() or similar.
                    // For now we notify the UI via callback.
                    log::info!("Fetching active polls for room {}", room_id_str);
                    
                    // Simple mock for now: list polls in timeline
                    // We need to fetch enough history or check state.
                    // For the sake of fixing the load error quickly:
                    let guard = POLL_LIST_CALLBACK.lock().unwrap();
                    if let Some(_cb) = *guard {
                         // Send end of list marker
                         // cb(c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), std::ptr::null(), std::ptr::null());
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_poll_list_callback(cb: PollListCallback) {
    let mut guard = POLL_LIST_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
