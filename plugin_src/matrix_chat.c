#include "matrix_chat.h"
#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_blist.h"

#include <libpurple/util.h>
#include <libpurple/debug.h>
#include <libpurple/imgstore.h>
#include <libpurple/server.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define MATRIX_PASTED_PREFIX "matrix_pasted_"

static void check_and_send_pasted_images(PurpleConnection *gc, const char *who,
                                         const char *message) {
  const char *p = message;
  while ((p = strstr(p, "<img id=\""))) {
    p += 9;
    int id = atoi(p);
    if (id > 0) {
      PurpleStoredImage *img = purple_imgstore_find_by_id(id);
      if (img) {
        char *filename = g_build_filename(g_get_tmp_dir(),
                                          MATRIX_PASTED_PREFIX "XXXXXX", NULL);
        int fd = g_mkstemp(filename);
        if (fd != -1) {
          if (write(fd, purple_imgstore_get_data(img),
                    purple_imgstore_get_size(img)) > 0) {
            close(fd);
            purple_matrix_rust_send_file(
                purple_account_get_username(purple_connection_get_account(gc)),
                who, filename);
          } else {
            close(fd);
          }
        }
        g_free(filename);
      }
    }
  }
}

int matrix_send_im(PurpleConnection *gc, const char *who,
                          const char *message, PurpleMessageFlags flags) {
  check_and_send_pasted_images(gc, who, message);
  if (who[0] == '!') {
    purple_matrix_rust_send_message(
        purple_account_get_username(purple_connection_get_account(gc)), who,
        message);
    return 1;
  }
  purple_matrix_rust_send_im(
      purple_account_get_username(purple_connection_get_account(gc)), who,
      message);
  return 1;
}

int matrix_send_chat(PurpleConnection *gc, int id, const char *message,
                            PurpleMessageFlags flags) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (!conv) return -1;
  const char *room_id = purple_conversation_get_name(conv);
  check_and_send_pasted_images(gc, room_id, message);
  purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "me", message,
                         PURPLE_MESSAGE_SEND, time(NULL));
  purple_matrix_rust_send_message(
      purple_account_get_username(purple_connection_get_account(gc)), room_id,
      message);
  return 0;
}

unsigned int matrix_send_typing(PurpleConnection *gc, const char *name,
                                       PurpleTypingState state) {
  purple_matrix_rust_send_typing(
      purple_account_get_username(purple_connection_get_account(gc)), name,
      state == PURPLE_TYPING);
  return 0;
}

static gboolean process_msg_cb(gpointer data) {
  MatrixMsgData *d = (MatrixMsgData *)data;
  PurpleAccount *account = find_matrix_account();
  if (!account) {
    g_free(d->sender); g_free(d->message); g_free(d->room_id);
    if (d->thread_root_id) g_free(d->thread_root_id);
    if (d->event_id) g_free(d->event_id);
    g_free(d);
    return FALSE;
  }

  if (d->event_id) {
    PurpleConversation *main_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, d->room_id, account);
    if (main_conv) {
      g_free(purple_conversation_get_data(main_conv, "last_event_id"));
      purple_conversation_set_data(main_conv, "last_event_id", g_strdup(d->event_id));
      g_free(purple_conversation_get_data(main_conv, "last_thread_root_id"));
      if (d->thread_root_id) purple_conversation_set_data(main_conv, "last_thread_root_id", g_strdup(d->thread_root_id));
      else purple_conversation_set_data(main_conv, "last_thread_root_id", NULL);
    }
  }

  char *target_id = g_strdup(d->room_id);
  gboolean separate_threads = purple_account_get_bool(account, "separate_threads", FALSE);
  if (d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
    char *virtual_id = g_strdup_printf("%s|%s", d->room_id, d->thread_root_id);
    g_free(target_id); target_id = virtual_id;
  }

  if (g_str_has_prefix(d->message, "[System] ")) {
    const char *sys_msg = d->message + 9;
    PurpleConnection *gc_sys = purple_account_get_connection(account);
    if (gc_sys) {
      int chat_id = get_chat_id(target_id);
      PurpleConversation *conv = purple_find_chat(gc_sys, chat_id);
      if (conv) purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "", sys_msg, PURPLE_MESSAGE_SYSTEM, d->timestamp / 1000);
    }
    g_free(target_id); g_free(d->sender); g_free(d->message); g_free(d->room_id);
    if (d->thread_root_id) g_free(d->thread_root_id);
    if (d->event_id) g_free(d->event_id);
    g_free(d);
    return FALSE;
  }

  if (g_str_has_prefix(d->message, "[History] ") && d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
    ensure_thread_in_blist(account, target_id, d->message + 10, d->room_id);
  }

  PurpleConnection *gc = purple_account_get_connection(account);
  if (gc) {
    int chat_id = get_chat_id(target_id);
    PurpleConversation *conv = purple_find_chat(gc, chat_id);
    if (!conv) conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, target_id, account);
    if (d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
      char *clean_alias = sanitize_markup_text(d->message);
      ensure_thread_in_blist(account, target_id, clean_alias, d->room_id);
      g_free(clean_alias);
    }
    if (!conv) {
      serv_got_joined_chat(gc, chat_id, target_id);
      conv = purple_find_chat(gc, chat_id);
      if (conv) {
        PurpleChat *blist_chat = purple_blist_find_chat(account, target_id);
        if (blist_chat && blist_chat->alias) purple_conversation_set_title(conv, blist_chat->alias);
        else if (d->thread_root_id && separate_threads) purple_conversation_set_title(conv, "Thread");
      }
    } else purple_conv_chat_set_id(PURPLE_CONV_CHAT(conv), chat_id);

    if (conv && !purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), d->sender)) {
      purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), d->sender, NULL, PURPLE_CBFLAGS_NONE, TRUE);
    }

    if (d->thread_root_id && separate_threads) {
      int main_id = get_chat_id(d->room_id);
      if (purple_find_chat(gc, main_id)) {
        char *main_msg = g_strdup_printf("[Thread] %s", d->message);
        serv_got_chat_in(gc, main_id, d->sender, PURPLE_MESSAGE_RECV, main_msg, d->timestamp / 1000);
        g_free(main_msg);
      }
    }
    serv_got_chat_in(gc, chat_id, d->sender, PURPLE_MESSAGE_RECV, d->message, d->timestamp / 1000);
  }

  g_free(d->sender); g_free(d->message); g_free(d->room_id);
  if (d->thread_root_id) g_free(d->thread_root_id);
  if (d->event_id) g_free(d->event_id);
  g_free(d); g_free(target_id);
  return FALSE;
}

void msg_callback(const char *sender, const char *msg, const char *room_id, const char *thread_root_id, const char *event_id, guint64 timestamp) {
  MatrixMsgData *d = g_new0(MatrixMsgData, 1);
  d->sender = g_strdup(sender); d->message = g_strdup(msg); d->room_id = g_strdup(room_id);
  if (thread_root_id) d->thread_root_id = g_strdup(thread_root_id);
  if (event_id) d->event_id = g_strdup(event_id);
  d->timestamp = timestamp;
  g_idle_add(process_msg_cb, d);
}

static gboolean process_typing_cb(gpointer data) {
  MatrixTypingData *d = (MatrixTypingData *)data;
  g_free(d->room_id); g_free(d->user_id); g_free(d);
  return FALSE;
}

void typing_callback(const char *room_id, const char *user_id, bool is_typing) {
  MatrixTypingData *d = g_new0(MatrixTypingData, 1);
  d->room_id = g_strdup(room_id); d->user_id = g_strdup(user_id); d->is_typing = is_typing;
  g_idle_add(process_typing_cb, d);
}

static gboolean process_read_marker_cb(gpointer data) {
  MatrixReadMarkerData *d = (MatrixReadMarkerData *)data;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, d->room_id, account);
    if (!conv) conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, d->room_id, account);
    if (conv) {
      const char *local_user = purple_account_get_username(account);
      if (d->user_id && strcmp(d->user_id, local_user) != 0) {
        char *msg = g_strdup_printf("[Read Receipt] %s read up to %s", d->user_id, d->event_id);
        purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "", msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
      }
    }
  }
  g_free(d->room_id); g_free(d->event_id); g_free(d->user_id); g_free(d);
  return FALSE;
}

void read_marker_cb(const char *room_id, const char *event_id, const char *user_id) {
  MatrixReadMarkerData *d = g_new0(MatrixReadMarkerData, 1);
  d->room_id = g_strdup(room_id); d->event_id = g_strdup(event_id); d->user_id = g_strdup(user_id);
  g_idle_add(process_read_marker_cb, d);
}

GList *matrix_chat_info(PurpleConnection *gc) {
  struct proto_chat_entry *pce = g_new0(struct proto_chat_entry, 1);
  pce->label = "Room ID"; pce->identifier = "room_id"; pce->required = TRUE;
  return g_list_append(NULL, pce);
}

GHashTable *matrix_chat_info_defaults(PurpleConnection *gc, const char *chat_name) {
  GHashTable *defaults = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  if (chat_name) g_hash_table_insert(defaults, g_strdup("room_id"), g_strdup(chat_name));
  return defaults;
}

void matrix_join_chat(PurpleConnection *gc, GHashTable *data) {
  const char *room_id = g_hash_table_lookup(data, "room_id");
  PurpleAccount *account = purple_connection_get_account(gc);
  if (!room_id) return;
  purple_matrix_rust_join_room(purple_account_get_username(account), room_id);
  purple_matrix_rust_fetch_room_members(purple_account_get_username(account), room_id);

  if (purple_account_get_bool(account, "auto_fetch_history_on_open", TRUE)) {
    purple_matrix_rust_fetch_history(purple_account_get_username(account), room_id);
  }

  int chat_id = get_chat_id(room_id);
  PurpleConversation *conv = purple_find_chat(gc, chat_id);
  if (!conv) {
    serv_got_joined_chat(gc, chat_id, room_id);
    conv = purple_find_chat(gc, chat_id);
    if (conv) {
      PurpleChat *blist_chat = purple_blist_find_chat(account, room_id);
      if (blist_chat && blist_chat->alias) purple_conversation_set_title(conv, blist_chat->alias);
      purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), purple_account_get_username(account), NULL, PURPLE_CBFLAGS_NONE, FALSE);
    }
  } else purple_conversation_present(conv);
}

void matrix_chat_invite(PurpleConnection *gc, int id, const char *message, const char *name) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv) purple_matrix_rust_invite_user(purple_account_get_username(purple_connection_get_account(gc)), purple_conversation_get_name(conv), name);
}

void matrix_chat_leave(PurpleConnection *gc, int id) { }
void matrix_chat_whisper(PurpleConnection *gc, int id, const char *who, const char *message) { }
int matrix_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags) { return matrix_send_chat(gc, id, message, flags); }

void matrix_set_chat_topic(PurpleConnection *gc, int id, const char *topic) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv) purple_matrix_rust_set_room_topic(purple_account_get_username(purple_connection_get_account(gc)), purple_conversation_get_name(conv), topic);
}

void matrix_send_file(PurpleConnection *gc, const char *who, const char *filename) {
  if (filename && who) purple_matrix_rust_send_file(purple_account_get_username(purple_connection_get_account(gc)), who, filename);
}
