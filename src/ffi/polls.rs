use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_create(user_id: *const c_char, room_id: *const c_char, question: *const c_char, options: *const c_char) {
    if user_id.is_null() || room_id.is_null() || question.is_null() || options.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let question_str = unsafe { CStr::from_ptr(question).to_string_lossy().into_owned() };
    let options_str = unsafe { CStr::from_ptr(options).to_string_lossy().into_owned() };
    
    let answers: Vec<String> = options_str.split('|').map(|s| s.to_string()).collect();

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::poll::start::{PollStartEventContent, PollAnswer, PollContentBlock, PollAnswers};
            use matrix_sdk::ruma::events::message::TextContentBlock;
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                     let poll_answers_vec: Vec<PollAnswer> = answers.iter()
                         .enumerate()
                         .map(|(i, s)| PollAnswer::new(i.to_string(), TextContentBlock::plain(s)))
                         .collect();

                     if let Ok(poll_answers) = PollAnswers::try_from(poll_answers_vec) {
                         let question_text = TextContentBlock::plain(question_str.clone());
                         let poll = PollContentBlock::new(question_text, poll_answers);
                         let fallback = TextContentBlock::plain(format!("Poll: {}", question_str));
                         let poll_content = PollStartEventContent::new(fallback, poll);
                         let content = AnyMessageLikeEventContent::PollStart(poll_content);
                         let _ = room.send(content).await;
                     }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_vote(user_id: *const c_char, room_id: *const c_char, poll_id: *const c_char, option_index: u64) {
    if user_id.is_null() || room_id.is_null() || poll_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let poll_id_str = unsafe { CStr::from_ptr(poll_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};
            use matrix_sdk::ruma::events::poll::response::PollResponseEventContent;
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;

            if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                    let response = PollResponseEventContent::new(vec![option_index.to_string()].into(), eid.to_owned());
                    let content = AnyMessageLikeEventContent::PollResponse(response);
                    let _ = room.send(content).await;
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_get_active_polls(user_id: *const c_char, room_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(_room) = client.get_room(rid) {
                    // Iterate through timeline to find polls.
                    // In a real implementation we'd use room.event_cache() or similar.
                    // For now we notify the UI via callback.
                    log::info!("Fetching active polls for room {}", room_id_str);
                    
                    // Simple mock for now: list polls in timeline
                    // We need to fetch enough history or check state.
                    // For the sake of fixing the load error quickly:
                    let guard = POLL_LIST_CALLBACK.lock().unwrap();
                    if let Some(_cb) = *guard {
                         // Send end of list marker
                         // cb(c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), std::ptr::null(), std::ptr::null());
                    }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_set_poll_list_callback(cb: PollListCallback) {
    let mut guard = POLL_LIST_CALLBACK.lock().unwrap();
    *guard = Some(cb);
}
