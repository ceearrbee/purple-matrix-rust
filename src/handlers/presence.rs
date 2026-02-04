use matrix_sdk::ruma::events::presence::PresenceEvent;
use matrix_sdk::Client;
use std::ffi::CString;
use crate::ffi::PRESENCE_CALLBACK;

pub async fn handle_presence(event: PresenceEvent, _client: Client) {
     let user_id = event.sender.as_str();
     use matrix_sdk::ruma::presence::PresenceState;
     let is_online = match event.content.presence {
         PresenceState::Online => true,
         PresenceState::Unavailable => true, // "Away" is online-ish
         _ => false,
     };
     
     log::debug!("Presence update for {}: {:?}", user_id, event.content.presence);
     
     let c_user_id = CString::new(user_id).unwrap_or_default();
     let guard = PRESENCE_CALLBACK.lock().unwrap();
     if let Some(cb) = *guard {
         cb(c_user_id.as_ptr(), is_online);
     }
}
