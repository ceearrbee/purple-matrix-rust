
use std::sync::Mutex;
use matrix_sdk::Client;
use once_cell::sync::Lazy;
use simple_logger::SimpleLogger;
use tokio::runtime::Runtime;

pub mod ffi;

pub mod handlers;
pub mod verification_logic;
pub mod sync_logic;
pub mod media_helper;
pub mod auth;
pub mod grouping;
pub mod html_fmt;

// Global Runtime/Client
pub(crate) static RUNTIME: Lazy<Runtime> = Lazy::new(|| Runtime::new().unwrap());
// user_id -> Client
pub(crate) static CLIENTS: Lazy<Mutex<std::collections::HashMap<String, Client>>> = Lazy::new(|| Mutex::new(std::collections::HashMap::new()));
pub(crate) static HISTORY_FETCHED_ROOMS: Lazy<Mutex<std::collections::HashSet<String>>> = Lazy::new(|| Mutex::new(std::collections::HashSet::new()));
pub(crate) static PAGINATION_TOKENS: Lazy<Mutex<std::collections::HashMap<String, String>>> = Lazy::new(|| Mutex::new(std::collections::HashMap::new()));
pub(crate) static DATA_PATH: Lazy<Mutex<Option<std::path::PathBuf>>> = Lazy::new(|| Mutex::new(None));
// event_id -> { emoji -> count }
pub(crate) static REACTIONS: Lazy<Mutex<std::collections::HashMap<String, std::collections::HashMap<String, usize>>>> = Lazy::new(|| Mutex::new(std::collections::HashMap::new()));

pub(crate) fn with_client<F, R>(user_id: &str, f: F) -> Option<R>
where
    F: FnOnce(Client) -> R,
{
    let guard = CLIENTS.lock().unwrap();
    if let Some(client) = guard.get(user_id) {
        Some(f(client.clone()))
    } else {
        log::warn!("No client found for user_id: {}", user_id);
        None
    }
}


#[repr(C)]
pub struct MatrixClientHandle {
    _private: [u8; 0],
}

#[no_mangle]
pub extern "C" fn purple_matrix_rust_init() {
    let _ = SimpleLogger::new()
        .with_level(log::LevelFilter::Info)
        .with_module_level("matrix_sdk", log::LevelFilter::Warn)
        .with_module_level("matrix_sdk_crypto", log::LevelFilter::Warn)
        .with_module_level("matrix_sdk_base", log::LevelFilter::Warn)
        .init();
    log::info!("Rust backend initialized");
    
    RUNTIME.spawn(async {
        media_helper::cleanup_media_files().await;
    });
}

pub fn escape_html(input: &str) -> String {
    input
        .replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#x27;")
}

pub fn strip_html_tags(input: &str) -> String {
    let mut output = String::new();
    let mut inside_tag = false;

    // This is a naive implementation. For production, consider using a proper library like `ammonia` or `scraper` if dependencies allowed.
    // For now, simple state machine to strip <...>
    for c in input.chars() {
        if c == '<' {
            inside_tag = true;
        } else if c == '>' {
            inside_tag = false;
        } else if !inside_tag {
            output.push(c);
        }
    }
    
    // Decode common entities
    output = output.replace("&lt;", "<")
                   .replace("&gt;", ">")
                   .replace("&amp;", "&")
                   .replace("&quot;", "\"")
                   .replace("&nbsp;", " ");
                   
    output
}

pub fn sanitize_untrusted_html(input: &str) -> String {
    // Delegate to our smart sanitizer in html_fmt
    crate::html_fmt::sanitize_matrix_html(input)
}


pub(crate) fn create_message_content(text: String) -> matrix_sdk::ruma::events::room::message::RoomMessageEventContent {
    use matrix_sdk::ruma::events::room::message::RoomMessageEventContent;
    use pulldown_cmark::{Parser, Options, html};

    // Always attempt to render as Markdown.
    // pulldown-cmark handles plain text gracefully (it just becomes <p>text</p> or similar, 
    // but usually we want to preserve exact input if it's just text.
    // However, Matrix clients usually expect HTML if `formatted_body` is present.
    
    let mut options = Options::empty();
    options.insert(Options::ENABLE_STRIKETHROUGH);
    options.insert(Options::ENABLE_TABLES);
    options.insert(Options::ENABLE_TASKLISTS);

    let parser = Parser::new_ext(&text, options);
    let mut html_output = String::new();
    html::push_html(&mut html_output, parser);

    // If the HTML output is identical to the input (wrapped in p or not), maybe send as plain?
    // Actually, it's safer to always send both.
    // But we need to be careful not to unnecessarily wrap simple text in <p> if it makes the UI look bad.
    // Many clients strip the outer <p>.
    
    // Let's strip the surrounding <p> if it's the only tag, for cleaner display in some clients?
    // Or just trust the client.
    
    // Ensure we don't treat the input as HTML if it wasn't intended.
    // But the user *wants* markdown.

    // trimming the resulting html often helps
    let html_trimmed = html_output.trim();
    
    // Check if it actually produced any HTML tags differing from text
    // (Simple heuristic: if it contains tags)
    
    if html_trimmed != text && (html_trimmed.contains('<') && html_trimmed.contains('>')) {
        RoomMessageEventContent::text_html(text.clone(), html_trimmed.to_string())
    } else {
        RoomMessageEventContent::text_plain(text)
    }
}

// Helper function to render HTML/Markdown
pub fn get_display_html(content: &matrix_sdk::ruma::events::room::message::RoomMessageEventContent) -> String {
    use matrix_sdk::ruma::events::room::message::MessageType;
    
    match &content.msgtype {
        MessageType::Text(text_content) => {
             // Check if HTML is present
             if let Some(formatted) = &text_content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     return sanitize_untrusted_html(&formatted.body);
                 }
             }
             // Treat plain text as untrusted and escape.
             escape_html(&text_content.body)
        },
        MessageType::Emote(content) => {
             let body_safe = if let Some(formatted) = &content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     sanitize_untrusted_html(&formatted.body)
                 } else {
                     escape_html(&content.body)
                 }
             } else {
                 escape_html(&content.body)
             };
             format!("* {}", body_safe)
        },
        MessageType::Notice(content) => {
             if let Some(formatted) = &content.formatted {
                 if formatted.format == matrix_sdk::ruma::events::room::message::MessageFormat::Html {
                     return sanitize_untrusted_html(&formatted.body);
                 }
             }
             escape_html(&content.body)
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

#[cfg(test)]
mod tests {
    use super::*;
    use matrix_sdk::ruma::events::room::message::RoomMessageEventContent;

    #[test]
    fn test_format_text_message() {
        let content = RoomMessageEventContent::text_plain("Hello World");
        assert_eq!(get_display_html(&content), "Hello World");
    }

    #[test]
    fn test_format_markdown() {
        let content = RoomMessageEventContent::text_plain("**Bold** and *Italic*");
        let html = get_display_html(&content);
        assert_eq!(html, "**Bold** and *Italic*");
    }
    
    #[test]
    fn test_formatted_html_override() {
        let content = RoomMessageEventContent::text_html("Plain", "<b>Bold</b>");
        // We now allow basic HTML!
        assert_eq!(get_display_html(&content), "<b>Bold</b>");
    }

    #[test]
    fn test_formatted_html_sanitizes_script_payload() {
        let content = RoomMessageEventContent::text_html(
            "x",
            "<img src=x onerror=alert(1)><script>alert(2)</script><b>ok</b>",
        );
        // We removed img from allow list (in next step), and script is always escaped.
        // b is allowed.
        // So expected: escaped img, escaped script, kept b.
        // "&lt;img src=x onerror=alert(1)&gt;&lt;script&gt;alert(2)&lt;/script&gt;<b>ok</b>"
        let output = get_display_html(&content);
        assert!(!output.contains("<script>"));
        assert!(output.contains("&lt;script&gt;"));
        assert!(output.contains("<b>ok</b>"));
    }

    #[test]
    fn test_location_rendering() {
        use matrix_sdk::ruma::events::room::message::LocationMessageEventContent;
        let content = RoomMessageEventContent::new(
            matrix_sdk::ruma::events::room::message::MessageType::Location(
                LocationMessageEventContent::new("Meeting Spot".to_string(), "geo:12.34,56.78".to_string())
            )
        );
        let html = get_display_html(&content);
        assert!(html.contains("Meeting Spot"));
        assert!(html.contains("geo:12.34,56.78"));
    }

    #[test]
    fn test_create_message_content_markdown() {
        let content = crate::create_message_content("**Bold**".to_string());
        // Should be HTML
        if let matrix_sdk::ruma::events::room::message::MessageType::Text(text_content) = content.msgtype {
            assert!(text_content.formatted.is_some());
            let formatted = text_content.formatted.unwrap();
            assert!(formatted.body.contains("<strong>Bold</strong>") || formatted.body.contains("<b>Bold</b>"));
        } else {
            panic!("Expected Text message");
        }
    }
}
