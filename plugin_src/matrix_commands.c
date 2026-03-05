#include "matrix_commands.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <libpurple/version.h>
#include <string.h>

/* Forward Declarations for Action Handlers */
static void handle_reply_signal(const char *room_id, gpointer user_data);
static void handle_react_signal(const char *room_id, gpointer user_data);
static void handle_edit_signal(const char *room_id, gpointer user_data);
static void handle_redact_signal(const char *room_id, gpointer user_data);
static void handle_poll_signal(const char *room_id, gpointer user_data);
static void handle_list_polls_signal(const char *room_id, gpointer user_data);
static void handle_start_thread_signal(const char *room_id, gpointer user_data);
static void handle_list_threads_signal(const char *room_id, gpointer user_data);
static void handle_dashboard_signal(const char *room_id, gpointer user_data);
static void handle_moderate_signal(const char *room_id, gpointer user_data);
static void handle_room_settings_signal(const char *room_id, gpointer user_data);
static void handle_invite_user_signal(const char *room_id, gpointer user_data);
static void handle_send_file_signal(const char *room_id, gpointer user_data);
static void handle_mark_unread_signal(const char *room_id, gpointer user_data);
static void handle_mark_read_signal(const char *room_id, gpointer user_data);
static void handle_send_location_signal(const char *room_id, gpointer user_data);
static void handle_show_last_event_details_signal(const char *room_id, gpointer user_data);

/* Registry function called by matrix_core.c */
void register_matrix_commands(PurplePlugin *plugin) {
  purple_signal_connect(plugin, "matrix-ui-action-reply", plugin, PURPLE_CALLBACK(handle_reply_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-react-pick-event", plugin, PURPLE_CALLBACK(handle_react_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-edit-pick-event", plugin, PURPLE_CALLBACK(handle_edit_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-redact-pick-event", plugin, PURPLE_CALLBACK(handle_redact_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-poll", plugin, PURPLE_CALLBACK(handle_poll_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-list-polls", plugin, PURPLE_CALLBACK(handle_list_polls_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-start-thread", plugin, PURPLE_CALLBACK(handle_start_thread_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-list-threads", plugin, PURPLE_CALLBACK(handle_list_threads_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-show-dashboard", plugin, PURPLE_CALLBACK(handle_dashboard_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-moderate", plugin, PURPLE_CALLBACK(handle_moderate_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-room-settings", plugin, PURPLE_CALLBACK(handle_room_settings_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-invite-user", plugin, PURPLE_CALLBACK(handle_invite_user_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-send-file", plugin, PURPLE_CALLBACK(handle_send_file_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-mark-unread", plugin, PURPLE_CALLBACK(handle_mark_unread_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-mark-read", plugin, PURPLE_CALLBACK(handle_mark_read_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-send-location", plugin, PURPLE_CALLBACK(handle_send_location_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-show-last-event-details", plugin, PURPLE_CALLBACK(handle_show_last_event_details_signal), NULL);
}

void open_room_dashboard(PurpleAccount *account, const char *room_id) {
  if (!room_id) return;
  purple_matrix_rust_get_room_dashboard_info(purple_account_get_username(account), room_id);
}

void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data) {
  if (!PURPLE_BLIST_NODE_IS_CHAT(node)) return;
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  matrix_ui_action_room_settings(room_id);
}

/* Common Callback Context */
typedef struct {
  char *room_id;
  char *event_id;
  PurpleAccount *account;
} MatrixActionCtx;

static void free_action_ctx(MatrixActionCtx *ctx) {
  if (!ctx) return;
  g_free(ctx->room_id);
  g_free(ctx->event_id);
  g_free(ctx);
}

/* Reply Action */
static void matrix_reply_cb(void *user_data, const char *text) {
  MatrixActionCtx *ctx = (MatrixActionCtx *)user_data;
  if (text && *text && ctx->account) {
    purple_matrix_rust_send_reply(purple_account_get_username(ctx->account), ctx->room_id, ctx->event_id, text);
  }
  free_action_ctx(ctx);
}

void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  const char *room_id = purple_conversation_get_name(conv);
  const char *event_id = purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
  if (!event_id) event_id = purple_conversation_get_data(conv, "last_event_id");
  if (!event_id || !room_id) return;

  MatrixActionCtx *ctx = g_new0(MatrixActionCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);
  ctx->account = account;
  purple_request_input(my_plugin, "Reply", "Enter message", NULL, NULL, TRUE, FALSE, NULL, "_Send", G_CALLBACK(matrix_reply_cb), "_Cancel", G_CALLBACK(matrix_reply_cb), account, NULL, NULL, ctx);
}

static void handle_reply_signal(const char *room_id, gpointer user_data) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) menu_action_reply_conv_cb(conv, user_data);
}

/* Reaction Action */
static void matrix_react_cb(void *user_data, const char *text) {
  MatrixActionCtx *ctx = (MatrixActionCtx *)user_data;
  if (text && *text && ctx->account) {
    purple_matrix_rust_send_reaction(purple_account_get_username(ctx->account), ctx->room_id, ctx->event_id, text);
  }
  free_action_ctx(ctx);
}

static void matrix_ui_react_action_cb(void *user_data, int action) {
  MatrixActionCtx *ctx = (MatrixActionCtx *)user_data;
  if (!ctx) return;

  const char *emojis[] = {"👍", "❤️", "😂", "😮", "😢", "🔥", "✅", "❌"};
  if (action >= 0 && action < 8) {
    purple_matrix_rust_send_reaction(purple_account_get_username(ctx->account),
                                     ctx->room_id, ctx->event_id, emojis[action]);
    free_action_ctx(ctx);
  } else if (action == 8) {
    purple_request_input(
        my_plugin, "Add Reaction", "Enter Emoji",
        "Type any emoji or text to react with.", NULL, FALSE, FALSE, NULL,
        "_React", G_CALLBACK(matrix_react_cb), "_Cancel", G_CALLBACK(matrix_react_cb),
        ctx->account, NULL, NULL, ctx);
  } else {
    free_action_ctx(ctx);
  }
}

static void matrix_ui_react_action_0_cb(void *d) { matrix_ui_react_action_cb(d, 0); }
static void matrix_ui_react_action_1_cb(void *d) { matrix_ui_react_action_cb(d, 1); }
static void matrix_ui_react_action_2_cb(void *d) { matrix_ui_react_action_cb(d, 2); }
static void matrix_ui_react_action_3_cb(void *d) { matrix_ui_react_action_cb(d, 3); }
static void matrix_ui_react_action_4_cb(void *d) { matrix_ui_react_action_cb(d, 4); }
static void matrix_ui_react_action_5_cb(void *d) { matrix_ui_react_action_cb(d, 5); }
static void matrix_ui_react_action_6_cb(void *d) { matrix_ui_react_action_cb(d, 6); }
static void matrix_ui_react_action_7_cb(void *d) { matrix_ui_react_action_cb(d, 7); }
static void matrix_ui_react_action_8_cb(void *d) { matrix_ui_react_action_cb(d, 8); }
static void matrix_ui_react_cancel_cb(void *d) { matrix_ui_react_action_cb(d, -1); }

void matrix_ui_action_react_pick_event(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  const char *event_id = purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
  if (!event_id) return;

  MatrixActionCtx *ctx = g_new0(MatrixActionCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);
  ctx->account = account;

  purple_request_action(
      my_plugin, "Add Reaction", "Choose a reaction",
      "Select a common emoji or enter a custom one.", 0,
      account, NULL, NULL, ctx, 10, 
      "👍", G_CALLBACK(matrix_ui_react_action_0_cb), 
      "❤️", G_CALLBACK(matrix_ui_react_action_1_cb), 
      "😂", G_CALLBACK(matrix_ui_react_action_2_cb), 
      "😮", G_CALLBACK(matrix_ui_react_action_3_cb), 
      "😢", G_CALLBACK(matrix_ui_react_action_4_cb), 
      "🔥", G_CALLBACK(matrix_ui_react_action_5_cb), 
      "✅", G_CALLBACK(matrix_ui_react_action_6_cb), 
      "❌", G_CALLBACK(matrix_ui_react_action_7_cb),
      "Custom...", G_CALLBACK(matrix_ui_react_action_8_cb), 
      "_Cancel", G_CALLBACK(matrix_ui_react_cancel_cb));
}

static void handle_react_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_react_pick_event(room_id);
}

/* Edit Action */
static void matrix_edit_cb(void *user_data, const char *text) {
  MatrixActionCtx *ctx = (MatrixActionCtx *)user_data;
  if (text && *text && ctx->account) {
    purple_matrix_rust_send_edit(purple_account_get_username(ctx->account), ctx->room_id, ctx->event_id, text);
  }
  free_action_ctx(ctx);
}

void matrix_ui_action_edit_pick_event(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  const char *event_id = purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
  if (!event_id) return;

  MatrixActionCtx *ctx = g_new0(MatrixActionCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);
  ctx->account = account;
  purple_request_input(my_plugin, "Edit Message", "Enter new message", NULL, NULL, TRUE, FALSE, NULL, "_Save", G_CALLBACK(matrix_edit_cb), "_Cancel", G_CALLBACK(matrix_edit_cb), account, NULL, NULL, ctx);
}

static void handle_edit_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_edit_pick_event(room_id);
}

/* Redact Action */
static void matrix_redact_confirm_cb(void *user_data, int action) {
  MatrixActionCtx *ctx = (MatrixActionCtx *)user_data;
  if (action == 1 && ctx->account) {
    purple_matrix_rust_redact_event(purple_account_get_username(ctx->account), ctx->room_id, ctx->event_id, "User requested redaction");
  }
  free_action_ctx(ctx);
}

void matrix_ui_action_redact_pick_event(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  const char *event_id = purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
  if (!event_id) return;

  MatrixActionCtx *ctx = g_new0(MatrixActionCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);
  ctx->account = account;
  purple_request_action(my_plugin, "Redact Message", "Are you sure?", "This will permanently remove the message from Matrix history.", 0, account, NULL, NULL, ctx, 2, "_Cancel", G_CALLBACK(matrix_redact_confirm_cb), "_Redact", G_CALLBACK(matrix_redact_confirm_cb));
}

static void handle_redact_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_redact_pick_event(room_id);
}

/* Other Actions */
void matrix_ui_action_list_polls(const char *room_id) {
  purple_matrix_rust_list_polls(purple_account_get_username(find_matrix_account()), room_id);
}

static void handle_poll_signal(const char *room_id, gpointer user_data) {
  purple_matrix_rust_ui_action_poll(purple_account_get_username(find_matrix_account()), room_id);
}

static void handle_list_polls_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_list_polls(room_id);
}

static void handle_start_thread_signal(const char *room_id, gpointer user_data) {
  purple_notify_info(my_plugin, "Threads", "Start Thread", "Threading UI is not yet implemented.");
}

static void handle_list_threads_signal(const char *room_id, gpointer user_data) {
  purple_matrix_rust_list_threads(purple_account_get_username(find_matrix_account()), room_id);
}

static void handle_dashboard_signal(const char *room_id, gpointer user_data) {
  open_room_dashboard(find_matrix_account(), room_id);
}

void matrix_ui_action_moderate(const char *room_id) {
  purple_debug_info("matrix", "Moderate action requested for room %s\n", room_id);
}

static void handle_moderate_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_moderate(room_id);
}

void matrix_ui_action_room_settings(const char *room_id) {
  purple_notify_info(my_plugin, "Room Settings", room_id, "Settings UI is coming soon.");
}

static void handle_room_settings_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_room_settings(room_id);
}

static void matrix_invite_cb(void *user_data, const char *target) {
  char *room_id = (char *)user_data;
  if (target && *target) {
    purple_matrix_rust_invite_user(purple_account_get_username(find_matrix_account()), room_id, target);
  }
  g_free(room_id);
}

void matrix_ui_action_invite_user(const char *room_id) {
  purple_request_input(my_plugin, "Invite User", "Enter Matrix ID", "Invite a user to this room.", NULL, FALSE, FALSE, NULL, "_Invite", G_CALLBACK(matrix_invite_cb), "_Cancel", G_CALLBACK(matrix_invite_cb), find_matrix_account(), NULL, NULL, g_strdup(room_id));
}

static void handle_invite_user_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_invite_user(room_id);
}

static void matrix_send_file_cb(void *user_data, const char *filename) {
  char *room_id = (char *)user_data;
  if (filename && *filename) {
    purple_matrix_rust_send_file(purple_account_get_username(find_matrix_account()), room_id, filename);
  }
  g_free(room_id);
}

void matrix_ui_action_send_file(const char *room_id) {
  purple_request_file(my_plugin, "Select File to Send", NULL, FALSE, G_CALLBACK(matrix_send_file_cb), NULL, find_matrix_account(), NULL, NULL, g_strdup(room_id));
}

static void handle_send_file_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_send_file(room_id);
}

void matrix_ui_action_mark_unread(const char *room_id) {
  purple_debug_info("matrix", "Mark unread requested for room %s\n", room_id);
}

static void handle_mark_unread_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_mark_unread(room_id);
}

void matrix_ui_action_mark_read(const char *room_id) {
  purple_matrix_rust_send_read_receipt(purple_account_get_username(find_matrix_account()), room_id, "");
}

static void handle_mark_read_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_mark_read(room_id);
}

void matrix_ui_action_send_location(const char *room_id) {
  purple_notify_info(my_plugin, "Location", "Send Location", "Location sharing is not yet implemented.");
}

static void handle_send_location_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_send_location(room_id);
}

void matrix_ui_action_show_last_event_details(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  const char *event_id = purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
  if (!event_id) event_id = purple_conversation_get_data(conv, "last_event_id");
  if (event_id) {
    purple_notify_info(my_plugin, "Event Details", event_id, "Event inspection is not yet implemented.");
  }
}

static void handle_show_last_event_details_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_show_last_event_details(room_id);
}

void matrix_ui_action_resync_recent_history(const char *room_id) {
  purple_matrix_rust_fetch_history(purple_account_get_username(find_matrix_account()), room_id);
}

/* Stubs for required linked functions */
void matrix_ui_action_moderate_user(const char *room_id, const char *target_user_id) {}
void matrix_ui_action_user_info(const char *room_id, const char *target_user_id) {
  purple_matrix_rust_get_user_info(purple_account_get_username(find_matrix_account()), target_user_id);
}
void matrix_ui_action_ignore_user(const char *room_id, const char *target_user_id) {
  purple_matrix_rust_ignore_user(purple_account_get_username(find_matrix_account()), target_user_id);
}
void matrix_ui_action_unignore_user(const char *room_id, const char *target_user_id) {
  purple_matrix_rust_unignore_user(purple_account_get_username(find_matrix_account()), target_user_id);
}
void matrix_ui_action_leave_room(const char *room_id) {
  purple_matrix_rust_leave_room(purple_account_get_username(find_matrix_account()), room_id);
}
void matrix_ui_action_verify_self(const char *room_id) {}
void matrix_ui_action_crypto_status(const char *room_id) {
  purple_matrix_rust_debug_crypto_status(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_list_devices(const char *room_id) {
  purple_matrix_rust_list_own_devices(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_search_room(const char *room_id) {}
void matrix_ui_action_react(const char *room_id) {
  matrix_ui_action_react_pick_event(room_id);
}
void matrix_ui_action_who_read(const char *room_id) {
  purple_debug_info("matrix", "Who read requested for room %s\n", room_id);
}
void matrix_ui_action_report_event(const char *room_id) {}
void matrix_ui_action_message_inspector(const char *room_id) {}
void matrix_ui_action_reply_pick_event(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) menu_action_reply_conv_cb(conv, NULL);
}
void matrix_ui_action_thread_pick_event(const char *room_id) {}
void matrix_ui_action_react_latest(const char *room_id) {}
void matrix_ui_action_edit_latest(const char *room_id) {}
void matrix_ui_action_redact_latest(const char *room_id) {}
void matrix_ui_action_report_latest(const char *room_id) {}
void matrix_ui_action_versions(const char *room_id) {
  purple_matrix_rust_get_server_versions(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_enable_key_backup(const char *room_id) {
  purple_matrix_rust_enable_key_backup(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_my_profile(const char *room_id) {
  purple_matrix_rust_get_user_info(purple_account_get_username(find_matrix_account()), purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_server_info(const char *room_id) {
  purple_matrix_rust_get_server_versions(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_search_stickers(const char *room_id) {}
void matrix_ui_action_recover_keys_prompt(const char *room_id) {
  purple_matrix_rust_recover_keys_prompt(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_export_keys_prompt(const char *room_id) {}
void matrix_ui_action_set_avatar_prompt(const char *room_id) {}
void matrix_ui_action_room_tag_prompt(const char *room_id) {}
void matrix_ui_action_upgrade_room_prompt(const char *room_id) {}
void matrix_ui_action_alias_create_prompt(const char *room_id) {}
void matrix_ui_action_alias_delete_prompt(const char *room_id) {}
void matrix_ui_action_space_hierarchy_prompt(const char *room_id) {
  purple_matrix_rust_get_hierarchy(purple_account_get_username(find_matrix_account()), room_id);
}
void matrix_ui_action_knock_prompt(const char *room_id) {}
void matrix_ui_action_report_pick_event(const char *room_id) {}
void matrix_ui_action_search_users(const char *room_id) {
  purple_request_input(my_plugin, "Search Users", "Enter search term", NULL, NULL, FALSE, FALSE, NULL, "_Search", G_CALLBACK(purple_matrix_rust_search_users), "_Cancel", NULL, find_matrix_account(), NULL, NULL, (void*)purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_search_public(const char *room_id) {
  purple_matrix_rust_list_public_rooms(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_search_members_global(const char *room_id) {}
void matrix_ui_action_search_room_global(const char *room_id) {}
void matrix_ui_action_discover_public_rooms(const char *room_id) {
  purple_matrix_rust_list_public_rooms(purple_account_get_username(find_matrix_account()));
}
void matrix_ui_action_discover_room_preview(const char *room_id) {}
void matrix_ui_action_redact_event_prompt(const char *room_id) {}
void matrix_ui_action_edit_event_prompt(const char *room_id) {}
GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
  GList *m = NULL;
  PurplePluginAction *act;

  act = purple_plugin_action_new("Login with Password...", matrix_action_login_password);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Login with SSO", matrix_action_login_sso);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Clear Session Cache", matrix_action_clear_session_cache);
  act->context = context;
  m = g_list_append(m, act);

  return m;
}

void matrix_action_login_password(PurplePluginAction *action) {
  PurpleAccount *account = (PurpleAccount *)action->context;
  matrix_action_login_password_for_user(purple_account_get_username(account));
}
void matrix_action_login_sso(PurplePluginAction *action) {
  PurpleAccount *account = (PurpleAccount *)action->context;
  matrix_action_login_sso_for_user(purple_account_get_username(account));
}
void matrix_action_set_account_defaults(PurplePluginAction *action) {
  PurpleAccount *account = (PurpleAccount *)action->context;
  matrix_action_set_account_defaults_for_user(purple_account_get_username(account));
}
void matrix_action_clear_session_cache(PurplePluginAction *action) {
  PurpleAccount *account = (PurpleAccount *)action->context;
  matrix_action_clear_session_cache_for_user(purple_account_get_username(account));
}

static void matrix_login_pw_cb(void *user_data, PurpleRequestFields *fields) {
    const char *user_id = (const char *)user_data;
    const char *pw = purple_request_fields_get_string(fields, "password");
    PurpleAccount *account = find_matrix_account_by_id(user_id);
    if (account) {
        const char *hs = purple_account_get_string(account, "server", "https://matrix.org");
        char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data", user_id, NULL);
        purple_matrix_rust_login(user_id, pw, hs, data_dir);
        g_free(data_dir);
    }
}

void matrix_action_login_password_for_user(const char *user_id) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(group, purple_request_field_string_new("password", "Password", NULL, FALSE));
  
  purple_request_fields(my_plugin, "Login", "Enter Password", NULL, fields, "_Login", G_CALLBACK(matrix_login_pw_cb), "_Cancel", NULL, find_matrix_account(), NULL, NULL, (void*)user_id);
}
void matrix_action_login_sso_for_user(const char *user_id) {
  PurpleAccount *account = find_matrix_account_by_id(user_id);
  if (account) {
      const char *hs = purple_account_get_string(account, "server", "https://matrix.org");
      char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data", user_id, NULL);
      purple_matrix_rust_login(user_id, NULL, hs, data_dir);
      g_free(data_dir);
  }
}
void matrix_action_set_account_defaults_for_user(const char *user_id) {
  // Usually handled by account settings
}
void matrix_action_clear_session_cache_for_user(const char *user_id) {
  purple_matrix_rust_destroy_session(user_id);
}
