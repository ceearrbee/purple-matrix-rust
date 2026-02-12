use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::sync::Mutex;

use matrix_sdk::{
    Client, RoomState,
};
use once_cell::sync::Lazy;
use simple_logger::SimpleLogger;
use tokio::runtime::Runtime;

pub mod ffi;
pub mod handlers;
pub mod verification_logic;
pub mod sync_logic;
pub mod media_helper;

use crate::ffi::*;

// Global Runtime/Client
pub(crate) static RUNTIME: Lazy<Runtime> = Lazy::new(|| Runtime::new().unwrap());
// user_id -> Client
pub(crate) static CLIENTS: Lazy<Mutex<std::collections::HashMap<String, Client>>> = Lazy::new(|| Mutex::new(std::collections::HashMap::new()));
// Legacy global client for backward compatibility during refactor
// GLOBAL_CLIENT removed in favor of CLIENTS map
pub(crate) static HISTORY_FETCHED_ROOMS: Lazy<Mutex<std::collections::HashSet<String>>> = Lazy::new(|| Mutex::new(std::collections::HashSet::new()));
pub(crate) static PAGINATION_TOKENS: Lazy<Mutex<std::collections::HashMap<String, String>>> = Lazy::new(|| Mutex::new(std::collections::HashMap::new()));
pub(crate) static DATA_PATH: Lazy<Mutex<Option<std::path::PathBuf>>> = Lazy::new(|| Mutex::new(None));

pub(crate) fn with_client<F, R>(user_id: &str, f: F) -> Option<R>
where
    F: FnOnce(Client) -> R,
{
    let guard = CLIENTS.lock().unwrap();
    if let Some(client) = guard.get(user_id) {
        Some(f(client.clone()))
    } else {
        log::warn!("No client found for user_id: {}", user_id);
        None
    }
}


#[repr(C)]
pub struct MatrixClientHandle {
    _private: [u8; 0],
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init() {
    let _ = SimpleLogger::new().init();
    log::info!("Rust backend initialized");
    
    RUNTIME.spawn(async {
        media_helper::cleanup_media_files().await;
    });
}

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
                            // We aren't downloading avatars here yet to avoid massive traffic on join.
                            // The C side can decide to fetch if needed or we can enhance later.
                            
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
            let actual_room_id = if room_id_str.starts_with('@') {
                use matrix_sdk::ruma::UserId;
                if let Ok(user_id) = <&UserId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_dm_room(user_id) {
                        Some(room.room_id().to_string())
                    } else {
                        log::warn!("Could not find DM room for user {}. History fetch skipped.", room_id_str);
                        None
                    }
                } else {
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

                    // If we're already in the room, include a small recent activity sample.
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

pub mod auth;
pub mod grouping;

pub fn escape_html(input: &str) -> String {
    input
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#x27;")
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_status(user_id: *const c_char, status: i32, msg: *const c_char) {
     if user_id.is_null() { return; }
     let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
     
     let msg_str = if !msg.is_null() {
        unsafe { CStr::from_ptr(msg).to_string_lossy().into_owned() }
    } else {
        String::new()
    };
    
    log::info!("Setting status for {} to: {} ('{}')", user_id_str, status, msg_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             let presence = match status {
                 0 => matrix_sdk::ruma::presence::PresenceState::Offline,
                 1 => matrix_sdk::ruma::presence::PresenceState::Online,
                 2 => matrix_sdk::ruma::presence::PresenceState::Unavailable,
                 _ => matrix_sdk::ruma::presence::PresenceState::Online,
             };
             
             // Presence
             use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request as PresenceRequest;
             use matrix_sdk::ruma::UserId;

             if let Ok(uid) = <&UserId>::try_from(user_id_str.as_str()) {
                 let mut request = PresenceRequest::new(uid.to_owned(), presence.clone());
                 request.status_msg = Some(msg_str);
                 
                 if let Err(e) = client.send(request).await {
                     log::error!("Failed to set presence: {:?}", e);
                 } else {
                     log::info!("Set presence to {:?}", presence);
                 }
             } else {
                 log::error!("Invalid user_id for presence: {}", user_id_str);
             }
                 // ok
        });
    });
}

// Ensure the previous function ended correctly.




#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_display_name(user_id: *const c_char, name: *const c_char) {
    if user_id.is_null() || name.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let name_str = unsafe { CStr::from_ptr(name).to_string_lossy().into_owned() };
    log::info!("Setting display name for {} to: {}", user_id_str, name_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             if let Err(e) = client.account().set_display_name(Some(&name_str)).await {
                 log::error!("Failed to set display name: {:?}", e);
             } else {
                 log::info!("Display name set successfully");
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_avatar(user_id: *const c_char, path: *const c_char) {
    if user_id.is_null() || path.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    log::info!("Setting avatar for {} from file: {}", user_id_str, path_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            let path_buf = std::path::PathBuf::from(&path_str);
            if !path_buf.exists() {
                log::error!("Avatar file not found: {}", path_str);
                return;
            }

            let mime = mime_guess::from_path(&path_buf).first_or_octet_stream();
            // Read file content
            let data = match std::fs::read(&path_buf) {
                Ok(d) => d,
                Err(e) => {
                    log::error!("Failed to read avatar file: {:?}", e);
                    return;
                }
            };

            // Upload to media repo first
            let response = match client.media().upload(&mime, data, None).await {
                 Ok(r) => r,
                 Err(e) => {
                     log::error!("Failed to upload avatar image: {:?}", e);
                     return;
                 }
            };

            // Set the avatar URL
            if let Err(e) = client.account().set_avatar_url(Some(&response.content_uri)).await {
                log::error!("Failed to set avatar URL: {:?}", e);
            } else {
                log::info!("Avatar set successfully");
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bootstrap_cross_signing(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    log::info!("Requesting Cross-Signing Bootstrap for {}...", user_id_str);
    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            let encryption = client.encryption();
            match encryption.bootstrap_cross_signing(None).await {
                Ok(_) => log::info!("Cross-signing bootstrapped successfully!"),
                Err(e) => log::error!("Failed to bootstrap cross-signing: {:?}", e),
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_recover_keys(user_id: *const c_char, passphrase: *const c_char) {
    if user_id.is_null() || passphrase.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let pass_str = unsafe { CStr::from_ptr(passphrase).to_string_lossy().into_owned() };
    log::info!("Attempting to recover keys for {}...", user_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             // The SDK documentation suggests recover() accepts a passphrase or key.
             match client.encryption().recovery().recover(&pass_str).await {
                 Ok(_) => log::info!("Secrets recovered successfully!"),
                 Err(e) => log::error!("Failed to recover secrets: {:?}", e),
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_verify_user(user_id: *const c_char, target_user_id: *const c_char) {
    if user_id.is_null() || target_user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    log::info!("Requesting verification from {} for user: {}", user_id_str, target_user_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::UserId;
             if let Ok(uid) = <&UserId>::try_from(user_id_str.as_str()) {
                 match client.encryption().get_user_identity(uid).await {
                     Ok(Some(identity)) => {
                         if let Err(e) = identity.request_verification().await {
                             log::error!("Failed to request verification from {}: {:?}", user_id_str, e);
                         } else {
                             log::info!("Verification request sent to {}", user_id_str);
                         }
                     },
                     Ok(None) => log::warn!("User identity not found for {}. Please ensure you share a room or have fetched keys.", user_id_str),
                     Err(e) => log::error!("Failed to get user identity: {:?}", e),
                 }
             } else {
                 log::error!("Invalid user ID: {}", user_id_str);
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_e2ee_status(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            let encryption = client.encryption();
            let cross_signing_status = encryption.cross_signing_status().await;
            
            let status_msg = format!(
                "Cross-Signing Status: {:?}\nDevice ID: {}\n", 
                cross_signing_status,
                client.device_id().map(|d| d.as_str()).unwrap_or("UNKNOWN")
            );
            
            log::info!("{}", status_msg);
            
            // Send back to chat as a system message (simulated via incoming message from "System")
            // In a real plugin, we might have a specific callback for printed info.
            // For now, let's just log it or maybe send a "notice" to the user effectively.
            // We can re-use the MSG_CALLBACK to print it locally if we treat it as a special system sender.
            
             let c_sender = CString::new("System").unwrap();
             let c_body = CString::new(status_msg.replace('\0', "")).unwrap_or(CString::new("Error: Invalid status message").unwrap());
             let c_room_id = CString::new(room_id_str).unwrap();

             let guard = MSG_CALLBACK.lock().unwrap();
             let mut timestamp: u64 = 0;
             if let Ok(n) = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
                   timestamp = n.as_millis() as u64;
             }
             if let Some(cb) = *guard {
                 cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
             }
        });
    });
}


#[no_mangle]
pub extern "C" fn purple_matrix_rust_change_password(user_id: *const c_char, old_pw: *const c_char, new_pw: *const c_char) {
    if user_id.is_null() || old_pw.is_null() || new_pw.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let old_pw_str = unsafe { CStr::from_ptr(old_pw).to_string_lossy().into_owned() };
    let new_pw_str = unsafe { CStr::from_ptr(new_pw).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            log::info!("Requesting password change...");
            
            // Construct AuthData for Password if we have the old one.
            // We need UserIdentifier.
            
            use matrix_sdk::ruma::api::client::uiaa::{AuthData, Password, UserIdentifier};
            
            let auth_data = if let Some(user_id) = client.user_id() {
                 Some(AuthData::Password(Password::new(
                     UserIdentifier::UserIdOrLocalpart(user_id.to_string()),
                     old_pw_str
                 )))
            } else {
                 None
            };

            if let Err(e) = client.account().change_password(&new_pw_str, auth_data).await {
                log::error!("Failed to change password: {:?}", e);
            } else {
                log::info!("Password changed successfully.");
            }
        });
    });
}

// ... (add_buddy, remove_buddy, set_idle remain same, I will include them to match context or assume replace block works)
// Note: replace tool needs unique context. I will replace the whole block I added previously.

#[no_mangle]
pub extern "C" fn purple_matrix_rust_add_buddy(account_user_id: *const c_char, buddy_user_id: *const c_char) {
    if account_user_id.is_null() || buddy_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let buddy_user_id_str = unsafe { CStr::from_ptr(buddy_user_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(buddy_user_id_str.as_str()) {
                log::info!("Adding buddy (Creating DM) for {}", buddy_user_id_str);
                if let Err(e) = client.create_dm(uid).await {
                    log::error!("Failed to create DM for added buddy {}: {:?}", buddy_user_id_str, e);
                } else {
                    log::info!("DM created/ensured for buddy {}", buddy_user_id_str);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_remove_buddy(account_user_id: *const c_char, buddy_user_id: *const c_char) {
    if account_user_id.is_null() || buddy_user_id.is_null() { return; }
    let _account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let buddy_user_id_str = unsafe { CStr::from_ptr(buddy_user_id).to_string_lossy().into_owned() };
    log::info!("Removed buddy from local list: {}", buddy_user_id_str);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_idle(user_id: *const c_char, seconds: i32) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::presence::PresenceState;
            use matrix_sdk::ruma::api::client::presence::set_presence::v3::Request as PresenceRequest;
            
            if let Some(uid) = client.user_id() {
                 let presence = if seconds > 0 { PresenceState::Unavailable } else { PresenceState::Online };
                 let request = PresenceRequest::new(uid.to_owned(), presence);
                 if let Err(e) = client.send(request).await {
                      log::error!("Failed to set idle presence: {:?}", e);
                 }
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

// Callback for Room List population
pub type RoomListAddCallback = extern "C" fn(*const c_char, *const c_char, *const c_char, u64); // name, id, topic, user_count
pub static ROOMLIST_ADD_CALLBACK: Lazy<Mutex<Option<RoomListAddCallback>>> = Lazy::new(|| Mutex::new(None));

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_roomlist_add_callback(cb: RoomListAddCallback) {
    let mut guard = ROOMLIST_ADD_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

// Callback for Thread Listing
// room_id, thread_root_id, topic/snippet, count, timestamp (0 timestamp = end of list)
pub type ThreadListCallback = extern "C" fn(*const c_char, *const c_char, *const c_char, u64, u64);
pub static THREAD_LIST_CALLBACK: Lazy<Mutex<Option<ThreadListCallback>>> = Lazy::new(|| Mutex::new(None));

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_thread_list_callback(cb: ThreadListCallback) {
    let mut guard = THREAD_LIST_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

// Callback for Poll Listing
// room_id, event_id, sender, question, options_json (Array of strings)
pub type PollListCallback = extern "C" fn(*const c_char, *const c_char, *const c_char, *const c_char, *const c_char);
pub static POLL_LIST_CALLBACK: Lazy<Mutex<Option<PollListCallback>>> = Lazy::new(|| Mutex::new(None));

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_poll_list_callback(cb: PollListCallback) {
    let mut guard = POLL_LIST_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_active_polls(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            log::info!("Scanning for active polls in room: {}", room_id_str);
            if let Ok(room_id_owned) = <matrix_sdk::ruma::OwnedRoomId>::try_from(room_id_str.as_str()) {
                let room_id_ref: &matrix_sdk::ruma::RoomId = &room_id_owned;
                if let Some(room) = matrix_sdk::Client::get_room(&client, room_id_ref) {
                    use matrix_sdk::room::MessagesOptions;
                    use matrix_sdk::ruma::events::{AnySyncTimelineEvent, AnySyncMessageLikeEvent, SyncMessageLikeEvent};
                    
                    use matrix_sdk::ruma::UInt;
                    let mut options = MessagesOptions::backward();
                    options.limit = UInt::new(50).unwrap();
                    if let Ok(messages) = room.messages(options).await {
                       let c_room_id = CString::new(room_id_str.clone()).unwrap_or_default();
                       let guard = POLL_LIST_CALLBACK.lock().unwrap();
                       
                       if let Some(cb) = *guard {
                           for event in messages.chunk {
                               if let Ok(any_ev) = event.raw().deserialize() {
                                   if let AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::PollStart(poll_ev)) = any_ev {
                                        if let SyncMessageLikeEvent::Original(orig) = poll_ev {
                                            let event_id = orig.event_id.to_string();
                                            let sender = orig.sender.to_string();
                                            let content = orig.content;
                                            
                                            // Extract Question and Answers
                                            // Handling different Poll types (New struct or old dict?)
                                            // Checking ruma docs for 0.8+: content.poll_start.question.text
                                            // content.poll_start.answers (Vec<PollAnswer>)
                                            
                                            let question = format!("{:?}", content.poll.question.text);
                                            let answers: Vec<String> = content.poll.answers.iter().map(|a| {
                                                format!("{}^{}", a.id, format!("{:?}", a.text))
                                            }).collect();
                                            let options_str = answers.join("|");
                                            
                                            let c_event_id = CString::new(event_id).unwrap_or_default();
                                            let c_sender = CString::new(sender).unwrap_or_default();
                                            let c_question = CString::new(question).unwrap_or_default();
                                            let c_options = CString::new(options_str).unwrap_or_default();
                                            
                                            cb(c_room_id.as_ptr(), c_event_id.as_ptr(), c_sender.as_ptr(), c_question.as_ptr(), c_options.as_ptr());
                                        }
                                   }
                               }
                           }
                           // Sentinel
                           cb(c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), std::ptr::null(), std::ptr::null());
                       }
                    }
                }
            }
        });
    });
}


#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_public_rooms_for_list(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    log::info!("Fetching public rooms for Room List (Account: {})...", user_id_str);
    
    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::api::client::directory::get_public_rooms_filtered::v3::Request as PublicRoomsRequest;
            let mut request = PublicRoomsRequest::new();
            request.limit = Some(50u32.into()); // Fetch reasonable batch

            match client.send(request).await {
                Ok(response) => {
                    for room in response.chunk {
                        let name = room.name.as_deref().unwrap_or("Unnamed");
                        let room_id = room.room_id.as_str();
                        let topic = room.topic.as_deref().unwrap_or("");
                        let count = room.num_joined_members.into();

                        let c_name = CString::new(name).unwrap_or_default();
                        let c_id = CString::new(room_id).unwrap_or_default();
                        let c_topic = CString::new(topic).unwrap_or_default();

                        let guard = ROOMLIST_ADD_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_name.as_ptr(), c_id.as_ptr(), c_topic.as_ptr(), count);
                        }
                    }
                },
                Err(e) => log::error!("Failed to fetch public rooms: {:?}", e),
            }
        });
    });
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

// Helper to create message content with optional HTML support
fn create_message_content(text: String) -> matrix_sdk::ruma::events::room::message::RoomMessageEventContent {
    use matrix_sdk::ruma::events::room::message::RoomMessageEventContent;
    
    // Check for HTML
    let is_html = text.contains('<') && text.contains('>'); // Simple heuristic for now
    
    if is_html {
        // Pidgin usually sends mostly clean HTML
        let plain_body = strip_html_tags(&text);
        RoomMessageEventContent::text_html(plain_body, text)
    } else {
        RoomMessageEventContent::text_plain(text)
    }
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_message(user_id: *const c_char, room_id: *const c_char, text: *const c_char) {
    if user_id.is_null() || room_id.is_null() || text.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let text = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{events::room::message::Relation, RoomId, EventId};

            // Parse ID for Thread Support: "room_id|thread_root_id"
            let (room_id_str, thread_root_id_opt) = match id_str.split_once('|') {
                Some((r, t)) if !t.is_empty() => (r, Some(t)),
                _ => (id_str.as_str(), None),
            };

            if let Ok(room_id_ruma) = <&RoomId>::try_from(room_id_str) {
                if let Some(room) = client.get_room(room_id_ruma) {
                    // Implicit Read Receipt
                     if let Some(latest_event) = room.latest_event() {
                         use matrix_sdk::ruma::events::receipt::ReceiptThread;
                         use matrix_sdk::ruma::api::client::receipt::create_receipt::v3::ReceiptType;
                         
                         if let Some(event_id) = latest_event.event_id() {
                             let event_id = event_id.to_owned();
                             log::info!("Marking room {} as read up to {}", room_id_str, event_id);
                             if let Err(e) = room.send_single_receipt(ReceiptType::Read, ReceiptThread::Unthreaded, event_id).await {
                                 log::warn!("Failed to send read receipt: {:?}", e);
                             }
                         }
                     }
                    
                    let mut content = create_message_content(text);
                    
                    // Attach Thread Relation if present
                    if let Some(thread_id_str) = thread_root_id_opt {
                        if let Ok(root_id) = <&EventId>::try_from(thread_id_str) {
                             log::info!("Sending message to thread {} in room {}", thread_id_str, room_id_str);
                             // Create a thread relation. 
                             // Note: SDK might have helper like content.make_for_thread() or similar, 
                             // but doing it manually via Relation::Thread is standard.
                             // Actually, Ruma's RoomMessageEventContent has `relates_to`.
                             // Thread::plain might be in relation module.
                             content.relates_to = Some(Relation::Thread(matrix_sdk::ruma::events::relation::Thread::plain(root_id.to_owned(), root_id.to_owned())));
                        } else {
                            log::warn!("Invalid Event ID for thread root: {}", thread_id_str);
                        }
                    }

                    if let Err(e) = room.send(content).await {
                        log::error!("Failed to send message to room {}: {:?}", room_id_str, e);
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
                         
                         let content = create_message_content(text);
                         if let Err(e) = room.send(content).await {
                             log::error!("Failed to send DM message: {:?}", e);
                         } else {
                             log::info!("DM Sent!");
                         }
                    },
                    Err(e) => {
                         log::error!("Failed to get or create DM with {}: {:?}", target_user_id_str, e);
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

// Get User Info
#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_user_info(account_user_id: *const c_char, user_id: *const c_char) {
    if account_user_id.is_null() || user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    
    log::info!("Fetching user info for: {} (on account {})", user_id_str, account_user_id_str);

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            
            if let Ok(uid) = <&UserId>::try_from(user_id_str.as_str()) {
                match client.account().fetch_user_profile_of(uid).await {
                    Ok(profile) => {
                        let display_name = profile.get("displayname")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string())
                            .unwrap_or_else(|| user_id_str.clone());
                        let avatar_url = profile.get("avatar_url")
                            .and_then(|v| v.as_str())
                            .map(|s| s.to_string())
                            .unwrap_or_default();
                        
                        let is_online = false; 

                        let c_user_id = CString::new(user_id_str).unwrap_or_default();
                        let c_display_name = CString::new(display_name).unwrap_or_default();
                        let c_avatar_url = CString::new(avatar_url).unwrap_or_default();
                        
                        let guard = SHOW_USER_INFO_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_display_name.as_ptr(), c_avatar_url.as_ptr(), is_online);
                        }
                    },
                    Err(e) => {
                        log::error!("Failed to fetch profile for {}: {:?}", user_id_str, e);
                        // Fallback
                        let c_user_id = CString::new(user_id_str.clone()).unwrap_or_default();
                        let c_display_name = CString::new(user_id_str).unwrap_or_default();
                        let c_avatar_url = CString::new("").unwrap_or_default();
                        let guard = SHOW_USER_INFO_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_user_id.as_ptr(), c_display_name.as_ptr(), c_avatar_url.as_ptr(), false);
                        }
                    }
                }
            }
        });
    });
}
// Helper function to render HTML/Markdown
pub fn get_display_html(content: &matrix_sdk::ruma::events::room::message::RoomMessageEventContent) -> String {
    use matrix_sdk::ruma::events::room::message::MessageType;
    
    match &content.msgtype {
        MessageType::Text(text_content) => {
             // Check if HTML is present
             if let Some(formatted) = &text_content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     return sanitize_untrusted_html(&formatted.body);
                 }
             }
             // Treat plain text as untrusted and escape.
             escape_html(&text_content.body)
        },
        MessageType::Emote(content) => {
             let body_safe = if let Some(formatted) = &content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     sanitize_untrusted_html(&formatted.body)
                 } else {
                     escape_html(&content.body)
                 }
             } else {
                 escape_html(&content.body)
             };
             format!("* {}", body_safe)
        },
        MessageType::Notice(content) => {
             if let Some(formatted) = &content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     return sanitize_untrusted_html(&formatted.body);
                 }
             }
             escape_html(&content.body)
        },
        MessageType::Image(image_content) => format!("[Image: {}]", escape_html(&image_content.body)),
        MessageType::Video(video_content) => format!("[Video: {}]", escape_html(&video_content.body)),
        MessageType::Audio(audio_content) => format!("[Audio: {}]", escape_html(&audio_content.body)),
        MessageType::File(file_content) => format!("[File: {}]", escape_html(&file_content.body)),
        MessageType::Location(location_content) => format!(
            "[Location: {}] ({})",
            escape_html(&location_content.body),
            escape_html(location_content.geo_uri.as_str())
        ),
        _ => "[Unsupported Message Type]".to_string(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use matrix_sdk::ruma::events::room::message::RoomMessageEventContent;

    #[test]
    fn test_format_text_message() {
        let content = RoomMessageEventContent::text_plain("Hello World");
        assert_eq!(get_display_html(&content), "Hello World");
    }

    #[test]
    fn test_format_markdown() {
        let content = RoomMessageEventContent::text_plain("**Bold** and *Italic*");
        let html = get_display_html(&content);
        assert_eq!(html, "**Bold** and *Italic*");
    }
    
    #[test]
    fn test_formatted_html_override() {
        let content = RoomMessageEventContent::text_html("Plain", "<b>Bold</b>");
        assert_eq!(get_display_html(&content), "Bold");
    }

    #[test]
    fn test_formatted_html_sanitizes_script_payload() {
        let content = RoomMessageEventContent::text_html(
            "x",
            "<img src=x onerror=alert(1)><script>alert(2)</script><b>ok</b>",
        );
        assert_eq!(get_display_html(&content), "alert(2)ok");
    }

    #[test]
    fn test_location_rendering() {
        use matrix_sdk::ruma::events::room::message::LocationMessageEventContent;
        let content = RoomMessageEventContent::new(
            matrix_sdk::ruma::events::room::message::MessageType::Location(
                LocationMessageEventContent::new("Meeting Spot".to_string(), "geo:12.34,56.78".to_string())
            )
        );
        let html = get_display_html(&content);
        assert!(html.contains("Meeting Spot"));
        assert!(html.contains("geo:12.34,56.78"));
    }
}

fn strip_html_tags(input: &str) -> String {
    let mut output = String::new();
    let mut inside_tag = false;

    // This is a naive implementation. For production, consider using a proper library like `ammonia` or `scraper` if dependencies allowed.
    // For now, simple state machine to strip <...>
    for c in input.chars() {
        if c == '<' {
            inside_tag = true;
        } else if c == '>' {
            inside_tag = false;
        } else if !inside_tag {
            output.push(c);
        }
    }
    
    // Decode common entities
    output = output.replace("&lt;", "<")
                   .replace("&gt;", ">")
                   .replace("&amp;", "&")
                   .replace("&quot;", "\"")
                   .replace("&nbsp;", " ");
                   
    output
}

fn sanitize_untrusted_html(input: &str) -> String {
    // Conservative strategy: strip all tags, then escape.
    escape_html(&strip_html_tags(input))
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
                 // client.get_or_create_dm_room is usually what we want.
                 // In some SDK versions it's get_dm_room or create_dm.
                 // Checking recent SDK: client.get_or_create_dm() might exist.
                 // Recent SDKs: `client.get_dm_room(user_id)` or `client.create_dm(user_id)`.
                 // Let's rely on what `send_im` used? `send_im` used `client.get_room` (it assumed room_id was passed).
                 // Wait, `matrix_send_im` in `plugin.c` says: if who[0] != '!', call `purple_matrix_rust_send_im`.
                 // Let's check `purple_matrix_rust_send_im` implementation in `src/lib.rs`.
             } else {
                 None
             };

             if let Some(room) = room_opt {
                 let path = Path::new(&filename_str);
                 if !path.exists() {
                     log::error!("File does not exist: {}", filename_str);
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
pub extern "C" fn purple_matrix_rust_accept_sas(account_user_id: *const c_char, user_id: *const c_char, flow_id: *const c_char) {
    if account_user_id.is_null() || user_id.is_null() || flow_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let flow_id_str = unsafe { CStr::from_ptr(flow_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(user_id) = <&UserId>::try_from(user_id_str.as_str()) {
                if let Some(request) = client.encryption().get_verification_request(user_id, &flow_id_str).await {
                    log::info!("Accepting SAS verification request from {}", user_id_str);
                    if let Err(e) = request.accept().await {
                        log::error!("Failed to accept verification request: {:?}", e);
                    } else {
                        // After accepting, we need to transition to SAS flow.
                        // However, `handle_verification_request` loop observes changes, so it should pick up the transition!
                        log::info!("Verification accepted, waiting for keys exchange...");
                    }
                } else {
                    log::error!("Could not find verification request {} from {}", flow_id_str, user_id_str);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_confirm_sas(account_user_id: *const c_char, user_id: *const c_char, flow_id: *const c_char, is_match: bool) {
    if account_user_id.is_null() || user_id.is_null() || flow_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let flow_id_str = unsafe { CStr::from_ptr(flow_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(user_id) = <&UserId>::try_from(user_id_str.as_str()) {
                // We need to get the SAS object. The Request object might have transitioned.
                // client.encryption().get_verification(user, flow) returns the active verification.
                if let Some(verification) = client.encryption().get_verification(user_id, &flow_id_str).await {
                    if let Some(sas) = verification.sas() {
                        if is_match {
                            log::info!("Confirming SAS match for {}", user_id_str);
                            if let Err(e) = sas.confirm().await {
                                log::error!("Failed to confirm SAS: {:?}", e);
                            }
                        } else {
                            log::warn!("Reporting SAS mismatch for {}", user_id_str);
                            if let Err(e) = sas.mismatch().await {
                                log::error!("Failed to cancel SAS: {:?}", e);
                            }
                        }
                    } else {
                        log::error!("Verification exists but is not SAS for {}", flow_id_str);
                    }
                } else {
                     log::error!("Could not find active verification {} for {}", flow_id_str, user_id_str);
                }
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
                    let mut content = create_message_content(text_str);
                    
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
                     let new_content = create_message_content(text_str.clone());
                     let mut content = create_message_content(format!("* {}", text_str)); 
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
pub extern "C" fn purple_matrix_rust_list_threads(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    
    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            log::info!("Scanning for threads in room: {}", room_id_str);
            if let Ok(room_id_owned) = <matrix_sdk::ruma::OwnedRoomId>::try_from(room_id_str.as_str()) {
                let room_id_ref: &matrix_sdk::ruma::RoomId = &room_id_owned;
                if let Some(room) = matrix_sdk::Client::get_room(&client, room_id_ref) {
                    // Fetch recent messages to find threads
                    // We will scan the last 50 messages for now
                    use matrix_sdk::room::MessagesOptions;
                    use matrix_sdk::ruma::events::{AnySyncMessageLikeEvent, AnySyncTimelineEvent, SyncMessageLikeEvent};
                    use matrix_sdk::ruma::events::room::message::Relation;
                    use matrix_sdk::ruma::UInt;
                    let mut options = MessagesOptions::backward();
                    options.limit = UInt::new(50).unwrap();
                    
                    // We need to accumulate threads: Map<RootID, (LatestMsg, Count, TS)>
                    // This is a naive scan. Ideally we'd use the /threads API if available.
                    let mut threads: std::collections::HashMap<String, (String, u64, u64)> = std::collections::HashMap::new();

                    if let Ok(messages) = room.messages(options).await {
                        for event in messages.chunk {
                            if let Ok(any_ev) = event.raw().deserialize() {
                                if let AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(msg_ev)) = any_ev {
                                    if let SyncMessageLikeEvent::Original(orig) = msg_ev {
                                        if let Some(Relation::Thread(thread)) = &orig.content.relates_to {
                                            let root_id = thread.event_id.to_string();
                                            let body = crate::handlers::messages::render_room_message(&orig, &room).await;
                                            let ts: u64 = orig.origin_server_ts.0.into();

                                            if let Some((latest_body, count, latest_ts)) = threads.get_mut(&root_id) {
                                                *count += 1;
                                                if ts >= *latest_ts {
                                                    *latest_body = body;
                                                    *latest_ts = ts;
                                                }
                                            } else {
                                                threads.insert(root_id, (body, 1, ts));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Send results to C
                    let c_room_id = CString::new(room_id_str.clone()).unwrap_or_default();
                    let guard = THREAD_LIST_CALLBACK.lock().unwrap();
                    if let Some(cb) = *guard {
                        let mut items: Vec<(String, (String, u64, u64))> = threads.into_iter().collect();
                        items.sort_by(|a, b| b.1.2.cmp(&a.1.2));
                        for (root_id, (body, count, ts)) in items {
                            let c_root = CString::new(root_id).unwrap_or_default();
                            // Sanitize body to prevent null byte crashes
                            let safe_body = body.replace('\0', "");
                            let c_body = CString::new(safe_body).unwrap_or_default();
                            
                            cb(c_room_id.as_ptr(), c_root.as_ptr(), c_body.as_ptr(), count, ts);
                        }
                        // Sentinel: NULL/0 to indicate end
                        cb(c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), 0, 0);
                    }
                }
            } else {
                log::error!("Invalid Room ID for listing threads: {}", room_id_str);
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
pub extern "C" fn purple_matrix_rust_search_public_rooms(user_id: *const c_char, search_term: *const c_char, output_room_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    
    let term = if search_term.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(search_term).to_string_lossy().into_owned() })
    };
    
    let output_rid = if output_room_id.is_null() {
        return;
    } else {
        unsafe { CStr::from_ptr(output_room_id).to_string_lossy().into_owned() }
    };

    log::info!("Searching public rooms with term: {:?} (Account: {})", term, user_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::api::client::directory::get_public_rooms_filtered::v3::Request as PublicRoomsRequest;

            let mut request = PublicRoomsRequest::new();
            if let Some(t) = term {
                request.filter.generic_search_term = Some(t);
            }
            
            match client.send(request).await {
                Ok(response) => {
                    let mut result_msg = format!("Found {} public rooms:\n", response.chunk.len());
                    for room in response.chunk.iter().take(10) {
                        result_msg.push_str(&format!("- {} ({}): {}\n", 
                            room.name.as_deref().unwrap_or("Unnamed"),
                            room.room_id,
                            room.topic.as_deref().unwrap_or("No topic")));
                    }
                    if response.chunk.len() > 10 {
                        result_msg.push_str("... (truncated)");
                    }
                    
                    // Send results back to the chat as a system message
                    let c_sender = CString::new("System").unwrap();
                    let c_body = CString::new(result_msg.replace('\0', "")).unwrap_or(CString::new("Error: Invalid search results").unwrap());
                    let c_room_id = CString::new(output_rid).unwrap();

                    let guard = MSG_CALLBACK.lock().unwrap();
                    let mut timestamp: u64 = 0;
                    if let Ok(n) = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
                        timestamp = n.as_millis() as u64;
                    }
                    if let Some(cb) = *guard {
                        cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
                    }
                },
                Err(e) => log::error!("Failed to search public rooms: {:?}", e),
            }
        });
    });
}
#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_users(user_id: *const c_char, search_term: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || search_term.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let term = unsafe { CStr::from_ptr(search_term).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    log::info!("Searching users with term: {} (Account: {})", term, user_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::api::client::user_directory::search_users::v3::Request as UserSearchRequest;
            let mut request = UserSearchRequest::new(term.clone());
            request.limit = 10u32.into();

            match client.send(request).await {
                Ok(response) => {
                    let mut result_msg = format!("Found {} users for '{}':\n", response.results.len(), term);
                    for user in response.results.iter().take(10) {
                        result_msg.push_str(&format!("- {} ({})\n", 
                            user.display_name.as_deref().unwrap_or("No Name"),
                            user.user_id));
                    }
                    
                    let c_sender = CString::new("System").unwrap();
                    let c_body = CString::new(result_msg.replace('\0', "")).unwrap_or(CString::new("Error: Invalid search results").unwrap());
                    let c_room_id = CString::new(room_id_str).unwrap();

                    let guard = MSG_CALLBACK.lock().unwrap();
                    let mut timestamp: u64 = 0;
                    if let Ok(n) = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH) {
                        timestamp = n.as_millis() as u64;
                    }
                    if let Some(cb) = *guard {
                        cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp);
                    }
                },
                Err(e) => log::error!("Failed to search users: {:?}", e),
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
    let reason_str = if reason.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() })
    };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
            if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Kicking user {} from room {}...", target_user_id_str, room_id_str);
                    if let Err(e) = room.kick_user(uid, reason_str.as_deref()).await {
                        log::error!("Failed to kick user: {:?}", e);
                    } else {
                        log::info!("User kicked successfully");
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
    let reason_str = if reason.is_null() {
        None
    } else {
        Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() })
    };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
            if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Banning user {} from room {}...", target_user_id_str, room_id_str);
                    if let Err(e) = room.ban_user(uid, reason_str.as_deref()).await {
                        log::error!("Failed to ban user: {:?}", e);
                    } else {
                        log::info!("User banned successfully");
                    }
            }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_tag(user_id: *const c_char, room_id: *const c_char, tag_name: *const c_char) {
    if user_id.is_null() || room_id.is_null() || tag_name.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let tag_name_str = unsafe { CStr::from_ptr(tag_name).to_string_lossy().into_owned() };

    log::info!("Setting tag '{}' for room {} (Account: {})...", tag_name_str, room_id_str, user_id_str);

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, events::tag::{TagName, TagInfo}};
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    let tag = match tag_name_str.as_bytes() {
                        b"favorite" => Some(TagName::Favorite),
                        b"lowpriority" => Some(TagName::LowPriority),
                        _ => None,
                    };

                    if let Some(t) = tag {
                        if let Err(e) = room.set_tag(t, TagInfo::new()).await {
                            log::error!("Failed to set tag: {:?}", e);
                        } else {
                            log::info!("Tag set successfully");
                        }
                    } else if tag_name_str == "none" {
                        // Remove common tags
                        let _ = room.remove_tag(TagName::Favorite).await;
                        let _ = room.remove_tag(TagName::LowPriority).await;
                        log::info!("Tags removed");
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_ignore_user(account_user_id: *const c_char, target_user_id: *const c_char) {
    if account_user_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

    log::info!("Ignoring user {} (Account: {})...", target_user_id_str, account_user_id_str);

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                if let Err(e) = client.account().ignore_user(uid).await {
                    log::error!("Failed to ignore user: {:?}", e);
                } else {
                    log::info!("User ignored successfully");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unignore_user(account_user_id: *const c_char, target_user_id: *const c_char) {
    if account_user_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

    log::info!("Unignoring user {} (Account: {})...", target_user_id_str, account_user_id_str);

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::UserId;
            if let Ok(uid) = <&UserId>::try_from(target_user_id_str.as_str()) {
                if let Err(e) = client.account().unignore_user(uid).await {
                    log::error!("Failed to unignore user: {:?}", e);
                } else {
                    log::info!("User unignored successfully");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_avatar_bytes(user_id: *const c_char, data: *const u8, len: usize) {
    if user_id.is_null() || data.is_null() || len == 0 { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    
    // Copy bytes immediately because pointer might not be valid in async task
    let bytes = unsafe { std::slice::from_raw_parts(data, len).to_vec() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            log::info!("Setting avatar from bytes ({} bytes)...", len);
            
            use mime_guess::mime;
            // Guess mime type from magic bytes if possible, or default to png/jpeg
            // Simple check: PNG starts with 89 50 4E 47, JPEG with FF D8
            let mime_type = if bytes.starts_with(&[0x89, 0x50, 0x4E, 0x47]) {
                mime::IMAGE_PNG
            } else if bytes.starts_with(&[0xFF, 0xD8]) {
                mime::IMAGE_JPEG
            } else {
                mime::APPLICATION_OCTET_STREAM 
            };

            match client.media().upload(&mime_type, bytes, None).await {
                 Ok(response) => {
                     if let Err(e) = client.account().set_avatar_url(Some(&response.content_uri)).await {
                        log::error!("Failed to set avatar URL: {:?}", e);
                     } else {
                        log::info!("Avatar set successfully from bytes");
                     }
                 },
                 Err(e) => log::error!("Failed to upload avatar bytes: {:?}", e),
            };
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_power_levels(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    if let Ok(power_levels) = room.power_levels().await {
                        let mut result_msg = "Current Power Levels:\n".to_string();
                        result_msg.push_str(&format!("- Users Default: {}\n", power_levels.users_default));
                        result_msg.push_str(&format!("- Events Default: {}\n", power_levels.events_default));
                        result_msg.push_str(&format!("- State Default: {}\n", power_levels.state_default));
                        result_msg.push_str("- Specific Users:\n");
                        for (user_id, level) in &power_levels.users {
                            result_msg.push_str(&format!("  - {}: {}\n", user_id, level));
                        }

                        let c_sender = CString::new("System").unwrap();
                        let c_body = CString::new(result_msg).unwrap();
                        let c_room_id = CString::new(room_id_str).unwrap();

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
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_power_level(account_user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, level: i64) {
    if account_user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
    let account_user_id_str = unsafe { CStr::from_ptr(account_user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };

    with_client(&account_user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId, Int};
            if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    if let Ok(p_level) = Int::try_from(level) {
                        log::info!("Setting power level for {} to {} in room {}...", target_user_id_str, level, room_id_str);
                        if let Err(e) = room.update_power_levels(vec![(uid, p_level)]).await {
                            log::error!("Failed to update power level: {:?}", e);
                        } else {
                            log::info!("Power level updated successfully");
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
                    log::info!("Setting room name to '{}' for {}...", name_str, room_id_str);
                    if let Err(e) = room.set_name(name_str).await {
                        log::error!("Failed to set room name: {:?}", e);
                    } else {
                        log::info!("Room name set successfully");
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
                    log::info!("Setting room topic for {}...", room_id_str);
                    if let Err(e) = room.set_room_topic(&topic_str).await {
                        log::error!("Failed to set room topic: {:?}", e);
                    } else {
                        log::info!("Room topic set successfully");
                    }
            }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_alias(user_id: *const c_char, room_id: *const c_char, alias: *const c_char) {
    if user_id.is_null() || room_id.is_null() || alias.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let alias_str = unsafe { CStr::from_ptr(alias).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, RoomAliasId, api::client::alias::create_alias::v3::Request as CreateAliasRequest};
            if let (Ok(rid), Ok(alias_id)) = (<&RoomId>::try_from(room_id_str.as_str()), <&RoomAliasId>::try_from(alias_str.as_str())) {
                log::info!("Creating alias {} for room {}...", alias_str, room_id_str);
                let request = CreateAliasRequest::new(alias_id.to_owned(), rid.to_owned());
                if let Err(e) = client.send(request).await {
                    log::error!("Failed to create alias: {:?}", e);
                } else {
                    log::info!("Alias created successfully");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_delete_alias(user_id: *const c_char, alias: *const c_char) {
    if user_id.is_null() || alias.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let alias_str = unsafe { CStr::from_ptr(alias).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomAliasId, api::client::alias::delete_alias::v3::Request as DeleteAliasRequest};
            if let Ok(alias_id) = <&RoomAliasId>::try_from(alias_str.as_str()) {
                log::info!("Deleting alias {}...", alias_str);
                let request = DeleteAliasRequest::new(alias_id.to_owned());
                let result = client.send(request).await;
                if let Err(e) = result {
                    log::error!("Failed to delete alias: {:?}", e);
                } else {
                    log::info!("Alias deleted successfully");
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

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};
            if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(event_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Reporting event {} in room {}...", event_id_str, room_id_str);
                    if let Err(e) = room.report_content(eid.to_owned(), None, reason_str).await {
                        log::error!("Failed to report content: {:?}", e);
                    } else {
                        log::info!("Content reported successfully");
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_export_room_keys(user_id: *const c_char, path: *const c_char, passphrase: *const c_char) {
    if user_id.is_null() || path.is_null() || passphrase.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let path_str = unsafe { CStr::from_ptr(path).to_string_lossy().into_owned() };
    let passphrase_str = unsafe { CStr::from_ptr(passphrase).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            log::info!("Exporting room keys to {}...", path_str);
            let path_buf = std::path::PathBuf::from(path_str);
            if let Err(e) = client.encryption().export_room_keys(path_buf, &passphrase_str, |_| true).await {
                log::error!("Failed to export room keys: {:?}", e);
            } else {
                log::info!("Room keys exported successfully");
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_location(user_id: *const c_char, room_id: *const c_char, body: *const c_char, geo_uri: *const c_char) {
    if user_id.is_null() || room_id.is_null() || body.is_null() || geo_uri.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let body_str = unsafe { CStr::from_ptr(body).to_string_lossy().into_owned() };
    let geo_uri_str = unsafe { CStr::from_ptr(geo_uri).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::room::message::{RoomMessageEventContent, LocationMessageEventContent};

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Sending location to {}: {}", room_id_str, geo_uri_str);
                    let content = RoomMessageEventContent::new(
                        matrix_sdk::ruma::events::room::message::MessageType::Location(
                            LocationMessageEventContent::new(body_str, geo_uri_str)
                        )
                    );
                    if let Err(e) = room.send(content).await {
                        log::error!("Failed to send location: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_create(user_id: *const c_char, room_id: *const c_char, question: *const c_char, options: *const c_char) {
    if user_id.is_null() || room_id.is_null() || question.is_null() || options.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let question_str = unsafe { CStr::from_ptr(question).to_string_lossy().into_owned() };
    let options_str = unsafe { CStr::from_ptr(options).to_string_lossy().into_owned() };

    // Parse options (split by |)
    let answers: Vec<String> = options_str.split('|').map(|s| s.trim().to_string()).collect();

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, events::poll::start::PollStartEventContent, events::poll::start::PollAnswer};
            use matrix_sdk::ruma::events::poll::start::{PollContentBlock, PollAnswers};
            use matrix_sdk::ruma::events::message::TextContentBlock; // Correct import
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Creating poll in {}: {}", room_id_str, question_str);
                    
                    let mut poll_answers_vec = Vec::new();
                    for (i, ans) in answers.iter().enumerate() {
                        poll_answers_vec.push(PollAnswer::new(format!("opt_{}", i), TextContentBlock::plain(ans)));
                    }
                    
                    if let Ok(poll_answers) = PollAnswers::try_from(poll_answers_vec) {
                         let poll_content = PollStartEventContent::new(
                            TextContentBlock::plain(question_str), 
                            PollContentBlock::new(TextContentBlock::plain("Poll"), poll_answers)
                         );
                    
                         let content = AnyMessageLikeEventContent::PollStart(poll_content);

                         if let Err(e) = room.send(content).await {
                              log::error!("Failed to send poll: {:?}", e);
                         }
                    } else {
                         log::error!("Failed to create poll answers (invalid count?)");
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_vote(user_id: *const c_char, room_id: *const c_char, poll_event_id: *const c_char, _option_text: *const c_char, selection_index_str: *const c_char) {
    if user_id.is_null() || room_id.is_null() || poll_event_id.is_null() || selection_index_str.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let poll_event_id_str = unsafe { CStr::from_ptr(poll_event_id).to_string_lossy().into_owned() };
    let index_str = unsafe { CStr::from_ptr(selection_index_str).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId, events::poll::response::PollResponseEventContent, events::poll::response::SelectionsContentBlock};
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;

            if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_event_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    
                    let answer_id = index_str; // It is now the ID
                    
                    log::info!("Voting on poll {} in {}: {}", poll_event_id_str, room_id_str, answer_id);
    
                    // SelectionsContentBlock usually implements From<Vec<String>>
                    let selections: SelectionsContentBlock = vec![answer_id].into();
    
                    let response = PollResponseEventContent::new(
                        selections, 
                        eid.to_owned()
                    );
                    let content = AnyMessageLikeEventContent::PollResponse(response);
                    
                    if let Err(e) = room.send(content).await {
                            log::error!("Failed to vote on poll: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_end(user_id: *const c_char, room_id: *const c_char, poll_event_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() || poll_event_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let poll_event_id_str = unsafe { CStr::from_ptr(poll_event_id).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId, events::poll::end::PollEndEventContent};
            use matrix_sdk::ruma::events::message::TextContentBlock; // Correct import
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;

            if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_event_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Ending poll {} in {}", poll_event_id_str, room_id_str);
                    
                    let end_content = PollEndEventContent::new(
                        TextContentBlock::plain("Poll closed"), 
                        eid.to_owned()
                    );
                    let content = AnyMessageLikeEventContent::PollEnd(end_content);

                    if let Err(e) = room.send(content).await {
                         log::error!("Failed to end poll: {:?}", e);
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_bulk_redact(user_id: *const c_char, room_id: *const c_char, count: i32, reason: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::room::MessagesOptions;
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Bulk redacting {} events in room {}...", count, room_id_str);
                    use matrix_sdk::ruma::UInt;
                    let mut options = MessagesOptions::backward();
                    options.limit = UInt::new(count as u64).unwrap();

                    match room.messages(options).await {
                        Ok(messages) => {
                            for event in messages.chunk {
                                if let Some(event_id) = event.event_id() {
                                    if let Err(e) = room.redact(&event_id, reason_str.as_deref(), None).await {
                                        log::error!("Failed to redact event {}: {:?}", event_id.as_str(), e);
                                    }
                                }
                            }
                            log::info!("Bulk redaction complete");
                        },
                        Err(e) => log::error!("Failed to fetch messages for bulk redaction: {:?}", e),
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_knock(user_id: *const c_char, room_id_or_alias: *const c_char, reason: *const c_char) {
    if user_id.is_null() || room_id_or_alias.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let id_or_alias_str = unsafe { CStr::from_ptr(room_id_or_alias).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomOrAliasId;
            if let Ok(id_or_alias) = <&RoomOrAliasId>::try_from(id_or_alias_str.as_str()) {
                log::info!("Knocking on room {}...", id_or_alias_str);
                if let Err(e) = client.knock(id_or_alias.to_owned(), reason_str, vec![]).await {
                    log::error!("Failed to knock on room: {:?}", e);
                } else {
                    log::info!("Knock sent successfully");
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_unban_user(user_id: *const c_char, room_id: *const c_char, target_user_id: *const c_char, reason: *const c_char) {
    if user_id.is_null() || room_id.is_null() || target_user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let target_user_id_str = unsafe { CStr::from_ptr(target_user_id).to_string_lossy().into_owned() };
    let reason_str = if reason.is_null() { None } else { Some(unsafe { CStr::from_ptr(reason).to_string_lossy().into_owned() }) };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, UserId};
            if let (Ok(rid), Ok(uid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&UserId>::try_from(target_user_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    log::info!("Unbanning user {} in room {}...", target_user_id_str, room_id_str);
                    if let Err(e) = room.unban_user(uid, reason_str.as_deref()).await {
                        log::error!("Failed to unban user: {:?}", e);
                    } else {
                        log::info!("User unbanned successfully");
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_avatar(user_id: *const c_char, room_id: *const c_char, filename: *const c_char) {
    if user_id.is_null() || room_id.is_null() || filename.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let filename_str = unsafe { CStr::from_ptr(filename).to_string_lossy().into_owned() };

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use std::path::Path;

            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                    let path = Path::new(&filename_str);
                    if !path.exists() {
                         log::error!("Avatar file not found: {}", filename_str);
                         return;
                    }
                    
                    if let Ok(bytes) = std::fs::read(path) {
                        let mime = mime_guess::from_path(path).first_or_octet_stream();
                        log::info!("Uploading avatar image {} ({} bytes, mime: {})", filename_str, bytes.len(), mime);
                        
                        // 1. Upload the image
                        match client.media().upload(&mime, bytes, None).await {
                            Ok(response) => {
                                // 2. Set the state event (m.room.avatar)
                                use matrix_sdk::ruma::events::room::avatar::RoomAvatarEventContent;
                                
                                let mut content = RoomAvatarEventContent::new();
                                content.url = Some(response.content_uri);
                                
                                if let Err(e) = room.send_state_event(content).await {
                                     log::error!("Failed to set room avatar state: {:?}", e);
                                } else {
                                     log::info!("Room avatar updated successfully");
                                }
                            },
                            Err(e) => log::error!("Failed to upload avatar image: {:?}", e),
                        }
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
