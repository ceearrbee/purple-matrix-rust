use matrix_sdk::ruma::events::poll::start::SyncPollStartEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_poll_start(event: SyncPollStartEvent, room: Room) {
    if let SyncPollStartEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        
        let question = ev.content.poll.question.text.find_plain().unwrap_or("Poll").to_string();
        let options: Vec<String> = ev.content.poll.answers.iter().map(|a| a.text.find_plain().unwrap_or("Option").to_string()).collect();
        
        let body = crate::html_fmt::style_poll(&question, options);

        let c_sender = CString::new(sender).unwrap_or_default();
        let c_body = CString::new(body).unwrap_or_default();
        let c_room_id = CString::new(room_id).unwrap_or_default();
        let c_event_id = CString::new(ev.event_id.as_str()).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
        }
    }
}