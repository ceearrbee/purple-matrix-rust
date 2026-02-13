use crate::ffi::{
    MSG_CALLBACK, ROOM_JOINED_CALLBACK, INVITE_CALLBACK
};
use crate::{CLIENTS, DATA_PATH};
// use crate::RUNTIME;
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
    // Clear stale local session so reconnect performs fresh auth.
    if let Some(user_id) = client.user_id().map(|u| u.to_string()) {
        CLIENTS.lock().unwrap().remove(&user_id);
    }

    if let Some(mut path) = DATA_PATH.lock().unwrap().clone() {
        path.push("session.json");
        if path.exists() {
            if let Err(err) = std::fs::remove_file(&path) {
                log::warn!("Failed to remove stale session file {:?}: {:?}", path, err);
            } else {
                log::info!("Removed stale session file after auth failure: {:?}", path);
            }
        }
    }

    let msg = "Matrix session expired or token invalidated. Please re-login.";
    if let Ok(c_msg) = CString::new(msg) {
        let guard = crate::ffi::LOGIN_FAILED_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_msg.as_ptr());
        }
    }
}

pub async fn start_sync_loop(client: Client) {
    let client_for_sync = client.clone();
    
    log::info!("Starting sync loop");

    if let Err(e) = client.sync_once(SyncSettings::default()).await {
        let error_str = e.to_string();
        log::error!("Initial sync failed: {:?}", e);
        if is_auth_failure(&error_str) {
            handle_auth_failure(&client_for_sync);
            return;
        }
    }

    // Initial Room List Population
    let rooms = client_for_sync.joined_rooms();
    for room in rooms {
        let room_id = room.room_id().as_str().to_string();
        let name = room.display_name().await.map(|d| d.to_string()).unwrap_or_else(|_| "Unknown Room".to_string());

        // Check if it is a space
        if room.is_space() {
            log::info!("Skipping space {} for chat list.", room_id);
            continue;
        }

        let group = crate::grouping::get_room_group_name(&room).await;
        let topic = room.topic().unwrap_or_default();
        let is_encrypted = room.get_state_event_static::<matrix_sdk::ruma::events::room::encryption::RoomEncryptionEventContent>().await.unwrap_or(None).is_some();
        
        log::info!("Populating initial room: {} ({}) in group {}. Encrypted: {}", room_id, name, group, is_encrypted);

        if let (Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_topic)) =
           (CString::new(room_id.clone()), CString::new(name.clone()), CString::new(group.clone()), CString::new(topic.clone()))
        {
            let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                cb(c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), std::ptr::null(), c_topic.as_ptr(), is_encrypted);
            }
        }

        // Spawn avatar download in background
        let client_clone = client_for_sync.clone();
        let room_id_clone = room_id.clone();
        let name_clone = name.clone();
        let group_clone = group.clone();
        let topic_clone = topic.clone();
        let is_encrypted_clone = is_encrypted;

        tokio::spawn(async move {
            if let Some(room) = client_clone.get_room(matrix_sdk::ruma::RoomId::parse(&room_id_clone).unwrap().as_ref()) {
                if let Some(url) = room.avatar_url() {
                    if let Some(path) = crate::media_helper::download_avatar(&client_clone, &url, &room_id_clone).await {
                         log::info!("Updating avatar for {}: {}", room_id_clone, path);
                         if let (Ok(c_room_id), Ok(c_name), Ok(c_group), Ok(c_avatar), Ok(c_topic)) =
                            (CString::new(room_id_clone), CString::new(name_clone), CString::new(group_clone), CString::new(path), CString::new(topic_clone))
                         {
                             let guard = ROOM_JOINED_CALLBACK.lock().unwrap();
                             if let Some(cb) = *guard {
                                 cb(c_room_id.as_ptr(), c_name.as_ptr(), c_group.as_ptr(), c_avatar.as_ptr(), c_topic.as_ptr(), is_encrypted_clone);
                             }
                         }
                    }
                }
            }
        });
    }

    // Initial Invite Population (for pending invites)
    let invited_rooms = client_for_sync.invited_rooms();
    for room in invited_rooms {
        let room_id = room.room_id().as_str();
        let name = "Pending Invite";
        
        log::info!("Found pending invite for room: {}", room_id);
        
        if let Ok(c_room_id) = CString::new(room_id) {
             let c_inviter = CString::new(name).unwrap();
             
             let guard = INVITE_CALLBACK.lock().unwrap();
             if let Some(cb) = *guard {
                 cb(c_room_id.as_ptr(), c_inviter.as_ptr());
             }
        }
    }

    // Automatic History Sync is Disabled by default to prevent "Opening every channel".
    // TODO: Implement on-demand sync via command or lazy loading.
    /*
    log::info!("Scanning for preexisting threads...");
    for room in client_for_sync.joined_rooms() {
         let room_id = room.room_id().as_str().to_string();
         
         // Spawn a task per room to not block the loop too much
         RUNTIME.spawn(async move {
             use matrix_sdk::ruma::events::room::message::Relation;
             
             log::info!("Scanning room {} history for threads...", room_id);
             let mut options = matrix_sdk::room::MessagesOptions::backward();
             options.limit = 50u16.into();
             
             // Notify Start
             {
                  let c_sender = CString::new("System").unwrap_or_default();
                  let c_body = CString::new("[System] Syncing history...").unwrap_or_default();
                  let c_room_id = CString::new(room_id.clone()).unwrap_or_default();
                  let guard = MSG_CALLBACK.lock().unwrap();
                  if let Some(cb) = *guard {
                      cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null());
                  }
             }

             if let Ok(messages) = room.messages(options).await {
                 // Messages come in reverse order (newest first).
                 // We need to reverse them to display chronologically in Pidgin.
                 for timeline_event in messages.chunk.iter().rev() {
                     if let Ok(event) = timeline_event.raw().deserialize() {
                         if let AnySyncTimelineEvent::MessageLike(matrix_sdk::ruma::events::AnySyncMessageLikeEvent::RoomMessage(msg_event)) = event {
                              let ev = msg_event.as_original();
                              if let Some(ev) = ev {
                                  // Extract body and thread ID (if any)
                                  let sender = ev.sender.as_str();
                                  
                                  // Determine thread ID
                                  let thread_id = if let Some(Relation::Thread(thread)) = &ev.content.relates_to {
                                       Some(thread.event_id.as_str())
                                  } else {
                                       None
                                  };
                                  
                                  // Use shared helper for rendering
                                  let body = crate::handlers::messages::render_room_message(ev, &room).await;
                                  
                                  // Mark as history to avoid notification spam in Pidgin
                                  let final_body = format!("[History] {}", body);
                                  
                                  let c_sender = CString::new(sender).unwrap_or_default();
                                  let c_body = CString::new(final_body).unwrap_or_default();
                                  let c_room_id = CString::new(room_id.clone()).unwrap_or_default();
                                  let c_thread_id = CString::new(thread_id.unwrap_or("")).unwrap_or_default();
                                  let c_event_id = CString::new(ev.event_id.as_str()).unwrap_or_default();
                                  
                                  let guard = MSG_CALLBACK.lock().unwrap();
                                  if let Some(cb) = *guard {
                                      cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), c_thread_id.as_ptr(), c_event_id.as_ptr());
                                  }
                              }
                         }
                     }
                 }
             }
             
             // Notify End
             {
                  let c_sender = CString::new("System").unwrap_or_default();
                  let c_body = CString::new("[System] History synced.").unwrap_or_default();
                  let c_room_id = CString::new(room_id.clone()).unwrap_or_default();
                  let guard = MSG_CALLBACK.lock().unwrap();
                  if let Some(cb) = *guard {
                      cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null());
                  }
             }
         });
    }
    */

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

    // Verification
    client_for_sync.add_event_handler(
         |event: matrix_sdk::ruma::events::key::verification::request::ToDeviceKeyVerificationRequestEvent, client: Client| async move {
             use crate::verification_logic::handle_verification_request;
             handle_verification_request(client, event).await;
         }
    );

    let sync_settings = SyncSettings::default();
    if let Err(e) = client_for_sync.sync(sync_settings).await {
         log::error!("Sync loop terminated with error: {:?}", e);
         
         // Notify C side about the failure if it's an auth error
         let error_str = e.to_string();
         if is_auth_failure(&error_str) {
             handle_auth_failure(&client_for_sync);
         }
    }
}

// Logic to fetch history for a single room
pub async fn fetch_room_history_logic(client: Client, room_id: String) {
     fetch_room_history_logic_with_limit(client, room_id, 50).await;
}

pub async fn fetch_room_history_logic_with_limit(client: Client, room_id: String, limit: u16) {
     use matrix_sdk::ruma::events::room::message::Relation;
     use matrix_sdk::ruma::RoomId;
     
     if let Ok(ruma_room_id) = <&RoomId>::try_from(room_id.as_str()) {
         if let Some(room) = client.get_room(ruma_room_id) {
             log::info!("Scanning room {} history for threads...", room_id);
             let mut options = matrix_sdk::room::MessagesOptions::backward();
             options.limit = limit.into();
             
             // Check for pagination token
             if let Some(token) = crate::PAGINATION_TOKENS.lock().unwrap().get(&room_id) {
                 log::info!("Continuing history fetch from token: {}", token);
                 options.from = Some(token.clone());
             }
             
             // Notify Start
             {
                  let c_sender = CString::new("System").unwrap_or_default();
                  let c_body = CString::new("[System] Syncing history...").unwrap_or_default();
                  let c_room_id = CString::new(room_id.clone()).unwrap_or_default();
                  let guard = MSG_CALLBACK.lock().unwrap();
                  let mut timestamp: u64 = 0;
                  if let Ok(n) = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
                        timestamp = n.as_millis() as u64;
                  }
                  if let Some(cb) = *guard {
                      cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
                  }
             }

             if let Ok(messages) = room.messages(options).await {
                 // Update pagination token
                 if let Some(end) = &messages.end {
                     crate::PAGINATION_TOKENS.lock().unwrap().insert(room_id.clone(), end.clone());
                 }

                 for timeline_event in messages.chunk.iter().rev() {
                     if let Ok(event) = timeline_event.raw().deserialize() {
                         if let AnySyncTimelineEvent::MessageLike(matrix_sdk::ruma::events::AnySyncMessageLikeEvent::RoomMessage(msg_event)) = event {
                               let ev = msg_event.as_original();
                               if let Some(ev) = ev {
                                   let sender = ev.sender.as_str();
                                   let thread_id = if let Some(Relation::Thread(thread)) = &ev.content.relates_to {
                                        Some(thread.event_id.as_str())
                                   } else { None };
                                   
                                   let mut body = crate::handlers::messages::render_room_message(ev, &room).await;
                                   
                                   if thread_id.is_some() {
                                       body = format!("&nbsp;&nbsp;🧵 {}", body);
                                   }

                                   let final_body = format!("[History] {}", body);
                                   
                                   let c_sender = CString::new(sender).unwrap_or_default();
                                   let c_body = CString::new(final_body).unwrap_or_default();
                                   let c_room_id = CString::new(room_id.clone()).unwrap_or_default();
                                   let c_thread_id = CString::new(thread_id.unwrap_or("")).unwrap_or_default();
                                   let c_event_id = CString::new(ev.event_id.as_str()).unwrap_or_default();
                                   
                                   let timestamp: u64 = ev.origin_server_ts.0.into();
                                   let guard = MSG_CALLBACK.lock().unwrap();
                                   if let Some(cb) = *guard {
                                       cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), c_thread_id.as_ptr(), c_event_id.as_ptr(), timestamp);
                                   }
                               }
                         }
                     }
                 }
             }
             
             // Notify End
             {
                  let c_sender = CString::new("System").unwrap_or_default();
                  let c_body = CString::new("[System] History synced.").unwrap_or_default();
                  let c_room_id = CString::new(room_id.clone()).unwrap_or_default();
                  let guard = MSG_CALLBACK.lock().unwrap();
                  let mut timestamp: u64 = 0;
                  if let Ok(n) = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
                        timestamp = n.as_millis() as u64;
                  }
                  if let Some(cb) = *guard {
                      cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
                  }
             }
         }
     }
}
