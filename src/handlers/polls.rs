use matrix_sdk::Room;

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
        
        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: sender.to_string(),
            msg: body,
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: ev.event_id.to_string(),
            timestamp,
            encrypted: false,
            is_system: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_poll_response(event: matrix_sdk::ruma::events::poll::response::SyncPollResponseEvent, room: Room) {
    if let matrix_sdk::ruma::events::poll::response::SyncPollResponseEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let poll_id = ev.content.relates_to.event_id.as_str();
        log::info!("User {} voted in poll {}", sender, poll_id);
    }
}
