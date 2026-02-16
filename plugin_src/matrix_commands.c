#include "matrix_commands.h"
#include "matrix_globals.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_types.h"
#include "matrix_utils.h"
#include "matrix_blist.h"
#include "matrix_account.h"

#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <libpurple/cmds.h>
#include <string.h>
#include <stdlib.h>

/* Forward Declarations */
static void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data);
static void action_join_room_cb(PurplePluginAction *action);
static void menu_action_reply_dialog_cb(void *user_data, const char *text);
static void menu_action_thread_dialog_cb(void *user_data, const char *text);
static PurpleCmdRet cmd_reply(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_thread(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
static void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data);
static void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data);
static PurpleCmdRet cmd_e2ee_status(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);

/* Commands */
static void sticker_selected_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleConversation *conv = (PurpleConversation *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "sticker");
  GList *selected = purple_request_field_list_get_selected(field);
  if (selected) {
    const char *url = (const char *)purple_request_field_list_get_data(field, (const char *)selected->data);
    if (url) purple_matrix_rust_send_sticker(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), url);
  }
}

static void sticker_cb(const char *shortcode, const char *body, const char *url, void *user_data) {
  PurpleRequestFields *fields = (PurpleRequestFields *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "sticker");
  char *label = g_strdup_printf("%s: %s", shortcode, body);
  purple_request_field_list_add(field, label, g_strdup(url));
  g_free(label);
}

static void sticker_done_cb(void *user_data) { }

static void sticker_pack_selected_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleConversation *conv = (PurpleConversation *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "pack");
  GList *selected = purple_request_field_list_get_selected(field);
  if (selected) {
    const char *pack_id = (const char *)purple_request_field_list_get_data(field, (const char *)selected->data);
    if (pack_id) {
       PurpleRequestFields *s_fields = purple_request_fields_new();
       PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
       purple_request_fields_add_group(s_fields, group);
       PurpleRequestField *s_field = purple_request_field_list_new("sticker", "Choose Sticker");
       purple_request_field_group_add_field(group, s_field);
       
       purple_matrix_rust_list_stickers_in_pack(purple_account_get_username(purple_conversation_get_account(conv)), pack_id, sticker_cb, sticker_done_cb, s_fields);
       purple_request_fields(conv, "Send Sticker", "Select a sticker to send", NULL, s_fields, "Send", G_CALLBACK(sticker_selected_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, NULL, conv);
    }
  }
}

static void sticker_pack_cb(const char *id, const char *name, void *user_data) {
  PurpleRequestFields *fields = (PurpleRequestFields *)user_data;
  PurpleRequestField *field = purple_request_fields_get_field(fields, "pack");
  purple_request_field_list_add(field, name, g_strdup(id));
}

static void sticker_pack_done_cb(void *user_data) { }

static PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field = purple_request_field_list_new("pack", "Choose Sticker Pack");
  purple_request_field_group_add_field(group, field);
  purple_matrix_rust_list_sticker_packs(purple_account_get_username(purple_conversation_get_account(conv)), sticker_pack_cb, sticker_pack_done_cb, fields);
  purple_request_fields(conv, "Stickers", "Available Sticker Packs", "Select a pack to browse stickers.", fields, "Open Pack", G_CALLBACK(sticker_pack_selected_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, NULL, conv);
  return PURPLE_CMD_RET_OK;
}

static void menu_action_sticker_conv_cb(PurpleConversation *conv, gpointer data) {
  cmd_sticker_list(conv, "matrix_sticker_list", NULL, NULL, NULL);
}

static void poll_create_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *q = purple_request_fields_get_string(fields, "q");
  const char *opts = purple_request_fields_get_string(fields, "opts");
  if (q && opts) purple_matrix_rust_poll_create(purple_account_get_username(account), room_id, q, opts);
  g_free(room_id);
}

static void menu_action_poll_conv_cb(PurpleConversation *conv, gpointer data) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(group, purple_request_field_string_new("q", "Question", NULL, FALSE));
  purple_request_field_group_add_field(group, purple_request_field_string_new("opts", "Options (split by |)", "Yes|No", FALSE));
  purple_request_fields(conv, "Create Poll", "Create a Matrix Poll", NULL, fields, "Create", G_CALLBACK(poll_create_dialog_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, NULL, g_strdup(purple_conversation_get_name(conv)));
}

/* Dedicated Dashboard Callbacks */
static void dash_reply_cb(char *room_id) {
  PurpleAccount *account = find_matrix_account();
  purple_request_input(account, "Reply", "Reply to last message", NULL, NULL, TRUE, FALSE, NULL, "Send", G_CALLBACK(menu_action_reply_dialog_cb), "Cancel", NULL, account, NULL, NULL, room_id);
}
static void dash_thread_cb(char *room_id) {
  PurpleAccount *account = find_matrix_account();
  purple_request_input(account, "Start Thread", "Start thread from last message", NULL, NULL, TRUE, FALSE, NULL, "Send", G_CALLBACK(menu_action_thread_dialog_cb), "Cancel", NULL, account, NULL, NULL, room_id);
}
static void dash_sticker_cb(char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) menu_action_sticker_conv_cb(conv, NULL);
  g_free(room_id);
}
static void dash_poll_cb(char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) menu_action_poll_conv_cb(conv, NULL);
  g_free(room_id);
}
static void dash_invite_cb(char *room_id) { action_join_room_cb(NULL); g_free(room_id); }
static void dash_settings_cb(char *room_id) {
  PurpleAccount *account = find_matrix_account();
  PurpleBlistNode *node = (PurpleBlistNode *)purple_blist_find_chat(account, room_id);
  if (node) menu_action_room_settings_cb(node, NULL);
  g_free(room_id);
}
static void dash_power_cb(char *room_id) { purple_matrix_rust_get_power_levels(purple_account_get_username(find_matrix_account()), room_id); g_free(room_id); }
static void dash_list_threads_cb(char *room_id) { purple_matrix_rust_list_threads(purple_account_get_username(find_matrix_account()), room_id); g_free(room_id); }
static void dash_e2ee_cb(char *room_id) { purple_matrix_rust_e2ee_status(purple_account_get_username(find_matrix_account()), room_id); g_free(room_id); }
static void dash_unread_cb(char *room_id) { purple_matrix_rust_mark_unread(purple_account_get_username(find_matrix_account()), room_id, TRUE); g_free(room_id); }

static void open_room_dashboard(PurpleAccount *account, const char *room_id) {
  if (!room_id) return;
  purple_request_action(account, "Room Dashboard", "Matrix Room Actions", "Manage room settings and interactions.", 0, account, NULL, NULL, g_strdup(room_id), 10,
    "Reply to Last", dash_reply_cb,
    "Start Thread", dash_thread_cb,
    "Send Sticker", dash_sticker_cb,
    "Create Poll", dash_poll_cb,
    "Invite User", dash_invite_cb,
    "Room Settings", dash_settings_cb,
    "Power Levels", dash_power_cb,
    "List Threads", dash_list_threads_cb,
    "E2EE Status", dash_e2ee_cb,
    "Mark Unread", dash_unread_cb);
}

static void menu_action_room_dashboard_conv_cb(PurpleConversation *conv, gpointer data) {
  open_room_dashboard(purple_conversation_get_account(conv), purple_conversation_get_name(conv));
}

static void poll_vote_selected_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleRequestField *f_poll = purple_request_fields_get_field(fields, "poll");
  PurpleRequestField *f_opt = purple_request_fields_get_field(fields, "opt");
  
  GList *sel_poll = purple_request_field_list_get_selected(f_poll);
  GList *sel_opt = purple_request_field_list_get_selected(f_opt);
  
  if (sel_poll && sel_opt) {
    const char *poll_id = (const char *)purple_request_field_list_get_data(f_poll, (const char *)sel_poll->data);
    int opt_idx = GPOINTER_TO_INT(purple_request_field_list_get_data(f_opt, (const char *)sel_opt->data));
    PurpleAccount *account = find_matrix_account();
    if (account) purple_matrix_rust_poll_vote(purple_account_get_username(account), room_id, poll_id, (guint64)opt_idx);
  }
  g_free(room_id);
}

void active_poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str) {
  /* This is called multiple times by Rust. We need to collect them. 
     Since we can't easily hold state between calls for a single dialog, 
     we'll use a simpler approach: the user sees the polls in the chat log 
     and uses the ID. 
     
     Actually, let's stick to the input dialog but explain it better. */
}

static void poll_vote_selected_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  /* Expecting "poll_id index" */
  if (text && *text) {
    char *dup = g_strdup(text);
    char *idx_str = strchr(dup, ' ');
    if (idx_str) {
      *idx_str = '\0';
      idx_str++;
      purple_matrix_rust_poll_vote(purple_account_get_username(account), room_id, dup, (guint64)atoll(idx_str));
    } else {
       purple_notify_error(my_plugin, "Matrix", "Invalid format. Expected 'event_id index'", "e.g. $abc... 0");
    }
    g_free(dup);
  }
  g_free(room_id);
}

static void menu_action_poll_vote_conv_cb(PurpleConversation *conv, gpointer data) {
  const char *room_id = purple_conversation_get_name(conv);
  /* Trigger discovery so they appear in log if missed */
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_get_active_polls(purple_account_get_username(account), room_id);
  
  purple_request_input(account, "Vote on Poll", "Enter Poll Event ID and Option Index", "Copy the Event ID from the [Active Poll] message in history.\nFormat: event_id index (e.g. $abc... 0)", NULL, FALSE, FALSE, NULL, "Vote", G_CALLBACK(poll_vote_selected_cb), "Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

static void menu_action_poll_vote_blist_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  if (room_id) purple_request_input(chat->account, "Vote on Poll", "Enter Poll Event ID and Option Index", "Format: event_id index", NULL, FALSE, FALSE, NULL, "Vote", G_CALLBACK(poll_vote_selected_cb), "Cancel", NULL, chat->account, NULL, NULL, g_strdup(room_id));
}

void conversation_extended_menu_cb(PurpleConversation *conv, GList **list) {
  if (strcmp(purple_account_get_protocol_id(purple_conversation_get_account(conv)), "prpl-matrix-rust") != 0) return;
  *list = g_list_append(*list, purple_menu_action_new("Room Dashboard...", PURPLE_CALLBACK(menu_action_room_dashboard_conv_cb), conv, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Reply to Last Message", PURPLE_CALLBACK(menu_action_reply_conv_cb), conv, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Start Thread from Last", PURPLE_CALLBACK(menu_action_thread_conv_cb), conv, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Send Sticker...", PURPLE_CALLBACK(menu_action_sticker_conv_cb), conv, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Create Poll...", PURPLE_CALLBACK(menu_action_poll_conv_cb), conv, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Vote on Poll...", PURPLE_CALLBACK(menu_action_poll_vote_conv_cb), conv, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Invite User...", PURPLE_CALLBACK(action_join_room_cb), NULL, NULL));
  *list = g_list_append(*list, purple_menu_action_new("Security Status", PURPLE_CALLBACK(cmd_e2ee_status), NULL, NULL));
}

static void menu_action_reply_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, room_id, account);
  if (!conv) conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, room_id, account);
  if (conv) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id && text && *text) purple_matrix_rust_send_reply(purple_account_get_username(account), room_id, last_id, text);
  }
  g_free(room_id);
}

static void menu_action_thread_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, room_id, account);
  if (conv) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id && text && *text) {
      char *virtual_id = g_strdup_printf("%s|%s", room_id, last_id);
      ensure_thread_in_blist(account, virtual_id, "Thread", room_id);
      purple_matrix_rust_send_message(purple_account_get_username(account), virtual_id, text);
      g_free(virtual_id);
    }
  }
  g_free(room_id);
}

static void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_request_input(purple_conversation_get_account(conv), "Reply", "Reply to last message", "Enter your message:", NULL, TRUE, FALSE, NULL, "Send", G_CALLBACK(menu_action_reply_dialog_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, NULL, g_strdup(room_id));
}

static void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_request_input(purple_conversation_get_account(conv), "Start Thread", "Start thread from last message", "Enter your message:", NULL, TRUE, FALSE, NULL, "Send", G_CALLBACK(menu_action_thread_dialog_cb), "Cancel", NULL, purple_conversation_get_account(conv), NULL, NULL, g_strdup(room_id));
}

static void menu_action_reply_last_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  if (!room_id) return;
  purple_request_input(chat->account, "Reply", "Reply to last message", "Enter your message:", NULL, TRUE, FALSE, NULL, "Send", G_CALLBACK(menu_action_reply_dialog_cb), "Cancel", NULL, chat->account, NULL, NULL, g_strdup(room_id));
}

static void menu_action_thread_last_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  if (!room_id) return;
  purple_request_input(chat->account, "Start Thread", "Start thread from last message", "Enter your message:", NULL, TRUE, FALSE, NULL, "Send", G_CALLBACK(menu_action_thread_dialog_cb), "Cancel", NULL, chat->account, NULL, NULL, g_strdup(room_id));
}

static void menu_action_room_dashboard_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  open_room_dashboard(chat->account, room_id);
}

static void menu_action_mark_read_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  purple_matrix_rust_send_read_receipt(purple_account_get_username(chat->account), g_hash_table_lookup(purple_chat_get_components(chat), "room_id"), NULL);
}

static void menu_action_mark_unread_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  purple_matrix_rust_mark_unread(purple_account_get_username(chat->account), g_hash_table_lookup(purple_chat_get_components(chat), "room_id"), TRUE);
}

static void menu_action_e2ee_status_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  purple_matrix_rust_e2ee_status(purple_account_get_username(chat->account), g_hash_table_lookup(purple_chat_get_components(chat), "room_id"));
}

static void menu_action_leave_room_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  purple_matrix_rust_leave_room(purple_account_get_username(chat->account), g_hash_table_lookup(purple_chat_get_components(chat), "room_id"));
}

static void menu_action_buddy_list_help_cb(PurpleBlistNode *node, gpointer data) {
  purple_notify_info(my_plugin, "Matrix Help", "Matrix Buddy List", "Right-click on rooms to access Matrix-specific features.");
}

static void menu_action_user_info_cb(PurpleBlistNode *node, gpointer data) {
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  purple_matrix_rust_get_user_info(purple_account_get_username(purple_buddy_get_account(buddy)), purple_buddy_get_name(buddy));
}

static void menu_action_verify_buddy_cb(PurpleBlistNode *node, gpointer data) {
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  purple_matrix_rust_verify_user(purple_account_get_username(purple_buddy_get_account(buddy)), purple_buddy_get_name(buddy));
}

static void blist_action_send_sticker_cb(PurpleBlistNode *node, gpointer data) {
  purple_notify_info(my_plugin, "Stickers", "Send Sticker", "Use the conversation tab menu to pick stickers.");
}

static void menu_action_ignore_buddy_cb(PurpleBlistNode *node, gpointer data) {
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  purple_matrix_rust_ignore_user(purple_account_get_username(purple_buddy_get_account(buddy)), purple_buddy_get_name(buddy));
}

static PurpleCmdRet cmd_power_levels(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_get_power_levels(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  const char *name = purple_request_fields_get_string(fields, "name");
  const char *topic = purple_request_fields_get_string(fields, "topic");
  if (name) purple_matrix_rust_set_room_name(purple_account_get_username(account), room_id, name);
  if (topic) purple_matrix_rust_set_room_topic(purple_account_get_username(account), room_id, topic);
  g_free(room_id);
}

static void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  if (!room_id) return;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(group, purple_request_field_string_new("name", "Room Name", purple_chat_get_name(chat), FALSE));
  purple_request_field_group_add_field(group, purple_request_field_string_new("topic", "Topic", g_hash_table_lookup(purple_chat_get_components(chat), "topic"), TRUE));
  purple_request_fields(chat->account, "Room Settings", "Matrix Settings", NULL, fields, "Save", G_CALLBACK(room_settings_dialog_cb), "Cancel", NULL, chat->account, NULL, NULL, g_strdup(room_id));
}

GList *blist_node_menu_cb(PurpleBlistNode *node) {
  GList *list = NULL;
  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    PurpleChat *chat = (PurpleChat *)node;
    if (strcmp(purple_account_get_protocol_id(purple_chat_get_account(chat)), "prpl-matrix-rust") == 0) {
      list = g_list_append(list, purple_menu_action_new("Room Dashboard", PURPLE_CALLBACK(menu_action_room_dashboard_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Reply to Last Message", PURPLE_CALLBACK(menu_action_reply_last_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Start Thread from Last", PURPLE_CALLBACK(menu_action_thread_last_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Settings...", PURPLE_CALLBACK(menu_action_room_settings_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Vote on Poll...", PURPLE_CALLBACK(menu_action_poll_vote_blist_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Mark Read", PURPLE_CALLBACK(menu_action_mark_read_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Mark Unread", PURPLE_CALLBACK(menu_action_mark_unread_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Security Status", PURPLE_CALLBACK(menu_action_e2ee_status_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Power Levels", PURPLE_CALLBACK(cmd_power_levels), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Leave Room", PURPLE_CALLBACK(menu_action_leave_room_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Help", PURPLE_CALLBACK(menu_action_buddy_list_help_cb), node, NULL));
    }
  } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    if (strcmp(purple_account_get_protocol_id(purple_buddy_get_account((PurpleBuddy *)node)), "prpl-matrix-rust") == 0) {
      list = g_list_append(list, purple_menu_action_new("Get Info", PURPLE_CALLBACK(menu_action_user_info_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Verify User", PURPLE_CALLBACK(menu_action_verify_buddy_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Send Sticker", PURPLE_CALLBACK(blist_action_send_sticker_cb), node, NULL));
      list = g_list_append(list, purple_menu_action_new("Ignore User", PURPLE_CALLBACK(menu_action_ignore_buddy_cb), node, NULL));
    }
  }
  return list;
}

static void action_reconnect_cb(PurplePluginAction *action) { PurpleAccount *account = find_matrix_account(); if (account) matrix_login(account); }
static void action_my_profile_cb(PurplePluginAction *action) { purple_matrix_rust_get_my_profile(purple_account_get_username(find_matrix_account())); }
static void action_enable_backup_cb(PurplePluginAction *action) { purple_matrix_rust_enable_key_backup(purple_account_get_username(find_matrix_account())); }
static void action_bootstrap_crypto_cb(PurplePluginAction *action) { purple_matrix_rust_bootstrap_cross_signing(purple_account_get_username(find_matrix_account())); }

static void create_room_dialog_cb(void *user_data, PurpleRequestFields *fields) {
  const char *name = purple_request_fields_get_string(fields, "name");
  const char *topic = purple_request_fields_get_string(fields, "topic");
  gboolean public = purple_request_fields_get_bool(fields, "public");
  if (name && *name) purple_matrix_rust_create_room(purple_account_get_username(find_matrix_account()), name, topic, public);
}

static void action_create_room_cb(PurplePluginAction *action) {
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(group, purple_request_field_string_new("name", "Room Name", NULL, FALSE));
  purple_request_field_group_add_field(group, purple_request_field_string_new("topic", "Topic", NULL, TRUE));
  purple_request_field_group_add_field(group, purple_request_field_bool_new("public", "Publicly Searchable", FALSE));
  purple_request_fields(find_matrix_account(), "Create Room", "Create a new Matrix room", NULL, fields, "Create", G_CALLBACK(create_room_dialog_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL);
}

static void action_public_rooms_cb(PurplePluginAction *action) { purple_roomlist_show_with_account(find_matrix_account()); }

static void join_room_dialog_cb(void *user_data, const char *room_id) {
  if (room_id && *room_id) purple_matrix_rust_join_room(purple_account_get_username(find_matrix_account()), room_id);
}

static void action_join_room_cb(PurplePluginAction *action) {
  purple_request_input(find_matrix_account(), "Join Room", "Enter Room ID or Alias", "e.g. #room:example.com", NULL, FALSE, FALSE, NULL, "Join", G_CALLBACK(join_room_dialog_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL);
}

static void user_search_dialog_cb(void *user_data, const char *term) {
  if (term && *term) purple_matrix_rust_search_users(purple_account_get_username(find_matrix_account()), term, "System");
}

static void action_user_search_cb(PurplePluginAction *action) {
  purple_request_input(find_matrix_account(), "Search Users", "Enter search term", NULL, NULL, FALSE, FALSE, NULL, "Search", G_CALLBACK(user_search_dialog_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL);
}

static void action_reset_session_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (account) {
    purple_matrix_rust_destroy_session(purple_account_get_username(account));
    purple_notify_info(account, "Session Reset", "Matrix session has been cleared.", "Please reconnect to start a fresh login flow.");
  }
}

static void control_panel_cb(void *user_data, PurpleRequestFields *fields) {
  int choice = purple_request_fields_get_choice(fields, "action");
  PurpleAccount *account = (PurpleAccount *)user_data;
  const char *username = purple_account_get_username(account);
  switch(choice) {
    case 0: purple_matrix_rust_get_my_profile(username); break;
    case 1: purple_matrix_rust_enable_key_backup(username); break;
    case 2: purple_matrix_rust_bootstrap_cross_signing(username); break;
    case 3: purple_matrix_rust_recover_keys(username, NULL); break;
    case 4: purple_matrix_rust_export_keys(username, NULL, NULL); break;
  }
}

static void action_control_panel_cb(PurplePluginAction *action) {
  PurpleAccount *account = find_matrix_account();
  if (!account) return;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field = purple_request_field_choice_new("action", "Action", 0);
  purple_request_field_choice_add(field, "My Profile");
  purple_request_field_choice_add(field, "Enable Online Backup");
  purple_request_field_choice_add(field, "Bootstrap Cross-Signing");
  purple_request_field_choice_add(field, "Recover Keys");
  purple_request_field_choice_add(field, "Export E2EE Keys");
  purple_request_field_group_add_field(group, field);
  purple_request_fields(account, "Matrix Control Panel", "Global Actions", "Choose a global Matrix action to perform.", fields, "Execute", G_CALLBACK(control_panel_cb), "Cancel", NULL, account, NULL, NULL, account);
}

GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
  GList *l = NULL; 
  l = g_list_append(l, purple_plugin_action_new("My Profile", action_my_profile_cb));
  l = g_list_append(l, purple_plugin_action_new("Matrix Control Panel", action_control_panel_cb));
  l = g_list_append(l, purple_plugin_action_new("Join Room by ID...", action_join_room_cb));
  l = g_list_append(l, purple_plugin_action_new("Create New Room...", action_create_room_cb));
  l = g_list_append(l, purple_plugin_action_new("Search Users...", action_user_search_cb));
  l = g_list_append(l, purple_plugin_action_new("Browse Public Rooms", action_public_rooms_cb));
  l = g_list_append(l, purple_plugin_action_new("Reconnect", action_reconnect_cb));
  l = g_list_append(l, purple_plugin_action_new("Reset Matrix Session (Fix Login)", action_reset_session_cb));
  l = g_list_append(l, purple_plugin_action_new("Check/Enable Online Backup", action_enable_backup_cb));
  l = g_list_append(l, purple_plugin_action_new("Bootstrap Cross-Signing", action_bootstrap_crypto_cb));
  return l;
}

static PurpleCmdRet cmd_resync_history(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_resync_recent_history(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_enable_backup(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_enable_key_backup(purple_account_get_username(purple_conversation_get_account(conv)));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_thread(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_CHAT) { *error = g_strdup("Only supported in chats."); return PURPLE_CMD_RET_FAILED; }
  const char *last_event_id = purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) { *error = g_strdup("No message to reply to."); return PURPLE_CMD_RET_FAILED; }
  char *room_id_copy = dup_base_room_id(purple_conversation_get_name(conv));
  char *virtual_id = g_strdup_printf("%s|%s", room_id_copy, last_event_id);
  ensure_thread_in_blist(purple_conversation_get_account(conv), virtual_id, "Thread", room_id_copy);
  int chat_id = get_chat_id(virtual_id);
  if (!purple_find_chat(purple_account_get_connection(purple_conversation_get_account(conv)), chat_id)) serv_got_joined_chat(purple_account_get_connection(purple_conversation_get_account(conv)), chat_id, virtual_id);
  if (args[0] && strlen(args[0]) > 0) purple_matrix_rust_send_message(purple_account_get_username(purple_conversation_get_account(conv)), virtual_id, args[0]);
  g_free(virtual_id); g_free(room_id_copy); return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_list_threads(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  char *room_id = dup_base_room_id(purple_conversation_get_name(conv));
  if (room_id) { purple_matrix_rust_list_threads(purple_account_get_username(purple_conversation_get_account(conv)), room_id); g_free(room_id); }
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  char *room_id = dup_base_room_id(purple_conversation_get_name(conv));
  if (room_id) { purple_matrix_rust_leave_room(purple_account_get_username(purple_conversation_get_account(conv)), room_id); g_free(room_id); }
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_sticker(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_send_sticker(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_nick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_set_display_name(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_avatar(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_set_avatar(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reply(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id && args[0]) purple_matrix_rust_send_reply(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_edit(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id && args[0]) purple_matrix_rust_send_edit(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_mute(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_mute_state(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), TRUE);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unmute(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_mute_state(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), FALSE);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_logout(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_destroy_session(purple_account_get_username(purple_conversation_get_account(conv)));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reconnect(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_account_connect(purple_conversation_get_account(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_e2ee_status(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_e2ee_status(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_react(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id && args[0]) purple_matrix_rust_send_reaction(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_redact(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id) purple_matrix_rust_redact_event(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_read_receipt(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  purple_matrix_rust_send_read_receipt(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_history(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_fetch_more_history(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_who_read(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_who_read(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_help(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  GString *msg = g_string_new("<b>Matrix Command Help:</b><br/>");
  g_string_append(msg, "/matrix_thread <msg> - Reply in new thread<br/>/matrix_reply <msg> - Reply to last<br/>/matrix_react <emoji> - React to last<br/>/matrix_sticker_list - Sticker picker<br/>/matrix_poll_create <q> <opts> - Create poll<br/>/matrix_help - This help");
  purple_conversation_write(conv, "System", msg->str, PURPLE_MESSAGE_SYSTEM, time(NULL));
  g_string_free(msg, TRUE); return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_location(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0] && args[1]) purple_matrix_rust_send_location(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), atof(args[0]), atof(args[1]));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_create(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0] && args[1]) purple_matrix_rust_poll_create(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_vote(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0] && args[1]) purple_matrix_rust_poll_vote(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], (guint64)atoll(args[1]));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_kick_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_ban_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_unban_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_invite(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_invite_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_tag(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_tag(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ignore(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_ignore_user(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_name(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_name(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_topic(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_topic(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_create(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_create_alias(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_delete(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_delete_alias(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_bulk_redact(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_bulk_redact(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), atoi(args[0]), args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_qr_login(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_login_with_qr(purple_account_get_string(purple_conversation_get_account(conv), "server", "https://matrix.org"), purple_user_dir());
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_my_profile(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_get_my_profile(purple_account_get_username(purple_conversation_get_account(conv)));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_create_room(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_create_room(purple_account_get_username(purple_conversation_get_account(conv)), args[0], NULL, FALSE);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_upgrade_room(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_upgrade_room(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_report(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_report_content(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_knock(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_knock(purple_account_get_username(purple_conversation_get_account(conv)), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_join_rule(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_join_rule(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_guest_access(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_guest_access(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), (strcmp(args[0], "allow") == 0));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_history_visibility(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_history_visibility(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_room_avatar(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_avatar(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_public_rooms(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_search_public_rooms(purple_account_get_username(purple_conversation_get_account(conv)), args[0], purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_user_search(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_search_users(purple_account_get_username(purple_conversation_get_account(conv)), args[0], purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_create_alias(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  menu_action_poll_conv_cb(conv, NULL);
  return PURPLE_CMD_RET_OK;
}

void register_matrix_commands(PurplePlugin *plugin) {
  /* Short-hand aliases */
  purple_cmd_register("reply", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reply, "reply <msg>: Reply to the last received message.", NULL);
  purple_cmd_register("thread", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread, "thread <msg>: Start a new thread from the last message.", NULL);
  purple_cmd_register("sticker", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker_list, "sticker: Open the Matrix sticker picker.", NULL);
  purple_cmd_register("poll", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll_create_alias, "poll: Create a new Matrix poll.", NULL);

  /* Full commands */
  purple_cmd_register("matrix_sync_history", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_resync_history, "matrix_sync_history: Reset and resync history for the current room.", NULL);
  purple_cmd_register("matrix_enable_backup", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_enable_backup, "matrix_enable_backup: Check or enable online key backup on the server.", NULL);
  purple_cmd_register("matrix_thread", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread, "matrix_thread <msg>: Reply to the last message in a new thread.", NULL);
  purple_cmd_register("matrix_threads", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_list_threads, "matrix_threads: List active threads in the current room.", NULL);
  purple_cmd_register("matrix_leave", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_leave, "matrix_leave [reason]: Leave the current room.", NULL);
  purple_cmd_register("matrix_sticker", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker, "matrix_sticker <url>: Send a sticker from an MXC or file URL.", NULL);
  purple_cmd_register("matrix_sticker_list", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker_list, "matrix_sticker_list: Show available sticker packs.", NULL);
  purple_cmd_register("matrix_nick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_nick, "matrix_nick <name>: Set your global Matrix display name.", NULL);
  purple_cmd_register("matrix_avatar", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_avatar, "matrix_avatar <path>: Set your global Matrix avatar from a local file.", NULL);
  purple_cmd_register("matrix_reply", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reply, "matrix_reply <msg>: Reply to the last received message.", NULL);
  purple_cmd_register("matrix_edit", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_edit, "matrix_edit <msg>: Edit your last sent message.", NULL);
  purple_cmd_register("matrix_mute", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_mute, "matrix_mute: Set room notification mode to 'Mentions Only'.", NULL);
  purple_cmd_register("matrix_unmute", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_unmute, "matrix_unmute: Set room notification mode to 'All Messages'.", NULL);
  purple_cmd_register("matrix_logout", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_logout, "matrix_logout: Invalidate the current session and logout.", NULL);
  purple_cmd_register("matrix_reconnect", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reconnect, "matrix_reconnect: Force a reconnection attempt.", NULL);
  purple_cmd_register("matrix_e2ee_status", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_e2ee_status, "matrix_e2ee_status: Show encryption and cross-signing status.", NULL);
  purple_cmd_register("matrix_react", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_react, "matrix_react <emoji>: React to the last received message.", NULL);
  purple_cmd_register("matrix_redact", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_redact, "matrix_redact [reason]: Redact the last received message.", NULL);
  purple_cmd_register("matrix_read_receipt", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_read_receipt, "matrix_read_receipt: Manually send a read receipt for the last message.", NULL);
  purple_cmd_register("matrix_history", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_history, "matrix_history: Fetch the next page of back-history for this room.", NULL);
  purple_cmd_register("matrix_who_read", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_who_read, "matrix_who_read: Show who has read the latest message in this room.", NULL);
  purple_cmd_register("matrix_help", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_help, "matrix_help: Show detailed help for Matrix commands.", NULL);
  purple_cmd_register("matrix_location", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_location, "matrix_location <lat> <lon>: Send location.", NULL);
  purple_cmd_register("matrix_poll_create", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll_create, "matrix_poll_create <question> <options|split|by|pipe>: Create poll.", NULL);
  purple_cmd_register("matrix_poll_vote", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll_vote, "matrix_poll_vote <poll_id> <index>: Vote on poll.", NULL);
  purple_cmd_register("matrix_kick", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_kick, "matrix_kick <user> [reason]: Kick user.", NULL);
  purple_cmd_register("matrix_ban", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_ban, "matrix_ban <user> [reason]: Ban user.", NULL);
  purple_cmd_register("matrix_unban", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_unban, "matrix_unban <user> [reason]: Unban user.", NULL);
  purple_cmd_register("matrix_invite", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_invite, "matrix_invite <user>: Invite user.", NULL);
  purple_cmd_register("matrix_tag", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_tag, "matrix_tag <tag>: Set room tag.", NULL);
  purple_cmd_register("matrix_ignore", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_ignore, "matrix_ignore <user>: Ignore user.", NULL);
  purple_cmd_register("matrix_set_name", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_name, "matrix_set_name <name>: Set room name.", NULL);
  purple_cmd_register("matrix_set_topic", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_topic, "matrix_set_topic <topic>: Set room topic.", NULL);
  purple_cmd_register("matrix_alias_create", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_alias_create, "matrix_alias_create <alias>: Create alias.", NULL);
  purple_cmd_register("matrix_alias_delete", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_alias_delete, "matrix_alias_delete <alias>: Delete alias.", NULL);
  purple_cmd_register("matrix_bulk_redact", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_bulk_redact, "matrix_bulk_redact <count> <user>: Bulk redact.", NULL);
  purple_cmd_register("matrix_qr_login", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_qr_login, "matrix_qr_login: Start QR login.", NULL);
  purple_cmd_register("matrix_my_profile", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_my_profile, "matrix_my_profile: Show your profile.", NULL);
  purple_cmd_register("matrix_create_room", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_create_room, "matrix_create_room <name>: Create room.", NULL);
  purple_cmd_register("matrix_upgrade_room", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_upgrade_room, "matrix_upgrade_room <ver>: Upgrade room.", NULL);
  purple_cmd_register("matrix_report", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_report, "matrix_report <id> <reason>: Report content.", NULL);
  purple_cmd_register("matrix_knock", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_knock, "matrix_knock <id> <reason>: Knock on room.", NULL);
  purple_cmd_register("matrix_join_rule", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_join_rule, "matrix_join_rule <rule>: Set join rule.", NULL);
  purple_cmd_register("matrix_guest_access", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_guest_access, "matrix_guest_access <allow|forbid>: Set guest access.", NULL);
  purple_cmd_register("matrix_history_visibility", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_history_visibility, "matrix_history_visibility <vis>: Set history visibility.", NULL);
  purple_cmd_register("matrix_set_room_avatar", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_room_avatar, "matrix_set_room_avatar <path>: Set room avatar.", NULL);
  purple_cmd_register("matrix_public_rooms", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_public_rooms, "matrix_public_rooms [term]: Search public rooms.", NULL);
  purple_cmd_register("matrix_user_search", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_user_search, "matrix_user_search <term>: Search user directory.", NULL);
}
