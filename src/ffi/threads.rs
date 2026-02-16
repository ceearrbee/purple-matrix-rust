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
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::room::MessagesOptions;
            use matrix_sdk::ruma::events::room::message::{Relation, SyncRoomMessageEvent};

            if let Ok(room_id) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(room_id) {
                    log::info!("Listing threads for {}", room_id_str);
                    
                    let mut options = MessagesOptions::backward();
                    options.limit = 100u16.into();
                    
                    if let Ok(messages) = room.messages(options).await {
                        let mut thread_roots = std::collections::HashMap::new();
                        for ev in messages.chunk {
                            if let Ok(raw) = ev.raw().deserialize() {
                                if let matrix_sdk::ruma::events::AnySyncTimelineEvent::MessageLike(msg_ev) = raw {
                                    if let matrix_sdk::ruma::events::AnySyncMessageLikeEvent::RoomMessage(SyncRoomMessageEvent::Original(original)) = msg_ev {
                                        if let Some(Relation::Thread(thread)) = original.content.relates_to {
                                            let root_id = thread.event_id.to_string();
                                            let count = thread_roots.entry(root_id).or_insert(0u64);
                                            *count += 1;
                                        }
                                    }
                                }
                            }
                        }

                        let guard = THREAD_LIST_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            let c_room_id = CString::new(room_id_str.clone()).unwrap_or_default();
                            for (root_id, count) in thread_roots {
                                let c_root_id = CString::new(root_id).unwrap_or_default();
                                let c_msg = CString::new(format!("Thread with {} messages", count)).unwrap_or_default();
                                cb(c_room_id.as_ptr(), c_root_id.as_ptr(), c_msg.as_ptr(), count, 0);
                            }
                        }
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_thread_list_callback(cb: ThreadListCallback) {
    let mut guard = THREAD_LIST_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
