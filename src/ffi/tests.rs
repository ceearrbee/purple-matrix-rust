#[cfg(test)]
mod tests {
    use crate::ffi::events::*;
    use std::mem::{size_of, align_of};
    use std::os::raw::c_char;

    #[test]
    fn test_ffi_struct_layouts() {
        // Assert 64 bit architecture standard sizes
        assert_eq!(size_of::<*mut c_char>(), 8, "Expected 64-bit pointers for libpurple ABI compatibility");

        // CMessageReceived fields:
        // 6 pointers = 48 bytes
        // 1 u64 = 8 bytes
        // 1 bool = 1 byte
        // Padding at the end: 7 bytes
        assert_eq!(size_of::<CMessageReceived>(), 48 + 8 + 8, "CMessageReceived size mismatch");
        assert_eq!(align_of::<CMessageReceived>(), 8, "CMessageReceived alignment mismatch");

        // CTyping
        // 3 pointers = 24 bytes
        // 1 bool = 1 byte, 7 padding = 32 bytes total
        assert_eq!(size_of::<CTyping>(), 32, "CTyping size mismatch");
        assert_eq!(align_of::<CTyping>(), 8);

        // CPollList
        // 6 pointers = 48 bytes
        assert_eq!(size_of::<CPollList>(), 48, "CPollList size mismatch");
        assert_eq!(align_of::<CPollList>(), 8);

        // CRoomPreview
        // 3 pointers = 24 bytes
        assert_eq!(size_of::<CRoomPreview>(), 24, "CRoomPreview size mismatch");
        assert_eq!(align_of::<CRoomPreview>(), 8);

        // CThreadList
        // 4 pointers = 32 bytes
        // 2 u64 = 16 bytes
        assert_eq!(size_of::<CThreadList>(), 48, "CThreadList size mismatch");
        assert_eq!(align_of::<CThreadList>(), 8);

        // CSso
        // 1 pointer = 8 bytes
        assert_eq!(size_of::<CSso>(), 8, "CSso size mismatch");
        assert_eq!(align_of::<CSso>(), 8);
    }

    #[test]
    fn test_events_channel_dispatch() {
        use crate::ffi::{EVENTS_CHANNEL, FfiEvent};
        
        let event = FfiEvent::LoginFailed {
            user_id: "test_user".to_string(),
            message: "test_message".to_string(),
        };
        
        // Send the event via the global channel (simulating a Toki worker)
        let _ = EVENTS_CHANNEL.0.send(event);
        
        // Receive the event (simulating the C main loop polling)
        let received = EVENTS_CHANNEL.1.try_recv();
        assert!(received.is_ok(), "Failed to receive event from MPSC channel");
        
        if let Ok(FfiEvent::LoginFailed { user_id, message }) = received {
            assert_eq!(user_id, "test_user");
            assert_eq!(message, "test_message");
        } else {
            panic!("Received wrong event type from channel");
        }
    }
}
