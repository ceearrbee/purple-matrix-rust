use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use matrix_sdk::ruma::events::room::ImageInfo;
use std::ffi::{CString, CStr, c_void, c_char};
use crate::ffi::{StickerPackCallback, StickerCallback, StickerDoneCallback};
use crate::{with_client, RUNTIME};

#[derive(Debug, Serialize, Deserialize)]
pub struct StickerPack {
    pub display_name: Option<String>,
    pub avatar_url: Option<String>,
    pub images: HashMap<String, Sticker>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct Sticker {
    pub body: String,
    pub url: String,
    pub info: Option<ImageInfo>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StickerPackContent {
    pub images: HashMap<String, Sticker>,
    pub pack: Option<PackInfo>,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct PackInfo {
    pub display_name: Option<String>,
    pub avatar_url: Option<String>,
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_sticker_packs(
    user_id: *const c_char, 
    cb: StickerPackCallback,
    done_cb: StickerDoneCallback,
    user_data: *mut c_void
) {
    if user_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let u_data_usize = user_data as usize;

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            // 1. Check Global Account Data
            if let Ok(Some(event)) = client.account().fetch_account_data(
                matrix_sdk::ruma::events::GlobalAccountDataEventType::from("im.ponies.user_defined_sticker_pack")
            ).await {
                // Use the raw JSON directly
                if let Ok(content) = serde_json::from_str::<StickerPackContent>(event.json().get()) {
                     let pack_name = content.pack.as_ref().and_then(|p| p.display_name.clone()).unwrap_or("Personal Pack".to_string());
                     let c_id = CString::new("im.ponies.user_defined_sticker_pack").unwrap_or_default();
                     let c_name = CString::new(pack_name).unwrap_or_default();
                     cb(c_id.as_ptr(), c_name.as_ptr(), u_data_usize as *mut c_void);
                }
            }

            // 2. Check Rooms for Sticker Pack state events
            for room in client.joined_rooms() {
                if let Ok(events) = room.get_state_events(matrix_sdk::ruma::events::StateEventType::from("im.ponies.user_defined_sticker_pack")).await {
                    for event in events {
                        use matrix_sdk::deserialized_responses::RawAnySyncOrStrippedState;
                        let json_str = match &event {
                            RawAnySyncOrStrippedState::Sync(raw) => raw.json().get(),
                            RawAnySyncOrStrippedState::Stripped(raw) => raw.json().get(),
                        };

                        if let Ok(content) = serde_json::from_str::<StickerPackContent>(json_str) {
                            let pack_name = content.pack.as_ref().and_then(|p| p.display_name.clone()).unwrap_or_else(|| format!("Room Pack ({})", room.room_id()));
                            
                            let pack_id = format!("room:{}:static", room.room_id());
                            
                            let c_id = CString::new(pack_id).unwrap_or_default();
                            let c_name = CString::new(pack_name).unwrap_or_default();
                            cb(c_id.as_ptr(), c_name.as_ptr(), u_data_usize as *mut c_void);
                        }
                    }
                }
            }
            
            done_cb(u_data_usize as *mut c_void);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_stickers_in_pack(
    user_id: *const c_char, 
    pack_id: *const c_char,
    cb: StickerCallback,
    done_cb: StickerDoneCallback,
    user_data: *mut c_void
) {
    if user_id.is_null() || pack_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let pack_id_str = unsafe { CStr::from_ptr(pack_id).to_string_lossy().into_owned() };
    let u_data_usize = user_data as usize;

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            if pack_id_str == "im.ponies.user_defined_sticker_pack" {
                 if let Ok(Some(event)) = client.account().fetch_account_data(
                    matrix_sdk::ruma::events::GlobalAccountDataEventType::from("im.ponies.user_defined_sticker_pack")
                ).await {
                    if let Ok(content) = serde_json::from_str::<StickerPackContent>(event.json().get()) {
                        for (shortcode, sticker) in content.images {
                             let c_short = CString::new(shortcode).unwrap_or_default();
                             let c_body = CString::new(sticker.body).unwrap_or_default();
                             let c_url = CString::new(sticker.url).unwrap_or_default();
                             cb(c_short.as_ptr(), c_body.as_ptr(), c_url.as_ptr(), u_data_usize as *mut c_void);
                        }
                    }
                }
            } else if pack_id_str.starts_with("room:") {
                let parts: Vec<&str> = pack_id_str.split(':').collect();
                if parts.len() >= 2 {
                    let room_id_str = parts[1];
                    if let Ok(room_id) = <&matrix_sdk::ruma::RoomId>::try_from(room_id_str) {
                        if let Some(room) = client.get_room(room_id) {
                            if let Ok(events) = room.get_state_events(matrix_sdk::ruma::events::StateEventType::from("im.ponies.user_defined_sticker_pack")).await {
                                for event in events {
                                    use matrix_sdk::deserialized_responses::RawAnySyncOrStrippedState;
                                    let json_str = match &event {
                                        RawAnySyncOrStrippedState::Sync(raw) => raw.json().get(),
                                        RawAnySyncOrStrippedState::Stripped(raw) => raw.json().get(),
                                    };
                                    if let Ok(content) = serde_json::from_str::<StickerPackContent>(json_str) {
                                        for (shortcode, sticker) in content.images {
                                            let c_short = CString::new(shortcode).unwrap_or_default();
                                            let c_body = CString::new(sticker.body).unwrap_or_default();
                                            let c_url = CString::new(sticker.url).unwrap_or_default();
                                            cb(c_short.as_ptr(), c_body.as_ptr(), c_url.as_ptr(), u_data_usize as *mut c_void);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            done_cb(u_data_usize as *mut c_void);
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_search_stickers(
    user_id: *const c_char,
    search_term: *const c_char,
    room_id: *const c_char
) {
    if user_id.is_null() || search_term.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let term_str = unsafe { CStr::from_ptr(search_term).to_string_lossy().to_lowercase() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            let mut results = Vec::new();
            
            if let Ok(Some(event)) = client.account().fetch_account_data(
                matrix_sdk::ruma::events::GlobalAccountDataEventType::from("im.ponies.user_defined_sticker_pack")
            ).await {
                if let Ok(content) = serde_json::from_str::<StickerPackContent>(event.json().get()) {
                    for (shortcode, sticker) in content.images {
                        if shortcode.to_lowercase().contains(&term_str) || sticker.body.to_lowercase().contains(&term_str) {
                            results.push(format!("{} ({}): {}", sticker.body, shortcode, sticker.url));
                        }
                    }
                }
            }

            for room in client.joined_rooms() {
                if let Ok(events) = room.get_state_events(matrix_sdk::ruma::events::StateEventType::from("im.ponies.user_defined_sticker_pack")).await {
                    for event in events {
                        use matrix_sdk::deserialized_responses::RawAnySyncOrStrippedState;
                        let json_str = match &event {
                            RawAnySyncOrStrippedState::Sync(raw) => raw.json().get(),
                            RawAnySyncOrStrippedState::Stripped(raw) => raw.json().get(),
                        };
                        if let Ok(content) = serde_json::from_str::<StickerPackContent>(json_str) {
                            for (shortcode, sticker) in content.images {
                                if shortcode.to_lowercase().contains(&term_str) || sticker.body.to_lowercase().contains(&term_str) {
                                    results.push(format!("{} ({}): {}", sticker.body, shortcode, sticker.url));
                                }
                            }
                        }
                    }
                }
            }

            if results.is_empty() {
                crate::ffi::send_system_message_to_room(&user_id_str, &room_id_str, &format!("No stickers found for '{}'.", term_str));
            } else {
                let mut msg = format!("<b>Sticker search results for '{}':</b><br/>", term_str);
                for r in results.iter().take(10) {
                    msg.push_str(&format!("- {}<br/>", r));
                }
                if results.len() > 10 {
                    msg.push_str("... (truncated)");
                }
                crate::ffi::send_system_message_to_room(&user_id_str, &room_id_str, &msg);
            }
        });
    });
}
