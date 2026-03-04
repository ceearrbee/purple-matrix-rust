use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_threads(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            let client_clone = client.clone();
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent};

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client_clone.get_room(rid) {
                        log::info!("Listing all threads for {} using server API", room_id_str);
                        
                        let mut options = matrix_sdk::room::MessagesOptions::backward();
                        options.limit = matrix_sdk::ruma::uint!(100);
                        
                        if let Ok(resp) = room.messages(options).await {
                            let mut found_roots = std::collections::HashSet::new();
                            for event in resp.chunk {
                                // Fix: use raw event to get timestamp if method is missing
                                let ts: u64 = if let Ok(val) = event.raw().deserialize_as::<serde_json::Value>() {
                                    val.get("origin_server_ts").and_then(|v| v.as_u64()).unwrap_or(0)
                                } else {
                                    0
                                };

                                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(msg))) = event.raw().deserialize() {
                                    if let Some(rel) = msg.as_original().and_then(|o| o.content.relates_to.as_ref()) {
                                        use matrix_sdk::ruma::events::room::message::Relation;
                                        let root_id = match rel {
                                            Relation::Thread(t) => Some(t.event_id.to_string()),
                                            Relation::Reply { in_reply_to } => Some(in_reply_to.event_id.to_string()),
                                            _ => None,
                                        };

                                        if let Some(rid) = root_id {
                                            if !found_roots.contains(&rid) {
                                                found_roots.insert(rid.clone());
                                                
                                                let latest_msg = match msg.as_original() {
                                                    Some(o) => o.content.body().to_string(),
                                                    None => "Thread message".to_string(),
                                                };

                                                let event = crate::ffi::FfiEvent::ThreadList {
                                                    user_id: uid_async.clone(),
                                                    room_id: room_id_str.clone(),
                                                    thread_root_id: Some(rid),
                                                    latest_msg: Some(latest_msg),
                                                    count: 0,
                                                    ts,
                                                };
                                                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // End of list marker
                        let event = crate::ffi::FfiEvent::ThreadList {
                            user_id: uid_async.clone(),
                            room_id: room_id_str.clone(),
                            thread_root_id: None,
                            latest_msg: None,
                            count: 0,
                            ts: 0,
                        };
                        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                    }
                }
            });
        });
    })
}
