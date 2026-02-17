use matrix_sdk::ruma::events::room::topic::SyncRoomTopicEvent;
use matrix_sdk::ruma::events::room::member::{SyncRoomMemberEvent, StrippedRoomMemberEvent, MembershipState};
use matrix_sdk::ruma::events::SyncStateEvent;
use matrix_sdk::{Room, Client};
use std::ffi::CString;
use crate::ffi::{MSG_CALLBACK, ROOM_JOINED_CALLBACK, ROOM_LEFT_CALLBACK, INVITE_CALLBACK, CHAT_TOPIC_CALLBACK, CHAT_USER_CALLBACK, UPDATE_BUDDY_CALLBACK};

pub async fn handle_room_topic(event: SyncRoomTopicEvent, room: Room) {
    if let Some(original_event) = event.as_original() {
        let local_user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let sender = original_event.sender.as_str();
        let room_id = room.room_id().as_str();
        let topic = &original_event.content.topic;
        
        log::info!("Topic change in {}: {}", room_id, topic);

        let s_local_user_id = crate::sanitize_string(&local_user_id);
        let s_sender = crate::sanitize_string(sender);
        let s_room_id = crate::sanitize_string(room_id);
        let s_topic_content = crate::sanitize_string(topic.as_str());
        let s_body_content = format!("[Topic] {} changed topic to: {}", s_sender, s_topic_content);

        let c_local_user_id = CString::new(s_local_user_id).unwrap_or_default();
        let c_sender = CString::new(s_sender.clone()).unwrap_or_default();
        let c_body = CString::new(s_body_content).unwrap_or_default();
        let c_room_id = CString::new(s_room_id).unwrap_or_default();
        let c_topic = CString::new(s_topic_content).unwrap_or_default();
        
        {
            let guard = MSG_CALLBACK.lock().unwrap();
            let timestamp: u64 = original_event.origin_server_ts.0.into();
            if let Some(cb) = *guard {
                cb(c_local_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
            }
        }
        
        {
            let guard = CHAT_TOPIC_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_local_user_id.as_ptr(), c_room_id.as_ptr(), c_topic.as_ptr(), c_sender.as_ptr());
            }
        }
    }
}

pub async fn handle_room_member(event: SyncRoomMemberEvent, room: Room, client: Client) {
    let local_user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
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

                    if let (Ok(c_user_id), Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) =
                        (CString::new(crate::sanitize_string(&local_user_id)), CString::new(crate::sanitize_string(room_id)), CString::new(crate::sanitize_string(&name)), CString::new(crate::sanitize_string(&group)), CString::new(avatar_path_str), CString::new(crate::sanitize_string(&topic)))
                    {
                        let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr(), c_topic.as_ptr(), is_encrypted);
                        }
                    }
                 },
                 MembershipState::Invite => {
                    let room_id = room.room_id().as_str();
                    let inviter = event.sender().as_str();
                    log::info!("User invited to room: {} by {}. Emitting callback.", room_id, inviter);
                    
                    if let (Ok(c_user_id), Ok(c_room_id), Ok(c_inviter)) = 
                        (CString::new(crate::sanitize_string(&local_user_id)), CString::new(crate::sanitize_string(room_id)), CString::new(crate::sanitize_string(inviter))) 
                    {
                        let guard = INVITE_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_inviter.as_ptr());
                        }
                    }
                 },
                 MembershipState::Leave | MembershipState::Ban => {
                    let room_id = room.room_id().as_str();
                    log::info!("User left room: {} - Emitting Callback", room_id);
                    if let (Ok(c_user_id), Ok(c_room_id)) = (CString::new(crate::sanitize_string(&local_user_id)), CString::new(crate::sanitize_string(room_id))) {
                        let guard = ROOM_LEFT_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_room_id.as_ptr());
                        }
                    }
                 },
                 _ => {}
             }
        }
        
        // Update Chat User list for ALL membership changes
        {
            let room_id = room.room_id().as_str();
            let member_id = event.state_key().as_str();
            
            let add = match event.membership() {
                MembershipState::Join => true,
                _ => false,
            };

            let (display_name, avatar_url) = if let SyncStateEvent::Original(ev) = &event {
                 (ev.content.displayname.as_deref().unwrap_or(""), ev.content.avatar_url.as_ref().map(|u| u.to_string()).unwrap_or_default())
            } else {
                 ("", "".to_string())
            };

            let c_local_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
            let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
            let c_member_id = CString::new(crate::sanitize_string(member_id)).unwrap_or_default();
            let c_alias = CString::new(crate::sanitize_string(display_name)).unwrap_or_default();
            let c_avatar = CString::new(avatar_url).unwrap_or_default(); 
            
            let guard = CHAT_USER_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_local_user_id.as_ptr(), c_room_id.as_ptr(), c_member_id.as_ptr(), add, c_alias.as_ptr(), c_avatar.as_ptr());
            }
        }
        
        // Profile Update Logic
        if let SyncStateEvent::Original(ev) = event {
            let content = &ev.content;
            if content.membership == MembershipState::Join {
                let member_id = ev.state_key.as_str();
            
                let display_name = content.displayname.clone().unwrap_or_else(|| member_id.to_string());
                let mut avatar_path_str = String::new();
                
                if let Some(avatar_url) = &content.avatar_url {
                    if let Some(path) = crate::media_helper::download_avatar(&client, avatar_url, member_id).await {
                        avatar_path_str = path;
                    }
                }
            
                log::info!("Updating profile for {}: Name={}, Avatar={}", member_id, display_name, avatar_path_str);
                
                if let (Ok(c_user_id), Ok(c_alias), Ok(c_path)) = 
                    (CString::new(crate::sanitize_string(member_id)), CString::new(crate::sanitize_string(&display_name)), CString::new(avatar_path_str))
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
        let local_user_id = user_id.as_str().to_string();
        if event.state_key == local_user_id {
             match event.content.membership {
                 MembershipState::Invite => {
                    let room_id = room.room_id().as_str();
                    let inviter = event.sender.as_str();
                    log::info!("(Stripped) User invited to room: {} by {}. Emitting callback.", room_id, inviter);
                    
                    if let (Ok(c_user_id), Ok(c_room_id), Ok(c_inviter)) = 
                        (CString::new(crate::sanitize_string(&local_user_id)), CString::new(crate::sanitize_string(room_id)), CString::new(crate::sanitize_string(inviter))) 
                    {
                        let guard = INVITE_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_inviter.as_ptr());
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
        let local_user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let replacement_room_id = ev.content.replacement_room.as_str();
        let old_room_id = room.room_id().as_str();
        
        let c_local_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(old_room_id)).unwrap_or_default();
        
        {
            let msg = format!("This room has been upgraded. Joining new room: {}", replacement_room_id);
            let c_body = CString::new(crate::sanitize_string(&msg)).unwrap_or_default();
            let guard = MSG_CALLBACK.lock().unwrap();
            let timestamp: u64 = ev.origin_server_ts.0.into();
            if let Some(cb) = *guard {
                 cb(c_local_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
            }
        }

        let old_name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| "Upgraded Room".to_string());
        let old_name = format!("{} [Upgraded]", old_name);
        let tombstone_topic = format!("Upgraded to {}", replacement_room_id);
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.unwrap_or(None).is_some();
        let mut avatar_path_str = String::new();
        if let Some(url) = room.avatar_url() {
            if let Some(path) = crate::media_helper::download_avatar(&client, &url, old_room_id).await {
                avatar_path_str = path;
            }
        }

        if let (Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) = (
            CString::new(crate::sanitize_string(&old_name)),
            CString::new("Tombstoned Rooms"),
            CString::new(avatar_path_str),
            CString::new(crate::sanitize_string(&tombstone_topic)),
        ) {
            let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_local_user_id.as_ptr(), c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr(), c_topic.as_ptr(), is_encrypted);
            }
        }

        if let Ok(c_topic) = CString::new(crate::sanitize_string(&tombstone_topic)) {
            let guard = CHAT_TOPIC_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_local_user_id.as_ptr(), c_room_id.as_ptr(), c_topic.as_ptr(), c_sender.as_ptr());
            }
        }

        if let Ok(new_room_id) = <&matrix_sdk::ruma::RoomId>::try_from(replacement_room_id) {
            let _ = client.join_room_by_id(new_room_id).await;
        }
    }
}
