use std::os::raw::c_void;
use crate::ffi::events::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_free_event(ev_type: i32, data: *mut c_void) {
    if data.is_null() { return; }
    crate::ffi_panic_boundary!({
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
                12 => {
                    let b = Box::from_raw(data as *mut CLoginFailed);
                    free_c_char(b.user_id);
                    free_c_char(b.message);
                },
                13 => {
                    let b = Box::from_raw(data as *mut CShowUserInfo);
                    free_c_char(b.user_id);
                    free_c_char(b.target_user_id);
                    free_c_char(b.display_name);
                    free_c_char(b.avatar_url);
                },
                14 => {
                    let b = Box::from_raw(data as *mut CThreadList);
                    free_c_char(b.user_id);
                    free_c_char(b.room_id);
                    free_c_char(b.thread_root_id);
                    free_c_char(b.latest_msg);
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
                19 => {
                    let b = Box::from_raw(data as *mut CUpdateBuddy);
                    free_c_char(b.user_id);
                    free_c_char(b.alias);
                    free_c_char(b.avatar_url);
                },
                21 => {
                    let b = Box::from_raw(data as *mut CStickerPack);
                    free_c_char(b.user_id);
                    free_c_char(b.pack_id);
                    free_c_char(b.pack_name);
                },
                22 => {
                    let b = Box::from_raw(data as *mut CSticker);
                    free_c_char(b.user_id);
                    free_c_char(b.pack_id);
                    free_c_char(b.sticker_id);
                    free_c_char(b.uri);
                    free_c_char(b.description);
                },
                23 => {
                    let _b = Box::from_raw(data as *mut CStickerDone);
                },
                24 => {
                    let b = Box::from_raw(data as *mut CSso);
                    free_c_char(b.url);
                },
                25 => {
                    let b = Box::from_raw(data as *mut CConnected);
                    free_c_char(b.user_id);
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
                29 => {
                    let b = Box::from_raw(data as *mut CPowerLevelUpdate);
                    free_c_char(b.user_id);
                    free_c_char(b.room_id);
                },
                30 => {
                    let b = Box::from_raw(data as *mut CReactionsChanged);
                    free_c_char(b.user_id);
                    free_c_char(b.room_id);
                    free_c_char(b.event_id);
                    free_c_char(b.reactions_text);
                },
                31 => {
                    let b = Box::from_raw(data as *mut CMessageEdited);
                    free_c_char(b.user_id);
                    free_c_char(b.room_id);
                    free_c_char(b.event_id);
                    free_c_char(b.new_msg);
                },
                32 => {
                    let b = Box::from_raw(data as *mut CReadReceipt);
                    free_c_char(b.user_id);
                    free_c_char(b.room_id);
                    free_c_char(b.receipt_user_id);
                    free_c_char(b.event_id);
                },
                33 => {
                    let b = Box::from_raw(data as *mut CPollCreationRequested);
                    free_c_char(b.user_id);
                    free_c_char(b.room_id);
                },
                _ => {}
            }
        }
    })
}
