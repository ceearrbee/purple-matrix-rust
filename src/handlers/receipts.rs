use matrix_sdk::ruma::events::receipt::SyncReceiptEvent;
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::READ_MARKER_CALLBACK;

pub async fn handle_receipt(event: SyncReceiptEvent, room: Room) {
    let local_user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
    // Receipts are nested: EventID -> ReceiptType -> UserID -> ReceiptInfo
    for (event_id, receipts) in event.content.0 {
        if let Some(read_receipts) = receipts.get(&matrix_sdk::ruma::events::receipt::ReceiptType::Read) {
            for (user_id, _receipt_info) in read_receipts {
                 // Call FFI
                 let c_local_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
                 let c_room_id = CString::new(crate::sanitize_string(room.room_id().as_str())).unwrap_or_default();
                 let c_event_id = CString::new(crate::sanitize_string(event_id.as_str())).unwrap_or_default();
                 let c_user_id = CString::new(crate::sanitize_string(user_id.as_str())).unwrap_or_default();

                 let guard = READ_MARKER_CALLBACK.lock().unwrap();
                 if let Some(cb) = *guard {
                     cb(c_local_user_id.as_ptr(), c_room_id.as_ptr(), c_event_id.as_ptr(), c_user_id.as_ptr());
                 }
            }
        }
    }
}
