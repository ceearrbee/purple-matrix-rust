use std::os::raw::{c_char, c_void};
use std::ffi::CStr;
use crate::ffi::{with_client, send_event, events::FfiEvent};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_sticker_packs(user_id: *const c_char, cb_ptr: *const c_void, user_data: *mut c_void) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };

        // Fix: Send events synchronously to prevent UAF of C pointers if the window closes.
        let event = FfiEvent::StickerPack {
            cb_ptr: cb_ptr as usize,
            user_id: user_id_str.clone(),
            pack_id: "default".to_string(),
            pack_name: "Default Stickers".to_string(),
            user_data: user_data as usize,
        };
        send_event(event);

        let done = FfiEvent::StickerDone {
            cb_ptr: cb_ptr as usize,
            user_data: user_data as usize,
        };
        send_event(done);
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_send_sticker(user_id: *const c_char, room_id: *const c_char, mxc_uri: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || mxc_uri.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let mxc_uri_str = unsafe { CStr::from_ptr(mxc_uri).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::OwnedMxcUri;
                use matrix_sdk::ruma::events::sticker::StickerEventContent;

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let uri = match <OwnedMxcUri>::try_from(mxc_uri_str.clone()) {
                                Ok(u) => u,
                                Err(e) => {
                                    log::error!("Invalid MXC URI '{}': {:?}", mxc_uri_str, e);
                                    return;
                                }
                            };
                        // Fix: StickerInfo doesn't have a simple new(), use default or construct manually
                        let content = StickerEventContent::new("Sticker".to_string(), Default::default(), uri);
                        let _ = room.send(content).await;
                    }
                }
            });
        });
    })
}
