use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_public_rooms(user_id: *const c_char, term: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let term_str = if term.is_null() { None } else { Some(unsafe { CStr::from_ptr(term).to_string_lossy().into_owned() }) };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                // Fix: public_rooms signature mismatch in this SDK version
                match client.public_rooms(Some(50), term_str.as_deref(), None).await {
                  Ok(response) => {
                      for room in response.chunk {
                          let name = room.name.unwrap_or_else(|| room.canonical_alias.map(|a| a.to_string()).unwrap_or_else(|| room.room_id.to_string()));
                          let event = crate::ffi::FfiEvent::RoomListAdd {
                              user_id: uid_async.clone(),
                              room_id: room.room_id.to_string(),
                              name,
                              topic: room.topic.unwrap_or_default(),
                              // Fix: UInt conversion
                              member_count: u64::from(room.num_joined_members) as usize,
                              is_space: false,
                              parent_id: None,
                          };
                          let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                      }
                  },
                  Err(e) => log::error!("Failed to fetch public rooms: {:?}", e),
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_preview_room(user_id: *const c_char, room_id_or_alias: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id_or_alias.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_or_alias_str = unsafe { CStr::from_ptr(room_id_or_alias).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomOrAliasId, RoomAliasId};

                if let Ok(id) = <&RoomOrAliasId>::try_from(room_id_or_alias_str.as_str()) {
                    if id.is_room_alias_id() {
                        if let Ok(alias) = <&RoomAliasId>::try_from(room_id_or_alias_str.as_str()) {
                            match client.resolve_room_alias(alias).await {
                                Ok(response) => {
                                    let room_id = response.room_id;
                                    let html = format!("<b>Room Preview:</b><br/>ID: {}<br/>Server: {:?}", ammonia::clean_text(&room_id.to_string()), response.servers);

                                    let event = crate::ffi::FfiEvent::RoomPreview {
                                        user_id: uid_async.clone(),
                                        room_id_or_alias: room_id_or_alias_str.clone(),
                                        html_body: html,
                                    };
                                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                                },
                                Err(e) => log::error!("Failed to resolve alias {}: {:?}", room_id_or_alias_str, e),
                            }
                        }
                    } else {
                        // It's already a Room ID
                        let html = format!("<b>Room Info:</b><br/>ID: {}", ammonia::clean_text(&room_id_or_alias_str));
                        let event = crate::ffi::FfiEvent::RoomPreview {
                            user_id: uid_async.clone(),
                            room_id_or_alias: room_id_or_alias_str.clone(),
                            html_body: html,
                        };
                        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_users(user_id: *const c_char, term: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || term.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let term_str = unsafe { CStr::from_ptr(term).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::api::client::user_directory::search_users::v3::Request as UserSearchRequest;
                let mut request = UserSearchRequest::new(term_str);
                request.limit = matrix_sdk::ruma::uint!(50);

                match client.send(request).await {
                  Ok(response) => {
                      for user in response.results {
                          let display_name = user.display_name.clone().unwrap_or_else(|| user.user_id.to_string());
                          let display_name = ammonia::clean_text(&display_name);
                          let event = crate::ffi::FfiEvent::ShowUserInfo {
                              user_id: uid_async.clone(),
                              target_user_id: user.user_id.to_string(),
                              display_name: Some(display_name),
                              avatar_url: user.avatar_url.map(|u| u.to_string()),
                              is_online: false,
                          };
                          let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                      }
                  },
                  Err(e) => log::error!("Failed to search users: {:?}", e),
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_public_rooms(user_id: *const c_char, server: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let server_str = if server.is_null() { None } else { Some(unsafe { CStr::from_ptr(server).to_string_lossy().into_owned() }) };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::ServerName;
                let server_name = server_str.and_then(|s| ServerName::parse(s).ok());
                
                // Fix: public_rooms signature
                match client.public_rooms(Some(100), None, server_name.as_deref().map(|s| s.as_ref())).await {
                  Ok(response) => {
                      for room in response.chunk {
                          let name = room.name.unwrap_or_else(|| room.canonical_alias.map(|a| a.to_string()).unwrap_or_else(|| room.room_id.to_string()));
                          let event = crate::ffi::FfiEvent::RoomListAdd {
                              user_id: uid_async.clone(),
                              room_id: room.room_id.to_string(),
                              name,
                              topic: room.topic.unwrap_or_default(),
                              // Fix: UInt conversion
                              member_count: u64::from(room.num_joined_members) as usize,
                              is_space: false,
                              parent_id: None,
                          };
                          let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                      }
                  },
                  Err(e) => log::error!("Failed to list public rooms: {:?}", e),
                }
            });
        });
    })
}
