use matrix_sdk::ruma::events::room::topic::SyncRoomTopicEvent;
use matrix_sdk::ruma::events::room::member::SyncRoomMemberEvent;
use matrix_sdk::ruma::events::room::tombstone::SyncRoomTombstoneEvent;
use matrix_sdk::Room;

pub async fn handle_room_topic(event: SyncRoomTopicEvent, room: Room) {
    if let SyncRoomTopicEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let topic = &ev.content.topic;
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();

        let body = format!("[System] {} set the topic to: {}", sender, topic);

        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: "System".to_string(),
            msg: body,
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: "".to_string(),
            timestamp,
            encrypted: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_room_member(event: SyncRoomMemberEvent, room: Room) {
    use matrix_sdk::ruma::events::room::member::MembershipState;
    if let SyncRoomMemberEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let target = ev.state_key.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();

        let body = match ev.content.membership {
            MembershipState::Join => format!("[System] {} joined the room.", target),
            MembershipState::Leave => format!("[System] {} left the room.", target),
            MembershipState::Invite => format!("[System] {} invited {} to the room.", sender, target),
            MembershipState::Ban => format!("[System] {} was banned by {}.", target, sender),
            _ => format!("[System] Membership change for {}: {:?}", target, ev.content.membership),
        };

        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: "System".to_string(),
            msg: body,
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: "".to_string(),
            timestamp,
            encrypted: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_stripped_member(_event: matrix_sdk::ruma::events::room::member::StrippedRoomMemberEvent, _room: Room) {
    // Handling for invited state
}

pub async fn handle_tombstone(event: SyncRoomTombstoneEvent, room: Room) {
    if let SyncRoomTombstoneEvent::Original(ev) = event {
        let room_id = room.room_id().as_str();
        let new_room = ev.content.replacement_room.as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let client = room.client();
        let Some(me) = client.user_id() else { return; };
        let local_user_id = me.as_str().to_string();

        let body = format!("[System] This room has been upgraded. New room ID: {}", new_room);

        let event = crate::ffi::FfiEvent::MessageReceived {
            user_id: local_user_id,
            sender: "System".to_string(),
            msg: body,
            room_id: Some(room_id.to_string()),
            thread_root_id: None,
            event_id: "".to_string(),
            timestamp,
            encrypted: false,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}

pub async fn handle_power_levels(_event: matrix_sdk::ruma::events::room::power_levels::SyncRoomPowerLevelsEvent, room: matrix_sdk::Room) {
    if let Ok(pl) = room.power_levels().await {
        let client = room.client();
        let Some(self_id) = client.user_id() else { return; };
        let user_id = self_id.as_str().to_string();
        let room_id = room.room_id().as_str().to_string();
        
        let my_level = pl.for_user(self_id);
        let is_admin = my_level >= matrix_sdk::ruma::int!(100);
        let can_kick = my_level >= pl.kick;
        let can_ban = my_level >= pl.ban;
        let can_redact = my_level >= pl.redact;
        let can_invite = my_level >= pl.invite;
        
        let event = crate::ffi::FfiEvent::PowerLevelUpdate {
            user_id,
            room_id,
            is_admin,
            can_kick,
            can_ban,
            can_redact,
            can_invite,
        };
        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
    }
}
