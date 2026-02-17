use matrix_sdk::ruma::events::reaction::SyncReactionEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_reaction(event: SyncReactionEvent, room: Room) {
    if let SyncReactionEvent::Original(ev) = event {
        let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let target_id = ev.content.relates_to.event_id.as_str();
        let emoji = ev.content.relates_to.key.as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();

        log::info!("Reaction in {}: {} reacted {} to {}", room_id, sender, emoji, target_id);

        let body = format!("[Reaction] {} reacted with {}", sender, emoji);
        
        let c_user_id = CString::new(crate::sanitize_string(&user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
        }
    }
}

pub async fn handle_sticker(event: matrix_sdk::ruma::events::sticker::SyncStickerEvent, room: Room) {
    if let matrix_sdk::ruma::events::sticker::SyncStickerEvent::Original(ev) = event {
        let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        
        let body = format!("[Sticker] {}", ev.content.body);
        
        let c_user_id = CString::new(crate::sanitize_string(&user_id)).unwrap_or_default();
        let c_sender = CString::new(crate::sanitize_string(sender)).unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
        }
    }
}
