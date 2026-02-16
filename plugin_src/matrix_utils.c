#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_blist.h"
#include "matrix_ffi_wrappers.h"
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

typedef struct {
  char *room_id;
  char *thread_root_id;
  char *latest_msg;
  guint64 count;
} ThreadData;

static void notify_close_free_cb(gpointer user_data) {
  g_free(user_data);
}

static void thread_search_result_cb(PurpleConnection *gc, GList *row, gpointer user_data) {
  char *room_id = (char *)user_data;
  const char *root_id = g_list_nth_data(row, 2);
  if (root_id) {
    char *virtual_id = g_strdup_printf("%s|%s", room_id, root_id);
    ensure_thread_in_blist(purple_connection_get_account(gc), virtual_id, "Thread", room_id);
    g_free(virtual_id);
  }
}

static gboolean thread_list_ui_idle_cb(gpointer user_data) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (!thread_lists || !room_id) { if (room_id) g_free(room_id); return FALSE; }
  GList *list = g_hash_table_lookup(thread_lists, room_id);
  if (!list) {
    purple_notify_info(my_plugin, "Threads", "No threads found", "No active threads were found.");
    g_free(room_id); return FALSE;
  }
  PurpleNotifySearchResults *results = purple_notify_searchresults_new();
  purple_notify_searchresults_column_add(results, purple_notify_searchresults_column_new("Topic / Latest Message"));
  purple_notify_searchresults_column_add(results, purple_notify_searchresults_column_new("Replies"));
  purple_notify_searchresults_column_add(results, purple_notify_searchresults_column_new("ID"));
  purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_CONTINUE, thread_search_result_cb);
  for (GList *it = list; it; it = it->next) {
    MatrixThreadInfo *info = (MatrixThreadInfo *)it->data;
    if (info) {
      GList *row = NULL;
      row = g_list_append(row, g_strdup(info->description));
      row = g_list_append(row, g_strdup("N/A"));
      row = g_list_append(row, g_strdup(info->root_id));
      purple_notify_searchresults_row_add(results, row);
    }
  }
  purple_notify_searchresults(purple_account_get_connection(account), "Active Threads", "Room Threads", "Select a thread to open it.", results, notify_close_free_cb, g_strdup(room_id));
  g_hash_table_remove(thread_lists, room_id);
  g_free(room_id);
  return FALSE;
}

static gboolean process_thread_item_cb(gpointer user_data) {
  ThreadData *d = (ThreadData *)user_data;
  if (!d) return FALSE;
  if (!thread_lists) thread_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)free_thread_list);
  
  if (d->thread_root_id == NULL) {
    g_idle_add(thread_list_ui_idle_cb, g_strdup(d->room_id));
  } else if (d->room_id) {
    MatrixThreadInfo *info = g_new0(MatrixThreadInfo, 1);
    info->root_id = g_strdup(d->thread_root_id);
    info->description = g_strdup_printf("%s (%" G_GUINT64_FORMAT " msgs)", d->latest_msg ? d->latest_msg : "No text", d->count);
    GList *list = g_hash_table_lookup(thread_lists, d->room_id);
    list = g_list_append(list, info);
    g_hash_table_insert(thread_lists, g_strdup(d->room_id), list);
  }
  g_free(d->room_id); g_free(d->thread_root_id); g_free(d->latest_msg); g_free(d);
  return FALSE;
}

void thread_list_cb(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts) {
  ThreadData *d = g_new0(ThreadData, 1);
  d->room_id = g_strdup(room_id); d->thread_root_id = g_strdup(thread_root_id); d->latest_msg = g_strdup(latest_msg); d->count = count;
  g_idle_add(process_thread_item_cb, d);
}

static GHashTable *poll_lists = NULL;
typedef struct { char *event_id; char *question; char *sender; char *options_str; } MatrixPollInfo;
static void free_matrix_poll_info(MatrixPollInfo *info) {
  if (!info) return; g_free(info->event_id); g_free(info->question); g_free(info->sender); g_free(info->options_str); g_free(info);
}
static void free_poll_list(GList *list) { g_list_free_full(list, (GDestroyNotify)free_matrix_poll_info); }

static void submit_vote_cb(void *user_data, const char *index_str) {
  char *data_str = (char *)user_data; char *sep = strchr(data_str, ' ');
  if (sep && index_str && *index_str) {
    *sep = '\0'; char *event_id = data_str; char *room_id = sep + 1;
    PurpleAccount *account = find_matrix_account();
    purple_matrix_rust_poll_vote(purple_account_get_username(account), room_id, event_id, (guint64)atoll(index_str));
  }
  g_free(data_str);
}

static void vote_on_selected_poll_step2_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  PurpleRequestField *f = purple_request_fields_get_field(fields, "poll");
  GList *sel = purple_request_field_list_get_selected(f);
  if (sel) {
    const char *combined = (const char *)purple_request_field_list_get_data(f, (const char *)sel->data);
    char *dup = g_strdup(combined); char *sep = strchr(dup, '|');
    if (sep) {
      *sep = '\0'; char *event_id = dup; char *options_str = sep + 1;
      char *prompt = g_strdup_printf("Options:\n%s\n\nEnter the index (0 for first, 1 for second...)", options_str);
      char *next_data = g_strdup_printf("%s %s", event_id, room_id);
      purple_request_input(find_matrix_account(), "Cast Vote", "Select Option", prompt, "0", FALSE, FALSE, NULL, "_Vote", G_CALLBACK(submit_vote_cb), "_Cancel", NULL, find_matrix_account(), NULL, NULL, next_data);
      g_free(prompt);
    }
    g_free(dup);
  }
  g_free(room_id);
}

static gboolean poll_list_ui_idle_cb(gpointer user_data) {
  char *room_id = (char *)user_data;
  if (!poll_lists || !room_id) { if (room_id) g_free(room_id); return FALSE; }
  GList *list = g_hash_table_lookup(poll_lists, room_id);
  if (!list) { purple_notify_info(my_plugin, "Polls", "No active polls found", "No polls were found in recent history."); g_free(room_id); return FALSE; }
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new("Available Polls");
  purple_request_fields_add_group(fields, group);
  PurpleRequestField *f = purple_request_field_list_new("poll", "Choose Poll");
  purple_request_field_group_add_field(group, f);
  for (GList *it = list; it; it = it->next) {
    MatrixPollInfo *info = (MatrixPollInfo *)it->data;
    if (info) {
      char *label = g_strdup_printf("%s (by %s)", info->question ? info->question : "Question", info->sender ? info->sender : "Unknown");
      char *value = g_strdup_printf("%s|%s", info->event_id, info->options_str ? info->options_str : "");
      purple_request_field_list_add(f, label, value); g_free(label);
    }
  }
  purple_request_fields(find_matrix_account(), "Poll Voting Wizard", "Active Polls", "Select a poll to vote on and click Select.", fields, "_Select", G_CALLBACK(vote_on_selected_poll_step2_cb), "_Cancel", NULL, find_matrix_account(), NULL, NULL, g_strdup(room_id));
  g_hash_table_remove(poll_lists, room_id); g_free(room_id); return FALSE;
}

typedef struct {
  char *room_id;
  char *event_id;
  char *sender;
  char *question;
  char *options_str;
} PollData;

static gboolean process_poll_item_cb(gpointer user_data) {
  PollData *d = (PollData *)user_data;
  if (!d) return FALSE;
  if (!poll_lists) poll_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)free_poll_list);
  
  if (d->event_id == NULL) {
    g_idle_add(poll_list_ui_idle_cb, g_strdup(d->room_id));
  } else if (d->room_id) {
    MatrixPollInfo *info = g_new0(MatrixPollInfo, 1);
    info->event_id = g_strdup(d->event_id);
    info->sender = g_strdup(d->sender);
    info->question = g_strdup(d->question);
    info->options_str = g_strdup(d->options_str);
    GList *list = g_hash_table_lookup(poll_lists, d->room_id);
    list = g_list_append(list, info);
    g_hash_table_insert(poll_lists, g_strdup(d->room_id), list);
  }
  g_free(d->room_id); g_free(d->event_id); g_free(d->sender); g_free(d->question); g_free(d->options_str); g_free(d);
  return FALSE;
}

void poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str) {
  PollData *d = g_new0(PollData, 1);
  d->room_id = g_strdup(room_id); d->event_id = g_strdup(event_id); d->sender = g_strdup(sender); d->question = g_strdup(question); d->options_str = g_strdup(options_str);
  g_idle_add(process_poll_item_cb, d);
}

static GHashTable *search_results_map = NULL;
typedef struct { char *sender; char *message; char *timestamp_str; } MatrixSearchResultInfo;
static void free_search_result_info(MatrixSearchResultInfo *info) {
  if (!info) return; g_free(info->sender); g_free(info->message); g_free(info->timestamp_str); g_free(info);
}
static void free_search_list(GList *list) { g_list_free_full(list, (GDestroyNotify)free_search_result_info); }

typedef struct { char *room_id; char *term; } SearchUIData;

static gboolean search_ui_idle_cb(gpointer user_data) {
  SearchUIData *ui_data = (SearchUIData *)user_data;
  PurpleAccount *account = find_matrix_account();
  if (!search_results_map) return FALSE;
  GList *list = g_hash_table_lookup(search_results_map, ui_data->room_id);
  if (!list) {
    purple_notify_info(my_plugin, "Search Results", "No matches found", "No messages matched your search term.");
    g_free(ui_data->room_id); g_free(ui_data->term); g_free(ui_data); return FALSE;
  }
  PurpleNotifySearchResults *results = purple_notify_searchresults_new();
  purple_notify_searchresults_column_add(results, purple_notify_searchresults_column_new("Date"));
  purple_notify_searchresults_column_add(results, purple_notify_searchresults_column_new("Sender"));
  purple_notify_searchresults_column_add(results, purple_notify_searchresults_column_new("Message"));
  for (GList *it = list; it; it = it->next) {
    MatrixSearchResultInfo *info = (MatrixSearchResultInfo *)it->data;
    GList *row = NULL;
    row = g_list_append(row, g_strdup(info->timestamp_str));
    row = g_list_append(row, g_strdup(info->sender));
    row = g_list_append(row, g_strdup(info->message));
    purple_notify_searchresults_row_add(results, row);
  }
  char *title = g_strdup_printf("Search: %s", ui_data->term);
  purple_notify_searchresults(purple_account_get_connection(account), title, "Search Results", "Matches found in history:", results, notify_close_free_cb, NULL);
  g_free(title); g_hash_table_remove(search_results_map, ui_data->room_id);
  g_free(ui_data->room_id); g_free(ui_data->term); g_free(ui_data); return FALSE;
}

typedef struct {
  char *room_id;
  char *sender;
  char *message;
  char *timestamp_str;
} SearchData;

static gboolean process_search_item_cb(gpointer user_data) {
  SearchData *d = (SearchData *)user_data;
  if (!search_results_map) search_results_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)free_search_list);
  if (d->sender == NULL) {
    g_idle_add(search_ui_idle_cb, g_strdup(d->room_id));
  } else {
    MatrixSearchResultInfo *info = g_new0(MatrixSearchResultInfo, 1);
    info->sender = g_strdup(d->sender); info->message = g_strdup(d->message); info->timestamp_str = g_strdup(d->timestamp_str);
    GList *list = g_hash_table_lookup(search_results_map, d->room_id);
    list = g_list_append(list, info);
    g_hash_table_insert(search_results_map, g_strdup(d->room_id), list);
  }
  g_free(d->room_id); g_free(d->sender); g_free(d->message); g_free(d->timestamp_str); g_free(d);
  return FALSE;
}

void search_result_cb(const char *room_id, const char *sender, const char *message, const char *timestamp_str) {
  SearchData *d = g_new0(SearchData, 1);
  d->room_id = g_strdup(room_id); d->sender = g_strdup(sender); d->message = g_strdup(message); d->timestamp_str = g_strdup(timestamp_str);
  g_idle_add(process_search_item_cb, d);
}

guint32 get_history_page_size(PurpleAccount *account) {
  const char *raw = purple_account_get_string(account, "history_page_size", "50");
  long n = raw ? strtol(raw, NULL, 10) : 50;
  if (n < 1) n = 1; if (n > 500) n = 500;
  return (guint32)n;
}
