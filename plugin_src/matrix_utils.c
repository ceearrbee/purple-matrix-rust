#include "matrix_utils.h"
#include "matrix_globals.h"
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

void thread_list_cb(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, room_id, account);
  if (conv) {
    char *msg = g_strdup_printf("Thread Found: root_id=%s, count=%" G_GUINT64_FORMAT ", latest=\"%s\"", thread_root_id, count, latest_msg);
    purple_conversation_write(conv, "Matrix", msg, PURPLE_MESSAGE_SYSTEM, (time_t)ts);
    g_free(msg);
  }
}

void poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, room_id, account);
  if (conv) {
    char *msg = g_strdup_printf("Active Poll: %s (by %s)\nOptions: %s", question, sender, options_str);
    purple_conversation_write(conv, "Matrix", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
    g_free(msg);
  }
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
