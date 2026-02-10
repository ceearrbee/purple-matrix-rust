# Project Status Summary

## Implementation Achievements
*   **Polls:** Full support for Creating, Voting, and Ending polls via slash commands.
    *   `src/lib.rs`: Added FFI exports `purple_matrix_rust_poll_create`, `purple_matrix_rust_poll_vote`, `purple_matrix_rust_poll_end`.
    *   `src/handlers/polls.rs`: Added basic text rendering for incoming polls.
    *   `plugin.c`: Added commands `/matrix_poll_create`, `/matrix_poll_vote`, `/matrix_poll_end`.
*   **Location Sharing:** Full support for Sending and Receiving location messages.
    *   `src/lib.rs`: Added FFI export `purple_matrix_rust_send_location`.
    *   `src/handlers/messages.rs`: Added rendering for `m.location` events with OpenStreetMap links.
    *   `plugin.c`: Added command `/matrix_location`.
*   **Account Management & Privacy:**
    *   **Change Password:** Implemented via Libpurple menu integration.
    *   **Deactivate Account:** Implemented via "Unregister" action (server-side deletion).
    *   **Buddy Management:** Mapped "Add Buddy" to DM creation; "Remove Buddy" safe stub.
    *   **Idle Status:** Mapped Libpurple idle time to Matrix presence.
    *   **Blocking:** Mapped "Block/Unblock" to Matrix ignore list.
    *   **Public Alias:** Mapped "Set Public Alias" to display name update.
    *   **Avatar:** Mapped "Set Buddy Icon" to avatar update.
*   **Room Directory:**
    *   Implemented `matrix_roomlist_get_list` to populate Pidgin's "Room List" window with public rooms fetched from the server.
*   **Stickers:**
    *   Added `/matrix_sticker` command and backend logic to upload/send stickers.

## Testing
*   **Rust Tests:** Added unit tests in `src/lib.rs` verifying HTML rendering for locations and formatted text.
*   **C Logic Tests:** Created `tests/test_logic.c` to verify Libpurple logic mock-up.
*   **Coverage:** The plugin now implements nearly all applicable Libpurple protocol features (`prpl_info`) relevant to Matrix.

## Verified Limitations
*   **Interaction Scope:** Actions like Reply, Edit, React, and Vote rely on `last_event_id` because Libpurple 2.x does not expose individual message handles.
*   **VoIP:** Not supported.
*   **File Transfer Progress:** Outgoing files use a simplified upload mechanism; incoming files are displayed as links.

## Files Modified
*   `src/lib.rs`: Added comprehensive FFI exports for all new features.
*   `plugin.c`: Wired up `prpl_info` callbacks, added UI commands, and implemented room list logic.
*   `README.md`: Updated to reflect accurate feature set and limitations.
*   `SDK_COVERAGE_REPORT.md`: Updated.
*   `tests/test_logic.c` & `src/lib.rs` (tests): Added test coverage.