#include "matrix_commands.h"
#include "matrix_globals.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_types.h"
#include "matrix_utils.h"
#include "matrix_blist.h"
#include "matrix_account.h"
#include "matrix_chat.h"

#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <libpurple/cmds.h>
#include <string.h>
#include <stdlib.h>

/* Forward Declarations */
static void action_join_room_cb(PurplePluginAction *action);
static void action_invite_user_cb(PurplePluginAction *action);
static void action_verify_device_cb(PurplePluginAction *action);
static void action_bootstrap_crypto_cb(PurplePluginAction *action);
static void action_restore_backup_cb(PurplePluginAction *action);
static void action_resync_all_keys_cb(PurplePluginAction *action);
static void action_sso_token_cb(PurplePluginAction *action);
static void action_clear_session_cache_cb(PurplePluginAction *action);
static void action_login_password_cb(PurplePluginAction *action);
static void action_login_sso_cb(PurplePluginAction *action);
static void action_set_account_defaults_cb(PurplePluginAction *action);
static PurpleCmdRet cmd_verify(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_crypto_status(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_devices(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_members(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_login(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_sso(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_join(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_create(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_matrix_clear_session(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static void action_my_profile_cb(PurplePluginAction *action);
static void my_profile_avatar_cb(void *user_data, const char *filename);
static void menu_action_reply_dialog_cb(void *user_data, const char *text);
static void menu_action_thread_dialog_cb(void *user_data, const char *text);
static void menu_action_room_dashboard_conv_cb(PurpleConversation *conv, gpointer data);
static void join_room_dialog_cb(void *user_data, const char *room_id);
static void invite_user_dialog_cb(void *user_data, const char *user_id);
void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields);
static PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);

static PurpleAccount *matrix_action_account(PurplePluginAction *action) {
    if (action && action->context) {
        PurpleConnection *gc = (PurpleConnection *)action->context;
        if (gc) {
            PurpleAccount *acct = purple_connection_get_account(gc);
            if (acct && strcmp(purple_account_get_protocol_id(acct), "prpl-matrix-rust") == 0) {
                return acct;
            }
        }
    }
    return find_matrix_account();
}

static PurpleAccount *matrix_account_from_user_id(const char *user_id) {
    if (user_id && *user_id) {
        PurpleAccount *acct = find_matrix_account_by_id(user_id);
        if (acct) return acct;
    }
    return find_matrix_account();
}

static char *matrix_build_data_dir_for_user(const char *username) {
    char *safe_user = g_strdup(username ? username : "");
    for (char *p = safe_user; *p; p++) {
        if (*p == ':' || *p == '/' || *p == '\\') *p = '_';
    }
    char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data", safe_user, NULL);
    if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(data_dir, 0700);
    }
    g_free(safe_user);
    return data_dir;
}

/* Sticker Selection Wizard */
static void sticker_selected_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleConversation *conv = (PurpleConversation *)user_data;
  if (!g_list_find(purple_get_conversations(), conv)) return;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "sticker");
  GList *selected = purple_request_field_list_get_selected(field);
  if (selected) {
    const char *url = (const char *)purple_request_field_list_get_data(field, (const char *)selected->data);
    if (url) purple_matrix_rust_send_sticker(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), url);
  }
}
static void sticker_cb(const char *user_id, const char *shortcode, const char *body, const char *url, void *user_data) {
  PurpleRequestFields *fields = (PurpleRequestFields *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "sticker");
  char *label = g_strdup_printf("%s: %s", shortcode, body);
  purple_request_field_list_add(field, label, g_strdup(url));
  g_free(label);
}
static void sticker_done_cb(void *user_data) { }
static void sticker_pack_selected_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleConversation *conv = (PurpleConversation *)user_data;
  if (!g_list_find(purple_get_conversations(), conv)) return;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "pack");
  GList *selected = purple_request_field_list_get_selected(field);
  if (selected) {
    const char *pack_id = (const char *)purple_request_field_list_get_data(field, (const char *)selected->data);
    if (pack_id) {
       PurpleRequestFields *s_fields = purple_request_fields_new();
       PurpleRequestFieldGroup *group = purple_request_field_group_new("Pack Contents");
       purple_request_fields_add_group(s_fields, group);
       PurpleRequestField *s_field = purple_request_field_list_new("sticker", "Choose Sticker");
       purple_request_field_group_add_field(group, s_field);
       purple_matrix_rust_list_stickers_in_pack(purple_account_get_username(purple_conversation_get_account(conv)), pack_id, sticker_cb, sticker_done_cb, s_fields);
       purple_request_fields(my_plugin, "Sticker Selection Wizard", "Select a Sticker", NULL, s_fields, "_Send", G_CALLBACK(sticker_selected_cb), "_Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, conv);
    }
  }
}
static void sticker_pack_cb(const char *user_id, const char *id, const char *name, void *user_data) {
  PurpleRequestFields *fields = (PurpleRequestFields *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "pack");
  purple_request_field_list_add(field, name, g_strdup(id));
}
static void sticker_pack_done_cb(void *user_data) { }
PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("Available Packs");
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field = purple_request_field_list_new("pack", "Sticker Packs");
  purple_request_field_group_add_field(group, field);
  purple_matrix_rust_list_sticker_packs(purple_account_get_username(purple_conversation_get_account(conv)), sticker_pack_cb, sticker_pack_done_cb, fields);
  purple_request_fields(my_plugin, "Sticker Selection Wizard", "Browse Sticker Packs", NULL, fields, "Open Pack", G_CALLBACK(sticker_pack_selected_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, conv);
  return PURPLE_CMD_RET_OK;
}
void menu_action_sticker_conv_cb(PurpleConversation *conv, gpointer data) { if (conv) cmd_sticker_list(conv, "sticker", NULL, NULL, NULL); }

/* Poll Creation Wizard */
static void poll_create_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *q = purple_request_fields_get_string(fields, "q");
  const char *opts = purple_request_fields_get_string(fields, "opts");
  if (q && opts && account) purple_matrix_rust_poll_create(purple_account_get_username(account), room_id, q, opts);
  g_free(room_id);
}
void menu_action_poll_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv) return;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("Poll Details");
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(group, purple_request_field_string_new("q", "Question:", NULL, FALSE));
  purple_request_field_group_add_field(group, purple_request_field_string_new("opts", "Options:", "Yes|No", FALSE));
  purple_request_fields(my_plugin, "Poll Creation Wizard", "Create a New Poll", NULL, fields, "Create Poll", G_CALLBACK(poll_create_dialog_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, g_strdup(purple_conversation_get_name(conv)));
}

static void action_send_file_cb(void *user_data, const char *filename) {
    if (!filename || !*filename) return;
    char *room_id = (char *)user_data;
    PurpleAccount *account = find_matrix_account();
    if (account && room_id) {
        purple_matrix_rust_send_file(purple_account_get_username(account), room_id, filename);
    }
    g_free(room_id);
}

/* Dashboard */
static void restore_backup_dialog_cb(void *user_data, const char *key) {
    if (!key || !*key) return;
    PurpleAccount *account = find_matrix_account();
    if (account) {
        purple_matrix_rust_restore_backup(purple_account_get_username(account), key);
    }
}

static void action_restore_backup_cb(PurplePluginAction *action) {
    purple_request_input(my_plugin, "Matrix Security", "Restore Key Backup", 
        "Enter your Matrix Security Key (Recovery Key) to decrypt historical messages and threads.", 
        NULL, FALSE, FALSE, NULL, "Restore", G_CALLBACK(restore_backup_dialog_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL);
}

/* User Management Wizard */
static void user_mgmt_dialog_cb(void *user_data, PurpleRequestFields *fields) {
    char *room_id = (char *)user_data;
    PurpleAccount *account = find_matrix_account();
    const char *target = purple_request_fields_get_string(fields, "target");
    const char *reason = purple_request_fields_get_string(fields, "reason");
    PurpleRequestField *f_action = purple_request_fields_get_field(fields, "action");
    
    if (account && target && *target && f_action) {
        int choice = purple_request_field_choice_get_value(f_action);
        switch(choice) {
            case 0: purple_matrix_rust_kick_user(purple_account_get_username(account), room_id, target, reason); break;
            case 1: purple_matrix_rust_ban_user(purple_account_get_username(account), room_id, target, reason); break;
            case 2: purple_matrix_rust_unban_user(purple_account_get_username(account), room_id, target, reason); break;
        }
    }
    g_free(room_id);
}

static void bootstrap_pass_dialog_cb(void *user_data, const char *password) {
    PurpleAccount *account = (PurpleAccount *)user_data;
    if (account && password && *password) {
        purple_matrix_rust_bootstrap_cross_signing_with_password(purple_account_get_username(account), password);
    }
}

static void open_user_mgmt_dialog(PurpleAccount *account, const char *room_id,
                                  const char *prefill_target_user_id) {
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new("Action Details");
    purple_request_fields_add_group(fields, group);
    
    purple_request_field_group_add_field(
        group, purple_request_field_string_new("target", "Target User ID",
                                               prefill_target_user_id, FALSE));
    purple_request_field_group_add_field(group, purple_request_field_string_new("reason", "Reason (Optional)", NULL, TRUE));
    
    PurpleRequestField *f_action = purple_request_field_choice_new("action", "Action", 0);
    purple_request_field_choice_add(f_action, "Kick");
    purple_request_field_choice_add(f_action, "Ban");
    purple_request_field_choice_add(f_action, "Unban");
    purple_request_field_group_add_field(group, f_action);
    
    purple_request_fields(my_plugin, "User Management", "Matrix Moderation", 
        "Perform moderation actions on a user in this room.", 
        fields, "_Execute", G_CALLBACK(user_mgmt_dialog_cb), "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

static void dash_choice_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleRequestField *f = purple_request_fields_get_field(fields, "action");
  int choice = purple_request_field_choice_get_value(f);
  PurpleAccount *account = find_matrix_account();
  
  if (account && room_id) {
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
    
    switch(choice) {
      case 0: if (conv) menu_action_reply_conv_cb(conv, NULL); break;
      case 1: if (conv) menu_action_thread_start_cb(conv, NULL); break;
      case 2: if (conv) menu_action_sticker_conv_cb(conv, NULL); break;
      case 3: if (conv) menu_action_poll_conv_cb(conv, NULL); break;
      case 4: action_invite_user_cb(NULL); break;
      case 5: { PurpleBlistNode *node = (PurpleBlistNode *)purple_blist_find_chat(account, room_id); if (node) menu_action_room_settings_cb(node, NULL); break; }
      case 6: purple_matrix_rust_mark_unread(purple_account_get_username(account), room_id, TRUE); break;
      case 7: if (conv) menu_action_thread_conv_cb(conv, NULL); break;
      case 8: purple_matrix_rust_e2ee_status(purple_account_get_username(account), room_id); break;
      case 9: purple_matrix_rust_get_power_levels(purple_account_get_username(account), room_id); break;
      case 10: {
          purple_request_file(my_plugin, "Select File to Send", NULL, FALSE, G_CALLBACK(action_send_file_cb), NULL, account, NULL, conv, g_strdup(room_id));
          break;
      }
      case 11: action_restore_backup_cb(NULL); break;
      case 12: if (conv) cmd_verify(conv, "matrix_verify", NULL, NULL, NULL); break;
      case 13: action_bootstrap_crypto_cb(NULL); break;
      case 14: {
          purple_request_input(my_plugin, "E2EE Setup", "Enter Account Password", "This is required to establish your cryptographic identity.", NULL, FALSE, TRUE, NULL, "_Bootstrap", G_CALLBACK(bootstrap_pass_dialog_cb), "_Cancel", NULL, account, NULL, NULL, NULL);
          break;
      }
      case 15: if (conv) cmd_crypto_status(conv, "matrix_debug_crypto", NULL, NULL, NULL); break;
      case 16: open_user_mgmt_dialog(account, room_id, NULL); break;
      case 17: {
          purple_request_file(my_plugin, "Select Avatar Image", NULL, FALSE, G_CALLBACK(my_profile_avatar_cb), NULL, account, NULL, conv, NULL);
          break;
      }
      case 18: {
          purple_request_input(my_plugin, "Room Settings", "Change Room Topic", NULL, NULL, TRUE, FALSE, NULL, "_Save", G_CALLBACK(room_settings_dialog_cb), "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
          break;
      }
      case 19: {
          purple_request_input(my_plugin, "Room Settings", "Change Room Name", NULL, NULL, FALSE, FALSE, NULL, "_Save", G_CALLBACK(room_settings_dialog_cb), "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
          break;
      }
      case 20: purple_matrix_rust_resync_room_keys(purple_account_get_username(account), room_id); break;
      case 21: if (conv) cmd_members(conv, "members", NULL, NULL, NULL); break;
      case 22: if (conv) cmd_leave(conv, "leave", NULL, NULL, NULL); break;
    }
  }
  
  if (choice != 10 && choice != 16 && choice != 18 && choice != 19) g_free(room_id);
}

void open_room_dashboard(PurpleAccount *account, const char *room_id) {
  if (!room_id || !account) return;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("Available Tasks");
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field = purple_request_field_choice_new("action", "What would you like to do?", 0);
  
  const char *tasks[] = { 
      "Reply to Latest", "Start New Thread", "Send Sticker", "Create Poll", 
      "Invite User", "Room Settings", "Mark as Read", "View Threads", 
      "E2EE Status", "Power Levels", "Send File...", 
      "Setup E2EE (Security Key)...", "Verify This Device (Emoji)...", 
      "Setup E2EE (Manual Bootstrap)", "Setup E2EE (Password)...", 
      "Check Crypto Status", "Kick/Ban User...", "Set My Avatar...", 
      "Set Room Topic...", "Set Room Name...", "Re-sync Room Keys", 
      "Show Participants", "Leave Room" 
  };
  
  for (int i = 0; i < 23; i++) purple_request_field_choice_add(field, tasks[i]);
  purple_request_field_group_add_field(group, field);
  purple_request_fields(my_plugin, "Room Dashboard", "Matrix Room Tasks", NULL, fields, "_Execute", G_CALLBACK(dash_choice_cb), "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv) return;
  purple_request_input(my_plugin, "Reply Wizard", "Reply to Message", NULL, NULL, TRUE, FALSE, NULL, "_Send", G_CALLBACK(menu_action_reply_dialog_cb), "_Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, g_strdup(purple_conversation_get_name(conv)));
}
static void menu_action_reply_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data; PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv && text && *text) { const char *last_id = purple_conversation_get_data(conv, "last_event_id"); if (last_id) purple_matrix_rust_send_reply(purple_account_get_username(account), room_id, last_id, text); }
  g_free(room_id);
}

void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv) return;
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_list_threads(purple_account_get_username(account), purple_conversation_get_name(conv));
}

void menu_action_thread_start_cb(PurpleConversation *conv, gpointer data) {
  if (!conv) return;
  purple_request_input(my_plugin, "Thread Wizard", "Start Thread", "Start a new thread from the latest message in this room.", NULL, TRUE, FALSE, NULL, "_Start", G_CALLBACK(menu_action_thread_dialog_cb), "_Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, g_strdup(purple_conversation_get_name(conv)));
}
static void menu_action_thread_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data; PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv && text && *text) { const char *last_id = purple_conversation_get_data(conv, "last_event_id"); if (last_id) { char *vid = g_strdup_printf("%s|%s", room_id, last_id); ensure_thread_in_blist(account, vid, "Thread", room_id); purple_matrix_rust_send_message(purple_account_get_username(account), vid, text); g_free(vid); } }
  g_free(room_id);
}

void conversation_extended_menu_cb(PurpleConversation *conv, GList **list) {
  if (!conv || strcmp(purple_account_get_protocol_id(purple_conversation_get_account(conv)), "prpl-matrix-rust") != 0) return;
  *list = g_list_append(*list, purple_menu_action_new("Room Dashboard...", PURPLE_CALLBACK(menu_action_room_dashboard_conv_cb), conv, NULL));
}
static void menu_action_room_dashboard_conv_cb(PurpleConversation *conv, gpointer data) { if (conv) open_room_dashboard(purple_conversation_get_account(conv), purple_conversation_get_name(conv)); }
static void action_join_room_cb(PurplePluginAction *action) { purple_request_input(my_plugin, "Join Room", "Enter ID or Alias", "Example: #room:matrix.org or !roomid:matrix.org", NULL, FALSE, FALSE, NULL, "Join", G_CALLBACK(join_room_dialog_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL); }
static void join_room_dialog_cb(void *user_data, const char *room_id) { if (room_id && *room_id) purple_matrix_rust_join_room(purple_account_get_username(find_matrix_account()), room_id); }

static void action_invite_user_cb(PurplePluginAction *action) { purple_request_input(my_plugin, "Invite User", "Enter User ID", "Example: @user:matrix.org", NULL, FALSE, FALSE, NULL, "Invite", G_CALLBACK(invite_user_dialog_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL); }
static void invite_user_dialog_cb(void *user_data, const char *user_id) { if (user_id && *user_id) { PurpleAccount *account = find_matrix_account(); if (account) purple_matrix_rust_invite_user(purple_account_get_username(account), "", user_id); } }

static void action_verify_device_cb(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    if (account) {
        purple_matrix_rust_verify_user(purple_account_get_username(account), purple_account_get_username(account));
        purple_notify_info(my_plugin, "Verification Started", "Request sent to other devices", "Please check Element or another client to approve the verification request.");
    }
}

static void action_bootstrap_crypto_cb(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    if (account) {
        purple_matrix_rust_bootstrap_cross_signing(purple_account_get_username(account));
        purple_notify_info(my_plugin, "E2EE Setup", "Cross-signing bootstrap started", "The plugin is attempting to establish your cryptographic identity. You may be prompted for your account password or security key in another client.");
    }
}

static void action_resync_all_keys_cb(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    if (account) {
        purple_matrix_rust_resync_room_keys(purple_account_get_username(account), "");
        purple_notify_info(my_plugin, "Key Sync", "Global key sync started", "The plugin is requesting missing keys from your backup and other devices.");
    }
}

static void my_profile_dialog_cb(void *user_data, PurpleRequestFields *fields) {
    PurpleAccount *account = find_matrix_account();
    if (!account) return;
    const char *nick = purple_request_fields_get_string(fields, "nick");
    if (nick && *nick) {
        purple_matrix_rust_set_display_name(purple_account_get_username(account), nick);
    }
}

static void my_profile_avatar_cb(void *user_data, const char *filename) {
    if (!filename || !*filename) return;
    PurpleAccount *account = find_matrix_account();
    if (account) {
        purple_matrix_rust_set_room_avatar(purple_account_get_username(account), "", filename);
    }
}

static void action_my_profile_cb(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    if (!account) return;
    
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new("Matrix Profile Details");
    purple_request_fields_add_group(fields, group);
    
    purple_request_field_group_add_field(group, purple_request_field_string_new("nick", "Display Name", NULL, FALSE));
    
    /* Secondary dialog for avatar until multi-button requests are implemented */
    purple_request_fields(my_plugin, "My Matrix Profile", "Edit Your Matrix Profile", 
        "You can update your display name here. To update your avatar, you will be prompted for a file next.", 
        fields, "_Save & Next", G_CALLBACK(my_profile_dialog_cb), "_Cancel", NULL, account, NULL, NULL, NULL);
    
    /* Trigger file picker immediately if requested? No, better to have a button. 
       Libpurple 2.x doesn't allow buttons inside request fields easily.
       We'll add a separate "Set Avatar" dashboard task. */
}

static void action_sso_token_cb(PurplePluginAction *action) { 
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);
    purple_request_field_group_add_field(group, purple_request_field_string_new("sso_token", "Login Token", NULL, FALSE));
    purple_request_fields(my_plugin, "SSO Login", "Authentication Required", "If you have a manual SSO login token, paste it below.", fields, "Submit", G_CALLBACK(manual_sso_token_action_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, find_matrix_account());
}

static void action_clear_session_cache_cb(PurplePluginAction *action) {
    PurpleAccount *account = matrix_action_account(action);
    if (!account) return;
    purple_matrix_rust_destroy_session(purple_account_get_username(account));
    purple_notify_info(my_plugin, "Matrix Session Cache", "Session cache cleared",
                       "Your local Matrix session has been removed. Reconnect to log in again.");
}

static void action_login_password_dialog_cb(void *user_data, PurpleRequestFields *fields) {
    PurpleAccount *account = (PurpleAccount *)user_data;
    if (!account) account = find_matrix_account();
    if (!account) return;

    const char *homeserver = purple_request_fields_get_string(fields, "homeserver");
    const char *username = purple_request_fields_get_string(fields, "username");
    const char *password = purple_request_fields_get_string(fields, "password");

    if (!homeserver || !*homeserver || !username || !*username || !password || !*password) {
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
    if (!account) return;

    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new("Credentials");
    purple_request_fields_add_group(fields, group);
    purple_request_field_group_add_field(
        group,
        purple_request_field_string_new("homeserver", "Homeserver URL",
            purple_account_get_string(account, "server", "https://matrix.org"), FALSE));
    purple_request_field_group_add_field(
        group,
        purple_request_field_string_new("username", "Username",
            purple_account_get_username(account), FALSE));
    PurpleRequestField *pw = purple_request_field_string_new("password", "Password", NULL, FALSE);
    purple_request_field_string_set_masked(pw, TRUE);
    purple_request_field_group_add_field(group, pw);

    purple_request_fields(my_plugin, "Matrix Login", "Login With Password", NULL,
        fields, "_Login", G_CALLBACK(action_login_password_dialog_cb),
        "_Cancel", NULL, account, NULL, NULL, account);
}

static void action_login_sso_dialog_cb(void *user_data, PurpleRequestFields *fields) {
    PurpleAccount *account = (PurpleAccount *)user_data;
    if (!account) account = find_matrix_account();
    if (!account) return;

    const char *homeserver = purple_request_fields_get_string(fields, "homeserver");
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
    if (!account) return;

    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new("SSO Login");
    purple_request_fields_add_group(fields, group);
    purple_request_field_group_add_field(
        group,
        purple_request_field_string_new("homeserver", "Homeserver URL",
            purple_account_get_string(account, "server", "https://matrix.org"), FALSE));
    purple_request_field_group_add_field(
        group,
        purple_request_field_string_new("username", "Username",
            purple_account_get_username(account), FALSE));

    purple_request_fields(my_plugin, "Matrix SSO", "Start SSO Login", NULL,
        fields, "_Start", G_CALLBACK(action_login_sso_dialog_cb),
        "_Cancel", NULL, account, NULL, NULL, account);
}

static void action_set_account_defaults_dialog_cb(void *user_data, PurpleRequestFields *fields) {
    PurpleAccount *account = (PurpleAccount *)user_data;
    if (!account) account = find_matrix_account();
    if (!account) return;

    const char *homeserver = purple_request_fields_get_string(fields, "homeserver");
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
    if (!account) return;

    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new("Defaults");
    purple_request_fields_add_group(fields, group);
    purple_request_field_group_add_field(
        group,
        purple_request_field_string_new("homeserver", "Homeserver URL",
            purple_account_get_string(account, "server", "https://matrix.org"), FALSE));
    purple_request_field_group_add_field(
        group,
        purple_request_field_string_new("username", "Username",
            purple_account_get_username(account), FALSE));

    purple_request_fields(my_plugin, "Matrix Account Defaults", "Set Homeserver / Username", NULL,
        fields, "_Save", G_CALLBACK(action_set_account_defaults_dialog_cb),
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
    if (!account) return;
    PurplePluginAction action = {0};
    PurpleConnection *gc = purple_account_get_connection(account);
    action.context = gc;
    action_login_password_cb(&action);
}

void matrix_action_login_sso_for_user(const char *user_id) {
    PurpleAccount *account = matrix_account_from_user_id(user_id);
    if (!account) return;
    PurplePluginAction action = {0};
    PurpleConnection *gc = purple_account_get_connection(account);
    action.context = gc;
    action_login_sso_cb(&action);
}

void matrix_action_set_account_defaults_for_user(const char *user_id) {
    PurpleAccount *account = matrix_account_from_user_id(user_id);
    if (!account) return;
    PurplePluginAction action = {0};
    PurpleConnection *gc = purple_account_get_connection(account);
    action.context = gc;
    action_set_account_defaults_cb(&action);
}

void matrix_action_clear_session_cache_for_user(const char *user_id) {
    PurpleAccount *account = matrix_account_from_user_id(user_id);
    if (!account) return;
    PurplePluginAction action = {0};
    PurpleConnection *gc = purple_account_get_connection(account);
    action.context = gc;
    action_clear_session_cache_cb(&action);
}

void matrix_ui_action_show_members(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    purple_matrix_rust_fetch_room_members(purple_account_get_username(account), room_id);
}

void matrix_ui_action_moderate(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    open_user_mgmt_dialog(account, room_id, NULL);
}

void matrix_ui_action_moderate_user(const char *room_id, const char *target_user_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    open_user_mgmt_dialog(account, room_id, target_user_id);
}

static void matrix_ui_user_info_prompt_cb(void *user_data, const char *target_user_id) {
    PurpleAccount *account = find_matrix_account();
    (void)user_data;
    if (!account || !target_user_id || !*target_user_id) return;
    purple_matrix_rust_get_user_info(purple_account_get_username(account), target_user_id);
}

void matrix_ui_action_user_info(const char *room_id, const char *target_user_id) {
    PurpleAccount *account = find_matrix_account();
    (void)room_id;
    if (!account) return;
    if (target_user_id && *target_user_id) {
        purple_matrix_rust_get_user_info(purple_account_get_username(account), target_user_id);
        return;
    }
    purple_request_input(my_plugin, "User Info", "Lookup Matrix User",
        "Enter Matrix user ID (example: @user:matrix.org)", NULL, FALSE, FALSE, NULL,
        "_Lookup", G_CALLBACK(matrix_ui_user_info_prompt_cb), "_Cancel", NULL,
        account, NULL, NULL, NULL);
}

void matrix_ui_action_leave_room(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    PurpleConnection *gc = purple_account_get_connection(account);
    if (!gc) return;
    matrix_chat_leave(gc, get_chat_id(room_id));
}

void matrix_ui_action_verify_self(const char *room_id) {
    (void)room_id;
    PurpleAccount *account = find_matrix_account();
    if (!account) return;
    const char *uid = purple_account_get_username(account);
    purple_matrix_rust_verify_user(uid, uid);
}

void matrix_ui_action_crypto_status(const char *room_id) {
    (void)room_id;
    PurpleAccount *account = find_matrix_account();
    if (!account) return;
    purple_matrix_rust_debug_crypto_status(purple_account_get_username(account));
}

void matrix_ui_action_list_devices(const char *room_id) {
    (void)room_id;
    PurpleAccount *account = find_matrix_account();
    if (!account) return;
    purple_matrix_rust_list_own_devices(purple_account_get_username(account));
}

void matrix_ui_action_room_settings(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    PurpleBlistNode *node = (PurpleBlistNode *)purple_blist_find_chat(account, room_id);
    if (node) menu_action_room_settings_cb(node, NULL);
}

static void matrix_ui_invite_room_cb(void *user_data, const char *target_user_id) {
    char *room_id = (char *)user_data;
    PurpleAccount *account = find_matrix_account();
    if (account && room_id && target_user_id && *target_user_id) {
        purple_matrix_rust_invite_user(purple_account_get_username(account), room_id, target_user_id);
    }
    g_free(room_id);
}

void matrix_ui_action_invite_user(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    purple_request_input(my_plugin, "Invite User", "Enter Matrix User ID",
        "Example: @user:matrix.org", NULL, FALSE, FALSE, NULL,
        "_Invite", G_CALLBACK(matrix_ui_invite_room_cb), "_Cancel", NULL,
        account, NULL, NULL, g_strdup(room_id));
}

static void matrix_ui_send_file_cb(void *user_data, const char *filename) {
    char *room_id = (char *)user_data;
    PurpleAccount *account = find_matrix_account();
    if (account && room_id && filename && *filename) {
        purple_matrix_rust_send_file(purple_account_get_username(account), room_id, filename);
    }
    g_free(room_id);
}

void matrix_ui_action_send_file(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    PurpleConversation *conv = NULL;
    if (!account || !room_id || !*room_id) return;
    conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
    purple_request_file(my_plugin, "Select File to Send", NULL, FALSE,
        G_CALLBACK(matrix_ui_send_file_cb), NULL, account, NULL, conv, g_strdup(room_id));
}

void matrix_ui_action_mark_unread(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    purple_matrix_rust_mark_unread(purple_account_get_username(account), room_id, TRUE);
}

void matrix_ui_action_set_room_mute(const char *room_id, bool muted) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    purple_matrix_rust_set_room_mute_state(purple_account_get_username(account), room_id, muted);
}

static void matrix_ui_search_room_cb(void *user_data, const char *term) {
    char *room_id = (char *)user_data;
    PurpleAccount *account = find_matrix_account();
    if (account && room_id && term && *term) {
        purple_matrix_rust_search_room_messages(purple_account_get_username(account), room_id, term);
    }
    g_free(room_id);
}

void matrix_ui_action_search_room(const char *room_id) {
    PurpleAccount *account = find_matrix_account();
    if (!account || !room_id || !*room_id) return;
    purple_request_input(my_plugin, "Search Messages", "Search in this Room",
        "Enter text to search recent messages.", NULL, FALSE, FALSE, NULL,
        "_Search", G_CALLBACK(matrix_ui_search_room_cb), "_Cancel", NULL,
        account, NULL, NULL, g_strdup(room_id));
}

void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields) { 
    char *room_id = (char *)user_data; 
    PurpleAccount *account = find_matrix_account(); 
    const char *name = purple_request_fields_get_string(fields, "name"); 
    const char *topic = purple_request_fields_get_string(fields, "topic"); 
    
    if (account) { 
        if (name) purple_matrix_rust_set_room_name(purple_account_get_username(account), room_id, name); 
        if (topic) purple_matrix_rust_set_room_topic(purple_account_get_username(account), room_id, topic); 
        
        /* Advanced Settings */
        PurpleRequestField *f_join = purple_request_fields_get_field(fields, "join_rule");
        if (f_join) {
            int choice = purple_request_field_choice_get_value(f_join);
            const char *rules[] = { "public", "invite", "knock" };
            if (choice >= 0 && choice < 3) purple_matrix_rust_set_room_join_rule(purple_account_get_username(account), room_id, rules[choice]);
        }
        
        PurpleRequestField *f_vis = purple_request_fields_get_field(fields, "visibility");
        if (f_vis) {
            int choice = purple_request_field_choice_get_value(f_vis);
            const char *vis[] = { "world_readable", "shared", "invited", "joined" };
            if (choice >= 0 && choice < 4) purple_matrix_rust_set_room_history_visibility(purple_account_get_username(account), room_id, vis[choice]);
        }
        
        gboolean guest = purple_request_fields_get_bool(fields, "guest_access");
        purple_matrix_rust_set_room_guest_access(purple_account_get_username(account), room_id, guest);
    } 
    g_free(room_id); 
}

void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data) {
    if (!PURPLE_BLIST_NODE_IS_CHAT(node)) return;
    PurpleChat *chat = (PurpleChat *)node;
    GHashTable *components = purple_chat_get_components(chat);
    const char *room_id = g_hash_table_lookup(components, "room_id");
    
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new("Room Settings");
    purple_request_fields_add_group(fields, group);
    
    purple_request_field_group_add_field(group, purple_request_field_string_new("name", "Room Name", chat->alias, FALSE));
    
    const char *topic = g_hash_table_lookup(components, "topic");
    purple_request_field_group_add_field(group, purple_request_field_string_new("topic", "Topic", topic, TRUE));
    
    /* Security & Access Group */
    PurpleRequestFieldGroup *sec_group = purple_request_field_group_new("Security & Access");
    purple_request_fields_add_group(fields, sec_group);
    
    PurpleRequestField *f_join = purple_request_field_choice_new("join_rule", "Join Rule", 1); /* Default to Invite */
    purple_request_field_choice_add(f_join, "Public");
    purple_request_field_choice_add(f_join, "Invite Only");
    purple_request_field_choice_add(f_join, "Knock Only");
    purple_request_field_group_add_field(sec_group, f_join);
    
    PurpleRequestField *f_vis = purple_request_field_choice_new("visibility", "History Visibility", 1); /* Default to Shared */
    purple_request_field_choice_add(f_vis, "World Readable");
    purple_request_field_choice_add(f_vis, "Members Only (Shared)");
    purple_request_field_choice_add(f_vis, "Since Invited");
    purple_request_field_choice_add(f_vis, "Since Joined");
    purple_request_field_group_add_field(sec_group, f_vis);
    
    purple_request_field_group_add_field(sec_group, purple_request_field_bool_new("guest_access", "Allow Guest Access", FALSE));
    
    const char *enc = g_hash_table_lookup(components, "encrypted");
    char *enc_status = g_strdup_printf("Encryption: %s", (enc && strcmp(enc, "1") == 0) ? "Enabled (🔒)" : "Disabled (🔓)");
    purple_request_field_group_add_field(sec_group, purple_request_field_label_new("enc_label", enc_status));
    g_free(enc_status);

    purple_request_fields(my_plugin, "Room Settings", "Edit Matrix Room Details", NULL, fields, "_Save", G_CALLBACK(room_settings_dialog_cb), "_Cancel", NULL, purple_chat_get_account(chat), NULL, NULL, g_strdup(room_id));
}

/* Commands & Registration */
static PurpleCmdRet cmd_help(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *msg = 
    "<b>Matrix Protocol Commands:</b><br/>"
    "<b>General:</b><br/>"
    "  /matrix_login &lt;homeserver&gt; &lt;user&gt; &lt;pass&gt; - Login with explicit credentials<br/>"
    "  /matrix_sso [homeserver] [user] - Start SSO login (uses account defaults if omitted)<br/>"
    "  /matrix_clear_session - Clear local session cache for this account<br/>"
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
    "<b>Security:</b><br/>"
    "  /matrix_verify [device_id_or_user_id] - Start interactive device verification<br/>"
    "  /matrix_devices - List your device IDs for verification targeting<br/>"
    "  /matrix_restore_backup &lt;key&gt; - Restore E2EE keys using your Security Key<br/>"
    "  /matrix_debug_crypto - Show detailed encryption status for this session";
  
  purple_conversation_write(conv, "System", msg, PURPLE_MESSAGE_SYSTEM, time(NULL)); 
  return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_sticker(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  return cmd_sticker_list(conv, cmd, args, error, data);
}
static PurpleCmdRet cmd_poll(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  menu_action_poll_conv_cb(conv, NULL); return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_reply(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  menu_action_reply_conv_cb(conv, NULL); return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_thread(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  menu_action_thread_conv_cb(conv, NULL); return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_dashboard(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  open_room_dashboard(purple_conversation_get_account(conv), purple_conversation_get_name(conv)); return PURPLE_CMD_RET_OK;
}
static PurpleCmdRet cmd_nick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (args[0] && *args[0]) {
        PurpleAccount *account = purple_conversation_get_account(conv);
        purple_matrix_rust_set_display_name(purple_account_get_username(account), args[0]);
        return PURPLE_CMD_RET_OK;
    }
    *error = g_strdup("Usage: /nick &lt;new_display_name&gt;");
    return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_join(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) {
        *error = g_strdup("Usage: /join &lt;room_id_or_alias&gt;");
        return PURPLE_CMD_RET_FAILED;
    }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_join_room(purple_account_get_username(account), args[0]);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_invite(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) {
        *error = g_strdup("Usage: /invite &lt;user_id&gt;");
        return PURPLE_CMD_RET_FAILED;
    }
    PurpleAccount *account = purple_conversation_get_account(conv);
    // Ensure we are in a chat to know WHERE to invite (unless inviter is global context?)
        // If we act on a conversation, usually invite is to THAT room.
        // matrix_chat_invite uses conversation name.
        if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
             purple_matrix_rust_invite_user(purple_account_get_username(account), purple_conversation_get_name(conv), args[0]);
             return PURPLE_CMD_RET_OK;
        } else {
             // For IM, maybe invite to a new room? Or invite to the DM?
             // Usually /invite is for MUCs.
             // Let's assume it's for the current room if chat.
             *error = g_strdup("Invite command is currently only supported in Chat rooms.");
             return PURPLE_CMD_RET_FAILED;
        }
}

static PurpleCmdRet cmd_shrug(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
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

static PurpleCmdRet cmd_restore_backup(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (args[0] && *args[0]) {
        PurpleAccount *account = purple_conversation_get_account(conv);
        purple_matrix_rust_restore_backup(purple_account_get_username(account), args[0]);
        return PURPLE_CMD_RET_OK;
    }
    return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_verify(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    const char *target = NULL;
    if (args && args[0] && *args[0]) {
        target = args[0];
    } else {
        /* Verify own device set by default when no explicit device/user is passed. */
        target = purple_account_get_username(account);
    }
    purple_matrix_rust_verify_user(purple_account_get_username(account), target);
    purple_conversation_write(conv, "System", "Verification request sent. Approve from your other Matrix client.", PURPLE_MESSAGE_SYSTEM, time(NULL));
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_crypto_status(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_debug_crypto_status(purple_account_get_username(account));
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_devices(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_list_own_devices(purple_account_get_username(account));
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_me(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (args[0] && *args[0]) {
        /* Matrix protocol handles emotes via special msgtype, Rust side will handle the prefix */
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

static PurpleCmdRet cmd_redact_last(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id) {
        purple_matrix_rust_redact_event(purple_account_get_username(account), purple_conversation_get_name(conv), last_id, "User requested redaction");
        return PURPLE_CMD_RET_OK;
    }
    *error = g_strdup("Could not find the ID of the last message to redact.");
    return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_edit_last(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (args[0] && *args[0]) {
        PurpleAccount *account = purple_conversation_get_account(conv);
        const char *last_id = purple_conversation_get_data(conv, "last_event_id");
        if (last_id) {
            purple_matrix_rust_send_edit(purple_account_get_username(account), purple_conversation_get_name(conv), last_id, args[0]);
            return PURPLE_CMD_RET_OK;
        }
        *error = g_strdup("Could not find the ID of the last message to edit.");
    } else {
        *error = g_strdup("Usage: /edit <new text>");
    }
    return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /kick <user_id> [reason]"); return PURPLE_CMD_RET_FAILED; }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_kick_user(purple_account_get_username(account), purple_conversation_get_name(conv), args[0], args[1]);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /ban <user_id> [reason]"); return PURPLE_CMD_RET_FAILED; }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_ban_user(purple_account_get_username(account), purple_conversation_get_name(conv), args[0], args[1]);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /unban <user_id>"); return PURPLE_CMD_RET_FAILED; }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_unban_user(purple_account_get_username(account), purple_conversation_get_name(conv), args[0], NULL);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_redact(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /redact <event_id> [reason]"); return PURPLE_CMD_RET_FAILED; }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_redact_event(purple_account_get_username(account), purple_conversation_get_name(conv), args[0], args[1]);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_topic(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /topic <new_topic>"); return PURPLE_CMD_RET_FAILED; }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_set_room_topic(purple_account_get_username(account), purple_conversation_get_name(conv), args[0]);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_room_name(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /room_name <new_name>"); return PURPLE_CMD_RET_FAILED; }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_set_room_name(purple_account_get_username(account), purple_conversation_get_name(conv), args[0]);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
        matrix_chat_leave(gc, get_chat_id(purple_conversation_get_name(conv)));
        return PURPLE_CMD_RET_OK;
    }
    return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_members(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_fetch_room_members(purple_account_get_username(account), purple_conversation_get_name(conv));
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_login(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0] || !args[1] || !*args[1] || !args[2] || !*args[2]) {
        *error = g_strdup("Usage: /matrix_login <homeserver> <user> <pass>");
        return PURPLE_CMD_RET_FAILED;
    }

    char *safe_user = g_strdup(args[1]);
    for (char *p = safe_user; *p; p++) {
        if (*p == ':' || *p == '/' || *p == '\\') *p = '_';
    }
    char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data", safe_user, NULL);
    if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(data_dir, 0700);
    }

    purple_matrix_rust_login(args[1], args[2], args[0], data_dir);
    purple_conversation_write(conv, "System", "Matrix login started.", PURPLE_MESSAGE_SYSTEM, time(NULL));
    g_free(data_dir);
    g_free(safe_user);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_sso(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    if (!account) account = find_matrix_account();
    if (!account) {
        *error = g_strdup("No Matrix account found.");
        return PURPLE_CMD_RET_FAILED;
    }

    const char *username = (args[1] && *args[1]) ? args[1] : purple_account_get_username(account);
    const char *homeserver = (args[0] && *args[0]) ? args[0] : purple_account_get_string(account, "server", "https://matrix.org");

    char *safe_user = g_strdup(username);
    for (char *p = safe_user; *p; p++) {
        if (*p == ':' || *p == '/' || *p == '\\') *p = '_';
    }
    char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data", safe_user, NULL);
    if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
        g_mkdir_with_parents(data_dir, 0700);
    }

    /* Empty password intentionally triggers SSO path in Rust auth flow. */
    purple_matrix_rust_login(username, "", homeserver, data_dir);
    purple_conversation_write(conv, "System", "Matrix SSO flow started. Check browser prompt.", PURPLE_MESSAGE_SYSTEM, time(NULL));

    g_free(data_dir);
    g_free(safe_user);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_join(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    return cmd_join(conv, cmd, args, error, data);
}

static PurpleCmdRet cmd_matrix_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    return cmd_leave(conv, cmd, args, error, data);
}

static PurpleCmdRet cmd_matrix_create(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (!args[0] || !*args[0]) {
        *error = g_strdup("Usage: /matrix_create <name>");
        return PURPLE_CMD_RET_FAILED;
    }
    PurpleAccount *account = purple_conversation_get_account(conv);
    purple_matrix_rust_create_room(purple_account_get_username(account), args[0], NULL, FALSE);
    return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_matrix_clear_session(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    PurpleAccount *account = purple_conversation_get_account(conv);
    if (!account) account = find_matrix_account();
    if (!account) {
        *error = g_strdup("No Matrix account found.");
        return PURPLE_CMD_RET_FAILED;
    }
    purple_matrix_rust_destroy_session(purple_account_get_username(account));
    purple_conversation_write(conv, "System", "Local Matrix session cache cleared.", PURPLE_MESSAGE_SYSTEM, time(NULL));
    return PURPLE_CMD_RET_OK;
}

void register_matrix_commands(PurplePlugin *plugin) {
  purple_cmd_register("matrix_help", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_help, "help", NULL);
  purple_cmd_register("sticker", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker, "sticker: Browse and send stickers", NULL);
  purple_cmd_register("poll", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll, "poll: Create a new poll", NULL);
  purple_cmd_register("reply", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reply, "reply: Reply to the latest message", NULL);
  purple_cmd_register("thread", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread, "thread: Start a new thread from the latest message", NULL);
  purple_cmd_register("dashboard", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_dashboard, "dashboard: Open the Matrix room dashboard", NULL);
  purple_cmd_register("nick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_nick, "nick <name>: Change your display name", NULL);
  
  purple_cmd_register("join", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_join, "join <room_id_or_alias>: Join a room", NULL);
  purple_cmd_register("invite", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_invite, "invite <user_id>: Invite a user", NULL);
  purple_cmd_register("leave", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_leave, "leave: Leave the current room", NULL);
  purple_cmd_register("members", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_members, "members: List participants in this room", NULL);
  
  purple_cmd_register("shrug", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_shrug, "shrug [message]: Add a shrug emoji", NULL);
  purple_cmd_register("matrix_restore_backup", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_restore_backup, "matrix_restore_backup <security_key>: Restore E2EE keys from backup", NULL);
  purple_cmd_register("matrix_verify", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_verify, "matrix_verify: Start device verification", NULL);
  purple_cmd_register("matrix_verify", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_verify, "matrix_verify [target]: Start device verification", NULL);
  purple_cmd_register("matrix_sso", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_sso, "matrix_sso [homeserver] [user]: Start SSO login", NULL);
  purple_cmd_register("matrix_sso", "w", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_sso, "matrix_sso [homeserver] [user]: Start SSO login", NULL);
  purple_cmd_register("matrix_sso", "ww", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_sso, "matrix_sso [homeserver] [user]: Start SSO login", NULL);
  purple_cmd_register("matrix_devices", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_devices, "matrix_devices: List your Matrix devices", NULL);
  purple_cmd_register("matrix_login", "www", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_login, "matrix_login <homeserver> <user> <pass>: Login explicitly", NULL);
  purple_cmd_register("matrix_join", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_join, "matrix_join <room_id_or_alias>: Join a room", NULL);
  purple_cmd_register("matrix_leave", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_leave, "matrix_leave: Leave current room", NULL);
  purple_cmd_register("matrix_create", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_create, "matrix_create <name>: Create a room", NULL);
  purple_cmd_register("matrix_clear_session", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_matrix_clear_session, "matrix_clear_session: Clear local session cache", NULL);
  purple_cmd_register("matrix_debug_crypto", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_crypto_status, "matrix_debug_crypto: Show detailed E2EE status", NULL);
  purple_cmd_register("me", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_me, "me <action>: Send an emote", NULL);
  purple_cmd_register("redact_last", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_redact_last, "redact_last: Delete your most recent message", NULL);
  purple_cmd_register("edit", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_edit_last, "edit <text>: Edit your most recent message", NULL);
  
  purple_cmd_register("kick", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_kick, "kick <user_id> [reason]: Remove a user from the room", NULL);
  purple_cmd_register("ban", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_ban, "ban <user_id> [reason]: Ban a user from the room", NULL);
  purple_cmd_register("unban", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_unban, "unban <user_id>: Unban a user from the room", NULL);
  purple_cmd_register("redact", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_redact, "redact <event_id> [reason]: Delete a specific message", NULL);
  purple_cmd_register("topic", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_topic, "topic <new_topic>: Set room topic", NULL);
  purple_cmd_register("room_name", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_room_name, "room_name <new_name>: Set room display name", NULL);
}

GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
    GList *m = NULL;
    PurplePluginAction *act;

    act = purple_plugin_action_new("Login (Password)...", action_login_password_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Login (SSO)...", action_login_sso_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Set Homeserver / Username...", action_set_account_defaults_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Join Room...", action_join_room_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Invite User...", action_invite_user_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Verify This Device (Emoji)...", action_verify_device_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Setup E2EE (Account Password)...", action_bootstrap_crypto_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Setup E2EE (Security Key)...", action_restore_backup_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Re-sync All Room Keys...", action_resync_all_keys_cb);
    act->context = context;
    m = g_list_append(m, act);
    
    act = purple_plugin_action_new("My Matrix Profile...", action_my_profile_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Enter Manual SSO Token...", action_sso_token_cb);
    act->context = context;
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Clear Session Cache...", action_clear_session_cache_cb);
    act->context = context;
    m = g_list_append(m, act);

    return m;
}
