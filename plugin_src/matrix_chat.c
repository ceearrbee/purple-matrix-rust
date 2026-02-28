#include "matrix_chat.h"
#include "matrix_blist.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/debug.h>
#include <libpurple/imgstore.h>
#include <libpurple/server.h>
#include <libpurple/signals.h>
#include <libpurple/util.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MATRIX_PASTED_PREFIX "matrix_pasted_"
#define MATRIX_RECENT_EVENT_SLOTS 10

static void matrix_conv_data_replace(PurpleConversation *conv, const char *key,
                                     const char *value) {
  if (!conv || !key)
    return;
  g_free(purple_conversation_get_data(conv, key));
  purple_conversation_set_data(conv, key, value ? g_strdup(value) : NULL);
}

static void matrix_record_recent_event(PurpleConversation *conv,
                                       const char *event_id, const char *sender,
                                       const char *message,
                                       const char *thread_root_id,
                                       gboolean encrypted, guint64 timestamp) {
  int i;
  char key_id[64], key_sender[64], key_msg[64], key_ts[64], key_enc[64],
      key_thread[64];
  char *plain = NULL;
  char *snippet = NULL;
  char tsbuf[32];
  char encbuf[8];

  if (!conv || !event_id || !*event_id)
    return;

  for (i = MATRIX_RECENT_EVENT_SLOTS - 1; i > 0; --i) {
    char prev_id[64], prev_sender[64], prev_msg[64], prev_ts[64], prev_enc[64],
        prev_thread[64];
    const char *v = NULL;

    g_snprintf(prev_id, sizeof(prev_id), "matrix_recent_event_id_%d", i - 1);
    g_snprintf(prev_sender, sizeof(prev_sender),
               "matrix_recent_event_sender_%d", i - 1);
    g_snprintf(prev_msg, sizeof(prev_msg), "matrix_recent_event_msg_%d", i - 1);
    g_snprintf(prev_ts, sizeof(prev_ts), "matrix_recent_event_ts_%d", i - 1);
    g_snprintf(prev_enc, sizeof(prev_enc), "matrix_recent_event_enc_%d", i - 1);
    g_snprintf(prev_thread, sizeof(prev_thread),
               "matrix_recent_event_thread_%d", i - 1);

    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d",
               i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    g_snprintf(key_ts, sizeof(key_ts), "matrix_recent_event_ts_%d", i);
    g_snprintf(key_enc, sizeof(key_enc), "matrix_recent_event_enc_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);

    v = purple_conversation_get_data(conv, prev_id);
    matrix_conv_data_replace(conv, key_id, v);
    v = purple_conversation_get_data(conv, prev_sender);
    matrix_conv_data_replace(conv, key_sender, v);
    v = purple_conversation_get_data(conv, prev_msg);
    matrix_conv_data_replace(conv, key_msg, v);
    v = purple_conversation_get_data(conv, prev_ts);
    matrix_conv_data_replace(conv, key_ts, v);
    v = purple_conversation_get_data(conv, prev_enc);
    matrix_conv_data_replace(conv, key_enc, v);
    v = purple_conversation_get_data(conv, prev_thread);
    matrix_conv_data_replace(conv, key_thread, v);
  }

  plain = purple_markup_strip_html(message ? message : "");
  if (!plain)
    plain = g_strdup("");
  if ((int)strlen(plain) > 120) {
    plain[120] = '\0';
  }
  snippet = sanitize_markup_text(plain);
  g_free(plain);

  g_snprintf(tsbuf, sizeof(tsbuf), "%" G_GUINT64_FORMAT, timestamp);
  g_snprintf(encbuf, sizeof(encbuf), "%d", encrypted ? 1 : 0);
  matrix_conv_data_replace(conv, "matrix_recent_event_id_0", event_id);
  matrix_conv_data_replace(conv, "matrix_recent_event_sender_0",
                           sender ? sender : "");
  matrix_conv_data_replace(conv, "matrix_recent_event_msg_0",
                           snippet ? snippet : "");
  matrix_conv_data_replace(conv, "matrix_recent_event_ts_0", tsbuf);
  matrix_conv_data_replace(conv, "matrix_recent_event_enc_0", encbuf);
  matrix_conv_data_replace(conv, "matrix_recent_event_thread_0",
                           thread_root_id ? thread_root_id : "");

  if (snippet)
    g_free(snippet);
}

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

int matrix_send_im(PurpleConnection *gc, const char *who, const char *message,
                   PurpleMessageFlags flags) {
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
  if (!conv)
    return -1;
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

  if (!d->user_id || !d->room_id || !d->sender || !d->message) {
    if (d->user_id)
      g_free(d->user_id);
    if (d->sender)
      g_free(d->sender);
    if (d->message)
      g_free(d->message);
    if (d->room_id)
      g_free(d->room_id);
    if (d->thread_root_id)
      g_free(d->thread_root_id);
    if (d->event_id)
      g_free(d->event_id);
    g_free(d);
    return FALSE;
  }

  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (!account) {
    g_free(d->user_id);
    g_free(d->sender);
    g_free(d->message);
    g_free(d->room_id);
    if (d->thread_root_id)
      g_free(d->thread_root_id);
    if (d->event_id)
      g_free(d->event_id);
    g_free(d);
    return FALSE;
  }

  /* Signal UI that room is (potentially) encrypted if we see an encrypted msg
   */
  if (d->encrypted) {
    purple_debug_info("matrix-ui-signal",
                      "Dispatching matrix-ui-room-encrypted room_id=%s\n",
                      d->room_id);
    purple_signal_emit(my_plugin, "matrix-ui-room-encrypted", d->room_id, TRUE);
  }

  if (d->event_id) {
    PurpleConversation *main_conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, d->room_id, account);
    if (!main_conv)
      main_conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
                                                        d->room_id, account);

    if (main_conv) {
      g_free(purple_conversation_get_data(main_conv, "last_event_id"));
      purple_conversation_set_data(main_conv, "last_event_id",
                                   g_strdup(d->event_id));
      matrix_record_recent_event(main_conv, d->event_id, d->sender, d->message,
                                 d->thread_root_id, d->encrypted, d->timestamp);
      if (d->message && *d->message) {
        char *plain = purple_markup_strip_html(d->message);
        char *snippet = NULL;
        if (!plain)
          plain = g_strdup("");
        if ((int)strlen(plain) > 120)
          plain[120] = '\0';
        snippet = sanitize_markup_text(plain);
        purple_debug_info(
            "matrix-ui-signal",
            "Dispatching matrix-ui-room-activity room_id=%s sender=%s\n",
            d->room_id, d->sender ? d->sender : "user");
        purple_signal_emit(my_plugin, "matrix-ui-room-activity", d->room_id,
                           d->sender ? d->sender : "user",
                           snippet ? snippet : "");
        g_free(snippet);
        g_free(plain);
      }
      g_free(purple_conversation_get_data(main_conv, "last_thread_root_id"));
      if (d->thread_root_id)
        purple_conversation_set_data(main_conv, "last_thread_root_id",
                                     g_strdup(d->thread_root_id));
      else
        purple_conversation_set_data(main_conv, "last_thread_root_id", NULL);
    }
  }

  char *target_id = g_strdup(d->room_id);
  gboolean separate_threads =
      purple_account_get_bool(account, "separate_threads", FALSE);
  if (d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
    char *virtual_id = g_strdup_printf("%s|%s", d->room_id, d->thread_root_id);
    g_free(target_id);
    target_id = virtual_id;
  }

  if (g_str_has_prefix(d->message, "[System] ")) {
    const char *sys_msg = d->message + 9;
    PurpleConnection *gc_sys = purple_account_get_connection(account);
    if (gc_sys) {
      int chat_id = get_chat_id(target_id);
      PurpleConversation *conv = purple_find_chat(gc_sys, chat_id);
      if (conv)
        purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "", sys_msg,
                               PURPLE_MESSAGE_SYSTEM, d->timestamp / 1000);
    }
    g_free(target_id);
    g_free(d->user_id);
    g_free(d->sender);
    g_free(d->message);
    g_free(d->room_id);
    if (d->thread_root_id)
      g_free(d->thread_root_id);
    if (d->event_id)
      g_free(d->event_id);
    g_free(d);
    return FALSE;
  }

  PurpleConnection *gc = purple_account_get_connection(account);
  if (gc) {
    int chat_id = get_chat_id(target_id);
    PurpleConversation *conv = purple_find_chat(gc, chat_id);
    if (!conv)
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                   target_id, account);

    if (d->thread_root_id && strlen(d->thread_root_id) > 0 &&
        separate_threads) {
      char *clean_alias = sanitize_markup_text(d->message);
      ensure_thread_in_blist(account, target_id, clean_alias, d->room_id);
      g_free(clean_alias);
    }

    if (conv) {
      char *s_sender = g_strdup(d->sender ? d->sender : "Unknown");
      char *s_msg = g_strdup(d->message ? d->message : " ");

      purple_conversation_write(conv, s_sender, s_msg, PURPLE_MESSAGE_RECV,
                                d->timestamp / 1000);

      g_free(s_msg);
      g_free(s_sender);
    }
  }

  g_free(d->user_id);
  g_free(d->sender);
  g_free(d->message);
  g_free(d->room_id);
  if (d->thread_root_id)
    g_free(d->thread_root_id);
  if (d->event_id)
    g_free(d->event_id);
  g_free(d);
  g_free(target_id);
  return FALSE;
}

void msg_callback(const char *user_id, const char *sender, const char *msg,
                  const char *room_id, const char *thread_root_id,
                  const char *event_id, guint64 timestamp, bool encrypted) {
  MatrixMsgData *d = g_new0(MatrixMsgData, 1);
  d->user_id = g_strdup(user_id);
  d->sender = g_strdup(sender);
  d->message = g_strdup(msg);
  d->room_id = g_strdup(room_id);
  if (thread_root_id)
    d->thread_root_id = g_strdup(thread_root_id);
  if (event_id)
    d->event_id = g_strdup(event_id);
  d->timestamp = timestamp;
  d->encrypted = encrypted;
  g_idle_add(process_msg_cb, d);
}

static gboolean process_typing_cb(gpointer data) {
  MatrixTypingData *d = (MatrixTypingData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      serv_got_typing(gc, d->who, 0,
                      d->is_typing ? PURPLE_TYPING : PURPLE_NOT_TYPING);
      /* Signal UI plugin */
      purple_debug_info(
          "matrix-ui-signal",
          "Dispatching matrix-ui-room-typing room_id=%s who=%s typing=%d\n",
          d->room_id, d->who, d->is_typing);
      purple_signal_emit(my_plugin, "matrix-ui-room-typing", d->room_id, d->who,
                         d->is_typing);
    }
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->who);
  g_free(d);
  return FALSE;
}

void typing_callback(const char *user_id, const char *room_id, const char *who,
                     bool is_typing) {
  MatrixTypingData *d = g_new0(MatrixTypingData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->who = g_strdup(who);
  d->is_typing = is_typing;
  g_idle_add(process_typing_cb, d);
}

static gboolean process_read_marker_cb(gpointer data) {
  MatrixReadMarkerData *d = (MatrixReadMarkerData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, d->room_id, account);
    if (!conv)
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
                                                   d->room_id, account);
    if (conv) {
      if (d->who && strcmp(d->who, d->user_id) != 0) {
        char *msg = g_strdup_printf("[Read Receipt] %s read up to %s", d->who,
                                    d->event_id);
        purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "", msg,
                               PURPLE_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
      }
    }
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->event_id);
  g_free(d->who);
  g_free(d);
  return FALSE;
}

void read_marker_cb(const char *user_id, const char *room_id,
                    const char *event_id, const char *who) {
  MatrixReadMarkerData *d = g_new0(MatrixReadMarkerData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->event_id = g_strdup(event_id);
  d->who = g_strdup(who);
  g_idle_add(process_read_marker_cb, d);
}

GList *matrix_chat_info(PurpleConnection *gc) {
  struct proto_chat_entry *pce = g_new0(struct proto_chat_entry, 1);
  pce->label = "Room ID";
  pce->identifier = "room_id";
  pce->required = TRUE;
  return g_list_append(NULL, pce);
}

GHashTable *matrix_chat_info_defaults(PurpleConnection *gc,
                                      const char *chat_name) {
  GHashTable *defaults =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  if (chat_name)
    g_hash_table_insert(defaults, g_strdup("room_id"), g_strdup(chat_name));
  return defaults;
}

void matrix_join_chat(PurpleConnection *gc, GHashTable *data) {
  const char *room_id = g_hash_table_lookup(data, "room_id");
  PurpleAccount *account = purple_connection_get_account(gc);
  if (!room_id)
    return;

  purple_matrix_rust_join_room(purple_account_get_username(account), room_id);
  purple_matrix_rust_fetch_room_members(purple_account_get_username(account),
                                        room_id);

  int chat_id = get_chat_id(room_id);
  PurpleConversation *conv = purple_find_chat(gc, chat_id);
  if (!conv) {
    serv_got_joined_chat(gc, chat_id, room_id);
    conv = purple_find_chat(gc, chat_id);
    if (conv) {
      PurpleChat *blist_chat = purple_blist_find_chat(account, room_id);
      if (blist_chat && blist_chat->alias) {
        purple_conversation_set_title(conv, blist_chat->alias);
      } else {
        purple_conversation_set_title(conv, room_id);
      }
      purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv),
                                purple_account_get_username(account), NULL,
                                PURPLE_CBFLAGS_NONE, FALSE);
      if (purple_account_get_bool(account, "auto_fetch_history_on_open",
                                  TRUE)) {
        purple_matrix_rust_fetch_history(purple_account_get_username(account),
                                         room_id);
      }
    }
  } else {
    purple_conversation_present(conv);
  }
}

void matrix_reject_chat(PurpleConnection *gc, GHashTable *data) {
  const char *room_id = g_hash_table_lookup(data, "room_id");
  PurpleAccount *account = purple_connection_get_account(gc);
  if (room_id) {
    purple_matrix_rust_leave_room(purple_account_get_username(account),
                                  room_id);
  }
}

void matrix_chat_invite(PurpleConnection *gc, int id, const char *message,
                        const char *name) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv)
    purple_matrix_rust_invite_user(
        purple_account_get_username(purple_connection_get_account(gc)),
        purple_conversation_get_name(conv), name);
}

typedef struct {
  char *user_id;
  char *room_id;
} LeaveContext;

static void matrix_chat_leave_confirm_cb(LeaveContext *ctx) {
  if (ctx && ctx->user_id && ctx->room_id) {
    purple_matrix_rust_leave_room(ctx->user_id, ctx->room_id);
  }
  if (ctx) {
    g_free(ctx->user_id);
    g_free(ctx->room_id);
    g_free(ctx);
  }
}

static void matrix_chat_leave_cancel_cb(LeaveContext *ctx) {
  if (ctx) {
    g_free(ctx->user_id);
    g_free(ctx->room_id);
    g_free(ctx);
  }
}

void matrix_chat_leave(PurpleConnection *gc, int id) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv) {
    PurpleAccount *account = purple_connection_get_account(gc);
    const char *room_id = purple_conversation_get_name(conv);

    LeaveContext *ctx = g_new0(LeaveContext, 1);
    ctx->user_id = g_strdup(purple_account_get_username(account));
    ctx->room_id = g_strdup(room_id);

    char *msg = g_strdup_printf(
        "Are you sure you want to leave the Matrix room '%s'?", room_id);
    purple_request_action(gc, "Leave Room", "Confirm Leave", msg, 0, account,
                          NULL, conv, ctx, 2, "Leave",
                          G_CALLBACK(matrix_chat_leave_confirm_cb), "Cancel",
                          G_CALLBACK(matrix_chat_leave_cancel_cb));
    g_free(msg);
  }
}
void matrix_chat_whisper(PurpleConnection *gc, int id, const char *who,
                         const char *message) {}
int matrix_chat_send(PurpleConnection *gc, int id, const char *message,
                     PurpleMessageFlags flags) {
  return matrix_send_chat(gc, id, message, flags);
}

void matrix_set_chat_topic(PurpleConnection *gc, int id, const char *topic) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv)
    purple_matrix_rust_set_room_topic(
        purple_account_get_username(purple_connection_get_account(gc)),
        purple_conversation_get_name(conv), topic);
}

void matrix_send_file(PurpleConnection *gc, const char *who,
                      const char *filename) {
  if (filename && who)
    purple_matrix_rust_send_file(
        purple_account_get_username(purple_connection_get_account(gc)), who,
        filename);
}
