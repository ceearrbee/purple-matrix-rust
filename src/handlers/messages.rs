use matrix_sdk::ruma::events::room::message::{SyncRoomMessageEvent, Relation};
use matrix_sdk::Room;

pub async fn handle_room_message(event: matrix_sdk::ruma::serde::Raw<SyncRoomMessageEvent>, room: Room) {
    if let Ok(SyncRoomMessageEvent::Original(ev)) = event.deserialize() {
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let client = room.client();
        let me = match client.user_id() {
            Some(u) => u.as_str().to_string(),
            None => {
                log::warn!("Ignored message: Client user_id is missing");
                return;
            }
        };
        let local_user_id = me.clone();

        if sender == local_user_id { return; }

        let mut thread_root_id: Option<String> = None;
        let body = render_room_message(&ev, &room).await;

        if let Some(Relation::Thread(ref thread)) = ev.content.relates_to {
            thread_root_id = Some(thread.event_id.to_string());
        }

        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();

        log::info!("Received msg from {} in {}: {} (Thread: {:?}, Enc: {})", sender, room_id, body, thread_root_id, is_encrypted);

        let target_room_id = if let Some(ref tid) = thread_root_id {
            format!("{}|{}", room_id, tid)
        } else {
            room_id.to_string()
        };
        
        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: sender.to_string(),
            msg: body,
            room_id: Some(target_room_id),
            thread_root_id,
            event_id: ev.event_id.to_string(),
            timestamp,
            encrypted: is_encrypted,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_sticker(event: matrix_sdk::ruma::events::sticker::SyncStickerEvent, room: Room) {
    if let matrix_sdk::ruma::events::sticker::SyncStickerEvent::Original(ev) = event {
        let client = room.client();
        let me = match client.user_id() {
            Some(u) => u.as_str().to_string(),
            None => {
                log::warn!("Ignored sticker: Client user_id is missing");
                return;
            }
        };
        let local_user_id = me.clone();
        let sender = ev.sender.as_str();
        
        if sender == local_user_id { return; }

        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let body = format!("[Sticker] {}", ev.content.body);
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
        
        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: sender.to_string(),
            msg: body,
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: ev.event_id.to_string(),
            timestamp,
            encrypted: is_encrypted,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_encrypted(event: matrix_sdk::ruma::serde::Raw<matrix_sdk::ruma::events::room::encrypted::SyncRoomEncryptedEvent>, room: Room) {
     use matrix_sdk::ruma::events::AnySyncTimelineEvent;
     use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;
     
     let raw_original = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::encrypted::OriginalSyncRoomEncryptedEvent>::from_json_string(event.json().get().to_string()).unwrap();
     match room.decrypt_event(&raw_original, None).await {
         Ok(decrypted) => {
             if let Ok(any_event) = decrypted.raw().deserialize() {
                 if let AnySyncTimelineEvent::MessageLike(msg_like) = any_event {
                     match msg_like {
                         AnySyncMessageLikeEvent::RoomMessage(_) => {
                             let raw_msg = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent>::from_json_string(decrypted.raw().json().get().to_string()).unwrap();
                             handle_room_message(raw_msg, room).await;
                             return;
                         },
                         AnySyncMessageLikeEvent::Sticker(sticker_ev) => {
                             handle_sticker(sticker_ev, room).await;
                             return;
                         },
                         _ => {}
                     }
                 }
             }
         },
         Err(e) => {
             log::warn!("Failed to decrypt live event: {:?}", e);
         }
     }

     if let Ok(ev) = event.deserialize() {
         let client = room.client();
         let me = match client.user_id() {
             Some(u) => u.as_str().to_string(),
             None => {
                 log::warn!("Ignored encrypted message: Client user_id is missing");
                 return;
             }
         };
         let local_user_id = me.clone();
         let sender = ev.sender().as_str();
         
         if sender == local_user_id { return; }

         let room_id = room.room_id().as_str();
         let timestamp: u64 = ev.origin_server_ts().0.into();
         
         let body = "[Encrypted]".to_string();
         
         let event = crate::ffi::FfiEvent::MessageReceived {
             user_id: local_user_id,
             sender: sender.to_string(),
             msg: body,
             room_id: Some(room_id.to_string()),
             thread_root_id: None,
             event_id: ev.event_id().to_string(),
             timestamp,
             encrypted: true,
         };
         let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
     }
}

pub async fn handle_redaction(event: matrix_sdk::ruma::events::room::redaction::SyncRoomRedactionEvent, room: Room) {
    if let Some(ev) = event.as_original() {
        let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let room_id = room.room_id().as_str();
        let target_event_id = ev.redacts.as_ref().map(|id| id.as_str()).unwrap_or("");
        
        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id,
            sender: "System".to_string(),
            msg: format!("[Redaction] Message {} was removed.", target_event_id),
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: ev.event_id.to_string(),
            timestamp: ev.origin_server_ts.0.into(),
            encrypted: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

fn sanitize_mime_ext(mime: &str, default: &str) -> String {
    let mut ext = mime.split('/').last().unwrap_or(default).to_string();
    ext.retain(|c| c.is_ascii_alphanumeric());
    if ext.is_empty() { default.to_string() } else { ext }
}

pub async fn render_room_message(ev: &matrix_sdk::ruma::events::room::message::OriginalSyncRoomMessageEvent, room: &Room) -> String {
    use matrix_sdk::ruma::events::room::message::MessageType;
    use matrix_sdk::media::{MediaFormat, MediaRequestParameters};

    match &ev.content.msgtype {
        MessageType::Image(content) => {
            log::debug!("Attempting to render image: {}", content.body);
            
            // Try to get a thumbnail first for better performance in the chat window
            let request = if let Some(info) = &content.info {
                if let Some(thumbnail_source) = &info.thumbnail_source {
                    use matrix_sdk::media::MediaThumbnailSettings;
                    use matrix_sdk::ruma::uint;
                    let settings = MediaThumbnailSettings::new(uint!(300), uint!(300));
                    MediaRequestParameters {
                        source: thumbnail_source.clone(),
                        format: MediaFormat::Thumbnail(settings),
                    }
                } else {
                    MediaRequestParameters {
                        source: content.source.clone(),
                        format: MediaFormat::File,
                    }
                }
            } else {
                MediaRequestParameters {
                    source: content.source.clone(),
                    format: MediaFormat::File,
                }
            };
            
            match room.client().media().get_media_content(&request, true).await {
                Ok(bytes) => {
                    let cb_opt = {
                        let guard = crate::ffi::IMGSTORE_ADD_CALLBACK.lock().unwrap();
                        *guard
                    };
                    if let Some(cb) = cb_opt {
                        let id = cb(bytes.as_ptr() as *const u8, bytes.len());
                        if id > 0 {
                            log::info!("Image registered with Pidgin store, ID: {}", id);
                            return format!("<img id=\"{}\" alt=\"{}\">", id, crate::escape_html(&content.body));
                        }
                    }
                    
                    // Fallback to file if callback failed or missing
                    let mut path = crate::media_helper::get_media_dir();
                    let file_id = ev.event_id.as_str().chars().filter(|c| c.is_alphanumeric()).collect::<String>();
                    let ext = content.info.as_ref().and_then(|i| i.mimetype.as_deref()).map(|m| sanitize_mime_ext(m, "png")).unwrap_or_else(|| "png".to_string());
                    let filename = format!("img_{}.{}", file_id, ext);
                    path.push(&filename);
                    
                    if let Ok(mut file) = tokio::fs::File::create(&path).await {
                        use tokio::io::AsyncWriteExt;
                        if file.write_all(&bytes).await.is_ok() {
                            let path_str = path.to_string_lossy().to_string();
                            log::info!("Successfully saved image to fallback file {}", path_str);
                            return format!("<a href=\"file://{}\"><img src=\"file://{}\" alt=\"{}\" width=\"300\"></a>", 
                                path_str, path_str, crate::escape_html(&content.body));
                        }
                    }
                },
                Err(e) => log::warn!("Failed to get media content for {}: {:?}", content.body, e),
            }
            format!("🖼️ [Image: {}]", crate::escape_html(&content.body))
        },
        MessageType::Video(content) => {
            log::debug!("Attempting to render video: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let mut path = crate::media_helper::get_media_dir();
                let file_id = ev.event_id.as_str().chars().filter(|c| c.is_alphanumeric()).collect::<String>();
                let ext = content.info.as_ref().and_then(|i| i.mimetype.as_deref()).map(|m| sanitize_mime_ext(m, "mp4")).unwrap_or_else(|| "mp4".to_string());
                path.push(format!("vid_{}.{}", file_id, ext));
                
                if let Ok(mut file) = tokio::fs::File::create(&path).await {
                    use tokio::io::AsyncWriteExt;
                    if file.write_all(&bytes).await.is_ok() {
                        let path_str = path.to_string_lossy().to_string();
                        let file_url = format!("file://{}", path_str);
                        log::info!("Successfully saved video to {}", path_str);
                        return format!("🎞️ <b>Video:</b> <a href=\"{}\">{}</a>", file_url, crate::escape_html(&content.body));
                    }
                }
            }
            format!("🎞️ [Video: {}]", crate::escape_html(&content.body))
        },
        MessageType::Audio(content) => {
            log::debug!("Attempting to render audio: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let mut path = crate::media_helper::get_media_dir();
                let file_id = ev.event_id.as_str().chars().filter(|c| c.is_alphanumeric()).collect::<String>();
                let ext = content.info.as_ref().and_then(|i| i.mimetype.as_deref()).map(|m| sanitize_mime_ext(m, "ogg")).unwrap_or_else(|| "ogg".to_string());
                path.push(format!("aud_{}.{}", file_id, ext));
                
                if let Ok(mut file) = tokio::fs::File::create(&path).await {
                    use tokio::io::AsyncWriteExt;
                    if file.write_all(&bytes).await.is_ok() {
                        let path_str = path.to_string_lossy().to_string();
                        let file_url = format!("file://{}", path_str);
                        log::info!("Successfully saved audio to {}", path_str);
                        return format!("🎵 <b>Audio:</b> <a href=\"{}\">{}</a>", file_url, crate::escape_html(&content.body));
                    }
                }
            }
            format!("🎵 [Audio: {}]", crate::escape_html(&content.body))
        },
        MessageType::File(content) => {
            log::debug!("Attempting to render file: {}", content.body);
            let request = MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File };
            if let Ok(bytes) = room.client().media().get_media_content(&request, true).await {
                let mut path = crate::media_helper::get_media_dir();
                let file_id = ev.event_id.as_str().chars().filter(|c| c.is_alphanumeric()).collect::<String>();
                path.push(format!("file_{}_{}", file_id, crate::sanitize_string(&content.body)));
                
                if let Ok(mut file) = tokio::fs::File::create(&path).await {
                    use tokio::io::AsyncWriteExt;
                    if file.write_all(&bytes).await.is_ok() {
                        let path_str = path.to_string_lossy().to_string();
                        let file_url = format!("file://{}", path_str);
                        log::info!("Successfully saved file to {}", path_str);
                        return format!("📁 <b>File:</b> <a href=\"{}\">{}</a>", file_url, crate::escape_html(&content.body));
                    }
                }
            }
            format!("📁 [File: {}]", crate::escape_html(&content.body))
        },
        _ => crate::get_display_html(&ev.content),
    }
}
