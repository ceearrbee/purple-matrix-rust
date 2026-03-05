#[cfg(test)]
mod ffi_tests {
    use crate::ffi::*;
    use std::mem::{size_of, align_of};

    #[test]
    fn test_struct_layouts() {
        // We verify that the sizes match our expectations for 64-bit systems
        // Pointer = 8 bytes, bool = 1 byte (but often padded to 4 or 8), u64 = 8 bytes.
        
        // MessageEventData
        // 6 pointers (48) + u64 (8) + bool (1) = 57 -> padded to 64
        assert_eq!(size_of::<MessageEventData>(), 64);
        
        // TypingEventData
        // 3 pointers (24) + bool (1) = 25 -> padded to 32
        assert_eq!(size_of::<TypingEventData>(), 32);
        
        // RoomJoinedEventData
        // 6 pointers (48) + bool (1) + u64 (8) = 57 + padding
        // 6 pointers (48) + bool(1) -> padded to 56? + u64(8) = 64
        assert_eq!(size_of::<RoomJoinedEventData>(), 64);

        // PowerLevelUpdateEventData
        // 2 pointers (16) + 5 bools (5) = 21 -> padded to 24 or 32
        // Rust repr(C) for 5 bools might be 5 bytes.
        assert!(size_of::<PowerLevelUpdateEventData>() >= 24);
    }

    #[test]
    fn test_enum_values() {
        assert_eq!(FfiEventType::MessageReceived as i32, 1);
        assert_eq!(FfiEventType::MessageEdited as i32, 25);
        assert_eq!(FfiEventType::MessageRedacted as i32, 29);
    }
}
