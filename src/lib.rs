#![recursion_limit = "256"]

use std::sync::Mutex;
use once_cell::sync::Lazy;
use tokio::runtime::Runtime;
use dashmap::{DashSet};

pub mod ffi;

pub mod handlers;
pub mod verification_logic;
pub mod sync_logic;
pub mod media_helper;
pub mod auth;
pub mod grouping;
pub mod html_fmt;

// Global Runtime/Client
pub(crate) static RUNTIME: Lazy<Runtime> = Lazy::new(|| Runtime::new().unwrap_or_else(|e| {
    log::error!("Failed to start Tokio runtime: {}", e);
    std::process::abort();
}));
// Globals are now moved to crate::ffi::mod to avoid duplication.
pub(crate) static ENCRYPTED_ROOMS: Lazy<DashSet<String>> = Lazy::new(DashSet::new);
pub(crate) static DATA_PATH: Lazy<Mutex<Option<std::path::PathBuf>>> = Lazy::new(|| Mutex::new(None));

pub fn send_system_message(user_id: &str, msg: &str) {
    crate::ffi::send_system_message(user_id, msg);
}


#[repr(C)]
pub struct MatrixClientHandle;

pub fn escape_html(input: &str) -> String {    let mut escaped = String::with_capacity(input.len());
    for c in input.chars() {
        match c {
            '&' => escaped.push_str("&amp;"),
            '<' => escaped.push_str("&lt;"),
            '>' => escaped.push_str("&gt;"),
            '"' => escaped.push_str("&quot;"),
            '\'' => escaped.push_str("&#x27;"),
            _ => escaped.push(c),
        }
    }
    escaped
}

pub fn sanitize_untrusted_html(input: &str) -> String {
    // Delegate to our smart sanitizer in html_fmt
    crate::html_fmt::sanitize_matrix_html(input)
}


pub fn create_message_content(text: String) -> matrix_sdk::ruma::events::room::message::RoomMessageEventContent {
    use matrix_sdk::ruma::events::room::message::RoomMessageEventContent;
    use pulldown_cmark::{Parser, Options, html};

    let mut options = Options::empty();
    options.insert(Options::ENABLE_STRIKETHROUGH);
    options.insert(Options::ENABLE_TABLES);
    options.insert(Options::ENABLE_TASKLISTS);

    let parser = Parser::new_ext(&text, options);
    let mut html_output = String::new();
    html::push_html(&mut html_output, parser);

    let html_trimmed = html_output.trim();
    
    // Fall back to plain text if the markdown parser produced nothing distinct
    if html_trimmed != text && (html_trimmed.contains('<') && html_trimmed.contains('>')) {
        RoomMessageEventContent::text_html(text.clone(), html_trimmed.to_string())
    } else {
        RoomMessageEventContent::text_plain(text)
    }
}

// Helper function to render HTML/Markdown
pub fn get_display_html(content: &matrix_sdk::ruma::events::room::message::RoomMessageEventContent) -> String {
    use matrix_sdk::ruma::events::room::message::MessageType;
    use pulldown_cmark::{Parser, Options, html};

    let mut options = Options::empty();
    options.insert(Options::ENABLE_STRIKETHROUGH);
    options.insert(Options::ENABLE_TABLES);
    options.insert(Options::ENABLE_TASKLISTS);

    match &content.msgtype {
        MessageType::Text(text_content) => {
             if let Some(formatted) = &text_content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     return sanitize_untrusted_html(&formatted.body);
                 }
             }
             
             // If no HTML, try rendering markdown
             let parser = Parser::new_ext(&text_content.body, options);
             let mut html_output = String::new();
             html::push_html(&mut html_output, parser);
             
             let trimmed = html_output.trim();
             if trimmed.is_empty() {
                 escape_html(&text_content.body)
             } else {
                 sanitize_untrusted_html(trimmed)
             }
        },
        MessageType::Emote(content) => {
             let body_safe = if let Some(formatted) = &content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     sanitize_untrusted_html(&formatted.body)
                 } else {
                     escape_html(&content.body)
                 }
             } else {
                 let parser = Parser::new_ext(&content.body, options);
                 let mut html_output = String::new();
                 html::push_html(&mut html_output, parser);
                 sanitize_untrusted_html(html_output.trim())
             };
             format!("* {}", body_safe)
        },
        MessageType::Notice(content) => {
             if let Some(formatted) = &content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     return sanitize_untrusted_html(&formatted.body);
                 }
             }
             let parser = Parser::new_ext(&content.body, options);
             let mut html_output = String::new();
             html::push_html(&mut html_output, parser);
             sanitize_untrusted_html(html_output.trim())
        },
        MessageType::Image(image_content) => format!("[Image: {}]", escape_html(&image_content.body)),
        MessageType::Video(video_content) => format!("[Video: {}]", escape_html(&video_content.body)),
        MessageType::Audio(audio_content) => format!("[Audio: {}]", escape_html(&audio_content.body)),
        MessageType::File(file_content) => format!("[File: {}]", escape_html(&file_content.body)),
        MessageType::Location(location_content) => format!(
            "[Location: {}] ({})",
            escape_html(&location_content.body),
            escape_html(location_content.geo_uri.as_str())
        ),
        _ => "[Unsupported Message Type]".to_string(),
    }
}

