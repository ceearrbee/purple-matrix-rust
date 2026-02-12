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
pub struct StickerAccountData {
    pub packs: HashMap<String, StickerPack>, // This might not be exact structure of im.ponies... let's verify
}

// Actually, im.ponies.user_defined_sticker_pack content IS the parsed map of keys to packs? 
// No, usually it's `content: { images: { ... }, pack: { ... } }` for a SINGLE pack event.
// And there is a `m.widgets` event that lists them?
// Or `im.ponies.user_defined_sticker_pack.<pack_id>` as separate account data events?
// Element uses `im.ponies.user_defined_sticker_pack` map in `m.widgets`? 
// Wait, `im.ponies.user_defined_sticker_pack` is the EVENT TYPE. 
// Account Data in Matrix is a key-value store where key = event_type (mostly).
// But you can have `im.ponies.user_defined_sticker_pack` as global account data?
// Actually, MSC2545 says: 
// "The sticker packs are stored in the user's account data. The type of the account data event is im.ponies.user_defined_sticker_pack."
// It seems it acts as an index or contains the packs? 
// 
// Let's assume a simpler model first: fetch ALL account data and look for `im.ponies.user_defined_sticker_pack`.
// Usually it's ONE event containing ALL packs? Or parsing `m.widgets`?
//
// Correction: MSC2545 is "Emotes/Sticker packs". 
// It defines `im.ponies.user_defined_sticker_pack` as *Room* Account Data? Or Global?
// Global Account Data event `m.widgets` often contains references.
// But some clients use individual account data events per pack?
//
// Let's look at what Element does. It typically uses `im.ponies.user_defined_sticker_pack` as the event type.
// If there are multiple packs, does it use multiple events? 
// "Account data events have a type and a content... The type matches the event type."
// So there can be only ONE global account data event of type `im.ponies.user_defined_sticker_pack`.
//
// Wait, Element (Web) stores sticker packs in `m.widgets` (for integration manager) OR `im.ponies.user_defined_sticker_pack` (for user parsing).
//
// References suggest `im.ponies.user_defined_sticker_pack` contains:
// {
//   "images": { "shortcode": { "body": "...", "url": "..." } },
//   "pack": { "display_name": "...", "avatar_url": "..." }
// }
//
// But if I want MULTIPLE packs? 
// Apparently Element supports multiple packs by having them as Widgets.
// But there is also a "native" support via `im.ponies.user_defined_sticker_pack`?
//
// Let's assume the user has ONE "Favorites" pack or similar for now if it's a singleton.
// OR, maybe it iterates over rooms?
// 
// Actually, for simplicity and strict MSC2545 compliance (which is vague): 
// Many users use a bot (Maunium Stickerpicker) that uses `m.widgets` account data.
// 
// Let's try to fetch `m.widgets` from Account Data first?
// 
// Alternative: `im.ponies.user_defined_sticker_pack` is often used for "The User's Pack".
// 
// Let's implement a generic way to fetch ANY account data by type, and we'll expose `im.ponies.user_defined_sticker_pack` specifically.
//
// Detailed logic:
// 1. Fetch global account data for `im.ponies.user_defined_sticker_pack`.
// 2. If present, it's ONE pack.
// 
// What about multiple packs?
// Some implementations use `m.widget` events in account data to point to rooms with state events?
// 
// Let's stick to the SIMPLEST first: Access `im.ponies.user_defined_sticker_pack`.
//
// We will also try `m.widgets` later.
//
// Refined Structs:

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
    
    // We need to pass user_data to the callback. 
    // Since `user_data` is `*mut c_void`, it is NOT Send. We cannot pass it successfully into async block?
    // We CAN if we cast it to usize (primitive) and back? Or wrapper?
    // `*mut c_void` is not Send.
    let u_data_usize = user_data as usize;

    with_client(&user_id_str, |client| {
        RUNTIME.spawn(async move {
            // Fetch Account Data
            if let Ok(Some(event)) = client.account().fetch_account_data(
                matrix_sdk::ruma::events::GlobalAccountDataEventType::from("im.ponies.user_defined_sticker_pack")
            ).await {
                if let Ok(content) = serde_json::from_str::<StickerPackContent>(event.json().get()) {
                     // We found A pack.
                     // Since key is "im.ponies...", we treat it as a single pack ID "favorites" or "main".
                     let pack_name = content.pack.as_ref().and_then(|p| p.display_name.clone()).unwrap_or("User Pack".to_string());
                     let pack_id = "im.ponies.user_defined_sticker_pack"; 

                     let c_id = CString::new(pack_id).unwrap_or_default();
                     let c_name = CString::new(pack_name).unwrap_or_default();
                     
                     cb(c_id.as_ptr(), c_name.as_ptr(), u_data_usize as *mut c_void);
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
            // Only supporting the one pack type for now
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
            }
            
            done_cb(u_data_usize as *mut c_void);
        });
    });
}

