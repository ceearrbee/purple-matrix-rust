use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_read_receipt(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    // Check if event_id is provided, otherwise we'll try to find the latest one
    let event_id_str = if !event_id.is_null() {
        Some(unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() })
    } else {
        None
    };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};
            use matrix_sdk::ruma::events::receipt::ReceiptThread;
            use matrix_sdk::ruma::api::client::receipt::create_receipt::v3::ReceiptType;

            if let Ok(room_id) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(room_id) {
                    
                    let target_event_id = if let Some(id) = event_id_str {
                        if let Ok(eid) = <&EventId>::try_from(id.as_str()) {
                            Some(eid.to_owned())
                        } else {
                            log::warn!("Invalid provided event ID for receipt: {}", id);
                            None
                        }
                    } else {
                        // Find latest event
                        room.latest_event().and_then(|ev| ev.event_id().map(|e| e.to_owned()))
                    };

                    if let Some(eid) = target_event_id {
                        log::info!("Sending Read Receipt for event {} in room {}", eid, room_id_str);
                        if let Err(e) = room.send_single_receipt(ReceiptType::Read, ReceiptThread::Unthreaded, eid).await {
                            log::error!("Failed to send read receipt: {:?}", e);
                        }
                    } else {
                        log::warn!("No event found to mark as read in room {}", room_id_str);
                    }
                }
            }
        });
    });
}



#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_message(user_id: *const c_char, room_id: *const c_char, text: *const c_char) {
    if user_id.is_null() || room_id.is_null() || text.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let text_str = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{events::room::message::Relation, RoomId, EventId};

            // Parse ID for Thread Support: "room_id|thread_root_id"
            let (room_id_str, thread_root_id_opt) = match id_str.split_once('|') {
                Some((r, t)) if !t.is_empty() => (r, Some(t)),
                _ => (id_str.as_str(), None),
            };

            if let Ok(room_id_ruma) = <&RoomId>::try_from(room_id_str) {
                
                // Slash Command Interception
                let final_text = if text_str.starts_with('/') {
                    match crate::handlers::commands::handle_slash_command(&client, room_id_ruma, &text_str).await {
                         Ok(crate::handlers::commands::CommandResult::Handled) => return,
                         Ok(crate::handlers::commands::CommandResult::Continue(t)) => t,
                         Err(e) => {
                             log::error!("Error handling slash command: {:?}", e);
                             let msg = format!("Command Error: {:?}", e);
                             crate::ffi::send_system_message(&user_id_str, &msg);
                             return;
                         }
                    }
                } else {
                    text_str
                };

                if let Some(room) = client.get_room(room_id_ruma) {
                    // Implicit Read Receipt
                     if let Some(latest_event) = room.latest_event() {
                         use matrix_sdk::ruma::events::receipt::ReceiptThread;
                         use matrix_sdk::ruma::api::client::receipt::create_receipt::v3::ReceiptType;
                         
                         if let Some(event_id) = latest_event.event_id() {
                             let event_id = event_id.to_owned();
                             // log::info!("Marking room {} as read up to {}", room_id_str, event_id);
                             if let Err(e) = room.send_single_receipt(ReceiptType::Read, ReceiptThread::Unthreaded, event_id).await {
                                 log::warn!("Failed to send read receipt: {:?}", e);
                             }
                         }
                     }
                    
                    let mut content = crate::create_message_content(final_text);
                    
                    // Attach Thread Relation if present
                    if let Some(thread_id_str) = thread_root_id_opt {
                        if let Ok(root_id) = <&EventId>::try_from(thread_id_str) {
                             log::info!("Sending message to thread {} in room {}", thread_id_str, room_id_str);
                             
                             content.relates_to = Some(Relation::Thread(matrix_sdk::ruma::events::relation::Thread::plain(root_id.to_owned(), root_id.to_owned())));
                        } else {
                            log::warn!("Invalid Event ID for thread root: {}", thread_id_str);
                        }
                    }

                    if let Err(e) = room.send(content).await {
                        log::error!("Failed to send message to room {}: {:?}", room_id_str, e);
                        let msg = format!("Failed to send message: {:?}", e);
                        crate::ffi::send_system_message(&user_id_str, &msg);
                    } else {
                        log::info!("Message sent successfully to room {}", room_id_str);
                    }
                } else {
                    log::error!("Failed to find room {} to send message.", room_id_str);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_im(account_user_id: *const c_char, target_user_id: *const c_char, text: *const c_char) {
    if account_user_id.is_null() || target_user_id.is_null() || text.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let text = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

    log::info!("Sending IM -> {} (from {})", target_user_id_str, account_user_id_str);

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;

            if let Ok(user_id_ruma) = <&UserId>::try_from(target_user_id_str.as_str()) {   
                log::info!("Resolving DM for {}", target_user_id_str);
                // Compiler says create_dm takes &UserId (single), not slice.
                match client.create_dm(user_id_ruma).await {
                    Ok(room) => {
                         // Force the type to break inference cycle
                         let room: matrix_sdk::Room = room;
                         let room_id_str = room.room_id().as_str();
                         log::info!("DM Resolved to room: {}", room_id_str);

                         // Check if the target user is actually in the room.
                         let target_user = user_id_ruma; 
                         
                         let needs_invite = match room.get_member(target_user).await {
                             Ok(Some(member)) => {
                                 use matrix_sdk::ruma::events::room::member::MembershipState;
                                 match member.membership() {
                                     MembershipState::Join => false,
                                     MembershipState::Invite => false,
                                     _ => true,
                                 }
                             },
                             Ok(None) => true,
                             Err(e) => {
                                 log::warn!("Failed to get member info for {}: {:?}. Assuming invite needed.", target_user, e);
                                 true
                             }
                         };

                         if needs_invite {
                             log::info!("Target user {} is not in room {}. Inviting...", target_user, room_id_str);
                             if let Err(e) = room.invite_user_by_id(target_user).await {
                                  log::error!("Failed to invite user {}: {:?}", target_user, e);
                             } else {
                                  log::info!("Invited user {} successfully.", target_user);
                             }
                         }
                         
                         // Ensure C side knows about this room (emit callback if it's new-ish)
                         
                         let content = crate::create_message_content(text);
                         if let Err(e) = room.send(content).await {
                             log::error!("Failed to send DM message: {:?}", e);
                             let msg = format!("Failed to send DM: {:?}", e);
                             crate::ffi::send_system_message(&account_user_id_str, &msg);
                         } else {
                             log::info!("DM Sent!");
                         }
                    },
                    Err(e) => {
                         log::error!("Failed to get or create DM with {}: {:?}", target_user_id_str, e);
                         let msg = format!("Failed to create DM with {}: {:?}", target_user_id_str, e);
                         crate::ffi::send_system_message(&account_user_id_str, &msg);
                    }
                }
            } else {
                log::error!("Invalid User ID format: {}", target_user_id_str);
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_typing(user_id: *const c_char, room_id_or_user_id: *const c_char, is_typing: bool) {
    if user_id.is_null() || room_id_or_user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(room_id_or_user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};

            // Case 1: It's a Room ID
            if let Ok(room_id) = <&RoomId>::try_from(id_str.as_str()) {
                if let Some(room) = client.get_room(room_id) {
                     // Implicit Read Receipt
                     if is_typing {
                         if let Some(latest_event) = room.latest_event() {
                             use matrix_sdk::ruma::events::receipt::ReceiptThread;
                             use matrix_sdk::ruma::api::client::receipt::create_receipt::v3::ReceiptType;
                             
                             if let Some(event_id) = latest_event.event_id() {
                                 let event_id = event_id.to_owned();
                                 if let Err(e) = room.send_single_receipt(ReceiptType::Read, ReceiptThread::Unthreaded, event_id).await {
                                     log::debug!("Failed to send read receipt on typing: {:?}", e);
                                 }
                             }
                         }
                     }
                    let _ = room.typing_notice(is_typing).await;
                }
            } 
            // Case 2: It's a User ID (DM)
            else if let Ok(user_id) = <&UserId>::try_from(id_str.as_str()) {
                 match client.create_dm(user_id).await {
                      Ok(room) => {
                          let room: matrix_sdk::Room = room; // Type fix
                          
                           // Implicit Read Receipt
                             if is_typing {
                                 if let Some(latest_event) = room.latest_event() {
                                     use matrix_sdk::ruma::events::receipt::ReceiptThread;
                                     use matrix_sdk::ruma::api::client::receipt::create_receipt::v3::ReceiptType;
                                     
                                     if let Some(event_id) = latest_event.event_id() {
                                         let event_id = event_id.to_owned();
                                         if let Err(e) = room.send_single_receipt(ReceiptType::Read, ReceiptThread::Unthreaded, event_id).await {
                                             log::debug!("Failed to send read receipt on typing (DM): {:?}", e);
                                         }
                                     }
                                 }
                             }
                          let _ = room.typing_notice(is_typing).await;
                      },
                      Err(e) => {
                          log::error!("Failed to resolve DM for typing to {}: {:?}", id_str, e);
                      }
                  }
            }
            else {
                log::warn!("Invalid ID for typing: {}", id_str);
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_sticker(user_id: *const c_char, room_id: *const c_char, url: *const c_char) {
    if user_id.is_null() || room_id.is_null() || url.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let url_str = unsafe { CStr::from_ptr(url).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, events::sticker::StickerEventContent, events::room::ImageInfo, OwnedMxcUri};
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Sending sticker to {}: {}", room_id_str, url_str);
                    
                    let mxc_uri = if url_str.starts_with("mxc://") {
                        url_str.clone()
                    } else {
                        use std::path::Path;

                        let path = Path::new(&url_str);
                        if path.exists() {
                             if let Ok(bytes) = std::fs::read(path) {
                                 let mime = mime_guess::from_path(path).first_or_octet_stream();
                                 if let Ok(response) = client.media().upload(&mime, bytes, None).await {
                                     response.content_uri.to_string()
                                 } else {
                                     log::error!("Failed to upload sticker file");
                                     return;
                                 }
                             } else {
                                 log::error!("Failed to read sticker file");
                                 return;
                             }
                        } else {
                             url_str.clone()
                        }
                    };

                    let uri = match <OwnedMxcUri>::try_from(mxc_uri.as_str()) {
                        Ok(u) => u,
                        Err(e) => {
                            log::error!("Failed to build MXC URI for sticker '{}': {:?}", mxc_uri, e);
                            return;
                        }
                    };
                    let info = ImageInfo::new();
                    let content = StickerEventContent::new("Sticker".to_string(), info, uri);
                    let any_content = AnyMessageLikeEventContent::Sticker(content);
                    
                    if let Err(e) = room.send(any_content).await {
                        log::error!("Failed to send sticker: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_file(user_id: *const c_char, id: *const c_char, filename: *const c_char) {
    if user_id.is_null() || id.is_null() || filename.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(id).to_string_lossy().into_owned() };
    let filename_str = unsafe { CStr::from_ptr(filename).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::{RoomId, UserId};
             use std::path::Path;
             use mime_guess::mime;
             
             let room_opt = if let Ok(room_id) = <&RoomId>::try_from(id_str.as_str()) {
                 client.get_room(room_id)
             } else if let Ok(user_id) = <&UserId>::try_from(id_str.as_str()) {
                 // Try to Open/Create DM
                 log::info!("File send target is User {}, resolving DM...", user_id);
                 match client.create_dm(user_id).await {
                     Ok(r) => Some(r),
                     Err(e) => {
                         log::error!("Failed to find/create DM for {}: {:?}", user_id, e);
                         None
                     }
                 }
             } else {
                 None
             };

             if let Some(room) = room_opt {
                 let path = Path::new(&filename_str);
                 if !path.exists() {
                     log::error!("File does not exist: {}", filename_str);
                     let msg = format!("File not found: {}", filename_str);
                     crate::ffi::send_system_message(&user_id_str, &msg);
                     return;
                 }
                 
                 if let Ok(bytes) = std::fs::read(path) {
                     let mime = mime_guess::from_path(path).first_or_octet_stream();
                     log::info!("Uploading file {} ({} bytes, mime: {})", filename_str, bytes.len(), mime);
                     
                     if let Ok(response) = client.media().upload(&mime, bytes, None).await {
                         let uri = response.content_uri;
                         let file_name = path.file_name().unwrap_or_default().to_string_lossy().into_owned();
                         
                         // Construct appropriate message content
                         use matrix_sdk::ruma::events::room::message::{RoomMessageEventContent, MessageType, ImageMessageEventContent, VideoMessageEventContent, AudioMessageEventContent, FileMessageEventContent};
                         
                         let msg_type = if mime.type_() == mime::IMAGE {
                             MessageType::Image(ImageMessageEventContent::plain(file_name.clone(), uri))
                         } else if mime.type_() == mime::VIDEO {
                             MessageType::Video(VideoMessageEventContent::plain(file_name.clone(), uri))
                         } else if mime.type_() == mime::AUDIO {
                             MessageType::Audio(AudioMessageEventContent::plain(file_name.clone(), uri))
                         } else {
                             MessageType::File(FileMessageEventContent::plain(file_name.clone(), uri))
                         };
                         
                         let content = RoomMessageEventContent::new(msg_type);
                         
                         if let Err(e) = room.send(content).await {
                             log::error!("Failed to send file event: {:?}", e);
                         } else {
                             log::info!("File sent successfully");
                             
                             // Delete temporary pasted images
                             if file_name.starts_with("matrix_pasted_") {
                                 log::info!("Cleaning up temporary pasted file: {}", filename_str);
                                 let _ = std::fs::remove_file(path);
                             }
                         }
                     } else {
                         log::error!("Failed to upload file");
                     }
                 }
             } else {
                 log::error!("Could not resolve target {} to a valid Room", id_str);
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_reply(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, text: *const c_char) {
    if user_id.is_null() || room_id.is_null() || event_id.is_null() || text.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
    let text_str = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};
            use matrix_sdk::ruma::events::room::message::Relation;
            use matrix_sdk::ruma::events::relation::InReplyTo;
            
            if let (Ok(room_id), Ok(event_id)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                if let Some(room) = client.get_room(room_id) {
                    let mut content = crate::create_message_content(text_str);
                    
                    // Construct reply relation (basic)
                    content.relates_to = Some(Relation::Reply { in_reply_to: InReplyTo::new(event_id.to_owned()) });
                    
                    if let Err(e) = room.send(content).await {
                        log::error!("Failed to send reply to {}: {:?}", room_id_str, e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_reaction(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, key: *const c_char) {
    if user_id.is_null() || room_id.is_null() || event_id.is_null() || key.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
    let key_str = unsafe { CStr::from_ptr(key).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};
            use matrix_sdk::ruma::events::reaction::ReactionEventContent;
            use matrix_sdk::ruma::events::relation::Annotation;

            if let (Ok(room_id), Ok(event_id)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                if let Some(room) = client.get_room(room_id) {
                    log::info!("Sending reaction {} to event {} in room {}", key_str, event_id_str, room_id_str);
                    let content = ReactionEventContent::new(Annotation::new(event_id.to_owned(), key_str));
                    if let Err(e) = room.send(content).await {
                        log::error!("Failed to send reaction: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_redact_event(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, reason: *const c_char) {
    if user_id.is_null() || room_id.is_null() || event_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() })
    };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};

            if let (Ok(room_id), Ok(event_id)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                if let Some(room) = client.get_room(room_id) {
                    log::info!("Redacting event {} in room {}", event_id_str, room_id_str);
                    if let Err(e) = room.redact(event_id, reason_str.as_deref(), None).await {
                        log::error!("Failed to redact event: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_edit(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, text: *const c_char) {
    if user_id.is_null() || room_id.is_null() || event_id.is_null() || text.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let event_id_str = unsafe { CStr::from_ptr(event_id).to_string_lossy().into_owned() };
    let text_str = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::{RoomId, EventId};
             use matrix_sdk::ruma::events::room::message::Relation;
             use matrix_sdk::ruma::events::relation::Replacement;
             
             if let (Ok(room_id), Ok(event_id)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                 if let Some(room) = client.get_room(room_id) {
                      let new_content = crate::create_message_content(text_str.clone());
                      let mut content = crate::create_message_content(format!("* {}", text_str)); 
                      // Edit content usually has fallback text "* original text" in body
                      
                      content.relates_to = Some(Relation::Replacement(Replacement::new(event_id.to_owned(), new_content.into())));
                      
                      if let Err(e) = room.send(content).await {
                          log::error!("Failed to edit message {}: {:?}", event_id_str, e);
                      }
                  }
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_location(user_id: *const c_char, room_id: *const c_char, lat: f64, lon: f64) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::room::message::{RoomMessageEventContent, MessageType, LocationMessageEventContent};

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                     let body = format!("Location: {}, {}", lat, lon);
                     let geo_uri = format!("geo:{},{}", lat, lon);
                     let loc = LocationMessageEventContent::new(body, geo_uri);
                     // Add info?
                     
                     let content = RoomMessageEventContent::new(MessageType::Location(loc));
                     
                     if let Err(e) = room.send(content).await {
                         log::error!("Failed to send location: {:?}", e);
                     }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bulk_redact(user_id: *const c_char, room_id: *const c_char, target_user: *const c_char) {
    if user_id.is_null() || room_id.is_null() || target_user.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_user_str = unsafe { CStr::from_ptr(target_user).to_string_lossy().into_owned() };
    
    log::info!("Bulk redacting messages from {} in {}", target_user_str, room_id_str);

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::RoomId;
             use matrix_sdk::room::MessagesOptions;
             
             if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                 if let Some(room) = client.get_room(rid) {
                     let mut options = MessagesOptions::backward();
                     options.limit = 100u16.into();
                     
                     if let Ok(messages) = room.messages(options).await {
                         for ev in messages.chunk {
                             if let Some(event_id) = ev.event_id() {
                                 if let Ok(raw) = ev.raw().deserialize() {
                                      if let matrix_sdk::ruma::events::AnySyncTimelineEvent::MessageLike(msg_ev) = raw {
                                           if msg_ev.sender().as_str() == target_user_str {
                                               let _ = room.redact(&event_id, Some("Bulk Redaction"), None).await;
                                           }
                                      }
                                 }
                             }
                         }
                     }
                 }
             }
        });
    });
}
