use matrix_sdk::ruma::events::poll::start::SyncPollStartEvent;
use matrix_sdk::ruma::events::poll::response::SyncPollResponseEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_poll_start(event: SyncPollStartEvent, room: Room) {
    if let SyncPollStartEvent::Original(ev) = event {
        let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        
        let question = ev.content.poll.question.text.find_plain().unwrap_or("Poll").to_string();
        let options: Vec<String> = ev.content.poll.answers.iter().map(|a| a.text.find_plain().unwrap_or("Option").to_string()).collect();
        
        let body = crate::html_fmt::style_poll(&question, options);

        let c_user_id = CString::new(crate::sanitize_string(&user_id)).unwrap_or_default();
        let c_sender = CString::new(crate::sanitize_string(sender)).unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
        }
    }
}

pub async fn handle_poll_response(_event: SyncPollResponseEvent, room: Room) {
    // When someone votes, we should ideally refresh the poll UI.
    // In libpurple, we can't easily "update" an old message, so we'll
    // post a system message with the new tally if requested, or just log it.
    log::info!("Received poll response in room {}", room.room_id());
}