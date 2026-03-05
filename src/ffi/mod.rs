use std::sync::Mutex;
use once_cell::sync::Lazy;
use std::os::raw::{c_char, c_void, c_int};
use std::ffi::{CString, CStr};
use crossbeam_channel::{unbounded, Receiver, Sender};
use dashmap::{DashMap, DashSet};
use matrix_sdk::Client;

pub mod events;
pub use events::*;

pub mod auth;
pub mod rooms;
pub mod messages;
pub mod ui;
pub mod users;
pub mod stickers;
pub mod polls;
pub mod threads;
pub mod encryption;
pub mod aliases;
pub mod discovery;

// --- Phase 2.2: Safe FFI String Conversion ---
pub fn safe_c_string(s: &str) -> CString {
    let mut bytes = s.as_bytes().to_vec();
    bytes.retain(|&b| b != 0); // Strip null bytes
    CString::new(bytes).unwrap_or_default()
}

#[macro_export]
macro_rules! into_c_ptr {
    ($s:expr) => {
        $crate::ffi::safe_c_string($s).into_raw()
    };
    (opt $s:expr) => {
        $s.as_ref().map(|s| $crate::ffi::safe_c_string(s).into_raw()).unwrap_or(std::ptr::null_mut())
    };
}

#[macro_export]
macro_rules! ffi_panic_boundary {
    ($t:block) => {
        match std::panic::catch_unwind(std::panic::AssertUnwindSafe(move || {
            $t
        })) {
            Ok(res) => res,
            Err(_) => {
                log::error!("FFI boundary caught a panic!");
            }
        }
    };
    ($t:block, $default:expr) => {
        match std::panic::catch_unwind(std::panic::AssertUnwindSafe(move || {
            $t
        })) {
            Ok(res) => res,
            Err(_) => {
                log::error!("FFI boundary caught a panic!");
                $default
            }
        }
    };
}

// Globals
pub(crate) static CLIENTS: Lazy<DashMap<String, Client>> = Lazy::new(DashMap::new);
pub(crate) static HISTORY_FETCHED_ROOMS: Lazy<DashSet<String>> = Lazy::new(DashSet::new);

pub fn with_client<F, R>(user_id: &str, f: F) -> Option<R> 
where F: FnOnce(Client) -> R {
    CLIENTS.get(user_id).map(|c| f(c.clone()))
}

// --- Phase 1.2: Thread-Safe Bridge ---
pub(crate) static EVENT_CHANNEL: Lazy<(Sender<FfiEvent>, Receiver<FfiEvent>)> = Lazy::new(|| unbounded());

pub(crate) fn send_event(event: FfiEvent) {
    let _ = EVENT_CHANNEL.0.send(event);
}

// Alias for files still using EVENTS_CHANNEL

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init() {
    crate::ffi_panic_boundary!({
        // Force initialization of Lazy globals
        let _ = &*CLIENTS;
        let _ = &*EVENT_CHANNEL;
        let _ = &*crate::RUNTIME;
        log::info!("Matrix Rust SDK FFI initialized.");
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_close_conversation(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        log::info!("Conversation closed: {} for user {}", room_id_str, user_id_str);
        // Here we could implement logic to stop tracking certain thread data if needed
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_event(out_type: *mut i32, out_data: *mut *mut c_void) -> bool {
    crate::ffi_panic_boundary!({
        if out_type.is_null() || out_data.is_null() {
            return false;
        }

        match EVENT_CHANNEL.1.try_recv() {
        Ok(event) => unsafe {
            match event {
                FfiEvent::Message { user_id, sender, msg, room_id, thread_root_id, event_id, timestamp, encrypted } => {
                    *out_type = FfiEventType::MessageReceived as i32;
                    let data = Box::new(MessageEventData {
                        user_id: into_c_ptr!(&user_id),
                        sender: into_c_ptr!(&sender),
                        msg: into_c_ptr!(&msg),
                        room_id: into_c_ptr!(&room_id),
                        thread_root_id: into_c_ptr!(opt thread_root_id),
                        event_id: into_c_ptr!(opt event_id),
                        timestamp,
                        encrypted,
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::MessageEdited { user_id, room_id, event_id, new_msg } => {
                    *out_type = FfiEventType::MessageEdited as i32;
                    let data = Box::new(MessageEditedData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        event_id: into_c_ptr!(&event_id),
                        new_msg: into_c_ptr!(&new_msg),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::Typing { user_id, room_id, who, is_typing } => {
                    *out_type = FfiEventType::TypingState as i32;
                    let data = Box::new(TypingEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        who: into_c_ptr!(&who),
                        is_typing,
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::RoomJoined { user_id, room_id, name, group_name, avatar_url, topic, encrypted, member_count } => {
                    *out_type = FfiEventType::RoomJoined as i32;
                    let data = Box::new(RoomJoinedEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        name: into_c_ptr!(&name),
                        group_name: into_c_ptr!(&group_name),
                        avatar_url: into_c_ptr!(opt avatar_url),
                        topic: into_c_ptr!(&topic),
                        encrypted,
                        member_count,
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ChatTopic { user_id, room_id, topic, sender } => {
                    *out_type = FfiEventType::ChatTopicUpdate as i32;
                    let data = Box::new(ChatTopicEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        topic: into_c_ptr!(&topic),
                        sender: into_c_ptr!(&sender),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ChatUser { user_id, room_id, member_id, add, alias, avatar_path } => {
                    *out_type = FfiEventType::ChatMemberUpdate as i32;
                    let data = Box::new(ChatUserEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        member_id: into_c_ptr!(&member_id),
                        add,
                        alias: into_c_ptr!(opt alias),
                        avatar_path: into_c_ptr!(opt avatar_path),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::Invite { user_id, room_id, inviter } => {
                    *out_type = FfiEventType::InviteReceived as i32;
                    let data = Box::new(InviteEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        inviter: into_c_ptr!(&inviter),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::UpdateBuddy { user_id, alias, avatar_url } => {
                    *out_type = FfiEventType::BuddyUpdate as i32;
                    let data = Box::new(UpdateBuddyEventData {
                        user_id: into_c_ptr!(&user_id),
                        alias: into_c_ptr!(&alias),
                        avatar_url: into_c_ptr!(&avatar_url),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::LoginFailed { user_id, error_msg } => {
                    *out_type = FfiEventType::LoginFailed as i32;
                    let data = Box::new(LoginFailedEventData {
                        user_id: into_c_ptr!(&user_id),
                        error_msg: into_c_ptr!(&error_msg),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::SsoUrl { url } => {
                    *out_type = FfiEventType::SsoUrlReceived as i32;
                    let data = Box::new(SsoUrlEventData {
                        url: into_c_ptr!(&url),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::Connected { user_id } => {
                    *out_type = FfiEventType::Connected as i32;
                    let data = Box::new(ConnectedEventData {
                        user_id: into_c_ptr!(&user_id),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ReadMarker { user_id, room_id, event_id, who } => {
                    *out_type = FfiEventType::ReadMarker as i32;
                    let data = Box::new(ReadMarkerEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        event_id: into_c_ptr!(&event_id),
                        who: into_c_ptr!(&who),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::PowerLevelUpdate { user_id, room_id, is_admin, can_kick, can_ban, can_redact, can_invite } => {
                    *out_type = FfiEventType::PowerLevelUpdate as i32;
                    let data = Box::new(PowerLevelUpdateEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        is_admin,
                        can_kick,
                        can_ban,
                        can_redact,
                        can_invite,
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ReactionsChanged { user_id, room_id, event_id, reactions_text } => {
                    *out_type = FfiEventType::ReactionUpdate as i32;
                    let data = Box::new(ReactionsChangedEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        event_id: into_c_ptr!(&event_id),
                        reactions_text: into_c_ptr!(&reactions_text),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::Presence { user_id, target_user_id, status, status_msg } => {
                    *out_type = FfiEventType::PresenceUpdate as i32;
                    let data = Box::new(PresenceEventData {
                        user_id: into_c_ptr!(&user_id),
                        target_user_id: into_c_ptr!(&target_user_id),
                        status,
                        status_msg: into_c_ptr!(opt status_msg),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::SasRequest { user_id, target_user_id, flow_id } => {
                    *out_type = FfiEventType::VerificationRequest as i32;
                    let data = Box::new(SasRequestEventData {
                        user_id: into_c_ptr!(&user_id),
                        target_user_id: into_c_ptr!(&target_user_id),
                        flow_id: into_c_ptr!(&flow_id),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::SasHaveEmoji { user_id, target_user_id, flow_id, emojis } => {
                    *out_type = FfiEventType::VerificationEmoji as i32;
                    let data = Box::new(SasHaveEmojiEventData {
                        user_id: into_c_ptr!(&user_id),
                        target_user_id: into_c_ptr!(&target_user_id),
                        flow_id: into_c_ptr!(&flow_id),
                        emojis: into_c_ptr!(&emojis),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ShowVerificationQr { user_id, target_user_id, html_data } => {
                    *out_type = FfiEventType::VerificationQr as i32;
                    let data = Box::new(ShowVerificationQrEventData {
                        user_id: into_c_ptr!(&user_id),
                        target_user_id: into_c_ptr!(&target_user_id),
                        html_data: into_c_ptr!(&html_data),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::RoomLeft { user_id, room_id } => {
                    *out_type = FfiEventType::RoomLeft as i32;
                    let data = Box::new(RoomLeftEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::PollCreationRequested { user_id, room_id } => {
                    *out_type = 33; 
                    let data = Box::new(PollCreationRequestedEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ShowUserInfo { user_id, target_user_id, display_name, avatar_url } => {
                    *out_type = FfiEventType::UserInfoReceived as i32;
                    let data = Box::new(ShowUserInfoEventData {
                        user_id: into_c_ptr!(&user_id),
                        target_user_id: into_c_ptr!(&target_user_id),
                        display_name: into_c_ptr!(opt display_name),
                        avatar_url: into_c_ptr!(opt avatar_url),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::PollList { user_id, room_id, event_id, question, sender, options_str } => {
                    *out_type = FfiEventType::PollListReceived as i32;
                    let data = Box::new(PollListEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        event_id: into_c_ptr!(&event_id),
                        question: into_c_ptr!(&question),
                        sender: into_c_ptr!(&sender),
                        options_str: into_c_ptr!(&options_str),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::ThreadList { user_id, room_id, thread_root_id, latest_msg } => {
                    *out_type = FfiEventType::ThreadListReceived as i32;
                    let data = Box::new(ThreadListEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        thread_root_id: into_c_ptr!(&thread_root_id),
                        latest_msg: into_c_ptr!(&latest_msg),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::RoomListAdd { user_id, room_id, name, topic, parent_id } => {
                    *out_type = FfiEventType::RoomListAdded as i32;
                    let data = Box::new(RoomListAddEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        name: into_c_ptr!(&name),
                        topic: into_c_ptr!(opt topic),
                        parent_id: into_c_ptr!(opt parent_id),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::RoomPreview { user_id, room_id_or_alias, html_body } => {
                    *out_type = FfiEventType::RoomPreview as i32;
                    let data = Box::new(RoomPreviewEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id_or_alias: into_c_ptr!(&room_id_or_alias),
                        html_body: into_c_ptr!(&html_body),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::MessageRedacted { user_id, room_id, event_id } => {
                    *out_type = FfiEventType::MessageRedacted as i32;
                    let data = Box::new(MessageRedactedEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        event_id: into_c_ptr!(&event_id),
                    });
                    *out_data = Box::into_raw(data) as *mut c_void;
                },
                FfiEvent::MediaDownloaded { user_id, room_id, event_id, data, content_type } => {
                    *out_type = FfiEventType::MediaDownloaded as i32;
                    let data_len = data.len();
                    let data_ptr = libc::malloc(data_len) as *mut u8;
                    std::ptr::copy_nonoverlapping(data.as_ptr(), data_ptr, data_len);
                    
                    let event_data = Box::new(MediaDownloadedEventData {
                        user_id: into_c_ptr!(&user_id),
                        room_id: into_c_ptr!(&room_id),
                        event_id: into_c_ptr!(&event_id),
                        media_data: data_ptr,
                        media_size: data_len,
                        content_type: into_c_ptr!(&content_type),
                    });
                    *out_data = Box::into_raw(event_data) as *mut c_void;
                },
                _ => return false,
            }
            true
        },
        Err(_) => false,
    }
    }, false)
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_free_event(event_type: i32, data: *mut c_void) {
    crate::ffi_panic_boundary!({
        if data.is_null() {
            return;
        }
        unsafe {
            if let Ok(event_enum) = FfiEventType::try_from(event_type) {
            match event_enum {
                FfiEventType::MessageReceived => { // MessageReceived
                    let d = Box::from_raw(data as *mut MessageEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.sender as *mut c_char);
                    let _ = CString::from_raw(d.msg as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    if !d.thread_root_id.is_null() { let _ = CString::from_raw(d.thread_root_id as *mut c_char); }
                    if !d.event_id.is_null() { let _ = CString::from_raw(d.event_id as *mut c_char); }
                },
                FfiEventType::MessageEdited => { // MessageEdited
                    let d = Box::from_raw(data as *mut MessageEditedData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.event_id as *mut c_char);
                    let _ = CString::from_raw(d.new_msg as *mut c_char);
                },
                FfiEventType::TypingState => { // TypingState
                    let d = Box::from_raw(data as *mut TypingEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.who as *mut c_char);
                },
                FfiEventType::RoomJoined => { // RoomJoined
                    let d = Box::from_raw(data as *mut RoomJoinedEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.name as *mut c_char);
                    let _ = CString::from_raw(d.group_name as *mut c_char);
                    if !d.avatar_url.is_null() { let _ = CString::from_raw(d.avatar_url as *mut c_char); }
                    let _ = CString::from_raw(d.topic as *mut c_char);
                },
                FfiEventType::ChatTopicUpdate => { // ChatTopicUpdate
                    let d = Box::from_raw(data as *mut ChatTopicEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.topic as *mut c_char);
                    let _ = CString::from_raw(d.sender as *mut c_char);
                },
                FfiEventType::ChatMemberUpdate => { // ChatMemberUpdate
                    let d = Box::from_raw(data as *mut ChatUserEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.member_id as *mut c_char);
                    if !d.alias.is_null() { let _ = CString::from_raw(d.alias as *mut c_char); }
                    if !d.avatar_path.is_null() { let _ = CString::from_raw(d.avatar_path as *mut c_char); }
                },
                FfiEventType::InviteReceived => { // InviteReceived
                    let d = Box::from_raw(data as *mut InviteEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.inviter as *mut c_char);
                },
                FfiEventType::BuddyUpdate => { // BuddyUpdate
                    let d = Box::from_raw(data as *mut UpdateBuddyEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.alias as *mut c_char);
                    let _ = CString::from_raw(d.avatar_url as *mut c_char);
                },
                FfiEventType::LoginFailed => { // LoginFailed
                    let d = Box::from_raw(data as *mut LoginFailedEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.error_msg as *mut c_char);
                },
                FfiEventType::SsoUrlReceived => { // SsoUrlReceived
                    let d = Box::from_raw(data as *mut SsoUrlEventData);
                    let _ = CString::from_raw(d.url as *mut c_char);
                },
                FfiEventType::Connected => { // Connected
                    let d = Box::from_raw(data as *mut ConnectedEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                },
                FfiEventType::ReadMarker => { // ReadMarker
                    let d = Box::from_raw(data as *mut ReadMarkerEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.event_id as *mut c_char);
                    let _ = CString::from_raw(d.who as *mut c_char);
                },
                FfiEventType::PowerLevelUpdate => { // PowerLevelUpdate
                    let d = Box::from_raw(data as *mut PowerLevelUpdateEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                },
                FfiEventType::ReactionUpdate => { // ReactionUpdate
                    let d = Box::from_raw(data as *mut ReactionsChangedEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.event_id as *mut c_char);
                    let _ = CString::from_raw(d.reactions_text as *mut c_char);
                },
                FfiEventType::PresenceUpdate => { // PresenceUpdate
                    let d = Box::from_raw(data as *mut PresenceEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.target_user_id as *mut c_char);
                    if !d.status_msg.is_null() { let _ = CString::from_raw(d.status_msg as *mut c_char); }
                },
                FfiEventType::VerificationRequest => { // VerificationRequest
                    let d = Box::from_raw(data as *mut SasRequestEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.target_user_id as *mut c_char);
                    let _ = CString::from_raw(d.flow_id as *mut c_char);
                },
                FfiEventType::VerificationEmoji => { // VerificationEmoji
                    let d = Box::from_raw(data as *mut SasHaveEmojiEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.target_user_id as *mut c_char);
                    let _ = CString::from_raw(d.flow_id as *mut c_char);
                    let _ = CString::from_raw(d.emojis as *mut c_char);
                },
                FfiEventType::VerificationQr => { // VerificationQr
                    let d = Box::from_raw(data as *mut ShowVerificationQrEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.target_user_id as *mut c_char);
                    let _ = CString::from_raw(d.html_data as *mut c_char);
                },
                FfiEventType::RoomLeft => { // RoomLeft
                    let d = Box::from_raw(data as *mut RoomLeftEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                },
                FfiEventType::UserInfoReceived => { // UserInfoReceived
                    let d = Box::from_raw(data as *mut ShowUserInfoEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.target_user_id as *mut c_char);
                    if !d.display_name.is_null() { let _ = CString::from_raw(d.display_name as *mut c_char); }
                    if !d.avatar_url.is_null() { let _ = CString::from_raw(d.avatar_url as *mut c_char); }
                },
                FfiEventType::PollListReceived => { // PollListReceived
                    let d = Box::from_raw(data as *mut PollListEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.event_id as *mut c_char);
                    let _ = CString::from_raw(d.question as *mut c_char);
                    let _ = CString::from_raw(d.sender as *mut c_char);
                    let _ = CString::from_raw(d.options_str as *mut c_char);
                },
                FfiEventType::ThreadListReceived => { // ThreadListReceived
                    let d = Box::from_raw(data as *mut ThreadListEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.thread_root_id as *mut c_char);
                    let _ = CString::from_raw(d.latest_msg as *mut c_char);
                },
                FfiEventType::RoomListAdded => { // RoomListAdded
                    let d = Box::from_raw(data as *mut RoomListAddEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.name as *mut c_char);
                    if !d.topic.is_null() { let _ = CString::from_raw(d.topic as *mut c_char); }
                    if !d.parent_id.is_null() { let _ = CString::from_raw(d.parent_id as *mut c_char); }
                },
                FfiEventType::RoomPreview => { // RoomPreview
                    let d = Box::from_raw(data as *mut RoomPreviewEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id_or_alias as *mut c_char);
                    let _ = CString::from_raw(d.html_body as *mut c_char);
                },
                FfiEventType::MessageRedacted => { // MessageRedacted
                    let d = Box::from_raw(data as *mut MessageRedactedEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.event_id as *mut c_char);
                },
                FfiEventType::MediaDownloaded => { // MediaDownloaded
                    let d = Box::from_raw(data as *mut MediaDownloadedEventData);
                    let _ = CString::from_raw(d.user_id as *mut c_char);
                    let _ = CString::from_raw(d.room_id as *mut c_char);
                    let _ = CString::from_raw(d.event_id as *mut c_char);
                    let _ = CString::from_raw(d.content_type as *mut c_char);
                    if !d.media_data.is_null() {
                        libc::free(d.media_data as *mut libc::c_void);
                    }
                },
                _ => {} // RoomMuteUpdate, RoomTagUpdate, etc. handled by generic ignore or unused
            }
        } else if event_type == 33 { // PollCreationRequested
            let d = Box::from_raw(data as *mut PollCreationRequestedEventData);
            let _ = CString::from_raw(d.user_id as *mut c_char);
            let _ = CString::from_raw(d.room_id as *mut c_char);
        }
        }
    })
}

pub(crate) static IMGSTORE_ADD_CALLBACK: Lazy<Mutex<Option<extern "C" fn(*const u8, usize) -> c_int>>> = Lazy::new(|| Mutex::new(None));
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_imgstore_add_callback(cb: extern "C" fn(*const u8, usize) -> c_int) { *IMGSTORE_ADD_CALLBACK.lock().unwrap_or_else(|e| e.into_inner()) = Some(cb); }

pub fn send_system_message(user_id: &str, msg: &str) {
    send_event(FfiEvent::Message {
        user_id: user_id.to_string(),
        sender: "System".to_string(),
        msg: format!("[System] {}", msg),
        room_id: String::new(),
        thread_root_id: None,
        event_id: None,
        timestamp: 0,
        encrypted: false,
    });
}

pub fn send_system_message_to_room(user_id: &str, room_id: &str, msg: &str) {
    send_event(FfiEvent::Message {
        user_id: user_id.to_string(),
        sender: "System".to_string(),
        msg: format!("[System] {}", msg),
        room_id: room_id.to_string(),
        thread_root_id: None,
        event_id: None,
        timestamp: 0,
        encrypted: false,
    });
}
