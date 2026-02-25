use matrix_sdk::Room;

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
        
        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: "System".to_string(),
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
