# Production Fix Plan (purple-matrix-rust)

## Context & Pain Points
1. **Thread-only buddy list** – every room is currently shown with a `Threads: ` prefix and becomes unclickable. The thread grouping helpers in `plugin.c` (`ensure_thread_in_blist`, `cleanup_stale_thread_labels`) are misfiring for canonical rooms, so the Buddy List ends up with only virtual thread entries and not the actual rooms.
2. **Unreliable login/SSO** – the Rust log shows repeated `M_UNKNOWN_TOKEN`/`Invalid login token` results, Olm account mismatches necessitating DB wipes, and SSO flows that never reach a stable connected state. Users can’t log in without manual intervention.
3. **Buddy List UX & discoverability** – the requested hierarchy (Space → Room → Thread) is missing. The UI currently exposes flattened groups with no inline guidance, which contributes to the “nothing is updating” and “rooms won’t open” symptoms.
4. **Audit surface & security stubs** – there are unused callbacks (`action_send_sticker_cb` warning in `plugin.c`), `irrefutable let` warnings in `src/lib.rs`, and an undocumented `MATRIX_RUST_INSECURE_SSL` override. Crypto backup restore also remains a stub/placeholder in the UI.
5. **Help + production polish** – onboarding lacks help text for SSO tokens, thread dialogs, backup restore guidance, and general troubleshooting, meaning users are left guessing how to finish the login or recover from failures.

## Objectives
1. Fix the thread-group regression so canonical rooms display normally and only actual thread chats get grouped under a nested `Threads` label.
2. Stabilize login/SSO workflows with deterministic session handling, meaningful user messaging, and faster recovery from invalid tokens.
3. Promote a multi-level Buddy List that reflects the space → room → thread hierarchy while keeping the UI responsive.
4. Audit code for dead/unused pieces, keep TLS verification strict (with clear gating), and unblock the crypto backup restore helper.
5. Add inline help and discoverability for SSO, threads, and backup actions so users can self-service the workflow.

## Implementation Steps
| ID | Task | Files/Modules | Notes | Owner |
| --- | --- | --- | --- | --- |
| 1 | Revisit thread grouping | `plugin.c`, `src/grouping.rs`, `tests/test_logic.c` | Ensure `cleanup_stale_thread_labels` and `ensure_thread_in_blist` only mutate virtual thread IDs, move canonical rooms back into base groups immediately, log group changes, and add regression tests verifying rooms keep their alias/group. | Codex |
| 2 | Harden login/SSO flow | `src/auth.rs`, `src/lib.rs`, `ffi.rs` | Rework `start_sso_flow` + `finish_sso_internal` to coordinate state, expose clean error messages, keep `SSO_CALLBACK` shipping the URL/token, and guard against stale `session.json` so repeated tokens stop throwing `UnknownToken`. Provide helpful notifications before retrying. | Codex |
| 3 | Multiply buddy list hierarchy | `src/grouping.rs`, `plugin.c`, `README.md` | Extend grouping logic to build a `Space / Room / Threads` path (using parent space metadata from `grouping::get_room_group_name`), move thread chat UI to nested groups (with fallback help text), and ensure the buddy list can drill down using `purple_group_new` semantics. Document the UX change in README. | Codex |
| 4 | Audit dead code & security knobs | `plugin.c`, `src/lib.rs`, `SECURITY_REMEDIATION_PLAN.md` | Remove unused callbacks, fix compiler warnings (e.g., irrefutable `if let`), document `MATRIX_RUST_INSECURE_SSL` usage, and review `SECURITY_REMEDIATION_PLAN` for gaps. Ensure TLS remains the default and add gating comments around the override. | Codex |
| 5 | Add help/discoverability hooks | `plugin.c`, `README.md`, `SECURITY_REMEDIATION_PLAN.md` | Surface help text (via `purple_notify_*` or context menus) for threads, SSO, and backup restore. Replace the “Stub: Restore from Backup” note with a real guidance message/flow or link to docs. | Codex |

## Next Steps
1. Land the thread hierarchy fixes (#1) and verify using the existing C test harness.
2. Tackle login/SSO (#2) together with help text (#5) so users can retry without manual DB wipes.
3. Finalize buddy list UX (#3) to match the requested drill-down experience.
4. Wrap up the audit/security passes (#4) and note remaining TODOs in `SECURITY_REMEDIATION_PLAN.md`.
