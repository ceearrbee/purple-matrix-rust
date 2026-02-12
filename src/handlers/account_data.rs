use matrix_sdk::ruma::events::AnyRoomAccountDataEvent;
use matrix_sdk::Room;

pub async fn handle_account_data(event: AnyRoomAccountDataEvent, room: Room) {
    match event {
        AnyRoomAccountDataEvent::FullyRead(ev) => {
            let room_id = room.room_id().to_string();
            let event_id = ev.content.event_id.to_string();
            log::info!("Read marker (fully_read) updated in room {}: {}", room_id, event_id);

            let guard = crate::ffi::READ_MARKER_CALLBACK.lock().unwrap();
            if let Some(cb) = *guard {
                let client = room.client();
                let user_id = client.user_id().map(|u| u.as_str()).unwrap_or("Unknown");
                let c_room_id = std::ffi::CString::new(room_id).unwrap_or_default();
                let c_event_id = std::ffi::CString::new(event_id).unwrap_or_default();
                let c_user_id = std::ffi::CString::new(user_id).unwrap_or_default();
                cb(c_room_id.as_ptr(), c_event_id.as_ptr(), c_user_id.as_ptr());
            }
        },
        _ => {
            // Log other interesting account data if needed
        }
    }
}
