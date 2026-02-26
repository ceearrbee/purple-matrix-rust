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
void matrix_ui_action_moderate_user(const char *room_id, const char *target_user_id);
void matrix_ui_action_user_info(const char *room_id, const char *target_user_id);
void matrix_ui_action_leave_room(const char *room_id);
void matrix_ui_action_verify_self(const char *room_id);
void matrix_ui_action_crypto_status(const char *room_id);
void matrix_ui_action_list_devices(const char *room_id);
void matrix_ui_action_room_settings(const char *room_id);
void matrix_ui_action_invite_user(const char *room_id);
void matrix_ui_action_send_file(const char *room_id);
void matrix_ui_action_mark_unread(const char *room_id);
void matrix_ui_action_set_room_mute(const char *room_id, bool muted);
void matrix_ui_action_search_room(const char *room_id);
void matrix_action_login_password(PurplePluginAction *action);
void matrix_action_login_sso(PurplePluginAction *action);
void matrix_action_set_account_defaults(PurplePluginAction *action);
void matrix_action_clear_session_cache(PurplePluginAction *action);
void matrix_action_login_password_for_user(const char *user_id);
void matrix_action_login_sso_for_user(const char *user_id);
void matrix_action_set_account_defaults_for_user(const char *user_id);
void matrix_action_clear_session_cache_for_user(const char *user_id);

#endif // MATRIX_COMMANDS_H
