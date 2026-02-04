# Implementation Plan: Fully Featured Matrix Plugin for Libpurple (Rust Backend)

## 1. Executive Summary
This document outlines the architectural roadmap to transform the current `purple-matrix-rust` prototype into a production-grade Matrix client. The implementation leverages `matrix-sdk` (Rust) for heavy lifting (encryption, state management, syncing) and exposes functionality to `libpurple` (C) via a thin FFI layer.

**Core Philosophy:** "Rust for logic, C for UI/Legacy Glue."

---

## 2. Architecture & Data Flow

### 2.1. The Rust Backend (`src/lib.rs`)
*   **Singleton Pattern:** Maintain a global `Lazy<Mutex<Option<Client>>>` (already present) but expand it to handle `Arc<Client>` for better async shared ownership.
*   **Async Runtime:** Use `tokio` for the internal loop.
*   **Event Channel:** Create a `mpsc::channel`. The Rust sync loop pushes events to this channel. A separate thread (or `g_idle_add`) pops events and executes C callbacks on the main thread. **Crucial for performance and preventing segfaults.**

### 2.2. The C Frontend (`plugin.c`)
*   **Signal Handlers:** Map Rust callbacks to `libpurple` signals (`received-im-msg`, `blist-node-added`, etc.).
*   **Data Marshalling:** Minimal logic. Receive raw pointers from Rust, convert to `char*` or GLib types, update UI, free memory.

---

## 3. Implementation Phases

### Phase 1: Robust Core & Stability (Current Status + Immediate Fixes)
**Goal:** A stable client that stays connected, populates the buddy list correctly, and handles text messages reliably.

1.  **Refine Contact List (Done/In-progress):**
    *   Ensure `find_chat_manual` works reliably (Completed).
    *   Handle `RoomMember` updates (Join/Leave events) to update the buddy list dynamically.
2.  **Robust Sync Loop:**
    *   Implement "Sliding Sync" (if supported by HS) or optimized `SyncSettings` filter to reduce bandwidth.
    *   Auto-reconnect logic (handle `429 Too Many Requests` backoff in Rust).
3.  **Typing & Presence:**
    *   Finalize `purple_matrix_rust_set_status` using `client.send(PresenceRequest)`.
    *   Throttle outgoing typing notifications to avoid API spam.

### Phase 2: End-to-End Encryption (E2EE) - **Critical**
**Goal:** Seamless encryption support. This is the hardest part to get right in a legacy UI.

1.  **Enable Feature:**
    *   Update `Cargo.toml` to enable `e2e-encryption`, `sqlite`, and `automatic-room-key-forwarding`.
2.  **Store Persistence:**
    *   Ensure `SqliteCryptoStore` is initialized with a secure passphrase (derived from account password or separate input).
3.  **Decryption Flow:**
    *   In the sync loop, `matrix-sdk` handles decryption automatically.
    *   **Edge Case:** If a message is `UnableToDecrypt`, return a specific error string to libpurple: `[Encrypted Message: Waiting for keys...]`.
    *   Implement a "Key Request" retry mechanism (Rust background task).
4.  **Verification (SAS - Emoji):**
    *   *Challenge:* Libpurple has no UI for Emoji comparison.
    *   *Solution:* Use a "System Chat" (a fake chat window named `Matrix Console`).
    *   When verification starts, print the emojis as text in the Console.
    *   User types `/verify match` or `/verify mismatch` commands which map to Rust FFI calls.

### Phase 3: Rich Media & UX
**Goal:** Make the chat feel modern.

1.  **Inline Images:**
    *   **Receive:** Rust downloads media -> Saves to temp file -> Calls C callback with file path -> C uses `<img src="...">` in HTML message buffer.
    *   **Send:** Intercept `write_im` in C. If it's an image paste, pass the buffer to Rust. Rust uploads -> sends `m.image` event.
2.  **Formatted Text:**
    *   Implement a Markdown <-> HTML converter in Rust (using `pulldown-cmark`).
    *   Matrix uses HTML. Libpurple uses a subset of HTML. Need a sanitization layer in Rust to ensure incoming Matrix HTML doesn't break Libpurple.
3.  **Read Receipts:**
    *   Listen for `conversation-displayed` signal in C.
    *   Call Rust `client.get_room(id).read_receipt(event_id)`.

### Phase 4: Threading & Replies
**Goal:** Map Matrix's DAG to a linear timeline.

1.  **Replies:**
    *   Detect `m.relates_to` in incoming events.
    *   Render as a blockquote: `> User said: ...`.
    *   Implement `reply_to` in sending: User selects "Reply" in Pidgin (if available) or right-clicks -> "Reply". We might need a custom slash command `/reply [text]` if UI support is lacking.
2.  **Threads:**
    *   *Option A (Flatten):* Render thread messages in the main chat with a `[Thread]` prefix.
    *   *Option B (Pop-out):* Detect `m.thread` relation. Create a *temporary* new Chat in Buddy List named `Room Name > Thread`.

### Phase 5: Spaces (Hierarchy)
**Goal:** Organize the cluttered Buddy List.

1.  **Map to PurpleGroups:**
    *   Matrix `Space` -> Purple `Group`.
    *   When syncing rooms, check their parent Space.
    *   If a room is in "Space A", add the buddy to Group "Space A".
    *   *Complexity:* Rooms can be in multiple spaces. Pick the canonical one or the first one found.

---

## 4. Technical Specifications & FFI Plan

### 4.1. Cargo.toml Features
```toml
[dependencies]
matrix-sdk = { version = "0.7", features = ["e2e-encryption", "sqlite", "markdown", "socks"] }
# "socks" needed for Tor support later
# "image" crate for thumbnail generation
```

### 4.2. Rust Structs (`src/lib.rs`)

```rust
// Enhanced handle to hold state
pub struct PluginState {
    client: Client,
    runtime: Runtime,
    // Channel to send commands back to C if needed
    command_tx: mpsc::Sender<CCommand>,
}

// Enum for events sending to C
pub enum MatrixEvent {
    Message { room_id: String, sender: String, content: String, is_encrypted: bool },
    Typing { room_id: String, user_id: String, active: bool },
    RoomJoined { room_id: String, name: String, category: Option<String> }, // Category = Space
    VerificationRequest { transaction_id: String, emojis: Vec<String> },
}
```

### 4.3. C FFI Surface (`plugin.c`)

We need to expand the FFI exports:

```c
// New exports required
void purple_matrix_rust_verify_sas_match(const char *transaction_id);
void purple_matrix_rust_send_read_receipt(const char *room_id, const char *event_id);
void purple_matrix_rust_upload_file(const char *room_id, const char *path);
```

---

## 5. Security & Performance Checklist

### Security
*   [ ] **Secret Storage:** Do NOT store the access token in plain text in `accounts.xml` if possible. Use `libsecret` (via libpurple) or the OS keyring.
*   [ ] **Sanitization:** Strictly sanitize all HTML coming from Rust before handing it to `libpurple` to prevent injection attacks in the chat window.
*   [ ] **Verification:** Never auto-trust devices. Force the SAS flow.

### Performance
*   [ ] **Batching:** When the buddy list initially loads, do not fire 500 `purple_blist_add_chat` calls instantly. Batch them or use `g_idle_add` to space them out so the UI doesn't freeze (Solved partially by current callback approach, but needs tuning).
*   [ ] **Lazy Loading:** Do not fetch member lists for all rooms on startup. Fetch them only when the chat window is opened (`conversation-created` signal).

---

## 6. Action Plan for Developer (Next 5 Steps)

1.  **Fix Status:** Implement `purple_matrix_rust_set_status` using `ruma` request construction (as discussed).
2.  **Enable E2EE:** Turn on the feature flag in Cargo.toml and handle the `SyncRoomMessageEvent` when it is `Redacted` or `UnableToDecrypt`.
3.  **HTML Parsing:** Import `pulldown-cmark` in Rust. In `process_msg_cb`, render markdown to HTML strings before sending to C.
4.  **Verification Command:** Implement a `/verify` hook in C `cmd_verify` that calls Rust to confirm SAS keys.
5.  **Spaces:** Update `process_room_cb` to look up the room's parent Space in Rust and pass it as the `Group` name to C.
