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
                         
                         let mut fallback_body = format!("📊 POLL: {}\n", question_str);
                         for (i, ans) in answers.iter().enumerate() {
                             fallback_body.push_str(&format!("{}. {}\n", i + 1, ans));
                         }
                         fallback_body.push_str("\n(Use a Matrix client that supports polls to vote)");

                         use matrix_sdk::ruma::events::room::message::{RoomMessageEventContent, MessageType, TextMessageEventContent};
                         use matrix_sdk::ruma::events::poll::start::PollStartEventContent;
                         
                         let mut content = RoomMessageEventContent::new(MessageType::Text(TextMessageEventContent::plain(fallback_body)));
                         // SDK 0.16.0 might not have Relation::PollStart yet in the helper, 
                         // but we can try to send it as a custom event if the above is insufficient.
                         // Actually, let's try sending it as a PollStart event but ensure it's fully spec-compliant.
                         
                         use matrix_sdk::ruma::events::AnyMessageLikeEventContent;
                         let poll_content = PollStartEventContent::new(TextContentBlock::plain(question_str), poll);
                         let any_content = AnyMessageLikeEventContent::PollStart(poll_content);

                         let _ = room.send(any_content).await;
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
        let client_clone = client.clone();
        RUNTIME.spawn(async move {
            use matrix_sdk::ruma::RoomId;
            use matrix_sdk::room::MessagesOptions;
            use matrix_sdk::ruma::events::AnySyncTimelineEvent;
            use std::ffi::CString;

            if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                if let Some(room) = client_clone.get_room(rid) {
                    log::info!("Fetching active polls for room {}", room_id_str);
                    
                    // Simple discovery: scan last 50 messages for PollStart events
                    let mut options = MessagesOptions::backward();
                    options.limit = 50u32.into();
                    
                    if let Ok(messages) = room.messages(options).await {
                        for timeline_event in messages.chunk {
                            if let Ok(event) = timeline_event.raw().deserialize() {
                                if let AnySyncTimelineEvent::MessageLike(matrix_sdk::ruma::events::AnySyncMessageLikeEvent::PollStart(poll_event)) = event {
                                    if let Some(ev) = poll_event.as_original() {
                                        let question = ev.content.poll.question.text.find_plain().unwrap_or("Poll");
                                        let options: Vec<String> = ev.content.poll.answers.iter().map(|a| a.text.find_plain().unwrap_or("Option").to_string()).collect();
                                        let options_str = options.join(", ");
                                        
                                        if let (Ok(c_room_id), Ok(c_event_id), Ok(c_sender), Ok(c_question), Ok(c_options)) =
                                           (CString::new(room_id_str.clone()), CString::new(ev.event_id.as_str()), CString::new(ev.sender.as_str()), CString::new(question), CString::new(options_str))
                                        {
                                            let guard = POLL_LIST_CALLBACK.lock().unwrap();
                                            if let Some(cb) = *guard {
                                                cb(c_room_id.as_ptr(), c_event_id.as_ptr(), c_sender.as_ptr(), c_question.as_ptr(), c_options.as_ptr());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    // End of list marker
                    if let Ok(c_room_id) = CString::new(room_id_str) {
                        let guard = POLL_LIST_CALLBACK.lock().unwrap();
                        if let Some(cb) = *guard {
                            cb(c_room_id.as_ptr(), std::ptr::null(), std::ptr::null(), std::ptr::null(), std::ptr::null());
                        }
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
