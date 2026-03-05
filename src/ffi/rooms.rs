use std::os::raw::c_char;
use std::ffi::CStr;
use crate::ffi::{with_client, send_event, events::FfiEvent, HISTORY_FETCHED_ROOMS};
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

                                let event = FfiEvent::ChatUser {
                                    user_id: uid_async.clone(),
                                    room_id: room_id_str.clone(),
                                    member_id: m_id,
                                    add: true,
                                    alias,
                                    avatar_path,
                                };
                                send_event(event);
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
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

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

                        let event = FfiEvent::RoomJoined {
                            user_id: _uid_async,
                            room_id,
                            name,
                            group_name: "Matrix".to_string(),
                            avatar_url: None,
                            topic,
                            encrypted,
                            member_count,
                        };
                        send_event(event);
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
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        if let Ok(_) = room.leave().await {
                            let event = FfiEvent::RoomLeft {
                                user_id: uid_async,
                                room_id: room_id_str,
                            };
                            send_event(event);
                        }
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
        let user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.invite_user_by_id(uid).await;
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

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        use matrix_sdk::ruma::events::room::topic::RoomTopicEventContent;
                        let content = RoomTopicEventContent::new(topic_str);
                        let _ = room.send_state_event(content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_upgrade_room(user_id: *const c_char, room_id: *const c_char, version: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || version.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let version_str = unsafe { CStr::from_ptr(version).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(_room) = client.get_room(rid) {
                        log::info!("Upgrading room {} to version {}", room_id_str, version_str);
                        crate::ffi::send_system_message_to_room(&uid_async, &room_id_str, &format!("Upgrading room to version {}...", crate::escape_html(&version_str)));
                    }
                }
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

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.set_name(name_str).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_kick_user(user_id: *const c_char, room_id: *const c_char, target_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || target_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_id_str = unsafe { CStr::from_ptr(target_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let (Ok(rid), Ok(tid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.kick_user(tid, reason_str.as_deref()).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ban_user(user_id: *const c_char, room_id: *const c_char, target_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || target_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_id_str = unsafe { CStr::from_ptr(target_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let (Ok(rid), Ok(tid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.ban_user(tid, reason_str.as_deref()).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unban_user(user_id: *const c_char, room_id: *const c_char, target_id: *const c_char, reason: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || target_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let target_id_str = unsafe { CStr::from_ptr(target_id).to_string_lossy().into_owned() };
        let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::UserId;
                if let (Ok(rid), Ok(tid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let _ = room.unban_user(tid, reason_str.as_deref()).await;
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
                                let _p_id = if r_id == space_id_str { None } else { Some(space_id_str.clone()) };
                                let event = crate::ffi::FfiEvent::RoomJoined {
                                    user_id: uid_async.clone(),
                                    room_id: r_id,
                                    name: room.summary.name.unwrap_or_else(|| "Sub Room".to_string()),
                                    group_name: "Matrix".to_string(),
                                    avatar_url: None,
                                    topic: room.summary.topic.unwrap_or_default(),
                                    encrypted: false,
                                    member_count: u64::from(room.summary.num_joined_members),
                                    };
                                send_event(event);
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
pub extern "C" fn purple_matrix_rust_get_room_dashboard_info(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| room_id_str.clone());
                        
                        use matrix_sdk::ruma::events::room::topic::RoomTopicEventContent;
                        use matrix_sdk::ruma::events::{SyncStateEvent};
                        use matrix_sdk_base::deserialized_responses::SyncOrStrippedState;
                        
                        let topic = room.get_state_event_static::<RoomTopicEventContent>().await.ok().flatten()
                            .map(|e| e.deserialize().ok()).flatten()
                            .map(|s| match s {
                                SyncOrStrippedState::Sync(SyncStateEvent::Original(ev)) => ev.content.topic.clone(),
                                SyncOrStrippedState::Stripped(ev) => ev.content.topic.clone().unwrap_or_default(),
                                _ => "".to_string(),
                            }).unwrap_or_default();

                        let member_count = room.joined_members_count();
                        let encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.ok().flatten().is_some();
                        
                        let my_id = client.user_id().expect("Client has user id");
                        let power_level = room.get_member(my_id).await.ok().flatten()
                            .map(|m| {
                                use matrix_sdk::ruma::events::room::power_levels::UserPowerLevel;
                                match m.power_level() {
                                    UserPowerLevel::Int(i) => i.into(),
                                    UserPowerLevel::Infinite => i64::MAX,
                                    _ => 0,
                                }
                            }).unwrap_or(0);

                        use matrix_sdk::ruma::events::room::canonical_alias::RoomCanonicalAliasEventContent;
                        let alias = room.get_state_event_static::<RoomCanonicalAliasEventContent>().await.ok().flatten()
                            .map(|e| e.deserialize().ok()).flatten()
                            .and_then(|s| match s {
                                SyncOrStrippedState::Sync(SyncStateEvent::Original(ev)) => ev.content.alias.as_ref().map(|a| a.to_string()),
                                SyncOrStrippedState::Stripped(ev) => ev.content.alias.as_ref().map(|a| a.to_string()),
                                _ => None,
                            });

                        send_event(FfiEvent::RoomDashboardInfo {
                            user_id: uid_async,
                            room_id: room_id_str,
                            name,
                            topic,
                            member_count,
                            encrypted,
                            power_level,
                            alias,
                        });
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_server_versions(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                match client.server_versions().await {
                    Ok(resp) => {
                        let mut s = format!("Server supported versions: {:?}", resp);
                        s = crate::escape_html(&s);
                        crate::ffi::send_system_message(&uid_async, &s);
                    },
                    Err(e) => log::error!("Failed to fetch server versions: {:?}", e),
                }
            });
        });
    })
}
