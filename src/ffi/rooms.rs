use std::os::raw::{c_char, c_void};
use std::ffi::CStr;
use crate::ffi::{with_client, HISTORY_FETCHED_ROOMS};
use crate::RUNTIME;
use matrix_sdk::Client;
use matrix_sdk::ruma::RoomId;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_room_members(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        log::info!("Fetching members for room {}", room_id_str);
                        use matrix_sdk_base::RoomMemberships;
                        if let Ok(members) = room.members(RoomMemberships::JOIN).await {
                            for member in members {
                                let m_id = member.user_id().to_string();
                                let alias = member.display_name().map(|d| d.to_string());
                                let avatar_path = if let Some(url) = member.avatar_url() {
                                    crate::media_helper::download_avatar(&client, &url.to_owned(), &m_id).await
                                } else {
                                    None
                                };

                                let event = crate::ffi::FfiEvent::ChatUser {
                                    user_id: uid_async.clone(),
                                    room_id: room_id_str.clone(),
                                    member_id: m_id,
                                    add: true,
                                    alias,
                                    avatar_path,
                                };
                                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                            }
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_history(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        if HISTORY_FETCHED_ROOMS.contains(&room_id_str) {
            return;
        }
        HISTORY_FETCHED_ROOMS.insert(room_id_str.clone());

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let actual_room_id = if let Ok(user_id) = <&matrix_sdk::ruma::UserId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_dm_room(user_id) {
                        Some(room.room_id().to_string())
                    } else {
                        None
                    }
                } else {
                    Some(room_id_str)
                };

                if let Some(rid) = actual_room_id {
                    crate::sync_logic::fetch_room_history(client, rid).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_more_history(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        log::info!("On-demand history fetch for: {}", room_id_str);

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                crate::sync_logic::fetch_room_history(client, room_id_str).await;
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_resync_recent_history(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let mut room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        if let Some((base, _)) = room_id_str.split_once('|') {
            room_id_str = base.to_string();
        }

        HISTORY_FETCHED_ROOMS.remove(&room_id_str);
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                crate::sync_logic::fetch_room_history(client, room_id_str).await;
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_join_room(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let _uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{OwnedRoomOrAliasId};
                if let Ok(rid) = <OwnedRoomOrAliasId>::try_from(id_str.as_str()) {
                    if let Ok(room) = client.join_room_by_id_or_alias(&rid, &[]).await {
                        let room_id = room.room_id().to_string();
                        let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| "Joined Room".to_string());
                        let topic = "".to_string(); 
                        let encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
                        let member_count = room.joined_members_count();

                        let event = crate::ffi::FfiEvent::RoomJoined {
                            user_id: _uid_async,
                            room_id,
                            name,
                            group_name: "Matrix".to_string(),
                            avatar_url: None,
                            topic: Some(topic),
                            encrypted,
                            member_count,
                        };
                        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_leave_room(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let _uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                if let Ok(room_id) = <&RoomId>::try_from(id_str.as_str()) {
                    if let Some(room) = client.get_room(room_id) {
                        log::info!("Leaving room {}", id_str);
                        let _ = room.leave().await;
                        let event = crate::ffi::FfiEvent::RoomLeft {
                            user_id: _uid_async,
                            room_id: id_str,
                        };
                        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_invite_user(account_user_id: *const c_char, room_id: *const c_char, user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || room_id.is_null() || user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomId, UserId};
                let r_id = <&RoomId>::try_from(room_id_str.as_str());
                let u_id = <&UserId>::try_from(user_id_str.as_str());

                if let (Ok(room_id), Ok(user_id)) = (r_id, u_id) {
                    if let Some(room) = client.get_room(room_id) {
                        if let Err(e) = room.invite_user_by_id(user_id).await {
                            log::error!("Failed to invite user {}: {:?}", user_id_str, e);
                            crate::ffi::send_system_message_to_room(&account_user_id_str, &room_id_str, &format!("Invite failed: {:?}", e));
                        } else {
                            crate::ffi::send_system_message_to_room(&account_user_id_str, &room_id_str, &format!("Invited user {} successfully.", user_id_str));
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_room(user_id: *const c_char, name: *const c_char, topic: *const c_char, is_public: bool) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let name_str = if name.is_null() { None } else { Some(unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() }) };
        let topic_str = if topic.is_null() { None } else { Some(unsafe { CStr::from_ptr(topic).to_string_lossy().into_owned() }) };

        log::info!("Creating room: Name={:?}, Topic={:?}, Public={}", name_str, topic_str, is_public);

        let _uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::api::client::room::create_room::v3::RoomPreset;
                let mut request = matrix_sdk::ruma::api::client::room::create_room::v3::Request::new();
                request.name = name_str;
                request.topic = topic_str;
                request.preset = if is_public { Some(RoomPreset::PublicChat) } else { Some(RoomPreset::PrivateChat) };

                match client.create_room(request).await {
                    Ok(room) => {
                        log::info!("Successfully created room: {}", room.room_id());
                        crate::ffi::send_system_message(&_uid_async, &format!("Room created successfully: {}", room.room_id()));
                    },
                    Err(e) => {
                        log::error!("Failed to create room: {:?}", e);
                        crate::ffi::send_system_message(&_uid_async, &format!("Failed to create room: {:?}", e));
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_power_levels(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        match room.power_levels().await {
                            Ok(pl) => {
                                let client_user_id = client.user_id().map(|u| u.to_owned());
                                if let Some(self_id) = client_user_id {
                                    let my_level = pl.for_user(&self_id);
                                    let is_admin = my_level >= matrix_sdk::ruma::int!(100);
                                    let can_kick = my_level >= pl.kick;
                                    let can_ban = my_level >= pl.ban;
                                    let can_redact = my_level >= pl.redact;
                                    let can_invite = my_level >= pl.invite;

                                    let event = crate::ffi::FfiEvent::PowerLevelUpdate {
                                        user_id: uid_async,
                                        room_id: room_id_str,
                                        is_admin,
                                        can_kick,
                                        can_ban,
                                        can_redact,
                                        can_invite,
                                    };
                                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                                }
                            },
                            Err(e) => log::error!("Failed to fetch power levels for {}: {:?}", room_id_str, e),
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_topic(user_id: *const c_char, room_id: *const c_char, topic: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || topic.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let topic_str = unsafe { CStr::from_ptr(topic).to_string_lossy().into_owned() };

        let _uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        if let Err(e) = room.set_room_topic(&topic_str).await {
                            log::error!("Failed to set room topic: {:?}", e);
                            crate::ffi::send_system_message_to_room(&_uid_async, &room_id_str, &format!("Failed to set topic: {:?}", e));
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_mute_state(user_id: *const c_char, room_id: *const c_char, muted: bool) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let _uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    let settings = client.notification_settings().await;
                    use matrix_sdk::notification_settings::RoomNotificationMode;
                    let mode = if muted { RoomNotificationMode::Mute } else { RoomNotificationMode::AllMessages };
                    let _ = settings.set_room_notification_mode(rid, mode).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_mark_unread(user_id: *const c_char, room_id: *const c_char, unread: bool) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(_room) = client.get_room(rid) {
                        log::info!("Successfully set marked_unread to {} for {}", unread, room_id_str);
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("Room unread status set to {}.", unread));
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_who_read(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let _receipts = room.read_receipts();
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_server_info(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let url = client.homeserver().to_string();
                crate::ffi::send_system_message(&uid_async, &format!("Homeserver: {}", url));
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_name(user_id: *const c_char, room_id: *const c_char, name: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || name.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let name_str = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        if let Err(e) = room.set_name(name_str).await {
                            log::error!("Failed to set room name: {:?}", e);
                            crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("Failed to set name: {:?}", e));
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_kick_user(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomId, UserId};
                if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        if let Err(e) = room.kick_user(uid, reason_str.as_deref()).await {
                            log::error!("Failed to kick user {}: {:?}", target_user_id_str, e);
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ban_user(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
        let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&account_user_id_str.clone(), |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomId, UserId};
                if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        if let Err(e) = room.ban_user(uid, reason_str.as_deref()).await {
                            log::error!("Failed to ban user {}: {:?}", target_user_id_str, e);
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unban_user(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                 use matrix_sdk::ruma::{RoomId, UserId};
                 if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_str.as_str())) {
                     if let Some(room) = client.get_room(rid) {
                        if let Err(e) = room.unban_user(uid, reason_str.as_deref()).await {
                            log::error!("Failed to unban user {}: {:?}", target_str, e);
                        }
                     }
                 }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_upgrade_room(user_id: *const c_char, room_id: *const c_char, new_version: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || new_version.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let version_str = unsafe { CStr::from_ptr(new_version).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(_room) = client.get_room(rid) {
                        log::info!("Upgrading room {} to version {}", room_id_str, version_str);
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("Upgrading room to version {}...", ammonia::clean_text(&version_str)));
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_knock(user_id: *const c_char, room_id_or_alias: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id_or_alias.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let id_or_alias = unsafe { CStr::from_ptr(room_id_or_alias).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{OwnedRoomOrAliasId};
                if let Ok(rid) = <OwnedRoomOrAliasId>::try_from(id_or_alias.as_str()) {
                    let _ = client.knock(rid, reason_str, vec![]).await;
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_join_rule(user_id: *const c_char, room_id: *const c_char, rule: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || rule.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let rule_str = unsafe { CStr::from_ptr(rule).to_string_lossy().to_lowercase() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::room::join_rules::{JoinRule, RoomJoinRulesEventContent};
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let rule = match rule_str.as_str() {
                            "public" => JoinRule::Public,
                            "invite" => JoinRule::Invite,
                            "knock" => JoinRule::Knock,
                            "private" => JoinRule::Invite,
                            _ => return,
                        };
                        let content = RoomJoinRulesEventContent::new(rule);
                        let _ = room.send_state_event(content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_guest_access(user_id: *const c_char, room_id: *const c_char, allow: bool) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
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
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_history_visibility(user_id: *const c_char, room_id: *const c_char, visibility: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || visibility.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let vis_str = unsafe { CStr::from_ptr(visibility).to_string_lossy().to_lowercase() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::room::history_visibility::{HistoryVisibility, RoomHistoryVisibilityEventContent};
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let vis = match vis_str.as_str() {
                            "shared" => HistoryVisibility::Shared,
                            "invited" => HistoryVisibility::Invited,
                            "joined" => HistoryVisibility::Joined,
                            "world_readable" => HistoryVisibility::WorldReadable,
                            _ => return,
                        };
                        let content = RoomHistoryVisibilityEventContent::new(vis);
                        let _ = room.send_state_event(content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_avatar(user_id: *const c_char, room_id: *const c_char, path: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || path.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                let path_buf = std::path::PathBuf::from(&path_str);
                if !path_buf.exists() { return; }
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                     if let Some(room) = client.get_room(rid) {
                          let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
                          if let Ok(data) = std::fs::read(&path_buf) {
                               if let Ok(response) = client.media().upload(&mime, data, None).await {
                                    let mut content = matrix_sdk::ruma::events::room::avatar::RoomAvatarEventContent::new();
                                    content.url = Some(response.content_uri);
                                    let _ = room.send_state_event(content).await;
                                }
                          }
                     }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_tag(user_id: *const c_char, room_id: *const c_char, tag: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || tag.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let tag_str = unsafe { CStr::from_ptr(tag).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::tag::{TagName, UserTagName};
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                         let tag_name = match tag_str.as_str() {
                             "favorite" => TagName::Favorite,
                             "lowpriority" => TagName::LowPriority,
                             "server_notice" => TagName::ServerNotice,
                             _ => return,
                         };
                         let _ = room.set_tag(tag_name, matrix_sdk::ruma::events::tag::TagInfo::new()).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_report_content(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || event_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                 use matrix_sdk::ruma::{RoomId, EventId};
                 if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                     if let Some(room) = client.get_room(rid) {
                        if let Err(e) = room.report_content(eid.to_owned(), None, reason_str).await {
                            log::error!("Failed to report content: {:?}", e);
                        }
                     }
                 }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_space_hierarchy(user_id: *const c_char, space_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || space_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let space_id_str = unsafe { CStr::from_ptr(space_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            let client_clone = client.clone();
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::api::client::space::get_hierarchy::v1::Request as HierarchyRequest;

                if let Ok(rid) = <&RoomId>::try_from(space_id_str.as_str()) {
                    log::info!("Fetching hierarchy for space {}", space_id_str);
                    let request = HierarchyRequest::new(rid.to_owned());
                    match client_clone.send(request).await {
                        Ok(response) => {
                            for room in response.rooms {
                                let r_id = room.summary.room_id.to_string();
                                let p_id = if r_id == space_id_str { None } else { Some(space_id_str.clone()) };
                                let event = crate::ffi::FfiEvent::RoomJoined {
                                    user_id: uid_async.clone(),
                                    room_id: r_id,
                                    name: room.summary.name.unwrap_or_else(|| "Sub Room".to_string()),
                                    group_name: "Matrix".to_string(),
                                    avatar_url: None,
                                    topic: room.summary.topic,
                                    encrypted: false,
                                    member_count: u64::from(room.summary.num_joined_members),
                                    };
                                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                            }
                        },
                        Err(e) => log::error!("Failed to fetch space hierarchy: {:?}", e),
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_room_members(user_id: *const c_char, room_id: *const c_char, term: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || term.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let term_str = unsafe { CStr::from_ptr(term).to_string_lossy().to_lowercase() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            let client_clone = client.clone();
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk_base::RoomMemberships;
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client_clone.get_room(rid) {
                        log::info!("Searching members in {} for '{}'", room_id_str, term_str);
                        if let Ok(members) = room.members(RoomMemberships::JOIN).await {
                            let mut count = 0;
                            for member in members {
                                let m_id = member.user_id().to_string();
                                let m_name = member.display_name().map(|d| d.to_string()).unwrap_or_default().to_lowercase();
                                if m_id.to_lowercase().contains(&term_str) || m_name.contains(&term_str) {
                                    if count >= 50 { break; }
                                    count += 1;
                                    let avatar_path = if let Some(url) = member.avatar_url() {
                                        crate::media_helper::download_avatar(&client_clone, &url.to_owned(), &m_id).await
                                    } else {
                                        None
                                    };

                                    let event = crate::ffi::FfiEvent::ChatUser {
                                        user_id: uid_async.clone(),
                                        room_id: room_id_str.clone(),
                                        member_id: m_id,
                                        add: true,
                                        alias: member.display_name().map(|d| d.to_string()),
                                        avatar_path,
                                    };
                                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                                }
                            }
                        }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_supported_versions(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                match client.server_versions().await {
                    Ok(resp) => {
                        let mut s = format!("Server supported versions: {:?}", resp);
                        s = ammonia::clean_text(&s);
                        crate::ffi::send_system_message(&uid_async, &s);
                    },
                    Err(e) => log::error!("Failed to fetch server versions: {:?}", e),
                }
            });
        });
    })
}
