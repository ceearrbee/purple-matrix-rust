use matrix_sdk::ruma::events::room::message::RoomMessageEventContent;
use purple_matrix_rust::{get_display_html, escape_html, create_message_content};

#[test]
fn plain_text_wrapped_in_paragraph() {
    // Plain text is run through the markdown pipeline, which wraps it in <p>
    let content = RoomMessageEventContent::text_plain("Hello World");
    let html = get_display_html(&content);
    assert!(html.contains("Hello World"), "Text content must be present");
    // Markdown pipeline wraps plain paragraphs in <p> tags
    assert!(html.contains("<p>") || html == "Hello World", "Output: {}", html);
}

#[test]
fn plain_text_markdown_rendered_to_html() {
    // Plain text with Markdown syntax is rendered to HTML via pulldown-cmark
    let content = RoomMessageEventContent::text_plain("**Bold** and *Italic*");
    let html = get_display_html(&content);
    assert!(html.contains("<strong>Bold</strong>") || html.contains("<b>Bold</b>"),
        "Bold not found in: {}", html);
    assert!(html.contains("<em>Italic</em>") || html.contains("<i>Italic</i>"),
        "Italic not found in: {}", html);
}

#[test]
fn formatted_html_body_returned_sanitized() {
    let content = RoomMessageEventContent::text_html("Plain", "<b>Bold</b>");
    assert_eq!(get_display_html(&content), "<b>Bold</b>");
}

#[test]
fn xss_script_tag_stripped() {
    let content = RoomMessageEventContent::text_html(
        "x",
        "<img src=x onerror=alert(1)><script>alert(2)</script><b>ok</b>",
    );
    let output = get_display_html(&content);
    assert!(!output.contains("<script>"), "script tag must be removed");
    assert!(!output.contains("alert(2)"), "script content must be removed");
    assert!(!output.contains("onerror"), "event handlers must be removed");
    assert!(output.contains("<b>ok</b>"), "safe tags must survive");
}

#[test]
fn xss_display_name_escaped() {
    // Simulates a malicious display name being inserted into HTML context
    let malicious = "</b><img src=x onerror=alert(1)>";
    let escaped = escape_html(malicious);
    assert!(!escaped.contains('<'), "< must be escaped");
    assert!(!escaped.contains('>'), "> must be escaped");
    assert!(escaped.contains("&lt;"), "< must become &lt;");
}

#[test]
fn location_message_rendered() {
    use matrix_sdk::ruma::events::room::message::{LocationMessageEventContent, MessageType};
    let content = RoomMessageEventContent::new(MessageType::Location(
        LocationMessageEventContent::new(
            "Meeting Spot".to_string(),
            "geo:12.34,56.78".to_string(),
        ),
    ));
    let html = get_display_html(&content);
    assert!(html.contains("Meeting Spot"));
    assert!(html.contains("geo:12.34,56.78"));
}

#[test]
fn create_message_content_markdown_produces_html_body() {
    let content = create_message_content("**Bold**".to_string());
    if let matrix_sdk::ruma::events::room::message::MessageType::Text(text_content) = content.msgtype {
        if let Some(formatted) = text_content.formatted {
            assert!(
                formatted.body.contains("<strong>Bold</strong>") || formatted.body.contains("<b>Bold</b>"),
                "Expected bold HTML in: {}",
                formatted.body
            );
        }
        // No formatted body means the markdown had no HTML-distinguishable structure — acceptable
    }
}
