use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{RUNTIME, with_client, handlers, sync_logic};
use crate::ffi::*;
use crate::{PAGINATION_TOKENS, HISTORY_FETCHED_ROOMS};
use matrix_sdk::RoomState;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_room_members(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk_base::RoomMemberships;
            if let Ok(room_id) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(room_id) {
                    log::info!("Fetching members for room {}", room_id_str);
                    if let Ok(members) = room.members(RoomMemberships::JOIN).await {
                        for member in members {
                            let user_id = member.user_id().as_str();
                            let display_name = member.display_name().unwrap_or(user_id);
                            let avatar_url = member.avatar_url().map(|u| u.to_string()).unwrap_or_default();
                            
                            let c_room_id = CString::new(room_id_str.clone()).unwrap_or_default();
                            let c_user_id = CString::new(user_id).unwrap_or_default();
                            let c_alias = CString::new(display_name).unwrap_or_default();
                            let c_avatar = CString::new(avatar_url).unwrap_or_default();

                            let guard = CHAT_USER_CALLBACK.lock().unwrap();
                            if let Some(cb) = *guard {
                                cb(c_room_id.as_ptr(), c_user_id.as_ptr(), true, c_alias.as_ptr(), c_avatar.as_ptr());
                            }
                        }
                    }
                }
            } else {
                 log::error!("Invalid Room ID for members: {}", room_id_str);
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_history(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    {
        let mut fetched = HISTORY_FETCHED_ROOMS.lock().unwrap();
        if fetched.contains(&room_id_str) {
            return;
        }
        fetched.insert(room_id_str.clone());
    }
    
    log::info!("Lazy fetching history for: {}", room_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            let actual_room_id = if let Ok(user_id) = <&matrix_sdk::ruma::UserId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_dm_room(user_id) {
                    Some(room.room_id().to_string())
                } else {
                    log::warn!("Could not find DM room for user {}. History fetch skipped.", room_id_str);
                    None
                }
            } else {
                Some(room_id_str)
            };

            if let Some(rid) = actual_room_id {
                sync_logic::fetch_room_history_logic(client, rid).await;
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_history_with_limit(user_id: *const c_char, room_id: *const c_char, limit: u32) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let clamped_limit = limit.clamp(1, 500) as u16;

    {
        let mut fetched = HISTORY_FETCHED_ROOMS.lock().unwrap();
        if fetched.contains(&room_id_str) {
            return;
        }
        fetched.insert(room_id_str.clone());
    }

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            let actual_room_id = if room_id_str.starts_with('@') {
                use matrix_sdk::ruma::UserId;
                if let Ok(user_id) = <&UserId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_dm_room(user_id) {
                        Some(room.room_id().to_string())
                    } else {
                        None
                    }
                } else {
                    None
                }
            } else {
                Some(room_id_str)
            };

            if let Some(rid) = actual_room_id {
                sync_logic::fetch_room_history_logic_with_limit(client, rid, clamped_limit).await;
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_more_history(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    log::info!("On-demand history fetch for: {}", room_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            sync_logic::fetch_room_history_logic(client, room_id_str).await;
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_more_history_with_limit(user_id: *const c_char, room_id: *const c_char, limit: u32) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let clamped_limit = limit.clamp(1, 500) as u16;

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            sync_logic::fetch_room_history_logic_with_limit(client, room_id_str, clamped_limit).await;
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_resync_recent_history(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let mut room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    if let Some((base, _)) = room_id_str.split_once('|') {
        room_id_str = base.to_string();
    }

    {
        let mut fetched = HISTORY_FETCHED_ROOMS.lock().unwrap();
        fetched.remove(&room_id_str);
    }
    {
        let mut tokens = PAGINATION_TOKENS.lock().unwrap();
        tokens.remove(&room_id_str);
    }

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            sync_logic::fetch_room_history_logic(client, room_id_str).await;
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_resync_recent_history_with_limit(user_id: *const c_char, room_id: *const c_char, limit: u32) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let mut room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let clamped_limit = limit.clamp(1, 500) as u16;
    if let Some((base, _)) = room_id_str.split_once('|') {
        room_id_str = base.to_string();
    }

    {
        let mut fetched = HISTORY_FETCHED_ROOMS.lock().unwrap();
        fetched.remove(&room_id_str);
    }
    {
        let mut tokens = PAGINATION_TOKENS.lock().unwrap();
        tokens.remove(&room_id_str);
    }

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            sync_logic::fetch_room_history_logic_with_limit(client, room_id_str, clamped_limit).await;
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_preview_room(user_id: *const c_char, room_id_or_alias: *const c_char, output_room_id: *const c_char) {
    if user_id.is_null() || room_id_or_alias.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_or_alias_str = unsafe { CStr::from_ptr(room_id_or_alias).to_string_lossy().into_owned() };
    let output_room_id_opt = if output_room_id.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(output_room_id).to_string_lossy().into_owned() })
    };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::api::client::room::get_summary::v1::Request as RoomSummaryRequest;
            use matrix_sdk::ruma::RoomOrAliasId;

            let mut lines = Vec::new();
            lines.push(format!("<b>Room Preview</b>: {}", room_id_or_alias_str));

            let parsed = match <&RoomOrAliasId>::try_from(room_id_or_alias_str.as_str()) {
                Ok(v) => v.to_owned(),
                Err(_) => {
                    lines.push("Invalid room ID or alias.".to_string());
                    send_preview_system_message(output_room_id_opt.as_deref(), &room_id_or_alias_str, &lines.join("<br/>"));
                    return;
                }
            };

            let request = RoomSummaryRequest::new(parsed, Vec::new());
            match client.send(request).await {
                Ok(response) => {
                    let summary = response.summary;
                    lines.push(format!("Name: {}", summary.name.unwrap_or_else(|| "Unnamed".to_string())));
                    if let Some(alias) = summary.canonical_alias {
                        lines.push(format!("Alias: {}", alias));
                    }
                    lines.push(format!("Room ID: {}", summary.room_id));
                    lines.push(format!("Joined Members: {}", summary.num_joined_members));
                    lines.push(format!("Join Rule: {:?}", summary.join_rule));
                    lines.push(format!("World Readable: {}", summary.world_readable));
                    lines.push(format!("Encrypted: {}", summary.encryption.is_some()));
                    if let Some(topic) = summary.topic {
                        lines.push(format!("Topic: {}", topic));
                    }

                    if let Some(room) = client.get_room(&summary.room_id) {
                        use matrix_sdk::room::MessagesOptions;
                        use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent};

                        let mut options = MessagesOptions::backward();
                        options.limit = 5u16.into();
                        if let Ok(messages) = room.messages(options).await {
                            let mut activity = Vec::new();
                            for timeline_event in messages.chunk.iter().rev() {
                                if let Ok(event) = timeline_event.raw().deserialize() {
                                    if let AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(msg_event)) = event {
                                        if let Some(ev) = msg_event.as_original() {
                                            let body = handlers::messages::render_room_message(ev, &room).await;
                                            activity.push(format!("{}: {}", ev.sender, body));
                                        }
                                    }
                                }
                            }
                            if !activity.is_empty() {
                                lines.push("<b>Recent Activity</b>:".to_string());
                                lines.extend(activity);
                            }
                        }
                    }
                }
                Err(e) => {
                    lines.push(format!("Failed to fetch room summary: {:?}", e));
                }
            }

            send_preview_system_message(output_room_id_opt.as_deref(), &room_id_or_alias_str, &lines.join("<br/>"));
        });
    });
}

fn send_preview_system_message(output_room_id: Option<&str>, preview_target: &str, body: &str) {
    let c_target = CString::new(preview_target).unwrap_or_default();
    let c_sender = CString::new("System").unwrap_or_default();
    let c_body = CString::new(body.replace('\0', "")).unwrap_or_default();
    let timestamp = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|d| d.as_millis() as u64)
        .unwrap_or(0);

    let preview_guard = ROOM_PREVIEW_CALLBACK.lock().unwrap();
    if let Some(cb) = *preview_guard {
        cb(c_target.as_ptr(), c_body.as_ptr());
    }
    drop(preview_guard);

    let Some(room_id) = output_room_id else {
        return;
    };
    let c_room_id = CString::new(room_id).unwrap_or_default();
    let guard = MSG_CALLBACK.lock().unwrap();
    if let Some(cb) = *guard {
        cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
    }
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_join_room(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;

            // Parse ID for Thread Support: "room_id|thread_root_id"
            let (room_id_str, thread_root_id_opt) = match id_str.split_once('|') {
                Some((r, t)) if !t.is_empty() => (r, Some(t)),
                _ => (id_str.as_str(), None),
            };
            
            if let Some(tid) = thread_root_id_opt {
                log::info!("Request to join/open thread {} in room {}", tid, room_id_str);
            }

            if let Ok(room_id_ruma) = <&RoomId>::try_from(room_id_str) {
                if let Some(room) = client.get_room(room_id_ruma) {

                    let state = room.state();
                    if state == RoomState::Joined {
                        log::info!("Already joined room {}, skipping join call", id_str);
                    } else {
                        if let Err(e) = room.join().await {
                            log::error!("Failed to join room {}: {:?}", id_str, e);
                        } else {
                            log::info!("Successfully joined room {}", id_str);
                        }
                    }
                } else {
                     // Try joining by ID if not in list (e.g. public room)
                     if let Err(e) = client.join_room_by_id(room_id_ruma).await {
                         log::error!("Failed to join room by ID {}: {:?}", id_str, e);
                     }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_leave_room(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(room_id) = <&RoomId>::try_from(id_str.as_str()) {
                if let Some(room) = client.get_room(room_id) {
                    log::info!("Leaving room {}", id_str);
                    if let Err(e) = room.leave().await {
                        log::error!("Failed to leave room {}: {:?}", id_str, e);
                    } else {
                        log::info!("Successfully left room {}", id_str);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_invite_user(account_user_id: *const c_char, room_id: *const c_char, user_id: *const c_char) {
    if account_user_id.is_null() || room_id.is_null() || user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
            
            let r_id = <&RoomId>::try_from(room_id_str.as_str());
            let u_id = <&UserId>::try_from(user_id_str.as_str());

            if let (Ok(room_id), Ok(user_id)) = (r_id, u_id) {
                if let Some(room) = client.get_room(room_id) {
                    log::info!("Inviting {} to {}", user_id_str, room_id_str);
                    if let Err(e) = room.invite_user_by_id(user_id).await {
                        log::error!("Failed to invite user {} to {}: {:?}", user_id_str, room_id_str, e);
                    } else {
                        log::info!("Successfully invited {} to {}", user_id_str, room_id_str);
                    }
                } else {
                    log::warn!("Room {} not found for invite", room_id_str);
                }
            } else {
                log::error!("Invalid Room ID or User ID for invite");
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_room(user_id: *const c_char, name: *const c_char, topic: *const c_char, is_public: bool) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let name_str = if name.is_null() { None } else { Some(unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() }) };
    let topic_str = if topic.is_null() { None } else { Some(unsafe { CStr::from_ptr(topic).to_string_lossy().into_owned() }) };
    
    log::info!("Creating room: Name={:?}, Topic={:?}, Public={}", name_str, topic_str, is_public);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::api::client::room::create_room::v3::Request as CreateRoomRequest;
            use matrix_sdk::ruma::api::client::room::create_room::v3::RoomPreset;
            
            let mut request = CreateRoomRequest::default();
            request.name = name_str;
            request.topic = topic_str;
            
            if is_public {
                request.preset = Some(RoomPreset::PublicChat);
            } else {
                request.preset = Some(RoomPreset::PrivateChat);
            }
            
            match client.create_room(request).await {
                Ok(room) => log::info!("Room created successfully: {}", room.room_id()),
                Err(e) => log::error!("Failed to create room: {:?}", e),
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_kick_user(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
            if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Kicking {} from {}", target_user_id_str, room_id_str);
                    if let Err(e) = room.kick_user(uid, reason_str.as_deref()).await {
                        log::error!("Failed to kick user: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ban_user(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
            if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Banning {} from {}", target_user_id_str, room_id_str);
                    if let Err(e) = room.ban_user(uid, reason_str.as_deref()).await {
                        log::error!("Failed to ban user: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_tag(user_id: *const c_char, room_id: *const c_char, tag: *const c_char) {
    if user_id.is_null() || room_id.is_null() || tag.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let tag_str = unsafe { CStr::from_ptr(tag).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::tag::TagName;

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                     let tag_name_opt = match tag_str.as_str() {
                         "Favourite" => Some(TagName::Favorite),
                         "LowPriority" => Some(TagName::LowPriority),
                         "ServerNotice" => Some(TagName::ServerNotice),
                         s => Some(s.into()),
                     };

                     if let Some(tag_name) = tag_name_opt {
                         log::info!("Setting tag {:?} for room {}", tag_name, room_id_str);
                         // Add tag
                         if let Err(e) = room.set_tag(tag_name, matrix_sdk::ruma::events::tag::TagInfo::default()).await {
                             log::error!("Failed to set tag: {:?}", e);
                         }
                     }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_power_levels(user_id: *const c_char, room_id: *const c_char) {
    // Placeholder: Fetch and print/return power levels.
    // Since we don't have a callback for generic text return, maybe just log or chat?
    // Implementation skipped unless required. Logs info.
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
             if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    match room.power_levels().await {
                        Ok(pl) => {
                             log::info!("Power Levels for {}: {:?}", room_id_str, pl);
                        }, 
                        Err(e) => log::error!("Failed to get power levels: {:?}", e),
                    }
                }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_power_level(user_id: *const c_char, room_id: *const c_char, target_user: *const c_char, level: i32) {
    if user_id.is_null() || room_id.is_null() || target_user.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_user_str = unsafe { CStr::from_ptr(target_user).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
             if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                   // Requires state event update for m.room.power_levels
                   // This is complex helper in SDK or manual state event
                   // SDK has room.update_power_level_for_user(user_id, level)
                   use matrix_sdk::ruma::Int;
                    if let Ok(level_int) =  Int::try_from(level as i64) {
                       if let Err(e) = room.update_power_levels(vec![(&uid, level_int)]).await {
                            log::error!("Failed to update power level: {:?}", e);
                       } else {
                            log::info!("Updated power level for {} to {}", target_user_str, level);
                       }
                   }
                }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_name(user_id: *const c_char, room_id: *const c_char, name: *const c_char) {
    if user_id.is_null() || room_id.is_null() || name.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let name_str = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    if let Err(e) = room.set_name(name_str).await {
                        log::error!("Failed to set room name: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_topic(user_id: *const c_char, room_id: *const c_char, topic: *const c_char) {
    if user_id.is_null() || room_id.is_null() || topic.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let topic_str = unsafe { CStr::from_ptr(topic).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    if let Err(e) = room.set_room_topic(&topic_str).await {
                        log::error!("Failed to set room topic: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_report_content(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, score: i32, reason: *const c_char) {
    if user_id.is_null() || room_id.is_null() || event_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
    let _reason_str = if reason.is_null() { String::new() } else { unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() } };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::{RoomId, EventId};
             use matrix_sdk::ruma::api::client::room::report_content::v3::Request as ReportRequest;
             use matrix_sdk::ruma::Int;

              if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                   
                   let reason_opt = if _reason_str.is_empty() { None } else { Some(_reason_str) };
                   let score_int = Int::try_from(score as i64).ok();
                   
                   let request = ReportRequest::new(rid.to_owned(), eid.to_owned(), score_int, reason_opt);

                   if let Err(e) = client.send(request).await {
                        log::error!("Failed to report content: {:?}", e);
                   } else {
                        log::info!("Content reported successfully.");
                   }
              }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_mute_state(user_id: *const c_char, room_id: *const c_char, muted: bool) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                let settings = client.notification_settings().await;
                
                // Muted = MentionsAndKeywordsOnly
                // Unmuted = AllMessages
                let mode = if muted {
                    matrix_sdk::notification_settings::RoomNotificationMode::MentionsAndKeywordsOnly
                } else {
                    matrix_sdk::notification_settings::RoomNotificationMode::AllMessages
                };

                log::info!("Setting notification mode for {} to {:?} (Muted: {})", room_id_str, mode, muted);

                if let Err(e) = settings.set_room_notification_mode(rid, mode).await {
                     log::error!("Failed to set room notification mode: {:?}", e);
                } else {
                     log::info!("Room notification mode updated successfully.");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_knock(user_id: *const c_char, room_id_or_alias: *const c_char, reason: *const c_char) {
    if user_id.is_null() || room_id_or_alias.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_or_alias = unsafe { CStr::from_ptr(room_id_or_alias).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, RoomAliasId};

            if let Ok(rid) = <&RoomId>::try_from(id_or_alias.as_str()) {
                if let Err(e) = client.knock(rid.to_owned().into(), reason_str, vec![]).await {
                    log::error!("Failed to knock on room: {:?}", e);
                    let msg = format!("Failed to knock on room: {:?}", e);
                    crate::ffi::send_system_message(&user_id_str, &msg);
                } else {
                    log::info!("Knocked on room successfully");
                }
            } else if let Ok(alias) = <&RoomAliasId>::try_from(id_or_alias.as_str()) {
                 if let Err(e) = client.knock(alias.to_owned().into(), reason_str, vec![]).await {
                    log::error!("Failed to knock on room: {:?}", e);
                    let msg = format!("Failed to knock on room: {:?}", e);
                    crate::ffi::send_system_message(&user_id_str, &msg);
                } else {
                    log::info!("Knocked on room successfully");
                }
            } else {
                log::error!("Invalid Room ID or Alias for knock: {}", id_or_alias);
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unban_user(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::{RoomId, UserId};
             
             if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_str.as_str())) {
                 if let Some(room) = client.get_room(rid) {
                    if let Err(e) = room.unban_user(uid, reason_str.as_deref()).await {
                        log::error!("Failed to unban user: {:?}", e);
                        let msg = format!("Failed to unban user: {:?}", e);
                        crate::ffi::send_system_message(&user_id_str, &msg);
                    } else {
                        log::info!("Unbanned user successfully");
                    }
                 }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_avatar(user_id: *const c_char, room_id: *const c_char, path: *const c_char) {
    if user_id.is_null() || room_id.is_null() || path.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    
    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            let path_buf = std::path::PathBuf::from(&path_str);
            if !path_buf.exists() { return; }
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                 if let Some(room) = client.get_room(rid) {
                      let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
                      if let Ok(data) = std::fs::read(&path_buf) {
                          if let Ok(response) = client.media().upload(&mime, data, None).await {
                               if let Err(e) = room.set_avatar_url(&response.content_uri, None).await {
                                   log::error!("Failed to set room avatar: {:?}", e);
                               }
                          }
                      }
                 }
            }
        });
    });
}
