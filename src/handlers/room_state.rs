use matrix_sdk::ruma::events::room::topic::SyncRoomTopicEvent;
use matrix_sdk::ruma::events::room::member::SyncRoomMemberEvent;
use matrix_sdk::ruma::events::room::tombstone::SyncRoomTombstoneEvent;
use matrix_sdk::Room;
use crate::ffi::{send_event, events::FfiEvent};

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

        // 1. Dispatch Message
        send_event(FfiEvent::Message {
            user_id: local_user_id.clone(),
            sender: "System".to_string(),
            msg: body,
            room_id: room_id.to_string(),
            thread_root_id: None,
            event_id: None,
            timestamp,
            encrypted: false,
        });

        // 2. Dispatch Topic Update
        send_event(FfiEvent::ChatTopic {
            user_id: local_user_id,
            room_id: room_id.to_string(),
            topic: topic.to_string(),
            sender: sender.to_string(),
        });
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

        // Update buddy info
        if ev.content.membership == MembershipState::Join {
            let display_name = ev.content.displayname.clone().unwrap_or_else(|| target.to_string());
            
            let avatar_path = if let Some(mxc_url) = &ev.content.avatar_url {
                crate::media_helper::download_avatar(&client, mxc_url, target).await
            } else {
                None
            };
            
            send_event(FfiEvent::UpdateBuddy {
                user_id: local_user_id.clone(),
                alias: display_name.clone(),
                avatar_url: avatar_path.clone().unwrap_or_default(),
            });

            send_event(FfiEvent::ChatUser {
                user_id: local_user_id.clone(),
                room_id: room_id.to_string(),
                member_id: target.to_string(),
                add: true,
                alias: Some(display_name.clone()),
                avatar_path: avatar_path.clone(),
            });
            
            if target == local_user_id {
                send_event(FfiEvent::UpdateBuddy {
                    user_id: local_user_id.clone(),
                    alias: display_name,
                    avatar_url: avatar_path.unwrap_or_default(),
                });
            }
        } else if ev.content.membership == MembershipState::Leave || ev.content.membership == MembershipState::Ban {
            send_event(FfiEvent::ChatUser {
                user_id: local_user_id.clone(),
                room_id: room_id.to_string(),
                member_id: target.to_string(),
                add: false,
                alias: None,
                avatar_path: None,
            });
        }

        send_event(FfiEvent::Message {
            user_id: local_user_id,
            sender: "System".to_string(),
            msg: body,
            room_id: room_id.to_string(),
            thread_root_id: None,
            event_id: None,
            timestamp,
            encrypted: false,
        });
    }
}

pub async fn handle_stripped_member(_event: matrix_sdk::ruma::events::room::member::StrippedRoomMemberEvent, _room: Room) {
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

        send_event(FfiEvent::Message {
            user_id: local_user_id,
            sender: "System".to_string(),
            msg: body,
            room_id: room_id.to_string(),
            thread_root_id: None,
            event_id: None,
            timestamp,
            encrypted: false,
        });
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
        
        send_event(FfiEvent::PowerLevelUpdate {
            user_id,
            room_id,
            is_admin,
            can_kick,
            can_ban,
            can_redact,
            can_invite,
        });
    }
}
