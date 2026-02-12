use matrix_sdk::ruma::events::room::message::{SyncRoomMessageEvent, OriginalSyncRoomMessageEvent, MessageType};
use matrix_sdk::ruma::events::room::message::Relation;
use matrix_sdk::{Room, media::{MediaFormat, MediaRequestParameters}};
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

fn sanitize_filename_component(input: &str, fallback: &str) -> String {
    let mut out: String = input
        .chars()
        .map(|c| {
            if c.is_ascii_alphanumeric() || matches!(c, '.' | '_' | '-') {
                c
            } else {
                '_'
            }
        })
        .collect();
    out = out.trim_matches('_').trim_matches('.').to_string();
    if out.is_empty() {
        fallback.to_string()
    } else if out.len() > 80 {
        out[..80].to_string()
    } else {
        out
    }
}

fn build_safe_media_path(event_id: &str, kind: &str, original_name: &str) -> String {
    let safe_event = sanitize_filename_component(event_id, "event");
    let safe_kind = sanitize_filename_component(kind, "media");
    let safe_name = sanitize_filename_component(original_name, "file");
    format!("/tmp/matrix_{}_{}_{}", safe_event, safe_kind, safe_name)
}

pub async fn handle_room_message(event: SyncRoomMessageEvent, room: Room) {
    if let SyncRoomMessageEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        
        // Suppress own messages (Local Echo Handles Display)
        if let Some(me) = room.client().user_id() {
             if ev.sender == me {
                 // Only suppress if it looks like a local echo (has transaction ID)
                 // Messages from other sessions usually won't have a transaction ID in the sync response.
                 if ev.unsigned.transaction_id.is_some() {
                     log::debug!("Ignoring own message (local echo) event: {}", ev.event_id);
                     return;
                 }
                 log::info!("Processing own message from another session: {}", ev.event_id);
             }
        }

        let mut thread_root_id: Option<String> = None;
        
        let body = render_room_message(&ev, &room).await;

        log::info!("[Rust] Inspecting event {} from {} for relations. Has relates_to: {}", ev.event_id, sender, ev.content.relates_to.is_some());
        
        // Check for Thread Relation
        if let Some(Relation::Thread(thread)) = ev.content.relates_to.clone() {
            log::info!("[Rust] Thread relation detected! Root: {}", thread.event_id);
            thread_root_id = Some(thread.event_id.to_string());
        }

        let mut final_body = body;

        // Handle Reply Rendering (Blockquote)
        if let Some(Relation::Reply { in_reply_to }) = &ev.content.relates_to {
            let style = "border-left: 3px solid #ccc; padding-left: 8px; margin-left: 5px; color: #666; font-size: 90%;";
            // Check if we can get the actual replied-to event? (Future improvement)
            final_body = format!(
                "<div style=\"{}\">Replying to msg {}</div><br/>{}",
                style,
                crate::escape_html(in_reply_to.event_id.as_str()),
                final_body
            );
        }

        // Apply Thread Styling
        if let Some(Relation::Thread(_)) = ev.content.relates_to.clone() {
            final_body = format!("&nbsp;&nbsp;🧵 {}", final_body);
        } else if let Some(Relation::Replacement(_)) = ev.content.relates_to {
            final_body = format!("<span style=\"color: #888; font-size: 80%;\">[Edited]</span> {}", final_body);
        }
        
        log::info!("Received msg from {} in {}: {}", sender, room_id, final_body);

        let c_sender = CString::new(sender).unwrap_or_default();
        let c_body = CString::new(final_body).unwrap_or_default();
        let c_room_id = CString::new(room_id).unwrap_or_default();
        let c_event_id = CString::new(ev.event_id.as_str()).unwrap_or_default();
        
        let c_thread_id = if let Some(tid) = thread_root_id {
            CString::new(tid).unwrap_or_default()
        } else {
            CString::default() 
        };

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            let thread_ptr = if c_thread_id.as_bytes().is_empty() {
                std::ptr::null()
            } else {
                c_thread_id.as_ptr()
            };
            cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), thread_ptr, c_event_id.as_ptr(), timestamp);
        }
    }
}

pub async fn handle_encrypted(event: matrix_sdk::ruma::events::room::encrypted::SyncRoomEncryptedEvent, room: Room) {
     use matrix_sdk::ruma::events::room::encrypted::SyncRoomEncryptedEvent;
     
     if let SyncRoomEncryptedEvent::Original(ev) = event {
         let sender = ev.sender.as_str();
         let room_id = room.room_id().as_str();
         let timestamp: u64 = ev.origin_server_ts.0.into();
         
         log::info!("Received Encrypted Event in {} from {}. Scheme: {:?}", room_id, sender, ev.content.scheme);

         // If we are here, it means the SDK could not decrypt it automatically.
         let body = "[Encrypted Message: Waiting for keys...] ".to_string();
         
         let c_sender = CString::new(sender).unwrap_or_default();
         let c_body = CString::new(body).unwrap_or_default();
         let c_room_id = CString::new(room_id).unwrap_or_default();
         let c_event_id = CString::new(ev.event_id.as_str()).unwrap_or_default();

         let guard = MSG_CALLBACK.lock().unwrap();
         if let Some(cb) = *guard {
             cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
         }
     }
}

pub async fn handle_redaction(event: matrix_sdk::ruma::events::room::redaction::SyncRoomRedactionEvent, room: Room) {
     let sender = event.sender().as_str();
     let room_id = room.room_id().as_str();
     let timestamp: u64 = event.origin_server_ts().0.into();
     let body = format!(
         "<span style=\"color: #999; font-style: italic;\">[Redacted] {} removed a message.</span>",
         crate::escape_html(sender)
     );
     
     let c_sender = CString::new(sender).unwrap_or_default();
     let c_body = CString::new(body).unwrap_or_default();
     let c_room_id = CString::new(room_id).unwrap_or_default();
     
     let guard = MSG_CALLBACK.lock().unwrap();
     if let Some(cb) = *guard {
         cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
     }
}

pub async fn render_room_message(ev: &OriginalSyncRoomMessageEvent, room: &Room) -> String {
    let mut body = String::new();
    let mut media_path: Option<String> = None;

    match &ev.content.msgtype {
        MessageType::Image(content) => {
            log::info!("Downloading image for display: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let filename = build_safe_media_path(ev.event_id.as_str(), "image", &content.body);
                
                // Use tokio::fs for non-blocking I/O
                use tokio::io::AsyncWriteExt;
                if let Ok(mut file) = tokio::fs::File::create(&filename).await {
                     if file.write_all(&bytes).await.is_ok() {
                         media_path = Some(filename.clone());
                         // Wrap in <a> so it's clickable (as a hyperlink) AND shows inline.
                         body = format!(
                             "<a href=\"file://{}\"><img src=\"file://{}\" alt=\"{}\" style=\"max-width: 100%;\"></a>",
                             filename,
                             filename,
                             crate::escape_html(&content.body)
                         );
                     }
                }
            }
            if media_path.is_none() { 
                body = format!("[Image: {}] (Download failed)", crate::escape_html(&content.body)); 
            }
        },
        MessageType::Video(content) => {
            log::info!("Downloading video: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let filename = build_safe_media_path(ev.event_id.as_str(), "video", &content.body);
                use tokio::io::AsyncWriteExt;
                if let Ok(mut file) = tokio::fs::File::create(&filename).await {
                    if file.write_all(&bytes).await.is_ok() {
                        media_path = Some(filename);
                        body = format!(
                            "<a href=\"file://{}\">[Video: {}]</a>",
                            media_path.as_ref().unwrap(),
                            crate::escape_html(&content.body)
                        );
                    }
                }
            }
            if media_path.is_none() { body = format!("[Video: {}] (Download failed)", crate::escape_html(&content.body)); }
        },
        MessageType::File(content) => {
            log::info!("Downloading file: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let filename = build_safe_media_path(ev.event_id.as_str(), "file", &content.body);
                use tokio::io::AsyncWriteExt;
                if let Ok(mut file) = tokio::fs::File::create(&filename).await {
                    if file.write_all(&bytes).await.is_ok() {
                        media_path = Some(filename.clone());
                        
                        // Check if it's an image
                        let is_image = content.info.as_ref().and_then(|i| i.mimetype.as_ref()).map(|m| m.starts_with("image/")).unwrap_or(false)
                            || content.body.to_lowercase().ends_with(".png")
                            || content.body.to_lowercase().ends_with(".jpg")
                            || content.body.to_lowercase().ends_with(".jpeg")
                            || content.body.to_lowercase().ends_with(".gif")
                            || content.body.to_lowercase().ends_with(".webp");

                        if is_image {
                            body = format!(
                                "<a href=\"file://{}\"><img src=\"file://{}\" alt=\"{}\" style=\"max-width: 100%;\"></a>",
                                filename,
                                filename,
                                crate::escape_html(&content.body)
                            );
                        } else {
                            body = format!(
                                "<a href=\"file://{}\">[File: {}]</a>",
                                filename,
                                crate::escape_html(&content.body)
                            );
                        }
                    }
                }
            }
            if media_path.is_none() { body = format!("[File: {}] (Download failed)", crate::escape_html(&content.body)); }
        },
        MessageType::Audio(content) => {
            log::info!("Downloading audio: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                
                let is_voice = content.voice.is_some(); // MSC3245
                let duration = content.info.as_ref().and_then(|i| i.duration).map(|d| d.as_secs()).unwrap_or(0);
                
                let filename_prefix = if is_voice { "voice_message" } else { "audio" };
                let filename = build_safe_media_path(ev.event_id.as_str(), filename_prefix, &content.body);
                
                use tokio::io::AsyncWriteExt;
                if let Ok(mut file) = tokio::fs::File::create(&filename).await {
                    if file.write_all(&bytes).await.is_ok() {
                        media_path = Some(filename);
                        
                        let label = if is_voice {
                             let mins = duration / 60;
                             let secs = duration % 60;
                             format!("[Voice Message - {}:{:02}]", mins, secs)
                        } else {
                             format!("[Audio: {}]", crate::escape_html(&content.body))
                        };
                        
                        body = format!("<a href=\"file://{}\">{}</a>", media_path.as_ref().unwrap(), label);
                    }
                }
            }
            if media_path.is_none() { 
                let label = if content.voice.is_some() { "[Voice Message]" } else { "[Audio]" };
                body = format!("{} {} (Download failed)", label, crate::escape_html(&content.body)); 
            }
        },
        MessageType::Location(content) => {
            let geo = &content.geo_uri;
            // geo:52.5186,13.3761;u=35
            let parts: Vec<&str> = geo.trim_start_matches("geo:").split(';').next().unwrap_or("").split(',').collect();
            if parts.len() >= 2 {
                let lat = parts[0];
                let lon = parts[1];
                body = format!("[Location: {}] <a href=\"https://www.openstreetmap.org/?mlat={}&mlon={}#map=16/{}/{}\">(View on Map)</a>", 
                    crate::escape_html(&content.body), crate::escape_html(lat), crate::escape_html(lon), crate::escape_html(lat), crate::escape_html(lon));
            } else {
                body = format!("[Location: {}] ({})", crate::escape_html(&content.body), crate::escape_html(geo));
            }
        },
        MessageType::Notice(content) => {
             body = format!("<span style=\"color: #666;\">{}</span>", crate::escape_html(&content.body));
        },
        _ => {
            body = crate::get_display_html(&ev.content);
        }
    }
    body
}

#[cfg(test)]
mod tests {
    use super::{build_safe_media_path, sanitize_filename_component};

    #[test]
    fn test_sanitize_filename_component_blocks_path_chars() {
        let s = sanitize_filename_component("../etc/passwd", "fallback");
        assert!(!s.contains('/'));
        assert!(!s.contains('\\'));
        assert_ne!(s, "..");
    }

    #[test]
    fn test_build_safe_media_path_stays_in_tmp() {
        let p = build_safe_media_path("$event:server", "image", "../../evil.png");
        assert!(p.starts_with("/tmp/matrix_"));
        assert!(!p.contains(".."));
    }
}
