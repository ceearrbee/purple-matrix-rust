# Security and Reliability Remediation Plan

## Goal
Address dead code, security issues, and unimplemented features in `purple-matrix-rust` with a staged, test-backed rollout.

## Progress Snapshot
- Completed:
1. TLS now secure-by-default with explicit debug override via `MATRIX_RUST_INSECURE_SSL`.
2. Session persistence now writes `session.json` with restricted permissions.
3. Consumed-token SSO mismatch retry removed; mismatch now requests a fresh SSO flow.
4. Normal disconnect now uses non-destructive logout path (`matrix_close` calls `purple_matrix_rust_logout`).
5. Removed invalid libpurple signal hookups (`conversation-created`, `chat-joined`).
6. Added thread ID guardrails and stale-group migration for room list population.
7. Hardened message HTML/text rendering and attachment filename sanitization.
8. Added backup-restore UI guard (`Not Yet Implemented`) instead of false-ready flow.
9. Sticker request lifecycle cleanup to avoid leaks/double-free paths.
10. Added stale-session eviction on `M_UNKNOWN_TOKEN`/401 sync failures to avoid reconnect loops on dead tokens.
- Remaining high-impact items:
1. Expand auth/session regression tests for unknown-token and mismatch flows.
2. Complete or hide remaining partial sticker/backup flows on Rust side.
3. Finish central sanitization reuse audit across all message/preview paths.

## Phase 0: Baseline and Guardrails
1. Freeze feature additions while remediation is in progress.
2. Add/record reproducible regressions in tests before behavior changes.
3. Keep a checklist of acceptance criteria tied to commands in `TESTING.md`.

Exit criteria:
- Known issues have reproducible test or manual scripts.

## Phase 1: Security Hardening (Highest Priority)
1. TLS verification
- File: `src/auth.rs`
- Remove unconditional `disable_ssl_verification()`.
- Add an explicit debug-only override (account option or env var), default secure.
- Ensure production default always verifies certificates.

2. HTML sanitization for remote content
- Files: `src/lib.rs`, `src/handlers/messages.rs`
- Sanitize incoming Matrix-formatted HTML before passing to libpurple rendering.
- Implement allowlist-based sanitization (tags/attributes).
- Treat all remote content as untrusted by default.

3. Safe attachment/media filenames
- File: `src/handlers/messages.rs`
- Replace direct usage of `content.body` in local paths.
- Use deterministic safe filenames (event-id-based + sanitized basename).
- Prevent traversal, control chars, and invalid path characters.

4. Session token file handling
- File: `src/auth.rs`
- Write `session.json` with strict file permissions.
- Ensure no access/refresh tokens are logged.

5. Security tests
- TLS default verification path test.
- HTML sanitization tests for script/event-attr payloads.
- Filename safety tests for traversal/path injection.

Exit criteria:
- All high-severity security findings resolved.
- Security-focused tests pass.

## Phase 2: Session/Login Correctness
1. Preserve normal disconnect semantics
- File: `plugin.c`
- Keep normal close path on disconnect-only teardown.

2. SSO mismatch recovery
- File: `src/auth.rs`
- On mismatch: wipe store, rebuild client, request fresh SSO token.
- Never retry consumed one-time login token.

3. Add login state tests
- Unknown token recovery.
- Mismatch recovery.
- Explicit logout vs disconnect behavior.

Exit criteria:
- No reconnect loops or accidental token invalidation on close.

## Phase 3: Dead Code and Warnings Cleanup
1. Remove or rewire stale lazy-history callback path.
- File: `plugin.c`

2. Remove or implement unused sticker action callback.
- File: `plugin.c`

3. Resolve compiler warnings in Rust and C for touched areas.
- Files include: `src/lib.rs`, `src/handlers/crypto.rs`, `plugin.c`.

Exit criteria:
- `cargo check` and `make` clean for touched code.

## Phase 4: Unimplemented Feature Resolution
1. Thread listing
- File: `src/lib.rs`
- Provide a real thread enumeration flow (manual token and `/matrix_threads` dialog) and keep the command active with clear feedback about how to open a selected thread.

2. Backup restore
- File: `src/handlers/crypto.rs`
- Implemented; users can now paste a recovery key and download keys from the configured backup. Keep the account action visible alongside the guidance text.

3. Sticker packs
- File: `src/handlers/stickers.rs`
- Replace single-pack assumptions with documented supported model.

Exit criteria:
- No user-visible command appears complete while still stubbed.

## Phase 5: Sanitization and Rendering Consistency
1. Centralize sanitization helpers.
- Add helper functions for HTML and filenames.

2. Route all message/preview paths through shared sanitization.

Exit criteria:
- One consistent sanitization policy applied across render paths.

## Phase 6: Observability and Runtime Safety
1. Structured logs for login/sync transitions.
2. Token redaction in logs.
3. Add counters/markers for auth recovery paths.

Exit criteria:
- Logs are useful for debugging and safe to share.

## Phase 7: UX Validation
1. Verify thread labeling only for real virtual thread IDs.
2. Verify room opening, previews, updates, and history behavior.
3. Confirm explicit logout remains explicit-only behavior.
4. Confirm buddy list hierarchy matches `Space / Room / Threads` expectations and document the drill-down help text for direct discovery.

Exit criteria:
- No normal rooms mislabeled as threads.
- Room open/update flow is stable.

## Test Matrix (Pre-merge)
1. `cargo check`
2. `make`
3. `./run_tests`
4. `./run_unit_tests` (or document/fix pre-existing failures)
5. New security tests (TLS/HTML/path safety)
6. Manual smoke: fresh login, restored session, SSO, room open/send/preview, disconnect/reconnect, explicit logout.

## Delivery Plan
1. PR 1: Security hardening + login/session correctness.
2. PR 2: Dead code cleanup + feature completion/guarding + docs.
3. Post-merge runtime verification with real account logs.
