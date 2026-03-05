use matrix_sdk::ruma::events::receipt::SyncReceiptEvent;
use matrix_sdk::Room;
use crate::ffi::{send_event, events::FfiEvent};

pub async fn handle_receipt(event: SyncReceiptEvent, room: Room) {
    let local_user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    // Receipts are nested: EventID -> ReceiptType -> UserID -> ReceiptInfo
    for (event_id, receipts) in event.content.0 {
        if let Some(read_receipts) = receipts.get(&matrix_sdk::ruma::events::receipt::ReceiptType::Read) {
            for (user_id, _receipt_info) in read_receipts {
                 let who_display = if let Some(m) = room.get_member(user_id).await.ok().flatten() {
                     m.display_name().map(|d| d.to_string()).unwrap_or_else(|| user_id.as_str().to_string())
                 } else {
                     user_id.as_str().to_string()
                 };

                 send_event(FfiEvent::ReadMarker {
                     user_id: local_user_id.clone(),
                     room_id: room.room_id().as_str().to_string(),
                     event_id: event_id.as_str().to_string(),
                     who: who_display,
                 });
            }
        }
    }
}
