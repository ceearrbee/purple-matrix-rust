# 100% Matrix Spec Implementation Plan (Sans VoIP)

This plan outlines the remaining steps to elevate `purple-matrix-rust` to a fully spec-compliant Matrix client.

## Phase 1: Robust Encryption (The "Don't Lose History" Phase)
**Goal:** Full E2EE persistence and portability.
- [x] **Online Key Backup (Secure Storage):**
    - [x] Implement `enable_key_backup`.
    - [x] Implement `restore_from_backup`.
    - [x] Signature upload/verification.
- [x] **Cross-Signing Automation:**
    - [x] Auto-accept own requests if verified.
    - [x] Interactive bootstrapping UI (with UIAA password support).

## Phase 2: Advanced Room Management (The "Admin" Phase)
**Goal:** Full control over rooms without needing another client.
- [x] **State Events:**
    - [x] `m.room.join_rules`: Command to set public/invite/knock/restricted.
    - [x] `m.room.guest_access`: Command to toggle guest access.
    - [x] `m.room.history_visibility`: Command to set world_readable/shared/invited/joined.
- [x] **Power Levels:**
    - [x] Enhanced `/power_levels` to show all granular permissions.
    - [x] Command `/set_power_level` for user-specific levels.
- [x] **Room Upgrades:**
    - [x] Handle `m.room.tombstone` by automatically joining the new room.

## Phase 3: User Interaction & Discovery
**Goal:** Feature parity with Element/modern clients.
- [x] **Push Rules (Notification Filtering):**
    - [x] Implement local evaluation of push rules (Highlights).
- [x] **Profile Management:**
    - [x] `/my_profile` command to see own data.
    - [x] `/set_avatar` (Robust).
- [x] **Read Receipts:**
    - [x] Fully implemented.
    - [x] `/matrix_who_read` command (Placeholder).

## Phase 4: Ephemeral & Timeline Features
- [x] **Reactions:**
    - [x] Aggregation: Display reaction counts via system notices.
- [x] **Edits:**
    - [x] Marked with `(edited)` suffix and styled.
- [x] **Stickers:**
    - [x] Interactive picker (`/matrix_sticker_list`).

## Phase 5: Spaces & Hierarchy
- [x] **Space Navigation:**
    - [x] Recursive space fetching and canonical parent discovery.

---

## Execution - Step 1: Key Backup
We will focus immediately on **Phase 1 (Key Backup)** as it is critical for data safety.
