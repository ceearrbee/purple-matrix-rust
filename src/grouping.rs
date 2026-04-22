use matrix_sdk::Room;
use matrix_sdk::ruma::events::{StateEventType, AnySyncStateEvent, AnyStrippedStateEvent};
use matrix_sdk::deserialized_responses::AnySyncOrStrippedState;
use async_recursion::async_recursion;

pub async fn get_room_group_name(room: &Room) -> String {
    let room_id = room.room_id().as_str();

    // 1. Check Direct Message
    if let Ok(true) = room.is_direct().await {
        log::debug!("Grouping: Room {} is a Direct Message", room_id);
        return "Direct Messages".to_string();
    }

    // 2. Check Space Path (Recursive)
    if let Some(path) = get_space_path(room).await {
        log::debug!("Grouping: Room {} belongs to space path: {}", room_id, path);
        return path;
    }

    // 3. Default fallback group for standalone rooms
    log::debug!("Grouping: Room {} has no space/DM found, using 'Matrix Rooms' fallback", room_id);
    "Matrix Rooms".to_string()
}

async fn get_space_path(room: &Room) -> Option<String> {
    let mut path_parts = Vec::new();
    get_space_path_recursive(room, &mut path_parts, 0).await;
    
    if path_parts.is_empty() {
        None
    } else {
        path_parts.reverse();
        Some(path_parts.join(" / "))
    }
}

#[async_recursion]
async fn get_space_path_recursive(room: &Room, path: &mut Vec<String>, depth: u32) {
    if depth > 5 { return; }

    let mut parent_room = None;

    // Try finding SpaceParent state events
    if let Ok(events) = room.get_state_events(StateEventType::SpaceParent).await {
         let mut parents = Vec::new();
         for raw_event in events {
             match raw_event.deserialize() {
                 Ok(AnySyncOrStrippedState::Sync(any_sync_event_box)) => {
                     if let AnySyncStateEvent::SpaceParent(e) = *any_sync_event_box {
                         if let Some(original_event) = e.as_original() {
                             if let Ok(parent_id) = <&matrix_sdk::ruma::RoomId>::try_from(e.state_key().as_str()) {
                                 parents.push((parent_id.to_owned(), original_event.content.canonical));
                             }
                         }
                     }
                 },
                 Ok(AnySyncOrStrippedState::Stripped(any_stripped_event)) => {
                     if let AnyStrippedStateEvent::SpaceParent(e) = *any_stripped_event {
                         if let Ok(parent_id) = <&matrix_sdk::ruma::RoomId>::try_from(e.state_key.as_str()) {
                             parents.push((parent_id.to_owned(), true));
                         }
                     }
                 },
                 _ => {}
             }
         }

         let canonical = parents.iter().find(|(_, is_canonical)| *is_canonical);
         let target_parent = canonical.or(parents.first());

         if let Some((parent_id, _)) = target_parent {
             parent_room = room.client().get_room(parent_id);
             if parent_room.is_none() {
                 log::debug!("Grouping: Found parent space ID {} but it is not joined. Using ID as name part.", parent_id);
                 path.push(format!("Space: {}", parent_id));
                 return;
             }
         }
    }

    if parent_room.is_none() {
        parent_room = find_parent_space_by_children(room).await;
    }

    if let Some(p_room) = parent_room {
         if let Ok(name) = p_room.display_name().await {
             path.push(name.to_string());
         } else {
             path.push("Unknown Space".to_string());
         }
         get_space_path_recursive(&p_room, path, depth + 1).await;
    }
}

async fn find_parent_space_by_children(room: &Room) -> Option<Room> {
    let client = room.client();
    let room_id = room.room_id();
    for r in client.joined_rooms() {
        // Even if is_space() is false, it might have space child events if it's a space
        // Some rooms might be missing the type flag in early sync
        let has_children = if let Ok(events) = r.get_state_events(StateEventType::SpaceChild).await {
            !events.is_empty()
        } else {
            false
        };

        if r.is_space() || has_children {
            if let Ok(events) = r.get_state_events(StateEventType::SpaceChild).await {
                for raw_event in events {
                    match raw_event.deserialize() {
                        Ok(AnySyncOrStrippedState::Sync(any_sync_event_box)) => {
                            if let AnySyncStateEvent::SpaceChild(e) = *any_sync_event_box {
                                if e.state_key() == room_id.as_str() {
                                    if let Some(original) = e.as_original() {
                                        if !original.content.via.is_empty() {
                                            return Some(r);
                                        }
                                    }
                                }
                            }
                        },
                        Ok(AnySyncOrStrippedState::Stripped(any_stripped_event)) => {
                            if let AnyStrippedStateEvent::SpaceChild(e) = *any_stripped_event {
                                if e.state_key == room_id.as_str() {
                                    return Some(r);
                                }
                            }
                        },
                        _ => {}
                    }
                }
            }
        }
    }
    None
}
