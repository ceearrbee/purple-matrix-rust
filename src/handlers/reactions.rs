use matrix_sdk::Room;
use matrix_sdk::ruma::EventId;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn update_reactions_for_event(room: &Room, target_id: &EventId) {
    let client = room.client();
    let Some(me) = client.user_id() else { return; };
    let local_user_id = me.as_str().to_string();

    let mut summary = String::new();
    match room.event(target_id, None).await {
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

    // If summary is empty, it means all reactions were redacted or never existed
    let display_text = if summary.is_empty() {
        "".to_string() 
    } else {
        format!("[System] [Reactions] {}", crate::escape_html(&summary))
    };

    log::info!("Dispatching ReactionsChanged for {}: {}", target_id, summary);
    send_event(FfiEvent::ReactionsChanged {
        user_id: local_user_id,
        room_id: room.room_id().to_string(),
        event_id: target_id.to_string(),
        reactions_text: display_text,
    });
}

pub async fn handle_reaction(event: matrix_sdk::ruma::events::reaction::SyncReactionEvent, room: Room) {
    if let matrix_sdk::ruma::events::reaction::SyncReactionEvent::Original(ev) = event {
        log::info!("Reaction received: {} to event {} from {}", ev.content.relates_to.key, ev.content.relates_to.event_id, ev.sender);
        update_reactions_for_event(&room, &ev.content.relates_to.event_id).await;
    }
}

pub async fn handle_sticker(event: matrix_sdk::ruma::events::sticker::SyncStickerEvent, room: Room) {
    if let matrix_sdk::ruma::events::sticker::SyncStickerEvent::Original(ev) = event {
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();
        let sender_id = ev.sender.as_str();
        
        if sender_id == local_user_id { return; }

        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let body = format!("[Sticker] {}", crate::escape_html(&ev.content.body));
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
        
        send_event(FfiEvent::Message {
            user_id: local_user_id,
            sender: sender_id.to_string(),
            sender_id: sender_id.to_string(),
            msg: body,
            room_id: room_id.to_string(),
            thread_root_id: None,
            event_id: Some(ev.event_id.to_string()),
            timestamp,
            encrypted: is_encrypted,
            is_direct: room.is_direct().await.unwrap_or(false),
        });
    }
}
