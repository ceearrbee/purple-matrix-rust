use std::os::raw::c_char;
use std::ffi::CStr;
use crate::ffi::{with_client, send_event, events::FfiEvent};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_public_rooms(user_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                if let Ok(response) = client.public_rooms(None, None, None).await {
                    for room in response.chunk {
                        let room_id = room.room_id.to_string();
                        let name = room.name.unwrap_or_else(|| "Public Room".to_string());
                        let topic = room.topic;

                        let event = FfiEvent::RoomListAdd {
                            user_id: uid_async.clone(),
                            room_id,
                            name,
                            topic,
                            parent_id: None,
                        };
                        send_event(event);
                    }
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
                use matrix_sdk::ruma::RoomAliasId;
                
                if room_id_or_alias_str.starts_with('#') {
                    if let Ok(alias) = <&RoomAliasId>::try_from(room_id_or_alias_str.as_str()) {
                        match client.resolve_room_alias(alias).await {
                            Ok(response) => {
                                let room_id = response.room_id;
                                let html = format!("<b>Room Preview:</b><br/>ID: {}<br/>Server: {:?}", crate::escape_html(&room_id.to_string()), response.servers);

                                let event = FfiEvent::RoomPreview {
                                    user_id: uid_async.clone(),
                                    room_id_or_alias: room_id_or_alias_str.clone(),
                                    html_body: html,
                                };
                                send_event(event);
                            },
                            Err(e) => log::error!("Failed to resolve alias {}: {:?}", room_id_or_alias_str, e),
                        }
                    }
                } else if room_id_or_alias_str.starts_with('!') {
                    // It's already a Room ID
                    let html = format!("<b>Room Info:</b><br/>ID: {}", crate::escape_html(&room_id_or_alias_str));
                    let event = FfiEvent::RoomPreview {
                        user_id: uid_async.clone(),
                        room_id_or_alias: room_id_or_alias_str.clone(),
                        html_body: html,
                    };
                    send_event(event);
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
                use matrix_sdk::ruma::api::client::user_directory::search_users::v3::Request;
                let request = Request::new(term_str);

                match client.send(request).await {
                  Ok(response) => {
                      for user in response.results {
                          let display_name = user.display_name.clone().unwrap_or_else(|| user.user_id.to_string());
                          let display_name = crate::escape_html(&display_name);
                          let event = FfiEvent::ShowUserInfo {
                              user_id: uid_async.clone(),
                              target_user_id: user.user_id.to_string(),
                              display_name: Some(display_name),
                              avatar_url: user.avatar_url.map(|u| u.to_string()),
                          };
                          send_event(event);
                      }
                  },
                  Err(e) => log::error!("User search failed: {:?}", e),
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_hierarchy(user_id: *const c_char, space_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || space_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let space_id_str = unsafe { CStr::from_ptr(space_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::api::client::space::get_hierarchy::v1::Request;
                if let Ok(rid) = <&matrix_sdk::ruma::RoomId>::try_from(space_id_str.as_str()) {
                    let request = Request::new(rid.to_owned());
                    if let Ok(response) = client.send(request).await {
                        for room in response.rooms {
                            let room_id = room.summary.room_id.to_string();
                            let name = room.summary.name.unwrap_or_else(|| "Sub Room".to_string());
                            let topic = room.summary.topic;

                            let event = FfiEvent::RoomListAdd {
                                user_id: uid_async.clone(),
                                room_id,
                                name,
                                topic,
                                parent_id: Some(space_id_str.clone()),
                            };
                            send_event(event);
                        }
                    }
                }
            });
        });
    })
}
