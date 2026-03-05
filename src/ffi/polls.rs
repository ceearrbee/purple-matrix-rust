use std::os::raw::c_char;
use std::ffi::CStr;
use crate::ffi::{with_client, send_event, events::FfiEvent};
use crate::RUNTIME;
use matrix_sdk::Client;

#[no_mangle]
pub extern "C" fn purple_matrix_rust_create_poll(user_id: *const c_char, room_id: *const c_char, question: *const c_char, options: *const *const c_char, options_count: i32) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() || question.is_null() || options.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };
        let question_str = unsafe { CStr::from_ptr(question).to_string_lossy().into_owned() };
        
        let mut opts = Vec::new();
        for i in 0..options_count {
            let opt_ptr = unsafe { *options.offset(i as isize) };
            if !opt_ptr.is_null() {
                opts.push(unsafe { CStr::from_ptr(opt_ptr).to_string_lossy().into_owned() });
            }
        }

        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::poll::start::{
                    PollStartEventContent, PollAnswers, PollContentBlock, PollAnswer,
                };
                // In some versions, TextContentBlock is in message module
                use matrix_sdk::ruma::events::message::{TextContentBlock};

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let mut answers_vec = Vec::new();
                        for (i, opt) in opts.into_iter().enumerate() {
                            answers_vec.push(PollAnswer::new(i.to_string(), TextContentBlock::plain(opt)));
                        }
                        
                        let answers = PollAnswers::try_from(answers_vec).unwrap();
                        let poll_block = PollContentBlock::new(TextContentBlock::plain(question_str.clone()), answers);
                        
                        let content = PollStartEventContent::new(TextContentBlock::plain(format!("Poll: {}", question_str)), poll_block);
                        let _ = room.send(content).await;
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

                if let (Ok(rid), Ok(pid)) = (<&RoomId>::try_from(room_id_str.as_str()), <&EventId>::try_from(poll_id_str.as_str())) {
                    if let Some(room) = client.get_room(rid) {
                        let content = PollResponseEventContent::new(vec![option_index.to_string()].into(), pid.to_owned());
                        let _ = room.send(content).await;
                    }
                }
            });
        });
    })
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_list_polls(user_id: *const c_char, room_id: *const c_char) {
    crate::ffi_panic_boundary!({
        if user_id.is_null() || room_id.is_null() { return; }
        let user_id_str = unsafe { CStr::from_ptr(user_id).to_string_lossy().into_owned() };
        let room_id_str = unsafe { CStr::from_ptr(room_id).to_string_lossy().into_owned() };

        let uid_async = user_id_str.clone();
        with_client(&user_id_str, move |client: Client| {
            RUNTIME.spawn(async move {
                use matrix_sdk::ruma::RoomId;
                use matrix_sdk::ruma::events::AnySyncTimelineEvent;
                use matrix_sdk::ruma::events::AnySyncMessageLikeEvent;

                if let Ok(rid) = <&RoomId>::try_from(room_id_str.as_str()) {
                    if let Some(room) = client.get_room(rid) {
                        let mut options = matrix_sdk::room::MessagesOptions::backward();
                        options.limit = matrix_sdk::ruma::uint!(50);
                        if let Ok(resp) = room.messages(options).await {
                            for ev in resp.chunk {
                                if let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::PollStart(poll_ev))) = ev.raw().deserialize() {
                                    if let matrix_sdk::ruma::events::poll::start::SyncPollStartEvent::Original(orig) = poll_ev {
                                        let question = orig.content.poll.question.text.find_plain().unwrap_or("Poll");
                                        let mut opts_str = String::new();
                                        for (i, opt) in orig.content.poll.answers.iter().enumerate() {
                                            if i > 0 { opts_str.push_str(" | "); }
                                            opts_str.push_str(opt.text.find_plain().unwrap_or("Option"));
                                        }

                                        let event = FfiEvent::PollList {
                                            user_id: uid_async.clone(),
                                            room_id: room_id_str.clone(),
                                            event_id: orig.event_id.to_string(),
                                            question: question.to_string(),
                                            sender: orig.sender.to_string(),
                                            options_str: opts_str,
                                        };
                                        send_event(event);
                                    }
                                }
                            }
                        }
                    }
                }
            });
        });
    })
}
