/// Helper functions for formatting Matrix messages as HTML for Pidgin/libpurple.

pub fn style_reply(quoted_body: &str, reply_body: &str) -> String {
    format!(
        "<font color='#777777'><b>[Reply]</b> {}</font><hr/>{}",
        quoted_body,
        reply_body
    )
}

pub fn style_reply_v2(quoted_body: &str, reply_body: &str) -> String {
    // A clean, compact reply style that avoids extra spacing.
    format!(
        "<font color='#777777'><b>[Reply]</b> {}</font><br/>&gt; {}",
        quoted_body,
        reply_body
    )
}

pub fn style_edit(body: &str) -> String {
    format!("{} <font color='#777777' size='1'><i>(edited)</i></font>", body)
}

pub fn style_poll(question: &str, options: Vec<String>) -> String {
    let mut body = format!("<b>Poll: {}</b><br/>", question);
    for (i, opt) in options.iter().enumerate() {
        body.push_str(&format!("{}. {}<br/>", i + 1, opt));
    }
    body
}

pub fn sanitize_matrix_html(input: &str) -> String {
    let mut cleaner = ammonia::Builder::default();
    cleaner
        .tags(
            [
                "b", "i", "u", "s", "strong", "em", "del", "blockquote", "p", "br", "span", "a",
                "img", "code", "pre", "font", "hr", "ul", "ol", "li",
            ]
            .iter()
            .cloned()
            .collect(),
        )
        .link_rel(None)
        .url_schemes(
            ["http", "https", "mailto", "matrix", "mxc"]
                .iter()
                .cloned()
                .collect(),
        );

    cleaner.clean(input).to_string()
}
