#ifndef MATRIX_FFI_WRAPPERS_H
#define MATRIX_FFI_WRAPPERS_H

#include <glib.h>
#include <stdbool.h>

// Rust FFI declarations
extern void purple_matrix_rust_init_invite_cb(void (*cb)(const char *, const char *));
extern void purple_matrix_rust_send_im(const char *user_id, const char *target_id, const char *text);
extern void purple_matrix_rust_fetch_room_members(const char *user_id, const char *room_id);
extern void purple_matrix_rust_send_file(const char *user_id, const char *room_id, const char *filename);
extern void purple_matrix_rust_init(void);
extern void purple_matrix_rust_set_msg_callback(void (*cb)(const char *sender, const char *msg, const char *room_id, const char *thread_root_id, const char *event_id, guint64 timestamp));
extern int purple_matrix_rust_login(const char *user, const char *pass, const char *hs, const char *data_dir);
extern void purple_matrix_rust_send_message(const char *user_id, const char *room_id, const char *text);
extern void purple_matrix_rust_set_typing_callback(void (*cb)(const char *room_id, const char *user_id, bool is_typing));
extern void purple_matrix_rust_send_typing(const char *user_id, const char *room_id, bool is_typing);
extern void purple_matrix_rust_set_room_joined_callback(void (*cb)(const char *room_id, const char *name, const char *group_name, const char *avatar_url, const char *topic, bool encrypted));
extern void purple_matrix_rust_set_room_left_callback(void (*cb)(const char *room_id));
extern void purple_matrix_rust_set_read_marker_callback(void (*cb)(const char *room_id, const char *event_id, const char *user_id));
extern void purple_matrix_rust_join_room(const char *user_id, const char *room_id);
extern void purple_matrix_rust_leave_room(const char *user_id, const char *room_id);
extern void purple_matrix_rust_invite_user(const char *user_id, const char *room_id, const char *invitee_id);
extern void purple_matrix_rust_init_sso_cb(void (*cb)(const char *));
extern void purple_matrix_rust_finish_sso(const char *token);
extern void purple_matrix_rust_set_display_name(const char *user_id, const char *name);
extern void purple_matrix_rust_set_avatar(const char *user_id, const char *path);
extern void purple_matrix_rust_bootstrap_cross_signing(const char *user_id);
extern void purple_matrix_rust_e2ee_status(const char *user_id, const char *room_id);
extern void purple_matrix_rust_verify_user(const char *user_id, const char *target_user_id);
extern void purple_matrix_rust_recover_keys(const char *user_id, const char *passphrase);
extern void purple_matrix_rust_logout(const char *user_id);
extern void purple_matrix_rust_send_location(const char *user_id, const char *room_id, const char *body, const char *geo_uri);
extern void purple_matrix_rust_poll_create(const char *user_id, const char *room_id, const char *question, const char *options);
extern void purple_matrix_rust_poll_vote(const char *user_id, const char *room_id, const char *poll_event_id, const char *option_text, const char *selection_index_str);
extern void purple_matrix_rust_poll_end(const char *user_id, const char *room_id, const char *poll_event_id);
extern void purple_matrix_rust_change_password(const char *user_id, const char *old_pw, const char *new_pw);
extern void purple_matrix_rust_add_buddy(const char *user_id, const char *buddy_id);
extern void purple_matrix_rust_remove_buddy(const char *user_id, const char *buddy_id);
extern void purple_matrix_rust_set_idle(const char *user_id, int seconds);
extern void purple_matrix_rust_send_sticker(const char *user_id, const char *room_id, const char *url);
extern void purple_matrix_rust_unignore_user(const char *user_id, const char *ignored_user_id);
extern void purple_matrix_rust_set_avatar_bytes(const char *user_id, const unsigned char *data, size_t len);
extern void purple_matrix_rust_deactivate_account(bool erase_data);
extern void purple_matrix_rust_fetch_public_rooms_for_list(const char *user_id);
extern void purple_matrix_rust_set_roomlist_add_callback(void (*cb)(const char *name, const char *id, const char *topic, guint64 count));
extern void purple_matrix_rust_set_room_preview_callback(void (*cb)(const char *room_id_or_alias, const char *html_body));
extern void purple_matrix_rust_set_thread_list_callback(void (*cb)(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts));
extern void purple_matrix_rust_list_threads(const char *user_id, const char *room_id);
extern void purple_matrix_rust_set_poll_list_callback(void (*cb)(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str));
extern void purple_matrix_rust_get_active_polls(const char *user_id, const char *room_id);
extern void purple_matrix_rust_send_read_receipt(const char *user_id, const char *room_id, const char *event_id);
extern void purple_matrix_rust_send_reaction(const char *user_id, const char *room_id, const char *event_id, const char *key);
extern void purple_matrix_rust_redact_event(const char *user_id, const char *room_id, const char *event_id, const char *reason);
extern void purple_matrix_rust_send_reply(const char *user_id, const char *room_id, const char *event_id, const char *text);
extern void purple_matrix_rust_send_edit(const char *user_id, const char *room_id, const char *event_id, const char *body);
extern void purple_matrix_rust_fetch_history(const char *user_id, const char *room_id);
extern void purple_matrix_rust_fetch_more_history(const char *user_id, const char *room_id);
extern void purple_matrix_rust_fetch_more_history_with_limit(const char *user_id, const char *room_id, guint32 limit);
extern void purple_matrix_rust_resync_recent_history(const char *user_id, const char *room_id);
extern void purple_matrix_rust_resync_recent_history_with_limit(const char *user_id, const char *room_id, guint32 limit);
extern void purple_matrix_rust_preview_room(const char *user_id, const char *room_id_or_alias, const char *output_room_id);
extern void purple_matrix_rust_fetch_history_with_limit(const char *user_id, const char *room_id, guint32 limit);
extern void purple_matrix_rust_create_room(const char *user_id, const char *name, const char *topic, bool is_public);
extern void purple_matrix_rust_search_public_rooms(const char *user_id, const char *search_term, const char *output_room_id);
extern void purple_matrix_rust_search_users(const char *user_id, const char *search_term, const char *room_id);
extern void purple_matrix_rust_kick_user(const char *user_id, const char *room_id, const char *target_user_id, const char *reason);
extern void purple_matrix_rust_ban_user(const char *user_id, const char *room_id, const char *target_user_id, const char *reason);
extern void purple_matrix_rust_set_room_tag(const char *user_id, const char *room_id, const char *tag_name);
extern void purple_matrix_rust_ignore_user(const char *user_id, const char *ignored_user_id);
extern void purple_matrix_rust_get_power_levels(const char *user_id, const char *room_id);
extern void purple_matrix_rust_set_power_level(const char *user_id, const char *room_id, const char *target_user_id, long long level);
extern void purple_matrix_rust_set_room_name(const char *user_id, const char *room_id, const char *name);
extern void purple_matrix_rust_set_room_topic(const char *user_id, const char *room_id, const char *topic);
extern void purple_matrix_rust_create_alias(const char *user_id, const char *room_id, const char *alias);
extern void purple_matrix_rust_delete_alias(const char *user_id, const char *alias);
extern void purple_matrix_rust_report_content(const char *user_id, const char *room_id, const char *event_id, const char *reason);
extern void purple_matrix_rust_export_room_keys(const char *user_id, const char *path, const char *passphrase);
extern void purple_matrix_rust_restore_from_backup(const char *user_id, const char *recovery_key);
extern void purple_matrix_rust_enable_key_backup(const char *user_id);
extern void purple_matrix_rust_bulk_redact(const char *user_id, const char *room_id, int count, const char *reason);
extern void purple_matrix_rust_knock(const char *user_id, const char *room_id_or_alias, const char *reason);
extern void purple_matrix_rust_unban_user(const char *user_id, const char *room_id, const char *target_user_id, const char *reason);
extern void purple_matrix_rust_set_room_avatar(const char *user_id, const char *room_id, const char *filename);
extern void purple_matrix_rust_set_room_mute_state(const char *user_id, const char *room_id, bool muted);
extern void purple_matrix_rust_destroy_session(const char *user_id);
// New callbacks
extern void purple_matrix_rust_init_connected_cb(void (*cb)(void));
extern void purple_matrix_rust_init_login_failed_cb(void (*cb)(const char *));
extern void purple_matrix_rust_set_show_user_info_callback(void (*cb)(const char *, const char *, const char *, bool));
extern void purple_matrix_rust_set_chat_topic_callback(void (*cb)(const char *, const char *, const char *));
extern void purple_matrix_rust_set_chat_user_callback(void (*cb)(const char *, const char *, bool, const char *, const char *));
extern void purple_matrix_rust_set_presence_callback(void (*cb)(const char *, bool));
extern void purple_matrix_rust_init_verification_cbs(
    void (*req_cb)(const char *, const char *),
    void (*emoji_cb)(const char *, const char *, const char *),
    void (*qr_cb)(const char *, const char *)
);
extern void purple_matrix_rust_accept_sas(const char *user_id, const char *target, const char *flow_id);
extern void purple_matrix_rust_confirm_sas(const char *user_id, const char *target, const char *flow_id, bool is_match);
extern void purple_matrix_rust_get_user_info(const char *user_id, const char *target);
extern void purple_matrix_rust_set_status(const char *user_id, int status, const char *msg);

#endif // MATRIX_FFI_WRAPPERS_H
