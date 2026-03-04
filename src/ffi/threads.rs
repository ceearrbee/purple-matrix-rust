use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};


#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_threads(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        let client_clone = client.clone();
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::room::ListThreadsOptions;
            use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent};

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client_clone.get_room(rid) {
                    log::info!("Listing all threads for {} using server API", room_id_str);
                    
                    // Proactively trigger key download for this room if backup is enabled
                    let encryption = client_clone.encryption();
                    let backups = encryption.backups();
                    if backups.are_enabled().await {
                        log::info!("Pre-fetching keys from backup for room {}", room_id_str);
                        let _ = backups.download_room_keys_for_room(rid).await;
                    }

                    let mut from_token: Option<String> = None;
                    let mut all_threads_found = false;

                    while !all_threads_found {
                        let mut options = ListThreadsOptions::default();
                        options.from = from_token.clone();

                        if let Ok(threads) = room.list_threads(options).await {
                            let _c_user_id = CString::new(crate::sanitize_string(&user_id_str)).unwrap_or_default();
                            let _c_room_id = CString::new(crate::sanitize_string(&room_id_str)).unwrap_or_default();
                            
                            if threads.chunk.is_empty() {
                                log::info!("No more threads found for {}", room_id_str);
                                all_threads_found = true;
                                continue;
                            }

                                                        for thread_root in &threads.chunk {
                                                            let root_id = thread_root.event_id().map(|e| e.to_string()).unwrap_or_default();
                                                            let _c_root_id = CString::new(crate::sanitize_string(&root_id)).unwrap_or_default();
                                                            
                                                            // 1. Get First Message (the root event itself)
                                                            let mut first_msg = String::new();
                                                            let mut root_raw = thread_root.raw().clone();
                                                            let mut root_decryption_failed = false;
                                                            
                                                            // Retry loop for decryption
                                                                                            for attempt in 0..2 {
                                                                                                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomEncrypted(encrypted_ev))) = root_raw.deserialize() {
                                                                                                    let _sender = encrypted_ev.sender().to_string();
                                                                                                    let raw_json = root_raw.json().get().to_string();
                                                            
                                                                    if let Ok(raw_original) = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::encrypted::OriginalSyncRoomEncryptedEvent>::from_json_string(raw_json) {
                                                                        match room.decrypt_event(&raw_original, None).await {
                                                                            Ok(decrypted) => {
                                                                                log::debug!("Successfully decrypted thread root {} on attempt {}", root_id, attempt + 1);
                                                                                if let Ok(parsed) = matrix_sdk::ruma::serde::Raw::from_json_string(decrypted.raw().json().get().to_string()) {
                                                                                    root_raw = parsed;
                                                                                }
                                                                                root_decryption_failed = false;
                                                                                break;
                                                                            },
                                                                                                                            Err(e) => { 
                                                                                                                                log::warn!("Failed to decrypt thread root {} (attempt {}): {:?}", root_id, attempt + 1, e);
                                                                                                                                root_decryption_failed = true; 
                                                                                                                                
                                                                                                                                if attempt == 0 {
                                                                                                                                    // First failure: Log session info and attempt targeted backup download
                                                                                                                                    if let Some(ev) = encrypted_ev.as_original() {
                                                                                                                                         use matrix_sdk::ruma::events::room::encrypted::EncryptedEventScheme;
                                                                                                                                         if let EncryptedEventScheme::MegolmV1AesSha2(megolm) = &ev.content.scheme {
                                                                                                                                             log::info!("Pending key for session {} in room {}. Attempting targeted backup recovery...", megolm.session_id, room.room_id());
                                                                                                                                             // Try to download this specific key
                                                                                                                                             let _ = backups.download_room_key(room.room_id(), &megolm.session_id).await;
                                                                                                                                         }
                                                                                                                                    }
                                                                                                                                    tokio::time::sleep(std::time::Duration::from_millis(20)).await; // Shorter wait
                                                                                                                                }
                                                                                                                            }
                                                                            
                                                                        }
                                                                    }
                                                                } else {
                                                                    break; // Not encrypted or already decrypted
                                                                }
                                                            }
                            
                                                            if let Ok(AnySyncTimelineEvent::MessageLike(msg_ev)) = root_raw.deserialize() {
                                                                if let AnySyncMessageLikeEvent::RoomMessage(original) = msg_ev {
                                                                    first_msg = original.as_original().map(|o| o.content.body().to_string()).unwrap_or_default();
                                                                } else if let AnySyncMessageLikeEvent::RoomEncrypted(_) = msg_ev {
                                                                    first_msg = "[Encrypted]".to_string();
                                                                }
                                                            }
                                                            if first_msg.is_empty() { 
                                                                first_msg = if root_decryption_failed { "[Decryption failed]" } else { "Root message unavailable" }.to_string(); 
                                                            }
                            
                                                            // 2. Get Latest Message
                                                            let mut latest_msg = String::new();
                                                            let mut latest_decryption_failed = false;
                                                            if let Some(ev) = &thread_root.bundled_latest_thread_event {
                                                                let latest_id = ev.event_id().map(|e| e.to_string()).unwrap_or_else(|| "unknown".to_string());
                                                                let mut latest_raw_val = ev.raw().clone();
                                                                
                                                                                                    for attempt in 0..2 {
                                                                
                                                                                                        if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomEncrypted(encrypted_ev))) = latest_raw_val.deserialize() {
                                                                
                                                                                                            let _sender = encrypted_ev.sender().to_string();
                                                                
                                                                                                            let raw_json = latest_raw_val.json().get().to_string();
                                                                
                                                                
                                                                        if let Ok(raw_original) = matrix_sdk::ruma::serde::Raw::<matrix_sdk::ruma::events::room::encrypted::OriginalSyncRoomEncryptedEvent>::from_json_string(raw_json) {
                                                                            match room.decrypt_event(&raw_original, None).await {
                                                                                Ok(decrypted) => {
                                                                                    log::debug!("Successfully decrypted latest thread event {} on attempt {}", latest_id, attempt + 1);
                                                                                    if let Ok(parsed) = matrix_sdk::ruma::serde::Raw::from_json_string(decrypted.raw().json().get().to_string()) {
                                                                                        latest_raw_val = parsed;
                                                                                    }
                                                                                    latest_decryption_failed = false;
                                                                                    break;
                                                                                },
                                                                                                                                    Err(e) => { 
                                                                                                                                        log::warn!("Failed to decrypt latest thread event {} (attempt {}): {:?}", latest_id, attempt + 1, e);
                                                                                                                                        latest_decryption_failed = true; 
                                                                                                                                        if attempt == 0 {
                                                                                                                                            if let Some(e_ev) = encrypted_ev.as_original() {
                                                                                                                                                use matrix_sdk::ruma::events::room::encrypted::EncryptedEventScheme;
                                                                                                                                                if let EncryptedEventScheme::MegolmV1AesSha2(megolm) = &e_ev.content.scheme {
                                                                                                                                                    let _ = backups.download_room_key(room.room_id(), &megolm.session_id).await;
                                                                                                                                                }
                                                                                                                                            }
                                                                                                                                            tokio::time::sleep(std::time::Duration::from_millis(20)).await; 
                                                                                                                                        }
                                                                                                                                    }
                                                                                
                                                                            }
                                                                        }
                                                                    } else {
                                                                        break;
                                                                    }
                                                                }
                            
                                                                if let Ok(AnySyncTimelineEvent::MessageLike(msg_ev)) = latest_raw_val.deserialize() {
                                                                    if let AnySyncMessageLikeEvent::RoomMessage(original) = msg_ev {
                                                                        latest_msg = original.as_original().map(|o| o.content.body().to_string()).unwrap_or_default();
                                                                    } else if let AnySyncMessageLikeEvent::RoomEncrypted(_) = msg_ev {
                                                                        latest_msg = "[Encrypted]".to_string();
                                                                    }
                                                                }
                                                            }
                                                            if latest_msg.is_empty() { 
                                                                latest_msg = if latest_decryption_failed { "[Decryption failed]" } else { "No replies yet" }.to_string(); 
                                                            }
                                                            // Descriptive label
                                let body = format!("Start: {} ... End: {}", first_msg, latest_msg);

                                // Extract actual message count from thread summary
                                let summary_json = serde_json::to_value(&thread_root.thread_summary).ok();
                                let msg_count: u64 = summary_json.as_ref()
                                    .and_then(|v| {
                                        let val = v.get("Some").unwrap_or(v);
                                        val.get("num_replies")
                                         .or_else(|| val.get("count"))
                                         .and_then(|n| n.as_u64())
                                    })
                                    .unwrap_or(0);

                                log::info!("Thread {} has {} replies.", root_id, msg_count);
                                
                                let ts: u64 = if let Some(latest) = &thread_root.bundled_latest_thread_event {
                                    latest.timestamp().map(|t| t.0.into()).unwrap_or_else(|| thread_root.timestamp().map(|t| t.0.into()).unwrap_or(0))
                                } else {
                                    thread_root.timestamp().map(|t| t.0.into()).unwrap_or(0)
                                };

                                let event = crate::ffi::FfiEvent::ThreadList {
                                    user_id: user_id_str.clone(),
                                    room_id: room_id_str.clone(),
                                    thread_root_id: Some(root_id.clone()),
                                    latest_msg: Some(body),
                                    count: msg_count,
                                    ts,
                                };
                                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                            }
                            
                            if let Some(next_token) = threads.prev_batch_token {
                                if Some(&next_token) == from_token.as_ref() {
                                    log::warn!("Pagination token hasn't changed, stopping to avoid infinite loop");
                                    all_threads_found = true;
                                } else {
                                    from_token = Some(next_token);
                                }
                            } else {
                                all_threads_found = true;
                            }
                        } else {
                            log::error!("list_threads API failed for {}", room_id_str);
                            all_threads_found = true; 
                        }
                    }
                    
                    let event = crate::ffi::FfiEvent::ThreadList {
                        user_id: user_id_str.clone(),
                        room_id: room_id_str.clone(),
                        thread_root_id: None,
                        latest_msg: None,
                        count: 0,
                        ts: 0,
                    };
                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                }
            }
        });
    });
}
