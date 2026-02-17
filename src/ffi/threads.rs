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
                    
                    let mut from_token: Option<String> = None;
                    let mut all_threads_found = false;

                    while !all_threads_found {
                        let mut options = ListThreadsOptions::default();
                        options.from = from_token.clone();

                        if let Ok(threads) = room.list_threads(options).await {
                            let guard = THREAD_LIST_CALLBACK.lock().unwrap();
                            if let Some(cb) = *guard {
                                let c_user_id = CString::new(crate::sanitize_string(&user_id_str)).unwrap_or_default();
                                let c_room_id = CString::new(crate::sanitize_string(&room_id_str)).unwrap_or_default();
                                
                                for thread_root in &threads.chunk {
                                    let root_id = thread_root.event_id().map(|e| e.to_string()).unwrap_or_default();
                                    let c_root_id = CString::new(crate::sanitize_string(&root_id)).unwrap_or_default();
                                    
                                    // 1. Get First Message (the root event itself)
                                    let mut first_msg = String::new();
                                    let raw_root: Result<AnySyncTimelineEvent, _> = thread_root.raw().deserialize();
                                    if let Ok(AnySyncTimelineEvent::MessageLike(msg_ev)) = raw_root {
                                        if let AnySyncMessageLikeEvent::RoomMessage(original) = msg_ev {
                                            first_msg = original.as_original().map(|o| o.content.body().to_string()).unwrap_or_default();
                                        }
                                    }
                                    if first_msg.is_empty() { first_msg = "Root message unavailable".to_string(); }

                                    // 2. Get Latest Message
                                    let mut latest_msg = String::new();
                                    if let Some(ev) = &thread_root.bundled_latest_thread_event {
                                        let raw_event: Result<AnySyncTimelineEvent, _> = ev.raw().deserialize();
                                        if let Ok(AnySyncTimelineEvent::MessageLike(msg_ev)) = raw_event {
                                            if let AnySyncMessageLikeEvent::RoomMessage(original) = msg_ev {
                                                latest_msg = original.as_original().map(|o| o.content.body().to_string()).unwrap_or_default();
                                            }
                                        }
                                    }
                                    if latest_msg.is_empty() { latest_msg = "No replies yet".to_string(); }

                                    // Descriptive label
                                    let body = format!("Start: {} ... End: {}", first_msg, latest_msg);

                                    // Placeholder for msg_count while we debug enum structure
                                    log::info!("DEBUG: thread_summary structure for {}: {:?}", root_id, thread_root.thread_summary);
                                    let msg_count: u64 = 0; 

                                    let ts: u64 = thread_root.timestamp().map(|t| t.0.into()).unwrap_or(0);

                                    let c_msg = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
                                    cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_root_id.as_ptr(), c_msg.as_ptr(), msg_count, ts);
                                }
                            }
                            
                            if let Some(next_token) = threads.prev_batch_token {
                                from_token = Some(next_token);
                            } else {
                                all_threads_found = true;
                            }
                        } else {
                            log::error!("list_threads API failed for {}", room_id_str);
                            all_threads_found = true; 
                        }
                    }
                    
                    let guard = THREAD_LIST_CALLBACK.lock().unwrap();
                    if let Some(cb) = *guard {
                        let c_user_id = CString::new(crate::sanitize_string(&user_id_str)).unwrap_or_default();
                        let c_room_id = CString::new(crate::sanitize_string(&room_id_str)).unwrap_or_default();
                        cb(c_user_id.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), 0, 0);
                    }
                }
            }
        });
    });
}
