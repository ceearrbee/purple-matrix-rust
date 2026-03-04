#include "matrix_chat.h"
#include "matrix_blist.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/debug.h>
#include <libpurple/imgstore.h>
#include <libpurple/notify.h>
#include <libpurple/prpl.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <libpurple/version.h>
#include <stdlib.h>
#include <string.h>

#define MATRIX_RECENT_EVENT_SLOTS 10

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
    g_snprintf(prev_sender, sizeof(prev_sender), "matrix_recent_event_sender_%d",
               i - 1);
    g_snprintf(prev_msg, sizeof(prev_msg), "matrix_recent_event_msg_%d", i - 1);
    g_snprintf(prev_ts, sizeof(prev_ts), "matrix_recent_event_ts_%d", i - 1);
    g_snprintf(prev_enc, sizeof(prev_enc), "matrix_recent_event_enc_%d", i - 1);
    g_snprintf(prev_thread, sizeof(prev_thread), "matrix_recent_event_thread_%d",
               i - 1);

    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d",
               i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    g_snprintf(key_ts, sizeof(key_ts), "matrix_recent_event_ts_%d", i);
    g_snprintf(key_enc, sizeof(key_enc), "matrix_recent_event_enc_%d", i);
    g_snprintf(key_thread, sizeof(key_thread), "matrix_recent_event_thread_%d",
               i);

    v = purple_conversation_get_data(conv, prev_id);
    g_free(purple_conversation_get_data(conv, key_id));
    purple_conversation_set_data(conv, key_id, g_strdup(v));

    v = purple_conversation_get_data(conv, prev_sender);
    g_free(purple_conversation_get_data(conv, key_sender));
    purple_conversation_set_data(conv, key_sender, g_strdup(v));

    v = purple_conversation_get_data(conv, prev_msg);
    g_free(purple_conversation_get_data(conv, key_msg));
    purple_conversation_set_data(conv, key_msg, g_strdup(v));

    v = purple_conversation_get_data(conv, prev_ts);
    g_free(purple_conversation_get_data(conv, key_ts));
    purple_conversation_set_data(conv, key_ts, g_strdup(v));

    v = purple_conversation_get_data(conv, prev_enc);
    g_free(purple_conversation_get_data(conv, key_enc));
    purple_conversation_set_data(conv, key_enc, g_strdup(v));

    v = purple_conversation_get_data(conv, prev_thread);
    g_free(purple_conversation_get_data(conv, key_thread));
    purple_conversation_set_data(conv, key_thread, g_strdup(v));
  }

  plain = purple_markup_strip_html(message);
  if (plain) {
    if ((int)strlen(plain) > 60)
      plain[60] = '\0';
    snippet = sanitize_markup_text(plain);
    g_free(plain);
  }

  g_snprintf(tsbuf, sizeof(tsbuf), "%" G_GUINT64_FORMAT, timestamp);
  g_snprintf(encbuf, sizeof(encbuf), "%d", encrypted ? 1 : 0);

  g_free(purple_conversation_get_data(conv, "matrix_recent_event_id_0"));
  purple_conversation_set_data(conv, "matrix_recent_event_id_0",
                               g_strdup(event_id));
  g_free(purple_conversation_get_data(conv, "matrix_recent_event_sender_0"));
  purple_conversation_set_data(conv, "matrix_recent_event_sender_0",
                               g_strdup(sender));
  g_free(purple_conversation_get_data(conv, "matrix_recent_event_msg_0"));
  purple_conversation_set_data(conv, "matrix_recent_event_msg_0",
                               g_strdup(snippet ? snippet : ""));
  g_free(purple_conversation_get_data(conv, "matrix_recent_event_ts_0"));
  purple_conversation_set_data(conv, "matrix_recent_event_ts_0", g_strdup(tsbuf));
  g_free(purple_conversation_get_data(conv, "matrix_recent_event_enc_0"));
  purple_conversation_set_data(conv, "matrix_recent_event_enc_0",
                               g_strdup(encbuf));
  g_free(purple_conversation_get_data(conv, "matrix_recent_event_thread_0"));
  purple_conversation_set_data(conv, "matrix_recent_event_thread_0",
                               g_strdup(thread_root_id));

  g_free(snippet);
}

void matrix_get_chat_info_cb(PurpleConnection *gc, GHashTable *components) {
  // Return default chat components if needed
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
        size_t len = purple_imgstore_get_size(img);
        const void *data = purple_imgstore_get_data(img);
        char *tmp_path = g_build_filename(g_get_tmp_dir(), "matrix_pasted.png", NULL);
        if (g_file_set_contents(tmp_path, data, len, NULL)) {
          purple_matrix_rust_send_file(
              purple_account_get_username(purple_connection_get_account(gc)), who,
              tmp_path);
        }
        g_free(tmp_path);
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
  
  /* Disable local echo to avoid duplicates; rely on sync callback for correct Matrix event ID */
  /* PurpleAccount *account = purple_connection_get_account(gc);
  const char *me = purple_account_get_alias(account);
  if (!me || !*me) me = purple_account_get_username(account);
  purple_conv_chat_write(PURPLE_CONV_CHAT(conv), me, message,
                         PURPLE_MESSAGE_SEND, time(NULL)); */
  
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
    /* Cleanup and return */
    goto cleanup;
  }

  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (!account) goto cleanup;

  if (d->encrypted) {
    purple_signal_emit(my_plugin, "matrix-ui-room-encrypted", d->room_id, TRUE);
  }

  PurpleConversation *main_conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, d->room_id, account);

  if (main_conv) {
    if (d->event_id && *d->event_id) {
      purple_conversation_set_data(main_conv, "last_event_id", g_strdup(d->event_id));
      matrix_record_recent_event(main_conv, d->event_id, d->sender, d->message,
                                 d->thread_root_id, d->encrypted, d->timestamp);
    }
    
    if (d->message && *d->message) {
      char *plain = purple_markup_strip_html(d->message);
      char *snippet = NULL;
      if (!plain) plain = g_strdup("");
      if ((int)strlen(plain) > 120) plain[120] = '\0';
      snippet = sanitize_markup_text(plain);
      purple_signal_emit(my_plugin, "matrix-ui-room-activity", d->room_id,
                         d->sender ? d->sender : "user",
                         snippet ? snippet : "");
      g_free(snippet);
      g_free(plain);
    }
    
    if (d->thread_root_id)
      purple_conversation_set_data(main_conv, "last_thread_root_id", g_strdup(d->thread_root_id));
  }

  char *target_id = g_strdup(d->room_id);
  gboolean separate_threads = purple_account_get_bool(account, "separate_threads", FALSE);
  if (d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
    char *virtual_id = g_strdup_printf("%s|%s", d->room_id, d->thread_root_id);
    g_free(target_id);
    target_id = virtual_id;
  }

  if (g_str_has_prefix(d->message, "[System] ")) {
    const char *sys_msg = d->message + 9;
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, target_id, account);
    if (conv) {
      purple_conversation_write(conv, "", sys_msg, PURPLE_MESSAGE_SYSTEM, d->timestamp / 1000);
    }
    g_free(target_id);
    goto cleanup;
  }

  PurpleConnection *gc = purple_account_get_connection(account);
  if (gc) {
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, target_id, account);
    if (d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
      char *clean_alias = sanitize_markup_text(d->message);
      ensure_thread_in_blist(account, target_id, clean_alias, d->room_id);
      g_free(clean_alias);
    }

    if (conv) {
      const char *me_id = purple_account_get_username(account);
      const char *me_alias = purple_account_get_alias(account);
      PurpleMessageFlags flags = PURPLE_MESSAGE_RECV;
      
      if (strcmp(d->sender, me_id) == 0 || (me_alias && strcmp(d->sender, me_alias) == 0)) {
          flags = PURPLE_MESSAGE_SEND;
      }
      
      purple_conversation_write(conv, d->sender, d->message, flags, d->timestamp / 1000);
    }
  }
  g_free(target_id);

cleanup:
  g_free(d->user_id);
  g_free(d->sender);
  g_free(d->message);
  g_free(d->room_id);
  if (d->thread_root_id) g_free(d->thread_root_id);
  if (d->event_id) g_free(d->event_id);
  g_free(d);
  return FALSE;
}

void msg_callback(const char *user_id, const char *sender, const char *msg,
                  const char *room_id, const char *thread_root_id,
                  const char *event_id, guint64 timestamp, bool encrypted) {
  purple_debug_info("matrix", "msg_callback: sender=%s msg=%s\n", sender, msg);
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

static gboolean process_reactions_changed_cb(gpointer data) {
  MatrixReactionsData *d = (MatrixReactionsData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account) {
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_ANY, d->room_id, account);
    if (conv) {
      char key[128];
      g_snprintf(key, sizeof(key), "matrix_reactions_%s", d->event_id);
      g_free(purple_conversation_get_data(conv, key));
      purple_conversation_set_data(conv, key, g_strdup(d->reactions_text));

      purple_signal_emit(my_plugin, "matrix-ui-reactions-changed", d->room_id,
                         d->event_id, d->reactions_text);
    }
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->event_id);
  g_free(d->reactions_text);
  g_free(d);
  return FALSE;
}

void reactions_changed_callback(const char *user_id, const char *room_id,
                                const char *event_id,
                                const char *reactions_text) {
  MatrixReactionsData *d = g_new0(MatrixReactionsData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->event_id = g_strdup(event_id);
  d->reactions_text = g_strdup(reactions_text);
  g_idle_add(process_reactions_changed_cb, d);
}

static gboolean process_message_edited_cb(gpointer data) {
  MatrixEditData *d = (MatrixEditData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account) {
    purple_signal_emit(my_plugin, "matrix-ui-message-edited", d->room_id,
                       d->event_id, d->new_msg);
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->event_id);
  g_free(d->new_msg);
  g_free(d);
  return FALSE;
}

void message_edited_callback(const char *user_id, const char *room_id,
                             const char *event_id, const char *new_msg) {
  MatrixEditData *d = g_new0(MatrixEditData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->event_id = g_strdup(event_id);
  d->new_msg = g_strdup(new_msg);
  g_idle_add(process_message_edited_cb, d);
}

static gboolean process_read_receipt_cb(gpointer data) {
  MatrixReadReceiptData *d = (MatrixReadReceiptData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account) {
    purple_signal_emit(my_plugin, "matrix-ui-read-receipt", d->room_id, d->who,
                       d->event_id);
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->who);
  g_free(d->event_id);
  g_free(d);
  return FALSE;
}

void read_receipt_callback(const char *user_id, const char *room_id,
                           const char *who, const char *event_id) {
  MatrixReadReceiptData *d = g_new0(MatrixReadReceiptData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->who = g_strdup(who);
  d->event_id = g_strdup(event_id);
  g_idle_add(process_read_receipt_cb, d);
}
