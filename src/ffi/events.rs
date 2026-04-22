use libc::c_char;

#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FfiEventType {
    MessageReceived = 1,
    TypingState = 2,
    RoomJoined = 3,
    RoomLeft = 4,
    ReadMarker = 5,
    PresenceUpdate = 6,
    ChatTopicUpdate = 7,
    ChatMemberUpdate = 8,
    InviteReceived = 9,
    RoomListAdded = 10,
    RoomPreview = 11,
    LoginFailed = 12,
    UserInfoReceived = 13,
    ThreadListReceived = 14,
    PollListReceived = 15,
    SearchResultReceived = 16,
    RoomMuteUpdate = 17,
    RoomTagUpdate = 18,
    BuddyUpdate = 19,
    SsoUrlReceived = 20,
    Connected = 21,
    VerificationRequest = 22,
    VerificationEmoji = 23,
    VerificationQr = 24,
    MessageEdited = 25,
    ReactionUpdate = 26,
    ReceiptReceived = 27,
    PowerLevelUpdate = 28,
    MessageRedacted = 29,
    MediaDownloaded = 30,
    RoomDashboardInfo = 31,
    PollCreationRequested = 32,
}

impl TryFrom<i32> for FfiEventType {
    type Error = ();
    fn try_from(v: i32) -> Result<Self, Self::Error> {
        match v {
            1 => Ok(FfiEventType::MessageReceived),
            2 => Ok(FfiEventType::TypingState),
            3 => Ok(FfiEventType::RoomJoined),
            4 => Ok(FfiEventType::RoomLeft),
            5 => Ok(FfiEventType::ReadMarker),
            6 => Ok(FfiEventType::PresenceUpdate),
            7 => Ok(FfiEventType::ChatTopicUpdate),
            8 => Ok(FfiEventType::ChatMemberUpdate),
            9 => Ok(FfiEventType::InviteReceived),
            10 => Ok(FfiEventType::RoomListAdded),
            11 => Ok(FfiEventType::RoomPreview),
            12 => Ok(FfiEventType::LoginFailed),
            13 => Ok(FfiEventType::UserInfoReceived),
            14 => Ok(FfiEventType::ThreadListReceived),
            15 => Ok(FfiEventType::PollListReceived),
            16 => Ok(FfiEventType::SearchResultReceived),
            17 => Ok(FfiEventType::RoomMuteUpdate),
            18 => Ok(FfiEventType::RoomTagUpdate),
            19 => Ok(FfiEventType::BuddyUpdate),
            20 => Ok(FfiEventType::SsoUrlReceived),
            21 => Ok(FfiEventType::Connected),
            22 => Ok(FfiEventType::VerificationRequest),
            23 => Ok(FfiEventType::VerificationEmoji),
            24 => Ok(FfiEventType::VerificationQr),
            25 => Ok(FfiEventType::MessageEdited),
            26 => Ok(FfiEventType::ReactionUpdate),
            27 => Ok(FfiEventType::ReceiptReceived),
            28 => Ok(FfiEventType::PowerLevelUpdate),
            29 => Ok(FfiEventType::MessageRedacted),
            30 => Ok(FfiEventType::MediaDownloaded),
            31 => Ok(FfiEventType::RoomDashboardInfo),
            32 => Ok(FfiEventType::PollCreationRequested),
            _ => Err(()),
        }
    }
}

#[repr(C)]
pub struct MessageEventData {
    pub user_id: *mut c_char,
    pub sender: *mut c_char,
    pub sender_id: *mut c_char,
    pub msg: *mut c_char,
    pub room_id: *mut c_char,
    pub thread_root_id: *mut c_char,
    pub event_id: *mut c_char,
    pub timestamp: u64,
    pub encrypted: bool,
    pub is_direct: bool,
}

#[repr(C)]
pub struct MessageEditedData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub new_msg: *mut c_char,
}

#[repr(C)]
pub struct TypingEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub who: *mut c_char,
    pub is_typing: bool,
}

#[repr(C)]
pub struct RoomJoinedEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub name: *mut c_char,
    pub group_name: *mut c_char,
    pub avatar_url: *mut c_char,
    pub topic: *mut c_char,
    pub encrypted: bool,
    pub member_count: u64,
}

#[repr(C)]
pub struct RoomLeftEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
}

#[repr(C)]
pub struct ChatTopicEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub topic: *mut c_char,
    pub sender: *mut c_char,
}

#[repr(C)]
pub struct ChatUserEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub member_id: *mut c_char,
    pub add: bool,
    pub alias: *mut c_char,
    pub avatar_path: *mut c_char,
}

#[repr(C)]
pub struct InviteEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub inviter: *mut c_char,
}

#[repr(C)]
pub struct UpdateBuddyEventData {
    pub user_id: *mut c_char,
    pub alias: *mut c_char,
    pub avatar_url: *mut c_char,
}

#[repr(C)]
pub struct LoginFailedEventData {
    pub user_id: *mut c_char,
    pub error_msg: *mut c_char,
}

#[repr(C)]
pub struct SsoUrlEventData {
    pub url: *mut c_char,
}

#[repr(C)]
pub struct ConnectedEventData {
    pub user_id: *mut c_char,
}

#[repr(C)]
pub struct ReadMarkerEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub who: *mut c_char,
}

#[repr(C)]
pub struct PowerLevelUpdateEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub is_admin: bool,
    pub can_kick: bool,
    pub can_ban: bool,
    pub can_redact: bool,
    pub can_invite: bool,
}

#[repr(C)]
pub struct ReactionsChangedEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub reactions_text: *mut c_char,
}

#[repr(C)]
pub struct PresenceEventData {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub status: i32,
    pub status_msg: *mut c_char,
}

#[repr(C)]
pub struct SasRequestEventData {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub flow_id: *mut c_char,
}

#[repr(C)]
pub struct SasHaveEmojiEventData {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub flow_id: *mut c_char,
    pub emojis: *mut c_char,
}

#[repr(C)]
pub struct ShowVerificationQrEventData {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub html_data: *mut c_char,
}

#[repr(C)]
pub struct ShowUserInfoEventData {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub display_name: *mut c_char,
    pub avatar_url: *mut c_char,
}

#[repr(C)]
pub struct PollListEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub question: *mut c_char,
    pub sender: *mut c_char,
    pub options_str: *mut c_char,
}

#[repr(C)]
pub struct ThreadListEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub thread_root_id: *mut c_char,
    pub latest_msg: *mut c_char,
}

#[repr(C)]
pub struct RoomListAddEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub name: *mut c_char,
    pub topic: *mut c_char,
    pub parent_id: *mut c_char,
}

#[repr(C)]
pub struct RoomPreviewEventData {
    pub user_id: *mut c_char,
    pub room_id_or_alias: *mut c_char,
    pub html_body: *mut c_char,
}

#[repr(C)]
pub struct PollCreationRequestedEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
}

#[repr(C)]
pub struct MessageRedactedEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
}

#[repr(C)]
pub struct MediaDownloadedEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub media_data: *mut u8,
    pub media_size: usize,
    pub content_type: *mut c_char,
}

#[repr(C)]
pub struct RoomDashboardInfoEventData {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub name: *mut c_char,
    pub topic: *mut c_char,
    pub member_count: u64,
    pub encrypted: bool,
    pub power_level: i64,
    pub alias: *mut c_char,
}

// Internal Rust enum for the channel
#[derive(Debug, Clone)]
pub enum FfiEvent {
    Message {
        user_id: String,
        sender: String,
        sender_id: String,
        msg: String,
        room_id: String,
        thread_root_id: Option<String>,
        event_id: Option<String>,
        timestamp: u64,
        encrypted: bool,
        is_direct: bool,
    },
    MessageEdited {
        user_id: String,
        room_id: String,
        event_id: String,
        new_msg: String,
    },
    Typing {
        user_id: String,
        room_id: String,
        who: String,
        is_typing: bool,
    },
    RoomJoined {
        user_id: String,
        room_id: String,
        name: String,
        group_name: String,
        avatar_url: Option<String>,
        topic: String,
        encrypted: bool,
        member_count: u64,
    },
    RoomLeft {
        user_id: String,
        room_id: String,
    },
    ChatTopic {
        user_id: String,
        room_id: String,
        topic: String,
        sender: String,
    },
    ChatUser {
        user_id: String,
        room_id: String,
        member_id: String,
        add: bool,
        alias: Option<String>,
        avatar_path: Option<String>,
    },
    Invite {
        user_id: String,
        room_id: String,
        inviter: String,
    },
    UpdateBuddy {
        user_id: String,
        alias: String,
        avatar_url: String,
    },
    LoginFailed {
        user_id: String,
        error_msg: String,
    },
    SsoUrl {
        url: String,
    },
    Connected {
        user_id: String,
    },
    ReadMarker {
        user_id: String,
        room_id: String,
        event_id: String,
        who: String,
    },
    PowerLevelUpdate {
        user_id: String,
        room_id: String,
        is_admin: bool,
        can_kick: bool,
        can_ban: bool,
        can_redact: bool,
        can_invite: bool,
    },
    ReactionsChanged {
        user_id: String,
        room_id: String,
        event_id: String,
        reactions_text: String,
    },
    Presence {
        user_id: String,
        target_user_id: String,
        status: i32,
        status_msg: Option<String>,
    },
    SasRequest {
        user_id: String,
        target_user_id: String,
        flow_id: String,
    },
    SasHaveEmoji {
        user_id: String,
        target_user_id: String,
        flow_id: String,
        emojis: String,
    },
    ShowVerificationQr {
        user_id: String,
        target_user_id: String,
        html_data: String,
    },
    ShowUserInfo {
        user_id: String,
        target_user_id: String,
        display_name: Option<String>,
        avatar_url: Option<String>,
    },
    PollList {
        user_id: String,
        room_id: String,
        event_id: String,
        question: String,
        sender: String,
        options_str: String,
    },
    ThreadList {
        user_id: String,
        room_id: String,
        thread_root_id: String,
        latest_msg: String,
    },
    RoomListAdd {
        user_id: String,
        room_id: String,
        name: String,
        topic: Option<String>,
        parent_id: Option<String>,
    },
    RoomPreview {
        user_id: String,
        room_id_or_alias: String,
        html_body: String,
    },
    PollCreationRequested {
        user_id: String,
        room_id: String,
    },
    MessageRedacted {
        user_id: String,
        room_id: String,
        event_id: String,
    },
    MediaDownloaded {
        user_id: String,
        room_id: String,
        event_id: String,
        data: Vec<u8>,
        content_type: String,
    },
    RoomDashboardInfo {
        user_id: String,
        room_id: String,
        name: String,
        topic: String,
        member_count: u64,
        encrypted: bool,
        power_level: i64,
        alias: Option<String>,
    },
    // Required by stickers.rs
    StickerPack {
        cb_ptr: usize,
        user_id: String,
        pack_id: String,
        pack_name: String,
        user_data: usize,
    },
    Sticker {
        cb_ptr: usize,
        user_id: String,
        pack_id: String,
        sticker_id: String,
        uri: String,
        description: String,
        user_data: usize,
    },
    StickerDone {
        cb_ptr: usize,
        user_data: usize,
    },
}
