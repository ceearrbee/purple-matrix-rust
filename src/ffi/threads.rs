use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_threads(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        let client_clone = client.clone();
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::room::ListThreadsOptions;
            use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent};

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client_clone.get_room(rid) {
                    log::info!("Listing all threads for {} using server API", room_id_str);
                    
                    let options = ListThreadsOptions::default();
                    
                    if let Ok(threads) = room.list_threads(options).await {
                        let guard = THREAD_LIST_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            let c_user_id = CString::new(crate::sanitize_string(&user_id_str)).unwrap_or_default();
                            let c_room_id = CString::new(crate::sanitize_string(&room_id_str)).unwrap_or_default();
                            for thread_root in threads.chunk {
                                let root_id = thread_root.event_id().map(|e| e.to_string()).unwrap_or_default();
                                let c_root_id = CString::new(crate::sanitize_string(&root_id)).unwrap_or_default();
                                
                                // Get a summary from the latest message in the thread
                                let mut body = String::new();
                                if let Some(ev) = thread_root.bundled_latest_thread_event {
                                    let raw_event: Result<AnySyncTimelineEvent, _> = ev.raw().deserialize();
                                    if let Ok(AnySyncTimelineEvent::MessageLike(msg_ev)) = raw_event {
                                        if let AnySyncMessageLikeEvent::RoomMessage(original) = msg_ev {
                                            body = original.as_original().map(|o| o.content.body().to_string()).unwrap_or_default();
                                        }
                                    }
                                }
                                
                                if body.is_empty() {
                                    body = "No message summary available".to_string();
                                }

                                let c_msg = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
                                cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_root_id.as_ptr(), c_msg.as_ptr(), 0, 0);
                            }
                            // End marker
                            cb(c_user_id.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), 0, 0);
                        }
                    } else {
                        log::error!("list_threads API failed for {}", room_id_str);
                        let c_user_id = CString::new(crate::sanitize_string(&user_id_str)).unwrap_or_default();
                        let c_room_id = CString::new(crate::sanitize_string(&room_id_str)).unwrap_or_default();
                        let guard = THREAD_LIST_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), 0, 0);
                        }
                    }
                }
            }
        });
    });
}


