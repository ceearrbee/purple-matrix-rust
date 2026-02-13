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

static PurpleCmdRet cmd_set_room_avatar(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_set_room_avatar(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_unban_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_mute(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_mute_state(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), true);
  purple_conversation_write(conv, "System", "Muting room...", PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unmute(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_set_room_mute_state(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), false);
  purple_conversation_write(conv, "System", "Unmuting room...", PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_logout(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_destroy_session(purple_account_get_username(purple_conversation_get_account(conv)));
  purple_conversation_write(conv, "System", "Logging out explicitly... Session invalidated.", PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reconnect(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_account_connect(account);
  purple_conversation_write(conv, "System", "Reconnecting account...", PURPLE_MESSAGE_SYSTEM, time(NULL));
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

static PurpleCmdRet cmd_list_threads(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  char *room_id = dup_base_room_id(purple_conversation_get_name(conv));
  if (room_id) {
    purple_matrix_rust_list_threads(purple_account_get_username(purple_conversation_get_account(conv)), room_id);
    purple_conversation_write(conv, "System", "Loading thread list...", PURPLE_MESSAGE_SYSTEM, time(NULL));
    g_free(room_id);
  }
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_separate_threads(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  gboolean enabled = (g_ascii_strcasecmp(args[0], "on") == 0 || g_ascii_strcasecmp(args[0], "true") == 0 || strcmp(args[0], "1") == 0);
  purple_account_set_bool(purple_conversation_get_account(conv), "separate_threads", enabled);
  purple_conversation_write(conv, "System", enabled ? "Separate thread tabs enabled." : "Separate thread tabs disabled.", PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_help(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_conversation_write(conv, "Matrix Help", "Matrix-specific features: /matrix_<command>. See README.", PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_location(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0] || !args[1]) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_send_location(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_create(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0] || !args[1]) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_poll_create(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_vote(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (!last_id) return PURPLE_CMD_RET_FAILED;
  purple_matrix_rust_poll_vote(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id, "Unknown", args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_end(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id) purple_matrix_rust_poll_end(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_verify(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_verify_user(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_e2ee_status(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_e2ee_status(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reset_cross_signing(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_bootstrap_cross_signing(purple_account_get_username(purple_conversation_get_account(conv)));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_recover_keys(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_recover_keys(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_restore_backup(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_restore_from_backup(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_read_receipt(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  const char *last_id = purple_conversation_get_data(conv, "last_event_id");
  if (last_id) purple_matrix_rust_send_read_receipt(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), last_id);
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

static PurpleCmdRet cmd_history(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  PurpleAccount *acc = purple_conversation_get_account(conv);
  purple_matrix_rust_fetch_more_history_with_limit(purple_account_get_username(acc), purple_conversation_get_name(conv), get_history_page_size(acc));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_preview_room(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_preview_room(purple_account_get_username(purple_conversation_get_account(conv)), args[0], "Matrix Preview");
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_history_page_size(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  purple_account_set_string(purple_conversation_get_account(conv), "history_page_size", args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_auto_fetch_history(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (!args[0]) return PURPLE_CMD_RET_FAILED;
  gboolean en = (g_ascii_strcasecmp(args[0], "on") == 0 || g_ascii_strcasecmp(args[0], "true") == 0 || strcmp(args[0], "1") == 0);
  purple_account_set_bool(purple_conversation_get_account(conv), "auto_fetch_history_on_open", en);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_create_room(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_create_room(purple_account_get_username(purple_conversation_get_account(conv)), args[0], NULL, false);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_public_rooms(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_search_public_rooms(purple_account_get_username(purple_conversation_get_account(conv)), args[0], purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_user_search(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_search_users(purple_account_get_username(purple_conversation_get_account(conv)), args[0], purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_kick(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_kick_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ban(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_ban_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_invite(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_invite_user(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_tag(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_set_room_tag(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ignore(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_ignore_user(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_power_levels(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_get_power_levels(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_power_level(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0] && args[1]) purple_matrix_rust_set_power_level(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], atoll(args[1]));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_name(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_set_room_name(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_topic(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_set_room_topic(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_create(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_create_alias(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_delete(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_delete_alias(purple_account_get_username(purple_conversation_get_account(conv)), args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_report(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_report_content(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_export_keys(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0] && args[1]) purple_matrix_rust_export_room_keys(purple_account_get_username(purple_conversation_get_account(conv)), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_bulk_redact(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_bulk_redact(purple_account_get_username(purple_conversation_get_account(conv)), purple_conversation_get_name(conv), atoi(args[0]), args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_knock(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  if (args[0]) purple_matrix_rust_knock(purple_account_get_username(purple_conversation_get_account(conv)), args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static void free_thread_list_item(gpointer data) {
  ThreadListItem *item = (ThreadListItem *)data; g_free(item->room_id); g_free(item->thread_root_id); g_free(item->latest_msg); g_free(item);
}
static void free_thread_list_glist(gpointer data) { g_list_free_full((GList *)data, free_thread_list_item); }
static void free_display_context(ThreadDisplayContext *ctx) { if (ctx) { g_free(ctx->room_id); g_list_free_full(ctx->threads, free_thread_list_item); g_free(ctx); } }

static void thread_list_action_cb(void *user_data, PurpleRequestFields *fields) {
  ThreadDisplayContext *ctx = (ThreadDisplayContext *)user_data;
  int idx = purple_request_field_choice_get_value(purple_request_fields_get_field(fields, "thread_id"));
  ThreadListItem *sel = (ThreadListItem *)g_list_nth_data(ctx->threads, idx);
  if (sel && sel->thread_root_id) {
    char *jid = g_strdup_printf("%s|%s", ctx->room_id, sel->thread_root_id);
    purple_matrix_rust_join_room(purple_account_get_username(find_matrix_account()), jid); g_free(jid);
  }
  free_display_context(ctx);
}

static GHashTable *pending_thread_lists = NULL;
static gboolean process_thread_list_with_ui(gpointer data) {
  ThreadListItem *item = (ThreadListItem *)data;
  if (!pending_thread_lists) pending_thread_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_thread_list_glist);
  GList *list = g_hash_table_lookup(pending_thread_lists, item->room_id);
  if (item->thread_root_id) { list = g_list_append(list, item); g_hash_table_replace(pending_thread_lists, g_strdup(item->room_id), list); return FALSE; }
  else {
    PurpleRequestFields *flds = purple_request_fields_new(); PurpleRequestFieldGroup *grp = purple_request_field_group_new(NULL); purple_request_fields_add_group(flds, grp);
    PurpleRequestField *fld = purple_request_field_choice_new("thread_id", "Select thread", 0); gboolean any = FALSE;
    for (GList *l = list; l; l = l->next) {
      ThreadListItem *t = (ThreadListItem *)l->data;
      if (t->thread_root_id) { char *lbl = g_strdup_printf("%s (%lu)", t->latest_msg ? t->latest_msg : "...", (unsigned long)t->count); purple_request_field_choice_add(fld, lbl); g_free(lbl); any = TRUE; }
    }
    purple_request_field_group_add_field(grp, fld);
    if (any) {
      ThreadDisplayContext *ctx = g_new0(ThreadDisplayContext, 1); ctx->room_id = g_strdup(item->room_id); g_hash_table_steal(pending_thread_lists, item->room_id); ctx->threads = list;
      purple_request_fields(find_matrix_account(), "Threads", "Active Threads", NULL, flds, "Open", G_CALLBACK(thread_list_action_cb), "Cancel", G_CALLBACK(free_display_context), find_matrix_account(), NULL, NULL, ctx);
    } else g_hash_table_remove(pending_thread_lists, item->room_id);
    free_thread_list_item(item); return FALSE;
  }
}

void thread_list_cb(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts) {
  ThreadListItem *item = g_new0(ThreadListItem, 1); item->room_id = g_strdup(room_id); if (thread_root_id) item->thread_root_id = g_strdup(thread_root_id); if (latest_msg) item->latest_msg = g_strdup(latest_msg); item->count = count; item->ts = ts;
  g_idle_add(process_thread_list_with_ui, item);
}

static GHashTable *pending_poll_lists = NULL;
static void free_poll_list_item(gpointer data) { PollListItem *i = (PollListItem *)data; g_free(i->room_id); g_free(i->event_id); g_free(i->sender); g_free(i->question); g_free(i->options_str); g_free(i); }
static void free_poll_list_glist(gpointer data) { g_list_free_full((GList *)data, free_poll_list_item); }
static void poll_vote_action_cb(void *user_data, PurpleRequestFields *fields) {
  PollVoteContext *ctx = (PollVoteContext *)user_data; int idx = purple_request_field_choice_get_value(purple_request_fields_get_field(fields, "option_idx"));
  char **opts = g_strsplit(ctx->options_str, "|", -1); int cnt = 0; while (opts[cnt]) cnt++;
  if (idx >= 0 && idx < cnt) { char *opt = opts[idx]; char *crt = strchr(opt, '^'); purple_matrix_rust_poll_vote(purple_account_get_username(find_matrix_account()), ctx->room_id, ctx->event_id, "ignored", crt ? opt : opt); }
  g_strfreev(opts); g_free(ctx->room_id); g_free(ctx->event_id); g_free(ctx->options_str); g_free(ctx);
}
static void show_vote_ui_cb(void *user_data, PurpleRequestFields *fields) {
  PollDisplayContext *ctx = (PollDisplayContext *)user_data; int idx = purple_request_field_choice_get_value(purple_request_fields_get_field(fields, "poll_id"));
  PollListItem *item = (PollListItem *)g_list_nth_data(ctx->polls, idx);
  if (item) {
    PurpleRequestFields *vf = purple_request_fields_new(); PurpleRequestFieldGroup *vg = purple_request_field_group_new(NULL); purple_request_fields_add_group(vf, vg);
    PurpleRequestField *vfl = purple_request_field_choice_new("option_idx", "Option", 0); char **opts = g_strsplit(item->options_str, "|", -1);
    for (int i = 0; opts[i]; i++) { char *crt = strchr(opts[i], '^'); purple_request_field_choice_add(vfl, crt ? crt + 1 : opts[i]); }
    g_strfreev(opts); purple_request_field_group_add_field(vg, vfl);
    PollVoteContext *vctx = g_new0(PollVoteContext, 1); vctx->room_id = g_strdup(item->room_id); vctx->event_id = g_strdup(item->event_id); vctx->options_str = g_strdup(item->options_str);
    purple_request_fields(find_matrix_account(), "Vote", item->question, NULL, vf, "Vote", G_CALLBACK(poll_vote_action_cb), "Cancel", G_CALLBACK(g_free), find_matrix_account(), NULL, NULL, vctx);
  }
  g_free(ctx->room_id); g_list_free_full(ctx->polls, free_poll_list_item); g_free(ctx);
}
static gboolean process_poll_list_with_ui(gpointer data) {
  PollListItem *item = (PollListItem *)data; if (!pending_poll_lists) pending_poll_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, free_poll_list_glist);
  GList *list = g_hash_table_lookup(pending_poll_lists, item->room_id);
  if (item->event_id) { list = g_list_append(list, item); g_hash_table_replace(pending_poll_lists, g_strdup(item->room_id), list); return FALSE; }
  else {
    PurpleRequestFields *f = purple_request_fields_new(); PurpleRequestFieldGroup *g = purple_request_field_group_new(NULL); purple_request_fields_add_group(f, g);
    PurpleRequestField *fl = purple_request_field_choice_new("poll_id", "Select poll", 0); gboolean any = FALSE;
    for (GList *l = list; l; l = l->next) { PollListItem *p = (PollListItem *)l->data; if (p->event_id) { purple_request_field_choice_add(fl, p->question); any = TRUE; } }
    purple_request_field_group_add_field(g, fl);
    if (any) {
      PollDisplayContext *ctx = g_new0(PollDisplayContext, 1); ctx->room_id = g_strdup(item->room_id); g_hash_table_steal(pending_poll_lists, item->room_id); ctx->polls = list;
      purple_request_fields(find_matrix_account(), "Polls", "Active Polls", NULL, f, "Select", G_CALLBACK(show_vote_ui_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, ctx);
    } else g_hash_table_remove(pending_poll_lists, item->room_id);
    free_poll_list_item(item); return FALSE;
  }
}
void poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str) {
  PollListItem *i = g_new0(PollListItem, 1);
  i->room_id = g_strdup(room_id);
  if (event_id)
    i->event_id = g_strdup(event_id);
  if (sender)
    i->sender = g_strdup(sender);
  if (question)
    i->question = g_strdup(question);
  if (options_str)
    i->options_str = g_strdup(options_str);
  g_idle_add(process_poll_list_with_ui, i);
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
      list = g_list_append(list, purple_menu_action_new("Help", PURPLE_CALLBACK(menu_action_buddy_list_help_cb), NULL, NULL));
    }
  } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    if (strcmp(purple_account_get_protocol_id(purple_buddy_get_account((PurpleBuddy *)node)), "prpl-matrix-rust") == 0) {
      list = g_list_append(list, purple_menu_action_new("Verify User", PURPLE_CALLBACK(menu_action_verify_buddy_cb), NULL, NULL));
      list = g_list_append(list, purple_menu_action_new("Send Sticker", PURPLE_CALLBACK(blist_action_send_sticker_cb), NULL, NULL));
    }
  }
  return list;
}

static PurpleCmdRet cmd_resync_history(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  PurpleAccount *acc = purple_conversation_get_account(conv);
  purple_matrix_rust_resync_recent_history(purple_account_get_username(acc), purple_conversation_get_name(conv));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_enable_backup(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data) {
  purple_matrix_rust_enable_key_backup(purple_account_get_username(purple_conversation_get_account(conv)));
  return PURPLE_CMD_RET_OK;
}

void register_matrix_commands(PurplePlugin *plugin) {
  purple_cmd_register("matrix_sync_history", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_resync_history, "matrix_sync_history: Reset and resync history", NULL);
  purple_cmd_register("matrix_enable_backup", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_enable_backup, "matrix_enable_backup: Check or enable online key backup", NULL);
  purple_cmd_register("matrix_thread", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread, "matrix_thread <msg>", NULL);
  purple_cmd_register("matrix_threads", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_list_threads, "matrix_threads: List threads", NULL);
  purple_cmd_register("matrix_leave", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_leave, "matrix_leave [reason]", NULL);
  purple_cmd_register("matrix_sticker", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_sticker, "matrix_sticker <url>", NULL);
  purple_cmd_register("matrix_nick", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_nick, "matrix_nick <name>", NULL);
  purple_cmd_register("matrix_avatar", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_avatar, "matrix_avatar <path>", NULL);
  purple_cmd_register("matrix_reply", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reply, "matrix_reply <msg>", NULL);
  purple_cmd_register("matrix_edit", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_edit, "matrix_edit <msg>", NULL);
  purple_cmd_register("matrix_mute", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_mute, "matrix_mute", NULL);
  purple_cmd_register("matrix_unmute", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_unmute, "matrix_unmute", NULL);
  purple_cmd_register("matrix_logout", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_logout, "matrix_logout", NULL);
  purple_cmd_register("matrix_reconnect", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reconnect, "matrix_reconnect", NULL);
  purple_cmd_register("matrix_help", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_help, "matrix_help", NULL);
  purple_cmd_register("matrix_location", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_location, "matrix_location <desc> <geo>", NULL);
  purple_cmd_register("matrix_poll_create", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll_create, "matrix_poll_create <q> <opts>", NULL);
  purple_cmd_register("matrix_poll_vote", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll_vote, "matrix_poll_vote <idx>", NULL);
  purple_cmd_register("matrix_poll_end", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_poll_end, "matrix_poll_end", NULL);
  purple_cmd_register("matrix_verify", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_verify, "matrix_verify <user_id>", NULL);
  purple_cmd_register("matrix_e2ee_status", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_e2ee_status, "matrix_e2ee_status", NULL);
  purple_cmd_register("matrix_reset_cross_signing", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_reset_cross_signing, "matrix_reset_cross_signing", NULL);
  purple_cmd_register("matrix_recover_keys", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_recover_keys, "matrix_recover_keys <pass>", NULL);
  purple_cmd_register("matrix_restore_backup", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_restore_backup, "matrix_restore_backup <key>", NULL);
  purple_cmd_register("matrix_read_receipt", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_read_receipt, "matrix_read_receipt", NULL);
  purple_cmd_register("matrix_react", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_react, "matrix_react <emoji>", NULL);
  purple_cmd_register("matrix_redact", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_redact, "matrix_redact [reason]", NULL);
  purple_cmd_register("matrix_history", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_history, "matrix_history", NULL);
  purple_cmd_register("matrix_preview_room", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_preview_room, "matrix_preview_room <id>", NULL);
  purple_cmd_register("matrix_set_history_page_size", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_history_page_size, "matrix_set_history_page_size <n>", NULL);
  purple_cmd_register("matrix_set_auto_fetch_history", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_auto_fetch_history, "matrix_set_auto_fetch_history <on|off>", NULL);
  purple_cmd_register("matrix_create_room", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_create_room, "matrix_create_room <name>", NULL);
  purple_cmd_register("matrix_public_rooms", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_public_rooms, "matrix_public_rooms [term]", NULL);
  purple_cmd_register("matrix_user_search", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_user_search, "matrix_user_search <term>", NULL);
  purple_cmd_register("matrix_kick", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_kick, "matrix_kick <user> [reason]", NULL);
  purple_cmd_register("matrix_ban", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_ban, "matrix_ban <user> [reason]", NULL);
  purple_cmd_register("matrix_invite", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_invite, "matrix_invite <user>", NULL);
  purple_cmd_register("matrix_tag", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_tag, "matrix_tag <tag>", NULL);
  purple_cmd_register("matrix_ignore", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_ignore, "matrix_ignore <user>", NULL);
  purple_cmd_register("matrix_power_levels", "", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_power_levels, "matrix_power_levels", NULL);
  purple_cmd_register("matrix_set_power_level", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_power_level, "matrix_set_power_level <user> <level>", NULL);
  purple_cmd_register("matrix_set_name", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_name, "matrix_set_name <name>", NULL);
  purple_cmd_register("matrix_set_topic", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_topic, "matrix_set_topic <topic>", NULL);
  purple_cmd_register("matrix_alias_create", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_alias_create, "matrix_alias_create <alias>", NULL);
  purple_cmd_register("matrix_alias_delete", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_alias_delete, "matrix_alias_delete <alias>", NULL);
  purple_cmd_register("matrix_report", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_report, "matrix_report <id> [reason]", NULL);
  purple_cmd_register("matrix_export_keys", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_export_keys, "matrix_export_keys <path> <pass>", NULL);
  purple_cmd_register("matrix_bulk_redact", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_bulk_redact, "matrix_bulk_redact <count> [reason]", NULL);
  purple_cmd_register("matrix_knock", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_knock, "matrix_knock <id> [reason]", NULL);
  purple_cmd_register("matrix_set_room_avatar", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_room_avatar, "matrix_set_room_avatar <path>", NULL);
  purple_cmd_register("matrix_set_separate_threads", "s", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_set_separate_threads, "matrix_set_separate_threads <on|off>", NULL);
  purple_cmd_register("matrix_unban", "ss", PURPLE_CMD_P_PLUGIN, PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_unban, "matrix_unban <user> [reason]", NULL);
}
