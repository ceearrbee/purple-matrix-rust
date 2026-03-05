use matrix_sdk::ruma::events::room::message::{SyncRoomMessageEvent, Relation};
use matrix_sdk::Room;

pub async fn handle_room_message(event: matrix_sdk::ruma::serde::Raw<SyncRoomMessageEvent>, room: Room) {
    let raw_val: Option<serde_json::Value> = event.deserialize_as::<serde_json::Value>().ok();
    
    if let Ok(SyncRoomMessageEvent::Original(ev)) = event.deserialize() {
        let sender_id = ev.sender.as_str();
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

        // Resolve display name for sender
        let sender_display = if let Some(member) = room.get_member(&ev.sender).await.ok().flatten() {
            member.display_name().map(|d| d.to_string()).unwrap_or_else(|| sender_id.to_string())
        } else {
            sender_id.to_string()
        };
        
        let pidgin_sender = crate::escape_html(&sender_display);

        // 0. Handle Edits (Replacements)
        let mut is_edit = false;
        let mut target_id = String::new();

        if let Some(Relation::Replacement(ref repl)) = &ev.content.relates_to {
            is_edit = true;
            target_id = repl.event_id.to_string();
        } else if let Some(rel) = raw_val.as_ref().and_then(|v| v.get("content")).and_then(|c| c.get("m.relates_to")) {
            if rel.get("rel_type").and_then(|t| t.as_str()) == Some("m.replace") {
                is_edit = true;
                target_id = rel.get("event_id").and_then(|id| id.as_str()).unwrap_or("").to_string();
            }
        }

        if is_edit && !target_id.is_empty() {
            log::info!("Message replacement detected for {}", target_id);
            let body = render_room_message(&ev, &room).await;
            let edited_body = crate::html_fmt::style_edit(&body);
            let ffi_ev = crate::ffi::FfiEvent::MessageEdited {
                user_id: local_user_id,
                room_id: room.room_id().to_string(),
                event_id: target_id,
                new_msg: edited_body,
            };
            let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);
            return;
        }

        // We no longer return early for own messages because local echo is disabled in C.
        // This ensures own messages get Matrix IDs and can be reacted to.

        let mut thread_root_id: Option<String> = None;
        if let Some(Relation::Thread(ref thread)) = ev.content.relates_to {
            thread_root_id = Some(thread.event_id.to_string());
        }

        let mut body = render_room_message(&ev, &room).await;

        // 1. Handle Replies
        let mut reply_to_id: Option<String> = None;
        if let Some(Relation::Reply { ref in_reply_to }) = &ev.content.relates_to {
            reply_to_id = Some(in_reply_to.event_id.to_string());
        } else if let Some(rel) = raw_val.as_ref().and_then(|v| v.get("content")).and_then(|c| c.get("m.relates_to")) {
            if let Some(id) = rel.get("m.in_reply_to").and_then(|r| r.get("event_id")).and_then(|id| id.as_str()) {
                reply_to_id = Some(id.to_string());
            }
        }

        if let Some(id) = reply_to_id {
            let mut quoted = format!("<i>Replying to {}...</i>", id);
            if let Ok(parent_ev) = room.event(id.as_str().try_into().unwrap_or(ev.event_id.as_ref()), None).await {
                use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent};
                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(parent_msg))) = parent_ev.raw().deserialize() {
                    if let matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent::Original(p_ev) = parent_msg {
                        let mut parent_body = crate::get_display_html(&p_ev.content);
                        
                        // Aggressive stripping of formatting from quoted body
                        parent_body = parent_body.replace("<p>", "").replace("</p>", " ").replace("<br/>", " ").replace("<br>", " ").replace("<div>", "").replace("</div>", " ");
                        if parent_body.len() > 100 { parent_body = format!("{}...", &parent_body[..97]); }
                        
                        let p_sender_id = p_ev.sender.as_str();
                        let p_sender_display = if let Some(m) = room.get_member(&p_ev.sender).await.ok().flatten() {
                            m.display_name().map(|d| d.to_string()).unwrap_or_else(|| p_sender_id.to_string())
                        } else {
                            p_sender_id.to_string()
                        };

                        quoted = format!("<b>{}</b>: {}", crate::escape_html(&p_sender_display), crate::sanitize_untrusted_html(&parent_body));
                    }
                }
            }
            body = crate::html_fmt::style_reply_v2(&quoted, &body);
        }

        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();

        let target_room_id = if let Some(ref tid) = thread_root_id {
            format!("{}|{}", room_id, tid)
        } else {
            room_id.to_string()
        };
        
        // Final Marker format: _MXID:[event_id]
        let display_body = format!("{} <font color='#fdfdfd' size='1'>_MXID:[{}]</font>", body, crate::escape_html(ev.event_id.as_str()));

        let ffi_ev = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: pidgin_sender,
            msg: display_body,
            room_id: Some(target_room_id),
            thread_root_id,
            event_id: ev.event_id.to_string(),
            timestamp,
            encrypted: is_encrypted,
            is_system: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);
    }
}

pub async fn handle_encrypted(event: matrix_sdk::ruma::serde::Raw<matrix_sdk::ruma::events::room::encrypted::SyncRoomEncryptedEvent>, room: Room) {
     use matrix_sdk::ruma::events::AnySyncTimelineEvent;
     use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;
     
     if let Ok(raw_original) = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::encrypted::OriginalSyncRoomEncryptedEvent>::from_json_string(event.json().get().to_string()) {
         match room.decrypt_event(&raw_original, None).await {
             Ok(decrypted) => {
                 if let Ok(any_event) = decrypted.raw().deserialize() {
                     if let AnySyncTimelineEvent::MessageLike(msg_like) = any_event {
                         match msg_like {
                             AnySyncMessageLikeEvent::RoomMessage(_) => {
                                 if let Ok(raw_msg) = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent>::from_json_string(decrypted.raw().json().get().to_string()) {
                                     handle_room_message(raw_msg, room).await;
                                 }
                                 return;
                             },
                         AnySyncMessageLikeEvent::Sticker(sticker_ev) => {
                             crate::handlers::reactions::handle_sticker(sticker_ev, room).await;
                             return;
                         },
                         AnySyncMessageLikeEvent::Reaction(reaction_ev) => {
                             crate::handlers::reactions::handle_reaction(reaction_ev, room).await;
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
         let sender_id = ev.sender().as_str();
         
         // We no longer return early for own messages

         let sender_display = if let Some(m) = room.get_member(ev.sender()).await.ok().flatten() {
             m.display_name().map(|d| d.to_string()).unwrap_or_else(|| sender_id.to_string())
         } else {
             sender_id.to_string()
         };
         
         let pidgin_sender = crate::escape_html(&sender_display);

         let room_id = room.room_id().as_str();
         let timestamp: u64 = ev.origin_server_ts().0.into();
         
         let body = "[Encrypted]".to_string();
         
         let ffi_ev = crate::ffi::FfiEvent::MessageReceived {
             user_id: local_user_id,
             sender: pidgin_sender,
             msg: body,
             room_id: Some(room_id.to_string()),
             thread_root_id: None,
             event_id: ev.event_id().to_string(),
             timestamp,
             encrypted: true,
             is_system: true,
         };
         let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);
     }
}

pub async fn handle_redaction(event: matrix_sdk::ruma::events::room::redaction::SyncRoomRedactionEvent, room: Room) {
    if let Some(ev) = event.as_original() {
        let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let room_id = room.room_id().as_str();
        let target_event_id_owned = ev.redacts.clone();
        
        if let Some(tid) = &target_event_id_owned {
            if let Ok(target_ev) = room.event(tid.as_ref(), None).await {
                use matrix_sdk::ruma::events::AnySyncTimelineEvent;
                use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;
                
                let any_ev = target_ev.raw().deserialize();
                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::Reaction(reaction_ev))) = any_ev {
                    if let Some(orig) = reaction_ev.as_original() {
                        let msg_id = orig.content.relates_to.event_id.clone();
                        log::info!("Reaction {} was redacted. Updating message {}", tid, msg_id);
                        crate::handlers::reactions::update_reactions_for_event(&room, &msg_id).await;
                        return;
                    }
                }
            }
        }

        let target_event_id = target_event_id_owned.as_ref().map(|id| id.as_str()).unwrap_or("");
        let ffi_ev = crate::ffi::FfiEvent::MessageReceived {
            user_id,
            sender: "System".to_string(),
            msg: format!("[System] [Redaction] Message {} was removed.", target_event_id),
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: ev.event_id.to_string(),
            timestamp: ev.origin_server_ts.0.into(),
            encrypted: false,
            is_system: true,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);
    }
}

pub async fn render_room_message(ev: &matrix_sdk::ruma::events::room::message::OriginalSyncRoomMessageEvent, room: &Room) -> String {
    use matrix_sdk::ruma::events::room::message::MessageType;
    use matrix_sdk::media::{MediaFormat, MediaRequestParameters};

    let msg_type = if let Some(Relation::Replacement(ref repl)) = &ev.content.relates_to {
        &repl.new_content.msgtype
    } else {
        &ev.content.msgtype
    };

    match msg_type {
        MessageType::Image(content) => {
            log::debug!("Attempting to render image: {}", content.body);
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
                    MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File }
                }
            } else {
                MediaRequestParameters { source: content.source.clone(), format: MediaFormat::File }
            };
            
            match room.client().media().get_media_content(&request, true).await {
                Ok(bytes) => {
                    let cb_opt = {
                        let guard = crate::ffi::IMGSTORE_ADD_CALLBACK.lock().unwrap_or_else(|e| e.into_inner());
                        *guard
                    };
                    if let Some(cb) = cb_opt {
                        let id = cb(bytes.as_ptr() as *const u8, bytes.len());
                        if id > 0 { format!("<img id=\"{}\" alt=\"{}\">", id, crate::escape_html(&content.body)) }
                        else { format!("🖼️ [Image: {}]", crate::escape_html(&content.body)) }
                    } else {
                        format!("🖼️ [Image: {}]", crate::escape_html(&content.body))
                    }
                },
                Err(e) => {
                    log::warn!("Failed to get media content for {}: {:?}", content.body, e);
                    format!("🖼️ [Image: {}]", crate::escape_html(&content.body))
                }
            }
        },
        MessageType::Video(_) => "[Video]".to_string(),
        MessageType::Audio(_) => "[Audio]".to_string(),
        MessageType::File(_) => "[File]".to_string(),
        _ => crate::get_display_html(&ev.content),
    }
}
