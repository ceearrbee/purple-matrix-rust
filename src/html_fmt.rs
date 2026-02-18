/// Helper functions for formatting Matrix messages as HTML for Pidgin/libpurple.

pub fn style_reply(quoted_body: &str, reply_body: &str) -> String {
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
/// ALLOWS: b, i, u, s, strong, em, del, blockquote, p, br, span, a, img, code, pre, font, hr, ul, ol, li.
pub fn sanitize_matrix_html(input: &str) -> String {
    use regex::Regex;
    use once_cell::sync::Lazy;

    static RE_ALLOWED_TAG: Lazy<Regex> = Lazy::new(|| Regex::new(r"(?i)<(/?)(b|strong|i|em|u|s|strike|del|blockquote|p|br|span|a|img|code|pre|font|hr|ul|ol|li)(\s+[^>]*)?>").unwrap());
    
    let mut output = String::new();
    let mut last_end = 0;
    
    for cap in RE_ALLOWED_TAG.captures_iter(input) {
        let range = cap.get(0).unwrap().range();
        
        let text_before = &input[last_end..range.start];
        output.push_str(&crate::escape_html(text_before));
        
        let tag_full = cap.get(0).unwrap().as_str();
        let is_close = cap.get(1).map_or(false, |m| m.as_str() == "/");
        let tag_name = cap.get(2).unwrap().as_str().to_lowercase();
        let attrs = cap.get(3).map_or("", |m| m.as_str());
        
        if tag_name == "a" && !is_close {
            if attrs.contains("matrix.to/#/@") {
                 let new_attrs = format!(" style=\"font-weight: bold; background-color: #eee; color: #2D3E50;\"{}", attrs);
                 output.push_str(&format!("<a{}>", new_attrs));
            } else {
                 output.push_str(tag_full);
            }
        } else if (tag_name == "code" || tag_name == "pre") && !is_close {
            // Give code blocks some visual distinction in Pidgin
            output.push_str(&format!("<{} style=\"font-family: monospace; background-color: #f8f8f8; border: 1px solid #ddd; padding: 2px;\">", tag_name));
        } else if tag_name == "span" && !is_close {
            // Filter span attributes: keep style, but remove title and others that break Pidgin's strict parser
            let mut clean_attrs = String::new();
            for attr in attrs.split_whitespace() {
                if attr.to_lowercase().starts_with("style=") {
                    clean_attrs.push(' ');
                    clean_attrs.push_str(attr);
                }
            }
            output.push_str(&format!("<span{}>", clean_attrs));
        } else {
            output.push_str(tag_full);
        }
        
        last_end = range.end;
    }
    
    output.push_str(&crate::escape_html(&input[last_end..]));
    output
}
