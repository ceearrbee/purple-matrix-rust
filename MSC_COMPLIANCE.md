# MSC Compliance Report

This document details the implementation status of Matrix Specification Changes (MSCs) within `purple-matrix-rust`.

## Fully Implemented & Accessible

| MSC | Title | Implementation Details |
| :--- | :--- | :--- |
| **MSC2676** | Message Editing | Handled via `/matrix_edit` and rendered with `(edited)` suffix. |
| **MSC2677** | Message Reactions | Handled via `/matrix_react` and aggregated summary notices. |
| **MSC2545** | Sticker Support | Interactive picker via `/matrix_sticker_list`. Sends `m.sticker`. |
| **MSC3245** | Voice Messages | Detected via `audio` type and `voice` field. Rendered with duration. |
| **MSC2946** | Spaces Summary | Recursive space parent discovery for Buddy List grouping. |
| **MSC3440** | Threads | Threaded replies rendered with context. `/matrix_thread` to start. |
| **MSC3381** | Polls (Start/End) | `/matrix_poll_create`, `/matrix_poll_vote`, `/matrix_poll_end`. |
| **MSC2654** | Unread Counts | Displayed via `/matrix_who_read` output. |
| **MSC2403** | Knocking | `/matrix_knock` command implemented. |
| **MSC2815** | Location Sharing | `/matrix_location` command and rendering. |
| **MSC3083** | Restricted Joins | Supported via `JoinRule::Restricted`. |
| **MSC2557** | Secret Storage | Cross-signing bootstrap and key backup enable/restore. |
| **MSC3270** | Server-Side Backup | Implemented via `matrix-sdk` backup API. |

## Partially Implemented / Handled

| MSC | Title | Status | Notes |
| :--- | :--- | :--- | :--- |
| **MSC2313** | Threaded Receipts | ⚠️ Partial | Receipts are handled, but specific thread-binding logic is implicit via SDK. |
| **MSC2448** | Blurhash | ❌ Ignored | Blurhash strings are received but ignored in favor of direct file downloads. |
| **MSC1767** | Extensible Events | ⚠️ Partial | SDK fallback handles basic text. Complex extensible types may downgrade to plain text. |
| **MSC2785** | Notifications | ✅ SDK | Handled internally by `matrix-sdk` sync loop. |

## Not Applicable / Out of Scope

| MSC | Title | Reason |
| :--- | :--- | :--- |
| **MSC2746** | VoIP | Explicitly excluded from project scope. |
| **MSC3575** | Sliding Sync | Requires newer SDK (not available in 0.16.0). |
