use matrix_sdk::ruma::events::presence::PresenceEvent;
use matrix_sdk::Client;

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
     
     let event = crate::ffi::FfiEvent::Presence {
         user_id,
         target_user_id: target_user_id.to_string(),
         is_online,
     };
     let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
}
