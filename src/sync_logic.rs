use matrix_sdk::Client;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn run_sync_loop(client: Client) {
    let client_for_sync = client.clone();

    // 1. Initial State Recovery
    let rooms = client.joined_rooms();
    for room in rooms {
        let room_id = room.room_id().to_string();
        let local_user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();

        // Notify C that we are in this room
        log::info!("Sync: Discovering joined room {}", room_id);
        
        // We await these here to ensure they are dispatched sequentially BEFORE the sync loop starts
        let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| "Joined Room".to_string());
        
        // Check encryption state
        let encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
        let member_count = room.joined_members_count();

        send_event(FfiEvent::RoomJoined {
            user_id: local_user_id.clone(),
            room_id: room_id.clone(),
            name,
            group_name: "Matrix".to_string(),
            avatar_url: None,
            topic: "".to_string(),
            encrypted,
            member_count,
        });

        // Load members in background
        let room_for_members = room.clone();
        let client_for_members = client.clone();
        let local_user_id_for_members = local_user_id.clone();
        let room_id_for_members = room_id.clone();
        crate::RUNTIME.spawn(async move {
            use matrix_sdk_base::RoomMemberships;
            if let Ok(members) = room_for_members.members(RoomMemberships::JOIN).await {
                for member in members {
                    let user_id = member.user_id().to_string();
                    let display_name = member.display_name().map(|d| d.to_string()).unwrap_or_else(|| user_id.clone());
                    
                    let mut local_avatar = String::new();
                    if let Some(mxc) = member.avatar_url() {
                        if let Some(path) = crate::media_helper::download_avatar(&client_for_members, &mxc.to_owned(), &user_id).await {
                            local_avatar = path;
                        }
                    }

                    send_event(FfiEvent::UpdateBuddy {
                        user_id: local_user_id_for_members.clone(),
                        alias: display_name.clone(),
                        avatar_url: local_avatar.clone(),
                    });

                    send_event(FfiEvent::ChatUser {
                        user_id: local_user_id_for_members.clone(),
                        room_id: room_id_for_members.clone(),
                        member_id: user_id,
                        add: true,
                        alias: Some(display_name),
                        avatar_path: if local_avatar.is_empty() { None } else { Some(local_avatar) },
                    });
                }
            }
        });

        // Fetch Topic
        let room_id_for_topic = room.room_id().to_string();
        let room_for_topic = room.clone();
        let local_user_id_for_topic = local_user_id.clone();
        crate::RUNTIME.spawn(async move {
            if let Some(topic_ev) = room_for_topic.get_state_event_static::<matrix_sdk::ruma::events::room::topic::RoomTopicEventContent>().await.ok().flatten() {
                if let Ok(content) = topic_ev.deserialize() {
                    use matrix_sdk::deserialized_responses::SyncOrStrippedState;
                    let topic_str = match content {
                        SyncOrStrippedState::Sync(s) => s.as_original().map(|o| o.content.topic.clone()),
                        SyncOrStrippedState::Stripped(s) => s.content.topic.clone(),
                    };
                    
                    if let Some(t) = topic_str {
                        send_event(FfiEvent::ChatTopic {
                            user_id: local_user_id_for_topic,
                            room_id: room_id_for_topic,
                            topic: t,
                            sender: "System".to_string(),
                        });
                    }
                }
            }
        });
    }

    // Handle Invites
    for room in client.invited_rooms() {
        let room_id = room.room_id().to_string();
        let me = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        send_event(FfiEvent::Invite {
            user_id: me,
            room_id,
            inviter: "Unknown".to_string(),
        });
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
    client_for_sync.add_event_handler(crate::handlers::polls::handle_poll_start);
    client_for_sync.add_event_handler(crate::handlers::polls::handle_poll_response);

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
            let display_body = format!("<span id=\"{}\">{}</span>", crate::escape_html(&event_id), body);
            
            send_event(FfiEvent::Message {
                user_id: me,
                sender,
                msg: display_body,
                room_id: room_id.to_string(),
                thread_root_id: cur_thread_id,
                event_id: Some(event_id),
                timestamp,
                encrypted: is_encrypted,
            });
        }
    }
}
