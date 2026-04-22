/// XSS / HTML injection security tests for the sanitization pipeline.
///
/// Each test case represents a real-world XSS vector or class of injection.
/// All inputs that reach the sanitizer originate from untrusted Matrix server data.
use purple_matrix_rust::sanitize_untrusted_html;

// ---------- helpers ----------

fn must_not_execute(output: &str) {
    assert!(!output.contains("<script"), "script open tag in: {output}");
    assert!(!output.contains("javascript:"), "javascript: URI in: {output}");
    assert!(!output.contains("onerror="), "onerror handler in: {output}");
    assert!(!output.contains("onload="), "onload handler in: {output}");
    assert!(!output.contains("onclick="), "onclick handler in: {output}");
    assert!(!output.contains("onfocus="), "onfocus handler in: {output}");
    assert!(!output.contains("onmouseover="), "onmouseover handler in: {output}");
    assert!(!output.contains("onerror ="), "onerror (space) handler in: {output}");
    assert!(!output.contains("ONERROR"), "ONERROR (uppercase) in: {output}");
}

// ---------- classic script injection ----------

#[test]
fn script_tag_removed() {
    let out = sanitize_untrusted_html("<script>alert(1)</script><b>ok</b>");
    must_not_execute(&out);
    assert!(out.contains("<b>ok</b>"));
}

#[test]
fn script_tag_uppercase() {
    let out = sanitize_untrusted_html("<SCRIPT>alert(1)</SCRIPT><i>fine</i>");
    must_not_execute(&out);
    assert!(out.contains("<i>fine</i>"));
}

#[test]
fn script_in_attribute() {
    let out = sanitize_untrusted_html(r#"<a href="javascript:alert(1)">click</a>"#);
    assert!(!out.contains("javascript:"), "javascript: href not removed in: {out}");
}

// ---------- event handler injection ----------

#[test]
fn img_onerror() {
    let out = sanitize_untrusted_html(r#"<img src=x onerror=alert(1)>"#);
    must_not_execute(&out);
    // img is not in the allow-list so the entire tag should be gone
    assert!(!out.contains("<img"), "img tag should be stripped in: {out}");
}

#[test]
fn svg_onload() {
    let out = sanitize_untrusted_html(r#"<svg onload=alert(1)></svg>"#);
    must_not_execute(&out);
}

#[test]
fn body_onload() {
    let out = sanitize_untrusted_html(r#"<body onload=alert(1)>text</body>"#);
    must_not_execute(&out);
    assert!(out.contains("text"), "text content must survive in: {out}");
}

// ---------- tag/attribute confusion attacks ----------

#[test]
fn nested_quotes_attribute() {
    // Attempts to break out of a quoted attribute
    let out = sanitize_untrusted_html(r#"<span style="color:red" onmouseover="alert(1)">x</span>"#);
    must_not_execute(&out);
    // span is allowed but the injected event attribute must be stripped
    assert!(out.contains("x"), "text content must survive in: {out}");
}

#[test]
fn style_with_expression() {
    // CSS expression() is an IE XSS vector; ammonia should strip or neutralise it
    let out = sanitize_untrusted_html(r#"<span style="color:expression(alert(1))">x</span>"#);
    // We require that the dangerous expression is not present verbatim
    assert!(!out.contains("expression("), "CSS expression() in: {out}");
}

#[test]
fn iframe_removed() {
    let out = sanitize_untrusted_html(r#"<iframe src="https://evil.example/"></iframe>"#);
    assert!(!out.contains("<iframe"), "iframe must be stripped in: {out}");
}

#[test]
fn object_embed_removed() {
    let out = sanitize_untrusted_html(r#"<object data="evil.swf"></object><embed src="evil.swf">"#);
    assert!(!out.contains("<object"), "object must be stripped in: {out}");
    assert!(!out.contains("<embed"), "embed must be stripped in: {out}");
}

// ---------- safe content preservation ----------

#[test]
fn bold_italic_preserved() {
    let out = sanitize_untrusted_html("<b>bold</b> and <i>italic</i>");
    assert_eq!(out, "<b>bold</b> and <i>italic</i>");
}

#[test]
fn blockquote_preserved() {
    let out = sanitize_untrusted_html(r#"<blockquote style="border-left:3px solid #ccc">quote</blockquote>"#);
    assert!(out.contains("<blockquote"), "blockquote must be kept in: {out}");
    assert!(out.contains("quote"), "text must survive in: {out}");
}

#[test]
fn code_block_preserved() {
    let out = sanitize_untrusted_html("<pre><code>fn main() {}</code></pre>");
    assert!(out.contains("fn main()"), "code must survive in: {out}");
}

#[test]
fn hyperlink_href_preserved() {
    let out = sanitize_untrusted_html(r#"<a href="https://matrix.org">Matrix</a>"#);
    assert!(out.contains(r#"href="https://matrix.org""#) || out.contains("href='https://matrix.org'"),
        "safe href must survive in: {out}");
    assert!(out.contains("Matrix"), "link text must survive in: {out}");
}

#[test]
fn hyperlink_javascript_href_stripped() {
    let out = sanitize_untrusted_html(r#"<a href="javascript:void(0)">click</a>"#);
    assert!(!out.contains("javascript:"), "javascript: scheme must be stripped in: {out}");
}

// ---------- display name injection (the vector we fixed) ----------

#[test]
fn display_name_html_escaped() {
    // Simulates what happens before inserting a display name into HTML context
    let malicious_name = r#"</b><img src=x onerror=alert(1)><b>"#;
    let escaped = purple_matrix_rust::escape_html(malicious_name);
    // Must contain no raw HTML tags
    assert!(!escaped.contains('<'), "< not escaped in display name: {escaped}");
    assert!(!escaped.contains('>'), "> not escaped in display name: {escaped}");
    assert!(escaped.contains("&lt;"), "expected &lt; in: {escaped}");
    assert!(escaped.contains("&gt;"), "expected &gt; in: {escaped}");
}

#[test]
fn display_name_script_escaped() {
    let name = "<script>alert(document.cookie)</script>";
    let escaped = purple_matrix_rust::escape_html(name);
    assert!(!escaped.contains("<script"), "script must be escaped in: {escaped}");
}

// ---------- system message injection (server-controlled user IDs / topics) ----------

#[test]
fn system_message_topic_escaped() {
    // A malicious topic set on the server must not inject HTML into system messages
    let malicious_topic = r#"</b><script>alert(1)</script><b>"#;
    let escaped = purple_matrix_rust::escape_html(malicious_topic);
    assert!(!escaped.contains("<script"), "script in escaped topic: {escaped}");
    assert!(!escaped.contains("</b>"), "raw </b> in escaped topic: {escaped}");
}

#[test]
fn system_message_user_id_escaped() {
    // Hypothetical malicious user ID
    let malicious_id = "@<img onerror=alert(1)>:example.com";
    let escaped = purple_matrix_rust::escape_html(malicious_id);
    assert!(!escaped.contains("<img"), "img in escaped user_id: {escaped}");
    assert!(escaped.contains("&lt;"), "expected &lt; in: {escaped}");
}

// ---------- entity / encoding tricks ----------

#[test]
fn null_byte_in_input() {
    // Null bytes should not crash or cause truncation issues
    let input = "hello\x00<script>alert(1)</script>world";
    let out = sanitize_untrusted_html(input);
    must_not_execute(&out);
    // Text before null should survive; text after may be dropped depending on C-string semantics
    // but the sanitizer itself must not crash
}

#[test]
fn unicode_confusable_tag() {
    // Some sanitizers can be confused by fullwidth < >
    let out = sanitize_untrusted_html("\u{FF1C}script\u{FF1E}alert(1)\u{FF1C}/script\u{FF1E}");
    // Full-width characters are not HTML, so no execution possible; just must not crash
    must_not_execute(&out);
}
