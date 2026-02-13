use matrix_sdk::ruma::events::room::topic::SyncRoomTopicEvent;
use matrix_sdk::ruma::events::room::member::{SyncRoomMemberEvent, StrippedRoomMemberEvent, MembershipState};
use matrix_sdk::{Room, Client};
use std::ffi::CString;
use crate::ffi::{MSG_CALLBACK, ROOM_JOINED_CALLBACK, ROOM_LEFT_CALLBACK, INVITE_CALLBACK, CHAT_TOPIC_CALLBACK, CHAT_USER_CALLBACK, UPDATE_BUDDY_CALLBACK};

pub async fn handle_room_topic(event: SyncRoomTopicEvent, room: Room) {
    if let Some(original_event) = event.as_original() {
        let sender = original_event.sender.as_str();
        let room_id = room.room_id().as_str();
        let topic = &original_event.content.topic;
        
        log::info!("Topic change in {}: {}", room_id, topic);

        let c_sender = CString::new(sender).unwrap_or_default();
        let c_body = CString::new(format!("[Topic] {} changed topic to: {}", sender, topic)).unwrap_or_default();
        let c_room_id = CString::new(room_id).unwrap_or_default();
        let c_topic = CString::new(topic.as_str()).unwrap_or_default();
        
        {
            let guard = MSG_CALLBACK.lock().unwrap();
            let timestamp: u64 = original_event.origin_server_ts.0.into();
            if let Some(cb) = *guard {
                cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
            }
        }
        
        {
            let guard = CHAT_TOPIC_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_room_id.as_ptr(), c_topic.as_ptr(), c_sender.as_ptr());
            }
        }
    }
}

pub async fn handle_room_member(event: SyncRoomMemberEvent, room: Room, client: Client) {
    if let Some(user_id) = client.user_id() {
        if event.state_key() == user_id {
             match event.membership() {
                 MembershipState::Join => {
                    let room_id = room.room_id().as_str();
                    let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| "New Room".to_string());
                    log::info!("User joined room: {} ({}) - Emitting Callback", room_id, name);

                    let group = crate::grouping::get_room_group_name(&room).await;
                    let topic = room.topic().unwrap_or_default();
                    let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.unwrap_or(None).is_some();
                    let mut avatar_path_str = String::new();
                    if let Some(url) = room.avatar_url() {
                        if let Some(path) = crate::media_helper::download_avatar(&client, &url, room_id).await {
                            avatar_path_str = path;
                        }
                    }

                    if let (Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) =
                        (CString::new(room_id), CString::new(name), CString::new(group), CString::new(avatar_path_str), CString::new(topic))
                    {
                        let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr(), c_topic.as_ptr(), is_encrypted);
                        }
                    }
                 },
                 MembershipState::Invite => {
                    let room_id = room.room_id().as_str();
                    let inviter = event.sender().as_str();
                    log::info!("User invited to room: {} by {}. Emitting callback.", room_id, inviter);
                    
                    if let (Ok(c_room_id), Ok(c_inviter)) = 
                        (CString::new(room_id), CString::new(inviter)) 
                    {
                        let guard = INVITE_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_room_id.as_ptr(), c_inviter.as_ptr());
                        }
                    }
                 },
                 MembershipState::Leave | MembershipState::Ban => {
                    let room_id = room.room_id().as_str();
                    log::info!("User left room: {} - Emitting Callback", room_id);
                    if let Ok(c_room_id) = CString::new(room_id) {
                        let guard = ROOM_LEFT_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_room_id.as_ptr());
                        }
                    }
                 },
                 _ => {}
             }
        }
        
        // Update Chat User list for ALL membership changes
        {
            let room_id = room.room_id().as_str();
            let user_id = event.state_key().as_str();
            
            let add = match event.membership() {
                MembershipState::Join => true,
                _ => false,
            };

            // Attempt to get display name and avatar even for generic updates if possible, 
            // or use defaults/empty if not immediately available without extra queries.
            // For now, let's try to get them if we have them from the event content (if it was a profile change)
            // or fall back to empty strings which the C side can handle (keep existing or query).
            
            let (display_name, avatar_url) = if let SyncStateEvent::Original(ev) = &event {
                 (ev.content.displayname.as_deref().unwrap_or(""), ev.content.avatar_url.as_ref().map(|u| u.to_string()).unwrap_or_default())
            } else {
                 ("", "".to_string())
            };

            // If it's a join, we might want to fetch profile if missing? 
            // The C side can lazily fetch if needed.

            let c_room_id = CString::new(room_id).unwrap_or_default();
            let c_user_id = CString::new(user_id).unwrap_or_default();
            let c_alias = CString::new(display_name).unwrap_or_default();
            let c_avatar = CString::new(avatar_url).unwrap_or_default(); // Passing URL/Path? C side expects Path usually.
            
            // Wait, previous avatar logic downloaded it. We should probably do that here too if we want avatars.
            // But downloading for EVERY member update might be heavy. 
            // Let's pass the URL/Path if we have it. 
            // For this iteration, let's pass what we have. C plugin can decide to download if it's a URL.
            
            let guard = CHAT_USER_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_room_id.as_ptr(), c_user_id.as_ptr(), add, c_alias.as_ptr(), c_avatar.as_ptr());
            }
        }
        
        // Profile Update Logic
        use matrix_sdk::ruma::events::SyncStateEvent;
        if let SyncStateEvent::Original(ev) = event {
            let content = &ev.content;
            if content.membership == MembershipState::Join {
                let user_id = ev.state_key.as_str();
            
                let display_name = content.displayname.clone().unwrap_or_else(|| user_id.to_string());
                let mut avatar_path_str = String::new();
                
                if let Some(avatar_url) = &content.avatar_url {
                    if let Some(path) = crate::media_helper::download_avatar(&client, avatar_url, user_id).await {
                        avatar_path_str = path;
                    }
                }
            
                log::info!("Updating profile for {}: Name={}, Avatar={}", user_id, display_name, avatar_path_str);
                
                if let (Ok(c_user_id), Ok(c_alias), Ok(c_path)) = 
                    (CString::new(user_id), CString::new(display_name), CString::new(avatar_path_str))
                {
                     let guard = UPDATE_BUDDY_CALLBACK.lock().unwrap();
                     if let Some(cb) = *guard {
                         cb(c_user_id.as_ptr(), c_alias.as_ptr(), c_path.as_ptr());
                     }
                }
            }
        }
    }
}

pub async fn handle_stripped_member(event: StrippedRoomMemberEvent, room: Room, client: Client) {
     if let Some(user_id) = client.user_id() {
        if event.state_key == user_id {
             match event.content.membership {
                 MembershipState::Invite => {
                    let room_id = room.room_id().as_str();
                    let inviter = event.sender.as_str();
                    log::info!("(Stripped) User invited to room: {} by {}. Emitting callback.", room_id, inviter);
                    
                    if let (Ok(c_room_id), Ok(c_inviter)) = 
                        (CString::new(room_id), CString::new(inviter)) 
                    {
                        let guard = INVITE_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_room_id.as_ptr(), c_inviter.as_ptr());
                        }
                    }
                 },
                 _ => {}
             }
        }
     }
}

pub async fn handle_tombstone(event: matrix_sdk::ruma::events::room::tombstone::SyncRoomTombstoneEvent, room: Room, client: Client) {
    if let Some(ev) = event.as_original() {
        let replacement_room_id = ev.content.replacement_room.as_str();
        let old_room_id = room.room_id().as_str();
        log::info!("Room {} has been upgraded to {}. Joining new room...", old_room_id, replacement_room_id);
        
        // Notify UI (System Message)
        let msg = format!("This room has been upgraded. Joining new room: {}", replacement_room_id);
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(msg).unwrap_or_default();
        let c_room_id = CString::new(old_room_id).unwrap_or_default();
        
        {
            let guard = MSG_CALLBACK.lock().unwrap();
            let timestamp: u64 = ev.origin_server_ts.0.into();
            if let Some(cb) = *guard {
                 cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
            }
        }

        // Mark the old room in the room list so users can see it's tombstoned.
        // We keep it visible under a dedicated group with an explicit topic.
        let old_name = room
            .display_name()
            .await
            .map(|d| d.to_string())
            .unwrap_or_else(|_| "Upgraded Room".to_string());
        let old_name = format!("{} [Upgraded]", old_name);
        let tombstone_topic = format!("Upgraded to {}", replacement_room_id);
        let is_encrypted = room
            .get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>()
            .await
            .unwrap_or(None)
            .is_some();
        let mut avatar_path_str = String::new();
        if let Some(url) = room.avatar_url() {
            if let Some(path) = crate::media_helper::download_avatar(&client, &url, old_room_id).await {
                avatar_path_str = path;
            }
        }

        if let (Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) = (
            CString::new(old_name),
            CString::new("Tombstoned Rooms"),
            CString::new(avatar_path_str),
            CString::new(tombstone_topic.clone()),
        ) {
            let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(
                    c_room_id.as_ptr(),
                    c_name.as_ptr(),
                    c_group.as_ptr(),
                    c_avatar.as_ptr(),
                    c_topic.as_ptr(),
                    is_encrypted,
                );
            }
        }

        // Also update the open chat topic immediately if the room window is open.
        if let (Ok(c_topic), Ok(c_sender)) = (
            CString::new(tombstone_topic),
            CString::new("System"),
        ) {
            let guard = CHAT_TOPIC_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_room_id.as_ptr(), c_topic.as_ptr(), c_sender.as_ptr());
            }
        }

        // Join the new room
        if let Ok(new_room_id) = <&matrix_sdk::ruma::RoomId>::try_from(replacement_room_id) {
            if let Err(e) = client.join_room_by_id(new_room_id).await {
                log::error!("Failed to auto-join upgraded room {}: {:?}", replacement_room_id, e);
                 let msg = format!("Failed to auto-join new room: {:?}", e);
                 let c_body = CString::new(msg).unwrap_or_default();
                 let guard = MSG_CALLBACK.lock().unwrap();
                 if let Some(cb) = *guard {
                     cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), 0);
                 }
            } else {
                log::info!("Successfully joined upgraded room {}", replacement_room_id);
            }
        } else {
            log::error!("Invalid replacement room ID in tombstone event: {}", replacement_room_id);
        }
    }
}
