#ifndef MATRIX_COMMANDS_H
#define MATRIX_COMMANDS_H

#include <libpurple/plugin.h>
#include <libpurple/conversation.h>
#include <stdbool.h>

void register_matrix_commands(PurplePlugin *plugin);
GList *matrix_actions(PurplePlugin *plugin, gpointer context);
void conversation_extended_menu_cb(PurpleConversation *conv, GList **list);
void open_room_dashboard(PurpleAccount *account, const char *room_id);
void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_thread_start_cb(PurpleConversation *conv, gpointer data);
void menu_action_sticker_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_poll_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data);
void matrix_ui_action_show_members(const char *room_id);
void matrix_ui_action_moderate(const char *room_id);
void matrix_ui_action_join_room_prompt(const char *room_id);
void matrix_ui_action_kick_prompt(const char *room_id);
void matrix_ui_action_ban_prompt(const char *room_id);
void matrix_ui_action_unban_prompt(const char *room_id);
void matrix_ui_action_moderate_user(const char *room_id, const char *target_user_id);
void matrix_ui_action_user_info(const char *room_id, const char *target_user_id);
void matrix_ui_action_ignore_user(const char *room_id, const char *target_user_id);
void matrix_ui_action_unignore_user(const char *room_id, const char *target_user_id);
void matrix_ui_action_leave_room(const char *room_id);
void matrix_ui_action_verify_self(const char *room_id);
void matrix_ui_action_crypto_status(const char *room_id);
void matrix_ui_action_list_devices(const char *room_id);
void matrix_ui_action_room_settings(const char *room_id);
void matrix_ui_action_invite_user(const char *room_id);
void matrix_ui_action_send_file(const char *room_id);
void matrix_ui_action_mark_unread(const char *room_id);
void matrix_ui_action_mark_read(const char *room_id);
void matrix_ui_action_set_room_mute(const char *room_id, bool muted);
void matrix_ui_action_search_room(const char *room_id);
void matrix_ui_action_list_polls(const char *room_id);
void matrix_ui_action_react(const char *room_id);
void matrix_ui_action_send_location(const char *room_id);
void matrix_ui_action_who_read(const char *room_id);
void matrix_ui_action_report_event(const char *room_id);
void matrix_ui_action_message_inspector(const char *room_id);
void matrix_ui_action_reply_pick_event(const char *room_id);
void matrix_ui_action_thread_pick_event(const char *room_id);
void matrix_ui_action_react_pick_event(const char *room_id);
void matrix_ui_action_edit_pick_event(const char *room_id);
void matrix_ui_action_redact_pick_event(const char *room_id);
void matrix_ui_action_report_pick_event(const char *room_id);
void matrix_ui_action_search_users(const char *room_id);
void matrix_ui_action_search_public(const char *room_id);
void matrix_ui_action_search_members_global(const char *room_id);
void matrix_ui_action_search_room_global(const char *room_id);
void matrix_ui_action_discover_public_rooms(const char *room_id);
void matrix_ui_action_discover_room_preview(const char *room_id);
void matrix_ui_action_redact_event_prompt(const char *room_id);
void matrix_ui_action_edit_event_prompt(const char *room_id);
void matrix_ui_action_react_latest(const char *room_id);
void matrix_ui_action_edit_latest(const char *room_id);
void matrix_ui_action_redact_latest(const char *room_id);
void matrix_ui_action_report_latest(const char *room_id);
void matrix_ui_action_versions(const char *room_id);
void matrix_ui_action_enable_key_backup(const char *room_id);
void matrix_ui_action_my_profile(const char *room_id);
void matrix_ui_action_server_info(const char *room_id);
void matrix_ui_action_resync_recent_history(const char *room_id);
void matrix_ui_action_search_stickers(const char *room_id);
void matrix_ui_action_recover_keys_prompt(const char *room_id);
void matrix_ui_action_export_keys_prompt(const char *room_id);
void matrix_ui_action_set_avatar_prompt(const char *room_id);
void matrix_ui_action_room_tag_prompt(const char *room_id);
void matrix_ui_action_upgrade_room_prompt(const char *room_id);
void matrix_ui_action_alias_create_prompt(const char *room_id);
void matrix_ui_action_alias_delete_prompt(const char *room_id);
void matrix_ui_action_space_hierarchy_prompt(const char *room_id);
void matrix_ui_action_knock_prompt(const char *room_id);
void matrix_action_login_password(PurplePluginAction *action);
void matrix_action_login_sso(PurplePluginAction *action);
void matrix_action_set_account_defaults(PurplePluginAction *action);
void matrix_action_clear_session_cache(PurplePluginAction *action);
void matrix_action_login_password_for_user(const char *user_id);
void matrix_action_login_sso_for_user(const char *user_id);
void matrix_action_set_account_defaults_for_user(const char *user_id);
void matrix_action_clear_session_cache_for_user(const char *user_id);

#endif // MATRIX_COMMANDS_H
