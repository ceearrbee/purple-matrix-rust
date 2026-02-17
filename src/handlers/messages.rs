use matrix_sdk::ruma::events::room::message::{SyncRoomMessageEvent, Relation};
use matrix_sdk::Room;
use std::ffi::CString;
use crate::ffi::MSG_CALLBACK;

pub async fn handle_room_message(event: matrix_sdk::ruma::serde::Raw<SyncRoomMessageEvent>, room: Room) {
    if let Ok(SyncRoomMessageEvent::Original(ev)) = event.deserialize() {
        let sender = ev.sender.as_str();
        let room_id = room.room_id().as_str();
        let timestamp: u64 = ev.origin_server_ts.0.into();
        let client = room.client();
        let me = client.user_id().unwrap();
        let local_user_id = me.as_str().to_string();

        let mut thread_root_id: Option<String> = None;
        let body = render_room_message(&ev, &room).await;

        // Check for Thread Relation
        if let Some(Relation::Thread(ref thread)) = ev.content.relates_to {
            thread_root_id = Some(thread.event_id.to_string());
        }

        // Simple Highlight Detection (v0.16.0 API alignment)
        let is_highlight = body.contains(me.as_str());

        log::info!("Received msg from {} in {}: {} (Thread: {:?}, Highlight: {})", sender, room_id, body, thread_root_id, is_highlight);

        let c_user_id = CString::new(crate::sanitize_string(&local_user_id)).unwrap_or_default();
        let c_sender = CString::new(crate::sanitize_string(sender)).unwrap_or_default();
        let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();
        
        let c_thread_id = if let Some(ref tid) = thread_root_id {
            let tid_str: &str = tid.as_str();
            CString::new(crate::sanitize_string(tid_str)).unwrap_or_default()
        } else {
            CString::new("").unwrap_or_default()
        };

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            let thread_ptr = if thread_root_id.is_some() {
                c_thread_id.as_ptr()
            } else {
                std::ptr::null()
            };
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), thread_ptr, c_event_id.as_ptr(), timestamp);
        }
    }
}

pub async fn handle_encrypted(event: matrix_sdk::ruma::serde::Raw<matrix_sdk::ruma::events::room::encrypted::SyncRoomEncryptedEvent>, room: Room) {
     use matrix_sdk::ruma::events::room::encrypted::SyncRoomEncryptedEvent;
     
     if let Ok(SyncRoomEncryptedEvent::Original(ev)) = event.deserialize() {
         let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
         let sender = ev.sender.as_str();
         let room_id = room.room_id().as_str();
         let timestamp: u64 = ev.origin_server_ts.0.into();
         
         log::info!("Received Encrypted Event in {} from {}. Scheme: {:?}", room_id, sender, ev.content.scheme);

         let body = "[Encrypted Message: Waiting for keys...] ".to_string();
         
         let c_user_id = CString::new(crate::sanitize_string(&user_id)).unwrap_or_default();
         let c_sender = CString::new(crate::sanitize_string(sender)).unwrap_or_default();
         let c_body = CString::new(crate::sanitize_string(&body)).unwrap_or_default();
         let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
         let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

         let guard = MSG_CALLBACK.lock().unwrap();
         if let Some(cb) = *guard {
             cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), timestamp);
         }
     }
}

pub async fn handle_redaction(event: matrix_sdk::ruma::events::room::redaction::SyncRoomRedactionEvent, room: Room) {
    if let Some(ev) = event.as_original() {
        let user_id = room.client().user_id().map(|u| u.as_str().to_string()).unwrap_or_default();
        let room_id = room.room_id().as_str();
        let target_event_id = ev.redacts.as_ref().map(|id| id.as_str()).unwrap_or("");
        
        let c_user_id = CString::new(crate::sanitize_string(&user_id)).unwrap_or_default();
        let c_sender = CString::new("System").unwrap_or_default();
        let c_body = CString::new(format!("[Redaction] Message {} was removed.", crate::sanitize_string(target_event_id))).unwrap_or_default();
        let c_room_id = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
        let c_event_id = CString::new(crate::sanitize_string(ev.event_id.as_str())).unwrap_or_default();

        let guard = MSG_CALLBACK.lock().unwrap();
        if let Some(cb) = *guard {
            cb(c_user_id.as_ptr(), c_sender.as_ptr(), c_body.as_ptr(), c_room_id.as_ptr(), std::ptr::null(), c_event_id.as_ptr(), ev.origin_server_ts.0.into());
        }
    }
}

pub async fn render_room_message(ev: &matrix_sdk::ruma::events::room::message::OriginalSyncRoomMessageEvent, _room: &Room) -> String {
    let content = &ev.content;
    crate::get_display_html(content)
}
