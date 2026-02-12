use matrix_sdk::Room;
use matrix_sdk::ruma::events::{StateEventType, AnySyncStateEvent};
use matrix_sdk::deserialized_responses::AnySyncOrStrippedState;
use matrix_sdk::ruma::events::tag::TagName;

pub async fn get_room_group_name(room: &Room) -> String {
    // 1. Check Tags (Favorites / Low Priority)
    // Tags are stored in account data, accessed via room.tags() if available in the SDK version
    // or we might need to look them up. matrix-sdk 0.6+ usually has room.tags().
    
    if let Ok(Some(tags)) = room.tags().await {
        if tags.contains_key(&TagName::Favorite) {
            return "Favorites / Rooms".to_string();
        }
        if tags.contains_key(&TagName::LowPriority) {
            return "Low Priority / Rooms".to_string();
        }
        // UserDefined tags could be used here too if we wanted "Matrix: [Tag]"
    }

    // 2. Check Direct Message
    if let Ok(true) = room.is_direct().await {
        return "Direct Messages".to_string();
    }

    // 3. Check Space Parent
    if let Ok(events) = room.get_state_events(StateEventType::SpaceParent).await {
         for raw_event in events {
             if let Ok(event_enum) = raw_event.deserialize() {
                 if let AnySyncOrStrippedState::Sync(any_sync_event) = event_enum {
                     if let AnySyncStateEvent::SpaceParent(sync_event) = *any_sync_event {
                         if let Some(original_event) = sync_event.as_original() {
                             if original_event.content.canonical {
                                 let parent_id = &original_event.state_key; 
                                 if let Ok(parent_id_obj) = <&matrix_sdk::ruma::RoomId>::try_from(parent_id.as_str()) {
                                      if let Some(parent_space) = room.client().get_room(parent_id_obj) {
                                           if let Ok(space_name) = parent_space.display_name().await {
                                               return format!("{} / Rooms", space_name);
                                           }
                                      }
                                 }
                             }
                         }
                     }
                 }
             }
         }
    }

    // 4. Default
    "Matrix Rooms / Rooms".to_string()
}
