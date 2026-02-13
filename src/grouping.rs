use matrix_sdk::Room;
use matrix_sdk::ruma::events::{StateEventType, AnySyncStateEvent};
use matrix_sdk::deserialized_responses::AnySyncOrStrippedState;
use matrix_sdk::ruma::events::tag::TagName;
use matrix_sdk::ruma::events::space::parent::SpaceParentEventContent;

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
    if let Ok(events) = room.get_state_events(StateEventType::SpaceParent).await {
         for raw_event in events {
             if let Ok(AnySyncOrStrippedState::Sync(any_sync_event_box)) = raw_event.deserialize() {
                 if let AnySyncStateEvent::SpaceParent(e) = *any_sync_event_box {
                     if let Some(original_event) = e.as_original() {
                         if let Ok(parent_id) = <&matrix_sdk::ruma::RoomId>::try_from(e.state_key().as_str()) {
                             if let Some(parent_room) = room.client().get_room(parent_id) {
                                 // Recurse to find the top-most space?
                                 // For now, let's just go one level up or use the first canonical one.
                                 if original_event.content.canonical {
                                     if let Ok(name) = parent_room.display_name().await {
                                         return Some(name.to_string());
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
