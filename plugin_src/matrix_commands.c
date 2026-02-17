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
static void action_join_room_cb(PurplePluginAction *action);
static void action_invite_user_cb(PurplePluginAction *action);
static void action_sso_token_cb(PurplePluginAction *action);
static void menu_action_reply_dialog_cb(void *user_data, const char *text);
static void menu_action_thread_dialog_cb(void *user_data, const char *text);
static void menu_action_room_dashboard_conv_cb(PurpleConversation *conv, gpointer data);
static void join_room_dialog_cb(void *user_data, const char *room_id);
static void invite_user_dialog_cb(void *user_data, const char *user_id);
void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields);
static PurpleCmdRet cmd_sticker_list(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);

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
static void sticker_pack_cb(const char *id, const char *name, void *user_data) {
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

/* Dashboard */
static void dash_choice_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleRequestField *f = purple_request_fields_get_field(fields, "action");
  int choice = purple_request_field_choice_get_value(f);
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) {
    switch(choice) {
      case 0: menu_action_reply_conv_cb(conv, NULL); break;
      case 1: menu_action_thread_conv_cb(conv, NULL); break;
      case 2: menu_action_sticker_conv_cb(conv, NULL); break;
      case 3: menu_action_poll_conv_cb(conv, NULL); break;
      case 4: action_invite_user_cb(NULL); break;
      case 5: { PurpleBlistNode *node = (PurpleBlistNode *)purple_blist_find_chat(account, room_id); if (node) menu_action_room_settings_cb(node, NULL); break; }
      case 6: purple_matrix_rust_mark_unread(purple_account_get_username(account), room_id, TRUE); break;
      case 7: purple_matrix_rust_list_threads(purple_account_get_username(account), room_id); break;
      case 8: purple_matrix_rust_e2ee_status(purple_account_get_username(account), room_id); break;
      case 9: purple_matrix_rust_get_power_levels(purple_account_get_username(account), room_id); break;
    }
  }
  g_free(room_id);
}
void open_room_dashboard(PurpleAccount *account, const char *room_id) {
  if (!room_id || !account) return;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("Available Tasks");
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *field = purple_request_field_choice_new("action", "What would you like to do?", 0);
  const char *tasks[] = { "Reply to latest", "Start new thread", "Send sticker", "Create poll", "Invite user", "Room settings", "Mark as read", "View threads", "E2EE status", "Power levels" };
  for (int i = 0; i < 10; i++) purple_request_field_choice_add(field, tasks[i]);
  purple_request_field_group_add_field(group, field);
  purple_request_fields(my_plugin, "Room Dashboard", "Matrix Room Tasks", NULL, fields, "_Execute", G_CALLBACK(dash_choice_cb), "_Cancel", NULL, account, NULL, NULL, g_strdup(room_id));
}

void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv) return;
  purple_request_input(my_plugin, "Reply Wizard", "Reply to Message", NULL, NULL, TRUE, FALSE, NULL, "_Send", G_CALLBACK(menu_action_reply_dialog_cb), "_Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, g_strdup(purple_conversation_get_name(conv)));
}
void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data) {
  if (!conv) return;
  purple_request_input(my_plugin, "Thread Wizard", "Start Thread", NULL, NULL, TRUE, FALSE, NULL, "_Start", G_CALLBACK(menu_action_thread_dialog_cb), "_Cancel", NULL, purple_conversation_get_account(conv), NULL, conv, g_strdup(purple_conversation_get_name(conv)));
}
static void menu_action_reply_dialog_cb(void *user_data, const char *text) {
  char *room_id = (char *)user_data; PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv && text && *text) { const char *last_id = purple_conversation_get_data(conv, "last_event_id"); if (last_id) purple_matrix_rust_send_reply(purple_account_get_username(account), room_id, last_id, text); }
  g_free(room_id);
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

static void action_sso_token_cb(PurplePluginAction *action) { 
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);
    purple_request_field_group_add_field(group, purple_request_field_string_new("sso_token", "Login Token", NULL, FALSE));
    purple_request_fields(my_plugin, "SSO Login", "Authentication Required", "If you have a manual SSO login token, paste it below.", fields, "Submit", G_CALLBACK(manual_sso_token_action_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, find_matrix_account());
}

void room_settings_dialog_cb(void *user_data, PurpleRequestFields *fields) { 
    char *room_id = (char *)user_data; 
    PurpleAccount *account = find_matrix_account(); 
    const char *name = purple_request_fields_get_string(fields, "name"); 
    const char *topic = purple_request_fields_get_string(fields, "topic"); 
    if (account) { 
        if (name) purple_matrix_rust_set_room_name(purple_account_get_username(account), room_id, name); 
        if (topic) purple_matrix_rust_set_room_topic(purple_account_get_username(account), room_id, topic); 
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
    
    purple_request_fields(my_plugin, "Room Settings", "Edit Matrix Room Details", NULL, fields, "Save", G_CALLBACK(room_settings_dialog_cb), "Cancel", NULL, purple_chat_get_account(chat), NULL, NULL, g_strdup(room_id));
}

/* Commands & Registration */
static PurpleCmdRet cmd_help(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_conversation_write(conv, "System", "<b>Matrix Help:</b> /reply, /thread, /poll, /sticker, /shrug, /join, /invite", PURPLE_MESSAGE_SYSTEM, time(NULL)); return PURPLE_CMD_RET_OK;
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

static PurpleCmdRet cmd_join(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (args[0] && *args[0]) {
        PurpleAccount *account = purple_conversation_get_account(conv);
        purple_matrix_rust_join_room(purple_account_get_username(account), args[0]);
        return PURPLE_CMD_RET_OK;
    }
    return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet cmd_invite(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
    if (args[0] && *args[0]) {
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
    return PURPLE_CMD_RET_FAILED;
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

void register_matrix_commands(PurplePlugin *plugin) {
  purple_cmd_register("matrix_help", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_help, "help", NULL);
  purple_cmd_register("sticker", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker, "sticker: Browse and send stickers", NULL);
  purple_cmd_register("poll", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll, "poll: Create a new poll", NULL);
  purple_cmd_register("reply", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reply, "reply: Reply to the latest message", NULL);
  purple_cmd_register("thread", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread, "thread: Start a new thread from the latest message", NULL);
  
  purple_cmd_register("join", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_join, "join <room_id_or_alias>: Join a room", NULL);
  purple_cmd_register("invite", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_invite, "invite <user_id>: Invite a user", NULL);
  purple_cmd_register("shrug", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_shrug, "shrug [message]: Add a shrug emoji", NULL);
}

GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
    GList *m = NULL;
    PurplePluginAction *act;

    act = purple_plugin_action_new("Join Room...", action_join_room_cb);
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Invite User...", action_invite_user_cb);
    m = g_list_append(m, act);

    act = purple_plugin_action_new("Enter Manual SSO Token...", action_sso_token_cb);
    m = g_list_append(m, act);

    return m;
}
