use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_alias(user_id: *const c_char, room_id: *const c_char, alias_localpart: *const c_char) {
    if user_id.is_null() || room_id.is_null() || alias_localpart.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let alias_localpart_str = unsafe { CStr::from_ptr(alias_localpart).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, RoomAliasId};
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                // Construct full alias. Needs server name.
                // Client `user_id` has server name.
                if let Some(uid) = client.user_id() {
                     let server = uid.server_name();
                     let full_alias = format!("#{}:{}", alias_localpart_str, server);
                     
                     if let Ok(alias) = <&RoomAliasId>::try_from(full_alias.as_str()) {
                          if let Err(e) = client.create_room_alias(alias, rid).await {
                              log::error!("Failed to create alias {}: {:?}", full_alias, e);
                          } else {
                              log::info!("Created alias {} for {}", full_alias, room_id_str);
                          }
                     }
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

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomAliasId;
            
            if let Ok(alias) = <&RoomAliasId>::try_from(alias_str.as_str()) {
                 use matrix_sdk::ruma::api::client::alias::delete_alias::v3::Request as DeleteAliasRequest;
                 let request = DeleteAliasRequest::new(alias.to_owned());
                 if let Err(e) = client.send(request).await {
                     log::error!("Failed to delete alias {}: {:?}", alias_str, e);
                 } else {
                     log::info!("Deleted alias {}", alias_str);
                 }
            }
        });
    });
}
