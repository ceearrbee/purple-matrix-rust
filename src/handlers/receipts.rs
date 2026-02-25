use matrix_sdk::ruma::events::receipt::SyncReceiptEvent;
use matrix_sdk::Room;

pub async fn handle_receipt(event: SyncReceiptEvent, room: Room) {
    let local_user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    // Receipts are nested: EventID -> ReceiptType -> UserID -> ReceiptInfo
    for (event_id, receipts) in event.content.0 {
        if let Some(read_receipts) = receipts.get(&matrix_sdk::ruma::events::receipt::ReceiptType::Read) {
            for (user_id, _receipt_info) in read_receipts {
                 let event = crate::ffi::FfiEvent::ReadMarker {
                     user_id: local_user_id.clone(),
                     room_id: room.room_id().as_str().to_string(),
                     event_id: event_id.as_str().to_string(),
                     who: user_id.as_str().to_string(),
                 };
                 let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
            }
        }
    }
}
