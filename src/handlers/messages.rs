use matrix_sdk::ruma::events::room::message::{SyncRoomMessageEvent, OriginalSyncRoomMessageEvent, MessageType};
use matrix_sdk::ruma::events::room::message::Relation;
use matrix_sdk::{Room, media::{MediaFormat, MediaRequestParameters}};
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

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

        // Handle Reply Rendering (Simple Header)
        if let Some(Relation::Reply { in_reply_to }) = &ev.content.relates_to {
            final_body = format!("<font color=\"#777777\"><i>Replying to {}</i></font><br/>{}", in_reply_to.event_id, final_body);
        }

        // Apply Thread Styling
        if let Some(Relation::Thread(thread)) = ev.content.relates_to.clone() {
            // Indent threaded messages in the main chat
            final_body = format!("&nbsp;&nbsp;&nbsp;[Thread: {}] {}", &thread.event_id.as_str()[..8], final_body);
        } else if let Some(Relation::Replacement(_)) = ev.content.relates_to {
            final_body = format!("[Edited] {}", final_body);
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
     let body = format!("[Redaction] {} redacted a message.", sender);
     
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
                let filename = format!("/tmp/matrix_{}_{}", ev.event_id, content.body);
                
                if let Ok(mut file) = std::fs::File::create(&filename) {
                     use std::io::Write;
                     if file.write_all(&bytes).is_ok() {
                         media_path = Some(filename.clone());
                         // Wrap in <a> so it's clickable (as a hyperlink) AND shows inline.
                         body = format!("<a href=\"file://{}\"><img src=\"file://{}\" alt=\"{}\" style=\"max-width: 100%;\"></a>", filename, filename, content.body.replace("\"", "&quot;"));
                     }
                }
            }
            if media_path.is_none() { 
                body = format!("[Image: {}] (Download failed)", content.body); 
            }
        },
        MessageType::Video(content) => {
            log::info!("Downloading video: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let filename = format!("/tmp/matrix_{}_{}", ev.event_id, content.body);
                if let Ok(mut file) = std::fs::File::create(&filename) {
                    use std::io::Write;
                    if file.write_all(&bytes).is_ok() {
                        media_path = Some(filename);
                        body = format!("<a href=\"file://{}\">[Video: {}]</a>", media_path.as_ref().unwrap(), content.body.replace("\"", "&quot;"));
                    }
                }
            }
            if media_path.is_none() { body = format!("[Video: {}] (Download failed)", content.body); }
        },
        MessageType::File(content) => {
            log::info!("Downloading file: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let filename = format!("/tmp/matrix_{}_{}", ev.event_id, content.body);
                if let Ok(mut file) = std::fs::File::create(&filename) {
                    use std::io::Write;
                    if file.write_all(&bytes).is_ok() {
                        media_path = Some(filename.clone());
                        
                        // Check if it's an image
                        let is_image = content.info.as_ref().and_then(|i| i.mimetype.as_ref()).map(|m| m.starts_with("image/")).unwrap_or(false)
                            || content.body.to_lowercase().ends_with(".png")
                            || content.body.to_lowercase().ends_with(".jpg")
                            || content.body.to_lowercase().ends_with(".jpeg")
                            || content.body.to_lowercase().ends_with(".gif")
                            || content.body.to_lowercase().ends_with(".webp");

                        if is_image {
                            body = format!("<a href=\"file://{}\"><img src=\"file://{}\" alt=\"{}\" style=\"max-width: 100%;\"></a>", filename, filename, content.body.replace("\"", "&quot;"));
                        } else {
                            body = format!("<a href=\"file://{}\">[File: {}]</a>", filename, content.body.replace("\"", "&quot;"));
                        }
                    }
                }
            }
            if media_path.is_none() { body = format!("[File: {}] (Download failed)", content.body); }
        },
        MessageType::Audio(content) => {
            log::info!("Downloading audio: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let filename = format!("/tmp/matrix_{}_{}", ev.event_id, content.body);
                if let Ok(mut file) = std::fs::File::create(&filename) {
                    use std::io::Write;
                    if file.write_all(&bytes).is_ok() {
                        media_path = Some(filename);
                        body = format!("<a href=\"file://{}\">[Audio: {}]</a>", media_path.as_ref().unwrap(), content.body.replace("\"", "&quot;"));
                    }
                }
            }
            if media_path.is_none() { body = format!("[Audio: {}] (Download failed)", content.body); }
        },
        MessageType::Location(content) => {
            let geo = &content.geo_uri;
            // geo:52.5186,13.3761;u=35
            let parts: Vec<&str> = geo.trim_start_matches("geo:").split(';').next().unwrap_or("").split(',').collect();
            if parts.len() >= 2 {
                let lat = parts[0];
                let lon = parts[1];
                body = format!("[Location: {}] <a href=\"https://www.openstreetmap.org/?mlat={}&mlon={}#map=16/{}/{}\">(View on Map)</a>", 
                    content.body, lat, lon, lat, lon);
            } else {
                body = format!("[Location: {}] ({})", content.body, geo);
            }
        },
        _ => {
            body = crate::get_display_html(&ev.content);
        }
    }
    body
}
