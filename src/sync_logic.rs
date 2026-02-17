use crate::ffi::{
    MSG_CALLBACK, ROOM_JOINED_CALLBACK, INVITE_CALLBACK
};
use crate::{CLIENTS, DATA_PATH};
use matrix_sdk::{
    config::SyncSettings,
    ruma::{
        events::{
            AnySyncTimelineEvent,
        },
    },
    Client, 
};
use std::ffi::CString;
use crate::handlers::{messages, presence, typing, reactions, room_state, account_data, polls, receipts};

fn is_auth_failure(error_str: &str) -> bool {
    error_str.contains("M_UNKNOWN_TOKEN")
        || error_str.contains("Token is not active")
        || error_str.contains("Invalid access token")
        || error_str.contains("401")
}

fn handle_auth_failure(client: &Client) {
    if let Some(user_id) = client.user_id().map(|u| u.to_string()) {
        CLIENTS.lock().unwrap().remove(&user_id);
        let entry = keyring::Entry::new("purple-matrix-rust", &user_id).unwrap();
        let _ = entry.delete_password();
    }

    if let Some(mut path) = DATA_PATH.lock().unwrap().clone() {
        path.push("session.json");
        if path.exists() { let _ = std::fs::remove_file(&path); }
    }

    let msg = "Matrix session expired. Please re-login.";
    if let Ok(c_msg) = CString::new(msg) {
        let guard = crate::ffi::LOGIN_FAILED_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard { cb(c_msg.as_ptr()); }
    }
}

pub async fn start_sync_loop(client: Client) {
    let client_for_sync = client.clone();
    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    
    log::info!("Starting sync loop for {}", user_id);

    // Initial sync to get latest state and some history
    match client.sync_once(SyncSettings::default()).await {
        Ok(response) => {
            log::info!("Initial sync complete for {}", user_id);
            for (room_id, joined_room) in response.rooms.joined {
                if let Some(room) = client.get_room(&room_id) {
                    let full_id = room_id.to_string();
                    for event in joined_room.timeline.events {
                        process_sync_event_for_history(&client, &room, &full_id, event).await;
                    }
                }
            }
        },
        Err(e) => {
            let error_str = e.to_string();
            if is_auth_failure(&error_str) { handle_auth_failure(&client_for_sync); return; }
        }
    }

    // Initial Room List Population
    let rooms = client_for_sync.joined_rooms();
    log::info!("Initial room population: found {} rooms for account {}", rooms.len(), user_id);

    for room in rooms {
        let room_id = room.room_id().as_str().to_string();
        let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| "Unknown Room".to_string());

        let group = crate::grouping::get_room_group_name(&room).await;
        let topic = room.topic().unwrap_or_default();
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.unwrap_or(None).is_some();
        
        let s_user_id = crate::sanitize_string(&user_id);
        let s_room_id = crate::sanitize_string(&room_id);
        let s_name = crate::sanitize_string(&name);
        let s_group = crate::sanitize_string(&group);
        let s_topic = crate::sanitize_string(&topic);

        if let (Ok(c_user_id), Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_topic)) =
           (CString::new(s_user_id), CString::new(s_room_id), CString::new(s_name), CString::new(s_group), CString::new(s_topic))
        {
            let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), std::ptr::null(), c_topic.as_ptr(), is_encrypted);
            }
        }

        let client_clone = client_for_sync.clone();
        let user_id_clone = user_id.clone();
        let room_id_clone = room_id.clone();
        let name_clone = name.clone();
        let group_clone = group.clone();
        let topic_clone = topic.clone();
        let is_encrypted_clone = is_encrypted;

        tokio::spawn(async move {
            if let Ok(ruma_room_id) = matrix_sdk::ruma::RoomId::parse(&room_id_clone) {
                if let Some(room) = client_clone.get_room(ruma_room_id.as_ref()) {
                    if let Some(url) = room.avatar_url() {
                        if let Some(path) = crate::media_helper::download_avatar(&client_clone, &url, &room_id_clone).await {
                             let s_user = crate::sanitize_string(&user_id_clone);
                             let s_room = crate::sanitize_string(&room_id_clone);
                             let s_name = crate::sanitize_string(&name_clone);
                             let s_group = crate::sanitize_string(&group_clone);
                             let s_topic = crate::sanitize_string(&topic_clone);

                             if let (Ok(c_user_id), Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) =
                                (CString::new(s_user), CString::new(s_room), CString::new(s_name), CString::new(s_group), CString::new(path), CString::new(s_topic))
                             {
                                 let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
                                 if let Some(cb) = *guard {
                                     cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr(), c_topic.as_ptr(), is_encrypted_clone);
                                 }
                             }
                        }
                    }
                }
            }
        });
        
        tokio::time::sleep(std::time::Duration::from_millis(50)).await;
    }

    // Pending Invites
    let invited_rooms = client_for_sync.invited_rooms();
    for room in invited_rooms {
        let room_id = room.room_id().as_str();
        if let (Ok(c_user_id), Ok(c_room_id)) = (CString::new(crate::sanitize_string(&user_id)), CString::new(crate::sanitize_string(room_id))) {
             let c_inviter = CString::new("Pending Invite").unwrap();
             let guard = INVITE_CALLBACK.lock().unwrap();
             if let Some(cb) = *guard { cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_inviter.as_ptr()); }
        }
    }

    client_for_sync.add_event_handler(messages::handle_room_message);
    client_for_sync.add_event_handler(messages::handle_encrypted);
    client_for_sync.add_event_handler(messages::handle_redaction);
    client_for_sync.add_event_handler(presence::handle_presence);
    client_for_sync.add_event_handler(typing::handle_typing);
    client_for_sync.add_event_handler(reactions::handle_reaction);
    client_for_sync.add_event_handler(reactions::handle_sticker);
    client_for_sync.add_event_handler(room_state::handle_room_topic);
    client_for_sync.add_event_handler(room_state::handle_room_member);
    client_for_sync.add_event_handler(room_state::handle_stripped_member);
    client_for_sync.add_event_handler(room_state::handle_tombstone);
    client_for_sync.add_event_handler(account_data::handle_account_data);
    client_for_sync.add_event_handler(polls::handle_poll_start);
    client_for_sync.add_event_handler(receipts::handle_receipt);

    client_for_sync.add_event_handler(|event: matrix_sdk::ruma::events::key::verification::request::ToDeviceKeyVerificationRequestEvent, client: Client| async move {
             crate::verification_logic::handle_verification_request(client, event).await;
    });

    if let Err(e) = client_for_sync.sync(SyncSettings::default()).await {
         let error_str = e.to_string();
         if is_auth_failure(&error_str) { handle_auth_failure(&client_for_sync); }
    }
}

async fn process_sync_event_for_history(client: &Client, room: &matrix_sdk::Room, room_id: &str, timeline_event: matrix_sdk::deserialized_responses::TimelineEvent) {
    use matrix_sdk::ruma::events::room::message::Relation;
    use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;

    let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    
    if let Ok(any_event) = timeline_event.raw().deserialize() {
        let sender = any_event.sender().to_string();
        let mut body = String::new();
        let mut cur_thread_id: Option<String> = None;
        let event_id = any_event.event_id().to_string();
        let timestamp = any_event.origin_server_ts().0.into();
        let mut is_encrypted = false;

        match any_event {
            AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(msg_event)) => {
                if let Some(ev) = msg_event.as_original() {
                    if let Some(Relation::Thread(thread)) = &ev.content.relates_to {
                        cur_thread_id = Some(thread.event_id.to_string());
                    }
                    body = crate::handlers::messages::render_room_message(ev, room).await;
                }
            },
            AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomEncrypted(_)) => {
                is_encrypted = true;
                body = "[Encrypted Message]".to_string();
            }
            _ => {}
        }

        if body.is_empty() { return; }

        let s_user_id = crate::sanitize_string(&user_id);
        let s_sender = crate::sanitize_string(&sender);
        let s_body = crate::sanitize_string(&body);
        
        let target_room_id = if let Some(tid) = &cur_thread_id {
            format!("{}|{}", room_id, tid)
        } else {
            room_id.to_string()
        };
        
        let s_room_id = crate::sanitize_string(&target_room_id);
        let s_thread_id = crate::sanitize_string(cur_thread_id.as_deref().unwrap_or(""));
        let s_event_id = crate::sanitize_string(&event_id);

        if let (Ok(c_user_id), Ok(c_sender), Ok(c_body), Ok(c_room_id)) =
            (CString::new(s_user_id), CString::new(s_sender), CString::new(s_body), CString::new(s_room_id))
        {
            let c_thread_id = CString::new(s_thread_id).unwrap_or_default();
            let c_event_id = CString::new(s_event_id).unwrap_or_default();
            let guard = MSG_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), c_thread_id.as_ptr(), c_event_id.as_ptr(), timestamp, is_encrypted);
            }
        }
    }
}

pub async fn fetch_room_history_logic(client: Client, room_id: String) {
    let limit = if room_id.contains('|') { 200 } else { 100 };
    fetch_room_history_logic_with_limit(client, room_id, limit).await;
}

pub async fn fetch_room_history_logic_with_limit(client: Client, full_room_id: String, limit: u16) {
     use matrix_sdk::ruma::events::room::message::Relation;
     use matrix_sdk::ruma::RoomId;
     use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;

     let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
     
     let (base_room_id, thread_root_id) = match full_room_id.split_once('|') {
         Some((r, t)) => (r.to_string(), Some(t.to_string())),
         None => (full_room_id.clone(), None),
     };

     if let Ok(ruma_room_id) = <&RoomId>::try_from(base_room_id.as_str()) {
         if let Some(room) = client.get_room(ruma_room_id) {
             let mut options = matrix_sdk::room::MessagesOptions::backward();
             options.limit = limit.into();
             if let Some(token) = crate::PAGINATION_TOKENS.lock().unwrap().get(&full_room_id) { options.from = Some(token.clone()); }
             
             if let Ok(messages) = room.messages(options).await {
                 if let Some(end) = &messages.end { crate::PAGINATION_TOKENS.lock().unwrap().insert(full_room_id.clone(), end.clone()); }

                 for timeline_event in messages.chunk.iter().rev() {
                     if let Ok(event) = timeline_event.raw().deserialize() {
                         let event_id = event.event_id().to_string();
                         let sender = event.sender().to_string();
                         let timestamp = event.origin_server_ts().0.into();
                         let mut body = String::new();
                         let mut cur_thread_id: Option<String> = None;
                         let mut is_encrypted = false;

                         match event {
                             AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(msg_event)) => {
                               if let Some(ev) = msg_event.as_original() {
                                   if let Some(Relation::Thread(thread)) = &ev.content.relates_to {
                                       cur_thread_id = Some(thread.event_id.to_string());
                                   }
                                   body = crate::handlers::messages::render_room_message(ev, &room).await;
                               }
                             },
                             AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomEncrypted(_)) => {
                                 is_encrypted = true;
                                 body = "[Encrypted Message]".to_string();
                             }
                             _ => {}
                         }

                         if body.is_empty() { continue; }

                         if let Some(target_thread) = &thread_root_id {
                             let is_root = event_id == *target_thread;
                             let is_in_thread = cur_thread_id.as_deref() == Some(target_thread);
                             if !is_root && !is_in_thread { continue; }
                         } else {
                             if cur_thread_id.is_some() { continue; }
                         }

                         let s_user_id = crate::sanitize_string(&user_id);
                         let s_sender = crate::sanitize_string(&sender);
                         let s_body = crate::sanitize_string(&body);
                         let s_room_id = crate::sanitize_string(&full_room_id);
                         let s_thread_id = crate::sanitize_string(cur_thread_id.as_deref().unwrap_or(""));
                         let s_event_id = crate::sanitize_string(&event_id);

                         if let (Ok(c_user_id), Ok(c_sender), Ok(c_body), Ok(c_room_id)) =
                            (CString::new(s_user_id), CString::new(s_sender), CString::new(s_body), CString::new(s_room_id))
                         {
                             let c_thread_id = CString::new(s_thread_id).unwrap_or_default();
                             let c_event_id = CString::new(s_event_id).unwrap_or_default();
                             let guard = MSG_CALLBACK.lock().unwrap();
                             if let Some(cb) = *guard {
                                 cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), c_thread_id.as_ptr(), c_event_id.as_ptr(), timestamp, is_encrypted);
                             }
                         }
                         tokio::time::sleep(std::time::Duration::from_millis(20)).await;
                     }
                 }
             }
         }
     }
}
