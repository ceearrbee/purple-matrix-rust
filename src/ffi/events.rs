use std::ffi::CString;
use std::os::raw::c_char;

pub fn to_c_char(s: &str) -> *mut c_char {
    match CString::new(crate::sanitize_string(s)) {
        Ok(c) => c.into_raw(),
        Err(_) => CString::new("").unwrap_or_default().into_raw()
    }
}

pub fn to_c_char_opt(s: &Option<String>) -> *mut c_char {
    if let Some(val) = s {
        to_c_char(val)
    } else {
        std::ptr::null_mut()
    }
}

pub fn free_c_char(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            let _ = CString::from_raw(ptr);
        }
    }
}

#[repr(C)]
pub struct CMessageReceived {
    pub user_id: *mut c_char,
    pub sender: *mut c_char,
    pub msg: *mut c_char,
    pub room_id: *mut c_char,
    pub thread_root_id: *mut c_char,
    pub event_id: *mut c_char,
    pub timestamp: u64,
    pub encrypted: bool,
    pub is_system: bool,
}

#[repr(C)]
pub struct CRoomJoined {
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
pub struct CTyping {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub who: *mut c_char,
    pub is_typing: bool,
}

#[repr(C)]
pub struct CRoomLeft {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
}

#[repr(C)]
pub struct CReadMarker {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub who: *mut c_char,
}

#[repr(C)]
pub struct CPresence {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub is_online: bool,
}

#[repr(C)]
pub struct CChatTopic {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub topic: *mut c_char,
    pub sender: *mut c_char,
}

#[repr(C)]
pub struct CChatUser {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub member_id: *mut c_char,
    pub add: bool,
    pub alias: *mut c_char,
    pub avatar_path: *mut c_char,
}

#[repr(C)]
pub struct CInvite {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub inviter: *mut c_char,
}

#[repr(C)]
pub struct CLoginFailed {
    pub user_id: *mut c_char,
    pub message: *mut c_char,
}

#[repr(C)]
pub struct CConnected {
    pub user_id: *mut c_char,
}

#[repr(C)]
pub struct CSso {
    pub url: *mut c_char,
}

#[repr(C)]
pub struct CReadReceipt {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub receipt_user_id: *mut c_char,
    pub event_id: *mut c_char,
}

#[repr(C)]
pub struct CSasRequest {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub flow_id: *mut c_char,
}

#[repr(C)]
pub struct CSasHaveEmoji {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub flow_id: *mut c_char,
    pub emojis: *mut c_char,
}

#[repr(C)]
pub struct CShowVerificationQr {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub html_data: *mut c_char,
}

#[repr(C)]
pub struct CPollList {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub question: *mut c_char,
    pub sender: *mut c_char,
    pub options_str: *mut c_char,
}

#[repr(C)]
pub struct CPowerLevelUpdate {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub is_admin: bool,
    pub can_kick: bool,
    pub can_ban: bool,
    pub can_redact: bool,
    pub can_invite: bool,
}

#[repr(C)]
pub struct CRoomListAdd {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub name: *mut c_char,
    pub topic: *mut c_char,
    pub member_count: usize,
    pub is_space: bool,
    pub parent_id: *mut c_char,
}

#[repr(C)]
pub struct CRoomPreview {
    pub user_id: *mut c_char,
    pub room_id_or_alias: *mut c_char,
    pub html_body: *mut c_char,
}

#[repr(C)]
pub struct CThreadList {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub thread_root_id: *mut c_char,
    pub latest_msg: *mut c_char,
    pub count: u64,
    pub ts: u64,
}

#[repr(C)]
pub struct CUpdateBuddy {
    pub user_id: *mut c_char,
    pub alias: *mut c_char,
    pub avatar_url: *mut c_char,
}

#[repr(C)]
pub struct CShowUserInfo {
    pub user_id: *mut c_char,
    pub target_user_id: *mut c_char,
    pub display_name: *mut c_char,
    pub avatar_url: *mut c_char,
    pub is_online: bool,
}

#[repr(C)]
pub struct CStickerPack {
    pub cb_ptr: usize,
    pub user_id: *mut c_char,
    pub pack_id: *mut c_char,
    pub pack_name: *mut c_char,
    pub user_data: usize,
}

#[repr(C)]
pub struct CStickerDone {
    pub cb_ptr: usize,
    pub user_data: usize,
}

#[repr(C)]
pub struct CSticker {
    pub cb_ptr: usize,
    pub user_id: *mut c_char,
    pub pack_id: *mut c_char,
    pub sticker_id: *mut c_char,
    pub uri: *mut c_char,
    pub description: *mut c_char,
    pub user_data: usize,
}

#[repr(C)]
pub struct CMessageEdited {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub new_msg: *mut c_char,
}

#[repr(C)]
pub struct CReactionsChanged {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
    pub event_id: *mut c_char,
    pub reactions_text: *mut c_char,
}

#[repr(C)]
pub struct CPollCreationRequested {
    pub user_id: *mut c_char,
    pub room_id: *mut c_char,
}

pub enum FfiEvent {
    MessageReceived {
        user_id: String,
        sender: String,
        msg: String,
        room_id: Option<String>,
        thread_root_id: Option<String>,
        event_id: String,
        timestamp: u64,
        encrypted: bool,
        is_system: bool,
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
        topic: Option<String>,
        encrypted: bool,
        member_count: u64,
    },
    RoomLeft {
        user_id: String,
        room_id: String,
    },
    ReadMarker {
        user_id: String,
        room_id: String,
        event_id: String,
        who: String,
    },
    Presence {
        user_id: String,
        target_user_id: String,
        is_online: bool,
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
    LoginFailed {
        user_id: String,
        message: String,
    },
    Connected {
        user_id: String,
    },
    SsoUrl {
        url: String,
    },
    UpdateBuddy {
        user_id: String,
        target_user_id: String,
        alias: String,
        avatar_url: String,
    },
    MessageEdited {
        user_id: String,
        room_id: String,
        event_id: String,
        new_msg: String,
    },
    ReadReceipt {
        user_id: String,
        room_id: String,
        receipt_user_id: String,
        event_id: String,
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
    PollList {
        user_id: String,
        room_id: String,
        event_id: Option<String>,
        question: Option<String>,
        sender: Option<String>,
        options_str: Option<String>,
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
    RoomListAdd {
        user_id: String,
        room_id: String,
        name: String,
        topic: String,
        member_count: usize,
        is_space: bool,
        parent_id: Option<String>,
    },
    RoomPreview {
        user_id: String,
        room_id_or_alias: String,
        html_body: String,
    },
    ThreadList {
        user_id: String,
        room_id: String,
        thread_root_id: Option<String>,
        latest_msg: Option<String>,
        count: u64,
        ts: u64,
    },
    ShowUserInfo {
        user_id: String,
        target_user_id: String,
        display_name: Option<String>,
        avatar_url: Option<String>,
        is_online: bool,
    },
    StickerPack {
        cb_ptr: usize,
        user_id: String,
        pack_id: String,
        pack_name: String,
        user_data: usize,
    },
    StickerDone {
        cb_ptr: usize,
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
    ReactionsChanged {
        user_id: String,
        room_id: String,
        event_id: String,
        reactions_text: String,
    },
    PollCreationRequested {
        user_id: String,
        room_id: String,
    }
}
