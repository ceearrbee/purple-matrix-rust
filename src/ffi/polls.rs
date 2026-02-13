use std::ffi::CStr;
use std::os::raw::c_char;
use crate::{RUNTIME, with_client};
use crate::ffi::*;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_create(user_id: *const c_char, room_id: *const c_char, question: *const c_char, options: *const c_char) {
    // options format: "Option1|Option2|Option3"
    if user_id.is_null() || room_id.is_null() || question.is_null() || options.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let question_str = unsafe { CStr::from_ptr(question).to_string_lossy().into_owned() };
    let options_str = unsafe { CStr::from_ptr(options).to_string_lossy().into_owned() };
    
    let answers: Vec<String> = options_str.split('|').map(|s| s.to_string()).collect();

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::ruma::events::poll::start::PollStartEventContent;
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;
            
            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client.get_room(rid) {
                     use matrix_sdk::ruma::events::poll::start::{PollAnswer, PollContentBlock, PollAnswers};
                     use matrix_sdk::ruma::events::message::TextContentBlock;
                     
                     let poll_answers_vec: Vec<PollAnswer> = answers.iter()
                         .enumerate()
                         .map(|(i, s)| PollAnswer::new(i.to_string(), TextContentBlock::plain(s)))
                         .collect();

                     // Use unwrap or default. In production, handle error (too many answers).
                     if let Ok(poll_answers) = PollAnswers::try_from(poll_answers_vec) {
                         let question_text = TextContentBlock::plain(question_str.clone());
                         let poll = PollContentBlock::new(question_text, poll_answers);
                         let fallback = TextContentBlock::plain(format!("Poll: {}", question_str));
                         let poll_content = PollStartEventContent::new(fallback, poll);
                         let content = AnyMessageLikeEventContent::PollStart(poll_content);
                         
                         if let Err(e) = room.send(content).await {
                             log::error!("Failed to create poll: {:?}", e);
                         }
                     } else {
                         log::error!("Failed to create poll: too many answers or validation failed");
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

            if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                     // Pass None for RequestConfig
                     if let Ok(event) = room.event(eid, None).await {
                          use matrix_sdk::ruma::events::AnySyncTimelineEvent;
                          use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;
                          use matrix_sdk::ruma::events::SyncMessageLikeEvent;
                          
                          // Compiler indicated deserialize returns AnySyncTimelineEvent
                          if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::PollStart(SyncMessageLikeEvent::Original(poll_ev)))) = event.raw().deserialize() {
                               let content = poll_ev.content;
                               log::warn!("Poll voting by index not implemented (needs poll def fetch). Index: {}", option_index);
                          }
                     }
                }
            }
        });
    });
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_poll_end(user_id: *const c_char, room_id: *const c_char, poll_id: *const c_char) {
    if user_id.is_null() || room_id.is_null() || poll_id.is_null() { return; }
    let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
    let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
    let poll_id_str = unsafe { CStr::from_ptr(poll_id).to_string_lossy().into_owned() };

    with_client(&user_id_str.clone(), |client| {
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::{RoomId, EventId};
            use matrix_sdk::ruma::events::poll::end::PollEndEventContent;
            use matrix_sdk::ruma::events::AnyMessageLikeEventContent;

            if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_id_str.as_str())) {
                if let Some(room) = client.get_room(rid) {
                     use matrix_sdk::ruma::events::message::TextContentBlock;
                     let end = PollEndEventContent::new(TextContentBlock::plain("Poll Ended"), eid.to_owned());
                     let content = AnyMessageLikeEventContent::PollEnd(end);
                     if let Err(e) = room.send(content).await {
                          log::error!("Failed to end poll: {:?}", e);
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
