/*
 * Purple Matrix Rust - Libpurple Plugin Wrapper
 */

#define PURPLE_PLUGINS

#include <glib.h>
#include <libpurple/accountopt.h>
#include <libpurple/cmds.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/imgstore.h>
#include <libpurple/notify.h>
#include <libpurple/plugin.h>
#include <libpurple/prpl.h>
#include <libpurple/request.h>
#include <libpurple/roomlist.h>
#include <libpurple/server.h>
#include <libpurple/status.h>
#include <libpurple/util.h>
#include <libpurple/version.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

// Forward Declarations
// static void matrix_invite_cb(const char *room_id, const char *inviter);
extern void purple_matrix_rust_init_logging(void);
extern void purple_matrix_rust_init_invite_cb(void (*cb)(const char *,
                                                         const char *));

extern void purple_matrix_rust_send_file(const char *user_id,
                                         const char *room_id,
                                         const char *filename);

#define _(x) ((const char *)x)
#define MATRIX_PREVIEW_ROOM_ID "Matrix Preview"

// Rust FFI declarations
typedef void (*MsgCallback)(const char *sender, const char *body,
                            const char *room_id, const char *thread_root_id,
                            const char *event_id, guint64 timestamp);

// ... (skipping lines)

// Moved process_msg_cb and msg_callback to later in the file where
// MatrixMsgData is defined.
extern void purple_matrix_rust_init(void);

// Forward declarations
static void ensure_thread_in_blist(PurpleAccount *account,
                                   const char *virtual_id, const char *alias,
                                   const char *parent_room_id);
extern void purple_matrix_rust_set_msg_callback(MsgCallback cb);
extern int purple_matrix_rust_login(const char *user, const char *pass,
                                    const char *hs, const char *data_dir);
extern void purple_matrix_rust_send_message(const char *user_id,
                                            const char *room_id,
                                            const char *text);
typedef void (*TypingCallback)(const char *room_id, const char *user_id,
                               gboolean is_typing);
extern void purple_matrix_rust_set_typing_callback(TypingCallback cb);
extern void purple_matrix_rust_send_typing(const char *user_id,
                                           const char *room_id,
                                           gboolean is_typing);
typedef void (*RoomJoinedCallback)(const char *room_id, const char *name,
                                   const char *group_name,
                                   const char *avatar_url, const char *topic,
                                   gboolean encrypted);
extern void purple_matrix_rust_set_room_joined_callback(RoomJoinedCallback cb);
typedef void (*ReadMarkerCallback)(const char *room_id, const char *event_id,
                                   const char *user_id);
extern void purple_matrix_rust_set_read_marker_callback(ReadMarkerCallback cb);

typedef enum {
  MATRIX_STATUS_OFFLINE = 0,
  MATRIX_STATUS_ONLINE = 1,
  MATRIX_STATUS_UNAVAILABLE = 2,
} MatrixStatus;

extern void purple_matrix_rust_join_room(const char *user_id,
                                         const char *room_id);
extern void purple_matrix_rust_leave_room(const char *user_id,
                                          const char *room_id);
extern void purple_matrix_rust_invite_user(const char *user_id,
                                           const char *room_id,
                                           const char *invitee_id);

typedef void (*UpdateBuddyCallback)(const char *user_id, const char *alias,
                                    const char *icon_path);
extern void
purple_matrix_rust_set_update_buddy_callback(UpdateBuddyCallback cb);

extern void purple_matrix_rust_init_sso_cb(void (*cb)(const char *));
extern void purple_matrix_rust_finish_sso(const char *token);
extern void purple_matrix_rust_set_display_name(const char *user_id,
                                                const char *name);
extern void purple_matrix_rust_set_avatar(const char *user_id,
                                          const char *path);
extern void purple_matrix_rust_bootstrap_cross_signing(const char *user_id);
extern void purple_matrix_rust_e2ee_status(const char *user_id,
                                           const char *room_id);
extern void purple_matrix_rust_verify_user(const char *user_id,
                                           const char *target_user_id);
extern void purple_matrix_rust_recover_keys(const char *user_id,
                                            const char *passphrase);
extern void purple_matrix_rust_logout(const char *user_id);
extern void purple_matrix_rust_send_location(const char *user_id,
                                             const char *room_id,
                                             const char *body,
                                             const char *geo_uri);
extern void purple_matrix_rust_poll_create(const char *user_id,
                                           const char *room_id,
                                           const char *question,
                                           const char *options);
extern void purple_matrix_rust_poll_vote(const char *user_id,
                                         const char *room_id,
                                         const char *poll_event_id,
                                         const char *option_text,
                                         const char *selection_index_str);
extern void purple_matrix_rust_poll_end(const char *user_id,
                                        const char *room_id,
                                        const char *poll_event_id);
extern void purple_matrix_rust_change_password(const char *user_id,
                                               const char *old_pw,
                                               const char *new_pw);
extern void purple_matrix_rust_add_buddy(const char *user_id,
                                         const char *buddy_id);
extern void purple_matrix_rust_remove_buddy(const char *user_id,
                                            const char *buddy_id);
extern void purple_matrix_rust_set_idle(const char *user_id, int seconds);
extern void purple_matrix_rust_send_sticker(const char *user_id,
                                            const char *room_id,
                                            const char *url);

extern void purple_matrix_rust_unignore_user(const char *user_id,
                                             const char *ignored_user_id);
extern void purple_matrix_rust_set_avatar_bytes(const char *user_id,
                                                const unsigned char *data,
                                                size_t len);

extern void purple_matrix_rust_deactivate_account(bool erase_data);
extern void purple_matrix_rust_fetch_public_rooms_for_list(const char *user_id);
extern void purple_matrix_rust_set_roomlist_add_callback(void (*cb)(
    const char *name, const char *id, const char *topic, guint64 count));
extern void purple_matrix_rust_set_room_preview_callback(
    void (*cb)(const char *room_id_or_alias, const char *html_body));

extern void purple_matrix_rust_set_thread_list_callback(
    void (*cb)(const char *room_id, const char *thread_root_id,
               const char *latest_msg, guint64 count, guint64 ts));
extern void purple_matrix_rust_list_threads(const char *user_id,
                                            const char *room_id);

extern void purple_matrix_rust_set_poll_list_callback(
    void (*cb)(const char *room_id, const char *event_id, const char *sender,
               const char *question, const char *options_str));

extern void purple_matrix_rust_get_active_polls(const char *user_id,
                                                const char *room_id);

extern void purple_matrix_rust_send_read_receipt(const char *user_id,
                                                 const char *room_id,
                                                 const char *event_id);
extern void purple_matrix_rust_send_reaction(const char *user_id,
                                             const char *room_id,
                                             const char *event_id,
                                             const char *key);
extern void purple_matrix_rust_redact_event(const char *user_id,
                                            const char *room_id,
                                            const char *event_id,
                                            const char *reason);
extern void purple_matrix_rust_send_edit(const char *user_id,
                                         const char *room_id,
                                         const char *event_id,
                                         const char *body);

extern void purple_matrix_rust_fetch_more_history(const char *user_id,
                                                  const char *room_id);
extern void purple_matrix_rust_fetch_more_history_with_limit(
    const char *user_id, const char *room_id, guint32 limit);
extern void purple_matrix_rust_resync_recent_history(const char *user_id,
                                                     const char *room_id);
extern void purple_matrix_rust_resync_recent_history_with_limit(
    const char *user_id, const char *room_id, guint32 limit);
extern void purple_matrix_rust_preview_room(const char *user_id,
                                            const char *room_id_or_alias,
                                            const char *output_room_id);
extern void purple_matrix_rust_fetch_history_with_limit(const char *user_id,
                                                        const char *room_id,
                                                        guint32 limit);
extern void purple_matrix_rust_create_room(const char *user_id,
                                           const char *name, const char *topic,
                                           bool is_public);
extern void purple_matrix_rust_search_public_rooms(const char *user_id,
                                                   const char *search_term,
                                                   const char *output_room_id);
extern void purple_matrix_rust_search_users(const char *user_id,
                                            const char *search_term,
                                            const char *room_id);
extern void purple_matrix_rust_kick_user(const char *user_id,
                                         const char *room_id,
                                         const char *target_user_id,
                                         const char *reason);
extern void purple_matrix_rust_ban_user(const char *user_id,
                                        const char *room_id,
                                        const char *target_user_id,
                                        const char *reason);
extern void purple_matrix_rust_set_room_tag(const char *user_id,
                                            const char *room_id,
                                            const char *tag_name);
extern void purple_matrix_rust_ignore_user(const char *user_id,
                                           const char *ignored_user_id);
extern void purple_matrix_rust_get_power_levels(const char *user_id,
                                                const char *room_id);
extern void purple_matrix_rust_set_power_level(const char *user_id,
                                               const char *room_id,
                                               const char *target_user_id,
                                               long long level);
extern void purple_matrix_rust_set_room_name(const char *user_id,
                                             const char *room_id,
                                             const char *name);
extern void purple_matrix_rust_set_room_topic(const char *user_id,
                                              const char *room_id,
                                              const char *topic);
extern void purple_matrix_rust_create_alias(const char *user_id,
                                            const char *room_id,
                                            const char *alias);
extern void purple_matrix_rust_delete_alias(const char *user_id,
                                            const char *alias);
extern void purple_matrix_rust_report_content(const char *user_id,
                                              const char *room_id,
                                              const char *event_id,
                                              const char *reason);
extern void purple_matrix_rust_export_room_keys(const char *user_id,
                                                const char *path,
                                                const char *passphrase);
extern void purple_matrix_rust_bulk_redact(const char *user_id,
                                           const char *room_id, int count,
                                           const char *reason);
extern void purple_matrix_rust_knock(const char *user_id,
                                     const char *room_id_or_alias,
                                     const char *reason);
extern void purple_matrix_rust_unban_user(const char *user_id,
                                          const char *room_id,
                                          const char *target_user_id,
                                          const char *reason);
extern void purple_matrix_rust_set_room_avatar(const char *user_id,
                                               const char *room_id,
                                               const char *filename);
extern void purple_matrix_rust_set_room_mute_state(const char *user_id,
                                                   const char *room_id,
                                                   bool muted);
extern void purple_matrix_rust_destroy_session(const char *user_id);

// Helper to extract and send images from libpurple's imgstore
static void check_and_send_pasted_images(PurpleConnection *gc, const char *who,
                                         const char *message) {
  const char *p = message;
  while ((p = strstr(p, "<img id=\""))) {
    p += 9;
    int id = atoi(p);
    if (id > 0) {
      PurpleStoredImage *img = purple_imgstore_find_by_id(id);
      if (img) {
        // Save to temp file
#define MATRIX_PASTED_PREFIX "matrix_pasted_"

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

// Global Plugin Data
static PurplePlugin *my_plugin = NULL;
static GHashTable *room_id_map = NULL; // Maps room_id (string) -> chat_id (int)
static int next_chat_id = 1;

static int get_chat_id(const char *room_id) {
  if (!room_id_map) {
    room_id_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  }

  gpointer val = g_hash_table_lookup(room_id_map, room_id);
  if (val) {
    return GPOINTER_TO_INT(val);
  }

  int new_id = next_chat_id++;
  g_hash_table_insert(room_id_map, g_strdup(room_id), GINT_TO_POINTER(new_id));
  return new_id;
}

static GList *matrix_status_types(PurpleAccount *account) {
  GList *types = NULL;
  PurpleStatusType *type;

  type = purple_status_type_new_with_attrs(
      PURPLE_STATUS_AVAILABLE, "available", NULL, TRUE, TRUE, FALSE, "message",
      _("Message"), purple_value_new(PURPLE_TYPE_STRING), NULL);
  types = g_list_append(types, type);

  type = purple_status_type_new_with_attrs(
      PURPLE_STATUS_AWAY, "away", NULL, TRUE, TRUE, FALSE, "message",
      _("Message"), purple_value_new(PURPLE_TYPE_STRING), NULL);
  types = g_list_append(types, type);

  type = purple_status_type_new(PURPLE_STATUS_OFFLINE, "offline", NULL, TRUE);
  types = g_list_append(types, type);

  return types;
}

extern void purple_matrix_rust_set_status(const char *user_id,
                                          MatrixStatus status, const char *msg);

static void matrix_set_status(PurpleAccount *account, PurpleStatus *status) {
  const char *id = purple_status_get_id(status);
  const char *message = purple_status_get_attr_string(status, "message");
  MatrixStatus mat_status = MATRIX_STATUS_ONLINE;

  if (!strcmp(id, "available")) {
    mat_status = MATRIX_STATUS_ONLINE;
  } else if (!strcmp(id, "away")) {
    mat_status = MATRIX_STATUS_UNAVAILABLE;
  } else if (!strcmp(id, "offline")) {
    mat_status = MATRIX_STATUS_OFFLINE;
  }

  purple_matrix_rust_set_status(purple_account_get_username(account),
                                mat_status, message);
}

// Structs to marshal data to main thread
typedef struct {
  char *sender;
  char *message;
  char *room_id;
  char *thread_root_id;
  char *event_id;
  guint64 timestamp;
} MatrixMsgData;

typedef struct {
  char *room_id;
  char *user_id;
  gboolean is_typing;
} MatrixTypingData;

// Helper to find the first matrix account
static PurpleAccount *find_matrix_account(void) {
  GList *accts = purple_accounts_get_all();
  while (accts) {
    PurpleAccount *a = (PurpleAccount *)accts->data;
    if (a && purple_account_get_protocol_id(a) &&
        strcmp(purple_account_get_protocol_id(a), "prpl-matrix-rust") == 0) {
      return a;
    }
    accts = accts->next;
  }
  return NULL;
}

static char *matrix_get_chat_name(GHashTable *components) {
  if (!components) {
    return NULL;
  }
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (!room_id) {
    return NULL;
  }
  return g_strdup(room_id);
}

static guint32 get_history_page_size(PurpleAccount *account) {
  const char *raw = purple_account_get_string(account, "history_page_size", "50");
  long n = raw ? strtol(raw, NULL, 10) : 50;
  if (n < 1)
    n = 1;
  if (n > 500)
    n = 500;
  return (guint32)n;
}

// Idle callback for Message (Runs on Main Thread)
// Idle callback for Message (Runs on Main Thread)
static gboolean process_msg_cb(gpointer data) {
  MatrixMsgData *d = (MatrixMsgData *)data;
  PurpleAccount *account = find_matrix_account();
  if (!account) {
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

  // Record Last Event ID and Thread Root for the room
  if (d->event_id) {
    PurpleConversation *main_conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, d->room_id, account);
    if (main_conv) {
      char *old_id = purple_conversation_get_data(main_conv, "last_event_id");
      g_free(old_id);
      purple_conversation_set_data(main_conv, "last_event_id",
                                   g_strdup(d->event_id));

      char *old_root =
          purple_conversation_get_data(main_conv, "last_thread_root_id");
      g_free(old_root);
      if (d->thread_root_id) {
        purple_conversation_set_data(main_conv, "last_thread_root_id",
                                     g_strdup(d->thread_root_id));
      } else {
        purple_conversation_set_data(main_conv, "last_thread_root_id", NULL);
      }
    }
  }

  // Decide target room ID (Main room or Virtual Thread Room)
  char *target_id = g_strdup(d->room_id);
  gboolean separate_threads =
      purple_account_get_bool(account, "separate_threads", FALSE);

  // If we have a thread ID, check if we should route to a virtual room
  if (d->thread_root_id && strlen(d->thread_root_id) > 0 && separate_threads) {
    // Construct Virtual ID: room_id|thread_id
    char *virtual_id = g_strdup_printf("%s|%s", d->room_id, d->thread_root_id);
    g_free(target_id);
    target_id = virtual_id; // Steal pointer
  }

  // Check for [System] tag (Notifications)
  if (g_str_has_prefix(d->message, "[System] ")) {
    const char *sys_msg = d->message + 9; // len("[System] ")
    PurpleConnection *gc_sys = purple_account_get_connection(account);
    if (gc_sys) {
      int chat_id = get_chat_id(target_id);
      PurpleConversation *conv = purple_find_chat(gc_sys, chat_id);
      if (conv) {
        purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "", sys_msg,
                               PURPLE_MESSAGE_SYSTEM, d->timestamp / 1000);
      }
    }
    g_free(target_id);
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

  // Check for [History] tag
  gboolean is_history = g_str_has_prefix(d->message, "[History] ");
  // We want to display history, but maybe mark it?
  // Current logic was DROPPING it. We will now allow it to proceed.

  if (is_history && d->thread_root_id && separate_threads) {
    // Still ensure threaded history is in BL for structure
    const char *clean_msg = d->message + 10;
    ensure_thread_in_blist(account, target_id, clean_msg, d->room_id);
  }

  // Ensure conversation exists and is joined
  PurpleConnection *gc = purple_account_get_connection(account);
  if (gc) {
    int chat_id = get_chat_id(target_id);
    PurpleConversation *conv = purple_find_chat(gc, chat_id);

    if (!conv) {
      // If not found by ID, check by name (just in case)
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT,
                                                   target_id, account);
    }

    // Ensure it is in the buddy list (grouped properly)
    if (d->thread_root_id && strlen(d->thread_root_id) > 0 &&
        separate_threads) {
      purple_debug_info(
          "purple-matrix-rust",
          "process_msg_cb calling ensure_thread_in_blist for %s\n", target_id);
      char *clean_alias = purple_markup_strip_html(d->message);
      ensure_thread_in_blist(account, target_id, clean_alias, d->room_id);
      g_free(clean_alias);
    }

    if (!conv) {
      // Create it properly
      serv_got_joined_chat(gc, chat_id, target_id);
      conv = purple_find_chat(gc, chat_id);

      if (conv) {
        // Keep stable human-friendly title from buddy list metadata, not
        // per-message content.
        PurpleChat *blist_chat = purple_blist_find_chat(account, target_id);
        if (blist_chat && blist_chat->alias) {
          purple_conversation_set_title(conv, blist_chat->alias);
        } else if (d->thread_root_id && separate_threads) {
          purple_conversation_set_title(conv, "Thread");
        }
      }
    } else {
      // Ensure ID is linked (important if found by name but not ID?)
      purple_conv_chat_set_id(PURPLE_CONV_CHAT(conv), chat_id);
    }

    // Add sender to chat users if not present (Fixes "doesn't show who is in
    // thread")
    if (conv) {
      if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), d->sender)) {
        purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), d->sender, NULL,
                                  PURPLE_CBFLAGS_NONE, TRUE);
      }
    }

    // Crosspost to Main Room if it's a thread
    // Crosspost to Main Room if it's a thread
    if (d->thread_root_id && separate_threads) {
      int main_id = get_chat_id(d->room_id);
      if (purple_find_chat(gc, main_id)) {
        char *main_msg = g_strdup_printf("[Thread] %s", d->message);
        serv_got_chat_in(gc, main_id, d->sender, PURPLE_MESSAGE_RECV, main_msg,
                         d->timestamp / 1000);
        g_free(main_msg);
      }
    }

    // Deliver message to target room
    serv_got_chat_in(gc, chat_id, d->sender, PURPLE_MESSAGE_RECV, d->message,
                     d->timestamp / 1000);
  }

  g_free(d->sender);
  g_free(d->message);
  g_free(d->room_id);
  if (d->thread_root_id)
    g_free(d->thread_root_id);
  if (d->event_id)
    g_free(d->event_id);
  g_free(d);
  g_free(target_id);

  return FALSE; // Remove source
}

static void msg_callback(const char *sender, const char *msg,
                         const char *room_id, const char *thread_root_id,
                         const char *event_id, guint64 timestamp) {
  MatrixMsgData *d = g_new0(MatrixMsgData, 1);
  d->sender = g_strdup(sender);
  d->message = g_strdup(msg);
  d->room_id = g_strdup(room_id);
  if (thread_root_id) {
    d->thread_root_id = g_strdup(thread_root_id);
  }
  if (event_id) {
    d->event_id = g_strdup(event_id);
  }
  d->timestamp = timestamp;
  g_idle_add(process_msg_cb, d);
}

// Idle callback for Typing
static gboolean process_typing_cb(gpointer data) {
  MatrixTypingData *d = (MatrixTypingData *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      // PurpleTypingState state = d->is_typing ? PURPLE_TYPING :
      // PURPLE_NOT_TYPING;

      purple_debug_info("purple-matrix-rust",
                        "Incoming typing: user=%s, room=%s, typing=%d\n",
                        d->user_id, d->room_id, d->is_typing);

      // Check if this room is already a Chat. If so, DO NOT call
      // serv_got_typing because that will open a separate IM window.
      int chat_id = get_chat_id(d->room_id);
      PurpleConversation *chat_conv = purple_find_chat(gc, chat_id);
      if (chat_conv) {
        // It is a chat. Libpurple doesn't standardly support "X is typing" in
        // chats via this API. Ignoring it is better than opening a new window.
        // purple_debug_info("purple-matrix-rust", "Ignoring typing for chat
        // room %s\n", d->room_id);
      } else {
        // It might be a pure DM (if we supported them as IMs).
        // However, we effectively treat all rooms as Chats currently.
        // But if no chat window is open/joined, serv_got_typing might be
        // useful? Actually, if we haven't joined the chat, we shouldn't get
        // typing events usually (unless syncing). For safety to fix the user
        // bug, we will suppress it if it looks like a Room ID. Or strictly:
        // Only if we verify it's NOT a chat.

        // Current architecture puts EVERYTHING in Chats. So we should probably
        // disable this globally until we implement distinct IM support.
        purple_debug_info("purple-matrix-rust",
                          "Suppressing typing for %s (avoiding IM window)\n",
                          d->user_id);
        // serv_got_typing(gc, d->user_id, 0, state); // DISABLED
      }
    }
  }

  g_free(d->room_id);
  g_free(d->user_id);
  g_free(d);

  return FALSE;
}

static void typing_callback(const char *room_id, const char *user_id,
                            gboolean is_typing) {
  MatrixTypingData *d = g_new0(MatrixTypingData, 1);
  d->room_id = g_strdup(room_id);
  d->user_id = g_strdup(user_id);
  d->is_typing = is_typing;
  g_idle_add(process_typing_cb, d);
}

// Room Joined Callback
typedef struct {
  char *room_id;
  char *name;
  char *group_name;
  char *avatar_url;
  char *topic;
  gboolean encrypted;
} MatrixRoomData;

static PurpleChat *find_chat_manual(PurpleAccount *account,
                                    const char *room_id) {
  PurpleBlistNode *gnode, *cnode;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
      continue;
    for (cnode = gnode->child; cnode; cnode = cnode->next) {
      if (!PURPLE_BLIST_NODE_IS_CHAT(cnode))
        continue;
      PurpleChat *chat = (PurpleChat *)cnode;
      if (purple_chat_get_account(chat) != account)
        continue;

      GHashTable *components = purple_chat_get_components(chat);
      if (!components)
        continue;
      const char *id = g_hash_table_lookup(components, "room_id");
      if (id && strcmp(id, room_id) == 0) {
        return chat;
      }
    }
  }
  return NULL;
}

static gboolean is_virtual_room_id(const char *room_id) {
  return room_id && strchr(room_id, '|') != NULL;
}

static char *derive_base_group_from_threads_group(const char *group_name) {
  if (!group_name || !*group_name) {
    return g_strdup("Matrix Rooms / Rooms");
  }
  const char *threads = strstr(group_name, " / Threads");
  if (!threads) {
    return g_strdup(group_name);
  }
  char *base = g_strndup(group_name, (gsize)(threads - group_name));
  // Input looks like "<space-or-tag> / Rooms / <room> / Threads".
  // Drop the trailing " / <room>" to get "<space-or-tag> / Rooms".
  char *last_sep = g_strrstr(base, " / ");
  if (last_sep) {
    *last_sep = '\0';
  }
  if (!*base) {
    g_free(base);
    return g_strdup("Matrix Rooms / Rooms");
  }
  return base;
}

static void cleanup_stale_thread_labels(PurpleAccount *account) {
  if (!account) {
    return;
  }
  PurpleBlistNode *gnode = NULL;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode)) {
      continue;
    }
    for (PurpleBlistNode *cnode = gnode->child; cnode; cnode = cnode->next) {
      if (!PURPLE_BLIST_NODE_IS_CHAT(cnode)) {
        continue;
      }
      PurpleChat *chat = (PurpleChat *)cnode;
      if (purple_chat_get_account(chat) != account) {
        continue;
      }

      GHashTable *components = purple_chat_get_components(chat);
      if (!components) {
        continue;
      }
      const char *room_id = g_hash_table_lookup(components, "room_id");
      if (!room_id || is_virtual_room_id(room_id)) {
        continue;
      }

      if (chat->alias && g_str_has_prefix(chat->alias, "Thread: ")) {
        const char *clean_alias = chat->alias + strlen("Thread: ");
        if (clean_alias && *clean_alias) {
          purple_blist_alias_chat(chat, clean_alias);
        }
      }

      PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
      if (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) {
        const char *group_name = purple_group_get_name((PurpleGroup *)parent);
        if (group_name && strstr(group_name, " / Threads")) {
          char *target_group_name =
              derive_base_group_from_threads_group(group_name);
          PurpleGroup *target_group = purple_find_group(target_group_name);
          if (!target_group) {
            target_group = purple_group_new(target_group_name);
            purple_blist_add_group(target_group, NULL);
          }
          purple_blist_add_chat(chat, target_group, NULL);
          g_free(target_group_name);
        }
      }
    }
  }
}

static gboolean process_room_cb(gpointer data) {
  MatrixRoomData *d = (MatrixRoomData *)data;
  PurpleAccount *account = find_matrix_account();

  // Normal room population must never create/update virtual thread chats.
  if (d->room_id && strchr(d->room_id, '|')) {
    purple_debug_warning("purple-matrix-rust",
                         "Skipping virtual thread id in process_room_cb: %s\n",
                         d->room_id);
    g_free(d->room_id);
    g_free(d->name);
    g_free(d->group_name);
    g_free(d->avatar_url);
    if (d->topic)
      g_free(d->topic);
    g_free(d);
    return FALSE;
  }

  // Determine key for group name (default to "Matrix Rooms" if NULL/Empty)
  const char *group_name = "Matrix Rooms";
  if (d->group_name && strlen(d->group_name) > 0) {
    group_name = d->group_name;
  }

  if (account) {
    purple_debug_info("purple-matrix-rust",
                      "Processing room: %s (%s) in Group: %s\n", d->room_id,
                      d->name, group_name);

    purple_debug_info("purple-matrix-rust", "Finding/Creating group '%s'\n",
                      group_name);
    PurpleGroup *group = purple_find_group(group_name);
    if (!group) {
      purple_debug_info("purple-matrix-rust", "Creating group '%s'\n",
                        group_name);
      group = purple_group_new(group_name);
      purple_blist_add_group(group, NULL);
    }

    purple_debug_info("purple-matrix-rust", "Checking if chat exists: %s\n",
                      d->room_id);
    PurpleChat *chat = find_chat_manual(account, d->room_id);
    if (!chat) {
      purple_debug_info("purple-matrix-rust", "Adding new chat to blist: %s\n",
                        d->room_id);
      GHashTable *components =
          g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert(components, g_strdup("room_id"),
                          g_strdup(d->room_id));
      if (d->avatar_url && strlen(d->avatar_url) > 0) {
        g_hash_table_insert(components, g_strdup("avatar_path"),
                            g_strdup(d->avatar_url));
      }
      if (d->topic && strlen(d->topic) > 0) {
        g_hash_table_insert(components, g_strdup("topic"), g_strdup(d->topic));
      }
      g_hash_table_insert(components, g_strdup("encrypted"),
                          g_strdup(d->encrypted ? "1" : "0"));

      chat = purple_chat_new(account, d->room_id, components);
      purple_blist_add_chat(chat, group, NULL);

      // Try to set the icon on the node (Pidgin/Adium often use this)
      if (d->avatar_url && strlen(d->avatar_url) > 0) {
        purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon",
                                     d->avatar_url);
        purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path",
                                     d->avatar_url);
      }
    } else {
      GHashTable *components = purple_chat_get_components(chat);
      if (d->avatar_url && strlen(d->avatar_url) > 0) {
        g_hash_table_replace(components, g_strdup("avatar_path"),
                             g_strdup(d->avatar_url));

        // Update Node Icon
        purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon",
                                     d->avatar_url);
        purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path",
                                     d->avatar_url);
      }
      if (d->topic && strlen(d->topic) > 0) {
        g_hash_table_replace(components, g_strdup("topic"), g_strdup(d->topic));
      }
      g_hash_table_replace(components, g_strdup("encrypted"),
                           g_strdup(d->encrypted ? "1" : "0"));

      // If an existing room is in a stale group (for example from an old
      // thread-grouping bug), move it to the current canonical group.
      PurpleBlistNode *chat_node = PURPLE_BLIST_NODE(chat);
      PurpleBlistNode *parent = chat_node ? chat_node->parent : NULL;
      const char *current_group_name = NULL;
      if (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) {
        current_group_name = purple_group_get_name((PurpleGroup *)parent);
      }
      if (!current_group_name || g_strcmp0(current_group_name, group_name) != 0) {
        purple_debug_info("purple-matrix-rust",
                          "Moving room %s from group '%s' to '%s'\n",
                          d->room_id,
                          current_group_name ? current_group_name : "(none)",
                          group_name);
        purple_blist_add_chat(chat, group, NULL);
      }
    }

    if (chat && d->name && strlen(d->name) > 0) {
      purple_blist_alias_chat(chat, d->name);
    }
  }

  g_free(d->room_id);
  g_free(d->name);
  g_free(d->group_name);
  g_free(d->avatar_url);
  if (d->topic)
    g_free(d->topic);
  g_free(d);
  return FALSE;
}

// Updated signature to take group_name and avatar_url
typedef void (*RoomJoinedCallback)(const char *room_id, const char *name,
                                   const char *group_name,
                                   const char *avatar_url, const char *topic,
                                   gboolean encrypted);
extern void purple_matrix_rust_set_room_joined_callback(RoomJoinedCallback cb);

static void room_joined_callback(const char *room_id, const char *name,
                                 const char *group_name, const char *avatar_url,
                                 const char *topic, gboolean encrypted) {
  MatrixRoomData *d = g_new0(MatrixRoomData, 1);
  d->room_id = g_strdup(room_id);
  d->name = g_strdup(name);
  if (group_name) {
    d->group_name = g_strdup(group_name);
  }
  if (avatar_url) {
    d->avatar_url = g_strdup(avatar_url);
  }
  if (topic) {
    d->topic = g_strdup(topic);
  }
  d->encrypted = encrypted;
  g_idle_add(process_room_cb, d);
}

static int matrix_chat_send(PurpleConnection *gc, int id, const char *message,
                            PurpleMessageFlags flags) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (!conv)
    return -1;

  const char *room_id = purple_conversation_get_name(conv);

  check_and_send_pasted_images(gc, room_id, message);

  // Local Echo: Display message immediately to user
  purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "me", message,
                         PURPLE_MESSAGE_SEND, time(NULL));

  purple_debug_info("purple-matrix-rust",
                    "matrix_chat_send: Sending to %s: %s\n", room_id, message);
  purple_matrix_rust_send_message(
      purple_account_get_username(purple_connection_get_account(gc)), room_id,
      message);
  return 0; // Success
}

static unsigned int matrix_send_typing(PurpleConnection *gc, const char *name,
                                       PurpleTypingState state) {
  // "name" might be user name in IM, but for chat we need conversation name.
  // Wait, send_typing signature: (PurpleConnection *gc, const char *name,
  // PurpleTypingState state) In IMs, name is the user. In Chats? Actually
  // prpl->send_typing is usually for IMs. For chats, it's often handled
  // differently or via the same callback if supported. Matrix treats mostly
  // everything as rooms. If we want typing in rooms, currently libpurple might
  // not invoke this for chats unless configured. checking `matrix-e2e`
  // reference...

  // For now, let's assume 'name' is the room (if it's a 1-1 chat disguised as
  // IM) or we look up the conversation.

  purple_debug_info("purple-matrix-rust",
                    "Outgoing typing: name=%s, state=%d\n", name, state);

  purple_matrix_rust_send_typing(
      purple_account_get_username(purple_connection_get_account(gc)), name,
      state == PURPLE_TYPING);
  return 0;
}

// Protocol Stubs

extern void purple_matrix_rust_get_user_info(const char *user_id,
                                             const char *target_user_id);
extern void purple_matrix_rust_set_show_user_info_callback(
    void (*cb)(const char *user_id, const char *display_name,
               const char *avatar_url, gboolean is_online));

typedef struct {
  char *user_id;
  char *display_name;
  char *avatar_url;
  gboolean is_online;
} MatrixUserInfoData;

static gboolean process_user_info_cb(gpointer data) {
  MatrixUserInfoData *d = (MatrixUserInfoData *)data;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    PurpleNotifyUserInfo *info = purple_notify_user_info_new();

    purple_notify_user_info_add_pair(info, "Matrix ID", d->user_id);
    if (d->display_name) {
      purple_notify_user_info_add_pair(info, "Display Name", d->display_name);
    }
    if (d->avatar_url) {
      purple_notify_user_info_add_pair(info, "Avatar URL", d->avatar_url);
    }

    purple_notify_user_info_add_pair(
        info, "Status", d->is_online ? "Online" : "Check Presence");

    purple_notify_userinfo(gc, d->user_id, info, NULL, NULL);
    purple_notify_user_info_destroy(info);
  }

  g_free(d->user_id);
  g_free(d->display_name);
  g_free(d->avatar_url);
  g_free(d);
  return FALSE;
}

static void show_user_info_cb(const char *user_id, const char *display_name,
                              const char *avatar_url, gboolean is_online) {
  MatrixUserInfoData *d = g_new0(MatrixUserInfoData, 1);
  d->user_id = g_strdup(user_id);
  d->display_name = g_strdup(display_name);
  d->avatar_url = g_strdup(avatar_url);
  d->is_online = is_online;
  g_idle_add(process_user_info_cb, d);
}

static void matrix_get_info(PurpleConnection *gc, const char *who) {
  purple_matrix_rust_get_user_info(
      purple_account_get_username(purple_connection_get_account(gc)), who);
}

static void matrix_set_chat_topic(PurpleConnection *gc, int id,
                                  const char *topic) {
  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv) {
    const char *room_id = purple_conversation_get_name(conv);
    purple_matrix_rust_set_room_topic(
        purple_account_get_username(purple_connection_get_account(gc)), room_id,
        topic);
  }
}

static PurpleRoomlist *active_roomlist = NULL;

typedef struct {
  char *name;
  char *id;
  char *topic;
  guint64 count;
} RoomListData;

static gboolean process_roomlist_add_cb(gpointer data) {
  RoomListData *d = (RoomListData *)data;
  if (active_roomlist) {
    PurpleRoomlistRoom *room =
        purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, d->name, NULL);
    purple_roomlist_room_add_field(active_roomlist, room, d->id);
    purple_roomlist_room_add_field(active_roomlist, room, d->topic);
    purple_roomlist_room_add(active_roomlist, room);
  }
  g_free(d->name);
  g_free(d->id);
  g_free(d->topic);
  g_free(d);
  return FALSE;
}

static void roomlist_add_cb(const char *name, const char *id, const char *topic,
                            guint64 count) {
  RoomListData *d = g_new0(RoomListData, 1);
  d->name = g_strdup(name);
  d->id = g_strdup(id);
  d->topic = g_strdup(topic);
  d->count = count;
  g_idle_add(process_roomlist_add_cb, d);
}

static PurpleRoomlist *matrix_roomlist_get_list(PurpleConnection *gc) {
  if (active_roomlist)
    purple_roomlist_unref(active_roomlist);
  active_roomlist = purple_roomlist_new(purple_connection_get_account(gc));

  GList *fields = NULL;
  PurpleRoomlistField *f;
  f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Room ID",
                                "room_id", FALSE);
  fields = g_list_append(fields, f);
  f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Topic", "topic",
                                FALSE);
  fields = g_list_append(fields, f);
  purple_roomlist_set_fields(active_roomlist, fields);

  purple_matrix_rust_fetch_public_rooms_for_list(
      purple_account_get_username(purple_connection_get_account(gc)));
  return active_roomlist;
}

static void matrix_roomlist_cancel(PurpleRoomlist *list) {
  if (list == active_roomlist) {
    active_roomlist = NULL;
  }
  purple_roomlist_unref(list);
}

// Thread List Logic

typedef struct {
  char *room_id;
  char *thread_root_id;
  char *latest_msg;
  guint64 count;
  guint64 ts;
} ThreadListItem;

typedef struct {
  char *room_id;
  GList *threads; // List of ThreadListItem*
} ThreadDisplayContext;

static GHashTable *pending_thread_lists = NULL;

static void free_thread_list_item(gpointer data) {
  ThreadListItem *item = (ThreadListItem *)data;
  g_free(item->room_id);
  g_free(item->thread_root_id);
  g_free(item->latest_msg);
  g_free(item);
}

static void free_thread_list_glist(gpointer data) {
  GList *list = (GList *)data;
  g_list_free_full(list, free_thread_list_item);
}

static void free_display_context(ThreadDisplayContext *ctx) {
  if (ctx) {
    g_free(ctx->room_id);
    g_list_free_full(ctx->threads, free_thread_list_item);
    g_free(ctx);
  }
}

static void thread_list_action_cb(void *user_data,
                                  PurpleRequestFields *fields) {
  ThreadDisplayContext *ctx = (ThreadDisplayContext *)user_data;
  PurpleRequestField *field =
      purple_request_fields_get_field(fields, "thread_id");

  // Get selected index
  int index = purple_request_field_choice_get_value(field);

  ThreadListItem *selected =
      (ThreadListItem *)g_list_nth_data(ctx->threads, index);

  if (selected && selected->thread_root_id) {
    char *join_id =
        g_strdup_printf("%s|%s", ctx->room_id, selected->thread_root_id);
    purple_matrix_rust_join_room(
        purple_account_get_username(find_matrix_account()), join_id);
    g_free(join_id);
  }

  free_display_context(ctx);
}

static void thread_list_close_cb(void *user_data) {
  ThreadDisplayContext *ctx = (ThreadDisplayContext *)user_data;
  free_display_context(ctx);
}

static gboolean process_thread_list_with_ui(gpointer data) {
  ThreadListItem *item = (ThreadListItem *)data;

  if (!pending_thread_lists) {
    pending_thread_lists = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, free_thread_list_glist);
  }

  GList *list = g_hash_table_lookup(pending_thread_lists, item->room_id);

  if (item->thread_root_id) {
    // Accumulate
    list = g_list_append(list, item);
    g_hash_table_replace(pending_thread_lists, g_strdup(item->room_id), list);
    return FALSE;
  } else {
    // End of list (Sentinel)
    // Build UI
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);

    PurpleRequestField *field = purple_request_field_choice_new(
        "thread_id", "Select a Thread to Join", 0);

    gboolean any = FALSE;
    for (GList *l = list; l != NULL; l = l->next) {
      ThreadListItem *t = (ThreadListItem *)l->data;
      if (t->thread_root_id) {
        // Label
        char *label = g_strdup_printf(
            "%s (Replies: %lu)", t->latest_msg ? t->latest_msg : "[No Preview]",
            t->count);
        purple_request_field_choice_add(field, label);
        g_free(label);
        any = TRUE;
      }
    }
    purple_request_field_group_add_field(group, field);

    if (any) {
      // Create context to pass to callback
      ThreadDisplayContext *ctx = g_new0(ThreadDisplayContext, 1);
      ctx->room_id = g_strdup(item->room_id);

      // Transfer ownership of list from Hash Table to Context
      g_hash_table_steal(pending_thread_lists, item->room_id);
      ctx->threads = list;

      purple_request_fields(find_matrix_account(), // handle
                            "Threads", "Active Threads",
                            "Select a thread to open it as a separate chat.",
                            fields, "Open", G_CALLBACK(thread_list_action_cb),
                            "Cancel", G_CALLBACK(thread_list_close_cb),
                            find_matrix_account(), NULL, NULL,
                            ctx // user_data
      );
    } else {
      purple_notify_info(NULL, "Threads", "No Active Threads",
                         "No threads found in recent history.");
      // Cleanup list since we stole it (or referenced it)
      // If we remove it now, the destroy notify works.
      g_hash_table_remove(pending_thread_lists, item->room_id);
    }

    // Free sentinel
    free_thread_list_item(item);

    return FALSE;
  }
}

static void thread_list_cb(const char *room_id, const char *thread_root_id,
                           const char *latest_msg, guint64 count, guint64 ts) {
  ThreadListItem *item = g_new0(ThreadListItem, 1);
  item->room_id = g_strdup(room_id);
  if (thread_root_id)
    item->thread_root_id = g_strdup(thread_root_id);
  if (latest_msg)
    item->latest_msg = g_strdup(latest_msg);
  item->count = count;
  item->ts = ts;
  g_idle_add(process_thread_list_with_ui, item);
}

static void matrix_unregister_user(PurpleAccount *account,
                                   PurpleAccountUnregistrationCb cb,
                                   void *user_data) {
  purple_matrix_rust_deactivate_account(TRUE);
  if (cb) {
    cb(account, TRUE, user_data);
  }
}

static void matrix_change_passwd(PurpleConnection *gc, const char *old_pass,
                                 const char *new_pass) {
  purple_matrix_rust_change_password(
      purple_account_get_username(purple_connection_get_account(gc)), old_pass,
      new_pass);
}

static void matrix_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                             PurpleGroup *group) {
  const char *name = purple_buddy_get_name(buddy);
  purple_matrix_rust_add_buddy(
      purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void matrix_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                                PurpleGroup *group) {
  const char *name = purple_buddy_get_name(buddy);
  purple_matrix_rust_remove_buddy(
      purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void matrix_set_idle(PurpleConnection *gc, int time) {
  purple_matrix_rust_set_idle(
      purple_account_get_username(purple_connection_get_account(gc)), time);
}

static PurpleCmdRet cmd_sticker(PurpleConversation *conv, const gchar *cmd,
                                gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_sticker <url_or_path>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_send_sticker(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static void matrix_add_deny(PurpleConnection *gc, const char *name) {
  purple_matrix_rust_ignore_user(
      purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void matrix_rem_deny(PurpleConnection *gc, const char *name) {
  purple_matrix_rust_unignore_user(
      purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void matrix_set_public_alias(PurpleConnection *gc, const char *alias,
                                    void *success_cb, void *failure_cb,
                                    void *data) {
  // Cast callbacks to void* to avoid strict typedef dependency if headers
  // missing In strict libpurple, success_cb is
  // PurpleSetPublicAliasSuccessCallback
  purple_matrix_rust_set_display_name(
      purple_account_get_username(purple_connection_get_account(gc)), alias);

  // Call success callback immediately (optimistic)
  typedef void (*SuccessCb)(PurpleAccount *, const char *, void *);
  if (success_cb) {
    ((SuccessCb)success_cb)(purple_connection_get_account(gc), alias, data);
  }
}

static void matrix_set_buddy_icon(PurpleConnection *gc,
                                  PurpleStoredImage *img) {
  if (!img)
    return;
  gconstpointer data = purple_imgstore_get_data(img);
  size_t len = purple_imgstore_get_size(img);
  purple_matrix_rust_set_avatar_bytes(
      purple_account_get_username(purple_connection_get_account(gc)),
      (const unsigned char *)data, len);
}

// Poll List Logic

typedef struct {
  char *room_id;
  char *event_id;
  char *sender;
  char *question;
  char *options_str;
} PollListItem;

static GHashTable *pending_poll_lists = NULL;

static void free_poll_list_item(gpointer data) {
  PollListItem *item = (PollListItem *)data;
  g_free(item->room_id);
  g_free(item->event_id);
  g_free(item->sender);
  g_free(item->question);
  g_free(item->options_str);
  g_free(item);
}

static void free_poll_list_glist(gpointer data) {
  GList *list = (GList *)data;
  g_list_free_full(list, free_poll_list_item);
}

extern void purple_matrix_rust_poll_vote(const char *user_id,
                                         const char *room_id,
                                         const char *event_id,
                                         const char *option,
                                         const char *option_index);

typedef struct {
  char *room_id;
  char *event_id;
  char *options_str;
} PollVoteContext;

static void poll_vote_action_cb(void *user_data, PurpleRequestFields *fields) {
  PollVoteContext *ctx = (PollVoteContext *)user_data;
  PurpleRequestField *field =
      purple_request_fields_get_field(fields, "option_idx");
  int index = purple_request_field_choice_get_value(field);

  char **options = g_strsplit(ctx->options_str, "|", -1);
  int count = 0;
  while (options[count])
    count++;

  if (index >= 0 && index < count) {
    char *opt_full = options[index];
    char *caret = strchr(opt_full, '^');
    char *answer_id = opt_full;
    if (caret) {
      *caret = '\0'; // Terminate ID part
    }

    // We pass NULL for option_text (4th arg) as we don't display it here?
    // Wait, poll_vote takes (user, room, poll_id, option_text, answer_id).
    // The previous code passed selected_opt as option_text, and idx_str as
    // answer_id. Rust code now treats 5th arg as answer_id. 4th arg is ignored
    // in Rust (`_option_text`).

    purple_matrix_rust_poll_vote(
        purple_account_get_username(find_matrix_account()), ctx->room_id,
        ctx->event_id, "ignored", answer_id);
  }
  g_strfreev(options);

  g_free(ctx->room_id);
  g_free(ctx->event_id);
  g_free(ctx->options_str);
  g_free(ctx);
}

static void poll_vote_cancel_cb(void *user_data) {
  PollVoteContext *ctx = (PollVoteContext *)user_data;
  g_free(ctx->room_id);
  g_free(ctx->event_id);
  g_free(ctx->options_str);
  g_free(ctx);
}

typedef struct {
  char *room_id;
  GList *polls;
} PollDisplayContext;

static void free_poll_display_context(PollDisplayContext *ctx) {
  if (ctx) {
    g_free(ctx->room_id);
    g_list_free_full(ctx->polls, free_poll_list_item);
    g_free(ctx);
  }
}

static void show_vote_ui_cb(void *user_data, PurpleRequestFields *fields) {
  PollDisplayContext *ctx = (PollDisplayContext *)user_data;
  PurpleRequestField *field =
      purple_request_fields_get_field(fields, "poll_id");
  int index = purple_request_field_choice_get_value(field);

  PollListItem *item = (PollListItem *)g_list_nth_data(ctx->polls, index);

  if (item) {
    // Show Vote UI
    PurpleRequestFields *v_fields = purple_request_fields_new();
    PurpleRequestFieldGroup *v_group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(v_fields, v_group);

    PurpleRequestField *v_field =
        purple_request_field_choice_new("option_idx", "Choose an Option", 0);

    char **options = g_strsplit(item->options_str, "|", -1);
    for (int i = 0; options[i] != NULL; i++) {
      char *caret = strchr(options[i], '^');
      if (caret) {
        purple_request_field_choice_add(v_field, caret + 1);
      } else {
        purple_request_field_choice_add(v_field, options[i]);
      }
    }
    g_strfreev(options);

    purple_request_field_group_add_field(v_group, v_field);

    PollVoteContext *v_ctx = g_new0(PollVoteContext, 1);
    v_ctx->room_id = g_strdup(item->room_id);
    v_ctx->event_id = g_strdup(item->event_id);
    v_ctx->options_str = g_strdup(item->options_str);

    purple_request_fields(find_matrix_account(), "Vote", item->question,
                          "Select your choice and click Vote.", v_fields,
                          "Vote", G_CALLBACK(poll_vote_action_cb), "Cancel",
                          G_CALLBACK(poll_vote_cancel_cb),
                          find_matrix_account(), NULL, NULL, v_ctx);
  }

  free_poll_display_context(ctx);
}

static void poll_list_close_cb(void *user_data) {
  PollDisplayContext *ctx = (PollDisplayContext *)user_data;
  free_poll_display_context(ctx);
}

static gboolean process_poll_list_with_ui(gpointer data) {
  PollListItem *item = (PollListItem *)data;

  if (!pending_poll_lists) {
    pending_poll_lists = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                               free_poll_list_glist);
  }

  GList *list = g_hash_table_lookup(pending_poll_lists, item->room_id);

  if (item->event_id) {
    // Accumulate
    list = g_list_append(list, item);
    g_hash_table_replace(pending_poll_lists, g_strdup(item->room_id), list);
    return FALSE;
  } else {
    // End Sentinel
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);

    PurpleRequestField *field =
        purple_request_field_choice_new("poll_id", "Select a Poll", 0);

    gboolean any = FALSE;
    for (GList *l = list; l != NULL; l = l->next) {
      PollListItem *p = (PollListItem *)l->data;
      if (p->event_id) {
        // Label: Question (Sender)
        char *label = g_strdup_printf("%s (by %s)", p->question, p->sender);
        purple_request_field_choice_add(field, label);
        g_free(label);
        any = TRUE;
      }
    }
    purple_request_field_group_add_field(group, field);

    if (any) {
      PollDisplayContext *ctx = g_new0(PollDisplayContext, 1);
      ctx->room_id = g_strdup(item->room_id);
      g_hash_table_steal(pending_poll_lists, item->room_id);
      ctx->polls = list;

      purple_request_fields(find_matrix_account(), "Active Polls",
                            "Available Polls", "Select a poll to vote.", fields,
                            "Select", G_CALLBACK(show_vote_ui_cb), "Cancel",
                            G_CALLBACK(poll_list_close_cb),
                            find_matrix_account(), NULL, NULL, ctx);
    } else {
      purple_notify_info(NULL, "Polls", "No Active Polls",
                         "No active polls found in recent history.");
      g_hash_table_remove(pending_poll_lists, item->room_id);
    }

    free_poll_list_item(item);
    return FALSE;
  }
}

static void poll_list_cb(const char *room_id, const char *event_id,
                         const char *sender, const char *question,
                         const char *options_str) {
  PollListItem *item = g_new0(PollListItem, 1);
  item->room_id = g_strdup(room_id);
  if (event_id)
    item->event_id = g_strdup(event_id);
  if (sender)
    item->sender = g_strdup(sender);
  if (question)
    item->question = g_strdup(question);
  if (options_str)
    item->options_str = g_strdup(options_str);

  g_idle_add(process_poll_list_with_ui, item);
}

static void matrix_login(PurpleAccount *account) {
  PurpleConnection *gc = purple_account_get_connection(account);
  const char *username = purple_account_get_username(account);
  const char *password = purple_account_get_password(account);
  char *homeserver_derived = NULL;
  const char *homeserver = purple_account_get_string(account, "server", NULL);

  // Check if homeserver is unset OR matches the generic default
  if (!homeserver || strcmp(homeserver, "https://matrix.org") == 0 ||
      strlen(homeserver) == 0) {
    const char *delimiter = strchr(username, ':');
    if (delimiter) {
      const char *domain = delimiter + 1;
      // If the domain is NOT matrix.org, override the default
      if (g_ascii_strcasecmp(domain, "matrix.org") != 0) {
        homeserver_derived = g_strdup_printf("https://%s", domain);
        homeserver = homeserver_derived;
        purple_debug_info("purple-matrix-rust",
                          "Overriding default homeserver with derived: %s\n",
                          homeserver);
      }
    }
  }

  // Fallback if still null
  if (!homeserver) {
    homeserver = "https://matrix.org";
  }

  purple_connection_update_progress(gc, "Connecting to Matrix...", 1, 5);
  purple_connection_set_state(gc, PURPLE_CONNECTING);

  purple_connection_update_progress(gc, "Connecting to Matrix...", 1, 5);
  purple_connection_set_state(gc, PURPLE_CONNECTING);

  // Persistent data directory
  const char *user_dir = purple_user_dir();
  char *safe_username = g_strdup(username);
  // Sanitize username for path (replace : with _)
  for (char *p = safe_username; *p; p++) {
    if (*p == ':' || *p == '/' || *p == '\\')
      *p = '_';
  }

  char *data_dir =
      g_build_filename(user_dir, "matrix_rust_data", safe_username, NULL);

  // Ensure directory exists
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) {
    g_mkdir_with_parents(data_dir, 0700);
  }

  purple_debug_info("purple-matrix-rust", "Using data directory: %s\n",
                    data_dir);

  int success =
      purple_matrix_rust_login(username, password, homeserver, data_dir);

  g_free(safe_username);
  g_free(data_dir);

  if (homeserver_derived) {
    g_free(homeserver_derived);
  } else {
    purple_debug_info("purple-matrix-rust", "Using configured homeserver: %s\n",
                      homeserver);
  }

  if (success == 1) {
    purple_connection_set_state(gc, PURPLE_CONNECTED);
    purple_debug_info("purple-matrix-rust", "Connected successfully!\n");
  } else if (success == 2) {
    purple_debug_info(
        "purple-matrix-rust",
        "SSO Login started. Keeping connection in connecting state.\n");
    // Do NOT set connected yet. Wait for finish_sso callback loop.
  } else {
    purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                   "Failed to login");
  }
}

static void matrix_close(PurpleConnection *gc) {
  // interactions to teardown rust client
  purple_debug_info("purple-matrix-rust", "Closing connection\n");
  // Normal disconnect must not invalidate the server session or delete session
  // storage; doing so causes reconnect/login loops.
  purple_matrix_rust_logout(
      purple_account_get_username(purple_connection_get_account(gc)));
}

static const char *matrix_list_icon(PurpleAccount *account,
                                    PurpleBuddy *buddy) {
  return "matrix"; // Must return a valid string, not NULL, to avoid GTK crashes
}

// Emblems for Chats
static const char *matrix_chat_list_emblem(PurpleChat *chat) {
  GHashTable *components = purple_chat_get_components(chat);
  const char *enc = g_hash_table_lookup(components, "encrypted");
  if (enc && strcmp(enc, "1") == 0) {
    return "secure"; // Standard libpurple emblem name for encrypted
  }
  return NULL;
}

// Status text for Chats (shown in blist)
static char *matrix_chat_status_text(PurpleChat *chat) {
  GHashTable *components = purple_chat_get_components(chat);
  const char *topic = g_hash_table_lookup(components, "topic");
  if (topic && strlen(topic) > 0) {
    // Strip HTML for blist display
    return purple_markup_strip_html(topic);
  }
  return NULL;
}

// Tooltip for Chats
static void matrix_chat_tooltip_text(PurpleChat *chat,
                                     PurpleNotifyUserInfo *user_info,
                                     gboolean full) {
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  const char *topic = g_hash_table_lookup(components, "topic");
  const char *enc = g_hash_table_lookup(components, "encrypted");

  if (room_id) {
    purple_notify_user_info_add_pair(user_info, "Room ID", room_id);
  }

  if (enc && strcmp(enc, "1") == 0) {
    purple_notify_user_info_add_pair(user_info, "Security",
                                     "End-to-End Encrypted");
  } else {
    purple_notify_user_info_add_pair(user_info, "Security",
                                     "Unencrypted / Public");
  }

  if (topic && strlen(topic) > 0) {
    char *clean_topic = purple_markup_strip_html(topic);
    purple_notify_user_info_add_pair(user_info, "Topic", clean_topic);
    g_free(clean_topic);
  }
}

static const char *matrix_list_emblem(PurpleBuddy *buddy) {
  if (PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)) {
    return matrix_chat_list_emblem((PurpleChat *)buddy);
  }
  return NULL;
}

static char *matrix_status_text(PurpleBuddy *buddy) {
  if (PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)) {
    return matrix_chat_status_text((PurpleChat *)buddy);
  }
  // For buddies, show status message.
  PurpleStatus *status =
      purple_presence_get_active_status(purple_buddy_get_presence(buddy));
  const char *msg = purple_status_get_attr_string(status, "message");
  if (msg)
    return g_strdup(msg);
  return NULL;
}

static void matrix_tooltip_text(PurpleBuddy *buddy,
                                PurpleNotifyUserInfo *user_info,
                                gboolean full) {
  if (PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)) {
    matrix_chat_tooltip_text((PurpleChat *)buddy, user_info, full);
    return;
  }
  const char *name = purple_buddy_get_name(buddy);

  purple_notify_user_info_add_pair(user_info, "Matrix ID", name);

  PurpleStatus *status =
      purple_presence_get_active_status(purple_buddy_get_presence(buddy));
  const char *msg = purple_status_get_attr_string(status, "message");
  if (msg) {
    purple_notify_user_info_add_pair(user_info, "Status Message", msg);
  }
}

static GList *matrix_chat_info(PurpleConnection *gc) {
  GList *m = NULL;
  struct proto_chat_entry *pce;

  pce = g_new0(struct proto_chat_entry, 1);
  pce->label = _("Room ID");
  pce->identifier = "room_id";
  pce->required = TRUE;
  m = g_list_append(m, pce);

  return m;
}

static char *matrix_get_chat_name(GHashTable *components);
static GList *blist_node_menu_cb(PurpleBlistNode *node);

static void matrix_chat_leave(PurpleConnection *gc, int id) {
  // Pidgin calls chat_leave when closing a chat window/tab.
  // Do not map that to a Matrix room leave; leaving must stay explicit
  // (e.g. /matrix_leave).
  purple_debug_info("purple-matrix-rust",
                    "chat_leave for id %d ignored (local close only)\n", id);
}

static void matrix_chat_invite(PurpleConnection *gc, int id,
                               const char *message, const char *name) {
  purple_debug_info("purple-matrix-rust", "Inviting %s to chat id %d: %s\n",
                    name, id, message);

  PurpleConversation *conv = purple_find_chat(gc, id);
  if (conv) {
    const char *room_id = purple_conversation_get_name(conv);
    // 'name' is the user to invite
    purple_matrix_rust_invite_user(
        purple_account_get_username(purple_connection_get_account(gc)), room_id,
        name);
  }
}

static void matrix_chat_whisper(PurpleConnection *gc, int id, const char *who,
                                const char *message) {
  // Stub - Matrix doesn't typically do in-room whispers like IRC
}

static GHashTable *matrix_chat_info_defaults(PurpleConnection *gc,
                                             const char *chat_name) {
  GHashTable *defaults =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  if (chat_name) {
    g_hash_table_insert(defaults, g_strdup("room_id"), g_strdup(chat_name));
  }
  return defaults;
}

extern void purple_matrix_rust_join_room(const char *user_id,
                                         const char *room_id);

extern void purple_matrix_rust_send_im(const char *user_id,
                                       const char *target_id, const char *text);
extern void purple_matrix_rust_fetch_room_members(const char *user_id,
                                                  const char *room_id);

static int matrix_send_im(PurpleConnection *gc, const char *who,
                          const char *message, PurpleMessageFlags flags) {
  // Logic for sending a DM.
  // In Matrix, a DM is just a room with that person.

  check_and_send_pasted_images(gc, who, message);

  if (who[0] == '!') {
    // It's a room ID. Just send message.
    purple_matrix_rust_send_message(
        purple_account_get_username(purple_connection_get_account(gc)), who,
        message);
    return 1;
  }

  // It's likely a User ID (e.g. @alice:example.com)
  // Call our new Rust function that handles get_or_create_dm
  purple_matrix_rust_send_im(
      purple_account_get_username(purple_connection_get_account(gc)), who,
      message);
  return 1;
}

static void matrix_join_chat(PurpleConnection *gc, GHashTable *data) {
  const char *room_id = g_hash_table_lookup(data, "room_id");
  PurpleAccount *account = purple_connection_get_account(gc);

  if (!room_id)
    return;

  // Call Rust to actually join!
  purple_matrix_rust_join_room(
      purple_account_get_username(purple_connection_get_account(gc)), room_id);
  purple_matrix_rust_fetch_room_members(
      purple_account_get_username(purple_connection_get_account(gc)), room_id);

  // Generate a stable small integer ID for the chat using our map
  // This avoids large hash values causing "value out of range" errors in GTK
  int chat_id = get_chat_id(room_id);

  // Check if we are already in the chat (core knows about it)
  PurpleConversation *conv = purple_find_chat(gc, chat_id);
  if (!conv) {
    purple_debug_info(
        "purple-matrix-rust",
        "Joining chat %s (id: %d) - Calling serv_got_joined_chat\n", room_id,
        chat_id);

    serv_got_joined_chat(gc, chat_id, room_id);
    purple_debug_info("purple-matrix-rust", "serv_got_joined_chat returned\n");

    // Add ourselves to the user list so the UI knows we are there
    conv = purple_find_chat(gc, chat_id);
    if (conv) {
      PurpleChat *blist_chat = purple_blist_find_chat(account, room_id);
      if (blist_chat && blist_chat->alias) {
        purple_conversation_set_title(conv, blist_chat->alias);
      }

      const char *username = purple_account_get_username(account);
      purple_debug_info("purple-matrix-rust", "Adding user %s to chat\n",
                        username);
      purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), username, NULL,
                                PURPLE_CBFLAGS_NONE, FALSE);
      purple_debug_info("purple-matrix-rust", "User added to chat\n");
    } else {
      purple_debug_error("purple-matrix-rust",
                         "Failed to find conversation after joining!\n");
    }
  } else {
    // Just focus it
    if (conv)
      purple_conversation_present(conv);
  }
}

static void matrix_send_file(PurpleConnection *gc, const char *who,
                             const char *filename) {
  // If file is NULL, Libpurple might be asking IS it supported, or expecting
  // Xfer. Actually send_file(gc, who, file) is called when user drags file.

  if (!filename || !who)
    return;
  purple_debug_info("purple-matrix-rust", "Sending file %s to %s\n", filename,
                    who);
  purple_matrix_rust_send_file(
      purple_account_get_username(purple_connection_get_account(gc)), who,
      filename);
}

extern void purple_matrix_rust_send_reply(const char *user_id,
                                          const char *room_id,
                                          const char *event_id,
                                          const char *text);
extern void purple_matrix_rust_send_edit(const char *user_id,
                                         const char *room_id,
                                         const char *event_id,
                                         const char *text);

static PurpleCmdRet cmd_reply(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error = g_strdup("No message to reply to.");
    return PURPLE_CMD_RET_FAILED;
  }

  char *msg = g_strjoinv(" ", args);
  const char *room_id = purple_conversation_get_name(conv);
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_send_reply(purple_account_get_username(account), room_id,
                                last_event_id, msg);
  g_free(msg);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_edit(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  // For now, edit the last EVENT ID we saw.
  // Ideally we should track "last_self_event_id".
  // But for MVP, assume we edits the last received (if echo'd) or just fail if
  // it's not ours (server will reject).
  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error = g_strdup("No message to edit.");
    return PURPLE_CMD_RET_FAILED;
  }

  char *msg = g_strjoinv(" ", args);
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_send_edit(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, last_event_id, msg);
  g_free(msg);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_nick(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /nick <display_name>");
    return PURPLE_CMD_RET_FAILED;
  }

  purple_matrix_rust_set_display_name(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_avatar(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /avatar <path_to_image>");
    return PURPLE_CMD_RET_FAILED;
  }

  purple_matrix_rust_set_avatar(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_room_avatar(PurpleConversation *conv,
                                        const gchar *cmd, gchar **args,
                                        gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_avatar <path_to_image>");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_set_room_avatar(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unban(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_unban <user_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_unban_user(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_mute(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_set_room_mute_state(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, true);
  purple_conversation_write(conv, "System", "Muting room...",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_unmute(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_set_room_mute_state(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, false);
  purple_conversation_write(conv, "System", "Unmuting room...",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_logout(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  purple_matrix_rust_destroy_session(
      purple_account_get_username(purple_conversation_get_account(conv)));
  purple_conversation_write(conv, "System",
                            "Logging out explicitly... Session invalidated.",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));

  // Optionally close connection?
  // purple_account_disconnect(purple_conversation_get_account(conv));

  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reconnect(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  PurpleConnection *gc = purple_account_get_connection(account);

  if (gc) {
    PurpleConnectionState state = purple_connection_get_state(gc);
    if (state == PURPLE_CONNECTED) {
      purple_conversation_write(conv, "System", "Already connected.",
                                PURPLE_MESSAGE_SYSTEM, time(NULL));
      return PURPLE_CMD_RET_OK;
    }
    if (state == PURPLE_CONNECTING) {
      purple_conversation_write(conv, "System", "Connection attempt already in progress.",
                                PURPLE_MESSAGE_SYSTEM, time(NULL));
      return PURPLE_CMD_RET_OK;
    }
  }

  purple_account_connect(account);
  purple_conversation_write(conv, "System", "Reconnecting account...",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static char *dup_base_room_id(const char *raw_room_id);

static PurpleCmdRet cmd_leave(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  const char *raw_room_id = purple_conversation_get_name(conv);
  if (!raw_room_id || !*raw_room_id) {
    *error = g_strdup("No room is selected.");
    return PURPLE_CMD_RET_FAILED;
  }

  // If called from a thread virtual chat (room_id|thread_id), leave only the
  // base room.
  char *room_id = dup_base_room_id(raw_room_id);

  purple_matrix_rust_leave_room(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id);
  purple_conversation_write(conv, "System", "Leaving room...",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  g_free(room_id);
  return PURPLE_CMD_RET_OK;
}

static char *dup_base_room_id(const char *raw_room_id) {
  if (!raw_room_id) {
    return NULL;
  }
  char *room_id = g_strdup(raw_room_id);
  char *pipe = strchr(room_id, '|');
  if (pipe) {
    *pipe = '\0';
  }
  return room_id;
}

static PurpleCmdRet cmd_list_threads(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  const char *raw_room_id = purple_conversation_get_name(conv);
  if (!raw_room_id || !*raw_room_id) {
    *error = g_strdup("No room is selected.");
    return PURPLE_CMD_RET_FAILED;
  }
  char *room_id = dup_base_room_id(raw_room_id);
  purple_matrix_rust_list_threads(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id);
  purple_conversation_write(conv, "System", "Loading thread list...",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  g_free(room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_separate_threads(PurpleConversation *conv,
                                             const gchar *cmd, gchar **args,
                                             gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_separate_threads <on|off>");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  gboolean enabled = FALSE;
  if (g_ascii_strcasecmp(args[0], "on") == 0 ||
      g_ascii_strcasecmp(args[0], "true") == 0 ||
      strcmp(args[0], "1") == 0) {
    enabled = TRUE;
  } else if (g_ascii_strcasecmp(args[0], "off") == 0 ||
             g_ascii_strcasecmp(args[0], "false") == 0 ||
             strcmp(args[0], "0") == 0) {
    enabled = FALSE;
  } else {
    *error = g_strdup("Value must be 'on' or 'off'.");
    return PURPLE_CMD_RET_FAILED;
  }

  purple_account_set_bool(account, "separate_threads", enabled);
  purple_conversation_write(
      conv, "System",
      enabled ? "Separate thread tabs enabled."
              : "Separate thread tabs disabled.",
      PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_help(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  GString *msg = g_string_new("");
  g_string_append(msg, "<font size='4'><b>Matrix Commands</b></font><br>");

  g_string_append(msg, "<br><b>General</b><br>");
  g_string_append(msg, "&bull; <b>/join &lt;room_id&gt;</b>: Join a room<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_invite &lt;user_id&gt;</b>: Invite a user<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_nick &lt;name&gt;</b>: Set display name<br>");
  g_string_append(msg, "&bull; <b>/matrix_avatar &lt;path&gt;</b>: Set avatar<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_create_room &lt;name&gt;</b>: Create a room<br>");
  g_string_append(msg, "&bull; <b>/matrix_leave [reason]</b>: Leave room<br>");
  g_string_append(msg, "&bull; <b>/matrix_reconnect</b>: Reconnect account<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_public_rooms [term]</b>: Search directory<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_user_search &lt;term&gt;</b>: Search users<br>");
  g_string_append(msg, "&bull; <b>/matrix_preview_room &lt;id_or_alias&gt;</b>: "
                       "Preview a room<br>");

  g_string_append(msg, "<br><b>Messages &amp; Interaction</b><br>");
  g_string_append(
      msg,
      "&bull; <b>/matrix_reply &lt;msg&gt;</b>: Reply to last message<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_thread &lt;msg&gt;</b>: Reply in thread<br>");
  g_string_append(msg, "&bull; <b>/matrix_threads</b>: Browse active threads in this room<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_set_separate_threads &lt;on|off&gt;</b>: Toggle separate thread tabs<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_edit &lt;msg&gt;</b>: Edit last message<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_react &lt;emoji&gt;</b>: React to message<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_redact [reason]</b>: Redact last message<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_set_history_page_size &lt;1-500&gt;</b>: "
                  "Set history page size<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_set_auto_fetch_history "
                  "&lt;on|off&gt;</b>: Toggle auto history sync<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_sticker &lt;url&gt;</b>: Send sticker<br>");
  g_string_append(msg, "&bull; <b>/matrix_location &lt;desc&gt; "
                       "&lt;geo&gt;</b>: Send location<br>");

  g_string_append(msg, "<br><b>Polls</b><br>");
  g_string_append(msg, "&bull; <b>/matrix_poll_create &lt;q&gt; "
                       "&lt;opts&gt;</b>: Create poll<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_poll_vote &lt;idx&gt;</b>: Vote on poll<br>");
  g_string_append(msg, "&bull; <b>/matrix_poll_end</b>: End poll<br>");

  g_string_append(msg, "<br><b>Moderation &amp; Admin</b><br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_kick &lt;user&gt; [reason]</b>: Kick user<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_ban &lt;user&gt; [reason]</b>: Ban user<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_unban &lt;user&gt;</b>: Unban user<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_set_name &lt;name&gt;</b>: Set room name<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_set_topic &lt;topic&gt;</b>: Set room topic<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_tag &lt;tag&gt;</b>: Set room tag<br>");
  g_string_append(
      msg, "&bull; <b>/matrix_alias_create/delete</b>: Manage aliases<br>");

  g_string_append(msg, "<br><b>Security</b><br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_verify &lt;user&gt;</b>: Verify user<br>");
  g_string_append(msg,
                  "&bull; <b>/matrix_e2ee_status</b>: Check E2EE status<br>");
  g_string_append(msg, "&bull; <b>/matrix_export_keys</b>: Export keys<br>");
  g_string_append(msg, "&bull; <b>/matrix_recover_keys &lt;passphrase&gt;</b>: "
                       "Recover keys from backup<br>");
  g_string_append(msg, "&bull; <b>/matrix_reconnect</b>: Reconnect after session issues<br>");
  g_string_append(msg, "&bull; <b>/matrix_logout</b>: Logout and invalidate "
                       "session<br>");

  purple_conversation_write(conv, "Matrix Help", msg->str,
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  g_string_free(msg, TRUE);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_location(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0] || !args[1] || !*args[1]) {
    *error = g_strdup("Usage: /matrix_location <description> <geo_uri>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_send_location(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_create(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0] || !args[1] || !*args[1]) {
    *error = g_strdup(
        "Usage: /matrix_poll_create <question> <options_pipe_separated>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_poll_create(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_vote(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_poll_vote <option_index>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  // Ideally we track last_poll_id separately or require user to reply to it.
  // For MVP, reply-to or last-event.
  if (!last_event_id) {
    *error = g_strdup("No poll found to vote on (reply or receive one).");
    return PURPLE_CMD_RET_FAILED;
  }

  int index = atoi(args[0]);
  if (index <= 0) {
    *error = g_strdup("Index must be positive.");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *room_id = purple_conversation_get_name(conv);
  char *idx_str = g_strdup_printf("%d", index);
  purple_matrix_rust_poll_vote(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, last_event_id, "Unknown", idx_str);
  g_free(idx_str);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_poll_end(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error = g_strdup("No poll found to end.");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_poll_end(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, last_event_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_verify(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_verify <user_id>");
    return PURPLE_CMD_RET_FAILED;
  }

  purple_matrix_rust_verify_user(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_e2ee_status(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_e2ee_status(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_reset_cross_signing(PurpleConversation *conv,
                                            const gchar *cmd, gchar **args,
                                            gchar **error, void *data) {
  purple_matrix_rust_bootstrap_cross_signing(
      purple_account_get_username(purple_conversation_get_account(conv)));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_recover_keys(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_recover_keys <passphrase>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_recover_keys(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_read_receipt(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error = g_strdup("No message to mark as read.");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_send_read_receipt(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, last_event_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_react(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_react <emoji>");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error = g_strdup("No message to react to.");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_send_reaction(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, last_event_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_redact(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error = g_strdup("No message to redact.");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *room_id = purple_conversation_get_name(conv);
  char *reason = args[0] ? g_strjoinv(" ", args) : NULL;
  purple_matrix_rust_redact_event(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, last_event_id, reason);
  g_free(reason);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_history(PurpleConversation *conv, const gchar *cmd,
                                gchar **args, gchar **error, void *data) {
  const char *room_id = purple_conversation_get_name(conv);
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_fetch_more_history_with_limit(
      purple_account_get_username(account), room_id,
      get_history_page_size(account));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_preview_room(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_preview_room <room_id_or_alias>");
    return PURPLE_CMD_RET_FAILED;
  }
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_preview_room(purple_account_get_username(account), args[0],
                                  MATRIX_PREVIEW_ROOM_ID);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_history_page_size(PurpleConversation *conv,
                                              const gchar *cmd, gchar **args,
                                              gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_history_page_size <1-500>");
    return PURPLE_CMD_RET_FAILED;
  }

  long n = strtol(args[0], NULL, 10);
  if (n < 1 || n > 500) {
    *error = g_strdup("History page size must be between 1 and 500.");
    return PURPLE_CMD_RET_FAILED;
  }

  PurpleAccount *account = purple_conversation_get_account(conv);
  char *val = g_strdup_printf("%ld", n);
  purple_account_set_string(account, "history_page_size", val);
  g_free(val);

  char *msg = g_strdup_printf("History page size set to %ld.", n);
  purple_conversation_write(conv, "System", msg, PURPLE_MESSAGE_SYSTEM,
                            time(NULL));
  g_free(msg);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_auto_fetch_history(PurpleConversation *conv,
                                               const gchar *cmd, gchar **args,
                                               gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_auto_fetch_history <on|off>");
    return PURPLE_CMD_RET_FAILED;
  }

  gboolean enabled;
  if (g_ascii_strcasecmp(args[0], "on") == 0 ||
      g_ascii_strcasecmp(args[0], "true") == 0 ||
      strcmp(args[0], "1") == 0) {
    enabled = TRUE;
  } else if (g_ascii_strcasecmp(args[0], "off") == 0 ||
             g_ascii_strcasecmp(args[0], "false") == 0 ||
             strcmp(args[0], "0") == 0) {
    enabled = FALSE;
  } else {
    *error = g_strdup("Value must be on|off.");
    return PURPLE_CMD_RET_FAILED;
  }

  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_account_set_bool(account, "auto_fetch_history_on_open", enabled);

  purple_conversation_write(conv, "System",
                            enabled ? "Auto history fetch on open: enabled."
                                    : "Auto history fetch on open: disabled.",
                            PURPLE_MESSAGE_SYSTEM, time(NULL));
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_create_room(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_create_room <name>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_create_room(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0], NULL, false);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_public_rooms(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_search_public_rooms(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0], room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_user_search(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_user_search <term>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_search_users(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0], room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_kick(PurpleConversation *conv, const gchar *cmd,
                             gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_kick <user_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  const char *user_id = args[0];
  const char *reason = args[1]; // might be NULL
  purple_matrix_rust_kick_user(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, user_id, reason);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ban(PurpleConversation *conv, const gchar *cmd,
                            gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_ban <user_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  const char *user_id = args[0];
  const char *reason = args[1]; // might be NULL
  purple_matrix_rust_ban_user(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, user_id, reason);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_invite(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_invite <user_id>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_invite_user(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_tag(PurpleConversation *conv, const gchar *cmd,
                            gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_tag <favorite|lowpriority|none>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_set_room_tag(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_ignore(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_ignore <user_id>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_ignore_user(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_power_levels(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_get_power_levels(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_power_level(PurpleConversation *conv,
                                        const gchar *cmd, gchar **args,
                                        gchar **error, void *data) {
  if (!args[0] || !*args[0] || !args[1] || !*args[1]) {
    *error = g_strdup("Usage: /matrix_set_power_level <user_id> <level>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  const char *user_id = args[0];
  long long level = atoll(args[1]);
  purple_matrix_rust_set_power_level(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, user_id, level);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_name(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_name <name>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_set_room_name(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_set_topic(PurpleConversation *conv, const gchar *cmd,
                                  gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_set_topic <topic>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_set_room_topic(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_create(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_alias_create <alias>");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_create_alias(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_alias_delete(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_alias_delete <alias>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_delete_alias(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_report(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_report <event_id> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  purple_matrix_rust_report_content(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_export_keys(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0] || !args[1] || !*args[1]) {
    *error = g_strdup("Usage: /matrix_export_keys <path> <passphrase>");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_export_room_keys(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_bulk_redact(PurpleConversation *conv, const gchar *cmd,
                                    gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_bulk_redact <count> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  const char *room_id = purple_conversation_get_name(conv);
  int count = atoi(args[0]);
  if (count <= 0) {
    *error = g_strdup("Count must be positive.");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_bulk_redact(
      purple_account_get_username(purple_conversation_get_account(conv)),
      room_id, count, args[1]);
  return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet cmd_knock(PurpleConversation *conv, const gchar *cmd,
                              gchar **args, gchar **error, void *data) {
  if (!args[0] || !*args[0]) {
    *error = g_strdup("Usage: /matrix_knock <room_id_or_alias> [reason]");
    return PURPLE_CMD_RET_FAILED;
  }
  purple_matrix_rust_knock(
      purple_account_get_username(purple_conversation_get_account(conv)),
      args[0], args[1]);
  return PURPLE_CMD_RET_OK;
}

static void menu_action_redact_last_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  PurpleAccount *account = purple_chat_get_account(chat);
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  if (room_id) {
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, room_id, account);
    if (conv) {
      const char *last_id = purple_conversation_get_data(conv, "last_event_id");
      if (last_id) {
        purple_matrix_rust_redact_event(purple_account_get_username(account),
                                        room_id, last_id, "Redacted via UI");
      }
    }
  }
}

// Verification Callbacks
extern void purple_matrix_rust_accept_sas(const char *user_id,
                                          const char *target_user_id,
                                          const char *flow_id);
extern void purple_matrix_rust_confirm_sas(const char *user_id,
                                           const char *target_user_id,
                                           const char *flow_id, bool is_match);

struct VerificationData {
  char *user_id;
  char *flow_id;
};

typedef struct {
  char *user_id;
  char *flow_id;
  char *emojis;
} MatrixEmojiData;

static void accept_verification_cb(struct VerificationData *data, int action) {
  purple_matrix_rust_accept_sas(
      purple_account_get_username(find_matrix_account()), data->user_id,
      data->flow_id);
  g_free(data->user_id);
  g_free(data->flow_id);
  g_free(data);
}

static void cancel_verification_cb(struct VerificationData *data, int action) {
  g_free(data->user_id);
  g_free(data->flow_id);
  g_free(data);
}

static void sas_match_cb(struct VerificationData *data, int action) {
  bool is_match = (action == 0); // Assuming Match is first
  purple_matrix_rust_confirm_sas(
      purple_account_get_username(find_matrix_account()), data->user_id,
      data->flow_id, is_match);

  g_free(data->user_id);
  g_free(data->flow_id);
  g_free(data);
}

static gboolean process_sas_request_cb(gpointer data) {
  struct VerificationData *vd = (struct VerificationData *)data;
  purple_request_action(NULL, "Verification Request", "Accept verification?",
                        vd->user_id, 0, NULL, NULL, NULL, vd, 2, "Accept",
                        G_CALLBACK(accept_verification_cb), "Cancel",
                        G_CALLBACK(cancel_verification_cb));
  return FALSE;
}

static gboolean process_sas_emoji_cb(gpointer data) {
  MatrixEmojiData *ed = (MatrixEmojiData *)data;
  struct VerificationData *vd = g_new0(struct VerificationData, 1);
  vd->user_id = g_strdup(ed->user_id);
  vd->flow_id = g_strdup(ed->flow_id);

  char *msg = g_strdup_printf("Do these emojis match?\n\n%s", ed->emojis);

  purple_request_action(NULL, "Compare Emojis", msg, ed->user_id, 0, NULL, NULL,
                        NULL, vd, 2, "Match", G_CALLBACK(sas_match_cb),
                        "Mismatch", G_CALLBACK(sas_match_cb));

  g_free(msg);
  g_free(ed->user_id);
  g_free(ed->flow_id);
  g_free(ed->emojis);
  g_free(ed);
  return FALSE;
}

static void sas_request_cb(const char *user_id, const char *flow_id) {
  struct VerificationData *data = g_new0(struct VerificationData, 1);
  data->user_id = g_strdup(user_id);
  data->flow_id = g_strdup(flow_id);
  g_idle_add(process_sas_request_cb, data);
}

static void sas_emoji_cb(const char *user_id, const char *flow_id,
                         const char *emojis) {
  MatrixEmojiData *data = g_new0(MatrixEmojiData, 1);
  data->user_id = g_strdup(user_id);
  data->flow_id = g_strdup(flow_id);
  data->emojis = g_strdup(emojis);
  g_idle_add(process_sas_emoji_cb, data);
}

// Plugin Actions
static void action_e2ee_status_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  // We need a context. Usually E2EE status is per-room.
  // But if triggered from the account menu, maybe we show general status?
  // Or we just ask for a room ID?
  // purple_matrix_rust_e2ee_status(room_id) prints to a room.

  // For now, let's show a dialog saying "Please use /e2ee_status in a chat
  // window, or check specific room settings." Or better, trigger a global
  // status check if possible. Rust side `purple_matrix_rust_e2ee_status` takes
  // a room_id.

  purple_notify_info(gc, "E2EE Status",
                     "To check E2EE status, please run /e2ee_status in the "
                     "specific chat room.",
                     NULL);
}

static void action_verify_device_input_cb(void *user_data,
                                          const char *user_id) {
  if (user_id && *user_id) {
    purple_matrix_rust_verify_user(
        purple_account_get_username(
            purple_connection_get_account((PurpleConnection *)user_data)),
        user_id);
  }
}

static void action_verify_device_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  purple_request_input(gc, "Verify Device", "Enter User ID to Verify",
                       "Changes will be verified via SAS (Emoji).", NULL, FALSE,
                       FALSE, NULL, "Verify",
                       G_CALLBACK(action_verify_device_input_cb), "Cancel",
                       NULL, purple_connection_get_account(gc), NULL, NULL, gc);
}

static void action_bootstrap_xs_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  purple_matrix_rust_bootstrap_cross_signing(
      purple_account_get_username(purple_connection_get_account(gc)));
  purple_notify_info(gc, "Cross-Signing",
                     "Bootstrap requested. Check debug log for details.", NULL);
}

static void action_recover_keys_input_cb(void *user_data,
                                         const char *passphrase) {
  if (passphrase && *passphrase) {
    if (user_data) {
      purple_matrix_rust_recover_keys(
          purple_account_get_username((PurpleAccount *)user_data), passphrase);
    }
  }
}

static void action_recover_keys_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  purple_request_input(gc, "Recover Keys", "Enter 4S Passphrase",
                       "Recover encryption keys from Secret Storage.", NULL,
                       FALSE, TRUE, NULL, // Masked = TRUE
                       "Recover", G_CALLBACK(action_recover_keys_input_cb),
                       "Cancel", NULL, purple_connection_get_account(gc), NULL,
                       NULL, NULL);
}

static void create_room_cb(void *user_data, PurpleRequestFields *fields) {
  const char *name = purple_request_fields_get_string(fields, "name");
  const char *topic = purple_request_fields_get_string(fields, "topic");
  int preset = purple_request_fields_get_choice(fields, "preset");
  bool is_public = (preset == 1);

  if (name && *name) {
    purple_matrix_rust_create_room(
        purple_account_get_username(find_matrix_account()), name, topic,
        is_public);
  }
}

static void action_create_room_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *field;
  field = purple_request_field_string_new("name", "Room Name", NULL, FALSE);
  purple_request_field_set_required(field, TRUE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("topic", "Topic", NULL, TRUE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_choice_new("preset", "Visibility", 0);
  purple_request_field_choice_add(field, "Private");
  purple_request_field_choice_add(field, "Public");
  purple_request_field_group_add_field(group, field);

  purple_request_fields(gc, "Create Room", "Create a new Matrix Room", NULL,
                        fields, "Create", G_CALLBACK(create_room_cb), "Cancel",
                        NULL, purple_connection_get_account(gc), NULL, NULL,
                        NULL);
}

static void search_user_cb(void *user_data, const char *term) {
  if (term && *term) {
    // Output to a generic console room if global
    if (user_data) {
      purple_matrix_rust_search_users(
          purple_account_get_username(
              purple_connection_get_account((PurpleConnection *)user_data)),
          term, "Matrix Console");
    }
  }
}

static void action_search_user_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  purple_request_input(gc, "User Search", "Search for a user",
                       "Enter a name or Matrix ID to search the directory.",
                       NULL, FALSE, FALSE, NULL, "Search",
                       G_CALLBACK(search_user_cb), "Cancel", NULL,
                       purple_connection_get_account(gc), NULL, NULL, NULL);
}

static void knock_room_input_cb(void *user_data, PurpleRequestFields *fields) {
  const char *id = purple_request_fields_get_string(fields, "id");
  const char *reason = purple_request_fields_get_string(fields, "reason");
  if (id && *id) {
    if (user_data) {
      purple_matrix_rust_knock(
          purple_account_get_username(
              purple_connection_get_account((PurpleConnection *)user_data)),
          id, reason);
    }
  }
}

static void action_knock_room_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new("id", "Room ID or Alias", NULL, FALSE));
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("reason", "Reason", NULL, FALSE));

  purple_request_fields(gc, "Knock on Room", "Request entry to a room", NULL,
                        fields, "Knock", G_CALLBACK(knock_room_input_cb),
                        "Cancel", NULL, purple_connection_get_account(gc), NULL,
                        NULL, NULL);
}

static void action_preview_room_input_cb(void *user_data, const char *target) {
  PurpleConnection *gc = (PurpleConnection *)user_data;
  if (!gc || !target || !*target) {
    return;
  }
  PurpleAccount *account = purple_connection_get_account(gc);
  purple_matrix_rust_preview_room(purple_account_get_username(account), target,
                                  MATRIX_PREVIEW_ROOM_ID);
}

static void action_preview_room_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  purple_request_input(
      gc, "Preview Room", "Room ID or Alias",
      "Enter a room ID (!room:server) or alias (#room:server).", NULL, FALSE,
      FALSE, NULL, "Preview", G_CALLBACK(action_preview_room_input_cb),
      "Cancel", NULL, purple_connection_get_account(gc), NULL, NULL, gc);
}

extern void purple_matrix_rust_enable_key_backup(const char *user_id);

static void action_enable_backup_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  purple_matrix_rust_enable_key_backup(
      purple_account_get_username(purple_connection_get_account(gc)));
}

static void action_reconnect_cb(PurplePluginAction *action) {
  PurpleConnection *gc = (PurpleConnection *)action->context;
  if (!gc)
    return;
  PurpleAccount *account = purple_connection_get_account(gc);
  if (!account)
    return;

  PurpleConnectionState state = purple_connection_get_state(gc);
  if (state == PURPLE_CONNECTED) {
    purple_notify_info(gc, "Matrix", "Reconnect", "Already connected.");
    return;
  }
  if (state == PURPLE_CONNECTING) {
    purple_notify_info(gc, "Matrix", "Reconnect",
                       "Connection attempt already in progress.");
    return;
  }

  purple_account_connect(account);
  purple_notify_info(gc, "Matrix", "Reconnect",
                     "Reconnect requested. Please complete login if prompted.");
}

// Sticker Data Structures
typedef struct {
  char *id;
  char *name;
} MatrixStickerPack;

typedef struct {
  char *shortcode;
  char *body;
  char *url;
} MatrixSticker;

typedef struct {
  PurpleConnection *gc;
  GList *packs;    // List of MatrixStickerPack*
  GList *stickers; // List of MatrixSticker*
  char *selected_pack_id;
  char *room_id; // For sending
} StickerRequestData;

// Externs for Stickers
extern void purple_matrix_rust_list_sticker_packs(
    const char *user_id, void (*cb)(const char *, const char *, void *),
    void (*done_cb)(void *), void *user_data);
extern void purple_matrix_rust_list_stickers_in_pack(
    const char *user_id, const char *pack_id,
    void (*cb)(const char *, const char *, const char *, void *),
    void (*done_cb)(void *), void *user_data);
extern void purple_matrix_rust_send_sticker(const char *user_id,
                                            const char *room_id,
                                            const char *url);

// Forward decls
static void sticker_pack_cb(const char *id, const char *name, void *user_data);
static void packs_loaded_cb(void *user_data);
static void sticker_cb(const char *shortcode, const char *body, const char *url,
                       void *user_data);
static void stickers_loaded_cb(void *user_data);
static void free_matrix_pack(gpointer data);
static void free_matrix_sticker(gpointer data);

static void free_sticker_req_data(StickerRequestData *data) {
  if (!data)
    return;
  g_list_free_full(data->packs, free_matrix_pack);
  g_list_free_full(data->stickers, free_matrix_sticker);
  data->packs = NULL;
  data->stickers = NULL;
  g_free(data->selected_pack_id);
  g_free(data->room_id);
  g_free(data);
}

static void free_matrix_pack(gpointer data) {
  MatrixStickerPack *p = (MatrixStickerPack *)data;
  g_free(p->id);
  g_free(p->name);
  g_free(p);
}
static void free_matrix_sticker(gpointer data) {
  MatrixSticker *s = (MatrixSticker *)data;
  g_free(s->shortcode);
  g_free(s->body);
  g_free(s->url);
  g_free(s);
}

// Callbacks

static void sticker_pack_cb(const char *id, const char *name, void *user_data) {
  StickerRequestData *data = (StickerRequestData *)user_data;
  MatrixStickerPack *pack = g_new0(MatrixStickerPack, 1);
  pack->id = g_strdup(id);
  pack->name = g_strdup(name);
  data->packs = g_list_append(data->packs, pack);
}

static void pack_selected_cb(gpointer user_data, PurpleRequestFields *fields) {
  StickerRequestData *data = (StickerRequestData *)user_data;
  int index = purple_request_fields_get_choice(fields, "pack");

  if (index >= 0) {
    MatrixStickerPack *pack =
        (MatrixStickerPack *)g_list_nth_data(data->packs, index);
    if (pack) {
      data->selected_pack_id = g_strdup(pack->id);

      // Load stickers for this pack
      PurpleAccount *account = purple_connection_get_account(data->gc);
      const char *user_id = purple_account_get_username(account);

      purple_matrix_rust_list_stickers_in_pack(user_id, pack->id, sticker_cb,
                                               stickers_loaded_cb, data);
      return;
    }
  }

  // Cancelled or invalid
  free_sticker_req_data(data);
}

static void pack_cancel_cb(gpointer user_data, PurpleRequestFields *fields) {
  StickerRequestData *data = (StickerRequestData *)user_data;
  free_sticker_req_data(data);
}

static void packs_loaded_cb(void *user_data) {
  StickerRequestData *data = (StickerRequestData *)user_data;

  if (!data->packs) {
    purple_notify_info(
        data->gc, "Stickers", "No Sticker Packs Found",
        "Support for 'im.ponies.user_defined_sticker_pack' account data only.");
    free_sticker_req_data(data);
    return;
  }

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *field =
      purple_request_field_choice_new("pack", "Select Sticker Pack", 0);

  GList *l;
  for (l = data->packs; l != NULL; l = l->next) {
    MatrixStickerPack *pack = (MatrixStickerPack *)l->data;
    purple_request_field_choice_add(field, pack->name);
    // We set the data to the ID? No, choice_add just adds label?
    // purple_request_field_choice_add accepts label.
    // We actually want key-value.
    // `purple_request_field_choice_add(field, label)` maps to index?
    // Wait, `purple_request_field_choice_new` creates it.
    // We can use `purple_request_field_choice_add(field, label)`.
    // BUT how do we get the ID back?
    // Pidgin API: `purple_request_field_choice_add` adds a *pointer*? No,
    // typically just label. `purple_request_field_choice_add_full`? Not in
    // standard libpurple 2.x API easily? Actually
    // `purple_request_field_choice_add` sets the value to the index? If we want
    // string IDs, we might need to look them up by index or use `_add(field,
    // label)`. Let's use `_add(field, label)` and assume order matches
    // `data->packs`.
  }
  purple_request_field_group_add_field(group, field);

  // We need to pass data, but we effectively transfer ownership of
  // `data->packs` logic to the next step But Request API doesn't let us attach
  // arbitrary data to choices easily to retrieve by ID. So in
  // `pack_selected_cb`, we get the index? `purple_request_fields_get_choice`
  // returns the index (int) cast to void*? Or int? It returns `int` usually.
  // Let's verify `purple_request_fields_get_choice`.
  // It returns `int` (index).

  purple_request_fields(
      data->gc, "Send Sticker", "Select Sticker Pack", NULL, fields, "Next",
      G_CALLBACK(pack_selected_cb), "Cancel", G_CALLBACK(pack_cancel_cb),
      purple_connection_get_account(data->gc), NULL, NULL, data);
}

// ... Sticker Selection ...

static void sticker_cb(const char *shortcode, const char *body, const char *url,
                       void *user_data) {
  StickerRequestData *data = (StickerRequestData *)user_data;
  MatrixSticker *s = g_new0(MatrixSticker, 1);
  s->shortcode = g_strdup(shortcode);
  s->body = g_strdup(body);
  s->url = g_strdup(url);
  data->stickers = g_list_append(data->stickers, s);
}

static void sticker_send_cb(gpointer user_data, PurpleRequestFields *fields) {
  StickerRequestData *data = (StickerRequestData *)user_data;
  int index = purple_request_fields_get_choice(fields, "sticker");

  if (index >= 0) {
    MatrixSticker *s = (MatrixSticker *)g_list_nth_data(data->stickers, index);
    if (s) {
      PurpleAccount *account = purple_connection_get_account(data->gc);
      const char *user_id = purple_account_get_username(account);

      // Sending sticker
      purple_matrix_rust_send_sticker(user_id, data->room_id, s->url);
    }
  }

  // Clean up everything
  free_sticker_req_data(data);
}

static void sticker_cancel_cb(gpointer user_data, PurpleRequestFields *fields) {
  StickerRequestData *data = (StickerRequestData *)user_data;
  free_sticker_req_data(data);
}

static void stickers_loaded_cb(void *user_data) {
  StickerRequestData *data = (StickerRequestData *)user_data;

  if (!data->stickers) {
    purple_notify_info(data->gc, "Stickers", "Empty Pack",
                       "No stickers found in this pack.");
    // Should we go back? For now, just cancel.
    free_sticker_req_data(data);
    return;
  }

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *field =
      purple_request_field_choice_new("sticker", "Select Sticker", 0);

  GList *l;
  for (l = data->stickers; l != NULL; l = l->next) {
    MatrixSticker *s = (MatrixSticker *)l->data;
    // Label: "shortcode (body)"
    char *label = g_strdup_printf("%s (%s)", s->shortcode, s->body);
    purple_request_field_choice_add(field, label);
    g_free(label);
  }
  purple_request_field_group_add_field(group, field);

  purple_request_fields(
      data->gc, "Send Sticker", "Select Sticker", NULL, fields, "Send",
      G_CALLBACK(sticker_send_cb), "Cancel", G_CALLBACK(sticker_cancel_cb),
      purple_connection_get_account(data->gc), NULL, NULL, data);
}

static void blist_action_send_sticker_cb(PurpleBlistNode *node, gpointer data) {
  PurpleAccount *account;
  const char *room_id_str = NULL;

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    PurpleChat *chat = (PurpleChat *)node;
    account = purple_chat_get_account(chat);
    GHashTable *components = purple_chat_get_components(chat);
    room_id_str = g_hash_table_lookup(components, "room_id"); // Or id?
    // Usually "room_id" in this plugin?
  } else {
    return; // Only chats
  }

  if (!room_id_str)
    return;

  PurpleConnection *gc = purple_account_get_connection(account);
  const char *user_id = purple_account_get_username(account);

  StickerRequestData *req = g_new0(StickerRequestData, 1);
  req->gc = gc;
  req->room_id = g_strdup(room_id_str);

  purple_matrix_rust_list_sticker_packs(user_id, sticker_pack_cb,
                                        packs_loaded_cb, req);
}

// And we need to add this to `blist_node_menu`

static GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
  GList *l = NULL;
  PurplePluginAction *act = NULL;

  act = purple_plugin_action_new("Verify User...", action_verify_device_cb);
  l = g_list_append(l, act);

  act =
      purple_plugin_action_new("Recover Keys (4S)...", action_recover_keys_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Create Room...", action_create_room_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Search Users...", action_search_user_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Knock on Room...", action_knock_room_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Preview Room...", action_preview_room_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Bootstrap Cross-Signing (Reset)",
                                 action_bootstrap_xs_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("E2EE Status Info", action_e2ee_status_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Reconnect / Re-Login", action_reconnect_cb);
  l = g_list_append(l, act);

  act = purple_plugin_action_new("Enable/Check Key Backup",
                                 action_enable_backup_cb);
  l = g_list_append(l, act);

  // Hidden until backup restore is fully implemented end-to-end.

  return l;
}

static PurplePluginProtocolInfo prpl_info = {
    .options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD,
    // .actions removed as it is not in this struct
    .user_splits = NULL,
    .protocol_options = NULL,
    .icon_spec =
        {
            .format = "png",
            .min_width = 30,
            .min_height = 30,
            .max_width = 96,
            .max_height = 96,
            .scale_rules = PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY,
        },
    .list_icon = matrix_list_icon,
    .list_emblem = matrix_list_emblem,
    .status_text = matrix_status_text,
    .tooltip_text = matrix_tooltip_text,
    .status_types = matrix_status_types,
    .blist_node_menu = blist_node_menu_cb,
    .chat_info = matrix_chat_info,
    .chat_info_defaults = matrix_chat_info_defaults,
    .login = matrix_login,
    .close = matrix_close,
    .send_im = matrix_send_im,
    .set_info = NULL,
    .send_typing = matrix_send_typing,
    .get_info = matrix_get_info,
    .set_status = matrix_set_status,
    .set_idle = matrix_set_idle,
    .change_passwd = matrix_change_passwd,
    .add_buddy = matrix_add_buddy,
    .add_buddies = NULL,
    .remove_buddy = matrix_remove_buddy,
    .remove_buddies = NULL,
    .add_permit = NULL,
    .add_deny = matrix_add_deny,
    .rem_permit = NULL,
    .rem_deny = matrix_rem_deny,
    .set_permit_deny = NULL,
    .join_chat = matrix_join_chat,
    .reject_chat = NULL,
    .get_chat_name = matrix_get_chat_name,
    .chat_invite = matrix_chat_invite,
    .chat_leave = matrix_chat_leave,
    .chat_whisper = matrix_chat_whisper,
    .chat_send = matrix_chat_send,
    .set_chat_topic = matrix_set_chat_topic,
    // .uri_handler = NULL, // Not supported in libpurple 2.x struct
    .send_file = matrix_send_file,
    .roomlist_get_list = matrix_roomlist_get_list,
    .roomlist_cancel = matrix_roomlist_cancel,
    .roomlist_expand_category = NULL,
    .can_receive_file = NULL,
    .new_xfer = NULL, // matrix_new_xfer,
    .offline_message = NULL,
    .whiteboard_prpl_ops = NULL,
    .send_raw = NULL,
    .roomlist_room_serialize = NULL,
    .unregister_user = matrix_unregister_user,
    .send_attention = NULL,
    .get_attention_types = NULL,
    .get_account_text_table = NULL,
    .initiate_media = NULL,
    .get_media_caps = NULL,
    .get_moods = NULL,
    .set_public_alias =
        (void *)matrix_set_public_alias, // Cast to avoid warning if sig differs
    .get_public_alias = NULL,
    .add_buddy_with_invite = NULL,
    .add_buddies_with_invite = NULL,
    .get_cb_real_name = NULL,
    .get_cb_info = (void *)matrix_chat_tooltip_text, // Use for rich chat info
    .get_cb_away = NULL,
    .set_buddy_icon = matrix_set_buddy_icon,
    .struct_size = sizeof(PurplePluginProtocolInfo)};

// ... implementation ...

typedef void (*InviteCallback)(const char *room_id, const char *inviter);
extern void purple_matrix_rust_set_invite_callback(InviteCallback cb);

// ... (Existing structs) ...

typedef struct {
  char *room_id;
  char *inviter;
} MatrixInviteData;

static gboolean process_invite_cb(gpointer data) {
  MatrixInviteData *d = (MatrixInviteData *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    purple_debug_info("purple-matrix-rust",
                      "Processing invite for room: %s from %s\n", d->room_id,
                      d->inviter);

    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      // We populate 'data' with the room_id so when they click 'Accept',
      // java/UI calls join_chat with these components
      GHashTable *components =
          g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert(components, g_strdup("room_id"),
                          g_strdup(d->room_id));

      // "name" (arg 2) is what shows up in the dialog as "Chat Name"
      // "who" (arg 3) is the inviter
      // "message" (arg 4)
      serv_got_chat_invite(gc, d->room_id, d->inviter, "Incoming Matrix Invite",
                           components);
    }
  }

  g_free(d->room_id);
  g_free(d->inviter);
  g_free(d);
  return FALSE;
}

// Read Marker Sync Callback
typedef struct {
  char *room_id;
  char *event_id;
  char *user_id;
} MatrixReadMarkerData;

static gboolean process_read_marker_cb(gpointer data) {
  MatrixReadMarkerData *d = (MatrixReadMarkerData *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    purple_debug_info(
        "purple-matrix-rust",
        "Processing read marker for room: %s (User: %s, Event: %s)\n",
        d->room_id, d->user_id, d->event_id);

    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, d->room_id, account);
    if (!conv) {
      conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
                                                   d->room_id, account);
    }

    if (conv) {
      // Display a system message about the read receipt
      // To avoid spam, we might want to suppress this or make it optional?
      // For now, let's show it as it's the requested feature.

      // Only show if it's NOT the local user (we know we read it)
      const char *local_user = purple_account_get_username(account);
      if (d->user_id && strcmp(d->user_id, local_user) != 0) {
        char *msg = g_strdup_printf("[Read Receipt] %s read up to %s",
                                    d->user_id, d->event_id);
        purple_conv_chat_write(PURPLE_CONV_CHAT(conv), "", msg,
                               PURPLE_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
      }

      // Store data for tooltips
      // We can't easily store a map of User->EventID in conversation data
      // without serializing it. For now, simple visual feedback is enough.
    }
  }

  g_free(d->room_id);
  g_free(d->event_id);
  g_free(d->user_id);
  g_free(d);
  return FALSE;
}

static void read_marker_cb(const char *room_id, const char *event_id,
                           const char *user_id) {
  MatrixReadMarkerData *d = g_new0(MatrixReadMarkerData, 1);
  d->room_id = g_strdup(room_id);
  d->event_id = g_strdup(event_id);
  d->user_id = g_strdup(user_id);
  g_idle_add(process_read_marker_cb, d);
}

// Buddy Update Callback (Profile Sync)
typedef struct {
  char *user_id;
  char *alias;
  char *icon_path;
} MatrixBuddyUpdateData;

static gboolean process_buddy_update_cb(gpointer data) {
  MatrixBuddyUpdateData *d = (MatrixBuddyUpdateData *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    purple_debug_info("purple-matrix-rust",
                      "Updating buddy profile: %s (Alias: %s, Icon: %s)\n",
                      d->user_id, d->alias, d->icon_path);

    // Find the buddy
    PurpleBuddy *buddy = purple_find_buddy(account, d->user_id);
    if (!buddy) {
      // If buddy doesn't exist, we generally don't create it just because of a
      // profile update unless we want to populate the roster dynamically. For
      // now, let's only update matches.
      // purple_debug_warning("purple-matrix-rust", "Buddy found for update:
      // %s\n", d->user_id);
    } else {
      // Update Alias
      if (d->alias && strlen(d->alias) > 0) {
        purple_blist_alias_buddy(buddy, d->alias);
      }

      // Update Icon
      if (d->icon_path && strlen(d->icon_path) > 0) {
        // purple_buddy_set_icon expects data.
        // Better: purple_buddy_icons_set_for_user(account, who, data, len,
        // checksum) But wait, Libpurple usually handles fetching if we provide
        // an icon URL. Since we downloaded it to a path, we can read it and set
        // it.

        gchar *contents;
        gsize length;
        if (g_file_get_contents(d->icon_path, &contents, &length, NULL)) {
          purple_buddy_icons_set_for_user(account, d->user_id, contents, length,
                                          NULL);
          g_free(contents);
        }
      }
    }
  }

  g_free(d->user_id);
  g_free(d->alias);
  g_free(d->icon_path);
  g_free(d);
  return FALSE;
}

static void update_buddy_callback(const char *user_id, const char *alias,
                                  const char *icon_path) {
  MatrixBuddyUpdateData *d = g_new0(MatrixBuddyUpdateData, 1);
  d->user_id = g_strdup(user_id);
  d->alias = g_strdup(alias);
  d->icon_path = g_strdup(icon_path);
  g_idle_add(process_buddy_update_cb, d);
}

// Helper to organize threads in Buddy List
static void ensure_thread_in_blist(PurpleAccount *account,
                                   const char *virtual_id, const char *alias,
                                   const char *parent_room_id) {
  if (!account || !virtual_id)
    return;
  // Only handle virtual thread room IDs (room_id|thread_root_id).
  if (!strchr(virtual_id, '|')) {
    purple_debug_warning(
        "purple-matrix-rust",
        "ensure_thread_in_blist called with non-thread id '%s'; ignoring\n",
        virtual_id);
    return;
  }

  PurpleConnection *gc = purple_account_get_connection(account);
  if (!gc)
    return;

  purple_debug_info("purple-matrix-rust",
                    "ensure_thread_in_blist: Processing %s (Parent: %s)\n",
                    virtual_id, parent_room_id ? parent_room_id : "None");

  // determine group name
  // Libpurple groups are flat (no true nested groups), so we emulate hierarchy
  // using path-like labels: "<Group> > <Room> > Threads".
  char *group_name = NULL;

  // Try to find parent room metadata for better grouping.
  if (parent_room_id) {
    // Check active conversations first
    PurpleConversation *parent_conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, parent_room_id, account);
    const char *parent_title = NULL;
    const char *parent_group_name = NULL;
    if (parent_conv) {
      parent_title = purple_conversation_get_title(parent_conv);
      PurpleChat *parent_chat = find_chat_manual(account, parent_room_id);
      if (parent_chat) {
        PurpleGroup *pg = purple_chat_get_group(parent_chat);
        if (pg)
          parent_group_name = purple_group_get_name(pg);
      }
    } else {
      // Check buddy list
      PurpleChat *parent_chat = find_chat_manual(account, parent_room_id);
      if (parent_chat) {
        if (parent_chat->alias) {
          parent_title = parent_chat->alias;
        } else {
          parent_title = purple_chat_get_name(parent_chat);
        }
        PurpleGroup *pg = purple_chat_get_group(parent_chat);
        if (pg)
          parent_group_name = purple_group_get_name(pg);
      }
    }

    if (!parent_group_name || !*parent_group_name)
      parent_group_name = "Matrix Rooms";
    if (!parent_title || !*parent_title)
      parent_title = parent_room_id;

    group_name =
        g_strdup_printf("%s / %s / Threads", parent_group_name, parent_title);
  } else {
    group_name = g_strdup("Matrix Rooms / Unknown Room / Threads");
  }

  purple_debug_info("purple-matrix-rust",
                    "ensure_thread_in_blist: Target Group '%s'\n", group_name);

  PurpleGroup *group = purple_find_group(group_name);
  if (!group) {
    purple_debug_info("purple-matrix-rust",
                      "ensure_thread_in_blist: Creating group '%s'\n",
                      group_name);
    group = purple_group_new(group_name);
    purple_blist_add_group(group, NULL);
  }

  // Check if chat already exists
  // We used find_chat_manual for room_id lookup because blist checks
  // components. For virtual_id which is stored in "room_id" component, we can
  // use find_chat_manual too.
  PurpleChat *chat = find_chat_manual(account, virtual_id);

  // Prepare a nice alias: "[Thread] Truncated Message..."
  char *nice_alias = NULL;
  if (alias) {
    char *truncated = g_strndup(alias, 50); // Keep it short
    char *end_ptr = NULL;
    if (!g_utf8_validate(truncated, -1, (const gchar **)&end_ptr)) {
      if (end_ptr) {
        *end_ptr = '\0';
      }
    }
    if (strlen(alias) > 50) {
      char *tmp = g_strdup_printf("%s...", truncated);
      g_free(truncated);
      truncated = tmp;
    }
    nice_alias = g_strdup_printf("Thread: %s", truncated);
    g_free(truncated);
  }

  if (!chat) {
    purple_debug_info("purple-matrix-rust",
                      "ensure_thread_in_blist: Adding new chat to group\n");
    GHashTable *components =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(components, g_strdup("room_id"), g_strdup(virtual_id));
    chat = purple_chat_new(account, virtual_id, components);
    purple_blist_add_chat(chat, group, NULL);
  } else {
    // Move to correct group if needed
    if (purple_chat_get_group(chat) != group) {
      purple_debug_info(
          "purple-matrix-rust",
          "ensure_thread_in_blist: Moving chat to correct group\n");
      purple_blist_add_chat(chat, group, NULL);
    }
  }

  // Always update alias to show latest topic/message
  if (nice_alias) {
    purple_blist_alias_chat(chat, nice_alias);
    g_free(nice_alias);
  }

  g_free(group_name);
}

// Slash Command Handler: /thread [message]
// Usage:
//   /thread <message>  -> Start thread on *last received message* with
//   <message> /thread            -> Open thread window for *last received
//   message* (if args empty)
static PurpleCmdRet cmd_thread(PurpleConversation *conv, const gchar *cmd,
                               gchar **args, gchar **error, void *data) {
  if (purple_conversation_get_type(conv) != PURPLE_CONV_TYPE_CHAT) {
    *error = g_strdup("Only supported in chats.");
    return PURPLE_CMD_RET_FAILED;
  }

  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    *error =
        g_strdup("No recent message to reply to. Receive a message first.");
    return PURPLE_CMD_RET_FAILED;
  }

  // Use conversation name as room_id. It holds the room_id (or
  // room_id|thread_id).
  const char *full_room_id = purple_conversation_get_name(conv);

  // Extract base room_id if we contain a pipe (don't nest threads yet)
  char *room_id_copy = g_strdup(full_room_id);
  char *pipe_ptr = strchr(room_id_copy, '|');
  if (pipe_ptr) {
    *pipe_ptr = '\0';
  }

  if (!room_id_copy || strlen(room_id_copy) == 0) {
    *error = g_strdup("Could not determine room ID.");
    g_free(room_id_copy);
    return PURPLE_CMD_RET_FAILED;
  }

  // Construct virtual ID
  char *virtual_id = g_strdup_printf("%s|%s", room_id_copy, last_event_id);

  PurpleAccount *account = purple_conversation_get_account(conv);
  PurpleConnection *gc = purple_account_get_connection(account);

  // Add to Buddy List Group
  ensure_thread_in_blist(account, virtual_id, "Thread", room_id_copy);

  // 1. Join/Open the thread window
  int chat_id = get_chat_id(virtual_id);
  if (!purple_find_chat(gc, chat_id)) {
    serv_got_joined_chat(gc, chat_id, virtual_id);
  }

  // 2. If message provided, send it
  if (args[0] && strlen(args[0]) > 0) {
    purple_matrix_rust_send_message(purple_account_get_username(account),
                                    virtual_id, args[0]);
  } else {
    // Just focus the window
    PurpleConversation *tconv = purple_find_chat(gc, chat_id);
    if (tconv) {
      purple_conversation_present(tconv);
    }
  }

  g_free(virtual_id);
  g_free(room_id_copy);
  return PURPLE_CMD_RET_OK;
}

static void invite_callback(const char *room_id, const char *inviter) {
  MatrixInviteData *d = g_new0(MatrixInviteData, 1);
  d->room_id = g_strdup(room_id);
  d->inviter = g_strdup(inviter);
  g_idle_add(process_invite_cb, d);
}

// Input Callback for "Reply to Thread"
static void menu_action_thread_input_cb(void *user_data, const char *msg) {
  char *virtual_id = (char *)user_data;
  PurpleAccount *account =
      find_matrix_account(); // Simplified, ideally pass account

  // We need to parse room_id from virtual_id to group it
  char *room_id = g_strdup(virtual_id);
  char *p = strchr(room_id, '|');
  if (p)
    *p = '\0';

  if (account) {
    ensure_thread_in_blist(account, virtual_id, "Thread", room_id);
  }
  g_free(room_id);

  if (msg && strlen(msg) > 0) {
    purple_matrix_rust_send_message(purple_account_get_username(account),
                                    virtual_id, msg);
  }
  g_free(virtual_id);
}

// Cancel Callback
static void menu_action_thread_cancel_cb(void *user_data) { g_free(user_data); }

// Action Callback: "Reply to Last Thread"
static void menu_action_thread_reply_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  PurpleAccount *account = purple_chat_get_account(chat);
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  if (!room_id)
    return;

  // Find updated validation of last_event_id
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_CHAT, room_id, account);
  if (!conv) {
    // If conversation isn't open, we might not have the last event ID.
    // In a real implementation we might fetch it from store, but for now rely
    // on active conv.
    purple_notify_error(NULL, "Error", "Conversation must be open to reply.",
                        NULL);
    return;
  }

  const char *last_event_id =
      purple_conversation_get_data(conv, "last_event_id");
  if (!last_event_id) {
    purple_notify_error(NULL, "Error", "No recent message to reply to.", NULL);
    return;
  }

  // Construct Virtual ID
  // Note: room_id from components might be raw. Check if it has pipe?
  // Usually chat components room_id is the clean one.
  char *virtual_id = g_strdup_printf("%s|%s", room_id, last_event_id);

  // Prompt user for message
  purple_request_input(
      NULL, "Reply to Thread", "Enter your reply:",
      "This will be sent as a threaded reply to the last message.",
      NULL,  // default value
      TRUE,  // multiline
      FALSE, // masked
      NULL,  // hint
      "Send", G_CALLBACK(menu_action_thread_input_cb), "Cancel",
      G_CALLBACK(menu_action_thread_cancel_cb), account, NULL, NULL,
      virtual_id);
}

static void room_settings_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  const char *name = purple_request_fields_get_string(fields, "name");
  const char *topic = purple_request_fields_get_string(fields, "topic");

  if (name && *name) {
    purple_matrix_rust_set_room_name(
        purple_account_get_username(find_matrix_account()), room_id, name);
  }
  if (topic) { // topic can be empty string to clear
    purple_matrix_rust_set_room_topic(
        purple_account_get_username(find_matrix_account()), room_id, topic);
  }
  g_free(room_id);
}

static void menu_action_list_threads_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    purple_matrix_rust_list_threads(
        purple_account_get_username(purple_chat_get_account(chat)), room_id);
  }
}

static void menu_action_active_polls_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    purple_matrix_rust_get_active_polls(
        purple_account_get_username(purple_chat_get_account(chat)), room_id);
  }
}

static void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  const char *topic = g_hash_table_lookup(components, "topic");
  // Name is the alias or blist name
  const char *name = purple_chat_get_name(chat);
  if (chat->alias)
    name = chat->alias;

  if (!room_id)
    return;

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *field;

  // Room ID (Read-onlyish - display only)
  field = purple_request_field_string_new("room_id_display", "Room ID", room_id,
                                          FALSE);
  // purple_request_field_string_set_editable(field, FALSE); // Not available in
  // all libpurple versions easily? Actually, simply ignoring it in callback is
  // enough, but UI wise it looks editable. We can use a label for it if we
  // want, but string is fine.
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("name", "Name", name, FALSE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("topic", "Topic", topic,
                                          TRUE); // Multiline for topic
  purple_request_field_group_add_field(group, field);

  purple_request_fields(
      purple_chat_get_account(chat), "Room Settings", "Edit Room Settings",
      NULL, fields, "Save", G_CALLBACK(room_settings_cb), "Cancel", NULL,
      purple_chat_get_account(chat), NULL, NULL, g_strdup(room_id));
}

static void react_input_cb(void *user_data, const char *emoji) {
  char *room_id = (char *)user_data; // We need room_id AND event_id.
  // Wait, context menu needs conversation to get last event id.
  // If called from blist, we might not have active conversation/last event.
  // We can only react to the *last received message in the active conversation*
  // corresponding to this chat.

  PurpleAccount *account =
      find_matrix_account(); // Should lookup from room_id ownership really
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_CHAT, room_id, account);

  if (conv) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id && emoji && *emoji) {
      purple_matrix_rust_send_reaction(purple_account_get_username(account),
                                       room_id, last_id, emoji);
    }
  }
  g_free(room_id);
}

static void menu_action_react_quick_cb(PurpleBlistNode *node, gpointer data) {
  char *emoji = (char *)data; // "👍", etc.
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  if (room_id) {
    PurpleAccount *account = purple_chat_get_account(chat);
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_CHAT, room_id, account);
    if (conv) {
      const char *last_id = purple_conversation_get_data(conv, "last_event_id");
      if (last_id) {
        purple_matrix_rust_send_reaction(purple_account_get_username(account),
                                         room_id, last_id, emoji);
      } else {
        purple_notify_error(NULL, "Error", "No message to react to.", NULL);
      }
    } else {
      purple_notify_error(NULL, "Error", "Open the chat window to react.",
                          NULL);
    }
  }
}

static void menu_action_react_custom_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  if (room_id) {
    PurpleAccount *account = purple_chat_get_account(chat);
    purple_request_input(account, "React", "Enter Emoji",
                         "Enter the emoji or text to react with:", NULL, FALSE,
                         FALSE, NULL, "React", G_CALLBACK(react_input_cb),
                         "Cancel", NULL, account, NULL, NULL,
                         g_strdup(room_id));
  }
}

static void reply_input_cb(void *user_data, const char *msg) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_CHAT, room_id, account);

  if (conv && msg && *msg) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id) {
      purple_matrix_rust_send_reply(purple_account_get_username(account),
                                    room_id, last_id, msg);
    }
  }
  g_free(room_id);
}

static void menu_action_reply_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  if (room_id) {
    PurpleAccount *account = purple_chat_get_account(chat);
    purple_request_input(account, "Reply", "Reply to Last Message",
                         "Enter your reply:", NULL, TRUE, FALSE, NULL, "Send",
                         G_CALLBACK(reply_input_cb), "Cancel", NULL, account,
                         NULL, NULL, g_strdup(room_id));
  }
}

static void edit_input_cb(void *user_data, const char *msg) {
  char *room_id = (char *)user_data;
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_CHAT, room_id, account);

  if (conv && msg && *msg) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id) {
      purple_matrix_rust_send_edit(purple_account_get_username(account),
                                   room_id, last_id, msg);
    }
  }
  g_free(room_id);
}

static void menu_action_edit_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");

  if (room_id) {
    PurpleAccount *account = purple_chat_get_account(chat);
    purple_request_input(account, "Edit", "Edit Last Message",
                         "Enter the new message content:", NULL, TRUE, FALSE,
                         NULL, "Update", G_CALLBACK(edit_input_cb), "Cancel",
                         NULL, account, NULL, NULL, g_strdup(room_id));
  }
}

static void edit_id_input_cb(void *user_data, const char *msg) {
  char *data_str = (char *)user_data;
  // Format: room_id|event_id
  char **parts = g_strsplit(data_str, "|", 2);
  if (parts[0] && parts[1]) {
    purple_matrix_rust_send_edit(
        purple_account_get_username(find_matrix_account()), parts[0], parts[1],
        msg);
  }
  g_strfreev(parts);
  g_free(data_str);
}

static void edit_id_prompt_cb(void *user_data, const char *event_id) {
  char *room_id = (char *)user_data;
  if (event_id && *event_id) {
    // Now prompt for content
    char *pass_data = g_strdup_printf("%s|%s", room_id, event_id);

    purple_request_input(find_matrix_account(), "Edit Message",
                         "Enter New Content", "New message body:", NULL, TRUE,
                         FALSE, NULL, "Edit", G_CALLBACK(edit_id_input_cb),
                         "Cancel", NULL, find_matrix_account(), NULL, NULL,
                         pass_data);
  }
  g_free(room_id);
}

static void menu_action_edit_id_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    purple_request_input(find_matrix_account(), "Edit by ID", "Enter Event ID",
                         "Paste the Event ID to edit:", NULL, FALSE, FALSE,
                         NULL, "Next", G_CALLBACK(edit_id_prompt_cb), "Cancel",
                         NULL, find_matrix_account(), NULL, NULL,
                         g_strdup(room_id));
  }
}

static void redact_id_prompt_cb(void *user_data, const char *event_id) {
  char *room_id = (char *)user_data;
  if (event_id && *event_id) {
    purple_matrix_rust_redact_event(
        purple_account_get_username(find_matrix_account()), room_id, event_id,
        "Redacted by user request");
  }
  g_free(room_id);
}

static void menu_action_redact_id_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    purple_request_input(
        find_matrix_account(), "Redact by ID", "Enter Event ID",
        "Paste the Event ID to redact:", NULL, FALSE, FALSE, NULL, "Redact",
        G_CALLBACK(redact_id_prompt_cb), "Cancel", NULL, find_matrix_account(),
        NULL, NULL, g_strdup(room_id));
  }
}

static void menu_action_mark_read_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    purple_matrix_rust_send_read_receipt(
        purple_account_get_username(purple_chat_get_account(chat)), room_id,
        NULL); // NULL = latest
  }
}

static void menu_action_e2ee_status_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    purple_matrix_rust_e2ee_status(
        purple_account_get_username(purple_chat_get_account(chat)), room_id);
  }
}

static void menu_action_fetch_history_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    PurpleAccount *account = purple_chat_get_account(chat);
    purple_matrix_rust_fetch_more_history_with_limit(
        purple_account_get_username(account), room_id,
        get_history_page_size(account));
  }
}

static void menu_action_sync_recent_history_cb(PurpleBlistNode *node,
                                               gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (room_id) {
    PurpleAccount *account = purple_chat_get_account(chat);
    purple_matrix_rust_resync_recent_history_with_limit(
        purple_account_get_username(account), room_id,
        get_history_page_size(account));
  }
}

typedef struct {
  char *output_room_id;
  PurpleAccount *account;
} PreviewRoomContext;

static void free_preview_room_context(PreviewRoomContext *ctx) {
  if (!ctx)
    return;
  g_free(ctx->output_room_id);
  g_free(ctx);
}

static void preview_room_input_cb(void *user_data, const char *target_room) {
  PreviewRoomContext *ctx = (PreviewRoomContext *)user_data;
  if (ctx && target_room && *target_room) {
    purple_matrix_rust_preview_room(purple_account_get_username(ctx->account),
                                    target_room, ctx->output_room_id);
  }
  free_preview_room_context(ctx);
}

static void preview_room_cancel_cb(void *user_data) {
  free_preview_room_context((PreviewRoomContext *)user_data);
}

static void menu_action_preview_room_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  PurpleAccount *account = purple_chat_get_account(chat);
  GHashTable *components = purple_chat_get_components(chat);
  const char *output_room_id = g_hash_table_lookup(components, "room_id");
  if (!output_room_id)
    return;

  PreviewRoomContext *ctx = g_new0(PreviewRoomContext, 1);
  ctx->output_room_id = g_strdup(output_room_id);
  ctx->account = account;

  purple_request_input(account, "Preview Room", "Room ID or Alias",
                       "Enter a room ID (!room:server) or alias (#room:server)",
                       NULL, FALSE, FALSE, NULL, "Preview",
                       G_CALLBACK(preview_room_input_cb), "Cancel",
                       G_CALLBACK(preview_room_cancel_cb), account, NULL, NULL,
                       ctx);
}

static void menu_action_verify_buddy_cb(PurpleBlistNode *node, gpointer data) {
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  const char *name = purple_buddy_get_name(buddy);
  if (name) {
    purple_matrix_rust_verify_user(
        purple_account_get_username(purple_buddy_get_account(buddy)), name);
  }
}

static void menu_action_tag_fav_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  purple_matrix_rust_set_room_tag(
      purple_account_get_username(purple_chat_get_account(chat)), room_id,
      "favorite");
}

static void menu_action_tag_low_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  purple_matrix_rust_set_room_tag(
      purple_account_get_username(purple_chat_get_account(chat)), room_id,
      "lowpriority");
}

static void menu_action_untag_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  purple_matrix_rust_set_room_tag(
      purple_account_get_username(purple_chat_get_account(chat)), room_id,
      "none");
}

static void kick_user_input_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  const char *who = purple_request_fields_get_string(fields, "who");
  const char *reason = purple_request_fields_get_string(fields, "reason");
  if (who && *who) {
    purple_matrix_rust_kick_user(
        purple_account_get_username(find_matrix_account()), room_id, who,
        reason);
  }
  g_free(room_id);
}

static void menu_action_kick_user_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id =
      g_hash_table_lookup(purple_chat_get_components(chat), "room_id");

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new("who", "User ID", NULL, FALSE));
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("reason", "Reason", NULL, FALSE));

  purple_request_fields(NULL, "Kick User", "Kick user from room", NULL, fields,
                        "Kick", G_CALLBACK(kick_user_input_cb), "Cancel", NULL,
                        purple_chat_get_account(chat), NULL, NULL,
                        g_strdup(room_id));
}

static void ban_user_input_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  const char *who = purple_request_fields_get_string(fields, "who");
  const char *reason = purple_request_fields_get_string(fields, "reason");
  if (who && *who) {
    purple_matrix_rust_ban_user(
        purple_account_get_username(find_matrix_account()), room_id, who,
        reason);
  }
  g_free(room_id);
}

static void menu_action_ban_user_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id =
      g_hash_table_lookup(purple_chat_get_components(chat), "room_id");

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new("who", "User ID", NULL, FALSE));
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("reason", "Reason", NULL, FALSE));

  purple_request_fields(NULL, "Ban User", "Ban user from room", NULL, fields,
                        "Ban", G_CALLBACK(ban_user_input_cb), "Cancel", NULL,
                        purple_chat_get_account(chat), NULL, NULL,
                        g_strdup(room_id));
}

static void unban_user_input_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  const char *who = purple_request_fields_get_string(fields, "who");
  const char *reason = purple_request_fields_get_string(fields, "reason");
  if (who && *who) {
    purple_matrix_rust_unban_user(
        purple_account_get_username(find_matrix_account()), room_id, who,
        reason);
  }
  g_free(room_id);
}

static void menu_action_unban_user_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id =
      g_hash_table_lookup(purple_chat_get_components(chat), "room_id");

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new("who", "User ID", NULL, FALSE));
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("reason", "Reason", NULL, FALSE));

  purple_request_fields(NULL, "Unban User", "Unban user from room", NULL,
                        fields, "Unban", G_CALLBACK(unban_user_input_cb),
                        "Cancel", NULL, purple_chat_get_account(chat), NULL,
                        NULL, g_strdup(room_id));
}

static void set_power_level_input_cb(void *user_data,
                                     PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  const char *who = purple_request_fields_get_string(fields, "who");
  const char *level_str = purple_request_fields_get_string(fields, "level");
  if (who && *who && level_str && *level_str) {
    long long level = atoll(level_str);
    purple_matrix_rust_set_power_level(
        purple_account_get_username(find_matrix_account()), room_id, who,
        level);
  }
  g_free(room_id);
}

static void menu_action_set_power_level_cb(PurpleBlistNode *node,
                                           gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id =
      g_hash_table_lookup(purple_chat_get_components(chat), "room_id");

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new("who", "User ID", NULL, FALSE));
  purple_request_field_group_add_field(
      group,
      purple_request_field_string_new("level", "Power Level", "50", FALSE));

  purple_request_fields(NULL, "Set Power Level", "Set room power level", NULL,
                        fields, "Set", G_CALLBACK(set_power_level_input_cb),
                        "Cancel", NULL, purple_chat_get_account(chat), NULL,
                        NULL, g_strdup(room_id));
}

static void alias_create_input_cb(void *user_data, const char *alias) {
  char *room_id = (char *)user_data;
  if (alias && *alias) {
    purple_matrix_rust_create_alias(
        purple_account_get_username(find_matrix_account()), room_id, alias);
  }
  g_free(room_id);
}

static void menu_action_alias_create_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  const char *room_id =
      g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
  if (!room_id)
    return;

  purple_request_input(find_matrix_account(), "Create Alias", "Enter Alias",
                       "Example: #my-room:example.org", NULL, FALSE, FALSE,
                       NULL, "Create", G_CALLBACK(alias_create_input_cb),
                       "Cancel", NULL, find_matrix_account(), NULL, NULL,
                       g_strdup(room_id));
}

static void alias_delete_input_cb(void *user_data, const char *alias) {
  if (alias && *alias) {
    purple_matrix_rust_delete_alias(
        purple_account_get_username(find_matrix_account()), alias);
  }
}

static void menu_action_alias_delete_cb(PurpleBlistNode *node, gpointer data) {
  purple_request_input(find_matrix_account(), "Delete Alias", "Enter Alias",
                       "Example: #my-room:example.org", NULL, FALSE, FALSE,
                       NULL, "Delete", G_CALLBACK(alias_delete_input_cb),
                       "Cancel", NULL, find_matrix_account(), NULL, NULL, NULL);
}

static void export_keys_fields_cb(void *user_data, PurpleRequestFields *fields) {
  PurpleAccount *account = (PurpleAccount *)user_data;
  const char *path = purple_request_fields_get_string(fields, "path");
  const char *passphrase =
      purple_request_fields_get_string(fields, "passphrase");
  if (account && path && *path && passphrase && *passphrase) {
    purple_matrix_rust_export_room_keys(purple_account_get_username(account),
                                        path, passphrase);
  }
}

static void menu_action_export_keys_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  PurpleAccount *account = purple_chat_get_account(chat);

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  purple_request_field_group_add_field(
      group, purple_request_field_string_new("path", "Output File Path",
                                             "~/.purple/matrix-keys.txt",
                                             FALSE));
  purple_request_field_group_add_field(
      group, purple_request_field_string_new("passphrase", "Passphrase", NULL,
                                             FALSE));

  purple_request_fields(account, "Export Room Keys", "Export E2EE Keys", NULL,
                        fields, "Export", G_CALLBACK(export_keys_fields_cb),
                        "Cancel", NULL, account, NULL, NULL, account);
}

static void create_poll_cb(void *user_data, PurpleRequestFields *fields) {
  char *room_id = (char *)user_data;
  const char *q = purple_request_fields_get_string(fields, "question");
  const char *opt1 = purple_request_fields_get_string(fields, "opt1");
  const char *opt2 = purple_request_fields_get_string(fields, "opt2");

  if (q && *q && opt1 && *opt1 && opt2 && *opt2) {
    GString *opts = g_string_new(opt1);
    g_string_append_printf(opts, "|%s", opt2);

    const char *opt3 = purple_request_fields_get_string(fields, "opt3");
    if (opt3 && *opt3)
      g_string_append_printf(opts, "|%s", opt3);
    const char *opt4 = purple_request_fields_get_string(fields, "opt4");
    if (opt4 && *opt4)
      g_string_append_printf(opts, "|%s", opt4);

    purple_matrix_rust_poll_create(
        purple_account_get_username(find_matrix_account()), room_id, q,
        opts->str);
    g_string_free(opts, TRUE);
  }
  g_free(room_id);
}

static void menu_action_create_poll_cb(PurpleBlistNode *node, gpointer data) {
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  if (!room_id)
    return;

  PurpleAccount *account = purple_chat_get_account(chat);
  PurpleConnection *gc = purple_account_get_connection(account);

  PurpleRequestFields *fields = purple_request_fields_new();
  PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
  purple_request_fields_add_group(fields, group);

  PurpleRequestField *field;
  field = purple_request_field_string_new("question", "Question", NULL, FALSE);
  purple_request_field_set_required(field, TRUE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("opt1", "Option 1", NULL, FALSE);
  purple_request_field_set_required(field, TRUE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("opt2", "Option 2", NULL, FALSE);
  purple_request_field_set_required(field, TRUE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("opt3", "Option 3", NULL, FALSE);
  purple_request_field_group_add_field(group, field);

  field = purple_request_field_string_new("opt4", "Option 4", NULL, FALSE);
  purple_request_field_group_add_field(group, field);

  purple_request_fields(gc, "Create Poll", "Create a new Poll", NULL, fields,
                        "Create", G_CALLBACK(create_poll_cb), "Cancel", NULL,
                        account, NULL, NULL, g_strdup(room_id));
}

static GList *blist_node_menu_cb(PurpleBlistNode *node) {
  GList *list = NULL;

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    PurpleChat *chat = (PurpleChat *)node;
    PurpleAccount *account = purple_chat_get_account(chat);

    // Check if it's our account
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") ==
        0) {

      // --- General & View ---
      PurpleMenuAction *act_settings = purple_menu_action_new(
          "Room Settings...", PURPLE_CALLBACK(menu_action_room_settings_cb),
          NULL, NULL);
      list = g_list_append(list, act_settings);

      PurpleMenuAction *act_read = purple_menu_action_new(
          "Mark as Read", PURPLE_CALLBACK(menu_action_mark_read_cb), NULL,
          NULL);
      list = g_list_append(list, act_read);

      PurpleMenuAction *act_history = purple_menu_action_new(
          "Fetch More History", PURPLE_CALLBACK(menu_action_fetch_history_cb),
          NULL, NULL);
      list = g_list_append(list, act_history);

      PurpleMenuAction *act_sync_recent = purple_menu_action_new(
          "Sync Recent History (Reset)",
          PURPLE_CALLBACK(menu_action_sync_recent_history_cb), NULL, NULL);
      list = g_list_append(list, act_sync_recent);

      PurpleMenuAction *act_preview = purple_menu_action_new(
          "Preview Room...", PURPLE_CALLBACK(menu_action_preview_room_cb), NULL,
          NULL);
      list = g_list_append(list, act_preview);

      PurpleMenuAction *act_e2ee = purple_menu_action_new(
          "Encryption Status (E2EE)",
          PURPLE_CALLBACK(menu_action_e2ee_status_cb), NULL, NULL);
      list = g_list_append(list, act_e2ee);

      // --- Interaction ---
      PurpleMenuAction *act_reply = purple_menu_action_new(
          "Reply (Last Msg)...", PURPLE_CALLBACK(menu_action_reply_cb), NULL,
          NULL);
      list = g_list_append(list, act_reply);

      PurpleMenuAction *act_thread = purple_menu_action_new(
          "Reply to Thread (Last Msg)...",
          PURPLE_CALLBACK(menu_action_thread_reply_cb), NULL, NULL);
      list = g_list_append(list, act_thread);

      PurpleMenuAction *act_list_threads = purple_menu_action_new(
          "List Threads", PURPLE_CALLBACK(menu_action_list_threads_cb), NULL,
          NULL);
      list = g_list_append(list, act_list_threads);

      PurpleMenuAction *act_polls = purple_menu_action_new(
          "Active Polls", PURPLE_CALLBACK(menu_action_active_polls_cb), NULL,
          NULL);
      list = g_list_append(list, act_polls);

      PurpleMenuAction *act_edit = purple_menu_action_new(
          "Edit Last Msg...", PURPLE_CALLBACK(menu_action_edit_cb), NULL, NULL);
      list = g_list_append(list, act_edit);

      PurpleMenuAction *act_edit_id = purple_menu_action_new(
          "Edit Msg by ID...", PURPLE_CALLBACK(menu_action_edit_id_cb), NULL,
          NULL);
      list = g_list_append(list, act_edit_id);

      PurpleMenuAction *act_redact_id = purple_menu_action_new(
          "Redact Msg by ID...", PURPLE_CALLBACK(menu_action_redact_id_cb),
          NULL, NULL);
      list = g_list_append(list, act_redact_id);

      PurpleMenuAction *act_send_sticker = purple_menu_action_new(
          "Send Sticker...", PURPLE_CALLBACK(blist_action_send_sticker_cb),
          NULL, NULL);
      list = g_list_append(list, act_send_sticker);

      // --- Reactions ---
      list = g_list_append(
          list, purple_menu_action_new(
                    "React: 👍", PURPLE_CALLBACK(menu_action_react_quick_cb),
                    "👍", NULL));
      list = g_list_append(
          list, purple_menu_action_new(
                    "React: 👎", PURPLE_CALLBACK(menu_action_react_quick_cb),
                    "👎", NULL));
      list = g_list_append(
          list, purple_menu_action_new(
                    "React: ❤️", PURPLE_CALLBACK(menu_action_react_quick_cb),
                    "❤️", NULL));
      list = g_list_append(
          list, purple_menu_action_new(
                    "React: 😂", PURPLE_CALLBACK(menu_action_react_quick_cb),
                    "😂", NULL));
      list = g_list_append(
          list, purple_menu_action_new(
                    "React: 🎉", PURPLE_CALLBACK(menu_action_react_quick_cb),
                    "🎉", NULL));
      list = g_list_append(
          list, purple_menu_action_new(
                    "React: Custom...",
                    PURPLE_CALLBACK(menu_action_react_custom_cb), NULL, NULL));

      PurpleMenuAction *act_poll = purple_menu_action_new(
          "Create Poll...", PURPLE_CALLBACK(menu_action_create_poll_cb), NULL,
          NULL);
      list = g_list_append(list, act_poll);

      PurpleMenuAction *act_redact = purple_menu_action_new(
          "Redact Last Msg", PURPLE_CALLBACK(menu_action_redact_last_cb), NULL,
          NULL);
      list = g_list_append(list, act_redact);

      // --- Organization (Tags) ---
      PurpleMenuAction *act_fav = purple_menu_action_new(
          "Tag: Favorite", PURPLE_CALLBACK(menu_action_tag_fav_cb), NULL, NULL);
      list = g_list_append(list, act_fav);

      PurpleMenuAction *act_low = purple_menu_action_new(
          "Tag: Low Priority", PURPLE_CALLBACK(menu_action_tag_low_cb), NULL,
          NULL);
      list = g_list_append(list, act_low);

      PurpleMenuAction *act_untag = purple_menu_action_new(
          "Tag: None (Untag)", PURPLE_CALLBACK(menu_action_untag_cb), NULL,
          NULL);
      list = g_list_append(list, act_untag);

      // --- Moderation ---
      PurpleMenuAction *act_kick = purple_menu_action_new(
          "Mod: Kick User...", PURPLE_CALLBACK(menu_action_kick_user_cb), NULL,
          NULL);
      list = g_list_append(list, act_kick);

      PurpleMenuAction *act_ban = purple_menu_action_new(
          "Mod: Ban User...", PURPLE_CALLBACK(menu_action_ban_user_cb), NULL,
          NULL);
      list = g_list_append(list, act_ban);

      PurpleMenuAction *act_unban = purple_menu_action_new(
          "Mod: Unban User...", PURPLE_CALLBACK(menu_action_unban_user_cb),
          NULL, NULL);
      list = g_list_append(list, act_unban);

      PurpleMenuAction *act_power = purple_menu_action_new(
          "Mod: Set Power Level...",
          PURPLE_CALLBACK(menu_action_set_power_level_cb), NULL, NULL);
      list = g_list_append(list, act_power);

      PurpleMenuAction *act_alias_create = purple_menu_action_new(
          "Alias: Create...", PURPLE_CALLBACK(menu_action_alias_create_cb),
          NULL, NULL);
      list = g_list_append(list, act_alias_create);

      PurpleMenuAction *act_alias_delete = purple_menu_action_new(
          "Alias: Delete...", PURPLE_CALLBACK(menu_action_alias_delete_cb),
          NULL, NULL);
      list = g_list_append(list, act_alias_delete);

      PurpleMenuAction *act_export_keys = purple_menu_action_new(
          "Security: Export Room Keys...",
          PURPLE_CALLBACK(menu_action_export_keys_cb), NULL, NULL);
      list = g_list_append(list, act_export_keys);
    }
  } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy = (PurpleBuddy *)node;
    PurpleAccount *account = purple_buddy_get_account(buddy);

    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") ==
        0) {
      // Verify User
      PurpleMenuAction *act_verify = purple_menu_action_new(
          "Verify User", PURPLE_CALLBACK(menu_action_verify_buddy_cb), NULL,
          NULL);
      list = g_list_append(list, act_verify);
    }
  }

  return list;
}

// Debug Command: /debug_thread
static PurpleCmdRet cmd_debug_thread(PurpleConversation *conv, const gchar *cmd,
                                     gchar **args, gchar **error, void *data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  const char *room_id = purple_conversation_get_name(conv);

  purple_debug_info("purple-matrix-rust", "Running /debug_thread test...\n");

  // Simulate a threaded message
  char *virtual_id = g_strdup_printf("%s|debug_thread_123", room_id);
  ensure_thread_in_blist(account, virtual_id, "Debug Thread Message", room_id);

  g_free(virtual_id);
  return PURPLE_CMD_RET_OK;
}

static gboolean process_sso_cb(gpointer data) {
  char *url = (char *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    void *handle = gc ? (void *)gc : (void *)my_plugin;

    purple_notify_uri(handle, url);

    char *msg = g_strdup_printf(
        "A browser window has been opened to:\n%s\n\n"
        "Please complete login in your browser.\n"
        "Pidgin will finish login automatically after the browser redirects "
        "back to localhost.\n\n"
        "If no callback is received within 3 minutes, login will time out and "
        "you can retry.",
        url);
    purple_notify_info(handle, "SSO Login", "Login Required", msg);
    g_free(msg);
  }
  g_free(url);
  return FALSE;
}

static void sso_url_cb(const char *url) {
  purple_debug_info("purple-matrix-rust",
                    "SSO URL received: %s. Scheduling UI update.\n", url);
  g_idle_add(process_sso_cb, g_strdup(url));
}

static void init_plugin(PurplePlugin *plugin) {
  PurpleAccountOption *option;

  printf("[Plugin] init_plugin called!\n");
  option = purple_account_option_string_new("Homeserver URL", "server",
                                            "https://matrix.org");
  prpl_info.protocol_options =
      g_list_append(prpl_info.protocol_options, option);

  option = purple_account_option_bool_new("Separate Thread Tabs",
                                          "separate_threads", FALSE);
  prpl_info.protocol_options =
      g_list_append(prpl_info.protocol_options, option);

  option = purple_account_option_bool_new("Auto Fetch History On Open",
                                          "auto_fetch_history_on_open", TRUE);
  prpl_info.protocol_options =
      g_list_append(prpl_info.protocol_options, option);

  option = purple_account_option_string_new("History Page Size",
                                            "history_page_size", "50");
  prpl_info.protocol_options =
      g_list_append(prpl_info.protocol_options, option);

  // URI Handler removed as not supported
  // purple_signal_connect(purple_get_core(), "uri-handler", plugin,
  // PURPLE_CALLBACK(matrix_uri_handler), NULL);

  purple_cmd_register(
      "matrix_thread", "s", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_thread,
      "matrix_thread <message>: Reply to a thread on the last message", NULL);

  purple_cmd_register(
      "matrix_threads", "", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_list_threads,
      "matrix_threads: List active threads in the current room", NULL);

  purple_cmd_register(
      "matrix_set_separate_threads", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_set_separate_threads,
      "matrix_set_separate_threads <on|off>: Toggle separate thread tabs",
      NULL);

  purple_cmd_register(
      "matrix_reply", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, NULL, cmd_reply,
      "matrix_reply &lt;message&gt;: Reply to last message", NULL);

  purple_cmd_register("matrix_edit", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, NULL, cmd_edit,
                      "matrix_edit &lt;message&gt;: Edit last message", NULL);

  purple_cmd_register(
      "matrix_verify", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_verify,
      "matrix_verify <user_id> [device_id]: Verify a user", NULL);

  purple_cmd_register("matrix_recover_keys", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_recover_keys,
                      "matrix_recover_keys <passphrase>: Recover keys from 4S",
                      NULL);

  purple_cmd_register("matrix_read_receipt", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_read_receipt,
                      "matrix_read_receipt: Send read receipt for last message",
                      NULL);

  purple_cmd_register("matrix_react", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_react,
                      "matrix_react <emoji>: React to last message", NULL);

  purple_cmd_register("matrix_redact", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_redact,
                      "matrix_redact [reason]: Redact last message", NULL);

  purple_cmd_register("matrix_e2ee_status", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_e2ee_status,
                      "matrix_e2ee_status: Check cross-signing status", NULL);

  purple_cmd_register("matrix_reset_cross_signing", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_reset_cross_signing,
                      "matrix_reset_cross_signing: Bootstrap cross-signing",
                      NULL);

  purple_cmd_register("matrix_nick", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_nick,
                      "matrix_nick <name>: Set display name", NULL);

  purple_cmd_register("matrix_avatar", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_avatar,
                      "matrix_avatar <path>: Set avatar", NULL);

  purple_cmd_register("matrix_history", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_IM,
                      "prpl-matrix-rust", cmd_history,
                      _("matrix_history: Fetch more history from the server"),
                      NULL);

  purple_cmd_register(
      "matrix_preview_room", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_preview_room,
      "matrix_preview_room <room_id_or_alias>: Preview room details and activity",
      NULL);

  purple_cmd_register("matrix_set_history_page_size", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_set_history_page_size,
                      "matrix_set_history_page_size <1-500>: Set history page "
                      "size for fetches",
                      NULL);

  purple_cmd_register(
      "matrix_set_auto_fetch_history", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_set_auto_fetch_history,
      "matrix_set_auto_fetch_history <on|off>: Toggle auto history fetch on "
      "conversation open",
      NULL);

  purple_cmd_register("matrix_create_room", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_create_room,
                      "matrix_create_room <name>: Create a new Matrix room",
                      NULL);

  purple_cmd_register("matrix_leave", "s", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_leave,
                      "matrix_leave [reason]: Leave the current Matrix room",
                      NULL);

  purple_cmd_register(
      "matrix_public_rooms", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_public_rooms,
      "matrix_public_rooms [search]: Search for public Matrix rooms", NULL);

  purple_cmd_register(
      "matrix_user_search", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_user_search,
      "matrix_user_search <term>: Search for users in the directory", NULL);

  purple_cmd_register(
      "matrix_kick", "ws", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_kick,
      "matrix_kick <user_id> [reason]: Kick a user from the room", NULL);

  purple_cmd_register(
      "matrix_ban", "ws", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_ban,
      "matrix_ban <user_id> [reason]: Ban a user from the room", NULL);

  purple_cmd_register(
      "matrix_invite", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_invite,
      "matrix_invite <user_id>: Invite a user to the room", NULL);

  purple_cmd_register(
      "matrix_tag", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_tag,
      "matrix_tag <favorite|lowpriority|none>: Set a tag on the room", NULL);

  purple_cmd_register("matrix_ignore", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_ignore,
                      "matrix_ignore <user_id>: Ignore a user", NULL);

  purple_cmd_register("matrix_power_levels", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_power_levels,
                      "matrix_power_levels: View current room power levels",
                      NULL);

  purple_cmd_register(
      "matrix_set_power_level", "ws", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_set_power_level,
      "matrix_set_power_level <user_id> <level>: Set power level for a user",
      NULL);

  purple_cmd_register("matrix_set_name", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_set_name,
                      "matrix_set_name <name>: Rename the Matrix room", NULL);

  purple_cmd_register("matrix_set_topic", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_set_topic,
                      "matrix_set_topic <topic>: Set the Matrix room topic",
                      NULL);

  purple_cmd_register("matrix_alias_create", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_alias_create,
                      "matrix_alias_create <alias>: Create a room alias", NULL);

  purple_cmd_register("matrix_alias_delete", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_alias_delete,
                      "matrix_alias_delete <alias>: Remove a room alias", NULL);

  purple_cmd_register(
      "matrix_report", "ws", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_report,
      "matrix_report <event_id> [reason]: Report content to moderators", NULL);

  purple_cmd_register(
      "matrix_export_keys", "ww", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_export_keys,
      "matrix_export_keys <path> <passphrase>: Export room keys", NULL);

  purple_cmd_register(
      "matrix_bulk_redact", "ws", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_bulk_redact,
      "matrix_bulk_redact <count> [reason]: Redact multiple recent messages",
      NULL);

  purple_cmd_register(
      "matrix_knock", "ws", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_knock,
      "matrix_knock <room_id_or_alias> [reason]: Knock on a restricted room",
      NULL);

  purple_cmd_register("matrix_unban", "ws", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_unban,
                      "matrix_unban <user_id> [reason]: Unban a user", NULL);

  purple_cmd_register(
      "matrix_set_avatar", "w", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_set_room_avatar,
      "matrix_set_avatar <path_to_image>: Set the room avatar/icon", NULL);

  purple_cmd_register("matrix_mute", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_mute,
                      "matrix_mute: Mute notifications for this room", NULL);

  purple_cmd_register(
      "matrix_unmute", "", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_unmute,
      "matrix_unmute: Unmute notifications for this room", NULL);

  purple_cmd_register(
      "matrix_logout", "", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust", cmd_logout,
      "matrix_logout: Explicitly invalidate session and logout", NULL);

  purple_cmd_register(
      "matrix_reconnect", "", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_reconnect,
      "matrix_reconnect: Reconnect the Matrix account and retry login", NULL);

  purple_cmd_register("matrix_debug_thread", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_debug_thread,
                      "matrix_debug_thread: Simulate a thread for debugging",
                      NULL);

  purple_cmd_register("matrix_help", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_help,
                      "matrix_help: List all available Matrix commands", NULL);

  purple_cmd_register(
      "matrix_location", "ww", PURPLE_CMD_P_PLUGIN,
      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT, "prpl-matrix-rust",
      cmd_location,
      "matrix_location <description> <geo_uri>: Send a location message", NULL);

  purple_cmd_register("matrix_poll_create", "ww", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_poll_create,
                      "matrix_poll_create <question> <options>: Create a poll "
                      "(options pipe-separated)",
                      NULL);

  purple_cmd_register("matrix_poll_vote", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_poll_vote,
                      "matrix_poll_vote <index>: Vote on the last poll", NULL);

  purple_cmd_register("matrix_poll_end", "", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_poll_end,
                      "matrix_poll_end: End the last poll", NULL);

  purple_cmd_register("matrix_sticker", "w", PURPLE_CMD_P_PLUGIN,
                      PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT,
                      "prpl-matrix-rust", cmd_sticker,
                      "matrix_sticker <url>: Send a sticker", NULL);
}

extern void purple_matrix_rust_init_verification_cbs(
    void (*req_cb)(const char *transaction_id, const char *device_id),
    void (*emoji_cb)(const char *transaction_id, const char *emoji,
                     const char *decimals),
    void (*qr_cb)(const char *user_id, const char *html_data));
extern void purple_matrix_rust_init_sso_cb(void (*cb)(const char *url));
extern void purple_matrix_rust_init_connected_cb(void (*cb)());
extern void
purple_matrix_rust_init_login_failed_cb(void (*cb)(const char *reason));
extern void purple_matrix_rust_finish_sso(const char *token);

extern void purple_matrix_rust_init_msg_cb(
    void (*cb)(const char *, const char *, const char *, const char *));

typedef void (*PresenceCallback)(const char *user_id, bool is_online);
extern void purple_matrix_rust_set_presence_callback(PresenceCallback cb);

// Presence Update Callback
typedef struct {
  char *user_id;
  bool is_online;
} MatrixPresenceData;

static gboolean process_presence_cb(gpointer data) {
  MatrixPresenceData *d = (MatrixPresenceData *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    // Update status
    // For now, simple toggling of "available" vs "offline"
    // Ideally we map states better.
    const char *status_id = d->is_online ? "available" : "offline";

    // purple_prpl_got_user_status(account, name, status_id, ...)
    purple_prpl_got_user_status(account, d->user_id, status_id, NULL);
  }

  g_free(d->user_id);
  g_free(d);
  return FALSE;
}

static void presence_callback(const char *user_id, bool is_online) {
  MatrixPresenceData *d = g_new0(MatrixPresenceData, 1);
  d->user_id = g_strdup(user_id);
  d->is_online = is_online;
  g_idle_add(process_presence_cb, d);
}

// Idle callback for Login Failure
static gboolean process_login_failed_cb(gpointer data) {
  char *reason = (char *)data;
  purple_debug_error("purple-matrix-rust", "Login Failed Callback: %s\n",
                     reason);

  gboolean notified = FALSE;
  GList *connections = purple_connections_get_all();
  for (GList *l = connections; l != NULL; l = l->next) {
    PurpleConnection *gc = (PurpleConnection *)l->data;
    PurpleAccount *account = purple_connection_get_account(gc);
    if (!account)
      continue;
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") != 0)
      continue;

    if (purple_connection_get_state(gc) == PURPLE_CONNECTING ||
        purple_connection_get_state(gc) == PURPLE_CONNECTED) {
      purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                     reason);
      notified = TRUE;
    }
  }

  if (!notified) {
    PurpleAccount *account = find_matrix_account();
    if (account) {
      PurpleConnection *gc = purple_account_get_connection(account);
      if (gc) {
        purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                       reason);
      }
    }
  }

  if (g_strrstr(reason, "session expired") ||
      g_strrstr(reason, "token invalidated") ||
      g_strrstr(reason, "M_UNKNOWN_TOKEN")) {
    purple_notify_info(
        NULL, "Matrix Login", "Session Expired",
        "Your Matrix session expired. Please sign in again.\n"
        "Use 'Reconnect / Re-Login' in the account actions menu, or run "
        "/matrix_reconnect.");
  }
  g_free(reason);
  return FALSE;
}

static void login_failed_cb(const char *reason) {
  g_idle_add(process_login_failed_cb, g_strdup(reason));
}

// Chat Topic and User Callbacks
typedef void (*ChatTopicCallback)(const char *room_id, const char *topic,
                                  const char *sender);
extern void purple_matrix_rust_set_chat_topic_callback(ChatTopicCallback cb);

typedef void (*ChatUserCallback)(const char *room_id, const char *user_id,
                                 bool add, const char *alias,
                                 const char *avatar_path);
extern void purple_matrix_rust_set_chat_user_callback(ChatUserCallback cb);

typedef struct {
  char *room_id;
  char *topic;
  char *sender;
} MatrixTopicData;

static gboolean process_chat_topic_cb(gpointer data) {
  MatrixTopicData *d = (MatrixTopicData *)data;
  PurpleAccount *account = find_matrix_account();
  PurpleConnection *gc =
      account ? purple_account_get_connection(account) : NULL;

  if (gc) {
    int id = get_chat_id(d->room_id);
    PurpleConversation *conv = purple_find_chat(gc, id);
    if (conv) {
      purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), d->sender, d->topic);
    }
  }
  g_free(d->room_id);
  g_free(d->topic);
  g_free(d->sender);
  g_free(d);
  return FALSE;
}

static void chat_topic_callback(const char *room_id, const char *topic,
                                const char *sender) {
  MatrixTopicData *d = g_new0(MatrixTopicData, 1);
  d->room_id = g_strdup(room_id);
  d->topic = g_strdup(topic);
  d->sender = g_strdup(sender);
  g_idle_add(process_chat_topic_cb, d);
}

typedef struct {
  char *room_id;
  char *user_id;
  bool add;
  char *alias;
  char *avatar_path;
} MatrixChatUserData;

static gboolean process_chat_user_cb(gpointer data) {
  MatrixChatUserData *d = (MatrixChatUserData *)data;
  PurpleAccount *account = find_matrix_account();
  PurpleConnection *gc =
      account ? purple_account_get_connection(account) : NULL;

  if (gc) {
    int id = get_chat_id(d->room_id);
    PurpleConversation *conv = purple_find_chat(gc, id);
    if (conv) {
      if (d->add) {
        if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), d->user_id)) {
          // Add user with alias if present, otherwise user_id
          const char *display_name =
              (d->alias && strlen(d->alias) > 0) ? d->alias : NULL;
          // Flags: PURPLE_CBFLAGS_NONE.
          purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), d->user_id,
                                    display_name, PURPLE_CBFLAGS_NONE, FALSE);

          // Set Avatar if provided
          if (d->avatar_path && strlen(d->avatar_path) > 0) {
            gchar *contents;
            gsize length;
            if (g_file_get_contents(d->avatar_path, &contents, &length, NULL)) {
              purple_buddy_icons_set_for_user(account, d->user_id, contents,
                                              length, NULL);
              g_free(contents);
            }
          }
        }
      } else {
        purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv), d->user_id, NULL);
      }
    }
  }
  g_free(d->room_id);
  g_free(d->user_id);
  if (d->alias)
    g_free(d->alias);
  if (d->avatar_path)
    g_free(d->avatar_path);
  g_free(d);
  return FALSE;
}

static void chat_user_callback(const char *room_id, const char *user_id,
                               bool add, const char *alias,
                               const char *avatar_path) {
  MatrixChatUserData *d = g_new0(MatrixChatUserData, 1);
  d->room_id = g_strdup(room_id);
  d->user_id = g_strdup(user_id);
  d->add = add;
  if (alias)
    d->alias = g_strdup(alias);
  if (avatar_path)
    d->avatar_path = g_strdup(avatar_path);
  g_idle_add(process_chat_user_cb, d);
}

// Idle callback for Connected event
static gboolean process_connected_cb(gpointer data) {
  purple_debug_info("purple-matrix-rust",
                    "Process Connected Callback (Main Thread)\n");
  GList *connections = purple_connections_get_all();

  for (GList *l = connections; l != NULL; l = l->next) {
    PurpleConnection *gc = (PurpleConnection *)l->data;
    PurpleAccount *account = purple_connection_get_account(gc);
    const char *proto_id = purple_account_get_protocol_id(account);
    if (strcmp(proto_id, "prpl-matrix-rust") == 0) {
      cleanup_stale_thread_labels(account);
      if (purple_connection_get_state(gc) == PURPLE_CONNECTING) {
        purple_connection_set_state(gc, PURPLE_CONNECTED);
        purple_debug_info("purple-matrix-rust",
                          "State updated to CONNECTED for account %s\n",
                          purple_account_get_username(account));
      }
    }
  }
  return FALSE;
}

static void connected_cb() {
  purple_debug_info(
      "purple-matrix-rust",
      "Received connected callback from Rust. Scheduling update.\n");
  g_idle_add(process_connected_cb, NULL);
}

// QR Verification Callback
typedef struct {
  char *user_id;
  char *html_data;
} MatrixQrData;

static gboolean process_qr_cb(gpointer data) {
  MatrixQrData *d = (MatrixQrData *)data;
  PurpleAccount *account = find_matrix_account();

  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    void *handle = gc ? (void *)gc : (void *)my_plugin;

    purple_notify_formatted(handle, "Verification QR Code", "Scan to Verify",
                            NULL, d->html_data, NULL, NULL);
  }

  g_free(d->user_id);
  g_free(d->html_data);
  g_free(d);
  return FALSE;
}

static void show_verification_qr_cb(const char *user_id,
                                    const char *html_data) {
  MatrixQrData *d = g_new0(MatrixQrData, 1);
  d->user_id = g_strdup(user_id);
  d->html_data = g_strdup(html_data);
  g_idle_add(process_qr_cb, d);
}

typedef struct {
  char *room_id_or_alias;
  char *html_body;
} MatrixRoomPreviewData;

static gboolean process_room_preview_cb(gpointer data) {
  MatrixRoomPreviewData *d = (MatrixRoomPreviewData *)data;
  // Preview details are rendered into chat windows (room context or
  // MATRIX_PREVIEW_ROOM_ID). Keep this callback for future UI hooks.
  purple_debug_info("purple-matrix-rust", "Room preview callback: %s\n",
                    d->room_id_or_alias);

  g_free(d->room_id_or_alias);
  g_free(d->html_body);
  g_free(d);
  return FALSE;
}

static void room_preview_cb(const char *room_id_or_alias,
                            const char *html_body) {
  MatrixRoomPreviewData *d = g_new0(MatrixRoomPreviewData, 1);
  d->room_id_or_alias = g_strdup(room_id_or_alias ? room_id_or_alias : "");
  d->html_body = g_strdup(html_body ? html_body : "");
  g_idle_add(process_room_preview_cb, d);
}

static gboolean plugin_load(PurplePlugin *plugin) {
  my_plugin = plugin;

  purple_matrix_rust_init();
  purple_matrix_rust_set_msg_callback(msg_callback);
  purple_matrix_rust_set_typing_callback(typing_callback);
  purple_matrix_rust_set_room_joined_callback(room_joined_callback);
  purple_matrix_rust_set_update_buddy_callback(update_buddy_callback);
  purple_matrix_rust_set_presence_callback(presence_callback);
  purple_matrix_rust_set_chat_topic_callback(chat_topic_callback);
  purple_matrix_rust_set_chat_user_callback(chat_user_callback);
  purple_matrix_rust_init_invite_cb(invite_callback);
  purple_matrix_rust_init_verification_cbs(sas_request_cb, sas_emoji_cb,
                                           show_verification_qr_cb);
  purple_matrix_rust_init_sso_cb(sso_url_cb);
  purple_matrix_rust_init_connected_cb(connected_cb);
  purple_matrix_rust_init_login_failed_cb(login_failed_cb);
  purple_matrix_rust_set_read_marker_callback(read_marker_cb);
  purple_matrix_rust_set_show_user_info_callback(show_user_info_cb);
  purple_matrix_rust_set_roomlist_add_callback(roomlist_add_cb);
  purple_matrix_rust_set_room_preview_callback(room_preview_cb);
  purple_matrix_rust_set_thread_list_callback(thread_list_cb);
  purple_matrix_rust_set_poll_list_callback(poll_list_cb);

  // These signals are not available on all libpurple builds (e.g. some
  // Pidgin 2.14 variants), and attempting to connect can leave signal system
  // warnings/errors. Keep lazy history available through explicit commands.

  return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) { return TRUE; }

static PurplePluginInfo info = {.magic = PURPLE_PLUGIN_MAGIC,
                                .major_version = PURPLE_MAJOR_VERSION,
                                .minor_version = PURPLE_MINOR_VERSION,
                                .type = PURPLE_PLUGIN_PROTOCOL,
                                .ui_requirement = NULL,
                                .flags = 0,
                                .dependencies = NULL,
                                .priority = PURPLE_PRIORITY_DEFAULT,
                                .id = "prpl-matrix-rust",
                                .name = "Matrix (Rust)",
                                .version = "0.1",
                                .summary =
                                    "Matrix Protocol Plugin (Rust Backend)",
                                .description = "Using matrix-rust-sdk",
                                .author = "Author Name",
                                .homepage = "https://matrix.org",
                                .load = plugin_load,
                                .unload = plugin_unload,
                                .destroy = NULL,
                                .ui_info = NULL,
                                .extra_info = &prpl_info,
                                .prefs_info = NULL,
                                .actions = matrix_actions};

#ifndef TESTING
PURPLE_INIT_PLUGIN(matrix_rust, init_plugin, info)
#endif
