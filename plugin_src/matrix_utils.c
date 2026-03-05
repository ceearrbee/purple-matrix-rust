#include "matrix_utils.h"
#include "matrix_blist.h"
#include "matrix_commands.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include <libpurple/accountopt.h>
#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/prpl.h>
#include <libpurple/request.h>
#include <libpurple/util.h>
#include <libpurple/version.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GHashTable *thread_lists = NULL;
static GMutex thread_lists_mutex;

typedef struct {
  char *root_id;
  char *description;
  char *timestamp_str;
  char *alias;
  guint64 ts;
} MatrixThreadInfo;

static gint sort_threads_by_ts(gconstpointer a, gconstpointer b) {
  MatrixThreadInfo *ta = (MatrixThreadInfo *)a;
  MatrixThreadInfo *tb = (MatrixThreadInfo *)b;
  if (ta->ts > tb->ts)
    return -1;
  if (ta->ts < tb->ts)
    return 1;
  return 0;
}

static void free_matrix_thread_info(MatrixThreadInfo *info) {
  if (!info)
    return;
  if (info->root_id) {
    g_free(info->root_id);
    info->root_id = NULL;
  }
  if (info->description) {
    g_free(info->description);
    info->description = NULL;
  }
  if (info->timestamp_str) {
    g_free(info->timestamp_str);
    info->timestamp_str = NULL;
  }
  if (info->alias) {
    g_free(info->alias);
    info->alias = NULL;
  }
  g_free(info);
}

static void free_thread_list(GList *list) {
  g_list_free_full(list, (GDestroyNotify)free_matrix_thread_info);
}

typedef struct {
  char *user_id;
  char *room_id;
  char *thread_root_id;
  char *latest_msg;
  guint64 count;
  guint64 ts;
} ThreadData;

static void thread_search_result_cb(PurpleConnection *gc, GList *row,
                                    gpointer user_data) {
  char *room_id = (char *)user_data;
  if (!row)
    return;
  /* Columns: 0:Alias/Latest, 1:Replies, 2:Last Mention, 3:Root ID */
  const char *alias = g_list_nth_data(row, 0);
  const char *root_id = g_list_nth_data(row, 3);
  if (root_id) {
    char *virtual_id = g_strdup_printf("%s|%s", room_id, root_id);
    ensure_thread_in_blist(purple_connection_get_account(gc), virtual_id, alias,
                           room_id);
    g_free(virtual_id);
  }
}

static void notify_close_free_cb(gpointer user_data) {
  if (user_data)
    g_free(user_data);
}

static gboolean thread_list_ui_idle_cb(gpointer data) {
  ThreadData *d = (ThreadData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);

  g_mutex_lock(&thread_lists_mutex);
  if (!thread_lists || !d->room_id || !account) {
    g_mutex_unlock(&thread_lists_mutex);
    if (d->room_id)
      g_free(d->room_id);
    if (d->user_id)
      g_free(d->user_id);
    g_free(d);
    return FALSE;
  }

  GList *list = g_hash_table_lookup(thread_lists, d->room_id);
  if (list)
    g_hash_table_steal(thread_lists, d->room_id);
  g_mutex_unlock(&thread_lists_mutex);

  if (!list) {
    purple_notify_info(
        my_plugin, "Thread Discovery", "No threads found",
        "No active threads were found in the recent history of this room.");
    g_free(d->user_id);
    g_free(d->room_id);
    g_free(d);
    return FALSE;
  }

  PurpleNotifySearchResults *results = purple_notify_searchresults_new();
  purple_notify_searchresults_column_add(
      results,
      purple_notify_searchresults_column_new("Thread Alias / Latest Message"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Replies"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Last Mention"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Root Event ID"));

  purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_CONTINUE,
                                         thread_search_result_cb);

  list = g_list_sort(list, sort_threads_by_ts);

  for (GList *it = list; it; it = it->next) {
    MatrixThreadInfo *info = (MatrixThreadInfo *)it->data;
    if (info) {
      GList *row = NULL;
      row = g_list_append(row, g_strdup(info->alias));
      row = g_list_append(row, g_strdup(info->description));
      row = g_list_append(row, g_strdup(info->timestamp_str));
      row = g_list_append(row, g_strdup(info->root_id));
      purple_notify_searchresults_row_add(results, row);
    }
  }

  purple_notify_searchresults(purple_account_get_connection(account),
                              "Active Room Threads", "Thread Selection Wizard",
                              "Please select a thread and click 'Forward' to "
                              "open it in a new conversation tab.",
                              results, notify_close_free_cb,
                              g_strdup(d->room_id));

  g_list_free_full(list, (GDestroyNotify)free_matrix_thread_info);
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d);
  return FALSE;
}

static gboolean process_thread_item_cb(gpointer user_data) {
  ThreadData *d = (ThreadData *)user_data;
  if (!d)
    return FALSE;

  g_mutex_lock(&thread_lists_mutex);
  if (!thread_lists)
    thread_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)free_thread_list);

  if (d->thread_root_id == NULL) {
    g_mutex_unlock(&thread_lists_mutex);
    g_idle_add(thread_list_ui_idle_cb, d);
    return FALSE;
  } else if (d->room_id) {
    MatrixThreadInfo *info = g_new0(MatrixThreadInfo, 1);
    info->root_id = g_strdup(d->thread_root_id);
    info->description =
        g_strdup_printf("%" G_GUINT64_FORMAT " messages", d->count);
    info->alias = g_strdup(d->latest_msg ? d->latest_msg : "No text");
    info->ts = d->ts;

    if (d->ts > 0) {
      time_t raw_ts = (time_t)(d->ts / 1000);
      struct tm *tm_info = localtime(&raw_ts);
      char cts[128];
      if (tm_info) {
        strftime(cts, sizeof(cts), "%Y-%m-%d %H:%M:%S", tm_info);
        info->timestamp_str = g_strdup(cts);
      } else
        info->timestamp_str = g_strdup("Invalid Date");
    } else
      info->timestamp_str = g_strdup("Unknown");

    GList *list = g_hash_table_lookup(thread_lists, d->room_id);
    if (list) {
      list = g_list_append(list, info);
    } else {
      list = g_list_append(NULL, info);
      g_hash_table_insert(thread_lists, g_strdup(d->room_id), list);
    }
  }
  g_mutex_unlock(&thread_lists_mutex);

  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->thread_root_id);
  g_free(d->latest_msg);
  g_free(d);
  return FALSE;
}

void thread_list_cb(const char *user_id, const char *room_id,
                    const char *thread_root_id, const char *latest_msg,
                    guint64 count, guint64 ts) {
  ThreadData *d = g_new0(ThreadData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->thread_root_id = g_strdup(thread_root_id);
  d->latest_msg = g_strdup(latest_msg);
  d->count = count;
  d->ts = ts;
  g_idle_add(process_thread_item_cb, d);
}

static GHashTable *poll_lists = NULL;
static GMutex poll_lists_mutex;

typedef struct {
  char *event_id;
  char *question;
  char *sender;
  char *options_str;
} MatrixPollInfo;
static void free_matrix_poll_info(MatrixPollInfo *info) {
  if (!info)
    return;
  if (info->event_id) {
    g_free(info->event_id);
    info->event_id = NULL;
  }
  if (info->question) {
    g_free(info->question);
    info->question = NULL;
  }
  if (info->sender) {
    g_free(info->sender);
    info->sender = NULL;
  }
  if (info->options_str) {
    g_free(info->options_str);
    info->options_str = NULL;
  }
  g_free(info);
}
static void free_poll_list(GList *list) {
  g_list_free_full(list, (GDestroyNotify)free_matrix_poll_info);
}

static void submit_vote_cb(void *user_data, const char *index_str) {
  char *data_str = (char *)user_data;
  char *sep = strchr(data_str, ' ');
  if (sep && index_str && *index_str) {
    *sep = '\0';
    char *event_id = data_str;
    char *user_id_room_id = sep + 1;
    char *sep2 = strchr(user_id_room_id, ' ');
    if (sep2) {
      *sep2 = '\0';
      char *user_id = user_id_room_id;
      char *room_id = sep2 + 1;
      purple_matrix_rust_vote_poll(user_id, room_id, event_id,
                                   (int)atoi(index_str));
    }
  }
  g_free(data_str);
}

typedef struct {
  char *user_id;
  char *room_id;
  char *event_id;
  char *question;
  char *sender;
  char *options_str;
} PollData;

static void poll_search_result_cb(PurpleConnection *gc, GList *row,
                                  gpointer user_data) {
  char *user_id_room_id = (char *)user_data;
  if (!row)
    return;
  const char *event_id = g_list_nth_data(row, 2);
  const char *options = g_list_nth_data(row, 3);
  if (event_id && options) {
    char *data_str = g_strdup_printf("%s %s", event_id, user_id_room_id);
    purple_request_input(
        gc, "Vote in Poll", "Cast Your Vote",
        "Enter the index (0, 1, 2...) of your choice:", NULL, FALSE, FALSE,
        NULL, "_Vote", G_CALLBACK(submit_vote_cb), "_Cancel", NULL,
        purple_connection_get_account(gc), NULL, NULL, data_str);
  }
}

static gboolean poll_list_ui_idle_cb(gpointer data) {
  PollData *d = (PollData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);

  g_mutex_lock(&poll_lists_mutex);
  if (!poll_lists || !d->room_id || !account) {
    g_mutex_unlock(&poll_lists_mutex);
    if (d->room_id)
      g_free(d->room_id);
    if (d->user_id)
      g_free(d->user_id);
    g_free(d);
    return FALSE;
  }

  GList *list = g_hash_table_lookup(poll_lists, d->room_id);
  if (list)
    g_hash_table_steal(poll_lists, d->room_id);
  g_mutex_unlock(&poll_lists_mutex);

  if (!list) {
    purple_notify_info(
        my_plugin, "Poll Discovery", "No polls found",
        "No active polls were found in the recent history of this room.");
    g_free(d->user_id);
    g_free(d->room_id);
    g_free(d);
    return FALSE;
  }

  PurpleNotifySearchResults *results = purple_notify_searchresults_new();
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Question"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Creator"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Event ID"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Options"));

  char *user_id_room_id = g_strdup_printf("%s %s", d->user_id, d->room_id);
  purple_notify_searchresults_button_add(results, PURPLE_NOTIFY_BUTTON_CONTINUE,
                                         poll_search_result_cb);

  for (GList *it = list; it; it = it->next) {
    MatrixPollInfo *info = (MatrixPollInfo *)it->data;
    if (info) {
      GList *row = NULL;
      row = g_list_append(row, g_strdup(info->question));
      row = g_list_append(row, g_strdup(info->sender));
      row = g_list_append(row, g_strdup(info->event_id));
      row = g_list_append(row, g_strdup(info->options_str));
      purple_notify_searchresults_row_add(results, row);
    }
  }

  purple_notify_searchresults(
      purple_account_get_connection(account), "Active Room Polls",
      "Poll Selection Wizard",
      "Please select a poll and click 'Forward' to cast your vote.", results,
      notify_close_free_cb, user_id_room_id);

  g_list_free_full(list, (GDestroyNotify)free_matrix_poll_info);
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d);
  return FALSE;
}

static gboolean process_poll_item_cb(gpointer user_data) {
  PollData *d = (PollData *)user_data;
  if (!d)
    return FALSE;

  g_mutex_lock(&poll_lists_mutex);
  if (!poll_lists)
    poll_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                       (GDestroyNotify)free_poll_list);

  if (d->event_id == NULL) {
    g_mutex_unlock(&poll_lists_mutex);
    g_idle_add(poll_list_ui_idle_cb, d);
    return FALSE;
  } else if (d->room_id) {
    MatrixPollInfo *info = g_new0(MatrixPollInfo, 1);
    info->event_id = g_strdup(d->event_id);
    info->question = g_strdup(d->question);
    info->sender = g_strdup(d->sender);
    info->options_str = g_strdup(d->options_str);

    GList *list = g_hash_table_lookup(poll_lists, d->room_id);
    if (list) {
      list = g_list_append(list, info);
    } else {
      list = g_list_append(NULL, info);
      g_hash_table_insert(poll_lists, g_strdup(d->room_id), list);
    }
  }
  g_mutex_unlock(&poll_lists_mutex);

  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->event_id);
  g_free(d->question);
  g_free(d->sender);
  g_free(d->options_str);
  g_free(d);
  return FALSE;
}

void poll_list_cb(const char *user_id, const char *room_id,
                  const char *event_id, const char *question,
                  const char *sender, const char *options_str) {
  PollData *d = g_new0(PollData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->event_id = g_strdup(event_id);
  d->question = g_strdup(question);
  d->sender = g_strdup(sender);
  d->options_str = g_strdup(options_str);
  g_idle_add(process_poll_item_cb, d);
}

static GHashTable *search_results_map = NULL;
static GMutex search_results_mutex;

typedef struct {
  char *sender;
  char *message;
  char *timestamp_str;
} MatrixSearchResultInfo;
static void free_search_result_info(MatrixSearchResultInfo *info) {
  if (!info)
    return;
  if (info->sender) {
    g_free(info->sender);
    info->sender = NULL;
  }
  if (info->message) {
    g_free(info->message);
    info->message = NULL;
  }
  if (info->timestamp_str) {
    g_free(info->timestamp_str);
    info->timestamp_str = NULL;
  }
  g_free(info);
}
static void free_search_list(GList *list) {
  g_list_free_full(list, (GDestroyNotify)free_search_result_info);
}

typedef struct {
  char *user_id;
  char *room_id;
  char *sender;
  char *message;
  char *timestamp_str;
} SearchData;

static gboolean search_ui_idle_cb(gpointer data) {
  SearchData *d = (SearchData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);

  g_mutex_lock(&search_results_mutex);
  if (!search_results_map || !d->room_id || !account) {
    g_mutex_unlock(&search_results_mutex);
    if (d->room_id)
      g_free(d->room_id);
    if (d->user_id)
      g_free(d->user_id);
    g_free(d);
    return FALSE;
  }

  GList *list = g_hash_table_lookup(search_results_map, d->room_id);
  if (list)
    g_hash_table_steal(search_results_map, d->room_id);
  g_mutex_unlock(&search_results_mutex);

  if (!list) {
    purple_notify_info(my_plugin, "Message Search", "No results found",
                       "No messages matched your search term in this room.");
    g_free(d->user_id);
    g_free(d->room_id);
    g_free(d);
    return FALSE;
  }

  PurpleNotifySearchResults *results = purple_notify_searchresults_new();
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Date"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Sender"));
  purple_notify_searchresults_column_add(
      results, purple_notify_searchresults_column_new("Message Content"));
  for (GList *it = list; it; it = it->next) {
    MatrixSearchResultInfo *info = (MatrixSearchResultInfo *)it->data;
    if (info) {
      GList *row = NULL;
      row = g_list_append(row, g_strdup(info->timestamp_str));
      row = g_list_append(row, g_strdup(info->sender));
      row = g_list_append(row, g_strdup(info->message));
      purple_notify_searchresults_row_add(results, row);
    }
  }
  purple_notify_searchresults(
      purple_account_get_connection(account), "Search Results",
      "History Search",
      "The following messages were found in the room history.", results,
      notify_close_free_cb, NULL);

  g_list_free_full(list, (GDestroyNotify)free_search_result_info);
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d);
  return FALSE;
}

static gboolean process_search_item_cb(gpointer user_data) {
  SearchData *d = (SearchData *)user_data;
  if (!d)
    return FALSE;

  g_mutex_lock(&search_results_mutex);
  if (!search_results_map)
    search_results_map = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, (GDestroyNotify)free_search_list);
  if (d->sender == NULL) {
    g_mutex_unlock(&search_results_mutex);
    g_idle_add(search_ui_idle_cb, d);
    return FALSE;
  } else if (d->room_id) {
    MatrixSearchResultInfo *info = g_new0(MatrixSearchResultInfo, 1);
    info->sender = g_strdup(d->sender);
    info->message = g_strdup(d->message);
    info->timestamp_str = g_strdup(d->timestamp_str);

    GList *list = g_hash_table_lookup(search_results_map, d->room_id);
    if (list) {
      list = g_list_append(list, info);
    } else {
      list = g_list_append(NULL, info);
      g_hash_table_insert(search_results_map, g_strdup(d->room_id), list);
    }
  }
  g_mutex_unlock(&search_results_mutex);
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->sender);
  g_free(d->message);
  g_free(d->timestamp_str);
  g_free(d);
  return FALSE;
}

void search_result_cb(const char *user_id, const char *room_id,
                      const char *sender, const char *message,
                      const char *timestamp_str) {
  SearchData *d = g_new0(SearchData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->sender = g_strdup(sender);
  d->message = g_strdup(message);
  d->timestamp_str = g_strdup(timestamp_str);
  g_idle_add(process_search_item_cb, d);
}

guint32 get_history_page_size(PurpleAccount *account) {
  if (!account)
    return 50;
  const char *raw =
      purple_account_get_string(account, "history_page_size", "50");
  long n = raw ? strtol(raw, NULL, 10) : 50;
  if (n < 1)
    n = 1;
  if (n > 500)
    n = 500;
  return (guint32)n;
}

PurpleAccount *find_matrix_account_by_id(const char *user_id) {
  if (!user_id || strlen(user_id) == 0)
    return NULL;
  
  purple_debug_info("matrix-ffi", "find_matrix_account_by_id: Looking for %s\n", user_id);
  
  GList *l;
  for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
    PurpleAccount *account = (PurpleAccount *)l->data;
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") ==
        0) {
      const char *username = purple_account_get_username(account);
      if (g_strcmp0(username, user_id) == 0) {
        purple_debug_info("matrix-ffi", "find_matrix_account_by_id: Exact match for %s\n", user_id);
        return account;
      }
      
      // Robust MXID matching: extract localpart if needed
      if (user_id[0] == '@' && username) {
        char *mxid_local = g_strdup(user_id + 1);
        char *sep = strchr(mxid_local, ':');
        if (sep)
          *sep = '\0';
        
        if (g_strcmp0(username, mxid_local) == 0) {
          purple_debug_info("matrix-ffi", "find_matrix_account_by_id: Localpart match: %s matched %s\n", username, user_id);
          g_free(mxid_local);
          return account;
        }
        g_free(mxid_local);
      }
    }
  }
  
  purple_debug_warning("matrix-ffi", "find_matrix_account_by_id: No match found for %s\n", user_id);
  return NULL;
}

PurpleAccount *find_matrix_account(void) {
  GList *accounts = purple_accounts_get_all();
  for (GList *l = accounts; l != NULL; l = l->next) {
    PurpleAccount *account = (PurpleAccount *)l->data;
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") ==
        0) {
      if (purple_account_is_connected(account))
        return account;
    }
  }
  // Diagnostic fallback
  for (GList *l = accounts; l != NULL; l = l->next) {
    PurpleAccount *account = (PurpleAccount *)l->data;
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") ==
        0)
      return account;
  }
  return NULL;
}

int get_chat_id(const char *room_id) {
  if (!room_id)
    return 0;
  
  /* room_id_map is initialized in matrix_core.c:plugin_load */
  if (!room_id_map) {
      purple_debug_warning("matrix", "get_chat_id called before room_id_map initialization\n");
      return 0; 
  }

  gpointer val = g_hash_table_lookup(room_id_map, room_id);
  if (val)
    return GPOINTER_TO_INT(val);
  static int next_id = 1;
  int id = next_id++;
  g_hash_table_insert(room_id_map, g_strdup(room_id), GINT_TO_POINTER(id));
  return id;
}

gboolean is_virtual_room_id(const char *room_id) {
  return (room_id && strchr(room_id, '|'));
}

char *dup_base_room_id(const char *room_id) {
  if (!room_id)
    return NULL;
  char *sep = strchr(room_id, '|');
  if (sep)
    return g_strndup(room_id, sep - room_id);
  return g_strdup(room_id);
}

char *derive_base_group_from_threads_group(const char *group_name) {
  if (!group_name)
    return g_strdup("Matrix Rooms");
  char *sep = strstr(group_name, " / ");
  if (sep)
    return g_strndup(group_name, sep - group_name);
  return g_strdup(group_name);
}

char *sanitize_markup_text(const char *input) {
  if (!input)
    return g_strdup("");
  char *tmp = purple_markup_strip_html(input);
  char *ret = g_strdup(tmp ? tmp : "");
  g_free(tmp);
  return ret;
}

char *strip_emojis(const char *input) {
  if (!input)
    return g_strdup("");
  return g_strdup(input);
}

void matrix_request_field_set_help_string(PurpleRequestField *field,
                                          const char *hint) {}

char *matrix_get_chat_name(GHashTable *components) {
  if (!components)
    return NULL;
  const char *room_id = g_hash_table_lookup(components, "room_id");
  return room_id ? g_strdup(room_id) : NULL;
}

void matrix_utils_cleanup(void) {
  g_mutex_lock(&thread_lists_mutex);
  if (thread_lists) {
    g_hash_table_destroy(thread_lists);
    thread_lists = NULL;
  }
  g_mutex_unlock(&thread_lists_mutex);

  g_mutex_lock(&poll_lists_mutex);
  if (poll_lists) {
    g_hash_table_destroy(poll_lists);
    poll_lists = NULL;
  }
  g_mutex_unlock(&poll_lists_mutex);

  g_mutex_lock(&search_results_mutex);
  if (search_results_map) {
    g_hash_table_destroy(search_results_map);
    search_results_map = NULL;
  }
  g_mutex_unlock(&search_results_mutex);
}

void matrix_ui_refresh_room_chips(PurpleConversation *conv) {
  if (!conv || purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_CHAT)
    return;

  const char *real_topic =
      purple_conversation_get_data(conv, "matrix_real_topic");
  const char *is_enc =
      purple_conversation_get_data(conv, "matrix_room_encrypted");
  const char *is_muted =
      purple_conversation_get_data(conv, "matrix_room_muted");
  const char *member_count =
      purple_conversation_get_data(conv, "matrix_room_member_count");

  GString *topic_buf = g_string_new("");

  if (is_enc && strcmp(is_enc, "1") == 0)
    g_string_append(topic_buf, "[Encrypted 🔒] ");
  if (is_muted && strcmp(is_muted, "1") == 0)
    g_string_append(topic_buf, "[Muted 🔕] ");
  if (member_count && *member_count)
    g_string_append_printf(topic_buf, "[Members: %s 👥] ", member_count);

  if (topic_buf->len > 0)
    g_string_append(topic_buf, "| ");

  if (real_topic && *real_topic)
    g_string_append(topic_buf, real_topic);
  else
    g_string_append(topic_buf, "(No topic)");

  purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), "System", topic_buf->str);
  g_string_free(topic_buf, TRUE);
}
