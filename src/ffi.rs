use std::os::raw::c_char;
use std::sync::Mutex;
use once_cell::sync::Lazy;

// C-compatible callback signatures
pub type MsgCallback = extern "C" fn(*const c_char, *const c_char, *const c_char, *const c_char, *const c_char, u64);
pub type TypingCallback = extern "C" fn(*const c_char, *const c_char, bool);
pub type RoomJoinedCallback = extern "C" fn(*const c_char, *const c_char, *const c_char, *const c_char);
pub type InviteCallback = extern "C" fn(*const c_char, *const c_char);
pub type UpdateBuddyCallback = extern "C" fn(*const c_char, *const c_char, *const c_char);
pub type PresenceCallback = extern "C" fn(*const c_char, bool);
pub type ChatTopicCallback = extern "C" fn(*const c_char, *const c_char, *const c_char);
pub type ChatUserCallback = extern "C" fn(*const c_char, *const c_char, bool);
pub type SsoCallback = extern "C" fn(*const c_char);
pub type ConnectedCallback = extern "C" fn();
pub type LoginFailedCallback = extern "C" fn(*const c_char);
pub type VerificationRequestCallback = extern "C" fn(*const c_char, *const c_char);
pub type VerificationEmojiCallback = extern "C" fn(*const c_char, *const c_char, *const c_char);
pub type ReadMarkerCallback = extern "C" fn(*const c_char, *const c_char);

// Global callbacks
pub static MSG_CALLBACK: Lazy<Mutex<Option<MsgCallback>>> = Lazy::new(|| Mutex::new(None));
pub static TYPING_CALLBACK: Lazy<Mutex<Option<TypingCallback>>> = Lazy::new(|| Mutex::new(None));
pub static ROOM_JOINED_CALLBACK: Lazy<Mutex<Option<RoomJoinedCallback>>> = Lazy::new(|| Mutex::new(None));
pub static INVITE_CALLBACK: Mutex<Option<InviteCallback>> = Mutex::new(None);
pub static SAS_REQUEST_CALLBACK: Mutex<Option<VerificationRequestCallback>> = Mutex::new(None);
pub static SAS_HAVE_EMOJI_CALLBACK: Mutex<Option<VerificationEmojiCallback>> = Mutex::new(None);
pub static UPDATE_BUDDY_CALLBACK: Lazy<Mutex<Option<UpdateBuddyCallback>>> = Lazy::new(|| Mutex::new(None));
pub static PRESENCE_CALLBACK: Lazy<Mutex<Option<PresenceCallback>>> = Lazy::new(|| Mutex::new(None));
pub static CHAT_TOPIC_CALLBACK: Lazy<Mutex<Option<ChatTopicCallback>>> = Lazy::new(|| Mutex::new(None));
pub static CHAT_USER_CALLBACK: Lazy<Mutex<Option<ChatUserCallback>>> = Lazy::new(|| Mutex::new(None));
pub static SSO_CALLBACK: Mutex<Option<SsoCallback>> = Mutex::new(None);
pub static CONNECTED_CALLBACK: Mutex<Option<ConnectedCallback>> = Mutex::new(None);
pub static LOGIN_FAILED_CALLBACK: Mutex<Option<LoginFailedCallback>> = Mutex::new(None);
pub static READ_MARKER_CALLBACK: Mutex<Option<ReadMarkerCallback>> = Mutex::new(None);

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_msg_callback(cb: MsgCallback) {
    let mut guard = MSG_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_typing_callback(cb: TypingCallback) {
    let mut guard = TYPING_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_room_joined_callback(cb: RoomJoinedCallback) {
    let mut guard = ROOM_JOINED_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_update_buddy_callback(cb: UpdateBuddyCallback) {
    let mut guard = UPDATE_BUDDY_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init_invite_cb(cb: InviteCallback) {
    let mut guard = INVITE_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init_verification_cbs(
    req_cb: VerificationRequestCallback,
    emoji_cb: VerificationEmojiCallback
) {
    let mut guard = SAS_REQUEST_CALLBACK.lock().unwrap();
    *guard = Some(req_cb);
    let mut guard = SAS_HAVE_EMOJI_CALLBACK.lock().unwrap();
    *guard = Some(emoji_cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init_sso_cb(cb: SsoCallback) {
    let mut guard = SSO_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init_connected_cb(cb: ConnectedCallback) {
    let mut guard = CONNECTED_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init_login_failed_cb(cb: LoginFailedCallback) {
    let mut guard = LOGIN_FAILED_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_presence_callback(cb: PresenceCallback) {
    let mut guard = PRESENCE_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_chat_topic_callback(cb: ChatTopicCallback) {
    let mut guard = CHAT_TOPIC_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_chat_user_callback(cb: ChatUserCallback) {
    let mut guard = CHAT_USER_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}

// User Info Callback
pub type ShowUserInfoCallback = extern "C" fn(*const c_char, *const c_char, *const c_char, bool);
pub static SHOW_USER_INFO_CALLBACK: Lazy<Mutex<Option<ShowUserInfoCallback>>> = Lazy::new(|| Mutex::new(None));

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_show_user_info_callback(cb: ShowUserInfoCallback) {
    let mut guard = SHOW_USER_INFO_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_read_marker_callback(cb: ReadMarkerCallback) {
    let mut guard = READ_MARKER_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
