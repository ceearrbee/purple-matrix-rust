use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

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
                      let guard = ROOMLIST_ADD_CALLBACK.lock().unwrap();
                      if let Some(cb) = *guard {
                          for chunk in response.chunk {
                               let room_id = chunk.room_id.to_string();
                               let name = chunk.name.or(chunk.canonical_alias.map(|a| a.to_string())).unwrap_or_else(|| room_id.clone());
                               let topic = chunk.topic.unwrap_or_default();
                               let members = chunk.num_joined_members;

                               let c_id = CString::new(room_id).unwrap_or_default();
                               let c_name = CString::new(name).unwrap_or_default();
                               let c_topic = CString::new(topic).unwrap_or_default();
                               
                               cb(c_name.as_ptr(), c_id.as_ptr(), c_topic.as_ptr(), members.into());
                          }
                      }
                 },
                 Err(e) => log::error!("Failed to fetch public rooms: {:?}", e),
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

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_roomlist_add_callback(cb: RoomListAddCallback) {
    let mut guard = ROOMLIST_ADD_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
