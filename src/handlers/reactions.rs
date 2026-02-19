use std::ffi::CString;
use matrix_sdk::Room;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_reaction(event: matrix_sdk::ruma::events::reaction::SyncReactionEvent, room: Room) {
    if let matrix_sdk::ruma::events::reaction::SyncReactionEvent::Original(ev) = event {
        log::info!("Reaction received: {} from {}", ev.content.relates_to.key, ev.sender);
        
        let client = room.client();
        let me = client.user_id().unwrap();
        let local_user_id = me.as_str().to_string();
        let sender = ev.sender.as_str();
        
        // Don't notify for our own reactions
        if sender == local_user_id { return; }

        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let body = format!("[Reaction] {} reacted with {}", sender, ev.content.relates_to.key);
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
        
        let c_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp, is_encrypted);
        }
    }
}

pub async fn handle_sticker(event: matrix_sdk::ruma::events::sticker::SyncStickerEvent, room: Room) {
    if let matrix_sdk::ruma::events::sticker::SyncStickerEvent::Original(ev) = event {
        let client = room.client();
        let me = client.user_id().unwrap();
        let local_user_id = me.as_str().to_string();
        let sender = ev.sender.as_str();
        
        if sender == local_user_id { return; }

        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let body = format!("[Sticker] {}", ev.content.body);
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.unwrap_or(None).is_some();
        
        let c_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new(crate::sanitize_string(sender)).unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp, is_encrypted);
        }
    }
}
