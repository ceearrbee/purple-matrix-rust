use matrix_sdk::ruma::events::presence::PresenceEvent;
use matrix_sdk::Client;
use std::ffi::CString;
use crate::ffi::PRESENCE_CALLBACK;

pub async fn handle_presence(event: PresenceEvent, client: Client) {
     let user_id = client.user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
     let target_user_id = event.sender.as_str();
     use matrix_sdk::ruma::presence::PresenceState;
     let is_online = match event.content.presence {
         PresenceState::Online => true,
         PresenceState::Unavailable => true, // "Away" is online-ish
         _ => false,
     };
     
     log::debug!("Presence update for {}: {:?}", target_user_id, event.content.presence);
     
     if let (Ok(c_user_id), Ok(c_target_id)) = (CString::new(crate::sanitize_string(&user_id)), CString::new(crate::sanitize_string(target_user_id))) {
         let guard = PRESENCE_CALLBACK.lock().unwrap();
         if let Some(cb) = *guard {
             cb(c_user_id.as_ptr(), c_target_id.as_ptr(), is_online);
         }
     }
}
