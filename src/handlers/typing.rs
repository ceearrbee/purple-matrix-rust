use matrix_sdk::ruma::events::typing::SyncTypingEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::TYPING_CALLBACK;

use std::collections::{HashMap, HashSet};
use std::sync::Mutex;
use once_cell::sync::Lazy;

// Map<RoomId, Set<UserId>>
static TYPING_STATUS: Lazy<Mutex<HashMap<String, HashSet<String>>>> = Lazy::new(|| Mutex::new(HashMap::new()));

pub async fn handle_typing(event: SyncTypingEvent, room: Room) {
    let room_id = room.room_id().as_str();
    let me = room.client().user_id().map(|u| u.to_owned());
    
    // Convert current typing users to HashSet<String>
    let mut current_typing: HashSet<String> = HashSet::new();
    for user_id in event.content.user_ids {
        if let Some(my_id) = &me {
            if user_id == *my_id {
                continue;
            }
        }
        current_typing.insert(user_id.to_string());
    }

    let mut to_add = Vec::new();
    let mut to_remove = Vec::new();

    {
        let mut guard = TYPING_STATUS.lock().unwrap();
        let old_typing = guard.entry(room_id.to_string()).or_insert_with(HashSet::new);

        // Who started typing? (In current but not in old)
        for user in &current_typing {
            if !old_typing.contains(user) {
                to_add.push(user.clone());
            }
        }

        // Who stopped typing? (In old but not in current)
        for user in old_typing.iter() {
            if !current_typing.contains(user) {
                to_remove.push(user.clone());
            }
        }

        // Update state
        *old_typing = current_typing;
    }

    // Emit callbacks
    let guard = TYPING_CALLBACK.lock().unwrap();
    if let Some(cb) = *guard {
        let c_room_id = CString::new(room_id).unwrap_or_default();
        
        for user in to_add {
            let c_user_id = CString::new(user).unwrap_or_default();
            cb(c_room_id.as_ptr(), c_user_id.as_ptr(), true);
        }
        
        for user in to_remove {
            let c_user_id = CString::new(user).unwrap_or_default();
            cb(c_room_id.as_ptr(), c_user_id.as_ptr(), false);
        }
    }
}
