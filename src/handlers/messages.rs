use matrix_sdk::ruma::events::room::message::{SyncRoomMessageEvent, Relation};
use matrix_sdk::Room;
use crate::ffi::{send_event, events::FfiEvent};

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
        
        let pidgin_sender = sender_display;

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
            send_event(FfiEvent::MessageEdited {
                user_id: local_user_id,
                room_id: room.room_id().to_string(),
                event_id: target_id,
                new_msg: edited_body,
            });
            return;
        }

        let mut thread_root_id: Option<String> = None;
        if let Some(Relation::Thread(ref thread)) = ev.content.relates_to {
            thread_root_id = Some(thread.event_id.to_string());
        }

        let body = render_room_message(&ev, &room).await;

        // 1. Handle Replies
        let mut reply_to_id: Option<String> = None;
        if let Some(Relation::Reply { ref in_reply_to }) = &ev.content.relates_to {
            reply_to_id = Some(in_reply_to.event_id.to_string());
        } else if let Some(rel) = raw_val.as_ref().and_then(|v| v.get("content")).and_then(|c| c.get("m.relates_to")) {
            if let Some(id) = rel.get("m.in_reply_to").and_then(|r| r.get("event_id")).and_then(|id| id.as_str()) {
                reply_to_id = Some(id.to_string());
            }
        }

        let mut final_body = body;
        if let Some(id) = reply_to_id {
            let mut quoted = format!("<i>Replying to {}...</i>", id);
            if let Ok(parent_ev) = room.event(id.as_str().try_into().unwrap_or(ev.event_id.as_ref()), None).await {
                use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent};
                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(parent_msg))) = parent_ev.raw().deserialize() {
                    if let matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent::Original(p_ev) = parent_msg {
                        let mut parent_body = crate::get_display_html(&p_ev.content);
                        
                        parent_body = parent_body.replace("<p>", "").replace("</p>", " ").replace("<br/>", " ").replace("<br>", " ").replace("<div>", "").replace("</div>", " ");
                        if parent_body.len() > 100 { parent_body = format!("{}...", &parent_body[..97]); }
                        
                        let p_sender_id = p_ev.sender.as_str();
                        let p_sender_display = if let Some(m) = room.get_member(&p_ev.sender).await.ok().flatten() {
                            m.display_name().map(|d| d.to_string()).unwrap_or_else(|| p_sender_id.to_string())
                        } else {
                            p_sender_id.to_string()
                        };

                        quoted = format!("<b>{}</b>: {}", p_sender_display, crate::sanitize_untrusted_html(&parent_body));
                    }
                }
            }
            final_body = crate::html_fmt::style_reply(&quoted, &final_body);
        }

        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();

        let target_room_id = if let Some(ref tid) = thread_root_id {
            format!("{}|{}", room_id, tid)
        } else {
            room_id.to_string()
        };
        
        let display_body = format!("<span id=\"{}\">{}</span>", ev.event_id.as_str(), final_body);

        send_event(FfiEvent::Message {
            user_id: local_user_id.clone(),
            sender: pidgin_sender,
            msg: display_body,
            room_id: target_room_id,
            thread_root_id,
            event_id: Some(ev.event_id.to_string()),
            timestamp,
            encrypted: is_encrypted,
        });

        // 2. Handle Media Asynchronously
        use matrix_sdk::ruma::events::room::message::MessageType;
        let msg_type = if let Some(Relation::Replacement(ref repl)) = &ev.content.relates_to {
            &repl.new_content.msgtype
        } else {
            &ev.content.msgtype
        };

        if let MessageType::Image(content) = msg_type {
            let room_clone = room.clone();
            let event_id = ev.event_id.to_string();
            let content_clone = content.clone();
            let user_id_clone = local_user_id.clone();
            let room_id_clone = room.room_id().to_string();

            crate::RUNTIME.spawn(async move {
                use matrix_sdk::media::{MediaFormat, MediaRequestParameters, MediaThumbnailSettings};
                use matrix_sdk::ruma::uint;

                let request = if let Some(info) = &content_clone.info {
                    if let Some(thumbnail_source) = &info.thumbnail_source {
                        let settings = MediaThumbnailSettings::new(uint!(300), uint!(300));
                        MediaRequestParameters {
                            source: thumbnail_source.clone(),
                            format: MediaFormat::Thumbnail(settings),
                        }
                    } else {
                        MediaRequestParameters { source: content_clone.source.clone(), format: MediaFormat::File }
                    }
                } else {
                    MediaRequestParameters { source: content_clone.source.clone(), format: MediaFormat::File }
                };

                match room_clone.client().media().get_media_content(&request, true).await {
                    Ok(data) => {
                        send_event(FfiEvent::MediaDownloaded {
                            user_id: user_id_clone,
                            room_id: room_id_clone,
                            event_id,
                            data,
                            content_type: content_clone.info.as_ref().and_then(|i| i.mimetype.clone()).unwrap_or_else(|| "image/png".to_string()),
                        });
                    }
                    Err(e) => log::warn!("Failed to download media for {}: {:?}", event_id, e),
                }
            });
        }
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
         
         let sender_display = if let Some(m) = room.get_member(ev.sender()).await.ok().flatten() {
             m.display_name().map(|d| d.to_string()).unwrap_or_else(|| sender_id.to_string())
         } else {
             sender_id.to_string()
         };
         
         let pidgin_sender = sender_display;
         let room_id = room.room_id().as_str();
         let timestamp: u64 = ev.origin_server_ts().0.into();
         
         send_event(FfiEvent::Message {
             user_id: local_user_id,
             sender: pidgin_sender,
             msg: "[Encrypted]".to_string(),
             room_id: room_id.to_string(),
             thread_root_id: None,
             event_id: Some(ev.event_id().to_string()),
             timestamp,
             encrypted: true,
         });
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
        if !target_event_id.is_empty() {
            send_event(FfiEvent::MessageRedacted {
                user_id,
                room_id: room_id.to_string(),
                event_id: target_event_id.to_string(),
            });
        }
    }
}

pub async fn render_room_message(ev: &matrix_sdk::ruma::events::room::message::OriginalSyncRoomMessageEvent, _room: &Room) -> String {
    use matrix_sdk::ruma::events::room::message::MessageType;

    let msg_type = if let Some(Relation::Replacement(ref repl)) = &ev.content.relates_to {
        &repl.new_content.msgtype
    } else {
        &ev.content.msgtype
    };

    match msg_type {
        MessageType::Image(content) => {
            format!("🖼️ [Image: {}] (Downloading...)", crate::escape_html(&content.body))
        },
        MessageType::Video(_) => "[Video]".to_string(),
        MessageType::Audio(_) => "[Audio]".to_string(),
        MessageType::File(_) => "[File]".to_string(),
        _ => crate::get_display_html(&ev.content),
    }
}
