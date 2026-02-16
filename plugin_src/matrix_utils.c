#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_blist.h"
#include <libpurple/util.h>
#include <libpurple/accountopt.h>
#include <libpurple/prpl.h>
#include <libpurple/version.h>
#include <string.h>
#include <stdlib.h>

static int next_chat_id = 1;

int get_chat_id(const char *room_id) {
  if (!room_id_map) {
    room_id_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  }
  gpointer val = g_hash_table_lookup(room_id_map, room_id);
  if (val) return GPOINTER_TO_INT(val);
  int new_id = next_chat_id++;
  g_hash_table_insert(room_id_map, g_strdup(room_id), GINT_TO_POINTER(new_id));
  return new_id;
}

gboolean is_virtual_room_id(const char *room_id) {
  return room_id && strchr(room_id, '|') != NULL;
}

char *dup_base_room_id(const char *room_id) {
  if (!room_id) return NULL;
  const char *sep = strchr(room_id, '|');
  if (!sep) return g_strdup(room_id);
  return g_strndup(room_id, (gsize)(sep - room_id));
}

char *derive_base_group_from_threads_group(const char *group_name) {
  if (!group_name || !*group_name) return g_strdup("Matrix Rooms");
  const char *threads = strstr(group_name, " / Threads");
  if (!threads) return g_strdup(group_name);
  char *base = g_strndup(group_name, (gsize)(threads - group_name));
  char *last_sep = g_strrstr(base, " / ");
  if (last_sep) *last_sep = '\0';
  if (!*base) { g_free(base); return g_strdup("Matrix Rooms"); }
  return base;
}

char *sanitize_markup_text(const char *input) {
  if (!input) return NULL;
  GString *out = g_string_new("");
  gboolean in_tag = FALSE;
  for (const char *p = input; *p; p++) {
    if (*p == '<') in_tag = TRUE;
    else if (*p == '>') in_tag = FALSE;
    else if (!in_tag) g_string_append_c(out, *p);
  }
  return g_string_free(out, FALSE);
}

void matrix_request_field_set_help_string(PurpleRequestField *field, const char *hint) {
  (void)field; (void)hint;
}

PurpleAccount *find_matrix_account(void) {
  GList *accts = purple_accounts_get_all();
  while (accts) {
    PurpleAccount *a = (PurpleAccount *)accts->data;
    if (a && purple_account_get_protocol_id(a) && strcmp(purple_account_get_protocol_id(a), "prpl-matrix-rust") == 0) return a;
    accts = accts->next;
  }
  return NULL;
}

char *matrix_get_chat_name(GHashTable *components) {
  if (!components) return NULL;
  const char *room_id = g_hash_table_lookup(components, "room_id");
  return room_id ? g_strdup(room_id) : NULL;
}

static GHashTable *thread_lists = NULL;

typedef struct {
  char *root_id;
  char *description;
} MatrixThreadInfo;

static void free_matrix_thread_info(MatrixThreadInfo *info) {
  if (!info) return;
  g_free(info->root_id);
  g_free(info->description);
  g_free(info);
}

static void free_thread_list(GList *list) {
  g_list_free_full(list, (GDestroyNotify)free_matrix_thread_info);
}

static void open_selected_thread_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleRequestField *f = purple_request_fields_get_field(fields, "thread");
  GList *sel = purple_request_field_list_get_selected(f);
  if (sel) {
    const char *root_id = (const char *)purple_request_field_list_get_data(f, (const char *)sel->data);
    char *virtual_id = g_strdup_printf("%s|%s", room_id, root_id);
    PurpleAccount *account = find_matrix_account();
    ensure_thread_in_blist(account, virtual_id, "Thread", room_id);
    g_free(virtual_id);
  }
  g_free(room_id);
}

typedef struct {
  char *room_id;
} ThreadListUIData;

static gboolean thread_list_ui_idle_cb(gpointer user_data) {
  ThreadListUIData *ui_data = (ThreadListUIData *)user_data;
  char *room_id = ui_data->room_id;
  
  GList *list = g_hash_table_lookup(thread_lists, room_id);
  if (!list) {
    purple_notify_info(my_plugin, "Threads", "No threads found", "Try fetching more history first.");
    g_free(room_id);
    g_free(ui_data);
    return FALSE;
  }
  
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *f = purple_request_field_list_new("thread", "Choose Thread");
  purple_request_field_group_add_field(group, f);
  
  for (GList *it = list; it; it = it->next) {
    MatrixThreadInfo *info = (MatrixThreadInfo *)it->data;
    purple_request_field_list_add(f, info->description, info->root_id);
  }
  
  purple_request_fields(find_matrix_account(), "Room Threads", "Active Threads", "Select a thread to open it in a new tab.", fields, "Open", G_CALLBACK(open_selected_thread_cb), "Cancel", NULL, find_matrix_account(), NULL, NULL, g_strdup(room_id));
  
  g_hash_table_remove(thread_lists, room_id);
  g_free(room_id);
  g_free(ui_data);
  return FALSE;
}

void thread_list_cb(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts) {
  if (!thread_lists) thread_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)free_thread_list);
  
  if (thread_root_id == NULL) {
    /* End of list - schedule UI on main thread */
    ThreadListUIData *ui_data = g_new0(ThreadListUIData, 1);
    ui_data->room_id = g_strdup(room_id);
    g_idle_add(thread_list_ui_idle_cb, ui_data);
    return;
  }

  /* Accumulate */
  MatrixThreadInfo *info = g_new0(MatrixThreadInfo, 1);
  info->root_id = g_strdup(thread_root_id);
  info->description = g_strdup_printf("%s (%" G_GUINT64_FORMAT " msgs)", latest_msg, count);
  
  GList *list = g_hash_table_lookup(thread_lists, room_id);
  list = g_list_append(list, info);
  g_hash_table_insert(thread_lists, g_strdup(room_id), list);
}

typedef struct {
  char *room_id;
  char *event_id;
  char *sender;
  char *question;
  char *options_str;
} PollUIData;

static gboolean poll_ui_idle_cb(gpointer user_data) {
  PollUIData *ui_data = (PollUIData *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, ui_data->room_id, account);
  
  if (conv) {
    char *msg;
    if (ui_data->event_id) {
      msg = g_strdup_printf("<b>Active Poll:</b> %s (by %s)<br/><b>Options:</b> %s<br/><b>ID:</b> %s", ui_data->question, ui_data->sender, ui_data->options_str, ui_data->event_id);
    } else {
      msg = g_strdup("[End of Active Polls List]");
    }
    purple_conversation_write(conv, "Matrix", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
    g_free(msg);
  }
  
  g_free(ui_data->room_id);
  g_free(ui_data->event_id);
  g_free(ui_data->sender);
  g_free(ui_data->question);
  g_free(ui_data->options_str);
  g_free(ui_data);
  return FALSE;
}

void poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str) {
  PollUIData *ui_data = g_new0(PollUIData, 1);
  ui_data->room_id = g_strdup(room_id);
  ui_data->event_id = g_strdup(event_id);
  ui_data->sender = g_strdup(sender);
  ui_data->question = g_strdup(question);
  ui_data->options_str = g_strdup(options_str);
  g_idle_add(poll_ui_idle_cb, ui_data);
}

guint32 get_history_page_size(PurpleAccount *account) {

  const char *raw = purple_account_get_string(account, "history_page_size", "50");

  long n = raw ? strtol(raw, NULL, 10) : 50;

  if (n < 1)

    n = 1;

  if (n > 500)

    n = 500;

  return (guint32)n;

}
