use std::os::raw::{c_char};
use std::ffi::CStr;
use crate::ffi::{with_client};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_poll(user_id: *const c_char, room_id: *const c_char, question: *const c_char, options: *const *const c_char, options_count: i32) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || question.is_null() || options.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let question_str = unsafe { CStr::from_ptr(question).to_string_lossy().into_owned() };
        
        let mut answers = Vec::new();
        for i in 0..options_count {
            let opt = unsafe { *options.offset(i as isize) };
            if !opt.is_null() {
                answers.push(unsafe { CStr::from_ptr(opt).to_string_lossy().into_owned() });
            }
        }

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::poll::start::{PollStartEventContent, PollAnswers, PollAnswer, PollContentBlock};
                use matrix_sdk::ruma::events::message::TextContentBlock;

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                         let poll_answers_vec: Vec<PollAnswer> = answers.iter()
                             .enumerate()
                             .map(|(i, text)| PollAnswer::new(i.to_string(), TextContentBlock::plain(text.clone())))
                             .collect();

                         if let Ok(poll_answers) = PollAnswers::try_from(poll_answers_vec) {
                             let question_text = TextContentBlock::plain(question_str.clone());
                             let poll = PollContentBlock::new(question_text.clone(), poll_answers);
                             let content = PollStartEventContent::new(question_text, poll);
                             let _ = room.send(content).await;
                         }
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_vote_poll(user_id: *const c_char, room_id: *const c_char, poll_id: *const c_char, option_index: i32) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || poll_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let poll_id_str = unsafe { CStr::from_ptr(poll_id).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomId, EventId};
                use matrix_sdk::ruma::events::poll::response::PollResponseEventContent;

                if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let response = PollResponseEventContent::new(vec![option_index.to_string()].into(), eid.to_owned());
                        let _ = room.send(response).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_end_poll(user_id: *const c_char, room_id: *const c_char, poll_id: *const c_char, text: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || poll_id.is_null() || text.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let poll_id_str = unsafe { CStr::from_ptr(poll_id).to_string_lossy().into_owned() };
        let text_str = unsafe { CStr::from_ptr(text).to_string_lossy().into_owned() };

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::{RoomId, EventId};
                use matrix_sdk::ruma::events::poll::end::PollEndEventContent;
                use matrix_sdk::ruma::events::message::TextContentBlock;

                if let (Ok(rid), Ok(eid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let end_content = PollEndEventContent::new(TextContentBlock::plain(text_str), eid.to_owned());
                        let _ = room.send(end_content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_active_polls(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let user_id_str_clone = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            let client_clone = client.clone();
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::AnySyncTimelineEvent;
                use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client_clone.get_room(rid) {
                        log::info!("Fetching active polls for room {}", room_id_str);
                        
                        let mut options = matrix_sdk::room::MessagesOptions::backward();
                        options.limit = matrix_sdk::ruma::uint!(100);
                        
                        if let Ok(resp) = room.messages(options).await {
                            for event in resp.chunk {
                                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::PollStart(poll))) = event.raw().deserialize() {
                                    if let Some(o) = poll.as_original() {
                                        // Fix: use find_plain() if plain() is associated
                                        let question = o.content.text.find_plain().map(|s| s.to_owned()).unwrap_or_else(|| "Poll".to_string());
                                        let options: Vec<String> = o.content.poll.answers.iter()
                                            .map(|a| a.text.find_plain().map(|s| s.to_owned()).unwrap_or_else(|| "Option".to_string()))
                                            .collect();
                                        let options_str = options.join(", ");

                                        let event = crate::ffi::FfiEvent::PollList {
                                            user_id: user_id_str_clone.clone(),
                                            room_id: room_id_str.clone(),
                                            event_id: Some(event.event_id().unwrap().to_string()),
                                            question: Some(question.to_string()),
                                            sender: Some(o.sender.to_string()),
                                            options_str: Some(options_str),
                                        };
                                        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                                    }
                                }
                            }
                        }

                        // End of list marker
                        let event = crate::ffi::FfiEvent::PollList {
                            user_id: user_id_str_clone.clone(),
                            room_id: room_id_str.clone(),
                            event_id: None,
                            question: None,
                            sender: None,
                            options_str: None,
                        };
                        let _ = crate::ffi::EVENTS_CHANNEL.0.send(event);
                    }
                }
            });
        });
    })
}
