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
#include <libpurple/signals.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#define MATRIX_PASTED_PREFIX "matrix_pasted_"

static PurpleConversation *find_conversation_by_room_id(PurpleAccount *account, const char *room_id) {
    if (!account || !room_id) return NULL;
    GList *convs = purple_get_conversations();
    for (GList *it = convs; it; it = it->next) {
        PurpleConversation *conv = (PurpleConversation *)it->data;
        if (purple_conversation_get_account(conv) != account) continue;
        
        // For Chats, the name is the room_id
        if (purple_conversation_get_type(conv) == PURPLE_CONV_TYPE_CHAT) {
            if (g_strcmp0(purple_conversation_get_name(conv), room_id) == 0) return conv;
        }
        
        // For IMs, we check the metadata
        const char *mrid = purple_conversation_get_data(conv, "matrix_room_id");
        if (g_strcmp0(mrid, room_id) == 0) return conv;
    }
    return NULL;
}

static void check_and_send_pasted_images(PurpleConnection *gc, const char *who,
                                         const char *message) {
  const char *p = message;
  while ((p = strstr(p, "<img id=\""))) {
    p += 9;
    char *endptr;
    errno = 0;
    long id_l = strtol(p, &endptr, 10);
    int id = (endptr == p || errno != 0 || id_l <= 0 || id_l > INT_MAX) ? 0 : (int)id_l;
    if (id > 0) {
      PurpleStoredImage *img = purple_imgstore_find_by_id(id);
      if (img) {
        char *filename = g_build_filename(g_get_tmp_dir(),
                                          MATRIX_PASTED_PREFIX "XXXXXX", NULL);
        mode_t old_umask = umask(0077);
        int fd = g_mkstemp(filename);
        umask(old_umask);
        if (fd != -1) {
          ssize_t expected = (ssize_t)purple_imgstore_get_size(img);
          if (write(fd, purple_imgstore_get_data(img), (size_t)expected) == expected) {
            close(fd);
            purple_matrix_rust_send_file(
                purple_account_get_username(purple_connection_get_account(gc)),
                who, filename);
          } else {
            close(fd);
            unlink(filename);
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
  purple_matrix_rust_send_message(
      purple_account_get_username(purple_connection_get_account(gc)), room_id,
      message);
  return 0;
}

int matrix_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags) {
    return matrix_send_chat(gc, id, message, flags);
}

void matrix_chat_leave(PurpleConnection *gc, int id) {
    PurpleConversation *conv = purple_find_chat(gc, id);
    if (!conv) return;
    purple_matrix_rust_leave_room(purple_account_get_username(purple_connection_get_account(gc)), purple_conversation_get_name(conv));
}

void matrix_chat_invite(PurpleConnection *gc, int id, const char *message, const char *name) {
    PurpleConversation *conv = purple_find_chat(gc, id);
    if (!conv) return;
    purple_matrix_rust_invite_user(purple_account_get_username(purple_connection_get_account(gc)), purple_conversation_get_name(conv), name);
}

void matrix_chat_whisper(PurpleConnection *gc, int id, const char *who, const char *message) {
    // whisper is not native to Matrix, could be implemented as a separate IM
    matrix_send_im(gc, who, message, PURPLE_MESSAGE_WHISPER);
}

void matrix_join_chat(PurpleConnection *gc, GHashTable *data) {
    const char *room_id = g_hash_table_lookup(data, "room_id");
    purple_debug_info("matrix-ffi", "matrix_join_chat: room_id=%s\n", room_id ? room_id : "(null)");
    if (!room_id) return;

    if (is_virtual_room_id(room_id)) {
        /* Thread virtual ID (room_id|thread_root_id): joining the virtual ID as a
           Matrix room would always fail.  Instead open the canonical parent room
           so the user gets the conversation window they expect. */
        char *parent_room_id = dup_base_room_id(room_id);
        if (parent_room_id) {
            purple_debug_info("matrix-ffi",
                "matrix_join_chat: virtual thread ID %s -> joining parent %s\n",
                room_id, parent_room_id);
            int cid = get_chat_id(parent_room_id);
            serv_got_joined_chat(gc, cid, parent_room_id);
            purple_matrix_rust_join_room(
                purple_account_get_username(purple_connection_get_account(gc)),
                parent_room_id);
            g_free(parent_room_id);
        }
        return;
    }

    /* Canonical room: open window and join normally */
    int cid = get_chat_id(room_id);
    serv_got_joined_chat(gc, cid, room_id);
    purple_matrix_rust_join_room(
        purple_account_get_username(purple_connection_get_account(gc)), room_id);
}

void matrix_reject_chat(PurpleConnection *gc, GHashTable *data) {
    // Rejection is similar to leaving or just ignoring
}

GList *matrix_chat_info(PurpleConnection *gc) {
    GList *m = NULL;
    struct proto_chat_entry *pce;

    pce = g_new0(struct proto_chat_entry, 1);
    pce->label = "Room ID or Alias";
    pce->identifier = "room_id";
    pce->required = TRUE;
    m = g_list_append(m, pce);

    return m;
}

GHashTable *matrix_chat_info_defaults(PurpleConnection *gc, const char *chat_name) {
    GHashTable *defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
    if (chat_name) {
        g_hash_table_insert(defaults, "room_id", g_strdup(chat_name));
    }
    return defaults;
}

void matrix_set_chat_topic(PurpleConnection *gc, int id, const char *topic) {
    PurpleConversation *conv = purple_find_chat(gc, id);
    if (conv) {
        purple_matrix_rust_set_room_topic(purple_account_get_username(purple_connection_get_account(gc)), purple_conversation_get_name(conv), topic);
    }
}

void matrix_send_file(PurpleConnection *gc, const char *who, const char *filename) {
    if (filename && who) {
        purple_matrix_rust_send_file(purple_account_get_username(purple_connection_get_account(gc)), who, filename);
    }
}

unsigned int matrix_send_typing(PurpleConnection *gc, const char *name,
                                       PurpleTypingState state) {
  static GHashTable *last_typing_times = NULL;
  if (!last_typing_times) {
    last_typing_times = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  }

  time_t now = time(NULL);
  time_t last_time = GPOINTER_TO_INT(g_hash_table_lookup(last_typing_times, name));
  
  /* Matrix typing notifications typically last 4-5 seconds. 
     Debounce to once every 4 seconds to avoid spamming the homeserver. */
  if (state == PURPLE_TYPING) {
    if (now - last_time < 4) {
      return 0;
    }
    g_hash_table_insert(last_typing_times, g_strdup(name), GINT_TO_POINTER(now));
  } else {
    /* Always send "stopped typing" immediately, and clear the timer */
    g_hash_table_remove(last_typing_times, name);
  }

  purple_matrix_rust_send_typing(
      purple_account_get_username(purple_connection_get_account(gc)), name,
      state == PURPLE_TYPING);
  return 0;
}

void msg_callback(const char *user_id, const char *sender, const char *sender_id, const char *msg,
                   const char *room_id, const char *thread_root_id,
                   const char *event_id, guint64 timestamp, bool encrypted, bool is_system, bool is_direct) {
  if (!user_id || !room_id || !sender || !sender_id || !msg) return;

  purple_debug_info("matrix-ffi", "msg_callback: room_id=%s sender=%s sender_id=%s user_id=%s is_direct=%d\n", room_id, sender, sender_id, user_id, is_direct);

  PurpleAccount *account = find_matrix_account_by_id(user_id);
  if (!account) {
    purple_debug_warning("matrix-ffi", "msg_callback: No account found for %s\n", user_id);
    return;
  }

  if (encrypted) {
    purple_signal_emit(my_plugin, "matrix-ui-room-encrypted", room_id, TRUE);
  }

  PurpleConversation *main_conv = find_conversation_by_room_id(account, room_id);

  if (main_conv) {
    /* Ensure the room ID is linked if it was opened via MXID */
    if (!purple_conversation_get_data(main_conv, "matrix_room_id")) {
        purple_conversation_set_data(main_conv, "matrix_room_id", g_strdup(room_id));
    }

    if (event_id && *event_id) {
      g_free(purple_conversation_get_data(main_conv, "last_event_id"));
      purple_conversation_set_data(main_conv, "last_event_id", g_strdup(event_id));
      g_free(purple_conversation_get_data(main_conv, "pending_event_id"));
      purple_conversation_set_data(main_conv, "pending_event_id", g_strdup(event_id));
    }
    
    if (msg && *msg) {
      char *plain = purple_markup_strip_html(msg);
      if (plain) {
          if (strlen(plain) > 120) plain[120] = '\0';
          purple_signal_emit(my_plugin, "matrix-ui-room-activity", room_id, sender, plain);
          g_free(plain);
      }
    }
    
    if (thread_root_id)
      purple_conversation_set_data(main_conv, "last_thread_root_id", g_strdup(thread_root_id));
  }

  char *target_id = g_strdup(room_id);
  gboolean separate_threads = purple_account_get_bool(account, "separate_threads", FALSE);
  if (thread_root_id && strlen(thread_root_id) > 0 && separate_threads) {
    char *virtual_id = g_strdup_printf("%s|%s", room_id, thread_root_id);
    g_free(target_id);
    target_id = virtual_id;
  }

  if (is_system || g_str_has_prefix(msg, "[System] ")) {
    const char *sys_msg = msg;
    if (g_str_has_prefix(msg, "[System] ")) sys_msg += 9;
    
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, target_id, account);
    if (conv) {
      purple_conversation_write(conv, "", sys_msg, PURPLE_MESSAGE_SYSTEM, timestamp / 1000);
    }
  } else {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
        int cid = get_chat_id(target_id);
        time_t mtime = timestamp / 1000;
        gboolean is_live = (mtime >= plugin_start_time - 5);
        
        PurpleConversation *conv = find_conversation_by_room_id(account, room_id);

        if (!is_direct) {
            if (conv && !purple_find_chat(gc, cid)) {
                /* Register the join for restored windows */
                serv_got_joined_chat(gc, cid, target_id);
            }
            if (conv) {
                /* delivers the message */
                serv_got_chat_in(gc, cid, sender, PURPLE_MESSAGE_RECV, msg, mtime);
            } else if (is_live && purple_account_get_bool(account, "auto_open_rooms", FALSE)) {
                /* Live message for closed chat: Join and deliver (opens window) */
                purple_debug_info("matrix-ffi", "Live message for room %s, opening window\n", target_id);
                serv_got_joined_chat(gc, cid, target_id);
                serv_got_chat_in(gc, cid, sender, PURPLE_MESSAGE_RECV, msg, mtime);
            } else {
                /* Backlog or auto-open disabled: skip popup but maybe we should still deliver if conv exists? 
                   conv is NULL here. We don't want to open it. */
                purple_debug_info("matrix-ffi", "msg_callback: Not opening window for room %s\n", target_id);
            }
        } else {
            /* IM delivery */
            if (conv || (is_live && purple_account_get_bool(account, "auto_open_dms", TRUE))) {
                /* If it's history and the sender is me, we need to deliver it as an 'outgoing' message or just a recv from me */
                serv_got_im(gc, is_direct ? sender_id : sender, msg, PURPLE_MESSAGE_RECV, mtime);
            } else {
                purple_debug_info("matrix-ffi", "msg_callback: Not opening window for DM %s\n", sender_id);
            }
        }
    } else {
        purple_debug_warning("matrix-ffi", "msg_callback: GC is NULL for user %s\n", user_id);
    }
  }
  g_free(target_id);
}

void reactions_changed_callback(const char *user_id, const char *room_id,
                                  const char *event_id,
                                  const char *reactions_text) {
    purple_signal_emit(my_plugin, "matrix-ui-reactions-changed", room_id, event_id, reactions_text);
}

void message_edited_callback(const char *user_id, const char *room_id,
                              const char *event_id, const char *new_msg) {
    purple_signal_emit(my_plugin, "matrix-ui-message-edited", room_id, event_id, new_msg);
}

void message_redacted_callback(const char *user_id, const char *room_id,
                                const char *event_id) {
    purple_signal_emit(my_plugin, "matrix-ui-message-redacted", room_id, event_id);
}

void media_downloaded_callback(const char *user_id, const char *room_id,
                                const char *event_id, const unsigned char *data,
                                size_t size, const char *content_type) {
    purple_signal_emit(my_plugin, "matrix-ui-media-downloaded", room_id, event_id, data, size, content_type);
}

void typing_callback(const char *user_id, const char *room_id, const char *who, bool is_typing) {
  PurpleAccount *account = find_matrix_account_by_id(user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    serv_got_typing(gc, who, 0, is_typing ? PURPLE_TYPING : PURPLE_NOT_TYPING);
    purple_signal_emit(my_plugin, "matrix-ui-room-typing", room_id, who, is_typing);
  }
}

void read_marker_cb(const char *user_id, const char *room_id, const char *event_id, const char *who) {
    purple_signal_emit(my_plugin, "matrix-ui-read-receipt", room_id, who, event_id);
}

void read_receipt_cb(const char *user_id, const char *room_id, const char *who, const char *event_id) {
    purple_signal_emit(my_plugin, "matrix-ui-read-receipt", room_id, who, event_id);
}

// Rest of the file...
