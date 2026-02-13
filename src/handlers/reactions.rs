use matrix_sdk::ruma::events::reaction::SyncReactionEvent;
use matrix_sdk::ruma::events::sticker::SyncStickerEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_reaction(event: SyncReactionEvent, room: Room) {
    if let Some(original_event) = event.as_original() {
        let sender_id = original_event.sender.as_str();
        let mut sender_display = sender_id.to_string();
        
        if let Ok(Some(member)) = room.get_member(&original_event.sender).await {
            if let Some(dn) = member.display_name() {
                 sender_display = dn.to_owned();
            }
        }
        
        let room_id = room.room_id().as_str();
        let emoji = &original_event.content.relates_to.key;
        let target_event_id = original_event.content.relates_to.event_id.as_str();
        
        // Update global reaction map
        {
            let mut guard = crate::REACTIONS.lock().unwrap();
            let event_reactions = guard.entry(target_event_id.to_string()).or_default();
            let count = event_reactions.entry(emoji.clone()).or_insert(0);
            *count += 1;
            
            let mut summary: Vec<String> = event_reactions.iter().map(|(e, c)| format!("{} {}", e, c)).collect();
            summary.sort();
            let summary_str = summary.join(", ");
            
            log::info!("Reaction update for {}: {}", target_event_id, summary_str);
            
            // Notify UI via a notice indicating reaction summary
            let update_msg = format!("<span style=\"color: #888; font-size: 85%; font-style: italic;\">Reactions: {}</span>", summary_str);
            crate::ffi::send_system_message_to_room(room.client().user_id().map(|u| u.as_str()).unwrap_or(""), room_id, &update_msg);
        }

        // Use italics and grey color for reactions to make them less intrusive
        let body = format!("<span style=\"color: #888; font-style: italic;\">* {} reacted with {}</span>", crate::escape_html(&sender_display), emoji);
        log::info!("Reaction in {}: {} -> {}", room_id, sender_display, emoji);

        let c_sender = CString::new(original_event.sender.as_str()).unwrap_or_default();
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

        
        let request = MediaRequestParameters { source: content.source.clone().into(), format: MediaFormat::File };
        
        // Generate a safe filename for the sticker
        let sticker_id = original_event.event_id.as_str().replace(":", "_").replace("$", ""); // Event ID is unique
        let mut path_buf = crate::media_helper::get_media_dir();
        path_buf.push(format!("sticker_{}.png", sticker_id));
        let path = path_buf.as_path();
        
        let mut sticker_uri = String::new();
        let mut downloaded = false;
        
        if path.exists() {
             sticker_uri = format!("file://{}", path.to_string_lossy());
             downloaded = true;
        } else {
             // Directory guaranteed by get_media_dir
             if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                  if let Ok(mut file) = std::fs::File::create(&path) {
                       use std::io::Write;
                       if file.write_all(&bytes).is_ok() {
                           sticker_uri = format!("file://{}", path.to_string_lossy());
                           downloaded = true;
                       }
                  }
             }
        }

        let body = if downloaded {
             format!("<img src=\"{}\" alt=\"[Sticker: {}]\">", sticker_uri, content.body)
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
