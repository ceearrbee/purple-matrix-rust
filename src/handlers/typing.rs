use matrix_sdk::ruma::events::typing::SyncTypingEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::TYPING_CALLBACK;

pub async fn handle_typing(event: SyncTypingEvent, room: Room) {
    let room_id = room.room_id().as_str();
    let me = room.client().user_id().map(|u| u.to_owned());

    for user_id in event.content.user_ids {
        if let Some(my_id) = &me {
            if user_id == *my_id {
                continue;
            }
        }

        let c_room_id = CString::new(room_id).unwrap_or_default();
        let c_user_id = CString::new(user_id.as_str()).unwrap_or_default();
        
        let guard = TYPING_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_room_id.as_ptr(), c_user_id.as_ptr(), true);
        }
    }
}
