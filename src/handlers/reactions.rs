use matrix_sdk::ruma::events::reaction::SyncReactionEvent;
use matrix_sdk::ruma::events::sticker::SyncStickerEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_reaction(event: SyncReactionEvent, room: Room) {
    if let Some(original_event) = event.as_original() {
        let sender = original_event.sender.as_str();
        let room_id = room.room_id().as_str();
        let emoji = &original_event.content.relates_to.key;
        
        let body = format!("[Reaction] {} reacted with {}", sender, emoji);
        log::info!("Reaction in {}: {} -> {}", room_id, sender, emoji);

        let c_sender = CString::new(sender).unwrap_or_default();
        let c_body = CString::new(body).unwrap_or_default();
        let c_room_id = CString::new(room_id).unwrap_or_default();
        
        let timestamp: u64 = original_event.origin_server_ts.0.into();
        
        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
        }
    }
}

pub async fn handle_sticker(event: SyncStickerEvent, room: Room) {
    if let Some(original_event) = event.as_original() {
        let sender = original_event.sender.as_str();
        let room_id = room.room_id().as_str();
        let content = &original_event.content;
        
        log::info!("Sticker in {}: {}", room_id, content.body);
        
        use matrix_sdk::media::{MediaFormat, MediaRequestParameters};
        use base64::Engine as _;
        
        let request = MediaRequestParameters { source: content.source.clone().into(), format: MediaFormat::File };
        let body = if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
             let b64 = base64::engine::general_purpose::STANDARD.encode(&bytes);
             // Default mime for stickers usually png/webp
             let mime = "image/png"; 
             format!("<img src=\"data:{};base64,{}\" alt=\"[Sticker: {}]\">", mime, b64, content.body)
        } else {
             format!("[Sticker: {}]", content.body)
        };

        let c_sender = CString::new(sender).unwrap_or_default();
        let c_body = CString::new(body).unwrap_or_default();
        let c_room_id = CString::new(room_id).unwrap_or_default();
        let c_event_id = CString::new(original_event.event_id.as_str()).unwrap_or_default();
        
        let timestamp: u64 = original_event.origin_server_ts.0.into();
        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
        }
    }
}
