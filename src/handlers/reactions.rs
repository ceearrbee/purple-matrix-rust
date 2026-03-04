use matrix_sdk::Room;

pub async fn handle_reaction(event: matrix_sdk::ruma::events::reaction::SyncReactionEvent, room: Room) {
    if let matrix_sdk::ruma::events::reaction::SyncReactionEvent::Original(ev) = event {
        log::info!("Reaction received: {} to event {} from {}", ev.content.relates_to.key, ev.content.relates_to.event_id, ev.sender);
        
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();
        
        let target_id = ev.content.relates_to.event_id.clone();
        
        // Fetch the event to get its bundled reactions
        let mut summary = String::new();
        match room.event(&target_id, None).await {
            Ok(timeline_ev) => {
                if let Ok(value) = timeline_ev.raw().deserialize_as::<serde_json::Value>() {
                    if let Some(chunks) = value.get("unsigned")
                        .and_then(|u| u.get("m.relations"))
                        .and_then(|r| r.get("m.annotation"))
                        .and_then(|a| a.get("chunk"))
                        .and_then(|c| c.as_array())
                    {
                        let mut first = true;
                        for chunk in chunks {
                            let key = chunk.get("key").and_then(|k| k.as_str()).unwrap_or("");
                            let count = chunk.get("count").and_then(|c| c.as_u64()).unwrap_or(0);
                            if key.is_empty() || count == 0 { continue; }
                            
                            if !first { summary.push_str(", "); }
                            summary.push_str(&format!("{} {}", key, count));
                            first = false;
                        }
                    }
                }
            },
            Err(e) => {
                log::warn!("Failed to fetch event {} for bundled reactions: {:?}", target_id, e);
            }
        }

        // Fallback: if summary is empty (bundling not available), just show the single reaction we just got
        if summary.is_empty() {
            summary = format!("{} 1", ev.content.relates_to.key);
        }
        
        log::info!("Dispatching ReactionsChanged for {}: {}", target_id, summary);
        let event = crate::ffi::FfiEvent::ReactionsChanged {
            user_id: local_user_id,
            room_id: room.room_id().to_string(),
            event_id: target_id.to_string(),
            reactions_text: format!("[System] [Reactions] {}", summary),
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_sticker(event: matrix_sdk::ruma::events::sticker::SyncStickerEvent, room: Room) {
    if let matrix_sdk::ruma::events::sticker::SyncStickerEvent::Original(ev) = event {
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();
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
