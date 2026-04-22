/// Helper functions for formatting Matrix messages as HTML for Pidgin/libpurple.
use ammonia::Builder;
use std::collections::{HashSet, HashMap};
use once_cell::sync::Lazy;
use regex::Regex;

pub fn style_reply(quoted_body: &str, reply_body: &str) -> String {
    format!(
        "<blockquote style=\"border-left: 3px solid #ccc; padding-left: 8px; margin-left: 5px; color: #666; font-size: 90%;\">{}</blockquote><br/>{}",
        quoted_body,
        reply_body
    )
}

pub fn style_edit(body: &str) -> String {
    format!(
        "{} <span style=\"color: #888; font-size: 80%;\">(edited)</span>",
        body
    )
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

/// Sanitizes HTML while preserving basic formatting.
/// Uses ammonia for production-grade security.
pub fn sanitize_matrix_html(input: &str) -> String {
    static AMMONIA: Lazy<Builder> = Lazy::new(|| {
        let mut builder = Builder::default();
        
        let tags: HashSet<&str> = HashSet::from_iter([
            "b", "i", "u", "s", "strong", "em", "strike", "del", "blockquote", 
            "p", "br", "span", "a", "code", "pre", "font", "hr", "ul", "ol", "li"
        ]);
        
        let mut tag_attrs: HashMap<&str, HashSet<&str>> = HashMap::new();
        tag_attrs.insert("a", HashSet::from_iter(["href"]));
        tag_attrs.insert("span", HashSet::from_iter(["style"]));
        tag_attrs.insert("blockquote", HashSet::from_iter(["style"]));
        tag_attrs.insert("code", HashSet::from_iter(["style"]));
        tag_attrs.insert("font", HashSet::from_iter(["color", "size", "face"]));

        builder
            .tags(tags)
            .tag_attributes(tag_attrs)
            .allowed_classes(HashMap::new())
            .link_rel(None);
        builder
    });

    let sanitized = AMMONIA.clean(input).to_string();
    // Strip CSS expression() — an IE 5/6 XSS vector that ammonia does not filter.
    // Although Pidgin's GTK renderer does not execute it, we remove it defensively
    // in case the output is ever used in another rendering context.
    if sanitized.contains("expression(") {
        static CSS_EXPR: Lazy<Regex> = Lazy::new(|| {
            Regex::new(r"(?i)expression\s*\(").expect("valid CSS expression regex")
        });
        CSS_EXPR.replace_all(&sanitized, "/*removed*/").into_owned()
    } else {
        sanitized
    }
}
