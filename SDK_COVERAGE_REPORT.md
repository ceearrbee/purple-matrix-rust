# Matrix SDK Coverage Report for `purple-matrix-rust`

This document tracks the implementation status of Matrix SDK features within the Libpurple plugin.

## 1. Client & Authentication
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Login** | ✅ Implemented | Username/Password login supported. |
| **Logout** | ✅ Implemented | Explicit `/matrix_logout` destroys session. Shutdown only disconnects. |
| **SSO / OIDC** | ✅ Implemented | Uses `matrix-sdk` native OIDC flow generation (v0.16). |
| **Session Persistence** | ✅ Implemented | Uses `matrix-sdk-sqlite` for state storage. |

## 2. Syncing & Room Management
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Sync Loop** | ✅ Implemented | Uses `sync()` with `tokio` runtime. |
| **Join Room** | ✅ Implemented | `purple_matrix_rust_join_room` handles joins via ID or alias. |
| **Leave Room** | ✅ Implemented | `purple_matrix_rust_leave_room` calls `room.leave()`. |
| **Invite User** | ✅ Implemented | `purple_matrix_rust_invite_user` calls `room.invite_user_by_id()`. |
| **Space Support** | ✅ Implemented | Rooms are grouped by their canonical parent Space in the Buddy List. |
| **Direct Messages** | ✅ Implemented | Detected via `is_direct()` and grouped separately. |
| **Room Creation** | ✅ Implemented | Supported via `/matrix_create_room <name>`. |
| **Public Directory** | ✅ Implemented | Search supported via `/matrix_public_rooms [term]`. |
| **Room Moderation** | ✅ Implemented | Commands `/matrix_kick`, `/matrix_ban`, `/matrix_unban`, and `/matrix_bulk_redact`. |
| **Room Aliases** | ✅ Implemented | Creating and removing room aliases via `/matrix_alias_create` and `/matrix_alias_delete`. |
| **Room Knocking** | ✅ Implemented | Requesting entry via `/matrix_knock <room_id_or_alias>`. |

## 3. Messaging & Events
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Send Text** | ✅ Implemented | Basic text messages with HTML support. |
| **Send Media** | ✅ Implemented | Images/Files can be sent via drag-and-drop or select. |
| **Receive Text** | ✅ Implemented | Handles incoming `m.room.message`. |
| **Formatted Text** | ✅ Implemented | HTML/Markdown parsing implemented using `pulldown-cmark`. |
| **Media (Images/Video)** | ✅ Implemented | Downloaded to `/tmp/` and displayed inline via `file://` URI. |
| **Stickers** | ✅ Implemented | Optimized: Downloads to temp cache and uses `file://` URI. |
| **Redactions** | ✅ Implemented | Received redactions logged/displayed. Sending via `/matrix_redact`. |
| **Message Edits** | ✅ Implemented | Sending via `/matrix_edit`. |
| **Replies** | ✅ Implemented | Sending via `/matrix_reply`. |
| **Room Topics** | ✅ Implemented | Topic changes displayed as system messages. |
| **Typing Notifications** | ✅ Implemented | Bidirectional (Send/Receive) support. |
| **Read Receipts** | ✅ Implemented | Implicitly sends read receipts on typing/message send. |
| **History Sync** | ✅ Implemented | Support for `/matrix_history` and menu-based backward pagination. |
| **Power Levels** | ✅ Implemented | Support for viewing and setting levels via `/matrix_power_levels`. |
| **Room State** | ✅ Implemented | Renaming rooms, setting topics, and downloading/displaying room avatars. |
| **Content Reporting** | ✅ Implemented | Reporting abusive content via `/matrix_report <event_id>`. |
| **Location Sharing** | ✅ Implemented | Sending and receiving locations with OpenStreetMap links. |
| **Polls** | ✅ Implemented | Basic rendering of incoming poll start events. |

## 4. Threads
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Thread Detection** | ✅ Implemented | Detects `m.thread` relation in incoming messages. |
| **Historical Scan** | ✅ Implemented | Scans last 50 messages on startup to populate active threads in Buddy List. |
| **Thread UI** | ✅ Implemented | Threads appear as distinct chats grouped under the parent room. |
| **Reply to Thread** | ✅ Implemented | Custom menu action and slash command `/matrix_thread` to reply. |

## 5. End-to-End Encryption (E2EE)
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Decryption** | ✅ Implemented | `matrix-sdk` handles decryption transparently in the loop. |
| **Key Storage** | ✅ Implemented | `sqlite` store enabled for encryption keys. |
| **Verification (SAS)** | ✅ Implemented | UI implemented for emoji comparison. |
| **Cross-Signing** | ✅ Implemented | Bootstrap and Secret Storage recovery implemented. |
| **Key Export** | ✅ Implemented | Bulk backup of room keys via `/matrix_export_keys <path> <passphrase>`. |

## 6. User Data & Profiles
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Presence** | ✅ Implemented | Maps Libpurple status (Online/Away/Offline) AND status messages to Matrix presence. |
| **User Profile** | ✅ Implemented | Real display names and avatars fetched via `fetch_user_profile_of`. |
| **User Search** | ✅ Implemented | Global directory search via `/matrix_user_search <term>`. |
| **Account Data** | ✅ Implemented | Syncing `m.fully_read` markers and room tags. |
| **Ignoring Users** | ✅ Implemented | Global ignore list supported via `/matrix_ignore <user_id>`. |
| **Notification Settings** | ✅ Implemented | Mute/Unmute rooms via `/matrix_mute` and `/matrix_unmute` (Push Rules). |

## 7. Usability
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Help Command** | ✅ Implemented | `/matrix_help` lists all available commands in the chat window. |

## Summary of Gaps
1.  **Implicit Read Marker UI**: Full bidirectional sync implemented. Pidgin clears unread state when marked read elsewhere, but explicitly marking as read from Pidgin relies on implicit actions (typing/sending).
