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

    // 3. Check Space Parent
    if let Ok(events) = room.get_state_events(StateEventType::SpaceParent).await {
         let mut parents: Vec<(matrix_sdk::ruma::OwnedRoomId, SpaceParentEventContent)> = Vec::new();

         for raw_event in events {
             if let Ok(event_enum) = raw_event.deserialize() {
                 match event_enum {
                     AnySyncOrStrippedState::Sync(any_sync_event_box) => {
                         if let AnySyncStateEvent::SpaceParent(e) = *any_sync_event_box {
                             if let Some(original_event) = e.as_original() {
                                 let content = original_event.content.clone();
                                 if let Ok(parent_id) = <&matrix_sdk::ruma::RoomId>::try_from(e.state_key().as_str()) {
                                     parents.push((parent_id.to_owned(), content));
                                 }
                             }
                         }
                     },
                     // Stripped events (for invited rooms) are skipped for space grouping for now to avoid compilation issues
                     // with PossiblyRedacted types in this environment.
                     _ => {}
                 }
             }
         }

         let canonical_parent = parents.iter().find(|(_, content)| content.canonical);
         
         if let Some((parent_id, _)) = canonical_parent {
             if let Some(space) = room.client().get_room(parent_id) {
                 if let Some(name) = space.display_name().await.ok().map(|d| d.to_string()) {
                     return name;
                 }
             }
         }
         
         if !parents.is_empty() {
             let (parent_id, _) = &parents[0];
             if let Some(space) = room.client().get_room(parent_id) {
                 if let Some(name) = space.display_name().await.ok().map(|d| d.to_string()) {
                     return name;
                 }
             }
         }
    }

    // 4. Default
    "Matrix Rooms".to_string()
}
