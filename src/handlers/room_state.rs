use matrix_sdk::ruma::events::room::topic::SyncRoomTopicEvent;
use matrix_sdk::ruma::events::room::member::SyncRoomMemberEvent;
use matrix_sdk::ruma::events::room::tombstone::SyncRoomTombstoneEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_room_topic(event: SyncRoomTopicEvent, room: Room) {
    if let SyncRoomTopicEvent::Original(ev) = event {
        let sender = ev.sender.as_str();
        let topic = &ev.content.topic;
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let client = room.client();
        let me = client.user_id().unwrap();
        let local_user_id = me.as_str().to_string();

        let body = format!("[System] {} set the topic to: {}", sender, topic);

        let c_local_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_local_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp, false);
        }
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
        let me = client.user_id().unwrap();
        let local_user_id = me.as_str().to_string();

        let body = match ev.content.membership {
            MembershipState::Join => format!("[System] {} joined the room.", target),
            MembershipState::Leave => format!("[System] {} left the room.", target),
            MembershipState::Invite => format!("[System] {} invited {} to the room.", sender, target),
            MembershipState::Ban => format!("[System] {} was banned by {}.", target, sender),
            _ => format!("[System] Membership change for {}: {:?}", target, ev.content.membership),
        };

        let c_local_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_local_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp, false);
        }
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
        let me = client.user_id().unwrap();
        let local_user_id = me.as_str().to_string();

        let body = format!("[System] This room has been upgraded. New room ID: {}", new_room);

        let c_local_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_local_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), timestamp, false);
        }
    }
}
