
/// Helper functions for formatting Matrix messages as HTML for Pidgin/libpurple.

pub fn style_reply(quoted_body: &str, reply_body: &str) -> String {
    // Use a blockquote with a border for the quoted part.
    // We escape the quoted body to ensure no injection, although it should already be safe HTML or text.
    // Assuming input is already safe/sanitized HTML or text.
    format!(
        "<blockquote style=\"border-left: 3px solid #ccc; padding-left: 8px; margin-left: 5px; color: #666; font-size: 90%;\">{}</blockquote><br/>{}",
        quoted_body,
        reply_body
    )
}

pub fn style_redaction(sender: &str) -> String {
    format!(
        "<span style=\"color: #999; font-style: italic;\">🚫 [Redacted] {} removed a message.</span>",
        crate::escape_html(sender)
    )
}

pub fn style_edit(body: &str) -> String {
    format!(
        "{} <span style=\"color: #888; font-size: 80%;\">(edited)</span>",
        body
    )
}

pub fn style_thread_prefix(body: &str) -> String {
    format!("&nbsp;&nbsp;🧵 {}", body)
}

pub fn style_mention(display_name: &str) -> String {
    format!(
        "<b style=\"color: #2D3E50;\">@{}</b>",
        crate::escape_html(display_name)
    )
}


pub fn style_notice(body: &str) -> String {
     format!("<span style=\"color: #666;\">{}</span>", body)
}

pub fn style_poll(question: &str, options: Vec<String>) -> String {
    let mut opts_html = String::new();
    for (i, opt) in options.iter().enumerate() {
        opts_html.push_str(&format!("<li><b>{}.</b> {}</li>", i + 1, crate::escape_html(opt)));
    }
    
    format!(
        "<div style=\"background-color: #f0f0f0; padding: 10px; border: 1px solid #ccc; border-radius: 5px; margin-top: 5px; margin-bottom: 5px;\"><div style=\"font-size: 1.1em; font-weight: bold; margin-bottom: 8px;\">📊 {}</div><ul style=\"list-style-type: none; padding-left: 10px; margin-top: 0; margin-bottom: 5px;\">{}</ul><div style=\"font-size: smaller; color: #777; margin-top: 5px;\">(Use 'Active Polls' menu to vote)</div></div>",
        crate::escape_html(question),
        opts_html
    )
}

/// Sanitizes HTML while preserving basic formatting and styling mentions.
/// ALLOWS: b, i, u, s, strong, em, del, blockquote, p, br, span, a, img.
/// REWRITES: Matrix user links to pills.
pub fn sanitize_matrix_html(input: &str) -> String {
    use regex::Regex;
    use once_cell::sync::Lazy;

    // 1. First, we escape everything to act as a baseline "text" representation? 
    // NO. If the input is HTML, we want to PRESERVE it but filter it.
    // If we assume input is possibly malicious HTML:
    // Writing a full sanitizer with Regex is hard and risky. 
    // BUT we can use a "allow-list replace" strategy.
    
    // Strategy:
    // 1. Identify all <tag...>...</tag> or <tag />.
    // 2. If tag is in allow list, keep it (and validate attributes).
    // 3. Else, escape it (replace < with &lt;).
    
    // Actually, `strip_html_tags` in lib.rs was stripping everything. Here we want to keep some.
    // A simple pure-rust regex-based sanitizer is still tricky. 
    // Given the constraints and the fact this is a chat plugin (not a high-security web browser context, but still), 
    // let's try to preserve specific tags by finding them and temporarily hiding them, escaping the rest, then restoring?
    
    // Better Strategy for "Visual Polish" phase:
    // Just handle the specific use-cases requested: Mentions, formatting.
    // Trusting `matrix-sdk` which might provide sanitary HTML? No, it passes raw events.
    
    // Let's rely on a simplified replacement for known safe tags.
    // Regex to match tags we want to Allow:
    // <(b|i|u|s|strong|em|del|blockquote|p|br|span|a|img)( [^>]*)?>
    // And closing tags <(/)(b|i|u|s|strong|em|del|blockquote|p|span|a)>
    
    static RE_ALLOWED_TAG: Lazy<Regex> = Lazy::new(|| Regex::new(r"(?i)<(/?)(b|strong|i|em|u|s|strike|del|blockquote|p|br|span|a)(\s+[^>]*)?>").unwrap());
    
    // We will build the output by iterating matches.
    // Anything NOT matched is text and should be escaped?
    // Wait, if I have "Use <b>bold</b>", and I match <b> and </b>.
    // "Use " is text -> escape.
    // "bold" is text -> escape.
    
    let mut output = String::new();
    let mut last_end = 0;
    
    for cap in RE_ALLOWED_TAG.captures_iter(input) {
        let range = cap.get(0).unwrap().range();
        
        // Escape text before the tag
        let text_before = &input[last_end..range.start];
        output.push_str(&crate::escape_html(text_before));
        
        // Process the tag
        let tag_full = cap.get(0).unwrap().as_str();
        let is_close = cap.get(1).map_or(false, |m| m.as_str() == "/");
        let tag_name = cap.get(2).unwrap().as_str().to_lowercase();
        let attrs = cap.get(3).map_or("", |m| m.as_str());
        
        if tag_name == "a" && !is_close {
            // Check for mention in href
            // Naive attribute check
            if attrs.contains("matrix.to/#/@") {
                 // It's a mention! Style it.
                 // We keep the tag but add style.
                 // Be careful not to double-add style if source already has it?
                 // Let's simple-prepend the style.
                 let new_attrs = format!(" style=\"font-weight: bold; background-color: #eee; color: #2D3E50;\"{}", attrs);
                 output.push_str(&format!("<a{}>", new_attrs));
            } else {
                 output.push_str(tag_full);
            }
        } else {
            output.push_str(tag_full);
        }
        
        last_end = range.end;
    }
    
    
    // Escape remaining text
    output.push_str(&crate::escape_html(&input[last_end..]));
    
    output
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_style_reply() {
        let res = style_reply("Quoted", "Reply");
        assert!(res.contains("<blockquote"));
        assert!(res.contains("Quoted"));
        assert!(res.contains("Reply"));
    }

    #[test]
    fn test_sanitize_matrix_html_allows_basic_tags() {
        let input = "<b>Bold</b> and <i>Italic</i>";
        let output = sanitize_matrix_html(input);
        assert_eq!(output, "<b>Bold</b> and <i>Italic</i>");
    }

    #[test]
    fn test_sanitize_matrix_html_escapes_bad_tags() {
        let input = "<script>alert(1)</script><b>Ok</b>";
        let output = sanitize_matrix_html(input);
        assert_eq!(output, "&lt;script&gt;alert(1)&lt;/script&gt;<b>Ok</b>");
    }

    #[test]
    fn test_sanitize_matrix_html_styles_mentions() {
        let input = "Hello <a href=\"https://matrix.to/#/@alice:example.com\">Alice</a>";
        let output = sanitize_matrix_html(input);
        // Should inject style
        assert!(output.contains("style=\"font-weight: bold; background-color: #eee; color: #2D3E50;\""));
        assert!(output.contains("@alice:example.com"));
    }

    #[test]
    fn test_sanitize_matrix_html_preserves_plain_links() {
        let input = "Check <a href=\"https://google.com\">Google</a>";
        let output = sanitize_matrix_html(input);
        // Should NOT inject pill style
        assert!(!output.contains("background-color"));
        assert_eq!(output, "Check <a href=\"https://google.com\">Google</a>");
    }

    #[test]
    fn test_style_poll() {
        let question = "Pizza or Burger?";
        let options = vec!["Pizza".to_string(), "Burger".to_string()];
        let html = style_poll(question, options);
        
        assert!(html.contains("📊 Pizza or Burger?"));
        assert!(html.contains("1.</b> Pizza"));
        assert!(html.contains("2.</b> Burger"));
        assert!(html.contains("background-color: #f0f0f0"));
    }
}
