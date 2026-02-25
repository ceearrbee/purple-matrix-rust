use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};


#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_public_rooms_for_list(user_id: *const c_char) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    
    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::api::client::directory::get_public_rooms::v3::Request as PublicRoomsRequest;
             
             let mut request = PublicRoomsRequest::new();
             request.limit = Some(50u32.into());

             match client.send(request).await {
                 Ok(response) => {
                     for room in response.chunk {
                         let event = crate::ffi::FfiEvent::RoomListAdd {
                             user_id: user_id_str.clone(),
                             room_id: room.room_id.to_string(),
                             name: room.name.unwrap_or_default(),
                             topic: room.topic.unwrap_or_default(),
                             member_count: u64::from(room.num_joined_members) as usize,
                         };
                         let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                     }
                 },
                 Err(e) => log::error!("Failed to fetch public rooms: {:?}", e),
             }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_fetch_room_preview(user_id: *const c_char, room_id_or_alias: *const c_char) {
    if user_id.is_null() || room_id_or_alias.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_or_alias_str = unsafe { CStr::from_ptr(room_id_or_alias).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomOrAliasId, RoomAliasId};
            
            if let Ok(id) = <&RoomOrAliasId>::try_from(room_id_or_alias_str.as_str()) {
                if id.is_room_alias_id() {
                    if let Ok(alias) = <&RoomAliasId>::try_from(room_id_or_alias_str.as_str()) {
                        match client.resolve_room_alias(alias).await {
                            Ok(response) => {
                                let room_id = response.room_id.to_string();
                                let html = format!("<b>Room Preview:</b><br/>ID: {}<br/>Server: {:?}", room_id, response.servers);
                                
                                let event = crate::ffi::FfiEvent::RoomPreview {
                                    user_id: user_id_str.clone(),
                                    room_id_or_alias: room_id_or_alias_str.clone(),
                                    html_body: html,
                                };
                                let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                            },
                            Err(e) => log::error!("Failed to preview room alias: {:?}", e),
                        }
                    }
                } else {
                    // It's already a Room ID
                    let html = format!("<b>Room Info:</b><br/>ID: {}", room_id_or_alias_str);
                    let event = crate::ffi::FfiEvent::RoomPreview {
                        user_id: user_id_str.clone(),
                        room_id_or_alias: room_id_or_alias_str.clone(),
                        html_body: html,
                    };
                    let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_public_rooms(user_id: *const c_char, search_term: *const c_char, output_room_id: *const c_char) {
     if user_id.is_null() || search_term.is_null() { return; }
     let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
     let term_str = unsafe { CStr::from_ptr(search_term).to_string_lossy().into_owned() };
     let output_rid = if output_room_id.is_null() {
        "System".to_string()
     } else {
        unsafe { CStr::from_ptr(output_room_id).to_string_lossy().into_owned() }
     };
     
     with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
             use matrix_sdk::ruma::api::client::directory::get_public_rooms_filtered::v3::Request as PublicRoomsFilteredRequest;
             use matrix_sdk::ruma::directory::Filter;
             
             let mut filter = Filter::new();
             filter.generic_search_term = Some(term_str);
             
             let mut request = PublicRoomsFilteredRequest::new();
             request.filter = filter;
             request.limit = Some(20u32.into());

             match client.send(request).await {
                 Ok(response) => {
                      let mut result_msg = format!("Found {} public rooms:\n", response.chunk.len());
                      for room in response.chunk.iter().take(10) {
                          result_msg.push_str(&format!("- {} ({}): {}\n", 
                              room.name.as_deref().unwrap_or("Unnamed"),
                              room.room_id,
                              room.topic.as_deref().unwrap_or("No topic")));
                      }
                      
                      crate::ffi::send_system_message_to_room(&user_id_str, &output_rid, &result_msg);
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
                    crate::ffi::send_system_message_to_room(&user_id_str, &room_id_str, &result_msg);
                },
                Err(e) => log::error!("Failed to search users: {:?}", e),
            }
        });
    });
}
