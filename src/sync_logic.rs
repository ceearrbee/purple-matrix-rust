use matrix_sdk::Client;

pub async fn run_sync_loop(client: Client) {
    let client_for_sync = client.clone();

    // 1. Initial State Recovery
    let rooms = client.joined_rooms();
    for room in rooms {
        let room_id = room.room_id().to_string();
        log::info!("Sync Loop: Recovery for room {}", room_id);

        // Warm member cache and get display names/avatars
        use matrix_sdk_base::RoomMemberships;
        match room.members(RoomMemberships::JOIN).await {
            Ok(members) => {
                for member in members {
                    let user_id = member.user_id().to_string();
                    let display_name = member.display_name().map(|d| d.to_string()).unwrap_or_else(|| user_id.clone());
                    
                    // Download avatar if available
                    let mut local_avatar = String::new();
                    if let Some(mxc) = member.avatar_url() {
                        if let Some(path) = crate::media_helper::download_avatar(&client, &mxc.to_owned(), &user_id).await {
                            local_avatar = path;
                        }
                    }

                    // Dispatch UpdateBuddy
                    let ffi_ev = crate::ffi::FfiEvent::UpdateBuddy {
                        user_id: client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default(),
                        target_user_id: user_id.clone(),
                        alias: display_name.clone(),
                        avatar_url: local_avatar.clone(),
                    };
                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);

                    // Update Chat User
                    let chat_user_ev = crate::ffi::FfiEvent::ChatUser {
                        user_id: client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default(),
                        room_id: room_id.clone(),
                        member_id: user_id,
                        add: true,
                        alias: Some(display_name),
                        avatar_path: if local_avatar.is_empty() { None } else { Some(local_avatar) },
                    };
                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(chat_user_ev);
                }
            }
            Err(e) => log::warn!("Failed to fetch members for {}: {:?}", room_id, e),
        }

        // Fetch Topic
        if let Some(topic_ev) = room.get_state_event_static::<matrix_sdk::ruma::events::room::topic::RoomTopicEventContent>().await.ok().flatten() {
            if let Ok(content) = topic_ev.deserialize() {
                use matrix_sdk::deserialized_responses::SyncOrStrippedState;
                let topic_str = match content {
                    SyncOrStrippedState::Sync(s) => s.as_original().map(|o| o.content.topic.clone()),
                    SyncOrStrippedState::Stripped(s) => s.content.topic.clone(),
                };
                
                if let Some(t) = topic_str {
                    let ffi_ev = crate::ffi::FfiEvent::ChatTopic {
                        user_id: client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default(),
                        room_id: room_id.clone(),
                        topic: t,
                        sender: "System".to_string(),
                    };
                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);
                }
            }
        }
    }

    // 2. Register Event Handlers
    client_for_sync.add_event_handler(crate::handlers::messages::handle_room_message);
    client_for_sync.add_event_handler(crate::handlers::messages::handle_encrypted);
    client_for_sync.add_event_handler(crate::handlers::messages::handle_redaction);
    client_for_sync.add_event_handler(crate::handlers::room_state::handle_room_topic);
    client_for_sync.add_event_handler(crate::handlers::room_state::handle_room_member);
    client_for_sync.add_event_handler(crate::handlers::typing::handle_typing);
    client_for_sync.add_event_handler(crate::handlers::reactions::handle_reaction);
    client_for_sync.add_event_handler(crate::handlers::receipts::handle_receipt);

    // 3. Start Sync
    log::info!("Starting Matrix sync loop...");
    let sync_settings = matrix_sdk::config::SyncSettings::default();
    if let Err(e) = client_for_sync.sync(sync_settings).await {
        log::error!("Sync loop failed: {:?}", e);
    }
}

pub async fn fetch_room_history(client: Client, room_id: String) {
    let Ok(rid) = room_id.as_str().try_into() else {
        log::warn!("Invalid room_id for history fetch: {}", room_id);
        return;
    };
    if let Some(room) = client.get_room(rid) {
        log::info!("Fetching history for room {}", room_id);
        
        let mut filter = matrix_sdk::room::MessagesOptions::backward();
        filter.limit = matrix_sdk::ruma::uint!(50);
        
        match room.messages(filter).await {
            Ok(resp) => {
                for timeline_event in resp.chunk {
                    process_sync_event_for_history(&client, &room, &room_id, timeline_event).await;
                }
            }
            Err(e) => log::warn!("Failed to fetch history for {}: {:?}", room_id, e),
        }
    }
}

async fn process_sync_event_for_history(client: &Client, room: &matrix_sdk::Room, room_id: &str, timeline_event: matrix_sdk::deserialized_responses::TimelineEvent) {
    use matrix_sdk::ruma::events::room::message::Relation;
    use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;
    use matrix_sdk::ruma::events::AnySyncTimelineEvent;

    let me = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    
    let mut any_event_opt: Option<AnySyncTimelineEvent> = timeline_event.raw().deserialize().ok();
    let mut is_encrypted = false;

    if let Some(AnySyncTimelineEvent::MessageLike(matrix_sdk::ruma::events::AnySyncMessageLikeEvent::RoomEncrypted(_))) = &any_event_opt {
        is_encrypted = true;
        let event_id = timeline_event.event_id().map(|e| e.to_string()).unwrap_or_default();
        let raw_json = timeline_event.raw().json().get().to_string();
        if let Ok(raw_original) = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::encrypted::OriginalSyncRoomEncryptedEvent>::from_json_string(raw_json) {
            match room.decrypt_event(&raw_original, None).await {
                 Ok(decrypted) => {
                      any_event_opt = decrypted.raw().deserialize().ok();
                 },
                 Err(e) => {
                      log::warn!("Failed to decrypt history event {}: {:?}", event_id, e);
                 }
            }
        }
    }

    if let Some(any_event) = any_event_opt {
        let sender_id = any_event.sender();
        let sender_display = if let Some(member) = room.get_member(sender_id).await.ok().flatten() {
            member.display_name().map(|d| d.to_string()).unwrap_or_else(|| sender_id.to_string())
        } else {
            sender_id.to_string()
        };
        let sender = sender_display;
        
        let mut body = String::new();
        let mut cur_thread_id: Option<String> = None;
        let event_id = any_event.event_id().to_string();
        let timestamp = any_event.origin_server_ts().0.into();

        if let AnySyncTimelineEvent::MessageLike(msg_like) = &any_event {
            match msg_like {
                AnySyncMessageLikeEvent::RoomMessage(msg_event) => {
                    if let Some(ev) = msg_event.as_original() {
                        if let Some(Relation::Thread(thread)) = &ev.content.relates_to {
                            cur_thread_id = Some(thread.event_id.to_string());
                        }
                        body = crate::handlers::messages::render_room_message(ev, room).await;
                    }
                },
                AnySyncMessageLikeEvent::Sticker(sticker_event) => {
                    if let Some(ev) = sticker_event.as_original() {
                        body = format!("[Sticker] {}", ev.content.body);
                    }
                },
                AnySyncMessageLikeEvent::RoomEncrypted(_) => {
                    is_encrypted = true;
                    body = "[Encrypted]".to_string();
                },
                _ => {}
            }
        }

        if !body.is_empty() {
            let display_body = format!("{} <font color='#fdfdfd' size='1'>_MXID:[{}]</font>", body, event_id);
            
            let ffi_ev = crate::ffi::FfiEvent::MessageReceived {
                user_id: me,
                sender,
                msg: display_body,
                room_id: Some(room_id.to_string()),
                thread_root_id: cur_thread_id,
                event_id,
                timestamp,
                encrypted: is_encrypted,
            };
            let _ = crate::ffi::EVENTS_CHANNEL.0.send(ffi_ev);
        }
    }
}
