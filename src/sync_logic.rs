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

    if let Err(e) = client.sync_once(SyncSettings::default()).await {
        let error_str = e.to_string();
        if is_auth_failure(&error_str) { handle_auth_failure(&client_for_sync); return; }
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

        log::info!("Dispatching room {} ({}) to C. Account: {}", s_room_id, s_name, s_user_id);

        if let (Ok(c_user_id), Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_topic)) =
           (CString::new(s_user_id), CString::new(s_room_id), CString::new(s_name), CString::new(s_group), CString::new(s_topic))
        {
            let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), std::ptr::null(), c_topic.as_ptr(), is_encrypted);
            }
        }

        /* Avatar downloading temporarily disabled to isolate crash */
        /*
        let client_clone = client_for_sync.clone();
        let user_id_clone = user_id.clone();
        let room_id_clone = room_id.clone();
        let name_clone = name.clone();
        let group_clone = group.clone();
        let topic_clone = topic.clone();
        tokio::spawn(async move {
            if let Ok(ruma_room_id) = matrix_sdk::ruma::RoomId::parse(&room_id_clone) {
                if let Some(room) = client_clone.get_room(ruma_room_id.as_ref()) {
                    if let Some(url) = room.avatar_url() {
                        if let Some(path) = crate::media_helper::download_avatar(&client_clone, &url, &room_id_clone).await {
                             if let (Ok(c_user_id), Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) =
                                (CString::new(crate::sanitize_string(&user_id_clone)), CString::new(crate::sanitize_string(&room_id_clone)), CString::new(crate::sanitize_string(&name_clone)), CString::new(crate::sanitize_string(&group_clone)), CString::new(path), CString::new(crate::sanitize_string(&topic_clone)))
                             {
                                 let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
                                 if let Some(cb) = *guard {
                                     cb(c_user_id.as_ptr(), c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr(), c_topic.as_ptr(), is_encrypted);
                                 }
                             }
                        }
                    }
                }
            }
        });
        */
        
        // Throttle to avoid hammering UI thread - 100ms is safe with sanitization
        tokio::time::sleep(std::time::Duration::from_millis(100)).await;
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

pub async fn fetch_room_history_logic(client: Client, room_id: String) {
    fetch_room_history_logic_with_limit(client, room_id, 50).await;
}

pub async fn fetch_room_history_logic_with_limit(client: Client, room_id: String, limit: u16) {
     use matrix_sdk::ruma::events::room::message::Relation;
     use matrix_sdk::ruma::RoomId;
     let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
     
     if let Ok(ruma_room_id) = <&RoomId>::try_from(room_id.as_str()) {
         if let Some(room) = client.get_room(ruma_room_id) {
             let mut options = matrix_sdk::room::MessagesOptions::backward();
             options.limit = limit.into();
             if let Some(token) = crate::PAGINATION_TOKENS.lock().unwrap().get(&room_id) { options.from = Some(token.clone()); }
             
             if let Ok(messages) = room.messages(options).await {
                 if let Some(end) = &messages.end { crate::PAGINATION_TOKENS.lock().unwrap().insert(room_id.clone(), end.clone()); }

                 for timeline_event in messages.chunk.iter().rev() {
                     if let Ok(event) = timeline_event.raw().deserialize() {
                         if let AnySyncTimelineEvent::MessageLike(matrix_sdk::ruma::events::AnySyncMessageLikeEvent::RoomMessage(msg_event)) = event {
                               if let Some(ev) = msg_event.as_original() {
                                   let sender = ev.sender.as_str();
                                   let thread_id = if let Some(Relation::Thread(thread)) = &ev.content.relates_to { Some(thread.event_id.as_str()) } else { None };
                                   let body = crate::handlers::messages::render_room_message(ev, &room).await;
                                   
                                   let s_user_id = crate::sanitize_string(&user_id);
                                   let s_sender = crate::sanitize_string(sender);
                                   let s_body = crate::sanitize_string(&body);
                                   let s_room_id = crate::sanitize_string(&room_id);
                                   let s_thread_id = crate::sanitize_string(thread_id.unwrap_or(""));
                                   let s_event_id = crate::sanitize_string(ev.event_id.as_str());

                                   if let (Ok(c_user_id), Ok(c_sender), Ok(c_body), Ok(c_room_id)) =
                                      (CString::new(s_user_id), CString::new(s_sender), CString::new(s_body), CString::new(s_room_id))
                                   {
                                       let c_thread_id = CString::new(s_thread_id).unwrap_or_default();
                                       let c_event_id = CString::new(s_event_id).unwrap_or_default();
                                       let timestamp: u64 = ev.origin_server_ts.0.into();
                                       let guard = MSG_CALLBACK.lock().unwrap();
                                       if let Some(cb) = *guard {
                                           cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), c_thread_id.as_ptr(), c_event_id.as_ptr(), timestamp);
                                       }
                                   }
                               }
                               // Throttle history loading to avoid UI thread bombardement - Nuclear Stability
                               tokio::time::sleep(std::time::Duration::from_millis(250)).await;
                         }
                     }
                 }
             }
         }
     }
}
