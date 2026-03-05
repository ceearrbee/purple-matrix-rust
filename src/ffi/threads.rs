use std::os::raw::c_char;
use std::ffi::CStr;
use crate::ffi::{with_client, send_event, events::FfiEvent};
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
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::room::message::Relation;
                use matrix_sdk::ruma::events::AnySyncTimelineEvent;
                use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let mut options = matrix_sdk::room::MessagesOptions::backward();
                        options.limit = matrix_sdk::ruma::uint!(100);
                        if let Ok(resp) = room.messages(options).await {
                            let mut threads = std::collections::HashSet::new();
                            for ev in resp.chunk {
                                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(msg_ev))) = ev.raw().deserialize() {
                                    if let matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent::Original(orig) = msg_ev {
                                        if let Some(Relation::Thread(thread)) = &orig.content.relates_to {
                                            let root_id = thread.event_id.to_string();
                                            if !threads.contains(&root_id) {
                                                let latest_msg = crate::get_display_html(&orig.content);
                                                let event = FfiEvent::ThreadList {
                                                    user_id: uid_async.clone(),
                                                    room_id: room_id_str.clone(),
                                                    thread_root_id: root_id.clone(),
                                                    latest_msg,
                                                };
                                                send_event(event);
                                                threads.insert(root_id);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            });
        });
    })
}
