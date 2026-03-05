use matrix_sdk::ruma::events::presence::PresenceEvent;
use matrix_sdk::Room;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn handle_presence(event: PresenceEvent, _room: Room) {
     let user_id = event.sender.to_string();
     let status = match event.content.presence {
         matrix_sdk::ruma::presence::PresenceState::Online => 1,
         matrix_sdk::ruma::presence::PresenceState::Unavailable => 2,
         matrix_sdk::ruma::presence::PresenceState::Offline => 0,
         _ => 0,
     };
     let status_msg = event.content.status_msg.clone();

     let me = _room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();

     let event = FfiEvent::Presence {
         user_id: me,
         target_user_id: user_id,
         status,
         status_msg,
     };
     send_event(event);
}
