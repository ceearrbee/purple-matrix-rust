use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_room_alias(user_id: *const c_char, room_id: *const c_char, alias_localpart: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || alias_localpart.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let alias_localpart_str = unsafe { CStr::from_ptr(alias_localpart).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomId, RoomAliasId};

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(uid) = client.user_id() {
                         let server = uid.server_name();
                         let full_alias = format!("#{}:{}", alias_localpart_str, server);

                         if let Ok(alias) = <&RoomAliasId>::try_from(full_alias.as_str()) {
                              if let Err(e) = client.create_room_alias(alias, rid).await {
                                   log::error!("Failed to create alias {}: {:?}", full_alias, e);
                              }
                         }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_delete_room_alias(user_id: *const c_char, alias: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || alias.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let alias_str = unsafe { CStr::from_ptr(alias).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomAliasId;

                if let Ok(alias) = <&RoomAliasId>::try_from(alias_str.as_str()) {
                     use matrix_sdk::ruma::api::client::alias::delete_alias::v3::Request as DeleteAliasRequest;
                     let request = DeleteAliasRequest::new(alias.to_owned());
                     // Fix: send only takes one argument
                     let _ = client.send(request).await;
                }
            });
        });
    })
}
