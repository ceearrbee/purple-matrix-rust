#include "matrix_commands.h"
#include "matrix_account.h"
#include "matrix_blist.h"
#include "matrix_chat.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/cmds.h>
#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
  MATRIX_EVENT_PICK_REPLY = 1,
  MATRIX_EVENT_PICK_THREAD,
  MATRIX_EVENT_PICK_OPEN_THREAD,
  MATRIX_EVENT_PICK_REACT,
  MATRIX_EVENT_PICK_EDIT,
  MATRIX_EVENT_PICK_REDACT,
  MATRIX_EVENT_PICK_REPORT,
  MATRIX_EVENT_PICK_USER_INFO,
  MATRIX_EVENT_PICK_MODERATE_USER,
  MATRIX_EVENT_PICK_IGNORE_USER,
  MATRIX_EVENT_PICK_UNIGNORE_USER,
  MATRIX_EVENT_PICK_OPEN_DM,
  MATRIX_EVENT_PICK_COPY_EVENT_ID,
  MATRIX_EVENT_PICK_COPY_SENDER_ID,
  MATRIX_EVENT_PICK_COPY_ROOM_ID,
  MATRIX_EVENT_PICK_SHOW_EVENT_DETAILS,
  MATRIX_EVENT_PICK_COPY_THREAD_ROOT_ID,
  MATRIX_EVENT_PICK_MARK_READ,
  MATRIX_EVENT_PICK_END_POLL
} MatrixEventPickAction;

/* Forward Declarations */
static void matrix_ui_event_pick_show(const char *room_id,
                                      MatrixEventPickAction action,
                                      const char *title, const char *message);
static void action_discover_center_cb(PurplePluginAction *action);
static void action_invite_user_cb(PurplePluginAction *action);
static void action_verify_device_cb(PurplePluginAction *action);
static void action_bootstrap_crypto_cb(PurplePluginAction *action);
static void action_restore_backup_cb(PurplePluginAction *action);
static void action_resync_all_keys_cb(PurplePluginAction *action);
static void action_recover_keys_cb(PurplePluginAction *action);
static void action_export_keys_cb(PurplePluginAction *action);
static void action_server_info_cb(PurplePluginAction *action);
static void action_sso_token_cb(PurplePluginAction *action);
static void action_clear_session_cache_cb(PurplePluginAction *action);
static void action_login_password_cb(PurplePluginAction *action);
static void action_login_sso_cb(PurplePluginAction *action);
static void action_set_account_defaults_cb(PurplePluginAction *action);
static PurpleCmdRet cmd_verify(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_crypto_status(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data);
static PurpleCmdRet cmd_matrix_devices(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data);
static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_members(PurpleConversation *conv, const gchar *cmd,
                                gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_login(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_sso(PurpleConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_join(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_leave(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_create(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data);
static PurpleCmdRet cmd_matrix_clear_session(PurpleConversation *conv,
                                             const gchar *cmd, gchar **args,
                                             gchar **error, void *data);
static PurpleCmdRet cmd_react(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_polls(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_location(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_who_read(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_report(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_bulk_redact(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_knock(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_room_tag(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_upgrade_room(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_alias_create(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_alias_delete(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_search_users(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_search_public(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data);
static PurpleCmdRet cmd_search_members(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data);
static PurpleCmdRet cmd_versions(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_enable_key_backup(PurpleConversation *conv,
                                          const gchar *cmd, gchar **args,
                                          gchar **error, void *data);
static PurpleCmdRet cmd_my_profile(PurpleConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_server_info(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_resync_recent(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data);
static PurpleCmdRet cmd_sticker_search(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data);
static PurpleCmdRet cmd_recover_keys(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_export_keys(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_set_avatar(PurpleConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_mark_read(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_power_levels(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_ignore_user(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_unignore_user(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data);
static PurpleCmdRet cmd_message_inspector(PurpleConversation *conv,
                                          const gchar *cmd, gchar **args,
                                          gchar **error, void *data);
static PurpleCmdRet cmd_last_event_details(PurpleConversation *conv,
                                           const gchar *cmd, gchar **args,
                                           gchar **error, void *data);
static void action_my_profile_cb(PurplePluginAction *action);
static void my_profile_avatar_cb(void *user_data, const char *filename);
static void menu_action_reply_dialog_cb(void *user_data, const char *text);
static void menu_action_thread_dialog_cb(void *user_data, const char *text);
static void menu_action_room_dashboard_conv_cb(PurpleConversation *conv,
                                               gpointer data);
static void invite_user_dialog_cb(void *user_data, const char *user_id);
void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields);
static PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data);
static const char *matrix_last_event_for_room(PurpleAccount *account,
                                              const char *room_id);
static void matrix_ui_open_thread_from_event(const char *room_id,
                                             const char *event_id);
static void matrix_ui_set_picker_last_event_id(const char *room_id,
                                               const char *event_id);
static const char *matrix_ui_get_picker_last_event_id(const char *room_id);

static PurpleAccount *matrix_action_account(PurplePluginAction *action) {
  if (action && action->context) {
    PurpleConnection *gc = (PurpleConnection *)action->context;
    if (gc) {
      PurpleAccount *acct = purple_connection_get_account(gc);
      if (acct && strcmp(purple_account_get_protocol_id(acct),
                         "prpl-matrix-rust") == 0) {
        return acct;
      }
    }
  }
  return find_matrix_account();
}

static PurpleAccount *matrix_account_from_user_id(const char *user_id) {
  if (user_id && *user_id) {
    PurpleAccount *acct = find_matrix_account_by_id(user_id);
    if (acct)
      return acct;
  }
  return find_matrix_account();
}

static char *matrix_build_data_dir_for_user(const char *username) {
  char *safe_user = g_strdup(username ? username : "");
  for (char *p = safe_user; *p; p++) {
    if (*p == ':' || *p == '/' || *p == '\\')
      *p = '_';
  }
  char *data_dir =
      g_build_filename(purple_user_dir(), "matrix_rust_data", safe_user, NULL);
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
    g_mkdir_with_parents(data_dir, 0700);
  }
  g_free(safe_user);
  return data_dir;
}

static PurpleCmdRet matrix_cmd_require_chat(PurpleConversation *conv,
                                            gchar **error) {
  if (!conv || purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_CHAT) {
    if (error)
      *error = g_strdup("This command is available only in Matrix room chats.");
    return PURPLE_CMD_RET_FAILED;
  }
  return PURPLE_CMD_RET_OK;
}

/* Sticker Selection Wizard */
static void sticker_selected_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleConversation *conv = (PurpleConversation *)user_data;
  if (!g_list_find(purple_get_conversations(), conv))
    return;
  PurpleRequestField *field =
      purple_request_fields_get_field(fields, "sticker");
  GList *selected = purple_request_field_list_get_selected(field);
  if (selected) {
    const char *url = (const char *)purple_request_field_list_get_data(
        field, (const char *)selected->data);
    if (url)
      purple_matrix_rust_send_sticker(
          purple_account_get_username(purple_conversation_get_account(conv)),
          purple_conversation_get_name(conv), url);
  }
}
static void sticker_cb(const char *user_id, const char *shortcode,
                       const char *body, const char *url, void *user_data) {
  PurpleRequestFields *fields = (PurpleRequestFields *)user_data;
  PurpleRequestField *field =
      purple_request_fields_get_field(fields, "sticker");
  char *label = g_strdup_printf("%s: %s", shortcode, body);
  purple_request_field_list_add(field, label, g_strdup(url));
  g_free(label);
}
static void sticker_done_cb(void *user_data) {}
static void sticker_pack_selected_cb(void *user_data,
                                     PurpleRequestFields *fields) {
  PurpleConversation *conv = (PurpleConversation *)user_data;
  if (!g_list_find(purple_get_conversations(), conv))
    return;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "pack");
  GList *selected = purple_request_field_list_get_selected(field);
  if (selected) {
    const char *pack_id = (const char *)purple_request_field_list_get_data(
        field, (const char *)selected->data);
    if (pack_id) {
      PurpleRequestFields *s_fields = purple_request_fields_new();
      PurpleRequestFieldGroup *group =
          purple_request_field_group_new("Pack Contents");
      purple_request_fields_add_group(s_fields, group);
      PurpleRequestField *s_field =
          purple_request_field_list_new("sticker", "Choose Sticker");
      purple_request_field_group_add_field(group, s_field);
      purple_matrix_rust_list_stickers_in_pack(
          purple_account_get_username(purple_conversation_get_account(conv)),
          pack_id, sticker_cb, sticker_done_cb, s_fields);
      purple_request_fields(
          my_plugin, "Sticker Selection Wizard", "Select a Sticker", NULL,
          s_fields, "_Send", G_CALLBACK(sticker_selected_cb), "_Cancel", NULL,
          purple_conversation_get_account(conv), NULL, conv, conv);
    }
  }
}
static void sticker_pack_cb(const char *user_id, const char *id,
                            const char *name, void *user_data) {
  PurpleRequestFields *fields = (PurpleRequestFields *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "pack");
  purple_request_field_list_add(field, name, g_strdup(id));
}
static void sticker_pack_done_cb(void *user_data) {}
PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Available Packs");
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field =
      purple_request_field_list_new("pack", "Sticker Packs");
  purple_request_field_group_add_field(group, field);
  purple_matrix_rust_list_sticker_packs(
      purple_account_get_username(purple_conversation_get_account(conv)),
      sticker_pack_cb, sticker_pack_done_cb, fields);
  purple_request_fields(
      my_plugin, "Sticker Selection Wizard", "Browse Sticker Packs", NULL,
      fields, "Open Pack", G_CALLBACK(sticker_pack_selected_cb), "Cancel", NULL,
      purple_conversation_get_account(conv), NULL, conv, conv);
  return PURPLE_CMD_RET_OK;
}
void menu_action_sticker_conv_cb(PurpleConversation *conv, gpointer data) {
  if (conv)
    cmd_sticker_list(conv, "sticker", NULL, NULL, NULL);
}

/* Poll Creation Wizard */
static void poll_create_dialog_cb(void *user_data,
                                  PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *q = purple_request_fields_get_string(fields, "q");
  const char *opts = purple_request_fields_get_string(fields, "opts");
  if (q && opts && account)
    purple_matrix_rust_poll_create(purple_account_get_username(account),
                                   room_id, q, opts);
  g_free(room_id);
}
void menu_action_poll_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv)
    return;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Poll Details");
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("q", "Question:", NULL, FALSE));
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new("opts", "Options:", "Yes|No", FALSE));
  purple_request_fields(my_plugin, "Poll Creation Wizard", "Create a New Poll",
                        NULL, fields, "Create Poll",
                        G_CALLBACK(poll_create_dialog_cb), "Cancel", NULL,
                        purple_conversation_get_account(conv), NULL, conv,
                        g_strdup(purple_conversation_get_name(conv)));
}

static void action_send_file_cb(void *user_data, const char *filename) {
  if (!filename || !*filename)
    return;
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id) {
    purple_matrix_rust_send_file(purple_account_get_username(account), room_id,
                                 filename);
  }
  g_free(room_id);
}

/* Dashboard */
static void restore_backup_dialog_cb(void *user_data, const char *key) {
  if (!key || !*key)
    return;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    purple_matrix_rust_restore_backup(purple_account_get_username(account),
                                      key);
  }
}

static void action_restore_backup_cb(PurplePluginAction *action) {
  purple_request_input(my_plugin, "Matrix Security", "Restore Key Backup",
                       "Enter your Matrix Security Key (Recovery Key) to "
                       "decrypt historical messages and threads.",
                       NULL, FALSE, FALSE, NULL, "Restore",
                       G_CALLBACK(restore_backup_dialog_cb), "Cancel", NULL,
                       find_matrix_account(), NULL, NULL, NULL);
}

/* User Management Wizard */
static void user_mgmt_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *target = purple_request_fields_get_string(fields, "target");
  const char *reason = purple_request_fields_get_string(fields, "reason");
  PurpleRequestField *f_action =
      purple_request_fields_get_field(fields, "action");

  if (account && target && *target && f_action) {
    int choice = purple_request_field_choice_get_value(f_action);
    const char *action_name = "";
    switch (choice) {
    case 0:
      purple_matrix_rust_kick_user(purple_account_get_username(account),
                                   room_id, target, reason);
      action_name = "Kick";
      break;
    case 1:
      purple_matrix_rust_ban_user(purple_account_get_username(account), room_id,
                                  target, reason);
      action_name = "Ban";
      break;
    case 2:
      purple_matrix_rust_unban_user(purple_account_get_username(account),
                                    room_id, target, reason);
      action_name = "Unban";
      break;
    }
    char *msg = g_strdup_printf(
        "The %s request for user %s has been submitted.", action_name, target);
    purple_notify_info(my_plugin, "Moderation Action", "Action Submitted", msg);
    g_free(msg);
  }
  g_free(room_id);
}

static void bootstrap_pass_dialog_cb(void *user_data, const char *password) {
  PurpleAccount *account = (PurpleAccount *)user_data;
  if (account && password && *password) {
    purple_matrix_rust_bootstrap_cross_signing_with_password(
        purple_account_get_username(account), password);
  }
}

static void open_user_mgmt_dialog(PurpleAccount *account, const char *room_id,
                                  const char *prefill_target_user_id) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Action Details");
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new("target", "Target User ID",
                                             prefill_target_user_id, FALSE));
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("reason", "Reason (Optional)",
                                             NULL, TRUE));

  PurpleRequestField *f_action =
      purple_request_field_choice_new("action", "Action", 0);
  purple_request_field_choice_add(f_action, "Kick");
  purple_request_field_choice_add(f_action, "Ban");
  purple_request_field_choice_add(f_action, "Unban");
  purple_request_field_group_add_field(group, f_action);

  purple_request_fields(my_plugin, "User Management", "Matrix Moderation",
                        "Perform moderation actions on a user in this room.",
                        fields, "_Execute", G_CALLBACK(user_mgmt_dialog_cb),
                        "_Cancel", NULL, account, NULL, NULL,
                        g_strdup(room_id));
}

typedef struct {
  char *room_id;
  int action; /* 0=kick, 1=ban, 2=unban */
} MatrixQuickModCtx;

static void matrix_quick_mod_target_cb(void *user_data,
                                       const char *target_user_id) {
  MatrixQuickModCtx *ctx = (MatrixQuickModCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && target_user_id && *target_user_id) {
    const char *uid = purple_account_get_username(account);
    const char *action_name = "";
    switch (ctx->action) {
    case 0:
      purple_matrix_rust_kick_user(uid, ctx->room_id, target_user_id, NULL);
      action_name = "Kick";
      break;
    case 1:
      purple_matrix_rust_ban_user(uid, ctx->room_id, target_user_id, NULL);
      action_name = "Ban";
      break;
    case 2:
      purple_matrix_rust_unban_user(uid, ctx->room_id, target_user_id, NULL);
      action_name = "Unban";
      break;
    }
    char *msg =
        g_strdup_printf("The %s request for user %s has been submitted.",
                        action_name, target_user_id);
    purple_notify_info(my_plugin, "Moderation Action", "Action Submitted", msg);
    g_free(msg);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx);
  }
}
typedef struct {
  char *room_id;
  int action_map[30];
} DashUiCtx;

static void dash_choice_cb(void *user_data, PurpleRequestFields *fields) {
  DashUiCtx *ctx = (DashUiCtx *)user_data;
  char *room_id = ctx->room_id;
  PurpleRequestField *f = purple_request_fields_get_field(fields, "action");
  int raw_choice = purple_request_field_choice_get_value(f);
  int choice = ctx->action_map[raw_choice];
  PurpleAccount *account = find_matrix_account();

  if (account && room_id) {
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_ANY, room_id, account);

    switch (choice) {
    case 0:
      if (conv)
        menu_action_reply_conv_cb(conv, NULL);
      break;
    case 1:
      if (conv)
        menu_action_thread_start_cb(conv, NULL);
      break;
    case 2:
      if (conv)
        menu_action_sticker_conv_cb(conv, NULL);
      break;
    case 3:
      if (conv)
        menu_action_poll_conv_cb(conv, NULL);
      break;
    case 4:
      action_invite_user_cb(NULL);
      break;
    case 5: {
      PurpleBlistNode *node =
          (PurpleBlistNode *)purple_blist_find_chat(account, room_id);
      if (node)
        menu_action_room_settings_cb(node, NULL);
      break;
    }
    case 6:
      purple_matrix_rust_mark_unread(purple_account_get_username(account),
                                     room_id, TRUE);
      break;
    case 7:
      if (conv)
        menu_action_thread_conv_cb(conv, NULL);
      break;
    case 8:
      purple_matrix_rust_e2ee_status(purple_account_get_username(account),
                                     room_id);
      break;
    case 9:
      purple_matrix_rust_get_power_levels(purple_account_get_username(account),
                                          room_id);
      break;
    case 10: {
      purple_request_file(my_plugin, "Select File to Send", NULL, FALSE,
                          G_CALLBACK(action_send_file_cb), NULL, account, NULL,
                          conv, g_strdup(room_id));
      break;
    }
    case 11:
      action_restore_backup_cb(NULL);
      break;
    case 12:
      if (conv)
        cmd_verify(conv, "matrix_verify", NULL, NULL, NULL);
      break;
    case 13:
      action_bootstrap_crypto_cb(NULL);
      break;
    case 14: {
      purple_request_input(
          my_plugin, "E2EE Setup", "Enter Account Password",
          "This is required to establish your cryptographic identity.", NULL,
          FALSE, TRUE, NULL, "_Bootstrap", G_CALLBACK(bootstrap_pass_dialog_cb),
          "_Cancel", NULL, account, NULL, NULL, NULL);
      break;
    }
    case 15:
      if (conv)
        cmd_crypto_status(conv, "matrix_debug_crypto", NULL, NULL, NULL);
      break;
    case 16:
      open_user_mgmt_dialog(account, room_id, NULL);
      break;
    case 17: {
      purple_request_file(my_plugin, "Select Avatar Image", NULL, FALSE,
                          G_CALLBACK(my_profile_avatar_cb), NULL, account, NULL,
                          conv, NULL);
      break;
    }
    case 18: {
      purple_request_input(my_plugin, "Room Settings", "Change Room Topic",
                           NULL, NULL, TRUE, FALSE, NULL, "_Save",
                           G_CALLBACK(room_settings_dialog_cb), "_Cancel", NULL,
                           account, NULL, NULL, g_strdup(room_id));
      break;
    }
    case 19: {
      purple_request_input(my_plugin, "Room Settings", "Change Room Name", NULL,
                           NULL, FALSE, FALSE, NULL, "_Save",
                           G_CALLBACK(room_settings_dialog_cb), "_Cancel", NULL,
                           account, NULL, NULL, g_strdup(room_id));
      break;
    }
    case 20:
      purple_matrix_rust_resync_room_keys(purple_account_get_username(account),
                                          room_id);
      break;
    case 21:
      if (conv)
        cmd_members(conv, "members", NULL, NULL, NULL);
      break;
    case 22:
      if (conv)
        cmd_leave(conv, "leave", NULL, NULL, NULL);
      break;
    }
  }

  if (choice != 10 && choice != 16 && choice != 18 && choice != 19)
    g_free(room_id);

  g_free(ctx);
}

void open_room_dashboard(PurpleAccount *account, const char *room_id) {
  if (!room_id || !account)
    return;
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv)
    return;

  const char *can_invite_str =
      purple_conversation_get_data(conv, "matrix_power_invite");
  const char *can_kick_str =
      purple_conversation_get_data(conv, "matrix_power_kick");
  const char *can_ban_str =
      purple_conversation_get_data(conv, "matrix_power_ban");
  const char *is_admin_str =
      purple_conversation_get_data(conv, "matrix_power_admin");
  gboolean can_invite = can_invite_str && strcmp(can_invite_str, "1") == 0;
  gboolean can_kick = can_kick_str && strcmp(can_kick_str, "1") == 0;
  gboolean can_ban = can_ban_str && strcmp(can_ban_str, "1") == 0;
  gboolean is_admin = is_admin_str && strcmp(is_admin_str, "1") == 0;
  gboolean can_mod = can_kick || can_ban;

  DashUiCtx *ctx = g_new0(DashUiCtx, 1);
  ctx->room_id = g_strdup(room_id);
  int n = 0;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Available Tasks");
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field = purple_request_field_choice_new(
      "action", "What would you like to do?", 0);

  purple_request_field_choice_add(field, "Reply to Latest");
  ctx->action_map[n++] = 0;
  purple_request_field_choice_add(field, "Start New Thread");
  ctx->action_map[n++] = 1;
  purple_request_field_choice_add(field, "Send Sticker");
  ctx->action_map[n++] = 2;
  purple_request_field_choice_add(field, "Create Poll");
  ctx->action_map[n++] = 3;
  if (can_invite) {
    purple_request_field_choice_add(field, "Invite User");
    ctx->action_map[n++] = 4;
  }
  if (is_admin) {
    purple_request_field_choice_add(field, "Room Settings");
    ctx->action_map[n++] = 5;
  }
  purple_request_field_choice_add(field, "Mark as Read");
  ctx->action_map[n++] = 6;
  purple_request_field_choice_add(field, "View Threads");
  ctx->action_map[n++] = 7;
  purple_request_field_choice_add(field, "E2EE Status");
  ctx->action_map[n++] = 8;
  purple_request_field_choice_add(field, "Power Levels");
  ctx->action_map[n++] = 9;
  purple_request_field_choice_add(field, "Send File...");
  ctx->action_map[n++] = 10;
  purple_request_field_choice_add(field, "Setup E2EE (Security Key)...");
  ctx->action_map[n++] = 11;
  purple_request_field_choice_add(field, "Verify This Device (Emoji)...");
  ctx->action_map[n++] = 12;
  purple_request_field_choice_add(field, "Setup E2EE (Manual Bootstrap)");
  ctx->action_map[n++] = 13;
  purple_request_field_choice_add(field, "Setup E2EE (Password)...");
  ctx->action_map[n++] = 14;
  purple_request_field_choice_add(field, "Check Crypto Status");
  ctx->action_map[n++] = 15;
  if (can_mod) {
    purple_request_field_choice_add(field, "Kick/Ban User...");
    ctx->action_map[n++] = 16;
  }
  purple_request_field_choice_add(field, "Set My Avatar...");
  ctx->action_map[n++] = 17;
  if (is_admin) {
    purple_request_field_choice_add(field, "Set Room Topic...");
    ctx->action_map[n++] = 18;
  }
  if (is_admin) {
    purple_request_field_choice_add(field, "Set Room Name...");
    ctx->action_map[n++] = 19;
  }
  purple_request_field_choice_add(field, "Re-sync Room Keys");
  ctx->action_map[n++] = 20;
  purple_request_field_choice_add(field, "Show Participants");
  ctx->action_map[n++] = 21;
  purple_request_field_choice_add(field, "Leave Room");
  ctx->action_map[n++] = 22;

  purple_request_field_group_add_field(group, field);
  purple_request_fields(my_plugin, "Room Dashboard", "Matrix Room Tasks", NULL,
                        fields, "_Execute", G_CALLBACK(dash_choice_cb),
                        "_Cancel", NULL, account, NULL, NULL, ctx);
}

void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv)
    return;
  purple_request_input(my_plugin, "Reply Wizard", "Reply to Message", NULL,
                       NULL, TRUE, FALSE, NULL, "_Send",
                       G_CALLBACK(menu_action_reply_dialog_cb), "_Cancel", NULL,
                       purple_conversation_get_account(conv), NULL, conv,
                       g_strdup(purple_conversation_get_name(conv)));
}
static void menu_action_reply_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv && text && *text) {
    const char *selected_id =
        purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    const char *target_id = selected_id ? selected_id : last_id;

    if (target_id) {
      purple_matrix_rust_send_reply(purple_account_get_username(account),
                                    room_id, target_id, text);
    }
  }
  g_free(room_id);
}

void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv)
    return;
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_list_threads(purple_account_get_username(account),
                                  purple_conversation_get_name(conv));
}

void menu_action_thread_start_cb(PurpleConversation *conv, gpointer data) {
  if (!conv)
    return;
  purple_request_input(
      my_plugin, "Thread Wizard", "Start Thread",
      "Start a new thread from the latest message in this room.", NULL, TRUE,
      FALSE, NULL, "_Start", G_CALLBACK(menu_action_thread_dialog_cb),
      "_Cancel", NULL, purple_conversation_get_account(conv), NULL, conv,
      g_strdup(purple_conversation_get_name(conv)));
}
static void menu_action_thread_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv && text && *text) {
    const char *selected_id =
        purple_conversation_get_data(conv, "matrix-ui-selected-event-id");
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    const char *target_id = selected_id ? selected_id : last_id;

    if (target_id) {
      char *vid = g_strdup_printf("%s|%s", room_id, target_id);
      ensure_thread_in_blist(account, vid, "Thread", room_id);
      purple_matrix_rust_send_message(purple_account_get_username(account), vid,
                                      text);
      g_free(vid);
    }
  }
  g_free(room_id);
}

static void menu_action_jump_to_thread_conv_cb(PurpleConversation *conv,
                                               gpointer data) {
  if (conv)
    matrix_ui_event_pick_show(purple_conversation_get_name(conv),
                              MATRIX_EVENT_PICK_OPEN_THREAD, "Jump to Thread",
                              "Select thread root event:");
}
static void menu_action_open_sender_dm_conv_cb(PurpleConversation *conv,
                                               gpointer data) {
  if (conv)
    matrix_ui_event_pick_show(purple_conversation_get_name(conv),
                              MATRIX_EVENT_PICK_OPEN_DM, "Open Sender DM",
                              "Select event from target sender:");
}

static void menu_action_message_inspector_conv_cb(PurpleConversation *conv,
                                                  gpointer data) {
  if (conv)
    matrix_ui_action_message_inspector(purple_conversation_get_name(conv));
}

void conversation_extended_menu_cb(PurpleConversation *conv, GList **list) {
  if (!conv || strcmp(purple_account_get_protocol_id(
                          purple_conversation_get_account(conv)),
                      "prpl-matrix-rust") != 0)
    return;
  *list = g_list_append(*list,
                        purple_menu_action_new(
                            "Room Dashboard...",
                            PURPLE_CALLBACK(menu_action_room_dashboard_conv_cb),
                            conv, NULL));
  *list = g_list_append(
      *list,
      purple_menu_action_new(
          "Message Inspector...",
          PURPLE_CALLBACK(menu_action_message_inspector_conv_cb), conv, NULL));
  *list = g_list_append(*list,
                        purple_menu_action_new(
                            "Jump to Thread...",
                            PURPLE_CALLBACK(menu_action_jump_to_thread_conv_cb),
                            conv, NULL));
  *list = g_list_append(*list,
                        purple_menu_action_new(
                            "Open Sender DM...",
                            PURPLE_CALLBACK(menu_action_open_sender_dm_conv_cb),
                            conv, NULL));
}
static void menu_action_room_dashboard_conv_cb(PurpleConversation *conv,
                                               gpointer data) {
  if (conv)
    open_room_dashboard(purple_conversation_get_account(conv),
                        purple_conversation_get_name(conv));
}
static void discover_center_exec_cb(void *user_data,
                                    PurpleRequestFields *fields) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;

  int action = purple_request_field_choice_get_value(
      purple_request_fields_get_field(fields, "action"));
  const char *target = purple_request_fields_get_string(fields, "target");
  const char *reason = purple_request_fields_get_string(fields, "reason");

  if (!target || !*target) {
    if (action != 2) {
      purple_notify_error(my_plugin, "Discover Center", "Missing Target",
                          "You must specify a room ID, alias, or search term.");
    }
    return;
  }

  purple_account_set_string(account, "last_discover_target", target);

  const char *username = purple_account_get_username(account);
  if (action == 0) {
    /* Join */
    purple_matrix_rust_join_room(username, target);
  } else if (action == 1) {
    /* Knock */
    purple_matrix_rust_knock(username, target, reason ? reason : "");
  } else if (action == 2) {
    /* Search Directory */
    purple_roomlist_show_with_account(account);
    purple_matrix_rust_search_public_rooms(username, target);
  } else if (action == 3) {
    /* Explore Space Hierarchy */
    purple_roomlist_show_with_account(account);
    purple_matrix_rust_get_space_hierarchy(username, target);
  }
}

static void action_discover_center_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Discover Center");
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *f_action =
      purple_request_field_choice_new("action", "Action", 0);
  purple_request_field_choice_add(f_action, "Join Room");
  purple_request_field_choice_add(f_action, "Knock on Room");
  purple_request_field_choice_add(f_action, "Search Directory");
  purple_request_field_choice_add(f_action, "Explore Space Hierarchy");
  purple_request_field_group_add_field(group, f_action);

  const char *last_target =
      purple_account_get_string(account, "last_discover_target", "");
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("target",
                                             "Room ID, Alias, or Search Term",
                                             last_target, FALSE));

  purple_request_field_group_add_field(
      group, purple_request_field_string_new(
                 "reason", "Knock Reason (Optional)", "", FALSE));

  purple_request_fields(my_plugin, "Discover Center", "Discover and Join Rooms",
                        "Use this center to join known rooms, knock on private "
                        "rooms, or search the directory.",
                        fields, "_Execute", G_CALLBACK(discover_center_exec_cb),
                        "_Cancel", NULL, account, NULL, NULL, NULL);
}

static void action_invite_user_cb(PurplePluginAction *action) {
  purple_request_input(my_plugin, "Invite User", "Enter User ID",
                       "Example: @user:matrix.org", NULL, FALSE, FALSE, NULL,
                       "Invite", G_CALLBACK(invite_user_dialog_cb), "Cancel",
                       NULL, find_matrix_account(), NULL, NULL, NULL);
}
static void invite_user_dialog_cb(void *user_data, const char *user_id) {
  if (user_id && *user_id) {
    PurpleAccount *account = find_matrix_account();
    if (account)
      purple_matrix_rust_invite_user(purple_account_get_username(account), "",
                                     user_id);
  }
}

static void action_verify_device_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (account) {
    purple_matrix_rust_verify_user(purple_account_get_username(account),
                                   purple_account_get_username(account));
    purple_notify_info(my_plugin, "Verification Started",
                       "Request sent to other devices",
                       "Please check Element or another client to approve the "
                       "verification request.");
  }
}

static void action_bootstrap_crypto_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (account) {
    purple_matrix_rust_bootstrap_cross_signing(
        purple_account_get_username(account));
    purple_notify_info(my_plugin, "E2EE Setup",
                       "Cross-signing bootstrap started",
                       "The plugin is attempting to establish your "
                       "cryptographic identity. You may be prompted for your "
                       "account password or security key in another client.");
  }
}

static void action_resync_all_keys_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (account) {
    purple_matrix_rust_resync_room_keys(purple_account_get_username(account),
                                        "");
    purple_notify_info(my_plugin, "Key Sync", "Global key sync started",
                       "The plugin is requesting missing keys from your backup "
                       "and other devices.");
  }
}

static void action_recover_keys_cb(PurplePluginAction *action) {
  (void)action;
  matrix_ui_action_recover_keys_prompt(NULL);
}

static void action_export_keys_cb(PurplePluginAction *action) {
  (void)action;
  matrix_ui_action_export_keys_prompt(NULL);
}

static void action_server_info_cb(PurplePluginAction *action) {
  (void)action;
  matrix_ui_action_server_info(NULL);
}

static void my_profile_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  const char *nick = purple_request_fields_get_string(fields, "nick");
  if (nick && *nick) {
    purple_matrix_rust_set_display_name(purple_account_get_username(account),
                                        nick);
  }
}

static void my_profile_avatar_cb(void *user_data, const char *filename) {
  if (!filename || !*filename)
    return;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    purple_matrix_rust_set_room_avatar(purple_account_get_username(account), "",
                                       filename);
  }
}

static void action_my_profile_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Matrix Profile Details");
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new("nick", "Display Name", NULL, FALSE));

  /* Secondary dialog for avatar until multi-button requests are implemented */
  purple_request_fields(
      my_plugin, "My Matrix Profile", "Edit Your Matrix Profile",
      "You can update your display name here. To update your avatar, you will "
      "be prompted for a file next.",
      fields, "_Save & Next", G_CALLBACK(my_profile_dialog_cb), "_Cancel", NULL,
      account, NULL, NULL, NULL);

  /* Trigger file picker immediately if requested? No, better to have a button.
     Libpurple 2.x doesn't allow buttons inside request fields easily.
     We'll add a separate "Set Avatar" dashboard task. */
}

static void action_sso_token_cb(PurplePluginAction *action) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new("sso_token", "Login Token", NULL, FALSE));
  purple_request_fields(
      my_plugin, "SSO Login", "Authentication Required",
      "If you have a manual SSO login token, paste it below.", fields, "Submit",
      G_CALLBACK(manual_sso_token_action_cb), "Cancel", NULL,
      find_matrix_account(), NULL, NULL, find_matrix_account());
}

static void action_clear_session_cache_cb(PurplePluginAction *action) {
  PurpleAccount *account = matrix_action_account(action);
  if (!account)
    return;
  purple_matrix_rust_destroy_session(purple_account_get_username(account));
  purple_notify_info(
      my_plugin, "Matrix Session Cache", "Session cache cleared",
      "Your local Matrix session has been removed. Reconnect to log in again.");
}

static void action_login_password_dialog_cb(void *user_data,
                                            PurpleRequestFields *fields) {
  PurpleAccount *account = (PurpleAccount *)user_data;
  if (!account)
    account = find_matrix_account();
  if (!account)
    return;

  const char *homeserver =
      purple_request_fields_get_string(fields, "homeserver");
  const char *username = purple_request_fields_get_string(fields, "username");
  const char *password = purple_request_fields_get_string(fields, "password");

  if (!homeserver || !*homeserver || !username || !*username || !password ||
      !*password) {
    purple_notify_error(my_plugin, "Matrix Login", "Missing required fields",
                        "Homeserver, Username and Password are required.");
    return;
  }

  purple_account_set_string(account, "server", homeserver);
  purple_account_set_username(account, username);

  char *data_dir = matrix_build_data_dir_for_user(username);
  purple_matrix_rust_login(username, password, homeserver, data_dir);
  g_free(data_dir);
}

static void action_login_password_cb(PurplePluginAction *action) {
  PurpleAccount *account = matrix_action_account(action);
  if (!account)
    return;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Credentials");
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new(
          "homeserver", "Homeserver URL",
          purple_account_get_string(account, "server", "https://matrix.org"),
          FALSE));
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new(
          "username", "Username", purple_account_get_username(account), FALSE));
  PurpleRequestField *pw =
      purple_request_field_string_new("password", "Password", NULL, FALSE);
  purple_request_field_string_set_masked(pw, TRUE);
  purple_request_field_group_add_field(group, pw);

  purple_request_fields(my_plugin, "Matrix Login", "Login With Password", NULL,
                        fields, "_Login",
                        G_CALLBACK(action_login_password_dialog_cb), "_Cancel",
                        NULL, account, NULL, NULL, account);
}

static void action_login_sso_dialog_cb(void *user_data,
                                       PurpleRequestFields *fields) {
  PurpleAccount *account = (PurpleAccount *)user_data;
  if (!account)
    account = find_matrix_account();
  if (!account)
    return;

  const char *homeserver =
      purple_request_fields_get_string(fields, "homeserver");
  const char *username = purple_request_fields_get_string(fields, "username");
  if (!homeserver || !*homeserver || !username || !*username) {
    purple_notify_error(my_plugin, "Matrix SSO", "Missing required fields",
                        "Homeserver and Username are required.");
    return;
  }

  purple_account_set_string(account, "server", homeserver);
  purple_account_set_username(account, username);

  char *data_dir = matrix_build_data_dir_for_user(username);
  /* Empty password intentionally triggers SSO flow in Rust auth. */
  purple_matrix_rust_login(username, "", homeserver, data_dir);
  g_free(data_dir);
}

static void action_login_sso_cb(PurplePluginAction *action) {
  PurpleAccount *account = matrix_action_account(action);
  if (!account)
    return;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("SSO Login");
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new(
          "homeserver", "Homeserver URL",
          purple_account_get_string(account, "server", "https://matrix.org"),
          FALSE));
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new(
          "username", "Username", purple_account_get_username(account), FALSE));

  purple_request_fields(my_plugin, "Matrix SSO", "Start SSO Login", NULL,
                        fields, "_Start",
                        G_CALLBACK(action_login_sso_dialog_cb), "_Cancel", NULL,
                        account, NULL, NULL, account);
}

static void action_set_account_defaults_dialog_cb(void *user_data,
                                                  PurpleRequestFields *fields) {
  PurpleAccount *account = (PurpleAccount *)user_data;
  if (!account)
    account = find_matrix_account();
  if (!account)
    return;

  const char *homeserver =
      purple_request_fields_get_string(fields, "homeserver");
  const char *username = purple_request_fields_get_string(fields, "username");

  if (homeserver && *homeserver) {
    purple_account_set_string(account, "server", homeserver);
  }
  if (username && *username) {
    purple_account_set_username(account, username);
  }

  purple_notify_info(my_plugin, "Matrix Account Defaults", "Defaults updated",
                     "Homeserver and username have been saved.");
}

static void action_set_account_defaults_cb(PurplePluginAction *action) {
  PurpleAccount *account = matrix_action_account(action);
  if (!account)
    return;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("Defaults");
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new(
          "homeserver", "Homeserver URL",
          purple_account_get_string(account, "server", "https://matrix.org"),
          FALSE));
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new(
          "username", "Username", purple_account_get_username(account), FALSE));

  purple_request_fields(my_plugin, "Matrix Account Defaults",
                        "Set Homeserver / Username", NULL, fields, "_Save",
                        G_CALLBACK(action_set_account_defaults_dialog_cb),
                        "_Cancel", NULL, account, NULL, NULL, account);
}

void matrix_action_login_password(PurplePluginAction *action) {
  action_login_password_cb(action);
}

void matrix_action_login_sso(PurplePluginAction *action) {
  action_login_sso_cb(action);
}

void matrix_action_set_account_defaults(PurplePluginAction *action) {
  action_set_account_defaults_cb(action);
}

void matrix_action_clear_session_cache(PurplePluginAction *action) {
  action_clear_session_cache_cb(action);
}

void matrix_action_login_password_for_user(const char *user_id) {
  PurpleAccount *account = matrix_account_from_user_id(user_id);
  if (!account)
    return;
  PurplePluginAction action = {0};
  PurpleConnection *gc = purple_account_get_connection(account);
  action.context = gc;
  action_login_password_cb(&action);
}

void matrix_action_login_sso_for_user(const char *user_id) {
  PurpleAccount *account = matrix_account_from_user_id(user_id);
  if (!account)
    return;
  PurplePluginAction action = {0};
  PurpleConnection *gc = purple_account_get_connection(account);
  action.context = gc;
  action_login_sso_cb(&action);
}

void matrix_action_set_account_defaults_for_user(const char *user_id) {
  PurpleAccount *account = matrix_account_from_user_id(user_id);
  if (!account)
    return;
  PurplePluginAction action = {0};
  PurpleConnection *gc = purple_account_get_connection(account);
  action.context = gc;
  action_set_account_defaults_cb(&action);
}

void matrix_action_clear_session_cache_for_user(const char *user_id) {
  PurpleAccount *account = matrix_account_from_user_id(user_id);
  if (!account)
    return;
  PurplePluginAction action = {0};
  PurpleConnection *gc = purple_account_get_connection(account);
  action.context = gc;
  action_clear_session_cache_cb(&action);
}

void matrix_ui_action_show_members(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_matrix_rust_fetch_room_members(purple_account_get_username(account),
                                        room_id);
}

static void join_room_dialog_cb(void *user_data, const char *room_id) {
  if (room_id && *room_id) {
    PurpleAccount *account = (PurpleAccount *)user_data;
    if (account)
      purple_matrix_rust_join_room(purple_account_get_username(account),
                                   room_id);
  }
}

void matrix_ui_action_join_room_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_input(my_plugin, "Join Room", "Enter ID or Alias",
                       "Example: #room:matrix.org or !roomid:matrix.org", NULL,
                       FALSE, FALSE, NULL, "_Join",
                       G_CALLBACK(join_room_dialog_cb), "_Cancel", NULL,
                       account, NULL, NULL, NULL);
}

void matrix_ui_action_moderate(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  open_user_mgmt_dialog(account, room_id, NULL);
}

void matrix_ui_action_kick_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  MatrixQuickModCtx *ctx = NULL;
  if (!account || !room_id || !*room_id)
    return;
  ctx = g_new0(MatrixQuickModCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->action = 0;
  purple_request_input(my_plugin, "Kick User", "Target User ID",
                       "Enter Matrix user ID to kick.", NULL, FALSE, FALSE,
                       NULL, "_Kick", G_CALLBACK(matrix_quick_mod_target_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_ban_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  MatrixQuickModCtx *ctx = NULL;
  if (!account || !room_id || !*room_id)
    return;
  ctx = g_new0(MatrixQuickModCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->action = 1;
  purple_request_input(my_plugin, "Ban User", "Target User ID",
                       "Enter Matrix user ID to ban.", NULL, FALSE, FALSE, NULL,
                       "_Ban", G_CALLBACK(matrix_quick_mod_target_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_unban_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  MatrixQuickModCtx *ctx = NULL;
  if (!account || !room_id || !*room_id)
    return;
  ctx = g_new0(MatrixQuickModCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->action = 2;
  purple_request_input(my_plugin, "Unban User", "Target User ID",
                       "Enter Matrix user ID to unban.", NULL, FALSE, FALSE,
                       NULL, "_Unban", G_CALLBACK(matrix_quick_mod_target_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_moderate_user(const char *room_id,
                                    const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  open_user_mgmt_dialog(account, room_id, target_user_id);
}

static void matrix_ui_user_info_prompt_cb(void *user_data,
                                          const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (!account || !target_user_id || !*target_user_id)
    return;
  purple_matrix_rust_get_user_info(purple_account_get_username(account),
                                   target_user_id);
}

void matrix_ui_action_user_info(const char *room_id,
                                const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  if (target_user_id && *target_user_id) {
    purple_matrix_rust_get_user_info(purple_account_get_username(account),
                                     target_user_id);
    return;
  }
  purple_request_input(my_plugin, "User Info", "Lookup Matrix User",
                       "Enter Matrix user ID (example: @user:matrix.org)", NULL,
                       FALSE, FALSE, NULL, "_Lookup",
                       G_CALLBACK(matrix_ui_user_info_prompt_cb), "_Cancel",
                       NULL, account, NULL, NULL, NULL);
}

static void matrix_ui_ignore_user_prompt_cb(void *user_data,
                                            const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (!account || !target_user_id || !*target_user_id)
    return;
  purple_matrix_rust_ignore_user(purple_account_get_username(account),
                                 target_user_id);
}

void matrix_ui_action_ignore_user(const char *room_id,
                                  const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  if (target_user_id && *target_user_id) {
    purple_matrix_rust_ignore_user(purple_account_get_username(account),
                                   target_user_id);
    return;
  }
  purple_request_input(my_plugin, "Ignore User", "Matrix User ID",
                       "Enter Matrix user ID to ignore.", NULL, FALSE, FALSE,
                       NULL, "_Ignore",
                       G_CALLBACK(matrix_ui_ignore_user_prompt_cb), "_Cancel",
                       NULL, account, NULL, NULL, NULL);
}

static void matrix_ui_unignore_user_prompt_cb(void *user_data,
                                              const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !target_user_id || !*target_user_id)
    return;
  (void)user_data;
  purple_matrix_rust_unignore_user(purple_account_get_username(account),
                                   target_user_id);
}

void matrix_ui_action_unignore_user(const char *room_id,
                                    const char *target_user_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  if (target_user_id && *target_user_id) {
    purple_matrix_rust_unignore_user(purple_account_get_username(account),
                                     target_user_id);
    return;
  }
  purple_request_input(my_plugin, "Unignore User", "Matrix User ID",
                       "Enter Matrix user ID to remove from ignore list.", NULL,
                       FALSE, FALSE, NULL, "_Unignore",
                       G_CALLBACK(matrix_ui_unignore_user_prompt_cb), "_Cancel",
                       NULL, account, NULL, NULL, NULL);
}

void matrix_ui_action_leave_room(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  PurpleConnection *gc = purple_account_get_connection(account);
  if (!gc)
    return;
  matrix_chat_leave(gc, get_chat_id(room_id));
}

void matrix_ui_action_verify_self(const char *room_id) {
  (void)room_id;
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  const char *uid = purple_account_get_username(account);
  purple_matrix_rust_verify_user(uid, uid);
}

void matrix_ui_action_crypto_status(const char *room_id) {
  (void)room_id;
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  purple_matrix_rust_debug_crypto_status(purple_account_get_username(account));
}

void matrix_ui_action_list_devices(const char *room_id) {
  (void)room_id;
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  purple_matrix_rust_list_own_devices(purple_account_get_username(account));
}

void matrix_ui_action_room_settings(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  PurpleBlistNode *node =
      (PurpleBlistNode *)purple_blist_find_chat(account, room_id);
  if (node)
    menu_action_room_settings_cb(node, NULL);
}

static void matrix_ui_invite_room_cb(void *user_data,
                                     const char *target_user_id) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id && target_user_id && *target_user_id) {
    purple_matrix_rust_invite_user(purple_account_get_username(account),
                                   room_id, target_user_id);
  }
  g_free(room_id);
}

void matrix_ui_action_invite_user(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Invite User", "Enter Matrix User ID",
                       "Example: @user:matrix.org", NULL, FALSE, FALSE, NULL,
                       "_Invite", G_CALLBACK(matrix_ui_invite_room_cb),
                       "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

static void matrix_ui_send_file_cb(void *user_data, const char *filename) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id && filename && *filename) {
    purple_matrix_rust_send_file(purple_account_get_username(account), room_id,
                                 filename);
  }
  g_free(room_id);
}

void matrix_ui_action_send_file(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  if (!account || !room_id || !*room_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  purple_request_file(my_plugin, "Select File to Send", NULL, FALSE,
                      G_CALLBACK(matrix_ui_send_file_cb), NULL, account, NULL,
                      conv, g_strdup(room_id));
}

void matrix_ui_action_mark_unread(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_matrix_rust_mark_unread(purple_account_get_username(account), room_id,
                                 TRUE);
}

void matrix_ui_action_mark_read(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  const char *event_id = NULL;
  if (!account || !room_id || !*room_id)
    return;
  event_id = matrix_last_event_for_room(account, room_id);
  if (event_id && *event_id) {
    purple_matrix_rust_send_read_receipt(purple_account_get_username(account),
                                         room_id, event_id);
  }
  purple_matrix_rust_mark_unread(purple_account_get_username(account), room_id,
                                 FALSE);
}

void matrix_ui_action_set_room_mute(const char *room_id, bool muted) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  char state[2];
  if (!account || !room_id || !*room_id)
    return;
  purple_matrix_rust_set_room_mute_state(purple_account_get_username(account),
                                         room_id, muted);
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (conv) {
    g_snprintf(state, sizeof(state), "%d", muted ? 1 : 0);
    g_free(purple_conversation_get_data(conv, "matrix_room_muted"));
    purple_conversation_set_data(conv, "matrix_room_muted", g_strdup(state));
  }
}

static void matrix_ui_search_room_cb(void *user_data, const char *term) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id && term && *term) {
    purple_matrix_rust_search_room_messages(
        purple_account_get_username(account), room_id, term);
  }
  g_free(room_id);
}

void matrix_ui_action_search_room(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Search Messages", "Search in this Room",
                       "Enter text to search recent messages.", NULL, FALSE,
                       FALSE, NULL, "_Search",
                       G_CALLBACK(matrix_ui_search_room_cb), "_Cancel", NULL,
                       account, NULL, NULL, g_strdup(room_id));
}

typedef struct {
  char *room_id;
  char *event_id;
} MatrixUiReactCtx;

static void matrix_ui_open_reaction_picker(const char *room_id,
                                           const char *event_id);

void matrix_ui_action_react(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  const char *event_id = NULL;
  if (!account || !room_id || !*room_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return;
  event_id = purple_conversation_get_data(conv, "last_event_id");
  if (!event_id || !*event_id) {
    purple_notify_error(
        my_plugin, "React", "No target message",
        "No recent event ID is available in this conversation.");
    return;
  }
  matrix_ui_open_reaction_picker(room_id, event_id);
}

static const char *matrix_last_event_for_room(PurpleAccount *account,
                                              const char *room_id) {
  PurpleConversation *conv = NULL;
  if (!account || !room_id || !*room_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  return purple_conversation_get_data(conv, "last_event_id");
}

typedef struct {
  char *room_id;
  char *event_id;
} MatrixUiReplyCtx;

typedef struct {
  char *room_id;
  char *event_id;
} MatrixUiPickTargetCtx;

typedef struct {
  char *room_id;
  MatrixEventPickAction action;
} MatrixUiEventPickCtx;

static void matrix_ui_reply_text_cb(void *user_data, const char *text) {
  MatrixUiReplyCtx *ctx = (MatrixUiReplyCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id && text && *text) {
    purple_matrix_rust_send_reply(purple_account_get_username(account),
                                  ctx->room_id, ctx->event_id, text);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_thread_text_cb(void *user_data, const char *text) {
  MatrixUiReplyCtx *ctx = (MatrixUiReplyCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id && text && *text) {
    char *vid = g_strdup_printf("%s|%s", ctx->room_id, ctx->event_id);
    ensure_thread_in_blist(account, vid, "Thread", ctx->room_id);
    purple_matrix_rust_send_message(purple_account_get_username(account), vid,
                                    text);
    g_free(vid);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_pick_edit_text_cb(void *user_data, const char *text) {
  MatrixUiPickTargetCtx *ctx = (MatrixUiPickTargetCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id && text && *text) {
    purple_matrix_rust_send_edit(purple_account_get_username(account),
                                 ctx->room_id, ctx->event_id, text);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_pick_redact_reason_cb(void *user_data,
                                            const char *reason) {
  MatrixUiPickTargetCtx *ctx = (MatrixUiPickTargetCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id) {
    purple_matrix_rust_redact_event(purple_account_get_username(account),
                                    ctx->room_id, ctx->event_id, reason);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_pick_report_reason_cb(void *user_data,
                                            const char *reason) {
  MatrixUiPickTargetCtx *ctx = (MatrixUiPickTargetCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id) {
    purple_matrix_rust_report_content(purple_account_get_username(account),
                                      ctx->room_id, ctx->event_id, reason);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_show_event_details(const char *room_id,
                                         const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  int i;
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return;

  for (i = 0; i < 10; ++i) {
    char key_id[64], key_sender[64], key_msg[64], key_ts[64], key_enc[64],
        key_thread[64];
    const char *id = NULL;
    const char *sender = NULL;
    const char *msg = NULL;
    const char *ts = NULL;
    const char *enc = NULL;
    const char *thread = NULL;
    char tbuf[64];
    char *details = NULL;
    guint64 ms = 0;
    time_t secs = 0;
    struct tm *tm_info = NULL;

    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d",
               i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    g_snprintf(key_ts, sizeof(key_ts), "matrix_recent_event_ts_%d", i);
    g_snprintf(key_enc, sizeof(key_enc), "matrix_recent_event_enc_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);

    id = purple_conversation_get_data(conv, key_id);
    if (!id || strcmp(id, event_id) != 0)
      continue;

    sender = purple_conversation_get_data(conv, key_sender);
    msg = purple_conversation_get_data(conv, key_msg);
    ts = purple_conversation_get_data(conv, key_ts);
    enc = purple_conversation_get_data(conv, key_enc);
    thread = purple_conversation_get_data(conv, key_thread);

    if (ts && *ts) {
      ms = g_ascii_strtoull(ts, NULL, 10);
      secs = (time_t)(ms / 1000);
      tm_info = localtime(&secs);
      if (tm_info)
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);
      else
        g_strlcpy(tbuf, "unknown", sizeof(tbuf));
    } else {
      g_strlcpy(tbuf, "unknown", sizeof(tbuf));
    }

    details = g_strdup_printf(
        "Event ID: %s\nRoom ID: %s\nSender: %s\nThread Root: %s\nEncrypted: "
        "%s\nTimestamp: %s\nSnippet: %s",
        event_id, room_id, (sender && *sender) ? sender : "(unknown)",
        (thread && *thread) ? thread : "(none)",
        (enc && strcmp(enc, "1") == 0) ? "yes" : "no", tbuf,
        (msg && *msg) ? msg : "(none)");
    purple_notify_info(my_plugin, "Event Details", "Selected Matrix Event",
                       details);
    g_free(details);
    return;
  }

  purple_notify_info(
      my_plugin, "Event Details", "Selected Matrix Event",
      "Event not found in recent cache. Try after new timeline events arrive.");
}

static char *
matrix_ui_event_thread_root_exact_for_room_event(const char *room_id,
                                                 const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  int i;
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  for (i = 0; i < 10; ++i) {
    char key_id[64];
    char key_thread[64];
    const char *id = NULL;
    const char *thread = NULL;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);
    id = purple_conversation_get_data(conv, key_id);
    if (!id || strcmp(id, event_id) != 0)
      continue;
    thread = purple_conversation_get_data(conv, key_thread);
    if (thread && *thread)
      return g_strdup(thread);
    return NULL;
  }
  return NULL;
}

static void matrix_ui_react_fields_cb(void *user_data,
                                      PurpleRequestFields *fields) {
  MatrixUiReactCtx *ctx = (MatrixUiReactCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleRequestField *f_choice = purple_request_fields_get_field(fields, "choice");
  PurpleRequestField *f_custom = purple_request_fields_get_field(fields, "custom");
  const char *key = NULL;
  int choice = 0;

  if (!account || !ctx || !f_choice || !f_custom)
    goto cleanup;

  choice = purple_request_field_choice_get_value(f_choice);
  if (choice == 0) {
    /* Custom */
    key = purple_request_field_string_get_value(f_custom);
  } else {
    /* Map choice to emoji */
    const char *emojis[] = {"", "👍", "❤️", "😂", "😮", "😢", "🔥", "✅", "❌"};
    if (choice < (int)(sizeof(emojis) / sizeof(emojis[0]))) {
      key = emojis[choice];
    }
  }

  if (key && *key) {
    purple_matrix_rust_send_reaction(purple_account_get_username(account),
                                     ctx->room_id, ctx->event_id, key);
  }

cleanup:
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_open_reaction_picker(const char *room_id,
                                           const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleRequestFields *fields;
  PurpleRequestFieldGroup *group;
  PurpleRequestField *field;
  MatrixUiReactCtx *ctx;

  if (!account || !room_id || !event_id)
    return;

  fields = purple_request_fields_new();
  group = purple_request_field_group_new("Select Reaction");
  purple_request_fields_add_group(fields, group);

  field = purple_request_field_choice_new("choice", "Common Reactions", 0);
  purple_request_field_choice_add(field, "Custom (Use text box below)");
  purple_request_field_choice_add(field, "👍 Thumbs Up");
  purple_request_field_choice_add(field, "❤️ Heart");
  purple_request_field_choice_add(field, "😂 Laughing");
  purple_request_field_choice_add(field, "😮 Surprised");
  purple_request_field_choice_add(field, "😢 Sad");
  purple_request_field_choice_add(field, "🔥 Fire");
  purple_request_field_choice_add(field, "✅ Check");
  purple_request_field_choice_add(field, "❌ Cross");
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("custom", "Custom Text/Emoji", NULL,
                                          FALSE);
  purple_request_field_group_add_field(group, field);

  ctx = g_new0(MatrixUiReactCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);

  purple_request_fields(my_plugin, "Add Reaction", "Choose a reaction", NULL,
                        fields, "_React", G_CALLBACK(matrix_ui_react_fields_cb),
                        "_Cancel", NULL, account, NULL, NULL, ctx);
}

static void matrix_ui_open_event_action_dialog(const char *room_id,
                                               const char *event_id,
                                               MatrixEventPickAction action) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return;

  if (action == MATRIX_EVENT_PICK_REACT) {
    matrix_ui_open_reaction_picker(room_id, event_id);
    return;
  }

  if (action == MATRIX_EVENT_PICK_EDIT) {
    MatrixUiPickTargetCtx *ctx = g_new0(MatrixUiPickTargetCtx, 1);
    ctx->room_id = g_strdup(room_id);
    ctx->event_id = g_strdup(event_id);
    purple_request_input(my_plugin, "Edit Event", "New Message Text",
                         "Enter replacement text.", NULL, TRUE, FALSE, NULL,
                         "_Send Edit", G_CALLBACK(matrix_ui_pick_edit_text_cb),
                         "_Cancel", NULL, account, NULL, NULL, ctx);
    return;
  }

  if (action == MATRIX_EVENT_PICK_REDACT) {
    MatrixUiPickTargetCtx *ctx = g_new0(MatrixUiPickTargetCtx, 1);
    ctx->room_id = g_strdup(room_id);
    ctx->event_id = g_strdup(event_id);
    purple_request_input(my_plugin, "Redact Event", "Reason",
                         "Optional reason for redaction.", NULL, FALSE, FALSE,
                         NULL, "_Redact",
                         G_CALLBACK(matrix_ui_pick_redact_reason_cb), "_Cancel",
                         NULL, account, NULL, NULL, ctx);
    return;
  }

  if (action == MATRIX_EVENT_PICK_REPORT) {
    MatrixUiPickTargetCtx *ctx = g_new0(MatrixUiPickTargetCtx, 1);
    ctx->room_id = g_strdup(room_id);
    ctx->event_id = g_strdup(event_id);
    purple_request_input(my_plugin, "Report Content", "Reason",
                         "Optional reason for reporting.", NULL, FALSE, FALSE,
                         NULL, "_Report",
                         G_CALLBACK(matrix_ui_pick_report_reason_cb), "_Cancel",
                         NULL, account, NULL, NULL, ctx);
    return;
  }

  if (action == MATRIX_EVENT_PICK_OPEN_THREAD) {
    matrix_ui_open_thread_from_event(room_id, event_id);
    return;
  }

  if (action == MATRIX_EVENT_PICK_COPY_EVENT_ID) {
    purple_notify_info(my_plugin, "Event ID", "Selected Event ID", event_id);
    return;
  }

  if (action == MATRIX_EVENT_PICK_COPY_ROOM_ID) {
    purple_notify_info(my_plugin, "Room ID", "Current Room ID", room_id);
    return;
  }

  if (action == MATRIX_EVENT_PICK_SHOW_EVENT_DETAILS) {
    matrix_ui_show_event_details(room_id, event_id);
    return;
  }

  if (action == MATRIX_EVENT_PICK_COPY_THREAD_ROOT_ID) {
    char *thread_root =
        matrix_ui_event_thread_root_exact_for_room_event(room_id, event_id);
    if (thread_root && *thread_root) {
      purple_notify_info(my_plugin, "Thread Root ID",
                         "Selected Event Thread Root", thread_root);
    } else {
      purple_notify_info(
          my_plugin, "Thread Root ID", "Selected Event Thread Root",
          "Selected event is not in a thread (no explicit thread root).");
    }
    if (thread_root)
      g_free(thread_root);
    return;
  }

  if (action == MATRIX_EVENT_PICK_REPLY) {
    MatrixUiReplyCtx *ctx = g_new0(MatrixUiReplyCtx, 1);
    ctx->room_id = g_strdup(room_id);
    ctx->event_id = g_strdup(event_id);
    purple_request_input(my_plugin, "Reply to Event", "Message Text",
                         "Enter reply text.", NULL, TRUE, FALSE, NULL, "_Reply",
                         G_CALLBACK(matrix_ui_reply_text_cb), "_Cancel", NULL,
                         account, NULL, NULL, ctx);
    return;
  }

  if (action == MATRIX_EVENT_PICK_THREAD) {
    MatrixUiReplyCtx *ctx = g_new0(MatrixUiReplyCtx, 1);
    ctx->room_id = g_strdup(room_id);
    ctx->event_id = g_strdup(event_id);
    purple_request_input(
        my_plugin, "Start Thread", "Thread Message",
        "Send first message in new thread rooted at selected event.", NULL,
        TRUE, FALSE, NULL, "_Start", G_CALLBACK(matrix_ui_thread_text_cb),
        "_Cancel", NULL, account, NULL, NULL, ctx);
    return;
  }
}

static char *matrix_ui_event_sender_for_room_event(const char *room_id,
                                                   const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  int i;
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  for (i = 0; i < 10; ++i) {
    char key_id[64];
    char key_sender[64];
    const char *id = NULL;
    const char *sender = NULL;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d",
               i);
    id = purple_conversation_get_data(conv, key_id);
    if (!id || strcmp(id, event_id) != 0)
      continue;
    sender = purple_conversation_get_data(conv, key_sender);
    if (sender && *sender)
      return g_strdup(sender);
  }
  return NULL;
}

static void matrix_ui_open_dm_for_user(const char *user_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  if (!account || !user_id || !*user_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, user_id,
                                               account);
  if (!conv) {
    conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, user_id);
  }
  if (conv)
    purple_conversation_present(conv);
}

static char *matrix_ui_event_thread_root_for_room_event(const char *room_id,
                                                        const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  int i;
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  for (i = 0; i < 10; ++i) {
    char key_id[64];
    char key_thread[64];
    const char *id = NULL;
    const char *thread = NULL;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);
    id = purple_conversation_get_data(conv, key_id);
    if (!id || strcmp(id, event_id) != 0)
      continue;
    thread = purple_conversation_get_data(conv, key_thread);
    if (thread && *thread)
      return g_strdup(thread);
    return g_strdup(event_id);
  }
  return g_strdup(event_id);
}

static void matrix_ui_open_thread_from_event(const char *room_id,
                                             const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConnection *gc = NULL;
  PurpleConversation *conv = NULL;
  char *root_id = NULL;
  char *virtual_id = NULL;
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return;

  root_id = matrix_ui_event_thread_root_for_room_event(room_id, event_id);
  if (!root_id || !*root_id) {
    if (root_id)
      g_free(root_id);
    return;
  }
  virtual_id = g_strdup_printf("%s|%s", room_id, root_id);
  ensure_thread_in_blist(account, virtual_id, "Thread", room_id);

  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                               virtual_id, account);
  if (!conv) {
    gc = purple_account_get_connection(account);
    if (gc) {
      serv_got_joined_chat(gc, get_chat_id(virtual_id), virtual_id);
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                   virtual_id, account);
    }
  }
  if (conv)
    purple_conversation_present(conv);

  g_free(virtual_id);
  g_free(root_id);
}

static void matrix_ui_event_pick_selected_cb(void *user_data,
                                             PurpleRequestFields *fields) {
  MatrixUiEventPickCtx *ctx = (MatrixUiEventPickCtx *)user_data;
  PurpleRequestField *field = NULL;
  GList *selected = NULL;
  const char *event_id = NULL;
  if (!ctx)
    return;
  field = purple_request_fields_get_field(fields, "event");
  if (!field)
    goto out;
  selected = purple_request_field_list_get_selected(field);
  if (!selected)
    goto out;
  event_id = (const char *)purple_request_field_list_get_data(
      field, (const char *)selected->data);
  if (event_id && *event_id) {
    matrix_ui_set_picker_last_event_id(ctx->room_id, event_id);
    matrix_ui_open_event_action_dialog(ctx->room_id, event_id, ctx->action);
  }
out:
  g_free(ctx->room_id);
  g_free(ctx);
}

static void matrix_ui_event_pick_show(const char *room_id,
                                      MatrixEventPickAction action,
                                      const char *title, const char *heading) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  PurpleRequestFields *fields = NULL;
  PurpleRequestFieldGroup *group = NULL;
  PurpleRequestField *list = NULL;
  MatrixUiEventPickCtx *ctx = NULL;
  const char *last_event_id = NULL;
  char *selected_label = NULL;
  int added = 0;
  int i = 0;

  if (!account || !room_id || !*room_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return;
  last_event_id = matrix_ui_get_picker_last_event_id(room_id);

  fields = purple_request_fields_new();
  group = purple_request_field_group_new("Recent Events");
  purple_request_fields_add_group(fields, group);
  list = purple_request_field_list_new("event", "Choose Target Event");
  purple_request_field_group_add_field(group, list);

  for (i = 0; i < 10; ++i) {
    char key_id[64], key_sender[64], key_msg[64], key_ts[64], key_enc[64],
        key_thread[64];
    const char *event_id = NULL;
    const char *sender = NULL;
    const char *msg = NULL;
    const char *ts = NULL;
    const char *enc = NULL;
    const char *thread = NULL;
    char tbuf[32];
    char *label = NULL;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d",
               i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    g_snprintf(key_ts, sizeof(key_ts), "matrix_recent_event_ts_%d", i);
    g_snprintf(key_enc, sizeof(key_enc), "matrix_recent_event_enc_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);
    event_id = purple_conversation_get_data(conv, key_id);
    if (!event_id || !*event_id)
      continue;
    sender = purple_conversation_get_data(conv, key_sender);
    msg = purple_conversation_get_data(conv, key_msg);
    ts = purple_conversation_get_data(conv, key_ts);
    enc = purple_conversation_get_data(conv, key_enc);
    thread = purple_conversation_get_data(conv, key_thread);
    if (ts && *ts) {
      guint64 ms = g_ascii_strtoull(ts, NULL, 10);
      time_t secs = (time_t)(ms / 1000);
      struct tm *tm_info = localtime(&secs);
      if (tm_info) {
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);
      } else {
        g_strlcpy(tbuf, "unknown", sizeof(tbuf));
      }
    } else {
      g_strlcpy(tbuf, "unknown", sizeof(tbuf));
    }
    label = g_strdup_printf("%d. %s: %s [%s] %s%s @%s", i + 1,
                            (sender && *sender) ? sender : "user",
                            (msg && *msg) ? msg : "(no text)", event_id,
                            (enc && strcmp(enc, "1") == 0) ? "[E2EE] " : "",
                            (thread && *thread) ? "[thread] " : "", tbuf);
    purple_request_field_list_add(list, label, g_strdup(event_id));
    if (!selected_label && last_event_id && *last_event_id &&
        strcmp(last_event_id, event_id) == 0) {
      selected_label = g_strdup(label);
    }
    g_free(label);
    added++;
  }

  if (added == 0) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id && *last_id) {
      purple_request_field_list_add(list, last_id, g_strdup(last_id));
      added = 1;
    }
  }

  if (added == 0) {
    if (selected_label)
      g_free(selected_label);
    purple_notify_error(my_plugin, "Event Target", "No event history available",
                        "No recent events are cached for this room yet.");
    return;
  }
  if (selected_label) {
    purple_request_field_list_add_selected(list, selected_label);
    g_free(selected_label);
  }

  ctx = g_new0(MatrixUiEventPickCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->action = action;
  purple_request_fields(my_plugin, title, heading, NULL, fields, "_Select",
                        G_CALLBACK(matrix_ui_event_pick_selected_cb), "_Cancel",
                        NULL, account, NULL, conv, ctx);
}

typedef struct {
  char *room_id;
  int action_map[30];
} MatrixUiInspectorCtx;

static void matrix_ui_set_inspector_last_event_id(const char *room_id,
                                                  const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  if (!account || !room_id || !*room_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return;
  g_free(purple_conversation_get_data(conv, "matrix_inspector_last_event_id"));
  purple_conversation_set_data(conv, "matrix_inspector_last_event_id",
                               event_id ? g_strdup(event_id) : NULL);
}

static void matrix_ui_set_picker_last_event_id(const char *room_id,
                                               const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  if (!account || !room_id || !*room_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return;
  g_free(purple_conversation_get_data(conv, "matrix_picker_last_event_id"));
  purple_conversation_set_data(conv, "matrix_picker_last_event_id",
                               event_id ? g_strdup(event_id) : NULL);
}

static const char *matrix_ui_get_picker_last_event_id(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  if (!account || !room_id || !*room_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  return purple_conversation_get_data(conv, "matrix_picker_last_event_id");
}

static const char *matrix_ui_get_inspector_last_event_id(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  if (!account || !room_id || !*room_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  return purple_conversation_get_data(conv, "matrix_inspector_last_event_id");
}

static void matrix_ui_message_inspector_exec_cb(void *user_data,
                                                PurpleRequestFields *fields) {
  MatrixUiInspectorCtx *ctx = (MatrixUiInspectorCtx *)user_data;
  PurpleRequestField *event_field = NULL;
  PurpleRequestField *action_field = NULL;
  GList *selected = NULL;
  const char *event_id = NULL;
  int action_choice = 0;
  MatrixEventPickAction action = MATRIX_EVENT_PICK_REPLY;
  char *sender_user_id = NULL;

  if (!ctx || !ctx->room_id)
    goto out;
  event_field = purple_request_fields_get_field(fields, "event");
  action_field = purple_request_fields_get_field(fields, "action");
  if (!event_field || !action_field)
    goto out;
  selected = purple_request_field_list_get_selected(event_field);
  if (!selected)
    goto out;
  event_id = (const char *)purple_request_field_list_get_data(
      event_field, (const char *)selected->data);
  if (!event_id || !*event_id)
    goto out;

  action_choice = purple_request_field_choice_get_value(action_field);
  action_choice = ctx->action_map[action_choice];

  matrix_ui_set_inspector_last_event_id(ctx->room_id, event_id);
  switch (action_choice) {
  case 0:
    action = MATRIX_EVENT_PICK_REPLY;
    break;
  case 1:
    action = MATRIX_EVENT_PICK_THREAD;
    break;
  case 2:
    action = MATRIX_EVENT_PICK_OPEN_THREAD;
    break;
  case 3:
    action = MATRIX_EVENT_PICK_REACT;
    break;
  case 4:
    action = MATRIX_EVENT_PICK_EDIT;
    break;
  case 5:
    action = MATRIX_EVENT_PICK_REDACT;
    break;
  case 6:
    action = MATRIX_EVENT_PICK_REPORT;
    break;
  case 7:
    action = MATRIX_EVENT_PICK_USER_INFO;
    break;
  case 8:
    action = MATRIX_EVENT_PICK_MODERATE_USER;
    break;
  case 9:
    action = MATRIX_EVENT_PICK_IGNORE_USER;
    break;
  case 10:
    action = MATRIX_EVENT_PICK_UNIGNORE_USER;
    break;
  case 11:
    action = MATRIX_EVENT_PICK_OPEN_DM;
    break;
  case 12:
    action = MATRIX_EVENT_PICK_COPY_EVENT_ID;
    break;
  case 13:
    action = MATRIX_EVENT_PICK_COPY_SENDER_ID;
    break;
  case 14:
    action = MATRIX_EVENT_PICK_COPY_ROOM_ID;
    break;
  case 15:
    action = MATRIX_EVENT_PICK_SHOW_EVENT_DETAILS;
    break;
  case 16:
    action = MATRIX_EVENT_PICK_COPY_THREAD_ROOT_ID;
    break;
  case 17:
    action = MATRIX_EVENT_PICK_MARK_READ;
    break;
  case 18:
    action = MATRIX_EVENT_PICK_END_POLL;
    break;
  default:
    action = MATRIX_EVENT_PICK_REPLY;
    break;
  }

  if (action == MATRIX_EVENT_PICK_USER_INFO ||
      action == MATRIX_EVENT_PICK_MODERATE_USER ||
      action == MATRIX_EVENT_PICK_IGNORE_USER ||
      action == MATRIX_EVENT_PICK_UNIGNORE_USER ||
      action == MATRIX_EVENT_PICK_COPY_SENDER_ID ||
      action == MATRIX_EVENT_PICK_OPEN_DM ||
      action == MATRIX_EVENT_PICK_MARK_READ ||
      action == MATRIX_EVENT_PICK_END_POLL) {
    sender_user_id =
        matrix_ui_event_sender_for_room_event(ctx->room_id, event_id);
    if (!sender_user_id || !*sender_user_id)
      goto out;
    if (action == MATRIX_EVENT_PICK_USER_INFO) {
      matrix_ui_action_user_info(ctx->room_id, sender_user_id);
    } else if (action == MATRIX_EVENT_PICK_MODERATE_USER) {
      matrix_ui_action_moderate_user(ctx->room_id, sender_user_id);
    } else if (action == MATRIX_EVENT_PICK_IGNORE_USER) {
      matrix_ui_action_ignore_user(ctx->room_id, sender_user_id);
    } else if (action == MATRIX_EVENT_PICK_UNIGNORE_USER) {
      matrix_ui_action_unignore_user(ctx->room_id, sender_user_id);
    } else if (action == MATRIX_EVENT_PICK_OPEN_DM) {
      if (sender_user_id[0] == '@') {
        matrix_ui_open_dm_for_user(sender_user_id);
      } else {
        purple_notify_error(
            my_plugin, "Open DM", "Invalid Matrix user ID",
            "Selected event sender is not a valid Matrix user ID.");
      }
    } else if (action == MATRIX_EVENT_PICK_COPY_SENDER_ID) {
      purple_notify_info(my_plugin, "Sender ID", "Selected Event Sender",
                         sender_user_id);
    } else if (action == MATRIX_EVENT_PICK_MARK_READ) {
      PurpleAccount *account = find_matrix_account();
      purple_matrix_rust_send_read_receipt(purple_account_get_username(account),
                                           ctx->room_id, event_id);
      purple_matrix_rust_mark_unread(purple_account_get_username(account),
                                     ctx->room_id, FALSE);
    }
    goto out;
  } else if (action == MATRIX_EVENT_PICK_END_POLL) {
    PurpleAccount *account = find_matrix_account();
    purple_matrix_rust_poll_end(purple_account_get_username(account),
                                ctx->room_id, event_id, "The poll has ended.");
    goto out;
  }

  matrix_ui_open_event_action_dialog(ctx->room_id, event_id, action);
out:
  if (sender_user_id)
    g_free(sender_user_id);
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx);
  }
}

void matrix_ui_action_message_inspector(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  PurpleRequestFields *fields = NULL;
  PurpleRequestFieldGroup *group = NULL;
  PurpleRequestField *event_list = NULL;
  PurpleRequestField *action_choice = NULL;
  MatrixUiInspectorCtx *ctx = NULL;
  const char *last_event_id = NULL;
  char *selected_label = NULL;
  int i;
  int added = 0;

  if (!account || !room_id || !*room_id)
    return;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return;
  last_event_id = matrix_ui_get_inspector_last_event_id(room_id);

  fields = purple_request_fields_new();
  group = purple_request_field_group_new("Target");
  purple_request_fields_add_group(fields, group);
  event_list = purple_request_field_list_new("event", "Recent Event");
  purple_request_field_group_add_field(group, event_list);

  for (i = 0; i < 10; ++i) {
    char key_id[64], key_sender[64], key_msg[64], key_ts[64], key_enc[64],
        key_thread[64];
    const char *event_id = NULL;
    const char *sender = NULL;
    const char *msg = NULL;
    const char *ts = NULL;
    const char *enc = NULL;
    const char *thread = NULL;
    char tbuf[32];
    char *label = NULL;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d",
               i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    g_snprintf(key_ts, sizeof(key_ts), "matrix_recent_event_ts_%d", i);
    g_snprintf(key_enc, sizeof(key_enc), "matrix_recent_event_enc_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);
    event_id = purple_conversation_get_data(conv, key_id);
    if (!event_id || !*event_id)
      continue;
    sender = purple_conversation_get_data(conv, key_sender);
    msg = purple_conversation_get_data(conv, key_msg);
    ts = purple_conversation_get_data(conv, key_ts);
    enc = purple_conversation_get_data(conv, key_enc);
    thread = purple_conversation_get_data(conv, key_thread);
    if (ts && *ts) {
      guint64 ms = g_ascii_strtoull(ts, NULL, 10);
      time_t secs = (time_t)(ms / 1000);
      struct tm *tm_info = localtime(&secs);
      if (tm_info) {
        strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);
      } else {
        g_strlcpy(tbuf, "unknown", sizeof(tbuf));
      }
    } else {
      g_strlcpy(tbuf, "unknown", sizeof(tbuf));
    }
    label = g_strdup_printf("%d. %s: %s [%s] %s%s @%s", i + 1,
                            (sender && *sender) ? sender : "user",
                            (msg && *msg) ? msg : "(no text)", event_id,
                            (enc && strcmp(enc, "1") == 0) ? "[E2EE] " : "",
                            (thread && *thread) ? "[thread] " : "", tbuf);
    purple_request_field_list_add(event_list, label, g_strdup(event_id));
    if (!selected_label && last_event_id && *last_event_id &&
        strcmp(last_event_id, event_id) == 0) {
      selected_label = g_strdup(label);
    }
    g_free(label);
    added++;
  }

  if (added == 0) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id && *last_id) {
      purple_request_field_list_add(event_list, last_id, g_strdup(last_id));
      added = 1;
    }
  }

  if (added == 0) {
    purple_notify_error(my_plugin, "Message Inspector",
                        "No event history available",
                        "No recent events are cached for this room yet.");
    return;
  }
  if (selected_label) {
    purple_request_field_list_add_selected(event_list, selected_label);
    g_free(selected_label);
  }

  const char *can_redact_str =
      purple_conversation_get_data(conv, "matrix_power_redact");
  const char *can_kick_str =
      purple_conversation_get_data(conv, "matrix_power_kick");
  const char *can_ban_str =
      purple_conversation_get_data(conv, "matrix_power_ban");
  gboolean can_redact = can_redact_str && strcmp(can_redact_str, "1") == 0;
  gboolean can_kick = can_kick_str && strcmp(can_kick_str, "1") == 0;
  gboolean can_ban = can_ban_str && strcmp(can_ban_str, "1") == 0;
  gboolean can_mod = can_kick || can_ban;
  int map_idx = 0;

  ctx = g_new0(MatrixUiInspectorCtx, 1);
  ctx->room_id = g_strdup(room_id);

  action_choice = purple_request_field_choice_new("action", "Action", 0);
  purple_request_field_choice_add(action_choice, "Reply");
  ctx->action_map[map_idx++] = 0;
  purple_request_field_choice_add(action_choice, "Start Thread");
  ctx->action_map[map_idx++] = 1;
  purple_request_field_choice_add(action_choice, "Open Thread");
  ctx->action_map[map_idx++] = 2;
  purple_request_field_choice_add(action_choice, "React");
  ctx->action_map[map_idx++] = 3;
  purple_request_field_choice_add(action_choice, "Edit");
  ctx->action_map[map_idx++] = 4;
  if (can_redact) {
    purple_request_field_choice_add(action_choice, "Redact");
    ctx->action_map[map_idx++] = 5;
  }
  purple_request_field_choice_add(action_choice, "Report");
  ctx->action_map[map_idx++] = 6;
  purple_request_field_choice_add(action_choice, "Sender Info");
  ctx->action_map[map_idx++] = 7;
  if (can_mod) {
    purple_request_field_choice_add(action_choice, "Moderate Sender");
    ctx->action_map[map_idx++] = 8;
  }
  purple_request_field_choice_add(action_choice, "Ignore Sender");
  ctx->action_map[map_idx++] = 9;
  purple_request_field_choice_add(action_choice, "Unignore Sender");
  ctx->action_map[map_idx++] = 10;
  purple_request_field_choice_add(action_choice, "Open DM with Sender");
  ctx->action_map[map_idx++] = 11;
  purple_request_field_choice_add(action_choice, "Copy/Show Event ID");
  ctx->action_map[map_idx++] = 12;
  purple_request_field_choice_add(action_choice, "Copy/Show Sender ID");
  ctx->action_map[map_idx++] = 13;
  purple_request_field_choice_add(action_choice, "Copy/Show Room ID");
  ctx->action_map[map_idx++] = 14;
  purple_request_field_choice_add(action_choice, "Show Event Details");
  ctx->action_map[map_idx++] = 15;
  purple_request_field_choice_add(action_choice, "Copy/Show Thread Root ID");
  ctx->action_map[map_idx++] = 16;
  purple_request_field_choice_add(action_choice, "Mark Read to Here");
  ctx->action_map[map_idx++] = 17;
  purple_request_field_choice_add(action_choice, "End Poll");
  ctx->action_map[map_idx++] = 18;
  purple_request_field_group_add_field(group, action_choice);
  purple_request_fields(my_plugin, "Message Inspector",
                        "Choose Event and Action", NULL, fields, "_Run",
                        G_CALLBACK(matrix_ui_message_inspector_exec_cb),
                        "_Cancel", NULL, account, NULL, conv, ctx);
}

void matrix_ui_action_show_last_event_details(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  const char *event_id = NULL;
  if (!account || !room_id || !*room_id)
    return;
  event_id = matrix_last_event_for_room(account, room_id);
  if (!event_id || !*event_id) {
    purple_notify_error(my_plugin, "Last Event Details", "No recent event",
                        "No recent event ID is available in this room.");
    return;
  }
  matrix_ui_show_event_details(room_id, event_id);
}

void matrix_ui_action_react_latest(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  const char *event_id = NULL;
  if (!account || !room_id || !*room_id)
    return;
  event_id = matrix_last_event_for_room(account, room_id);
  if (!event_id || !*event_id) {
    purple_notify_error(my_plugin, "React", "No recent event",
                        "No recent event ID is available in this room.");
    return;
  }
  matrix_ui_open_reaction_picker(room_id, event_id);
}

void matrix_ui_action_list_polls(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_matrix_rust_list_polls(purple_account_get_username(account), room_id);
}

typedef struct {
  char *room_id;
  double lat;
} MatrixUiLocationCtx;

static void matrix_ui_location_lon_cb(void *user_data, const char *lon_str) {
  MatrixUiLocationCtx *ctx = (MatrixUiLocationCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  char *end = NULL;
  double lon = 0.0;
  if (ctx && lon_str && *lon_str) {
    lon = g_ascii_strtod(lon_str, &end);
    if (account && end && *end == '\0' && ctx->room_id && *ctx->room_id) {
      purple_matrix_rust_send_location(purple_account_get_username(account),
                                       ctx->room_id, ctx->lat, lon);
    }
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx);
  }
}

static void matrix_ui_location_lat_cb(void *user_data, const char *lat_str) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  char *end = NULL;
  double lat = 0.0;
  MatrixUiLocationCtx *ctx = NULL;
  if (!account || !room_id || !lat_str || !*lat_str) {
    g_free(room_id);
    return;
  }
  lat = g_ascii_strtod(lat_str, &end);
  if (!end || *end != '\0') {
    purple_notify_error(my_plugin, "Location", "Invalid latitude",
                        "Latitude must be a decimal number.");
    g_free(room_id);
    return;
  }
  ctx = g_new0(MatrixUiLocationCtx, 1);
  ctx->room_id = room_id;
  ctx->lat = lat;
  purple_request_input(my_plugin, "Send Location", "Longitude",
                       "Enter longitude value.", NULL, FALSE, FALSE, NULL,
                       "_Send", G_CALLBACK(matrix_ui_location_lon_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_send_location(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Send Location", "Latitude",
                       "Enter latitude value.", NULL, FALSE, FALSE, NULL,
                       "_Next", G_CALLBACK(matrix_ui_location_lat_cb),
                       "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

void matrix_ui_action_who_read(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_matrix_rust_who_read(purple_account_get_username(account), room_id);
}

typedef struct {
  char *room_id;
  char *event_id;
} MatrixUiReportCtx;

static char *matrix_ui_event_body_for_room_event(const char *room_id,
                                                 const char *event_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = NULL;
  int i;
  if (!account || !room_id || !*room_id || !event_id || !*event_id)
    return NULL;
  conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id,
                                               account);
  if (!conv)
    return NULL;
  for (i = 0; i < 10; ++i) {
    char key_id[64];
    char key_msg[64];
    const char *id = NULL;
    const char *msg = NULL;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    id = purple_conversation_get_data(conv, key_id);
    if (!id || strcmp(id, event_id) != 0)
      continue;
    msg = purple_conversation_get_data(conv, key_msg);
    return g_strdup(msg);
  }
  return NULL;
}

static void matrix_ui_report_reason_cb(void *user_data,
                                       PurpleRequestFields *fields) {
  MatrixUiReportCtx *ctx = (MatrixUiReportCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id) {
    PurpleRequestField *f_reason_preset =
        purple_request_fields_get_field(fields, "reason_preset");
    const char *details = purple_request_fields_get_string(fields, "details");
    int preset_idx = purple_request_field_choice_get_value(f_reason_preset);

    char *final_reason = NULL;
    const char *preset_str = "";
    switch (preset_idx) {
    case 0:
      preset_str = "Spam";
      break;
    case 1:
      preset_str = "Abuse / Harassment";
      break;
    case 2:
      preset_str = "Inappropriate Content";
      break;
    case 3:
      preset_str = "Other";
      break;
    }

    if (details && *details) {
      final_reason = g_strdup_printf("%s: %s", preset_str, details);
    } else {
      final_reason = g_strdup(preset_str);
    }

    purple_matrix_rust_report_content(purple_account_get_username(account),
                                      ctx->room_id, ctx->event_id,
                                      final_reason);
    purple_notify_info(
        my_plugin, "Report Event", "Report Submitted",
        "Your report has been sent to the server for moderation.");
    g_free(final_reason);
  }

  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_report_event_cb(void *user_data, const char *event_id) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  MatrixUiReportCtx *ctx = NULL;

  if (!account || !room_id || !event_id || !*event_id) {
    g_free(room_id);
    return;
  }

  ctx = g_new0(MatrixUiReportCtx, 1);
  ctx->room_id = room_id;
  ctx->event_id = g_strdup(event_id);

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Report Details");
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *f_preset =
      purple_request_field_choice_new("reason_preset", "Reason", 0);
  purple_request_field_choice_add(f_preset, "Spam");
  purple_request_field_choice_add(f_preset, "Abuse / Harassment");
  purple_request_field_choice_add(f_preset, "Inappropriate Content");
  purple_request_field_choice_add(f_preset, "Other");
  purple_request_field_group_add_field(group, f_preset);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new(
                 "details", "Additional Details (Optional)", NULL, TRUE));

  purple_request_fields(my_plugin, "Report Content",
                        "Report Event to Moderation Hub",
                        "Select a reason and provide optional details.", fields,
                        "_Report", G_CALLBACK(matrix_ui_report_reason_cb),
                        "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_report_event(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Report Content", "Event ID",
                       "Enter event_id to report.", NULL, FALSE, FALSE, NULL,
                       "_Next", G_CALLBACK(matrix_ui_report_event_cb),
                       "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

void matrix_ui_action_reply_pick_event(const char *room_id) {
  matrix_ui_event_pick_show(room_id, MATRIX_EVENT_PICK_REPLY, "Reply to Event",
                            "Select Event to Reply To");
}

void matrix_ui_action_thread_pick_event(const char *room_id) {
  matrix_ui_event_pick_show(room_id, MATRIX_EVENT_PICK_THREAD, "Start Thread",
                            "Select Root Event");
}

void matrix_ui_action_react_pick_event(const char *room_id) {
  matrix_ui_event_pick_show(room_id, MATRIX_EVENT_PICK_REACT, "React to Event",
                            "Select Target Event");
}

void matrix_ui_action_edit_pick_event(const char *room_id) {
  matrix_ui_event_pick_show(room_id, MATRIX_EVENT_PICK_EDIT, "Edit Event",
                            "Select Target Event");
}

void matrix_ui_action_redact_pick_event(const char *room_id) {
  matrix_ui_event_pick_show(room_id, MATRIX_EVENT_PICK_REDACT, "Redact Event",
                            "Select Target Event");
}

void matrix_ui_action_report_pick_event(const char *room_id) {
  matrix_ui_event_pick_show(room_id, MATRIX_EVENT_PICK_REPORT, "Report Event",
                            "Select Target Event");
}

static void matrix_ui_search_users_cb(void *user_data, const char *term) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && term && *term) {
    purple_matrix_rust_search_users(purple_account_get_username(account), term,
                                    room_id ? room_id : "");
  }
  g_free(room_id);
}

void matrix_ui_action_search_users(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  purple_request_input(my_plugin, "Search Users", "Matrix User Search",
                       "Search by user ID or display name.", NULL, FALSE, FALSE,
                       NULL, "_Search", G_CALLBACK(matrix_ui_search_users_cb),
                       "_Cancel", NULL, account, NULL, NULL,
                       g_strdup(room_id ? room_id : ""));
}

static void matrix_ui_search_public_cb(void *user_data, const char *term) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && term && *term) {
    purple_roomlist_show_with_account(account);
    purple_matrix_rust_search_public_rooms(purple_account_get_username(account),
                                           term);
  }
  g_free(room_id);
}

void matrix_ui_action_search_public(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  purple_request_input(my_plugin, "Search Public Rooms", "Public Room Search",
                       "Search public Matrix rooms.", NULL, FALSE, FALSE, NULL,
                       "_Search", G_CALLBACK(matrix_ui_search_public_cb),
                       "_Cancel", NULL, account, NULL, NULL,
                       g_strdup(room_id ? room_id : ""));
}

typedef struct {
  char *room_id;
  bool search_members;
} MatrixUiGlobalRoomSearchCtx;

static void matrix_ui_global_room_search_term_cb(void *user_data,
                                                 const char *term) {
  MatrixUiGlobalRoomSearchCtx *ctx = (MatrixUiGlobalRoomSearchCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && term && *term) {
    if (ctx->search_members) {
      purple_matrix_rust_search_room_members(
          purple_account_get_username(account), ctx->room_id, term);
    } else {
      purple_matrix_rust_search_room_messages(
          purple_account_get_username(account), ctx->room_id, term);
    }
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx);
  }
}

static void matrix_ui_global_room_search_room_cb(void *user_data,
                                                 const char *room_id) {
  MatrixUiGlobalRoomSearchCtx *ctx = (MatrixUiGlobalRoomSearchCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *title = NULL;
  const char *heading = NULL;
  const char *desc = NULL;
  if (!account || !ctx || !room_id || !*room_id) {
    if (ctx)
      g_free(ctx);
    return;
  }
  ctx->room_id = g_strdup(room_id);
  title = ctx->search_members ? "Search Members" : "Search Messages";
  heading = ctx->search_members ? "Member Search Term" : "Message Search Term";
  desc = ctx->search_members ? "Search users in the specified room."
                             : "Search recent messages in the specified room.";
  purple_request_input(my_plugin, title, heading, desc, NULL, FALSE, FALSE,
                       NULL, "_Search",
                       G_CALLBACK(matrix_ui_global_room_search_term_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_search_members_global(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  MatrixUiGlobalRoomSearchCtx *ctx = NULL;
  (void)room_id;
  if (!account)
    return;
  ctx = g_new0(MatrixUiGlobalRoomSearchCtx, 1);
  ctx->search_members = true;
  purple_request_input(my_plugin, "Search Members", "Room ID",
                       "Enter Matrix room ID (example: !room:server.tld).",
                       NULL, FALSE, FALSE, NULL, "_Next",
                       G_CALLBACK(matrix_ui_global_room_search_room_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_search_room_global(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  MatrixUiGlobalRoomSearchCtx *ctx = NULL;
  (void)room_id;
  if (!account)
    return;
  ctx = g_new0(MatrixUiGlobalRoomSearchCtx, 1);
  ctx->search_members = false;
  purple_request_input(my_plugin, "Search Messages", "Room ID",
                       "Enter Matrix room ID (example: !room:server.tld).",
                       NULL, FALSE, FALSE, NULL, "_Next",
                       G_CALLBACK(matrix_ui_global_room_search_room_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_discover_public_rooms(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_matrix_rust_fetch_public_rooms_for_list(
      purple_account_get_username(account));
}

static void matrix_ui_room_preview_target_cb(void *user_data,
                                             const char *room_or_alias) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (account && room_or_alias && *room_or_alias) {
    purple_matrix_rust_fetch_room_preview(purple_account_get_username(account),
                                          room_or_alias);
  }
}

void matrix_ui_action_discover_room_preview(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_input(my_plugin, "Room Preview", "Room ID or Alias",
                       "Example: #room:server.tld or !room:server.tld", NULL,
                       FALSE, FALSE, NULL, "_Preview",
                       G_CALLBACK(matrix_ui_room_preview_target_cb), "_Cancel",
                       NULL, account, NULL, NULL, NULL);
}

typedef struct {
  char *room_id;
  char *event_id;
} MatrixUiEventTargetCtx;

static void matrix_ui_redact_reason_cb(void *user_data, const char *reason) {
  MatrixUiEventTargetCtx *ctx = (MatrixUiEventTargetCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id) {
    purple_matrix_rust_redact_event(purple_account_get_username(account),
                                    ctx->room_id, ctx->event_id, reason);
    purple_notify_info(my_plugin, "Redact Event", "Redaction Submitted",
                       "The redaction request has been sent to the server.");
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_redact_event_id_cb(void *user_data,
                                         const char *event_id) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  MatrixUiEventTargetCtx *ctx = NULL;
  char *msg = NULL;
  char *preview = NULL;

  if (!account || !room_id || !event_id || !*event_id) {
    g_free(room_id);
    return;
  }
  ctx = g_new0(MatrixUiEventTargetCtx, 1);
  ctx->room_id = room_id;
  ctx->event_id = g_strdup(event_id);

  msg = matrix_ui_event_body_for_room_event(room_id, event_id);
  preview =
      g_strdup_printf("Are you sure you want to redact this event?\n\n\"%s\"",
                      msg ? msg : "(content not cached locally)");

  purple_request_input(my_plugin, "Redact Event", "Reason for Redaction",
                       preview, NULL, FALSE, FALSE, NULL, "_Redact",
                       G_CALLBACK(matrix_ui_redact_reason_cb), "_Cancel", NULL,
                       account, NULL, NULL, ctx);

  if (msg)
    g_free(msg);
  g_free(preview);
}

void matrix_ui_action_redact_event_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Redact Event", "Event ID",
                       "Enter event_id to redact.", NULL, FALSE, FALSE, NULL,
                       "_Next", G_CALLBACK(matrix_ui_redact_event_id_cb),
                       "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

void matrix_ui_action_redact_latest(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  const char *event_id = NULL;
  MatrixUiEventTargetCtx *ctx = NULL;
  char *msg = NULL;
  char *preview = NULL;

  if (!account || !room_id || !*room_id)
    return;
  event_id = matrix_last_event_for_room(account, room_id);
  if (!event_id || !*event_id) {
    purple_notify_error(my_plugin, "Redact", "No recent event",
                        "No recent event ID is available in this room.");
    return;
  }
  ctx = g_new0(MatrixUiEventTargetCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);

  msg = matrix_ui_event_body_for_room_event(room_id, event_id);
  preview =
      g_strdup_printf("Are you sure you want to redact this event?\n\n\"%s\"",
                      msg ? msg : "(content not cached locally)");

  purple_request_input(my_plugin, "Redact Latest Event", "Reason for Redaction",
                       preview, NULL, FALSE, FALSE, NULL, "_Redact",
                       G_CALLBACK(matrix_ui_redact_reason_cb), "_Cancel", NULL,
                       account, NULL, NULL, ctx);

  if (msg)
    g_free(msg);
  g_free(preview);
}

static void matrix_ui_edit_text_cb(void *user_data, const char *text) {
  MatrixUiEventTargetCtx *ctx = (MatrixUiEventTargetCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->room_id && ctx->event_id && text && *text) {
    purple_matrix_rust_send_edit(purple_account_get_username(account),
                                 ctx->room_id, ctx->event_id, text);
  }
  if (ctx) {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_edit_event_id_cb(void *user_data, const char *event_id) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  MatrixUiEventTargetCtx *ctx = NULL;
  if (!account || !room_id || !event_id || !*event_id) {
    g_free(room_id);
    return;
  }
  ctx = g_new0(MatrixUiEventTargetCtx, 1);
  ctx->room_id = room_id;
  ctx->event_id = g_strdup(event_id);
  purple_request_input(my_plugin, "Edit Event", "New Message Text",
                       "Enter replacement text.", NULL, TRUE, FALSE, NULL,
                       "_Send Edit", G_CALLBACK(matrix_ui_edit_text_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_edit_event_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Edit Event", "Event ID",
                       "Enter event_id to edit.", NULL, FALSE, FALSE, NULL,
                       "_Next", G_CALLBACK(matrix_ui_edit_event_id_cb),
                       "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

void matrix_ui_action_edit_latest(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  const char *event_id = NULL;
  MatrixUiEventTargetCtx *ctx = NULL;
  if (!account || !room_id || !*room_id)
    return;
  event_id = matrix_last_event_for_room(account, room_id);
  if (!event_id || !*event_id) {
    purple_notify_error(my_plugin, "Edit", "No recent event",
                        "No recent event ID is available in this room.");
    return;
  }
  ctx = g_new0(MatrixUiEventTargetCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);
  purple_request_input(my_plugin, "Edit Latest Event", "New Message Text",
                       "Enter replacement text.", NULL, TRUE, FALSE, NULL,
                       "_Send Edit", G_CALLBACK(matrix_ui_edit_text_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_report_latest(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  const char *event_id = NULL;
  MatrixUiReportCtx *ctx = NULL;
  if (!account || !room_id || !*room_id)
    return;
  event_id = matrix_last_event_for_room(account, room_id);
  if (!event_id || !*event_id) {
    purple_notify_error(my_plugin, "Report", "No recent event",
                        "No recent event ID is available in this room.");
    return;
  }
  ctx = g_new0(MatrixUiReportCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);
  purple_request_input(my_plugin, "Report Latest Event", "Reason",
                       "Optional reason for report.", NULL, FALSE, FALSE, NULL,
                       "_Report", G_CALLBACK(matrix_ui_report_reason_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_versions(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_matrix_rust_get_supported_versions(
      purple_account_get_username(account));
}

void matrix_ui_action_enable_key_backup(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_matrix_rust_enable_key_backup(purple_account_get_username(account));
}

void matrix_ui_action_my_profile(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_matrix_rust_get_my_profile(purple_account_get_username(account));
}

void matrix_ui_action_server_info(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_matrix_rust_get_server_info(purple_account_get_username(account));
}

void matrix_ui_action_resync_recent_history(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_matrix_rust_resync_recent_history(purple_account_get_username(account),
                                           room_id);
}

static void matrix_ui_search_stickers_cb(void *user_data, const char *term) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (!account || !term || !*term)
    return;
  purple_matrix_rust_search_stickers(purple_account_get_username(account),
                                     term);
}

void matrix_ui_action_search_stickers(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_input(my_plugin, "Sticker Search", "Find Stickers",
                       "Search across available sticker packs.", NULL, FALSE,
                       FALSE, NULL, "_Search",
                       G_CALLBACK(matrix_ui_search_stickers_cb), "_Cancel",
                       NULL, account, NULL, NULL, NULL);
}

static void matrix_ui_recover_keys_cb(void *user_data, const char *passphrase) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (!account || !passphrase || !*passphrase)
    return;
  purple_matrix_rust_recover_keys(purple_account_get_username(account),
                                  passphrase);
}

void matrix_ui_action_recover_keys_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_input(
      my_plugin, "Recover Room Keys", "Recovery Passphrase",
      "Enter passphrase used for encrypted key backup recovery.", NULL, FALSE,
      TRUE, NULL, "_Recover", G_CALLBACK(matrix_ui_recover_keys_cb), "_Cancel",
      NULL, account, NULL, NULL, NULL);
}

typedef struct {
  char *path;
} MatrixUiExportKeysCtx;

static void matrix_ui_export_keys_passphrase_cb(void *user_data,
                                                const char *passphrase) {
  MatrixUiExportKeysCtx *ctx = (MatrixUiExportKeysCtx *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && ctx && ctx->path && passphrase && *passphrase) {
    purple_matrix_rust_export_keys(purple_account_get_username(account),
                                   ctx->path, passphrase);
  }
  if (ctx) {
    g_free(ctx->path);
    g_free(ctx);
  }
}

static void matrix_ui_export_keys_path_cb(void *user_data, const char *path) {
  PurpleAccount *account = find_matrix_account();
  MatrixUiExportKeysCtx *ctx = NULL;
  (void)user_data;
  if (!account || !path || !*path)
    return;
  ctx = g_new0(MatrixUiExportKeysCtx, 1);
  ctx->path = g_strdup(path);
  purple_request_input(my_plugin, "Export Room Keys", "Passphrase",
                       "Choose a passphrase used to encrypt the export file.",
                       NULL, FALSE, TRUE, NULL, "_Export",
                       G_CALLBACK(matrix_ui_export_keys_passphrase_cb),
                       "_Cancel", NULL, account, NULL, NULL, ctx);
}

void matrix_ui_action_export_keys_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_file(my_plugin, "Export Keys File Path", NULL, TRUE,
                      G_CALLBACK(matrix_ui_export_keys_path_cb), NULL, account,
                      NULL, NULL, NULL);
}

static void matrix_ui_set_avatar_file_cb(void *user_data,
                                         const char *filename) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (!account || !filename || !*filename)
    return;
  purple_matrix_rust_set_avatar(purple_account_get_username(account), filename);
}

void matrix_ui_action_set_avatar_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_file(my_plugin, "Select Avatar Image", NULL, FALSE,
                      G_CALLBACK(matrix_ui_set_avatar_file_cb), NULL, account,
                      NULL, NULL, NULL);
}

static void matrix_ui_room_tag_cb(void *user_data, const char *tag) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id && tag && *tag) {
    purple_matrix_rust_set_room_tag(purple_account_get_username(account),
                                    room_id, tag);
  }
  g_free(room_id);
}

void matrix_ui_action_room_tag_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Room Tag", "Set Room Tag",
                       "Examples: m.favourite, m.lowpriority", NULL, FALSE,
                       FALSE, NULL, "_Set", G_CALLBACK(matrix_ui_room_tag_cb),
                       "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

static void matrix_ui_upgrade_room_cb(void *user_data, const char *version) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id && version && *version) {
    purple_matrix_rust_upgrade_room(purple_account_get_username(account),
                                    room_id, version);
  }
  g_free(room_id);
}

void matrix_ui_action_upgrade_room_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Upgrade Room", "Target Room Version",
                       "Example: 10", NULL, FALSE, FALSE, NULL, "_Upgrade",
                       G_CALLBACK(matrix_ui_upgrade_room_cb), "_Cancel", NULL,
                       account, NULL, NULL, g_strdup(room_id));
}

static void matrix_ui_alias_create_cb(void *user_data, const char *localpart) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (account && room_id && localpart && *localpart) {
    purple_matrix_rust_create_alias(purple_account_get_username(account),
                                    room_id, localpart);
  }
  g_free(room_id);
}

void matrix_ui_action_alias_create_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account || !room_id || !*room_id)
    return;
  purple_request_input(my_plugin, "Create Alias", "Alias Localpart",
                       "Enter only the localpart (without # and server).", NULL,
                       FALSE, FALSE, NULL, "_Create",
                       G_CALLBACK(matrix_ui_alias_create_cb), "_Cancel", NULL,
                       account, NULL, NULL, g_strdup(room_id));
}

static void matrix_ui_alias_delete_cb(void *user_data, const char *alias) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (account && alias && *alias) {
    purple_matrix_rust_delete_alias(purple_account_get_username(account),
                                    alias);
  }
}

void matrix_ui_action_alias_delete_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_input(my_plugin, "Delete Alias", "Matrix Room Alias",
                       "Example: #room:server.tld", NULL, FALSE, FALSE, NULL,
                       "_Delete", G_CALLBACK(matrix_ui_alias_delete_cb),
                       "_Cancel", NULL, account, NULL, NULL, NULL);
}

static void matrix_ui_space_hierarchy_cb(void *user_data,
                                         const char *space_id) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (account && space_id && *space_id) {
    purple_matrix_rust_get_space_hierarchy(purple_account_get_username(account),
                                           space_id);
  }
}

void matrix_ui_action_space_hierarchy_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  purple_request_input(my_plugin, "Space Hierarchy", "Space ID",
                       "Example: !space:server.tld", room_id, FALSE, FALSE,
                       NULL, "_Fetch", G_CALLBACK(matrix_ui_space_hierarchy_cb),
                       "_Cancel", NULL, account, NULL, NULL, NULL);
}

static void matrix_ui_knock_target_cb(void *user_data, const char *target) {
  PurpleAccount *account = find_matrix_account();
  (void)user_data;
  if (account && target && *target) {
    purple_matrix_rust_knock(purple_account_get_username(account), target,
                             NULL);
  }
}

void matrix_ui_action_knock_prompt(const char *room_id) {
  PurpleAccount *account = find_matrix_account();
  (void)room_id;
  if (!account)
    return;
  purple_request_input(my_plugin, "Knock Room", "Room ID or Alias",
                       "Example: !room:server or #alias:server", NULL, FALSE,
                       FALSE, NULL, "_Knock",
                       G_CALLBACK(matrix_ui_knock_target_cb), "_Cancel", NULL,
                       account, NULL, NULL, NULL);
}

void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *name = purple_request_fields_get_string(fields, "name");
  const char *topic = purple_request_fields_get_string(fields, "topic");

  if (account) {
    if (name)
      purple_matrix_rust_set_room_name(purple_account_get_username(account),
                                       room_id, name);
    if (topic)
      purple_matrix_rust_set_room_topic(purple_account_get_username(account),
                                        room_id, topic);

    /* Advanced Settings */
    PurpleRequestField *f_join =
        purple_request_fields_get_field(fields, "join_rule");
    if (f_join) {
      int choice = purple_request_field_choice_get_value(f_join);
      const char *rules[] = {"public", "invite", "knock"};
      if (choice >= 0 && choice < 3)
        purple_matrix_rust_set_room_join_rule(
            purple_account_get_username(account), room_id, rules[choice]);
    }

    PurpleRequestField *f_vis =
        purple_request_fields_get_field(fields, "visibility");
    if (f_vis) {
      int choice = purple_request_field_choice_get_value(f_vis);
      const char *vis[] = {"world_readable", "shared", "invited", "joined"};
      if (choice >= 0 && choice < 4)
        purple_matrix_rust_set_room_history_visibility(
            purple_account_get_username(account), room_id, vis[choice]);
    }

    gboolean guest = purple_request_fields_get_bool(fields, "guest_access");
    purple_matrix_rust_set_room_guest_access(
        purple_account_get_username(account), room_id, guest);
  }
  g_free(room_id);
}

void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data) {
  if (!PURPLE_BLIST_NODE_IS_CHAT(node))
    return;
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group =
      purple_request_field_group_new("Room Settings");
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new("name", "Room Name", chat->alias, FALSE));

  const char *topic = g_hash_table_lookup(components, "topic");
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("topic", "Topic", topic, TRUE));

  /* Security & Access Group */
  PurpleRequestFieldGroup *sec_group =
      purple_request_field_group_new("Security & Access");
  purple_request_fields_add_group(fields, sec_group);

  PurpleRequestField *f_join = purple_request_field_choice_new(
      "join_rule", "Join Rule", 1); /* Default to Invite */
  purple_request_field_choice_add(f_join, "Public");
  purple_request_field_choice_add(f_join, "Invite Only");
  purple_request_field_choice_add(f_join, "Knock Only");
  purple_request_field_group_add_field(sec_group, f_join);

  PurpleRequestField *f_vis = purple_request_field_choice_new(
      "visibility", "History Visibility", 1); /* Default to Shared */
  purple_request_field_choice_add(f_vis, "World Readable");
  purple_request_field_choice_add(f_vis, "Members Only (Shared)");
  purple_request_field_choice_add(f_vis, "Since Invited");
  purple_request_field_choice_add(f_vis, "Since Joined");
  purple_request_field_group_add_field(sec_group, f_vis);

  purple_request_field_group_add_field(
      sec_group, purple_request_field_bool_new("guest_access",
                                               "Allow Guest Access", FALSE));

  const char *enc = g_hash_table_lookup(components, "encrypted");
  char *enc_status = g_strdup_printf(
      "Encryption: %s",
      (enc && strcmp(enc, "1") == 0) ? "Enabled (🔒)" : "Disabled (🔓)");
  purple_request_field_group_add_field(
      sec_group, purple_request_field_label_new("enc_label", enc_status));
  g_free(enc_status);

  /* Permissions Summary Group */
  PurpleRequestFieldGroup *perm_group =
      purple_request_field_group_new("Your Permissions Summary");
  purple_request_fields_add_group(fields, perm_group);

  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_CHAT, room_id, purple_chat_get_account(chat));

  const char *is_admin =
      conv ? purple_conversation_get_data(conv, "matrix_power_admin") : NULL;
  const char *can_kick =
      conv ? purple_conversation_get_data(conv, "matrix_power_kick") : NULL;
  const char *can_ban =
      conv ? purple_conversation_get_data(conv, "matrix_power_ban") : NULL;
  const char *can_redact =
      conv ? purple_conversation_get_data(conv, "matrix_power_redact") : NULL;
  const char *can_invite =
      conv ? purple_conversation_get_data(conv, "matrix_power_invite") : NULL;

  char *perm_text = g_strdup_printf(
      "Admin: %s\n"
      "Can Kick: %s\n"
      "Can Ban: %s\n"
      "Can Redact: %s\n"
      "Can Invite: %s",
      (is_admin && strcmp(is_admin, "1") == 0) ? "Yes" : "No",
      (can_kick && strcmp(can_kick, "1") == 0) ? "Yes" : "No",
      (can_ban && strcmp(can_ban, "1") == 0) ? "Yes" : "No",
      (can_redact && strcmp(can_redact, "1") == 0) ? "Yes" : "No",
      (can_invite && strcmp(can_invite, "1") == 0) ? "Yes" : "No");

  purple_request_field_group_add_field(
      perm_group, purple_request_field_label_new("perms_label", perm_text));
  g_free(perm_text);

  purple_request_fields(
      my_plugin, "Room Settings", "Edit Matrix Room Details", NULL, fields,
      "_Save", G_CALLBACK(room_settings_dialog_cb), "_Cancel", NULL,
      purple_chat_get_account(chat), NULL, NULL, g_strdup(room_id));
}

/* Commands & Registration */
static PurpleCmdRet cmd_help(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  const char *msg =
      "<b>Matrix Protocol Commands:</b><br/>"
      "<b>General:</b><br/>"
      "  /matrix_login &lt;homeserver&gt; &lt;user&gt; &lt;pass&gt; - Login "
      "with explicit credentials<br/>"
      "  /matrix_sso [homeserver] [user] - Start SSO login (uses account "
      "defaults if omitted)<br/>"
      "  /matrix_clear_session - Clear local session cache for this "
      "account<br/>"
      "  /matrix_create &lt;name&gt; - Create a new private room<br/>"
      "  /matrix_join &lt;id/alias&gt; - Join a Matrix room<br/>"
      "  /matrix_leave - Leave the current Matrix room<br/>"
      "  /dashboard - Open the Room Dashboard for quick tasks<br/>"
      "  /nick &lt;name&gt; - Change your display name on Matrix<br/>"
      "  /join &lt;id/alias&gt; - Join a Matrix room<br/>"
      "  /invite &lt;user_id&gt; - Invite a user to this room<br/>"
      "  /shrug [msg] - Add a ¯\\_(ツ)_/¯ to your message<br/>"
      "<b>Messaging:</b><br/>"
      "  /reply - Reply to the latest message in this room<br/>"
      "  /thread - Start a new thread from the latest message<br/>"
      "  /edit &lt;text&gt; - Edit your most recent message<br/>"
      "  /redact_last - Delete your most recent message<br/>"
      "  /me &lt;action&gt; - Send an emote<br/>"
      "  /sticker - Browse and send Matrix stickers<br/>"
      "  /poll - Create a new poll in this room<br/>"
      "  /polls - List polls and vote by index<br/>"
      "  /react &lt;key&gt; [event_id] - React to a message (defaults to "
      "latest)<br/>"
      "  /location &lt;lat&gt; &lt;lon&gt; - Send location coordinates<br/>"
      "  /message_inspector - Pick recent event and run "
      "reply/thread/react/edit/redact/report<br/>"
      "  /last_event_details - Show details for most recent event in room<br/>"
      "  /mark_read - Mark current room read and send read receipt<br/>"
      "  /power_levels - Show power levels for current room<br/>"
      "<b>Security:</b><br/>"
      "  /matrix_verify [device_id_or_user_id] - Start interactive device "
      "verification<br/>"
      "  /matrix_devices - List your device IDs for verification targeting<br/>"
      "  /matrix_restore_backup &lt;key&gt; - Restore E2EE keys using your "
      "Security Key<br/>"
      "  /matrix_recover_keys &lt;passphrase&gt; - Recover keys from secret "
      "storage passphrase<br/>"
      "  /matrix_export_keys &lt;path&gt; &lt;passphrase&gt; - Export "
      "encrypted key file<br/>"
      "  /matrix_debug_crypto - Show detailed encryption status for this "
      "session<br/>"
      "  /matrix_server_info - Show homeserver capabilities and versions<br/>"
      "  /matrix_profile - Refresh and display your profile info<br/>"
      "<b>Moderation/Admin:</b><br/>"
      "  /report &lt;event_id&gt; [reason] - Report abusive content<br/>"
      "  /bulk_redact &lt;count&gt; [user] - Redact recent events<br/>"
      "  /who_read - List read-receipt users for room<br/>"
      "  /knock &lt;room_or_alias&gt; [reason] - Request access to room<br/>"
      "  /room_tag &lt;tag&gt; - Set room tag<br/>"
      "  /upgrade_room &lt;version&gt; - Upgrade room version<br/>"
      "  /alias_create &lt;localpart&gt;, /alias_delete &lt;alias&gt;<br/>"
      "  /search_users &lt;term&gt;, /search_public &lt;term&gt;, "
      "/search_members &lt;term&gt;<br/>"
      "  /ignore_user &lt;id&gt;, /unignore_user &lt;id&gt;<br/>"
      "  /resync_recent [room_id], /sticker_search &lt;term&gt;, "
      "/matrix_set_avatar &lt;path&gt;";

  purple_conversation_write(conv, "System", msg, PURPLE_MESSAGE_SYSTEM,
                            time(NULL));
  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_sticker(PurpleConversation *conv, const gchar *cmd,
                                gchar **args, gchar **error, void *data) {
  return cmd_sticker_list(conv, cmd, args, error, data);
}
static PurpleCmdRet cmd_poll(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  menu_action_poll_conv_cb(conv, NULL);
  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_reply(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  menu_action_reply_conv_cb(conv, NULL);
  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_thread(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  menu_action_thread_conv_cb(conv, NULL);
  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_dashboard(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  open_room_dashboard(purple_conversation_get_account(conv),
                      purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_nick(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  if (args[0] && *args[0]) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_set_display_name(purple_account_get_username(account),
                                        args[0]);
    return PURPLE_CMD_RET_OK;
  }
  *error = g_strdup("Usage: /nick &lt;new_display_name&gt;");
  return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_join(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /join &lt;room_id_or_alias&gt;");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_join_room(purple_account_get_username(account), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_invite(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /invite &lt;user_id&gt;");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  // Ensure we are in a chat to know WHERE to invite (unless inviter is global
  // context?) If we act on a conversation, usually invite is to THAT room.
  // matrix_chat_invite uses conversation name.
  if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
    purple_matrix_rust_invite_user(purple_account_get_username(account),
                                   purple_conversation_get_name(conv), args[0]);
    return PURPLE_CMD_RET_OK;
  } else {
    // For IM, maybe invite to a new room? Or invite to the DM?
    // Usually /invite is for MUCs.
    // Let's assume it's for the current room if chat.
    *error =
        g_strdup("Invite command is currently only supported in Chat rooms.");
    return PURPLE_CMD_RET_FAILED;
  }
}

static PurpleCmdRet cmd_shrug(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  char *msg = NULL;
  if (args[0] && *args[0]) {
    msg = g_strdup_printf("%s ¯\\_(ツ)_/¯", args[0]);
  } else {
    msg = g_strdup("¯\\_(ツ)_/¯");
  }

  if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
    purple_conv_chat_send(PURPLE_CONV_CHAT(conv), msg);
  } else {
    purple_conv_im_send(PURPLE_CONV_IM(conv), msg);
  }
  g_free(msg);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_restore_backup(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data) {
  if (args[0] && *args[0]) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_restore_backup(purple_account_get_username(account),
                                      args[0]);
    return PURPLE_CMD_RET_OK;
  }
  return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_verify(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  const char *target = NULL;
  if (args && args[0] && *args[0]) {
    target = args[0];
  } else {
    /* Verify own device set by default when no explicit device/user is passed.
     */
    target = purple_account_get_username(account);
  }
  purple_matrix_rust_verify_user(purple_account_get_username(account), target);
  purple_conversation_write(
      conv, "System",
      "Verification request sent. Approve from your other Matrix client.",
      PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_crypto_status(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_debug_crypto_status(purple_account_get_username(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_devices(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_list_own_devices(purple_account_get_username(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_me(PurpleConversation *conv, const gchar *cmd,
                           gchar **args, gchar **error, void *data) {
  if (args[0] && *args[0]) {
    /* Matrix protocol handles emotes via special msgtype, Rust side will handle
     * the prefix */
    char *emote = g_strdup_printf("/me %s", args[0]);
    if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
      purple_conv_chat_send(PURPLE_CONV_CHAT(conv), emote);
    } else {
      purple_conv_im_send(PURPLE_CONV_IM(conv), emote);
    }
    g_free(emote);
    return PURPLE_CMD_RET_OK;
  }
  return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_redact_last(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id) {
    purple_matrix_rust_redact_event(purple_account_get_username(account),
                                    purple_conversation_get_name(conv), last_id,
                                    "User requested redaction");
    return PURPLE_CMD_RET_OK;
  }
  *error = g_strdup("Could not find the ID of the last message to redact.");
  return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_edit_last(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  if (args[0] && *args[0]) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id) {
      purple_matrix_rust_send_edit(purple_account_get_username(account),
                                   purple_conversation_get_name(conv), last_id,
                                   args[0]);
      return PURPLE_CMD_RET_OK;
    }
    *error = g_strdup("Could not find the ID of the last message to edit.");
  } else {
    *error = g_strdup("Usage: /edit <new text>");
  }
  return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_react(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  const char *event_id = NULL;
  const char *key = NULL;
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /react <emoji_or_key> [event_id]");
    return PURPLE_CMD_RET_FAILED;
  }
  key = args[0];
  if (args[1] && *args[1]) {
    event_id = args[1];
  } else {
    event_id = purple_conversation_get_data(conv, "last_event_id");
  }
  if (!event_id || !*event_id) {
    *error = g_strdup("No target event found. Pass an explicit event_id.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_send_reaction(purple_account_get_username(account),
                                   purple_conversation_get_name(conv), event_id,
                                   key);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_polls(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_list_polls(purple_account_get_username(account),
                                purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_location(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  char *end1 = NULL;
  char *end2 = NULL;
  double lat = 0.0;
  double lon = 0.0;
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0] || !args[1] || !*args[1]) {
    *error = g_strdup("Usage: /location <latitude> <longitude>");
    return PURPLE_CMD_RET_FAILED;
  }
  lat = g_ascii_strtod(args[0], &end1);
  lon = g_ascii_strtod(args[1], &end2);
  if (!end1 || *end1 != '\0' || !end2 || *end2 != '\0') {
    *error =
        g_strdup("Invalid coordinates. Example: /location 37.7749 -122.4194");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_send_location(purple_account_get_username(account),
                                   purple_conversation_get_name(conv), lat,
                                   lon);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_who_read(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_who_read(purple_account_get_username(account),
                              purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_report(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /report <event_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_report_content(purple_account_get_username(account),
                                    purple_conversation_get_name(conv), args[0],
                                    args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_bulk_redact(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  char *end = NULL;
  long count = 0;
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /bulk_redact <count> [target_user_id]");
    return PURPLE_CMD_RET_FAILED;
  }
  count = strtol(args[0], &end, 10);
  if (!end || *end != '\0' || count <= 0 || count > 1000) {
    *error = g_strdup("Count must be an integer between 1 and 1000.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_bulk_redact(purple_account_get_username(account),
                                 purple_conversation_get_name(conv), (int)count,
                                 args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_knock(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /knock <room_id_or_alias> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_knock(purple_account_get_username(account), args[0],
                           args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_room_tag(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /room_tag <tag>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_set_room_tag(purple_account_get_username(account),
                                  purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_upgrade_room(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /upgrade_room <new_version>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_upgrade_room(purple_account_get_username(account),
                                  purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_create(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /alias_create <localpart>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_create_alias(purple_account_get_username(account),
                                  purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_delete(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /alias_delete <#alias:server>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_delete_alias(purple_account_get_username(account),
                                  args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_search_users(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  const char *room_id = NULL;
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /search_users <term>");
    return PURPLE_CMD_RET_FAILED;
  }
  room_id =
      (conv && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT)
          ? purple_conversation_get_name(conv)
          : "";
  purple_matrix_rust_search_users(purple_account_get_username(account), args[0],
                                  room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_search_public(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /search_public <term>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_roomlist_show_with_account(account);
  purple_matrix_rust_search_public_rooms(purple_account_get_username(account),
                                         args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_search_members(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /search_members <term>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_search_room_members(purple_account_get_username(account),
                                         purple_conversation_get_name(conv),
                                         args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_versions(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_get_supported_versions(
      purple_account_get_username(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_enable_key_backup(PurpleConversation *conv,
                                          const gchar *cmd, gchar **args,
                                          gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_enable_key_backup(purple_account_get_username(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_my_profile(PurpleConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_get_my_profile(purple_account_get_username(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_server_info(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_get_server_info(purple_account_get_username(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_resync_recent(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  const char *room_id = NULL;
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (conv && purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
    room_id = purple_conversation_get_name(conv);
  } else if (args && args[0] && *args[0]) {
    room_id = args[0];
  }
  if (!room_id || !*room_id) {
    *error = g_strdup("Usage: /resync_recent [room_id] (room_id required "
                      "outside chat rooms)");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_resync_recent_history(purple_account_get_username(account),
                                           room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_sticker_search(PurpleConversation *conv,
                                       const gchar *cmd, gchar **args,
                                       gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /sticker_search <term>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_search_stickers(purple_account_get_username(account),
                                     args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_recover_keys(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_recover_keys <passphrase>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_recover_keys(purple_account_get_username(account),
                                  args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_export_keys(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0] || !args[1] || !*args[1]) {
    *error = g_strdup("Usage: /matrix_export_keys <path> <passphrase>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_export_keys(purple_account_get_username(account), args[0],
                                 args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_avatar(PurpleConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_avatar <path_to_image>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_set_avatar(purple_account_get_username(account), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_mark_read(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  matrix_ui_action_mark_read(purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_power_levels(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_get_power_levels(purple_account_get_username(account),
                                      purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ignore_user(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /ignore_user <@user:server>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_ignore_user(purple_account_get_username(account), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unignore_user(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  if (!args || !args[0] || !*args[0]) {
    *error = g_strdup("Usage: /unignore_user <@user:server>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_unignore_user(purple_account_get_username(account),
                                   args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_message_inspector(PurpleConversation *conv,
                                          const gchar *cmd, gchar **args,
                                          gchar **error, void *data) {
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  matrix_ui_action_message_inspector(purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_last_event_details(PurpleConversation *conv,
                                           const gchar *cmd, gchar **args,
                                           gchar **error, void *data) {
  if (matrix_cmd_require_chat(conv, error) != PURPLE_CMD_RET_OK)
    return PURPLE_CMD_RET_FAILED;
  matrix_ui_action_show_last_event_details(purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_kick(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /kick <user_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_kick_user(purple_account_get_username(account),
                               purple_conversation_get_name(conv), args[0],
                               args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ban(PurpleConversation *conv, const gchar *cmd,
                            gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /ban <user_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_ban_user(purple_account_get_username(account),
                              purple_conversation_get_name(conv), args[0],
                              args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unban(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /unban <user_id>");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_unban_user(purple_account_get_username(account),
                                purple_conversation_get_name(conv), args[0],
                                NULL);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_redact(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /redact <event_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_redact_event(purple_account_get_username(account),
                                  purple_conversation_get_name(conv), args[0],
                                  args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_topic(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /topic <new_topic>");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_set_room_topic(purple_account_get_username(account),
                                    purple_conversation_get_name(conv),
                                    args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_room_name(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /room_name <new_name>");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_set_room_name(purple_account_get_username(account),
                                   purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  PurpleConnection *gc = purple_account_get_connection(account);
  if (gc) {
    matrix_chat_leave(gc, get_chat_id(purple_conversation_get_name(conv)));
    return PURPLE_CMD_RET_OK;
  }
  return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_members(PurpleConversation *conv, const gchar *cmd,
                                gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_fetch_room_members(purple_account_get_username(account),
                                        purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_login(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0] || !args[1] || !*args[1] || !args[2] || !*args[2]) {
    *error = g_strdup("Usage: /matrix_login <homeserver> <user> <pass>");
    return PURPLE_CMD_RET_FAILED;
  }

  char *safe_user = g_strdup(args[1]);
  for (char *p = safe_user; *p; p++) {
    if (*p == ':' || *p == '/' || *p == '\\')
      *p = '_';
  }
  char *data_dir =
      g_build_filename(purple_user_dir(), "matrix_rust_data", safe_user, NULL);
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
    g_mkdir_with_parents(data_dir, 0700);
  }

  purple_matrix_rust_login(args[1], args[2], args[0], data_dir);
  purple_conversation_write(conv, "System", "Matrix login started.",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  g_free(data_dir);
  g_free(safe_user);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_sso(PurpleConversation *conv, const gchar *cmd,
                                   gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *username =
      (args[1] && *args[1]) ? args[1] : purple_account_get_username(account);
  const char *homeserver =
      (args[0] && *args[0])
          ? args[0]
          : purple_account_get_string(account, "server", "https://matrix.org");

  char *safe_user = g_strdup(username);
  for (char *p = safe_user; *p; p++) {
    if (*p == ':' || *p == '/' || *p == '\\')
      *p = '_';
  }
  char *data_dir =
      g_build_filename(purple_user_dir(), "matrix_rust_data", safe_user, NULL);
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
    g_mkdir_with_parents(data_dir, 0700);
  }

  /* Empty password intentionally triggers SSO path in Rust auth flow. */
  purple_matrix_rust_login(username, "", homeserver, data_dir);
  purple_conversation_write(conv, "System",
                            "Matrix SSO flow started. Check browser prompt.",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));

  g_free(data_dir);
  g_free(safe_user);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_join(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  return cmd_join(conv, cmd, args, error, data);
}

static PurpleCmdRet cmd_matrix_leave(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  return cmd_leave(conv, cmd, args, error, data);
}

static PurpleCmdRet cmd_matrix_create(PurpleConversation *conv,
                                      const gchar *cmd, gchar **args,
                                      gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_create <name>");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_create_room(purple_account_get_username(account), args[0],
                                 NULL, FALSE);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_clear_session(PurpleConversation *conv,
                                             const gchar *cmd, gchar **args,
                                             gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (!account)
    account = find_matrix_account();
  if (!account) {
    *error = g_strdup("No Matrix account found.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_destroy_session(purple_account_get_username(account));
  purple_conversation_write(conv, "System",
                            "Local Matrix session cache cleared.",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

void register_matrix_commands(PurplePlugin *plugin) {
  purple_cmd_register("matrix_help", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_help, "help", NULL);
  purple_cmd_register("sticker", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_sticker,
                      "sticker: Browse and send stickers", NULL);
  purple_cmd_register("poll", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_poll, "poll: Create a new poll",
                      NULL);
  purple_cmd_register("reply", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_reply,
                      "reply: Reply to the latest message", NULL);
  purple_cmd_register(
      "thread", "", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread,
      "thread: Start a new thread from the latest message", NULL);
  purple_cmd_register("dashboard", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_dashboard,
                      "dashboard: Open the Matrix room dashboard", NULL);
  purple_cmd_register("nick", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_nick,
                      "nick <name>: Change your display name", NULL);

  purple_cmd_register("join", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_join,
                      "join <room_id_or_alias>: Join a room", NULL);
  purple_cmd_register("invite", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_invite,
                      "invite <user_id>: Invite a user", NULL);
  purple_cmd_register("leave", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_leave,
                      "leave: Leave the current room", NULL);
  purple_cmd_register("members", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_members,
                      "members: List participants in this room", NULL);

  purple_cmd_register("shrug", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_shrug,
                      "shrug [message]: Add a shrug emoji", NULL);
  purple_cmd_register(
      "matrix_restore_backup", "s", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_restore_backup,
      "matrix_restore_backup <security_key>: Restore E2EE keys from backup",
      NULL);
  purple_cmd_register("matrix_verify", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_verify,
                      "matrix_verify: Start device verification", NULL);
  purple_cmd_register(
      "matrix_verify", "s", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_verify,
      "matrix_verify [target]: Start device verification", NULL);
  purple_cmd_register("matrix_sso", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_sso,
                      "matrix_sso [homeserver] [user]: Start SSO login", NULL);
  purple_cmd_register("matrix_sso", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_sso,
                      "matrix_sso [homeserver] [user]: Start SSO login", NULL);
  purple_cmd_register("matrix_sso", "ww", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_sso,
                      "matrix_sso [homeserver] [user]: Start SSO login", NULL);
  purple_cmd_register("matrix_devices", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_devices,
                      "matrix_devices: List your Matrix devices", NULL);
  purple_cmd_register("matrix_profile", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_my_profile,
                      "matrix_profile: Refresh and show your Matrix profile",
                      NULL);
  purple_cmd_register("matrix_server_info", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_server_info,
                      "matrix_server_info: Query server capabilities", NULL);
  purple_cmd_register(
      "matrix_login", "www", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_matrix_login,
      "matrix_login <homeserver> <user> <pass>: Login explicitly", NULL);
  purple_cmd_register("matrix_join", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_join,
                      "matrix_join <room_id_or_alias>: Join a room", NULL);
  purple_cmd_register("matrix_leave", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
                      cmd_matrix_leave, "matrix_leave: Leave current room",
                      NULL);
  purple_cmd_register("matrix_create", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_create,
                      "matrix_create <name>: Create a room", NULL);
  purple_cmd_register("matrix_clear_session", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_matrix_clear_session,
                      "matrix_clear_session: Clear local session cache", NULL);
  purple_cmd_register("matrix_debug_crypto", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_crypto_status,
                      "matrix_debug_crypto: Show detailed E2EE status", NULL);
  purple_cmd_register(
      "matrix_recover_keys", "s", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_recover_keys,
      "matrix_recover_keys <passphrase>: Recover keys from secret storage",
      NULL);
  purple_cmd_register(
      "matrix_export_keys", "ss", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_export_keys,
      "matrix_export_keys <path> <passphrase>: Export encrypted key backup",
      NULL);
  purple_cmd_register(
      "matrix_set_avatar", "s", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_set_avatar,
      "matrix_set_avatar <path>: Set account avatar from image file", NULL);
  purple_cmd_register(
      "me", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
      "prpl-matrix-rust", cmd_me, "me <action>: Send an emote", NULL);
  purple_cmd_register("redact_last", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_redact_last,
                      "redact_last: Delete your most recent message", NULL);
  purple_cmd_register("edit", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_edit_last,
                      "edit <text>: Edit your most recent message", NULL);
  purple_cmd_register("react", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_react,
                      "react <emoji_or_key> [event_id]: React to a message",
                      NULL);
  purple_cmd_register("react", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_react,
                      "react <emoji_or_key> [event_id]: React to a message",
                      NULL);
  purple_cmd_register("polls", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_polls,
                      "polls: List active polls and vote", NULL);
  purple_cmd_register("location", "ss", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_location,
                      "location <lat> <lon>: Send a location", NULL);
  purple_cmd_register("message_inspector", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
                      cmd_message_inspector,
                      "message_inspector: Pick recent event and action", NULL);
  purple_cmd_register(
      "last_event_details", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
      "prpl-matrix-rust", cmd_last_event_details,
      "last_event_details: Show details for latest event", NULL);
  purple_cmd_register("mark_read", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_mark_read,
                      "mark_read: Mark room read and send read receipt", NULL);
  purple_cmd_register("power_levels", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
                      cmd_power_levels, "power_levels: Show room power levels",
                      NULL);
  purple_cmd_register("who_read", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_who_read,
                      "who_read: Show readers for this room", NULL);
  purple_cmd_register("report", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_report,
                      "report <event_id> [reason]: Report content", NULL);
  purple_cmd_register("report", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_report,
                      "report <event_id> [reason]: Report content", NULL);
  purple_cmd_register("bulk_redact", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_bulk_redact,
                      "bulk_redact <count> [target_user]: Redact recent events",
                      NULL);
  purple_cmd_register("bulk_redact", "ss", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_bulk_redact,
                      "bulk_redact <count> [target_user]: Redact recent events",
                      NULL);
  purple_cmd_register(
      "knock", "s", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_knock,
      "knock <room_or_alias> [reason]: Request to join room", NULL);
  purple_cmd_register(
      "knock", "ss", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_knock,
      "knock <room_or_alias> [reason]: Request to join room", NULL);
  purple_cmd_register("room_tag", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_room_tag,
                      "room_tag <tag>: Apply room tag", NULL);
  purple_cmd_register("upgrade_room", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
                      cmd_upgrade_room,
                      "upgrade_room <version>: Upgrade room version", NULL);
  purple_cmd_register(
      "alias_create", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
      "prpl-matrix-rust", cmd_alias_create,
      "alias_create <localpart>: Create canonical room alias", NULL);
  purple_cmd_register("alias_delete", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_alias_delete,
                      "alias_delete <#alias:server>: Delete room alias", NULL);
  purple_cmd_register("search_users", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_search_users,
                      "search_users <term>: Search Matrix users", NULL);
  purple_cmd_register("search_public", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_search_public,
                      "search_public <term>: Search public rooms", NULL);
  purple_cmd_register(
      "search_members", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
      "prpl-matrix-rust", cmd_search_members,
      "search_members <term>: Search users in current room", NULL);
  purple_cmd_register("ignore_user", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_ignore_user,
                      "ignore_user <@user:server>: Ignore a user", NULL);
  purple_cmd_register("unignore_user", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_unignore_user,
                      "unignore_user <@user:server>: Unignore a user", NULL);
  purple_cmd_register(
      "resync_recent", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
      "prpl-matrix-rust", cmd_resync_recent,
      "resync_recent: Refresh latest timeline in this room", NULL);
  purple_cmd_register("resync_recent", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_resync_recent,
                      "resync_recent [room_id]: Refresh latest timeline", NULL);
  purple_cmd_register("sticker_search", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_sticker_search,
                      "sticker_search <term>: Search sticker catalog", NULL);
  purple_cmd_register("versions", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_versions,
                      "versions: Query supported Matrix server versions", NULL);
  purple_cmd_register("enable_key_backup", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_enable_key_backup,
                      "enable_key_backup: Enable secure key backup", NULL);

  purple_cmd_register("kick", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_kick,
                      "kick <user_id> [reason]: Remove a user from the room",
                      NULL);
  purple_cmd_register("ban", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_ban,
                      "ban <user_id> [reason]: Ban a user from the room", NULL);
  purple_cmd_register("unban", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_unban,
                      "unban <user_id>: Unban a user from the room", NULL);
  purple_cmd_register(
      "redact", "ss", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_redact,
      "redact <event_id> [reason]: Delete a specific message", NULL);
  purple_cmd_register("topic", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_topic,
                      "topic <new_topic>: Set room topic", NULL);
  purple_cmd_register("room_name", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_room_name,
                      "room_name <new_name>: Set room display name", NULL);
}

GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
  GList *m = NULL;
  PurplePluginAction *act;

  act =
      purple_plugin_action_new("Login (Password)...", action_login_password_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Login (SSO)...", action_login_sso_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Set Homeserver / Username...",
                                 action_set_account_defaults_cb);
  act->context = context;
  m = g_list_append(m, act);

  act =
      purple_plugin_action_new("Discover Center...", action_discover_center_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Invite User...", action_invite_user_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Verify This Device (Emoji)...",
                                 action_verify_device_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Setup E2EE (Account Password)...",
                                 action_bootstrap_crypto_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Setup E2EE (Security Key)...",
                                 action_restore_backup_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Re-sync All Room Keys...",
                                 action_resync_all_keys_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Recover Keys (Passphrase)...",
                                 action_recover_keys_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Export Keys...", action_export_keys_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Server Info", action_server_info_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("My Matrix Profile...", action_my_profile_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Enter Manual SSO Token...",
                                 action_sso_token_cb);
  act->context = context;
  m = g_list_append(m, act);

  act = purple_plugin_action_new("Clear Session Cache...",
                                 action_clear_session_cache_cb);
  act->context = context;
  m = g_list_append(m, act);

  return m;
}
