# Purple-Matrix-Rust: Documentation & Interaction Guide

This document provides a comprehensive overview of the plugin's interface, features, and implementation status.

---

## 1. Interaction Mockups

### 1.1. Login Flow (SSO)
When a server requires SSO, a system notification appears with a link.
```text
+-------------------------------------------------------------+
| SSO Login Required                                          |
+-------------------------------------------------------------+
| Please visit: https://matrix.org/_matrix/client/v3/login/sso|
|                                                             |
| [Login Token: ] [________________________________________ ] |
|                                                             |
|                      < Submit >  < Cancel >                 |
+-------------------------------------------------------------+
```

### 1.2. Buddy List Grouping (Spaces & Tags)
Rooms are automatically grouped by their parent Space or Matrix Tag.
```text
Buddy List
+-- Favorites
|   # General (Matrix)
+-- Work Space
|   # Project Alpha
|   # Sprint Planning
+-- Direct Messages
|   Alice (@alice:example.com) [Online]
+-- Matrix Rooms
    # Random
```

### 1.3. Rich Media Rendering (In-line)
Images and files are rendered with interactive links.
```text
(12:05) Alice: Check out this photo!
(12:05) Alice: [image: photo.jpg]
               +-----------------------+
               |        IMAGE          |
               |       PREVIEW         |
               +-----------------------+
               (Click to open in viewer)
```

### 1.4. Context Menus (UX)
Right-clicking a chat or buddy provides Matrix-specific actions.
```text
+----------------------------+      +----------------------------+
| Chat: #General             |      | Buddy: Alice               |
+----------------------------+      +----------------------------+
| Settings...                |      | Get Info                   |
| Mark Read                  |      | Verify User                |
| Security Status            |      | Send Sticker               |
| Power Levels               |      +----------------------------+
| Help                       |      | Send IM                    |
+----------------------------+      | Block                      |
                                    +----------------------------+
```

---

## 2. Implementation Status

### Core Features
| Feature | Status | Notes |
| :--- | :--- | :--- |
| Login (Password) | ✅ Done | Robust with auto-wipe on auth failure. |
| Login (SSO) | ✅ Done | Fully interactive via token submission. |
| Sync Loop | ✅ Done | Uses matrix-sdk 0.16. |
| Text Messaging | ✅ Done | Plain text and Markdown/HTML support. |
| History | ✅ Done | Lazy-fetch on open + /matrix_history command. |
| Replies | ✅ Done | Rendered as blockquotes with style. |
| Threads | ✅ Done | Linear rendering with 🧵 prefix or separate tabs. |
| Room Creation | ✅ Done | Support for name/topic/presets. |

### Rich Media
| Feature | Status | Notes |
| :--- | :--- | :--- |
| Images (Receive) | ✅ Done | Rendered inline with clickable paths. |
| Images (Send) | ✅ Done | Pasted images are auto-uploaded to Matrix. |
| Files | ✅ Done | Downloads to temp and provides links. |
| Audio/Voice | ✅ Done | Duration shown, clickable file link. |
| Location | ✅ Done | Text description + OpenStreetMap link. |
| Stickers | ⚠️ Partial | Sending works via URL; search is stubbed. |

### Encryption & Security
| Feature | Status | Notes |
| :--- | :--- | :--- |
| E2EE Messaging | ✅ Done | Decryption handled by SDK. |
| SAS Verification | ✅ Done | Interactive Accept/Match/Mismatch dialogs. |
| QR Code | ✅ Done | Renders QR as Base64 HTML in a popup. |
| Key Backup | ✅ Done | Check status and Restore via command. |
| Cross-Signing | ✅ Done | Bootstrap command implemented. |

### Administration
| Feature | Status | Notes |
| :--- | :--- | :--- |
| Power Levels | ✅ Done | Detailed view of all permission integers. |
| Kick/Ban/Unban | ✅ Done | Full support with optional reasons. |
| Join Rules | ✅ Done | Public/Invite/Knock/Restricted toggle. |
| Visibility | ✅ Done | History visibility (world_readable, etc.). |
| Room Tags | ✅ Done | Favorites / Low Priority support. |

---

## 3. Needs Improvement (Roadmap)

### 3.1. UI Limitations (Libpurple)
*   **Message Editing:** Currently, an edit appears as a new message with an `(edited)` suffix. Libpurple doesn't support modifying the history buffer in-place easily.
*   **Reaction Aggregation:** Reactions are shown as system notices. A better approach would be a small footer under the original message (requires UI hacks).

### 3.2. Missing Spec Parts
*   **VoIP:** Not implemented (as requested).
*   **Sticker Search:** Needs integration with a sticker pack API or account data index.
*   **Recursive Spaces:** Current implementation finds the *immediate* canonical parent. Deeply nested space trees might still look flat.

### 3.3. Performance & Polish
*   **Credential Storage:** Access tokens are currently saved in `accounts.xml` (plain text). Integration with `libsecret` is recommended.
*   **Large History:** fetching 500+ messages at once can freeze the UI thread briefly. Batching the C-side population is needed.
*   **Media Cache:** Temp files are cleaned on init, but a long-running session will accumulate files in `/tmp`.

---

## 4. Complete Command Reference
Type `/matrix_help` in any chat to see the live list of 40+ commands and their usage.
