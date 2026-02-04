use matrix_sdk::ruma::events::room::topic::SyncRoomTopicEvent;
use matrix_sdk::ruma::events::room::member::{SyncRoomMemberEvent, StrippedRoomMemberEvent, MembershipState};
use matrix_sdk::{Room, Client};
use std::ffi::CString;
use crate::ffi::{MSG_CALLBACK, ROOM_JOINED_CALLBACK, INVITE_CALLBACK, CHAT_TOPIC_CALLBACK, CHAT_USER_CALLBACK, UPDATE_BUDDY_CALLBACK};

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
                    let avatar_url = room.avatar_url().map(|u| u.to_string()).unwrap_or_default();

                    if let (Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_avatar)) =
                        (CString::new(room_id), CString::new(name), CString::new(group), CString::new(avatar_url))
                    {
                        let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr());
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
            
            let c_room_id = CString::new(room_id).unwrap_or_default();
            let c_user_id = CString::new(user_id).unwrap_or_default();
            
            let guard = CHAT_USER_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_room_id.as_ptr(), c_user_id.as_ptr(), add);
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
                    use matrix_sdk::media::{MediaFormat, MediaRequestParameters};
                    use matrix_sdk::ruma::events::room::MediaSource;
                    
                    let safe_filename = format!("avatar_{}", user_id.replace(":", "_").replace("@", ""));
                    let path = std::path::PathBuf::from(format!("/tmp/matrix_avatars/{}", safe_filename));
                    
                    if !path.exists() {
                        let _ = std::fs::create_dir_all("/tmp/matrix_avatars");
                        log::info!("Downloading avatar for {}...", user_id);
                        let request = MediaRequestParameters { source: MediaSource::Plain(avatar_url.clone()), format: MediaFormat::File };
                          if let Ok(bytes) = client.media().get_media_content(&request, true).await {
                              if let Ok(mut file) = std::fs::File::create(&path) {
                                   use std::io::Write;
                                   if file.write_all(&bytes).is_ok() {
                                       avatar_path_str = path.to_string_lossy().to_string();
                                   }
                              }
                          }
                     } else {
                         avatar_path_str = path.to_string_lossy().to_string();
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
