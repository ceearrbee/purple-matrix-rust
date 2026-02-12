# purple-matrix-rust

**A modern, efficient, and feature-rich Matrix plugin for Libpurple (Pidgin, Finch), powered by Rust.**

This plugin is a rewrite of the original `purple-matrix` plugin. It leverages the official `matrix-sdk` to provide robust state management, reliable syncing, thread support, and end-to-end encryption (E2EE) capabilities.

## ✨ Features

### ✨ Implementation Status: 90% (Beta / Daily Driver Ready)

The plugin is currently **Feature Complete** for core messaging, media, and encryption. It is stable enough for daily use as a text-only client.

**UX/UI Experience**:
- **Native Integration**: Seamlessly fits into Pidgin's Buddy List and Conversation windows.
- **Inline Media**: Images, Videos, and Audio files play/display directly in the chat window.
- **Flat Threading**: Threads are rendered inline with indentation (🧵) to preserve the linear chat history while indicating context.
- **Command Driven**: Advanced features (Polls, Moderation) are accessible via context menus or `/matrix_` slash commands.

### 1. Client & Authentication
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Login** | ✅ | Username/Password and legacy SSO fallback supported. |
| **Logout** | ✅ | Normal disconnect keeps session; explicit `/matrix_logout` invalidates server session. |
| **SSO / OIDC** | ✅ | Session token persistence implemented. Use password field for token manually or trigger SSO flow. |
| **Session Persistence** | ✅ | Uses `matrix-sdk-sqlite` for state storage and `session.json` for token persistence. |
| **Change Password** | ✅ | Supported via Pidgin "Change Password" menu. |

### 2. Syncing & Room Management
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Sync Loop** | ✅ | Uses `sync()` with `tokio` runtime. |
| **Join Room** | ✅ | `purple_matrix_rust_join_room` handles joins via ID or alias. |
| **Leave Room** | ✅ | `purple_matrix_rust_leave_room` calls `room.leave()`. |
| **Invite User** | ✅ | `purple_matrix_rust_invite_user` calls `room.invite_user_by_id()`. |
| **Space Support** | ✅ | Rooms are grouped by their canonical parent Space in the Buddy List. |
| **Direct Messages** | ✅ | Detected via `is_direct()` and grouped separately. |
| **Room Creation** | ✅ | Via "Create Room" menu or `/matrix_create_room`. |
| **Public Search** | ✅ | Via `/matrix_public_rooms` and Pidgin's **Room List** window. |
| **Room Moderation** | ✅ | Kick, Ban, Unban, Redact, Knock supported. |
| **Room State** | ✅ | Rename, Topic, Avatar supported. Topic via standard UI. |

### 3. Messaging & Events
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Send Text** | ✅ | Basic text messages supported. |
| **Receive Text** | ✅ | Handles incoming `m.room.message` with proper local/remote echo handling. |
| **Formatted Text** | ✅ | Remote formatted content is sanitized before display. |
| **Media (Images/Video)** | ✅ | Downloaded to `/tmp/`, displayed inline. |
| **Stickers** | ✅ | Receiving supported. Sending via `/matrix_sticker`. |
| **Reactions** | ✅ | Receiving and Sending (`/matrix_react`) supported. |
| **Redactions** | ✅ | Receiving and Sending (`/matrix_redact` or Context Menu) supported. |
| **Room Topics** | ✅ | Topic changes displayed as system messages. |
| **Typing Notifications** | ✅ | Bidirectional (Send/Receive) support. |
| **Read Receipts** | ✅ | Cross-device sync enabled. Context Menu "Mark as Read". |
| **Location Sharing** | ✅ | Sending and receiving locations. |
| **Polls** | ✅ | Create via Context Menu. Vote via interactive dialog or slash command. |
| **Editing** | ✅ | "Edit Last Message" or "Edit by ID" (Context Menu). |
| **Redaction** | ✅ | "Redact Last Message" or "Redact by ID" (Context Menu). |
| **History Fetching** | ✅ | On-demand history via Context Menu or `/history`. |

### 4. Threads
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Thread Detection** | ✅ | Detects `m.thread` relation in incoming messages. |
| **Historical Scan** | ⚠️ | Startup-wide pre-scan is disabled; use on-demand thread/history actions. |
| **Thread UI** | ✅ | Threads appear as distinct chats grouped under the parent room. Inline indentation used in main chat. |
| **Reply to Thread** | ✅ | "Start Thread" Context Menu and slash command `/matrix_thread`. |
| **List Threads** | ✅ | Full list fetched from recent history, displayed in dialog. Join via selection. |

### 5. End-to-End Encryption (E2EE)
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Decryption** | ✅ | `matrix-sdk` handles decryption transparently in the loop. |
| **Key Storage** | ✅ | `sqlite` store enabled for encryption keys. |
| **Verification (SAS)** | ✅ | UI implemented for emoji comparison (Buddy Menu -> Verify). |
| **Cross-Signing** | ✅ | Bootstrap and Secret Storage recovery implemented (Account Actions). |
| **Key Export** | ✅ | Direct key export for backups via `/matrix_export_keys`. |

### 6. User Data & Profiles
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Presence** | ✅ | Maps Libpurple status (Online/Away/Offline) to Matrix presence. |
| **Idle Status** | ✅ | Reports idle time to Matrix (sets 'unavailable' state). |
| **User Profile** | ✅ | Avatars and Display Names synchronized. |
| **Buddy Management** | ✅ | Adding a buddy ensures a Direct Message (DM) room exists. |
| **Change Password** | ✅ | Supported via Pidgin "Change Password" menu. |
| **Deactivation** | ✅ | Supported via Pidgin "Unregister" (Delete Account on Server) action. |
| **Account Data** | ✅ | Syncing `m.fully_read`, tags, and ignored users. |
| **User Search** | ✅ | Via "Search Users" Account Action or `/matrix_user_search`. |

## ⚠️ Known Limitations
*   **VoIP**: Voice and Video calls are not yet supported.
*   **Backup Restore UI**: "Restore from Backup" currently shows a not-implemented notice.
*   **Sticker Packs**: Account-data based sticker-pack discovery is limited (`im.ponies.user_defined_sticker_pack`).

## Security Notes
* TLS certificate verification is enabled by default.
* A debug-only override exists: set `MATRIX_RUST_INSECURE_SSL=1` to disable TLS verification (not recommended).

## Changelog

### Version 0.2.0 (Moderation & Stability Update) - 2026-02-04
*   **New Moderation Features**:
    *   Added `/matrix_unban <user_id> [reason]`.
    *   Added `/matrix_set_avatar <path_to_image>`.
    *   Added `/matrix_knock <room_id_or_alias> [reason]`.
    *   Added `/matrix_bulk_redact <count> [reason]`.
*   **Stability & Bug Fixes**:
    *   **Fixed Message Sync**: Resolved a critical bug where messages from other authenticated devices were suppressed by local echo logic.
    *   **Fixed SSO Persistence**: Session tokens are now correctly saved to disk after SSO login, preventing re-auth loops.
    *   **Fixed HTML Support**: Proper parsing for HTML messages in all contexts.
    *   **Fixed SAS Verification**: Threading issues resolved for emoji verification flow.
*   **Enhancements**:
    *   Added On-Demand history fetching.
    *   Added Cross-device read marker synchronization.
    *   Added Key Export and Recovery utilities.

## 🛠 Building & Installation

### Requirements
*   **Rust (Stable)**: Install via [rustup](https://rustup.rs/)
*   **GCC / Clang**: For compiling the C glue layer
*   **Libpurple Development Headers**:
    *   Debian/Ubuntu: `libpurple-dev`, `libglib2.0-dev`
    *   Fedora: `libpurple-devel`, `glib2-devel`

### Compile
1.  Navigate to the directory:
    ```bash
    cd purple-matrix-rust
    ```
2.  Build the plugin:
    ```bash
    make
    ```
    This will compile the Rust backend (`target/release/`) and link it with `plugin.c` to produce `libpurple-matrix-rust.so`.

### Install
*   **System-wide:**
    ```bash
    sudo make install
    ```
*   **User-only (Pidgin):**
    ```bash
    cp libpurple-matrix-rust.so ~/.purple/plugins/
    ```

## 🧪 Testing

We include both functionality tests for the C logic and unit tests for the Rust backend.

### Running Rust Tests
Standard cargo testing applies:
```bash
cargo test
```

### Running C Logic Tests
We use a mock header set to verify `plugin.c` logic without needing a full Libpurple installation or GUI.

```bash
# Compiles a mock libpurple test runner and checks Blist/Thread logic
gcc -I. -Itests -Itests/libpurple tests/test_logic.c $(pkg-config --cflags --libs glib-2.0) -o run_tests
./run_tests
```

**Coverage:**
*   **Thread Grouping:** Verified that threads are correctly grouped under their parent Room Name.
*   **Buddy List Management:** Verified that groups and chats are created/found as expected.

## 🧩 Architecture

The plugin follows a "Rust for Logic, C for UI" philosophy.

### The Rust Backend (`src/lib.rs`)
*   **Singleton Pattern:** Maintains a global `Lazy<Mutex<Option<Client>>>` to hold the Matrix Client state.
*   **Async Runtime:** Uses `tokio` for the internal event loop.
*   **Event Handling:** The Rust sync loop acts as the source of truth, invoking C callbacks on the main thread to update the UI.

### The C Frontend (`plugin.c`)
*   **Signal Handlers:** Maps Rust callbacks to `libpurple` signals.
*   **Data Marshalling:** Generic wrappers to convert C strings to Rust types and vice-versa.
*   **Minimality:** The C layer contains minimal business logic, mostly focused on adapting the Libpurple API quirks to the modern Matrix paradigm.

## License
[GPLv2+](../LICENSE)
