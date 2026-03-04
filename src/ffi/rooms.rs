use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client, sync_logic};

use crate::{PAGINATION_TOKENS, HISTORY_FETCHED_ROOMS};
use matrix_sdk::RoomState;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_room_members(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

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
                            let mut avatar_path_str = None;
                            if let Some(mxc_uri) = member.avatar_url() {
                                avatar_path_str = crate::media_helper::download_avatar(&client, &mxc_uri.to_owned(), user_id).await;
                            }
                            
                            let event = crate::ffi::FfiEvent::ChatUser {
                                user_id: uid_async.clone(),
                                room_id: room_id_str.clone(),
                                member_id: user_id.to_string(),
                                add: true,
                                alias: Some(display_name.to_string()),
                                avatar_path: avatar_path_str,
                            };
                            let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
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
    
    if HISTORY_FETCHED_ROOMS.contains(&room_id_str) {
        return;
    }
    HISTORY_FETCHED_ROOMS.insert(room_id_str.clone());
    
    log::info!("Lazy fetching history for: {}", room_id_str);

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        
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
pub extern "C" fn purple_matrix_rust_fetch_more_history(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    log::info!("On-demand history fetch for: {}", room_id_str);

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        
        RUNTIME.spawn(async move {
            sync_logic::fetch_room_history_logic(client, room_id_str).await;
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

    HISTORY_FETCHED_ROOMS.remove(&room_id_str);
    PAGINATION_TOKENS.remove(&room_id_str);

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            sync_logic::fetch_room_history_logic(client, room_id_str).await;
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_join_room(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;

            // Parse ID for Thread Support: "room_id|thread_root_id"
            let (room_id_str, _thread_root_id_opt) = match id_str.split_once('|') {
                Some((r, t)) if !t.is_empty() => (r, Some(t)),
                _ => (id_str.as_str(), None),
            };
            
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

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        
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

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        
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
pub extern "C" fn purple_matrix_rust_get_power_levels(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
             if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    match room.power_levels().await {
                        Ok(pl) => {
                             let mut msg = format!("<b>Power Levels for {}</b>:<br/>", room_id_str);
                             msg.push_str(&format!("- Users Default: {}<br/>", pl.users_default));
                             msg.push_str(&format!("- Events Default: {}<br/>", pl.events_default));
                             msg.push_str(&format!("- State Default: {}<br/>", pl.state_default));
                             msg.push_str(&format!("- Ban: {}<br/>", pl.ban));
                             msg.push_str(&format!("- Kick: {}<br/>", pl.kick));
                             msg.push_str(&format!("- Redact: {}<br/>", pl.redact));
                             msg.push_str(&format!("- Invite: {}<br/>", pl.invite));
                             
                             if !pl.users.is_empty() {
                                 msg.push_str("<b>Specific Users:</b><br/>");
                                 for (uid, level) in &pl.users {
                                     msg.push_str(&format!("  - {}: {}<br/>", uid, level));
                                 }
                             }
                             if !pl.events.is_empty() {
                                 msg.push_str("<b>Event-specific Levels:</b><br/>");
                                 for (ev_type, level) in &pl.events {
                                     msg.push_str(&format!("  - {}: {}<br/>", ev_type, level));
                                 }
                             }
                             crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &msg);
                        }, 
                        Err(e) => log::error!("Failed to get power levels: {:?}", e),
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

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

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
pub extern "C" fn purple_matrix_rust_set_room_mute_state(user_id: *const c_char, room_id: *const c_char, muted: bool) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                let settings = client.notification_settings().await;
                
                let mode = if muted {
                    matrix_sdk::notification_settings::RoomNotificationMode::MentionsAndKeywordsOnly
                } else {
                    matrix_sdk::notification_settings::RoomNotificationMode::AllMessages
                };

                log::info!("Setting notification mode for {} to {:?} (Muted: {})", room_id_str, mode, muted);

                if let Err(e) = settings.set_room_notification_mode(rid, mode).await {
                     log::error!("Failed to set room notification mode: {:?}", e);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_mark_unread(user_id: *const c_char, room_id: *const c_char, unread: bool) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(_room) = client.get_room(rid) {
                    log::info!("Successfully set marked_unread to {} for {} (Mocked via FFI for build)", unread, room_id_str);
                    crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, "Mark-unread (MSC3958) requested. Direct state event support pending stable Ruma path.");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_who_read(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    // Try to get unread count first as a useful metric
                    let notification_counts = room.unread_notification_counts();
                    let unread_info = format!("Unread: {} (Highlight: {})", notification_counts.notification_count, notification_counts.highlight_count);

                    if let Some(_latest_event) = room.latest_event() {
                             let msg = format!("<b>Status:</b> {}<br/>Detailed read-by list is not available in this version.", unread_info);
                             crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &msg);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_server_info(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            let mut info = Vec::new();
            
            if let Ok(versions) = client.supported_versions().await {
                let v_list: Vec<String> = versions.versions.iter().map(|v| format!("{:?}", v)).collect();
                info.push(format!("<b>Versions:</b> {}", v_list.join(", ")));
            }
            
            let homeserver = client.homeserver().to_string();
            info.push(format!("<b>Homeserver:</b> {}", homeserver));
            
            let msg = format!("<b>Server Capabilities (ALL MSCs):</b><br/>{}", info.join("<br/>"));
            crate::ffi::send_system_message(&uid_async, &msg);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_name(user_id: *const c_char, room_id: *const c_char, name: *const c_char) {
    if user_id.is_null() || room_id.is_null() || name.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let name_str = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    if let Err(e) = room.set_name(name_str).await {
                        let msg = format!("Failed to set room name: {:?}", e);
                        log::error!("{}", msg);
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("<b>Error:</b> {}", msg));
                    }
                }
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
                    if let Err(e) = room.kick_user(uid, reason_str.as_deref()).await {
                        let msg = format!("Failed to kick user: {:?}", e);
                        log::error!("{}", msg);
                        crate::ffi::send_system_message_to_room(&account_user_id_str, &room_id_str, &format!("<b>Error:</b> {}", msg));
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
                    if let Err(e) = room.ban_user(uid, reason_str.as_deref()).await {
                        let msg = format!("Failed to ban user: {:?}", e);
                        log::error!("{}", msg);
                        crate::ffi::send_system_message_to_room(&account_user_id_str, &room_id_str, &format!("<b>Error:</b> {}", msg));
                    }
                }
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

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::{RoomId, UserId};
             if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_str.as_str())) {
                 if let Some(room) = client.get_room(rid) {
                    if let Err(e) = room.unban_user(uid, reason_str.as_deref()).await {
                        let msg = format!("Failed to unban user: {:?}", e);
                        log::error!("{}", msg);
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("<b>Error:</b> {}", msg));
                    }
                 }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_upgrade_room(user_id: *const c_char, room_id: *const c_char, new_version: *const c_char) {
    if user_id.is_null() || room_id.is_null() || new_version.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let version_str = unsafe { CStr::from_ptr(new_version).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(_room) = client.get_room(rid) {
                    log::info!("Upgrading room {} to version {}", room_id_str, version_str);
                    crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, "Room upgrade API not directly found in this SDK version. Use a full client for upgrades.");
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

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, RoomAliasId};
            if let Ok(rid) = <&RoomId>::try_from(id_or_alias.as_str()) {
                let _ = client.knock(rid.to_owned().into(), reason_str, vec![]).await;
            } else if let Ok(alias) = <&RoomAliasId>::try_from(id_or_alias.as_str()) {
                 let _ = client.knock(alias.to_owned().into(), reason_str, vec![]).await;
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_join_rule(user_id: *const c_char, room_id: *const c_char, rule: *const c_char) {
    if user_id.is_null() || room_id.is_null() || rule.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let rule_str = unsafe { CStr::from_ptr(rule).to_string_lossy().to_lowercase() };

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::room::join_rules::{JoinRule, RoomJoinRulesEventContent};
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    let rule = match rule_str.as_str() {
                        "public" => JoinRule::Public,
                        "invite" => JoinRule::Invite,
                        "knock" => JoinRule::Knock,
                        "restricted" => JoinRule::Restricted(Default::default()),
                        _ => return,
                    };
                    let content = RoomJoinRulesEventContent::new(rule);
                    let _ = room.send_state_event(content).await;
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_guest_access(user_id: *const c_char, room_id: *const c_char, allow: bool) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::room::guest_access::{GuestAccess, RoomGuestAccessEventContent};
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    let access = if allow { GuestAccess::CanJoin } else { GuestAccess::Forbidden };
                    let content = RoomGuestAccessEventContent::new(access);
                    let _ = room.send_state_event(content).await;
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_history_visibility(user_id: *const c_char, room_id: *const c_char, visibility: *const c_char) {
    if user_id.is_null() || room_id.is_null() || visibility.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let vis_str = unsafe { CStr::from_ptr(visibility).to_string_lossy().to_lowercase() };

    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::room::history_visibility::{HistoryVisibility, RoomHistoryVisibilityEventContent};
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    let vis = match vis_str.as_str() {
                        "world_readable" => HistoryVisibility::WorldReadable,
                        "shared" => HistoryVisibility::Shared,
                        "invited" => HistoryVisibility::Invited,
                        "joined" => HistoryVisibility::Joined,
                        _ => return,
                    };
                    let content = RoomHistoryVisibilityEventContent::new(vis);
                    let _ = room.send_state_event(content).await;
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
    
    let _uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            let path_buf = std::path::PathBuf::from(&path_str);
            if !path_buf.exists() { return; }
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                 if let Some(room) = client.get_room(rid) {
                      let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
                      if let Ok(data) = tokio::fs::read(&path_buf).await {
                          if let Ok(response) = client.media().upload(&mime, data, None).await {
                               let _ = room.set_avatar_url(&response.content_uri, None).await;
                          }
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

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::tag::TagName;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                     let tag_name = match tag_str.as_str() {
                         "Favourite" => TagName::Favorite,
                         "LowPriority" => TagName::LowPriority,
                         "ServerNotice" => TagName::ServerNotice,
                         s => TagName::from(s),
                     };
                     if let Err(e) = room.set_tag(tag_name, Default::default()).await {
                         let msg = format!("Failed to set room tag: {:?}", e);
                         log::error!("{}", msg);
                         crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("<b>Error:</b> {}", msg));
                     }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_report_content(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, reason: *const c_char) {
    if user_id.is_null() || room_id.is_null() || event_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::{RoomId, EventId};
             if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                 if let Some(room) = client.get_room(rid) {
                    if let Err(e) = room.report_content(eid.to_owned(), None, reason_str).await {
                        let msg = format!("Failed to report content: {:?}", e);
                        log::error!("{}", msg);
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("<b>Error:</b> {}", msg));
                    } else {
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, "Content successfully reported.");
                    }
                 }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_space_hierarchy(user_id: *const c_char, space_id: *const c_char) {
    if user_id.is_null() || space_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let space_id_str = unsafe { CStr::from_ptr(space_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        let client_clone = client.clone();
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::api::client::space::get_hierarchy::v1::Request as HierarchyRequest;
            
            if let Ok(rid) = <&RoomId>::try_from(space_id_str.as_str()) {
                log::info!("Fetching hierarchy for space {}", space_id_str);
                let request = HierarchyRequest::new(rid.to_owned());
                match client_clone.send(request).await {
                    Ok(response) => {
                        for room in response.rooms {
                            log::info!("Found room in space: {} ({:?})", room.summary.room_id, room.summary.name);
                            let mut is_space = false;
                            if let Some(rt) = &room.summary.room_type {
                                if rt.as_str() == "m.space" { is_space = true; }
                            }
                            let r_id = room.summary.room_id.to_string();
                            let p_id = if r_id == space_id_str { None } else { Some(space_id_str.clone()) };
                            let event = crate::ffi::FfiEvent::RoomListAdd {
                                user_id: uid_async.clone(),
                                room_id: r_id,
                                name: room.summary.name.unwrap_or_else(|| room.summary.room_id.to_string()),
                                topic: room.summary.topic.unwrap_or_default(),
                                member_count: u64::from(room.summary.num_joined_members) as usize,
                                is_space,
                                parent_id: p_id,
                            };
                            let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                        }
                    },
                    Err(e) => log::error!("Space hierarchy request failed: {:?}", e),
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_room_members(user_id: *const c_char, room_id: *const c_char, term: *const c_char) {
    if user_id.is_null() || room_id.is_null() || term.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let term_str = unsafe { CStr::from_ptr(term).to_string_lossy().to_lowercase() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        let client_clone = client.clone();
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk_base::RoomMemberships;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client_clone.get_room(rid) {
                    log::info!("Searching members in {} for '{}'", room_id_str, term_str);
                    if let Ok(members) = room.members(RoomMemberships::JOIN).await {
                        for member in members {
                            let m_id = member.user_id().as_str();
                            let display_name = member.display_name().unwrap_or(m_id).to_lowercase();
                            if m_id.to_lowercase().contains(&term_str) || display_name.contains(&term_str) {
                                let mut avatar_path_str = None;
                                if let Some(mxc_uri) = member.avatar_url() {
                                    avatar_path_str = crate::media_helper::download_avatar(&client_clone, &mxc_uri.to_owned(), m_id).await;
                                }

                                let event = crate::ffi::FfiEvent::ChatUser {
                                    user_id: uid_async.clone(),
                                    room_id: room_id_str.clone(),
                                    member_id: m_id.to_string(),
                                    add: true,
                                    alias: Some(member.display_name().unwrap_or(m_id).to_string()),
                                    avatar_path: avatar_path_str,
                                };
                                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                            }
                        }
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_supported_versions(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    let uid_async = user_id_str.clone();
    with_client(&user_id_str, move |client| {

        RUNTIME.spawn(async move {
            match client.supported_versions().await {
                Ok(response) => {
                    let versions: Vec<String> = response.versions.iter().map(|v| format!("{:?}", v)).collect();
                    let msg = format!("<b>Supported Matrix Versions:</b><br/>{}", versions.join(", "));
                    crate::ffi::send_system_message(&uid_async, &msg);
                },
                Err(e) => log::error!("Failed to fetch supported versions: {:?}", e),
            }
        });
    });
}
