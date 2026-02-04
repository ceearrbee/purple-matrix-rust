# Matrix SDK Coverage Report for `purple-matrix-rust`

This document tracks the implementation status of Matrix SDK features within the Libpurple plugin.

## 1. Client & Authentication
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Login** | ✅ Implemented | Username/Password login supported. |
| **Logout** | ❌ Missing | Client shutdown exists, but explicit API logout is not hooked up. |
| **SSO / OIDC** | ❌ Missing | Only password auth is currently implemented. |
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

## 3. Messaging & Events
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Send Text** | ✅ Implemented | Basic text messages. |
| **Receive Text** | ✅ Implemented | Handles incoming `m.room.message`. |
| **Formatted Text** | ✅ Implemented | HTML/Markdown parsing implemented using `pulldown-cmark`. |
| **Media (Images/Video)** | ✅ Implemented | Downloaded to `/tmp/` and displayed inline via `file://` URI. |
| **Reactions** | ✅ Implemented | Received reactions displayed as `[Reaction] ...`. Sending not implemented. |
| **Redactions** | ✅ Implemented | Received redactions logged/displayed. |
| **Room Topics** | ✅ Implemented | Topic changes displayed as system messages. |
| **Typing Notifications** | ✅ Implemented | Bidirectional (Send/Receive) support. |
| **Read Receipts** | ✅ Implemented | Implicitly sends read receipts on typing/message send. Explicit "mark read" API not hooked to UI. |

## 4. Threads
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Thread Detection** | ✅ Implemented | Detects `m.thread` relation in incoming messages. |
| **Historical Scan** | ✅ Implemented | Scans last 50 messages on startup to populate active threads in Buddy List. |
| **Thread UI** | ✅ Implemented | Threads appear as distinct chats grouped under the parent room. |
| **Reply to Thread** | ✅ Implemented | Custom menu action and slash command `/thread` to reply. |

## 5. End-to-End Encryption (E2EE)
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Decryption** | ✅ Implemented | `matrix-sdk` handles decryption transparently in the loop. |
| **Key Storage** | ✅ Implemented | `sqlite` store enabled for encryption keys. |
| **Verification (SAS)** | ❌ Missing | No UI to display/confirm emoji SAS strings. Devices are not verified. |
| **Cross-Signing** | ❌ Missing | Bootstrap/Upload logic not implemented. |

## 6. User Data & Profiles
| Feature | Status | Notes |
| :--- | :---: | :--- |
| **Presence** | ✅ Implemented | Maps Libpurple status (Online/Away/Offline) to Matrix presence. |
| **User Profile** | ✅ Implemented | Avatars and Display Names synchronized to Buddy List. |
| **Account Data** | ❌ Missing | Ignoring `m.fully_read`, tags, or push rules. |

## Summary of Gaps
1.  **Encryption UI:** Critical for security. Users cannot verify devices, meaning they might see "Unable to decrypt" or unverifiable sessions.
2.  **Encryption UI:** Critical for security. Users cannot verify devices, meaning they might see "Unable to decrypt" or unverifiable sessions.
2.  **(Resolved)** Profile Sync: Buddy icons and aliases are now fetched.
4.  **Formatting:** Sending HTML/Markdown to support bold/italic/links.
