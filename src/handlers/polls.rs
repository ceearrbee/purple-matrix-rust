use matrix_sdk::Room;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn handle_poll_start(event: matrix_sdk::ruma::events::poll::start::SyncPollStartEvent, room: Room) {
    if let matrix_sdk::ruma::events::poll::start::SyncPollStartEvent::Original(ev) = event {
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();
        let sender = ev.sender.as_str();
        
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        
        let question = ev.content.poll.question.text.find_plain().unwrap_or("Poll");
        let options: Vec<String> = ev.content.poll.answers.iter().map(|a| a.text.find_plain().unwrap_or("Option").to_string()).collect();
        let body = crate::html_fmt::style_poll(question, options);
        
        send_event(FfiEvent::Message {
            user_id: local_user_id,
            sender: sender.to_string(),
            sender_id: sender.to_string(),
            msg: body,
            room_id: room_id.to_string(),
            thread_root_id: None,
            event_id: Some(ev.event_id.to_string()),
            timestamp,
            encrypted: false,
            is_direct: room.is_direct().await.unwrap_or(false),
        });
    }
}

pub async fn handle_poll_response(event: matrix_sdk::ruma::events::poll::response::SyncPollResponseEvent, _room: Room) {
    if let matrix_sdk::ruma::events::poll::response::SyncPollResponseEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let poll_id = ev.content.relates_to.event_id.as_str();
        log::info!("User {} voted in poll {}", sender, poll_id);
    }
}
