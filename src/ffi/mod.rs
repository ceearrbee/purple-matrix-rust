
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
pub mod events;

#[cfg(test)]
mod tests;

pub use events::*;

pub static EVENTS_CHANNEL: Lazy<(crossbeam_channel::Sender<FfiEvent>, crossbeam_channel::Receiver<FfiEvent>)> = Lazy::new(|| crossbeam_channel::unbounded());

pub(crate) static IMGSTORE_ADD_CALLBACK: Lazy<std::sync::Mutex<Option<extern "C" fn(*const u8, usize) -> std::os::raw::c_int>>> = Lazy::new(|| std::sync::Mutex::new(None));

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_imgstore_add_callback(cb: extern "C" fn(*const u8, usize) -> std::os::raw::c_int) {
    if let Ok(mut lock) = IMGSTORE_ADD_CALLBACK.lock() {
        *lock = Some(cb);
    }
}

pub type MsgCallback = extern "C" fn(user_id: *const c_char, sender: *const c_char, msg: *const c_char, room_id: *const c_char, thread_root_id: *const c_char, event_id: *const c_char, timestamp: u64, encrypted: bool);
pub type TypingCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, who: *const c_char, is_typing: bool);
pub type RoomJoinedCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, name: *const c_char, group_name: *const c_char, avatar_url: *const c_char, topic: *const c_char, encrypted: bool);
pub type RoomLeftCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char);
pub type ReadMarkerCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, who: *const c_char);
pub type PresenceCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, is_online: bool);
pub type ChatTopicCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, topic: *const c_char, sender: *const c_char);
pub type ChatUserCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, member_id: *const c_char, add: bool, alias: *const c_char, avatar_path: *const c_char);
pub type InviteCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, inviter: *const c_char);
pub type RoomListAddCallback = extern "C" fn(user_id: *const c_char, name: *const c_char, id: *const c_char, topic: *const c_char, count: u64, is_space: bool, parent_id: *const c_char);
pub type RoomPreviewCallback = extern "C" fn(user_id: *const c_char, room_id_or_alias: *const c_char, html_body: *const c_char);
pub type LoginFailedCallback = extern "C" fn(error_msg: *const c_char);
pub type ShowUserInfoCallback = extern "C" fn(user_id: *const c_char, display_name: *const c_char, avatar_url: *const c_char, target_user_id: *const c_char, is_online: bool);
pub type ThreadListCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, thread_root_id: *const c_char, latest_msg: *const c_char, count: u64, ts: u64);
pub type PollListCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, event_id: *const c_char, question: *const c_char, sender: *const c_char, options_str: *const c_char);
pub type SearchCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, sender: *const c_char, message: *const c_char, timestamp_str: *const c_char);
pub type RoomMuteCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, muted: bool);
pub type RoomTagCallback = extern "C" fn(user_id: *const c_char, room_id: *const c_char, tag: *const c_char);
pub type UpdateBuddyCallback = extern "C" fn(user_id: *const c_char, alias: *const c_char, avatar_url: *const c_char);
pub type ImgStoreAddCallback = extern "C" fn(data: *const c_void, size: usize) -> i32;

pub type StickerPackCallback = extern "C" fn(user_id: *const c_char, pack_id: *const c_char, pack_name: *const c_char, user_data: *mut c_void);
pub type StickerCallback = extern "C" fn(user_id: *const c_char, short_name: *const c_char, body: *const c_char, url: *const c_char, user_data: *mut c_void);
pub type StickerDoneCallback = extern "C" fn(user_data: *mut c_void);

pub type SsoCallback = extern "C" fn(url: *const c_char);
pub type ConnectedCallback = extern "C" fn(user_id: *const c_char);
pub type SasRequestCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char);
pub type SasHaveEmojiCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, flow_id: *const c_char, emojis: *const c_char);
pub type ShowVerificationQrCallback = extern "C" fn(user_id: *const c_char, target_user_id: *const c_char, html_data: *const c_char);

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_event(
    out_type: *mut i32,
    out_data: *mut *mut c_void,
) -> bool {
    if let Ok(event) = EVENTS_CHANNEL.1.try_recv() {
        let (ev_type, ptr) = match event {
            FfiEvent::MessageReceived { user_id, sender, msg, room_id, thread_root_id, event_id, timestamp, encrypted } => (
                1,
                Box::into_raw(Box::new(CMessageReceived {
                    user_id: to_c_char(&user_id),
                    sender: to_c_char(&sender),
                    msg: to_c_char(&msg),
                    room_id: to_c_char_opt(&room_id),
                    thread_root_id: to_c_char_opt(&thread_root_id),
                    event_id: to_c_char(&event_id),
                    timestamp,
                    encrypted,
                })) as *mut c_void
            ),
            FfiEvent::Typing { user_id, room_id, who, is_typing } => (
                2,
                Box::into_raw(Box::new(CTyping {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    who: to_c_char(&who),
                    is_typing,
                })) as *mut c_void
            ),
            FfiEvent::RoomJoined { user_id, room_id, name, group_name, avatar_url, topic, encrypted, member_count } => (
                3,
                Box::into_raw(Box::new(CRoomJoined {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    name: to_c_char(&name),
                    group_name: to_c_char(&group_name),
                    avatar_url: to_c_char_opt(&avatar_url),
                    topic: to_c_char_opt(&topic),
                    encrypted,
                    member_count,
                })) as *mut c_void
            ),
            FfiEvent::RoomLeft { user_id, room_id } => (
                4,
                Box::into_raw(Box::new(CRoomLeft {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                })) as *mut c_void
            ),
            FfiEvent::ReadMarker { user_id, room_id, event_id, who } => (
                5,
                Box::into_raw(Box::new(CReadMarker {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    event_id: to_c_char(&event_id),
                    who: to_c_char(&who),
                })) as *mut c_void
            ),
            FfiEvent::Presence { user_id, target_user_id, is_online } => (
                6,
                Box::into_raw(Box::new(CPresence {
                    user_id: to_c_char(&user_id),
                    target_user_id: to_c_char(&target_user_id),
                    is_online,
                })) as *mut c_void
            ),
            FfiEvent::ChatTopic { user_id, room_id, topic, sender } => (
                7,
                Box::into_raw(Box::new(CChatTopic {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    topic: to_c_char(&topic),
                    sender: to_c_char(&sender),
                })) as *mut c_void
            ),
            FfiEvent::ChatUser { user_id, room_id, member_id, add, alias, avatar_path } => (
                8,
                Box::into_raw(Box::new(CChatUser {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    member_id: to_c_char(&member_id),
                    add,
                    alias: to_c_char_opt(&alias),
                    avatar_path: to_c_char_opt(&avatar_path),
                })) as *mut c_void
            ),
            FfiEvent::Invite { user_id, room_id, inviter } => (
                9,
                Box::into_raw(Box::new(CInvite {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    inviter: to_c_char(&inviter),
                })) as *mut c_void
            ),
            FfiEvent::LoginFailed { user_id, message } => (
                12,
                Box::into_raw(Box::new(CLoginFailed {
                    user_id: to_c_char(&user_id),
                    message: to_c_char(&message),
                })) as *mut c_void
            ),
            FfiEvent::Connected { user_id } => (
                25,
                Box::into_raw(Box::new(CConnected {
                    user_id: to_c_char(&user_id),
                })) as *mut c_void
            ),
            FfiEvent::SsoUrl { url } => (
                24,
                Box::into_raw(Box::new(CSso {
                    url: to_c_char(&url),
                })) as *mut c_void
            ),
            FfiEvent::SasRequest { user_id, target_user_id, flow_id } => (
                26,
                Box::into_raw(Box::new(CSasRequest {
                    user_id: to_c_char(&user_id),
                    target_user_id: to_c_char(&target_user_id),
                    flow_id: to_c_char(&flow_id),
                })) as *mut c_void
            ),
            FfiEvent::SasHaveEmoji { user_id, target_user_id, flow_id, emojis } => (
                27,
                Box::into_raw(Box::new(CSasHaveEmoji {
                    user_id: to_c_char(&user_id),
                    target_user_id: to_c_char(&target_user_id),
                    flow_id: to_c_char(&flow_id),
                    emojis: to_c_char(&emojis),
                })) as *mut c_void
            ),
            FfiEvent::ShowVerificationQr { user_id, target_user_id, html_data } => (
                28,
                Box::into_raw(Box::new(CShowVerificationQr {
                    user_id: to_c_char(&user_id),
                    target_user_id: to_c_char(&target_user_id),
                    html_data: to_c_char(&html_data),
                })) as *mut c_void
            ),
            FfiEvent::PollList { user_id, room_id, event_id, question, sender, options_str } => (
                15,
                Box::into_raw(Box::new(CPollList {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    event_id: to_c_char_opt(&event_id),
                    question: to_c_char_opt(&question),
                    sender: to_c_char_opt(&sender),
                    options_str: to_c_char_opt(&options_str),
                })) as *mut c_void
            ),
            FfiEvent::RoomListAdd { user_id, room_id, name, topic, member_count, is_space, parent_id } => (
                10,
                Box::into_raw(Box::new(CRoomListAdd {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    name: to_c_char(&name),
                    topic: to_c_char(&topic),
                    member_count,
                    is_space,
                    parent_id: to_c_char_opt(&parent_id),
                })) as *mut c_void
            ),
            FfiEvent::RoomPreview { user_id, room_id_or_alias, html_body } => (
                11,
                Box::into_raw(Box::new(CRoomPreview {
                    user_id: to_c_char(&user_id),
                    room_id_or_alias: to_c_char(&room_id_or_alias),
                    html_body: to_c_char(&html_body),
                })) as *mut c_void
            ),
            FfiEvent::ThreadList { user_id, room_id, thread_root_id, latest_msg, count, ts } => (
                14,
                Box::into_raw(Box::new(CThreadList {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    thread_root_id: to_c_char_opt(&thread_root_id),
                    latest_msg: to_c_char_opt(&latest_msg),
                    count,
                    ts,
                })) as *mut c_void
            ),
            FfiEvent::ShowUserInfo { user_id, target_user_id, display_name, avatar_url, is_online } => (
                13,
                Box::into_raw(Box::new(CShowUserInfo {
                    user_id: to_c_char(&user_id),
                    target_user_id: to_c_char(&target_user_id),
                    display_name: to_c_char_opt(&display_name),
                    avatar_url: to_c_char_opt(&avatar_url),
                    is_online,
                })) as *mut c_void
            ),
            FfiEvent::StickerPack { cb_ptr, user_id, pack_id, pack_name, user_data } => (
                21,
                Box::into_raw(Box::new(CStickerPack {
                    cb_ptr,
                    user_id: to_c_char(&user_id),
                    pack_id: to_c_char(&pack_id),
                    pack_name: to_c_char(&pack_name),
                    user_data,
                })) as *mut c_void
            ),
            FfiEvent::StickerDone { cb_ptr, user_data } => (
                23,
                Box::into_raw(Box::new(CStickerDone {
                    cb_ptr,
                    user_data,
                })) as *mut c_void
            ),
            FfiEvent::Sticker { cb_ptr, user_id, pack_id, sticker_id, uri, description, user_data } => (
                22,
                Box::into_raw(Box::new(CSticker {
                    cb_ptr,
                    user_id: to_c_char(&user_id),
                    pack_id: to_c_char(&pack_id),
                    sticker_id: to_c_char(&sticker_id),
                    uri: to_c_char(&uri),
                    description: to_c_char(&description),
                    user_data,
                })) as *mut c_void
            ),
            FfiEvent::PowerLevelUpdate { user_id, room_id, is_admin, can_kick, can_ban, can_redact, can_invite } => (
                29,
                Box::into_raw(Box::new(CPowerLevelUpdate {
                    user_id: to_c_char(&user_id),
                    room_id: to_c_char(&room_id),
                    is_admin,
                    can_kick,
                    can_ban,
                    can_redact,
                    can_invite,
                })) as *mut c_void
            ),
        };

        unsafe {
            *out_type = ev_type;
            *out_data = ptr;
        }
        return true;
    }
    false
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_free_event(ev_type: i32, data: *mut c_void) {
    if data.is_null() { return; }
    unsafe {
        match ev_type {
            1 => {
                let b = Box::from_raw(data as *mut CMessageReceived);
                free_c_char(b.user_id);
                free_c_char(b.sender);
                free_c_char(b.msg);
                free_c_char(b.room_id);
                free_c_char(b.thread_root_id);
                free_c_char(b.event_id);
            },
            2 => {
                let b = Box::from_raw(data as *mut CTyping);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.who);
            },
            3 => {
                let b = Box::from_raw(data as *mut CRoomJoined);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.name);
                free_c_char(b.group_name);
                free_c_char(b.avatar_url);
                free_c_char(b.topic);
            },
            4 => {
                let b = Box::from_raw(data as *mut CRoomLeft);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
            },
            5 => {
                let b = Box::from_raw(data as *mut CReadMarker);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.event_id);
                free_c_char(b.who);
            },
            6 => {
                let b = Box::from_raw(data as *mut CPresence);
                free_c_char(b.user_id);
                free_c_char(b.target_user_id);
            },
            7 => {
                let b = Box::from_raw(data as *mut CChatTopic);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.topic);
                free_c_char(b.sender);
            },
            8 => {
                let b = Box::from_raw(data as *mut CChatUser);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.member_id);
                free_c_char(b.alias);
                free_c_char(b.avatar_path);
            },
            9 => {
                let b = Box::from_raw(data as *mut CInvite);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.inviter);
            },
            12 => {
                let b = Box::from_raw(data as *mut CLoginFailed);
                free_c_char(b.user_id);
                free_c_char(b.message);
            },
            25 => {
                let b = Box::from_raw(data as *mut CConnected);
                free_c_char(b.user_id);
            },
            24 => {
                let b = Box::from_raw(data as *mut CSso);
                free_c_char(b.url);
            },
            26 => {
                let b = Box::from_raw(data as *mut CSasRequest);
                free_c_char(b.user_id);
                free_c_char(b.target_user_id);
                free_c_char(b.flow_id);
            },
            27 => {
                let b = Box::from_raw(data as *mut CSasHaveEmoji);
                free_c_char(b.user_id);
                free_c_char(b.target_user_id);
                free_c_char(b.flow_id);
                free_c_char(b.emojis);
            },
            28 => {
                let b = Box::from_raw(data as *mut CShowVerificationQr);
                free_c_char(b.user_id);
                free_c_char(b.target_user_id);
                free_c_char(b.html_data);
            },
            15 => {
                let b = Box::from_raw(data as *mut CPollList);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.event_id);
                free_c_char(b.question);
                free_c_char(b.sender);
                free_c_char(b.options_str);
            },
            10 => {
                let b = Box::from_raw(data as *mut CRoomListAdd);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.name);
                free_c_char(b.topic);
                free_c_char(b.parent_id);
            },
            11 => {
                let b = Box::from_raw(data as *mut CRoomPreview);
                free_c_char(b.user_id);
                free_c_char(b.room_id_or_alias);
                free_c_char(b.html_body);
            },
            14 => {
                let b = Box::from_raw(data as *mut CThreadList);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
                free_c_char(b.thread_root_id);
                free_c_char(b.latest_msg);
            },
            13 => {
                let b = Box::from_raw(data as *mut CShowUserInfo);
                free_c_char(b.user_id);
                free_c_char(b.target_user_id);
                free_c_char(b.display_name);
                free_c_char(b.avatar_url);
            },
            21 => {
                let b = Box::from_raw(data as *mut CStickerPack);
                free_c_char(b.user_id);
                free_c_char(b.pack_id);
                free_c_char(b.pack_name);
            },
            23 => {
                let _b = Box::from_raw(data as *mut CStickerDone);
            },
            22 => {
                let b = Box::from_raw(data as *mut CSticker);
                free_c_char(b.user_id);
                free_c_char(b.pack_id);
                free_c_char(b.sticker_id);
                free_c_char(b.uri);
                free_c_char(b.description);
            },
            29 => {
                let b = Box::from_raw(data as *mut CPowerLevelUpdate);
                free_c_char(b.user_id);
                free_c_char(b.room_id);
            },
            _ => log::error!("Unknown FFI event type to free: {}", ev_type),
        }
    }
}

pub fn send_system_message(user_id: &str, msg: &str) {
    let event = FfiEvent::MessageReceived {
        user_id: crate::sanitize_string(user_id),
        sender: "System".to_string(),
        msg: format!("[System] {}", crate::sanitize_string(msg)),
        room_id: None,
        thread_root_id: None,
        event_id: "system".to_string(),
        timestamp: 0,
        encrypted: false,
    };
    let _ = EVENTS_CHANNEL.0.send(event);
}

pub fn send_system_message_to_room(user_id: &str, room_id: &str, msg: &str) {
    let event = FfiEvent::MessageReceived {
        user_id: crate::sanitize_string(user_id),
        sender: "System".to_string(),
        msg: format!("[System] {}", crate::sanitize_string(msg)),
        room_id: Some(crate::sanitize_string(room_id)),
        thread_root_id: None,
        event_id: "system".to_string(),
        timestamp: 0,
        encrypted: false,
    };
    let _ = EVENTS_CHANNEL.0.send(event);
}
