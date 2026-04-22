use matrix_sdk::Client;
use crate::ffi::{send_event, events::FfiEvent};
use crate::verification_logic::handle_verification_request_with_state;

pub async fn run_sync_loop(client: Client) {
    let local_user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    if local_user_id.is_empty() { return; }

    if !crate::ffi::ACTIVE_SYNC_LOOPS.insert(local_user_id.clone()) {
        log::warn!("Sync loop already running for {}, skipping second start.", local_user_id);
        return;
    }

    log::info!("Starting run_sync_loop for {}", local_user_id);

    let client_for_sync = client.clone();

    // 1. Initial State Recovery
    let rooms = client.joined_rooms();
    for room in rooms {
        let room_id = room.room_id().to_string();
        
        let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| room_id.clone());
        
        if room.is_space() {
            log::info!("Sync: Skipping space room {} as a chat entry", room_id);
            continue;
        }

        let group_name = crate::grouping::get_room_group_name(&room).await;
        let is_direct = room.is_direct().await.unwrap_or(false) || room.joined_members_count() <= 2;
        
        if let Some(old_group) = crate::ffi::NOTIFIED_ROOMS.get(&room_id) {
            if *old_group == group_name {
                // If it's already notified and not open, we might want to skip member loading
                if !crate::ffi::TRACKED_ROOMS.contains(&room_id) && !is_direct {
                    continue;
                }
            }
        }
        crate::ffi::NOTIFIED_ROOMS.insert(room_id.clone(), group_name.clone());

        // Check encryption state
        let encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
        let member_count = room.joined_members_count();

        send_event(FfiEvent::RoomJoined {
            user_id: local_user_id.clone(),
            room_id: room_id.clone(),
            name,
            group_name,
            avatar_url: None,
            topic: "".to_string(),
            encrypted,
            member_count,
        });

        // Load members ONLY if it's a DM (to populate buddy list)
        // Full member list for regular rooms is loaded on-demand via TRACKED_ROOMS check
        if is_direct {
            let room_for_members = room.clone();
            let client_for_members = client.clone();
            let local_uid = local_user_id.clone();
            let room_id_for_members = room_id.clone();

            crate::RUNTIME.spawn(async move {
                use matrix_sdk_base::RoomMemberships;
                if let Ok(members) = room_for_members.members(RoomMemberships::JOIN).await {
                    for member in members {
                        let user_id = member.user_id().to_string();
                        let display_name = member.display_name()
                            .map(|d| d.to_string())
                            .unwrap_or_else(|| user_id.clone());

                        if user_id == local_uid { continue; }

                        let mxc = member.avatar_url().map(|url| url.to_owned());

                        if !crate::ffi::NOTIFIED_BUDDIES.contains(&user_id) {
                            send_event(FfiEvent::UpdateBuddy {
                                user_id: user_id.clone(),
                                alias: display_name.clone(),
                                avatar_url: String::new(),
                            });
                        }

                        if let Some(mxc_url) = mxc {
                            // Spawn avatar download; capture only what the inner task needs.
                            let client_av = client_for_members.clone();
                            let local_uid_av = local_uid.clone();
                            let room_id_av = room_id_for_members.clone();
                            crate::RUNTIME.spawn(async move {
                                if let Some(path) = crate::media_helper::download_avatar(&client_av, &mxc_url, &user_id).await {
                                    if crate::ffi::NOTIFIED_BUDDIES.insert(user_id.clone()) {
                                        send_event(FfiEvent::UpdateBuddy {
                                            user_id: user_id.clone(),
                                            alias: display_name.clone(),
                                            avatar_url: path.clone(),
                                        });
                                    }
                                    send_event(FfiEvent::ChatUser {
                                        user_id: local_uid_av,
                                        room_id: room_id_av,
                                        member_id: user_id,
                                        add: true,
                                        alias: Some(display_name),
                                        avatar_path: Some(path),
                                    });
                                }
                            });
                        } else {
                            send_event(FfiEvent::ChatUser {
                                user_id: local_uid.clone(),
                                room_id: room_id_for_members.clone(),
                                member_id: user_id,
                                add: true,
                                alias: Some(display_name),
                                avatar_path: None,
                            });
                        }
                    }
                }
            });
        }

        // Fetch Topic ONLY if tracked
        if crate::ffi::TRACKED_ROOMS.contains(&room_id) {
            let room_id_for_topic = room_id.clone();
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

    // 3. Handle Verifications
    client_for_sync.add_event_handler(|ev: matrix_sdk::ruma::events::key::verification::request::ToDeviceKeyVerificationRequestEvent, client: Client| async move {
        let flow_id = ev.content.transaction_id.to_string();
        let sender = ev.sender.clone();
        if let Some(request) = client.encryption().get_verification_request(&sender, &flow_id).await {
             handle_verification_request_with_state(request, client).await;
        }
    });

    client_for_sync.add_event_handler(|ev: matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent, room: matrix_sdk::Room| async move {
        if let matrix_sdk::ruma::events::room::message::SyncRoomMessageEvent::Original(orig) = ev {
            if let matrix_sdk::ruma::events::room::message::MessageType::VerificationRequest(_) = &orig.content.msgtype {
                let client = room.client();
                if let Some(request) = client.encryption().get_verification_request(&orig.sender, &orig.event_id).await {
                    handle_verification_request_with_state(request, client.clone()).await;
                }
            }
        }
    });

    // 4. Start Sync with exponential backoff on transient failures.
    const MAX_BACKOFF_SECS: u64 = 300;
    const MAX_RETRIES: u32 = 20;
    let mut backoff_secs: u64 = 2;
    let mut retry_count: u32 = 0;

    loop {
        log::info!("Starting Matrix sync for {} (attempt {})…", local_user_id, retry_count + 1);
        let sync_settings = matrix_sdk::config::SyncSettings::default();

        match client_for_sync.sync(sync_settings).await {
            Ok(()) => {
                // Clean shutdown (e.g., logout called on the client).
                log::info!("Sync for {} stopped cleanly.", local_user_id);
                break;
            }
            Err(e) => {
                retry_count += 1;
                let err_str = format!("{:?}", e);
                log::error!("Sync error for {} (attempt {}): {}", local_user_id, retry_count, err_str);

                // Don't retry authentication errors — user must re-login.
                if err_str.contains("M_UNKNOWN_TOKEN")
                    || err_str.contains("M_FORBIDDEN")
                    || err_str.contains("Unauthorized")
                    || err_str.contains("M_GUEST_ACCESS_FORBIDDEN")
                {
                    log::error!("Auth error in sync for {}, giving up.", local_user_id);
                    send_event(FfiEvent::LoginFailed {
                        user_id: local_user_id.clone(),
                        error_msg: "Lost connection: authentication error. Please reconnect.".to_string(),
                    });
                    break;
                }

                if retry_count >= MAX_RETRIES {
                    log::error!("Sync giving up for {} after {} retries.", local_user_id, retry_count);
                    send_event(FfiEvent::LoginFailed {
                        user_id: local_user_id.clone(),
                        error_msg: format!(
                            "Lost connection to the Matrix server after {} reconnect attempts. \
                             Please sign out and back in.",
                            retry_count
                        ),
                    });
                    break;
                }

                log::info!("Reconnecting sync for {} in {}s…", local_user_id, backoff_secs);
                tokio::time::sleep(std::time::Duration::from_secs(backoff_secs)).await;
                backoff_secs = (backoff_secs * 2).min(MAX_BACKOFF_SECS);
            }
        }
    }

    crate::ffi::ACTIVE_SYNC_LOOPS.remove(&local_user_id);
}

pub async fn fetch_room_history(client: Client, room_or_user_id: String) {
    let mut room_id = room_or_user_id.clone();
    
    // If it's a user ID, try to find the DM room
    if room_or_user_id.starts_with('@') {
        use matrix_sdk::ruma::UserId;
        if let Ok(uid) = <&UserId>::try_from(room_or_user_id.as_str()) {
            // Find joined DM room with this user
            for room in client.joined_rooms() {
                if room.is_direct().await.unwrap_or(false) {
                    if let Ok(members) = room.members(matrix_sdk_base::RoomMemberships::JOIN).await {
                        if members.iter().any(|m| m.user_id() == uid) {
                            room_id = room.room_id().to_string();
                            break;
                        }
                    }
                }
            }
        }
    }

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
                        body = format!("[Sticker] {}", crate::escape_html(&ev.content.body));
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
            let display_body = format!("<span id=\"{}\">{}</span>", crate::escape_html(event_id.as_str()), body);
            let is_direct = room.is_direct().await.unwrap_or(false) || room.joined_members_count() <= 2;
            
            send_event(FfiEvent::Message {
                user_id: me,
                sender,
                sender_id: sender_id.to_string(),
                msg: display_body,
                room_id: room_id.to_string(),
                thread_root_id: cur_thread_id,
                event_id: Some(event_id),
                timestamp,
                encrypted: is_encrypted,
                is_direct,
            });
        }
    }
}
