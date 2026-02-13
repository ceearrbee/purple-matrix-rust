#include "matrix_commands.h"
#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_blist.h"
#include "matrix_chat.h"
#include "matrix_account.h"
#include "matrix_types.h"

#include <libpurple/util.h>
#include <libpurple/debug.h>
#include <libpurple/cmds.h>
#include <libpurple/request.h>
#include <libpurple/notify.h>
#include <libpurple/roomlist.h>
#include <libpurple/server.h>
#include <string.h>
#include <stdlib.h>

extern void purple_matrix_rust_set_room_name(const char *user_id, const char *room_id, const char *name);

static char *dup_base_room_id(const char *raw_room_id) {
  if (!raw_room_id) return NULL;
  char *room_id = g_strdup(raw_room_id);
  char *pipe = strchr(room_id, '|');
  if (pipe) *pipe = '\0';
  return room_id;
}

static PurpleCmdRet cmd_sticker(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) { *error = g_strdup("Usage: /matrix_sticker <url>"); return PURPLE_CMD_RET_FAILED; }
  purple_matrix_rust_send_sticker(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reply(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (!last_id) { *error = g_strdup("No message to reply to."); return PURPLE_CMD_RET_FAILED; }
  char *msg = g_strjoinv(" ", args);
  purple_matrix_rust_send_reply(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, msg);
  g_free(msg); return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_edit(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (!last_id) { *error = g_strdup("No message to edit."); return PURPLE_CMD_RET_FAILED; }
  char *msg = g_strjoinv(" ", args);
  purple_matrix_rust_send_edit(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, msg);
  g_free(msg); return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_nick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_set_display_name(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_avatar(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_set_avatar(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  char *room_id = dup_base_room_id(purple_conversation_get_name(conv));
  if (room_id) {
    purple_matrix_rust_leave_room(purple_account_get_username(purple_conversation_get_account(conv)), room_id);
    purple_conversation_write(conv, "System", "Leaving room...", PURPLE_MESSAGE_SYSTEM, time(NULL));
    g_free(room_id);
  }
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

static GHashTable *pending_thread_lists = NULL;
static void free_thread_list_item(gpointer data) {
  ThreadListItem *item = (ThreadListItem *)data;
  g_free(item->room_id); g_free(item->thread_root_id); g_free(item->latest_msg); g_free(item);
}
static void free_thread_list_glist(gpointer data) { g_list_free_full((GList *)data, free_thread_list_item); }
static void free_display_context(ThreadDisplayContext *ctx) { if (ctx) { g_free(ctx->room_id); g_list_free_full(ctx->threads, free_thread_list_item); g_free(ctx); } }

static void thread_list_action_cb(void *user_data, PurpleRequestFields *fields) {
  ThreadDisplayContext *ctx = (ThreadDisplayContext *)user_data;
  int index = purple_request_field_choice_get_value(purple_request_fields_get_field(fields, "thread_id"));
  ThreadListItem *selected = (ThreadListItem *)g_list_nth_data(ctx->threads, index);
  if (selected && selected->thread_root_id) {
    char *join_id = g_strdup_printf("%s|%s", ctx->room_id, selected->thread_root_id);
    purple_matrix_rust_join_room(purple_account_get_username(find_matrix_account()), join_id);
    g_free(join_id);
  }
  free_display_context(ctx);
}

static void thread_list_close_cb(void *user_data) { free_display_context((ThreadDisplayContext *)user_data); }

static gboolean process_thread_list_with_ui(gpointer data) {
  ThreadListItem *item = (ThreadListItem *)data;
  if (!pending_thread_lists) pending_thread_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_thread_list_glist);
  GList *list = g_hash_table_lookup(pending_thread_lists, item->room_id);
  if (item->thread_root_id) {
    list = g_list_append(list, item);
    g_hash_table_replace(pending_thread_lists, g_strdup(item->room_id), list);
    return FALSE;
  } else {
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);
    PurpleRequestField *field = purple_request_field_choice_new("thread_id", "Select thread", 0);
    gboolean any = FALSE;
    for (GList *l = list; l; l = l->next) {
      ThreadListItem *t = (ThreadListItem *)l->data;
      if (t->thread_root_id) {
        char *label = g_strdup_printf("%s (%lu)", t->latest_msg ? t->latest_msg : "...", (unsigned long)t->count);
        purple_request_field_choice_add(field, label); g_free(label); any = TRUE;
      }
    }
    purple_request_field_group_add_field(group, field);
    if (any) {
      ThreadDisplayContext *ctx = g_new0(ThreadDisplayContext, 1); ctx->room_id = g_strdup(item->room_id);
      g_hash_table_steal(pending_thread_lists, item->room_id); ctx->threads = list;
      purple_request_fields(find_matrix_account(), "Threads", "Active Threads", NULL, fields, "Open", G_CALLBACK(thread_list_action_cb), "Cancel", G_CALLBACK(thread_list_close_cb), find_matrix_account(), NULL, NULL, ctx);
    } else {
      purple_notify_info(NULL, "Threads", "None found.", NULL);
      g_hash_table_remove(pending_thread_lists, item->room_id);
    }
    free_thread_list_item(item); return FALSE;
  }
}

void thread_list_cb(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts) {
  ThreadListItem *item = g_new0(ThreadListItem, 1);
  item->room_id = g_strdup(room_id); if (thread_root_id) item->thread_root_id = g_strdup(thread_root_id);
  if (latest_msg) item->latest_msg = g_strdup(latest_msg);
  item->count = count; item->ts = ts;
  g_idle_add(process_thread_list_with_ui, item);
}

static GHashTable *pending_poll_lists = NULL;
static void free_poll_list_item(gpointer data) {
  PollListItem *item = (PollListItem *)data;
  g_free(item->room_id); g_free(item->event_id); g_free(item->sender); g_free(item->question); g_free(item->options_str); g_free(item);
}
static void free_poll_list_glist(gpointer data) { g_list_free_full((GList *)data, free_poll_list_item); }

static void poll_vote_action_cb(void *user_data, PurpleRequestFields *fields) {
  PollVoteContext *ctx = (PollVoteContext *)user_data;
  int index = purple_request_field_choice_get_value(purple_request_fields_get_field(fields, "option_idx"));
  char **options = g_strsplit(ctx->options_str, "|", -1);
  int count = 0; while (options[count]) count++;
  if (index >= 0 && index < count) {
    char *opt = options[index]; char *caret = strchr(opt, '^');
    purple_matrix_rust_poll_vote(purple_account_get_username(find_matrix_account()), ctx->room_id, ctx->event_id, "ignored", caret ? opt : opt);
  }
  g_strfreev(options); g_free(ctx->room_id); g_free(ctx->event_id); g_free(ctx->options_str); g_free(ctx);
}

static void poll_vote_cancel_cb(void *user_data) { PollVoteContext *ctx = (PollVoteContext *)user_data; g_free(ctx->room_id); g_free(ctx->event_id); g_free(ctx->options_str); g_free(ctx); }

static void show_vote_ui_cb(void *user_data, PurpleRequestFields *fields) {
  PollDisplayContext *ctx = (PollDisplayContext *)user_data;
  int index = purple_request_field_choice_get_value(purple_request_fields_get_field(fields, "poll_id"));
  PollListItem *item = (PollListItem *)g_list_nth_data(ctx->polls, index);
  if (item) {
    PurpleRequestFields *v_fields = purple_request_fields_new(); PurpleRequestFieldGroup *v_group = purple_request_field_group_new(NULL); purple_request_fields_add_group(v_fields, v_group);
    PurpleRequestField *v_field = purple_request_field_choice_new("option_idx", "Option", 0);
    char **options = g_strsplit(item->options_str, "|", -1);
    for (int i = 0; options[i]; i++) { char *caret = strchr(options[i], '^'); purple_request_field_choice_add(v_field, caret ? caret + 1 : options[i]); }
    g_strfreev(options); purple_request_field_group_add_field(v_group, v_field);
    PollVoteContext *v_ctx = g_new0(PollVoteContext, 1); v_ctx->room_id = g_strdup(item->room_id); v_ctx->event_id = g_strdup(item->event_id); v_ctx->options_str = g_strdup(item->options_str);
    purple_request_fields(find_matrix_account(), "Vote", item->question, NULL, v_fields, "Vote", G_CALLBACK(poll_vote_action_cb), "Cancel", G_CALLBACK(poll_vote_cancel_cb), find_matrix_account(), NULL, NULL, v_ctx);
  }
  g_free(ctx->room_id); g_list_free_full(ctx->polls, free_poll_list_item); g_free(ctx);
}

static gboolean process_poll_list_with_ui(gpointer data) {
  PollListItem *item = (PollListItem *)data;
  if (!pending_poll_lists) pending_poll_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_poll_list_glist);
  GList *list = g_hash_table_lookup(pending_poll_lists, item->room_id);
  if (item->event_id) {
    list = g_list_append(list, item); g_hash_table_replace(pending_poll_lists, g_strdup(item->room_id), list);
    return FALSE;
  } else {
    PurpleRequestFields *fields = purple_request_fields_new(); PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL); purple_request_fields_add_group(fields, group);
    PurpleRequestField *field = purple_request_field_choice_new("poll_id", "Select poll", 0);
    gboolean any = FALSE;
    for (GList *l = list; l; l = l->next) { PollListItem *p = (PollListItem *)l->data; if (p->event_id) { purple_request_field_choice_add(field, p->question); any = TRUE; } }
    purple_request_field_group_add_field(group, field);
    if (any) {
      PollDisplayContext *ctx = g_new0(PollDisplayContext, 1); ctx->room_id = g_strdup(item->room_id); g_hash_table_steal(pending_poll_lists, item->room_id); ctx->polls = list;
      purple_request_fields(find_matrix_account(), "Polls", "Active Polls", NULL, fields, "Select", G_CALLBACK(show_vote_ui_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, ctx);
    } else { purple_notify_info(NULL, "Polls", "None found.", NULL); g_hash_table_remove(pending_poll_lists, item->room_id); }
    free_poll_list_item(item); return FALSE;
  }
}

void poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str) {
  PollListItem *item = g_new0(PollListItem, 1); item->room_id = g_strdup(room_id); if (event_id) item->event_id = g_strdup(event_id);
  if (sender) item->sender = g_strdup(sender); if (question) item->question = g_strdup(question); if (options_str) item->options_str = g_strdup(options_str);
  g_idle_add(process_poll_list_with_ui, item);
}

static void menu_action_buddy_list_help_cb(PurpleBlistNode *node, gpointer data) { purple_notify_info(NULL, "Help", "Buddy List Help", "Rooms grouped by Space/Tag."); }
static void blist_action_send_sticker_cb(PurpleBlistNode *node, gpointer data) { purple_notify_info(NULL, "Sticker", "Send Sticker", "Use /matrix_sticker"); }
static void menu_action_verify_buddy_cb(PurpleBlistNode *node, gpointer data) { purple_matrix_rust_verify_user(purple_account_get_username(purple_buddy_get_account((PurpleBuddy *)node)), purple_buddy_get_name((PurpleBuddy *)node)); }
static void menu_action_mark_read_cb(PurpleBlistNode *node, gpointer data) { purple_matrix_rust_send_read_receipt(purple_account_get_username(purple_chat_get_account((PurpleChat *)node)), g_hash_table_lookup(purple_chat_get_components((PurpleChat *)node), "room_id"), NULL); }

static void room_settings_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data; const char *name = purple_request_fields_get_string(fields, "name"); const char *topic = purple_request_fields_get_string(fields, "topic");
  if (name && *name) purple_matrix_rust_set_room_name(purple_account_get_username(find_matrix_account()), room_id, name);
  if (topic) purple_matrix_rust_set_room_topic(purple_account_get_username(find_matrix_account()), room_id, topic);
  g_free(room_id);
}

static void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node; const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id"); if (!room_id) return;
  PurpleRequestFields *fields = purple_request_fields_new(); PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL); purple_request_fields_add_group(fields, group);
  purple_request_field_group_add_field(group, purple_request_field_string_new("name", "Name", chat->alias, FALSE));
  purple_request_field_group_add_field(group, purple_request_field_string_new("topic", "Topic", g_hash_table_lookup(purple_chat_get_components(chat), "topic"), TRUE));
  purple_request_fields(purple_chat_get_account(chat), "Settings", "Room Settings", NULL, fields, "Save", G_CALLBACK(room_settings_cb), "Cancel", NULL, purple_chat_get_account(chat), NULL, NULL, g_strdup(room_id));
}

GList *blist_node_menu_cb(PurpleBlistNode *node) {
  GList *list = NULL;
  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    PurpleChat *chat = (PurpleChat *)node;
    if (strcmp(purple_account_get_protocol_id(purple_chat_get_account(chat)), "prpl-matrix-rust") == 0) {
      list = g_list_append(list, purple_menu_action_new("Settings...", PURPLE_CALLBACK(menu_action_room_settings_cb), NULL, NULL));
      list = g_list_append(list, purple_menu_action_new("Mark Read", PURPLE_CALLBACK(menu_action_mark_read_cb), NULL, NULL));
    }
  } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    if (strcmp(purple_account_get_protocol_id(purple_buddy_get_account((PurpleBuddy *)node)), "prpl-matrix-rust") == 0)
      list = g_list_append(list, purple_menu_action_new("Verify User", PURPLE_CALLBACK(menu_action_verify_buddy_cb), NULL, NULL));
  }
  return list;
}

void register_matrix_commands(PurplePlugin *plugin) {
  purple_cmd_register("matrix_thread", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread, "matrix_thread <msg>", NULL);
  purple_cmd_register("matrix_leave", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_leave, "matrix_leave [reason]", NULL);
  purple_cmd_register("matrix_sticker", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker, "matrix_sticker <url>", NULL);
  purple_cmd_register("matrix_nick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_nick, "matrix_nick <name>", NULL);
  purple_cmd_register("matrix_avatar", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_avatar, "matrix_avatar <path>", NULL);
  purple_cmd_register("matrix_reply", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reply, "matrix_reply <msg>", NULL);
  purple_cmd_register("matrix_edit", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_edit, "matrix_edit <msg>", NULL);
}
