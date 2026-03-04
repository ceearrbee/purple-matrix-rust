use matrix_sdk::Room;
use matrix_sdk::ruma::events::{StateEventType, AnySyncStateEvent};
use matrix_sdk::deserialized_responses::AnySyncOrStrippedState;
use matrix_sdk::ruma::events::tag::TagName;

pub async fn get_room_group_name(room: &Room) -> String {
    // 1. Check Tags (Favorites / Low Priority)
    if let Ok(Some(tags)) = room.tags().await {
        if tags.contains_key(&TagName::Favorite) {
            return "Favorites".to_string();
        }
        if tags.contains_key(&TagName::LowPriority) {
            return "Low Priority".to_string();
        }
    }

    // 2. Check Direct Message
    if let Ok(true) = room.is_direct().await {
        return "Direct Messages".to_string();
    }

    // 3. Check Space Parent (Recursive)
    if let Some(space_name) = find_top_space_parent(room).await {
        return space_name;
    }

    // 4. Default
    "Matrix Rooms".to_string()
}

async fn find_top_space_parent(room: &Room) -> Option<String> {
    find_top_space_parent_recursive(room, 0).await
}

async fn find_parent_space_by_children(room: &Room) -> Option<Room> {
    let client = room.client();
    let room_id = room.room_id().as_str();
    for r in client.joined_rooms() {
        if r.is_space() {
            // Check if this space has the room as a child
            if let Ok(events) = r.get_state_events(StateEventType::SpaceChild).await {
                for raw_event in events {
                    if let Ok(AnySyncOrStrippedState::Sync(any_sync_event_box)) = raw_event.deserialize() {
                        if let AnySyncStateEvent::SpaceChild(e) = *any_sync_event_box {
                            if e.state_key() == room_id {
                                // Found it! (Make sure it's not a deletion - usually indicated by lack of 'via')
                                if let Some(original) = e.as_original() {
                                    if !original.content.via.is_empty() {
                                        return Some(r);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    None
}

#[async_recursion::async_recursion]
async fn find_top_space_parent_recursive(room: &Room, depth: u32) -> Option<String> {
    if depth > 5 { return None; } // Prevent infinite loops or too deep search

    let mut parent_room = None;

    // 1. Try SpaceParent state events in the room itself
    if let Ok(events) = room.get_state_events(StateEventType::SpaceParent).await {
         let mut parents = Vec::new();
         for raw_event in events {
             if let Ok(AnySyncOrStrippedState::Sync(any_sync_event_box)) = raw_event.deserialize() {
                 if let AnySyncStateEvent::SpaceParent(e) = *any_sync_event_box {
                     if let Some(original_event) = e.as_original() {
                         if let Ok(parent_id) = <&matrix_sdk::ruma::RoomId>::try_from(e.state_key().as_str()) {
                             parents.push((parent_id.to_owned(), original_event.content.canonical));
                         }
                     }
                 }
             }
         }

         // Prefer canonical parent
         let canonical = parents.iter().find(|(_, is_canonical)| *is_canonical);
         let target_parent = canonical.or(parents.first());

         if let Some((parent_id, _)) = target_parent {
             parent_room = room.client().get_room(parent_id);
         }
    }

    // 2. Fallback: Search joined spaces for m.space.child events
    if parent_room.is_none() {
        parent_room = find_parent_space_by_children(room).await;
    }

    if let Some(p_room) = parent_room {
         // Try to go higher
         if let Some(top_name) = find_top_space_parent_recursive(&p_room, depth + 1).await {
             return Some(top_name);
         }
         // If no higher parent found, this is the top one we know
         if let Ok(name) = p_room.display_name().await {
             return Some(name.to_string());
         }
    }

    None
}
