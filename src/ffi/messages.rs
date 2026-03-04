use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use crate::RUNTIME;
use matrix_sdk::Client;
use matrix_sdk::ruma::RoomId;
use matrix_sdk::ruma::EventId;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_message(user_id: *const c_char, room_id: *const c_char, message: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || message.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let message_str = unsafe { CStr::from_ptr(message).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let content = crate::create_message_content(message_str);
                        let _ = room.send(content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_im(user_id: *const c_char, target_user_id: *const c_char, message: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || target_user_id.is_null() || message.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
        let message_str = unsafe { CStr::from_ptr(message).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                    match client.create_dm(uid).await {
                        Ok(room) => {
                            let content = crate::create_message_content(message_str);
                            let _ = room.send(content).await;
                        },
                        Err(e) => log::error!("Failed to resolve DM for {}: {:?}", target_user_id_str, e),
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_reaction(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, key: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || event_id.is_null() || key.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
        let key_str = unsafe { CStr::from_ptr(key).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::events::reaction::ReactionEventContent;
                use matrix_sdk::ruma::events::relation::Annotation;
                if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let rel = Annotation::new(eid.to_owned(), key_str);
                        let content = ReactionEventContent::new(rel);
                        let _ = room.send(content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_redact_event(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || event_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.redact(eid, reason_str.as_deref(), None).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_reply(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, text: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || event_id.is_null() || text.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
        let text_str = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::events::room::message::{RoomMessageEventContent, ForwardThread, AddMentions};
                use matrix_sdk::ruma::events::AnySyncTimelineEvent;

                if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        if let Ok(parent_ev) = room.event(eid, None).await {
                            // Fix: Use raw event directly for reply construction if into_full_event is missing
                            let raw_json = parent_ev.raw().json().get().to_string();
                            if let Ok(m) = serde_json::from_str::<matrix_sdk::ruma::events::room::message::OriginalRoomMessageEvent>(&raw_json) {
                                let content = RoomMessageEventContent::text_plain(text_str.clone());
                                let reply_content = content.make_reply_to(&m, ForwardThread::No, AddMentions::No);
                                let _ = room.send(reply_content).await;
                            }
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_edit(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, text: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || event_id.is_null() || text.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
        let text_str = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        if let Ok(parent_ev) = room.event(eid, None).await {
                            let raw_json = parent_ev.raw().json().get().to_string();
                            if let Ok(m) = serde_json::from_str::<matrix_sdk::ruma::events::room::message::OriginalRoomMessageEvent>(&raw_json) {
                                let new_content = crate::create_message_content(text_str);
                                let edit_content = new_content.make_replacement(&m);
                                let _ = room.send(edit_content).await;
                            }
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_typing(user_id: *const c_char, room_id: *const c_char, is_typing: bool) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.typing_notice(is_typing).await;
                    }
                }
            });
        });
    })
}
