# 100% Matrix Spec Implementation Plan (Sans VoIP)

This plan outlines the remaining steps to elevate `purple-matrix-rust` to a fully spec-compliant Matrix client.

## Phase 1: Robust Encryption (The "Don't Lose History" Phase)
**Goal:** Full E2EE persistence and portability.
- [ ] **Online Key Backup (Secure Storage):**
    - [ ] Implement `enable_key_backup` (creation of new backup on server).
    - [ ] Implement `restore_from_backup` (downloading keys using recovery key/passphrase).
    - [ ] Signature upload/verification.
- [ ] **Cross-Signing Automation:**
    - [ ] Auto-accept own requests if verified.
    - [ ] Interactive bootstrapping UI (via System chat prompts).

## Phase 2: Advanced Room Management (The "Admin" Phase)
**Goal:** Full control over rooms without needing another client.
- [ ] **State Events:**
    - [ ] `m.room.join_rules`: Command to set public/invite/knock/restricted.
    - [ ] `m.room.guest_access`: Command to toggle guest access.
    - [ ] `m.room.history_visibility`: Command to set world_readable/shared/invited/joined.
- [ ] **Power Levels:**
    - [ ] Enhanced `/power_levels` to show all granular permissions.
    - [ ] Command `/set_permission <event_type> <level>` (e.g. "Who can ban?").
- [ ] **Room Upgrades:**
    - [ ] Handle `m.room.tombstone` by automatically joining the new room and hiding the old one (or marking it obsolete).

## Phase 3: User Interaction & Discovery
**Goal:** Feature parity with Element/modern clients.
- [ ] **Push Rules (Notification Filtering):**
    - [ ] Implement local evaluation of push rules to correctly set the `PURPLE_SOUND` or `PURPLE_NOTIFY` flags (Highlight vs Normal).
    - [ ] Support "Mention only" mode correctly.
- [ ] **Profile Management:**
    - [ ] `/my_profile` command to see own data.
    - [ ] `/set_avatar` (already done, verify robustness).
- [ ] **Read Receipts:**
    - [ ] Fully implemented (Check).
    - [ ] Add "Read by X, Y, Z" tooltip inspection if possible in libpurple (hard, maybe via `/message_info` command).

## Phase 4: Ephemeral & Timeline Features
- [ ] **Reactions:**
    - [ ] Aggregation: Display reaction counts in the message text (libpurple limitation: cannot modify old messages easily without re-writing history).
    - [ ] Strategy: Append `[👍 3]` to messages or use system notifications.
- [ ] **Edits:**
    - [ ] In-place replacement (best effort in libpurple).
- [ ] **Stickers:**
    - [ ] Native sticker picker is impossible in Pidgin.
    - [ ] Implement `/sticker <search>` command that returns a list of MXC URLs or renders a picker in a "System" popup.

## Phase 5: Spaces & Hierarchy
- [ ] **Space Navigation:**
    - [ ] Recursive space fetching.
    - [ ] Handling `m.space.child` events dynamically.

---

## Execution - Step 1: Key Backup
We will focus immediately on **Phase 1 (Key Backup)** as it is critical for data safety.
