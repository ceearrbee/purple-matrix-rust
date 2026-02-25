use matrix_sdk::ruma::events::typing::SyncTypingEvent;
use matrix_sdk::Room;
// Removed CString and callback imports

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
    let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    
    for user in to_add {
        let event = crate::ffi::FfiEvent::Typing {
            user_id: user_id.clone(),
            room_id: room_id.to_string(),
            who: user,
            is_typing: true,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
    
    for user in to_remove {
        let event = crate::ffi::FfiEvent::Typing {
            user_id: user_id.clone(),
            room_id: room_id.to_string(),
            who: user,
            is_typing: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}
