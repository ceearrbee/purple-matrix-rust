use std::sync::Mutex;
use once_cell::sync::Lazy;
use std::os::raw::{c_char, c_void};

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

pub type MsgCallback = extern "C" fn(user_id: *const c_char, sender: *const c_char, msg: *const c_char, room_id: *const c_char, thread_root_id: *const c_char, event_id: *const c_char, timestamp: u64, encrypted: bool);
pub type TypingCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, who: *const c_char, is_typing: bool);
pub type RoomJoinedCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, name: *const c_char, group_name: *const c_char, avatar_url: *const c_char, topic: *const c_char, encrypted: bool);
pub type RoomLeftCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char);
pub type ReadMarkerCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, who: *const c_char);
pub type PresenceCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, is_online: bool);
pub type ChatTopicCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, topic: *const c_char, sender: *const c_char);
pub type ChatUserCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, member_id: *const c_char, add: bool, alias: *const c_char, avatar_path: *const c_char);
pub type InviteCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, inviter: *const c_char);
pub type RoomListAddCallback = extern "C" fn(user_id: *const c_char, name: *const c_char, id: *const c_char, topic: *const c_char, count: u64);
pub type RoomPreviewCallback = extern "C" fn(user_id: *const c_char, room_id_or_alias: *const c_char, html_body: *const c_char);
pub type LoginFailedCallback = extern "C" fn(error_msg: *const c_char);
pub type ShowUserInfoCallback = extern "C" fn(user_id: *const c_char, display_name: *const c_char, avatar_url: *const c_char, target_user_id: *const c_char, is_online: bool);
pub type ThreadListCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, thread_root_id: *const c_char, latest_msg: *const c_char, count: u64, ts: u64);
pub type PollListCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, question: *const c_char, sender: *const c_char, options_str: *const c_char);
pub type SearchCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, sender: *const c_char, message: *const c_char, timestamp_str: *const c_char);
pub type RoomMuteCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, muted: bool);
pub type RoomTagCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, tag: *const c_char);
pub type UpdateBuddyCallback = extern "C" fn(user_id: *const c_char, alias: *const c_char, avatar_url: *const c_char);

pub type StickerPackCallback = extern "C" fn(user_id: *const c_char, pack_id: *const c_char, pack_name: *const c_char, user_data: *mut c_void);
pub type StickerCallback = extern "C" fn(user_id: *const c_char, short_name: *const c_char, body: *const c_char, url: *const c_char, user_data: *mut c_void);
pub type StickerDoneCallback = extern "C" fn(user_data: *mut c_void);

pub type SsoCallback = extern "C" fn(url: *const c_char);
pub type ConnectedCallback = extern "C" fn(user_id: *const c_char);
pub type SasRequestCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char);
pub type SasHaveEmojiCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char, emojis: *const c_char);
pub type ShowVerificationQrCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, html_data: *const c_char);

pub(crate) static MSG_CALLBACK: Lazy<Mutex<Option<MsgCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static TYPING_CALLBACK: Lazy<Mutex<Option<TypingCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static ROOM_JOINED_CALLBACK: Lazy<Mutex<Option<RoomJoinedCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static ROOM_LEFT_CALLBACK: Lazy<Mutex<Option<RoomLeftCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static READ_MARKER_CALLBACK: Lazy<Mutex<Option<ReadMarkerCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static PRESENCE_CALLBACK: Lazy<Mutex<Option<PresenceCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static CHAT_TOPIC_CALLBACK: Lazy<Mutex<Option<ChatTopicCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static CHAT_USER_CALLBACK: Lazy<Mutex<Option<ChatUserCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static INVITE_CALLBACK: Lazy<Mutex<Option<InviteCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static ROOMLIST_ADD_CALLBACK: Lazy<Mutex<Option<RoomListAddCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static ROOM_PREVIEW_CALLBACK: Lazy<Mutex<Option<RoomPreviewCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static LOGIN_FAILED_CALLBACK: Lazy<Mutex<Option<LoginFailedCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static SHOW_USER_INFO_CALLBACK: Lazy<Mutex<Option<ShowUserInfoCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static THREAD_LIST_CALLBACK: Lazy<Mutex<Option<ThreadListCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static POLL_LIST_CALLBACK: Lazy<Mutex<Option<PollListCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static SEARCH_CALLBACK: Lazy<Mutex<Option<SearchCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static ROOM_MUTE_CALLBACK: Lazy<Mutex<Option<RoomMuteCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static ROOM_TAG_CALLBACK: Lazy<Mutex<Option<RoomTagCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static UPDATE_BUDDY_CALLBACK: Lazy<Mutex<Option<UpdateBuddyCallback>>> = Lazy::new(|| Mutex::new(None));

pub(crate) static SSO_CALLBACK: Lazy<Mutex<Option<SsoCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static CONNECTED_CALLBACK: Lazy<Mutex<Option<ConnectedCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static SAS_REQUEST_CALLBACK: Lazy<Mutex<Option<SasRequestCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static SAS_HAVE_EMOJI_CALLBACK: Lazy<Mutex<Option<SasHaveEmojiCallback>>> = Lazy::new(|| Mutex::new(None));
pub(crate) static SHOW_VERIFICATION_QR_CALLBACK: Lazy<Mutex<Option<ShowVerificationQrCallback>>> = Lazy::new(|| Mutex::new(None));

#[no_mangle] pub extern "C" fn purple_matrix_rust_set_msg_callback(cb: MsgCallback) { *MSG_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_typing_callback(cb: TypingCallback) { *TYPING_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_room_joined_callback(cb: RoomJoinedCallback) { *ROOM_JOINED_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_room_left_callback(cb: RoomLeftCallback) { *ROOM_LEFT_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_read_marker_callback(cb: ReadMarkerCallback) { *READ_MARKER_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_presence_callback(cb: PresenceCallback) { *PRESENCE_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_chat_topic_callback(cb: ChatTopicCallback) { *CHAT_TOPIC_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_chat_user_callback(cb: ChatUserCallback) { *CHAT_USER_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_init_invite_cb(cb: InviteCallback) { *INVITE_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_roomlist_add_callback(cb: RoomListAddCallback) { *ROOMLIST_ADD_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_room_preview_callback(cb: RoomPreviewCallback) { *ROOM_PREVIEW_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_login_failed_callback(cb: LoginFailedCallback) { *LOGIN_FAILED_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_show_user_info_callback(cb: ShowUserInfoCallback) { *SHOW_USER_INFO_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_thread_list_callback(cb: ThreadListCallback) { *THREAD_LIST_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_poll_list_callback(cb: PollListCallback) { *POLL_LIST_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_search_callback(cb: SearchCallback) { *SEARCH_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_room_mute_callback(cb: RoomMuteCallback) { *ROOM_MUTE_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_room_tag_callback(cb: RoomTagCallback) { *ROOM_TAG_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_set_update_buddy_callback(cb: UpdateBuddyCallback) { *UPDATE_BUDDY_CALLBACK.lock().unwrap() = Some(cb); }

#[no_mangle] pub extern "C" fn purple_matrix_rust_init_sso_cb(cb: SsoCallback) { *SSO_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_init_connected_cb(cb: ConnectedCallback) { *CONNECTED_CALLBACK.lock().unwrap() = Some(cb); }
#[no_mangle] pub extern "C" fn purple_matrix_rust_init_verification_cbs(req_cb: SasRequestCallback, emoji_cb: SasHaveEmojiCallback, qr_cb: ShowVerificationQrCallback) {
    *SAS_REQUEST_CALLBACK.lock().unwrap() = Some(req_cb);
    *SAS_HAVE_EMOJI_CALLBACK.lock().unwrap() = Some(emoji_cb);
    *SHOW_VERIFICATION_QR_CALLBACK.lock().unwrap() = Some(qr_cb);
}

pub fn send_system_message(user_id: &str, msg: &str) {
    use std::ffi::CString;
    let c_user = CString::new(crate::sanitize_string(user_id)).unwrap_or_default();
    let c_sender = CString::new("System").unwrap();
    let c_msg = CString::new(format!("[System] {}", msg)).unwrap_or_default();
    
    let guard = MSG_CALLBACK.lock().unwrap();
    if let Some(cb) = *guard {
        cb(c_user.as_ptr(), c_sender.as_ptr(), c_msg.as_ptr(), std::ptr::null(), std::ptr::null(), std::ptr::null(), 0, false);
    }
}

pub fn send_system_message_to_room(user_id: &str, room_id: &str, msg: &str) {
    use std::ffi::CString;
    let c_user = CString::new(crate::sanitize_string(user_id)).unwrap_or_default();
    let c_sender = CString::new("System").unwrap();
    let c_room = CString::new(crate::sanitize_string(room_id)).unwrap_or_default();
    let c_msg = CString::new(format!("[System] {}", msg)).unwrap_or_default();
    
    let guard = MSG_CALLBACK.lock().unwrap();
    if let Some(cb) = *guard {
        cb(c_user.as_ptr(), c_sender.as_ptr(), c_msg.as_ptr(), c_room.as_ptr(), std::ptr::null(), std::ptr::null(), 0, false);
    }
}
