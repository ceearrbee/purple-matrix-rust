#ifndef MATRIX_FFI_WRAPPERS_H
#define MATRIX_FFI_WRAPPERS_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>
#include "matrix_types.h"

// FFI Functions for Event Polling
extern void purple_matrix_rust_init();
extern bool purple_matrix_rust_poll_event(int *out_type, void **out_data);
extern void purple_matrix_rust_free_event(int type, void *data);

// FFI Functions for Account Management
extern int purple_matrix_rust_login(const char *username, const char *password, const char *homeserver, const char *data_dir);
extern void purple_matrix_rust_logout(const char *user_id);
extern void purple_matrix_rust_destroy_session(const char *user_id);
extern void purple_matrix_rust_finish_sso(const char *token);
extern void purple_matrix_rust_remove_client(const char *user_id); // Phase 3.3

// FFI Functions for Messaging
extern void purple_matrix_rust_send_message(const char *user_id, const char *room_id, const char *message);
extern void purple_matrix_rust_send_im(const char *user_id, const char *target_user_id, const char *message);
extern void purple_matrix_rust_send_reaction(const char *user_id, const char *room_id, const char *event_id, const char *key);
extern void purple_matrix_rust_redact_event(const char *user_id, const char *room_id, const char *event_id, const char *reason);
extern void purple_matrix_rust_send_reply(const char *user_id, const char *room_id, const char *event_id, const char *text);
extern void purple_matrix_rust_send_edit(const char *user_id, const char *room_id, const char *event_id, const char *text);
extern void purple_matrix_rust_send_typing(const char *user_id, const char *room_id, bool is_typing);
extern void purple_matrix_rust_send_file(const char *user_id, const char *room_id, const char *filename);

// FFI Functions for Room Management
extern void purple_matrix_rust_fetch_room_members(const char *user_id, const char *room_id);
extern void purple_matrix_rust_fetch_history(const char *user_id, const char *room_id);
extern void purple_matrix_rust_join_room(const char *user_id, const char *room_id);
extern void purple_matrix_rust_leave_room(const char *user_id, const char *room_id);
extern void purple_matrix_rust_invite_user(const char *account_user_id, const char *room_id, const char *user_id);
extern void purple_matrix_rust_set_room_topic(const char *user_id, const char *room_id, const char *topic);
extern void purple_matrix_rust_send_read_receipt(const char *user_id, const char *room_id, const char *event_id);
extern void purple_matrix_rust_close_conversation(const char *user_id, const char *room_id);

// FFI Functions for Profile/Buddy List
extern void purple_matrix_rust_set_status(const char *user_id, int status, const char *msg);
extern void purple_matrix_rust_add_buddy(const char *account_user_id, const char *buddy_user_id);
extern void purple_matrix_rust_remove_buddy(const char *account_user_id, const char *buddy_user_id);
extern void purple_matrix_rust_set_idle(const char *user_id, int seconds);
extern void purple_matrix_rust_set_display_name(const char *user_id, const char *name);
extern void purple_matrix_rust_change_password(const char *user_id, const char *old, const char *new_pw);
extern void purple_matrix_rust_ignore_user(const char *user_id, const char *target_id);
extern void purple_matrix_rust_unignore_user(const char *user_id, const char *target_id);

// FFI Functions for Verification & Crypto
extern void purple_matrix_rust_verify_user(const char *user_id, const char *target_user_id);
extern void purple_matrix_rust_confirm_verification(const char *user_id, const char *target_user_id, const char *flow_id, bool confirm);
extern void purple_matrix_rust_accept_verification(const char *user_id, const char *target_user_id, const char *flow_id);
extern void purple_matrix_rust_confirm_sas(const char *user_id, const char *target_user_id, const char *flow_id, bool confirm);
extern void purple_matrix_rust_accept_sas(const char *user_id, const char *target_user_id, const char *flow_id);
extern bool purple_matrix_rust_is_room_encrypted(const char *user_id, const char *room_id);
extern void purple_matrix_rust_debug_crypto_status(const char *user_id);
extern void purple_matrix_rust_list_own_devices(const char *user_id);
extern void purple_matrix_rust_bootstrap_cross_signing(const char *user_id);
extern void purple_matrix_rust_bootstrap_cross_signing_with_password(const char *user_id, const char *password);
extern void purple_matrix_rust_enable_key_backup(const char *user_id);

// Image Store Callback (Sync call from Rust to C, needs careful handling)
extern void purple_matrix_rust_set_imgstore_add_callback(int (*cb)(const uint8_t *data, size_t size));

extern void purple_matrix_rust_list_public_rooms(const char *user_id);
extern void purple_matrix_rust_preview_room(const char *user_id, const char *room_id_or_alias);
extern void purple_matrix_rust_search_users(const char *user_id, const char *term);
extern void purple_matrix_rust_get_hierarchy(const char *user_id, const char *space_id);
extern void purple_matrix_rust_get_server_versions(const char *user_id);
extern void purple_matrix_rust_get_user_info(const char *user_id, const char *target_user_id);
extern void purple_matrix_rust_set_avatar(const char *user_id, const uint8_t *data, size_t size);
extern void purple_matrix_rust_create_poll(const char *user_id, const char *room_id, const char *question, const char **options, int options_count);
extern void purple_matrix_rust_vote_poll(const char *user_id, const char *room_id, const char *poll_id, int option_index);
extern void purple_matrix_rust_list_polls(const char *user_id, const char *room_id);
extern void purple_matrix_rust_list_threads(const char *user_id, const char *room_id);
extern void purple_matrix_rust_ui_action_poll(const char *user_id, const char *room_id);

#endif // MATRIX_FFI_WRAPPERS_H
