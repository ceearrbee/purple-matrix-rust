#include "matrix_account.h"
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
#include <glib/gstdio.h>

typedef struct {
  char *user_id;
  char *target_user_id;
  char *display_name;
  char *avatar_url;
} MatrixUserInfoData;

typedef struct {
  char *user_id;
  char *room_id_or_alias;
  char *html_body;
} MatrixRoomPreviewData;

typedef struct {
  char *user_id;
  char *room_id;
  char *topic;
  char *sender;
} MatrixTopicData;

typedef struct {
  char *user_id;
  char *room_id;
  char *member_id;
  bool add;
  char *alias;
  char *avatar_path;
} MatrixChatUserData;

typedef struct {
  char *user_id;
  char *target_user_id;
  int status;
  char *status_msg;
} MatrixPresenceData;

struct VerificationData {
  char *user_id;
  char *flow_id;
};

typedef struct {
  char *user_id;
  char *flow_id;
  char *emojis;
} MatrixEmojiData;

typedef struct {
  char *user_id;
  char *html_data;
} MatrixQrData;

typedef struct {
  char *user_id;
  char *room_id;
  char *name;
  char *topic;
  uint64_t member_count;
  bool encrypted;
  int64_t power_level;
  char *alias;
} MatrixDashboardData;

void matrix_login(PurpleAccount *account) {
  PurpleConnection *gc = purple_account_get_connection(account);
  const char *username = purple_account_get_username(account);
  const char *password = purple_account_get_password(account);
  char *homeserver_derived = NULL;
  const char *homeserver = purple_account_get_string(account, "server", NULL);

  if (!homeserver || strcmp(homeserver, "https://matrix.org") == 0 ||
      strlen(homeserver) == 0) {
    const char *delimiter = strchr(username, ':');
    if (delimiter) {
      const char *domain = delimiter + 1;
      if (g_ascii_strcasecmp(domain, "matrix.org") != 0)
        homeserver_derived = g_strdup_printf("https://%s", domain);
    }
  }
  if (homeserver_derived)
    homeserver = homeserver_derived;
  if (!homeserver)
    homeserver = "https://matrix.org";

  /* SECURITY: The insecure_ssl option disables TLS certificate verification.
   * This is intentionally unavailable in release builds — it is only compiled
   * in when MATRIX_ALLOW_INSECURE_TLS is defined (e.g. for local test servers).
   * Never enable this in production. */
#ifdef MATRIX_ALLOW_INSECURE_TLS
  if (purple_account_get_bool(account, "insecure_ssl", FALSE)) {
    g_setenv("MATRIX_RUST_INSECURE_SSL", "1", TRUE);
    purple_notify_warning(
        gc, "Security Warning", "Insecure TLS Mode Enabled",
        "The insecure_ssl option is enabled. "
        "TLS certificate verification is DISABLED. Your connection may be "
        "insecure. This build was compiled with MATRIX_ALLOW_INSECURE_TLS.");
  } else {
    g_unsetenv("MATRIX_RUST_INSECURE_SSL");
  }
#else
  /* In release builds, always clear the env var and ignore the account option. */
  (void)purple_account_get_bool(account, "insecure_ssl", FALSE); /* suppress unused warning */
  g_unsetenv("MATRIX_RUST_INSECURE_SSL");
#endif

  purple_connection_set_state(gc, PURPLE_CONNECTING);

  char *safe_username = g_strdup(username);
  for (char *p = safe_username; *p; p++)
    if (*p == ':' || *p == '/' || *p == '\\')
      *p = '_';
  char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data",
                                    safe_username, NULL);
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS))
    g_mkdir_with_parents(data_dir, 0700);

  /* ALWAYS returns 2 (Pending) now, connected state handled by connected_cb */
  purple_matrix_rust_login(username, password, homeserver, data_dir);

  g_free(safe_username);
  g_free(data_dir);
  if (homeserver_derived)
    g_free(homeserver_derived);
}

void matrix_close(PurpleConnection *gc) {
  PurpleAccount *account = purple_connection_get_account(gc);
  purple_matrix_rust_logout(purple_account_get_username(account));
}

void manual_sso_token_action_cb(void *user_data, PurpleRequestFields *fields) {
  const char *token = purple_request_fields_get_string(fields, "sso_token");
  if (token && *token && strlen(token) < 2048) {
    purple_matrix_rust_finish_sso(token);
  } else {
    purple_debug_error("matrix", "SSO token is empty or too large.\n");
    PurpleAccount *account = find_matrix_account();
    if (account && purple_account_get_connection(account)) {
      purple_notify_error(purple_account_get_connection(account),
                          "SSO Login Failed", "Invalid Login Token",
                          "The token was empty or too long. Please retry "
                          "the login and paste only the 'loginToken' value "
                          "from the browser redirect URL.");
    }
  }
}

static char *last_sso_url = NULL;

static gboolean process_sso_cb(gpointer data) {
  char *url = (char *)data;
  PurpleAccount *account = find_matrix_account();

  if (last_sso_url && strcmp(last_sso_url, url) == 0) {
    purple_debug_info("matrix", "Ignoring duplicate SSO URL: %s\n", url);
    g_free(url);
    return FALSE;
  }

  g_free(last_sso_url);
  last_sso_url = g_strdup(url);

  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    purple_notify_uri(gc ? (void *)gc : (void *)my_plugin, url);

    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);
    PurpleRequestField *field = purple_request_field_string_new(
        "sso_token", "Login Token", NULL, FALSE);
    matrix_request_field_set_help_string(field,
        "After logging in with your browser, copy the full redirect URL "
        "(or just the 'loginToken=...' value) and paste it here.");
    purple_request_field_group_add_field(group, field);

    char *msg = g_strdup_printf(
        "A browser window should have opened to Matrix SSO. If not, please "
        "visit the URL below and paste the 'loginToken' here.\n\n%s",
        url);
    purple_request_fields(gc, "SSO Login", "Authentication Required", msg,
                          fields, "Submit",
                          G_CALLBACK(manual_sso_token_action_cb), "Cancel",
                          NULL, account, NULL, NULL, account);
    g_free(msg);
  } else {
    purple_debug_warning(
        "matrix", "process_sso_cb: No Matrix account found to handle SSO!\n");
  }
  g_free(url);
  return FALSE;
}

void sso_url_cb(const char *url) { g_idle_add(process_sso_cb, g_strdup(url)); }

static gboolean process_connected_cb(gpointer data) {
  GList *connections = purple_connections_get_all();
  for (GList *l = connections; l != NULL; l = l->next) {
    PurpleConnection *gc = (PurpleConnection *)l->data;
    PurpleAccount *account = purple_connection_get_account(gc);
    if (account && strcmp(purple_account_get_protocol_id(account),
                          "prpl-matrix-rust") == 0) {
      cleanup_stale_thread_labels(account);
      purple_connection_set_state(gc, PURPLE_CONNECTED);
    }
  }
  return FALSE;
}

void connected_cb(const char *user_id) {
  g_idle_add(process_connected_cb, NULL);
}

static gboolean process_login_failed_cb(gpointer data) {
  char *reason = (char *)data;
  PurpleAccount *account = find_matrix_account();
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      char *esc_reason = g_markup_escape_text(reason, -1);
      purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
                                     esc_reason);
      g_free(esc_reason);
    }
  }
  g_free(reason);
  return FALSE;
}

void login_failed_cb(const char *reason) {
  g_idle_add(process_login_failed_cb, g_strdup(reason));
}

GList *matrix_status_types(PurpleAccount *account) {
  GList *types = NULL;
  types = g_list_append(types, purple_status_type_new_with_attrs(
                                   PURPLE_STATUS_AVAILABLE, "available", NULL,
                                   TRUE, TRUE, FALSE, "message", "Message",
                                   purple_value_new(PURPLE_TYPE_STRING), NULL));
  types = g_list_append(types, purple_status_type_new_with_attrs(
                                   PURPLE_STATUS_AWAY, "away", NULL, TRUE, TRUE,
                                   FALSE, "message", "Message",
                                   purple_value_new(PURPLE_TYPE_STRING), NULL));
  types = g_list_append(types, purple_status_type_new(PURPLE_STATUS_OFFLINE,
                                                      "offline", NULL, TRUE));
  return types;
}

char *matrix_status_text(PurpleBuddy *buddy) {
  if (PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)) {
    PurpleChat *chat = (PurpleChat *)buddy;
    PurpleAccount *account = purple_chat_get_account(chat);
    const char *room_id = g_hash_table_lookup(purple_chat_get_components(chat), "room_id");
    if (account && room_id) {
        PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
        if (conv) {
            const char *activity = purple_conversation_get_data(conv, "matrix_last_activity");
            if (activity) return g_strdup(activity);
        }
    }
    GHashTable *components = purple_chat_get_components(chat);
    const char *topic = g_hash_table_lookup(components, "topic");
    return topic ? purple_markup_strip_html(topic) : NULL;
  }
  PurplePresence *presence = purple_buddy_get_presence(buddy);
  if (!presence)
    return NULL;
  PurpleStatus *status = purple_presence_get_active_status(presence);
  if (!status)
    return NULL;
  const char *msg = purple_status_get_attr_string(status, "message");
  return msg ? g_strdup(msg) : NULL;
}

void matrix_get_info(PurpleConnection *gc, const char *who) {
  purple_matrix_rust_get_user_info(
      purple_account_get_username(purple_connection_get_account(gc)), who);
}

void matrix_set_status(PurpleAccount *account, PurpleStatus *status) {
  const char *id = purple_status_get_id(status);
  const char *message = purple_status_get_attr_string(status, "message");
  int mat_status = 1;
  if (!strcmp(id, "available"))
    mat_status = 1;
  else if (!strcmp(id, "away"))
    mat_status = 2;
  else if (!strcmp(id, "offline"))
    mat_status = 0;
  purple_matrix_rust_set_status(purple_account_get_username(account),
                                mat_status, message);
}

void matrix_set_idle(PurpleConnection *gc, int time) {
  purple_matrix_rust_set_idle(
      purple_account_get_username(purple_connection_get_account(gc)), time);
}
void matrix_change_passwd(PurpleConnection *gc, const char *old,
                          const char *new) {
  purple_matrix_rust_change_password(
      purple_account_get_username(purple_connection_get_account(gc)), old, new);
}
void matrix_add_deny(PurpleConnection *gc, const char *who) {
  purple_matrix_rust_ignore_user(
      purple_account_get_username(purple_connection_get_account(gc)), who);
}
void matrix_rem_deny(PurpleConnection *gc, const char *who) {
  purple_matrix_rust_unignore_user(
      purple_account_get_username(purple_connection_get_account(gc)), who);
}
void matrix_unregister_user(PurpleAccount *account,
                            PurpleAccountUnregistrationCb cb, void *user_data) {
  purple_matrix_rust_destroy_session(purple_account_get_username(account));
  if (cb)
    cb(account, TRUE, user_data);
}
void matrix_set_public_alias(PurpleConnection *gc, const char *alias) {
  purple_matrix_rust_set_display_name(
      purple_account_get_username(purple_connection_get_account(gc)), alias);
}
void matrix_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img) {
  if (img)
    purple_matrix_rust_set_avatar(
        purple_account_get_username(purple_connection_get_account(gc)),
        (const unsigned char *)purple_imgstore_get_data(img),
        purple_imgstore_get_size(img));
}

static gboolean process_user_info_cb(gpointer data) {
  MatrixUserInfoData *d = (MatrixUserInfoData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleNotifyUserInfo *info = purple_notify_user_info_new();
    char *esc_id = g_markup_escape_text(d->target_user_id, -1);
    purple_notify_user_info_add_pair(info, "Matrix ID", esc_id);
    g_free(esc_id);

    if (d->display_name) {
      char *esc_dn = g_markup_escape_text(d->display_name, -1);
      purple_notify_user_info_add_pair(info, "Display Name", esc_dn);
      g_free(esc_dn);
    }
    if (d->avatar_url) {
      char *esc_av = g_markup_escape_text(d->avatar_url, -1);
      purple_notify_user_info_add_pair(info, "Avatar URL", esc_av);
      g_free(esc_av);
    }
    purple_notify_userinfo(purple_account_get_connection(account),
                           d->target_user_id, info, NULL, NULL);
    purple_notify_user_info_destroy(info);
  }
  g_free(d->user_id);
  g_free(d->target_user_id);
  g_free(d->display_name);
  g_free(d->avatar_url);
  g_free(d);
  return FALSE;
}

void show_user_info_cb(const char *user_id, const char *target_user_id,
                       const char *display_name, const char *avatar_url) {
  MatrixUserInfoData *d = g_new0(MatrixUserInfoData, 1);
  d->user_id = g_strdup(user_id);
  d->target_user_id = g_strdup(target_user_id);
  d->display_name = g_strdup(display_name);
  d->avatar_url = g_strdup(avatar_url);
  g_idle_add(process_user_info_cb, d);
}

static gboolean process_room_preview_cb(gpointer data) {
  MatrixRoomPreviewData *d = (MatrixRoomPreviewData *)data;
  // Account context is missing here, but usually dashboard is open
  g_free(d->user_id);
  g_free(d->room_id_or_alias);
  g_free(d->html_body);
  g_free(d);
  return FALSE;
}

void room_preview_cb(const char *user_id, const char *room_id_or_alias,
                     const char *html_body) {
  MatrixRoomPreviewData *d = g_new0(MatrixRoomPreviewData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id_or_alias = g_strdup(room_id_or_alias);
  d->html_body = g_strdup(html_body);
  g_idle_add(process_room_preview_cb, d);
}

static gboolean process_chat_topic_cb(gpointer data) {
  MatrixTopicData *d = (MatrixTopicData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_ANY, d->room_id, account);
    if (conv) {
      purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), d->sender, d->topic);
      g_free(purple_conversation_get_data(conv, "matrix_topic"));
      purple_conversation_set_data(conv, "matrix_topic", g_strdup(d->topic));
    }
    PurpleChat *chat = purple_blist_find_chat(account, d->room_id);
    if (chat) {
      purple_blist_node_set_string((PurpleBlistNode *)chat, "topic", d->topic);
      purple_blist_update_node_icon((PurpleBlistNode *)chat);
    }
    purple_signal_emit(my_plugin, "matrix-ui-room-topic", d->room_id, d->topic);
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->topic);
  g_free(d->sender);
  g_free(d);
  return FALSE;
}

void chat_topic_callback(const char *user_id, const char *room_id,
                         const char *topic, const char *sender) {
  MatrixTopicData *d = g_new0(MatrixTopicData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->topic = g_strdup(topic);
  d->sender = g_strdup(sender);
  g_idle_add(process_chat_topic_cb, d);
}

static gboolean process_chat_user_cb(gpointer data) {
  MatrixChatUserData *d = (MatrixChatUserData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConversation *conv = purple_find_conversation_with_account(
        PURPLE_CONV_TYPE_ANY, d->room_id, account);
    if (conv) {
      if (d->add) {
        if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), d->member_id)) {
          purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), d->member_id,
                                    d->alias, PURPLE_CBFLAGS_NONE, FALSE);
        }
        if (d->alias) {
          serv_got_alias(purple_account_get_connection(account), d->member_id,
                         d->alias);
        }
      } else {
        purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv), d->member_id, NULL);
      }
    }
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->member_id);
  if (d->alias)
    g_free(d->alias);
  if (d->avatar_path)
    g_free(d->avatar_path);
  g_free(d);
  return FALSE;
}

void chat_user_callback(const char *user_id, const char *room_id,
                        const char *member_id, bool add, const char *alias,
                        const char *avatar_path) {
  MatrixChatUserData *d = g_new0(MatrixChatUserData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->member_id = g_strdup(member_id);
  d->add = add;
  d->alias = g_strdup(alias);
  d->avatar_path = g_strdup(avatar_path);
  g_idle_add(process_chat_user_cb, d);
}

static gboolean process_presence_cb(gpointer data) {
  MatrixPresenceData *d = (MatrixPresenceData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    const char *status_id = "available";
    if (d->status == 2) status_id = "away";
    else if (d->status == 0) status_id = "offline";
    
    purple_prpl_got_user_status(account, d->target_user_id, status_id, "message", d->status_msg, NULL);
  }
  g_free(d->user_id);
  g_free(d->target_user_id);
  g_free(d->status_msg);
  g_free(d);
  return FALSE;
}

void presence_callback(const char *user_id, const char *target_user_id,
                       int status, const char *status_msg) {
  MatrixPresenceData *d = g_new0(MatrixPresenceData, 1);
  d->user_id = g_strdup(user_id);
  d->target_user_id = g_strdup(target_user_id);
  d->status = status;
  d->status_msg = g_strdup(status_msg);
  g_idle_add(process_presence_cb, d);
}

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
  purple_matrix_rust_confirm_sas(
      purple_account_get_username(find_matrix_account()), data->user_id,
      data->flow_id, (action == 0));
  g_free(data->user_id);
  g_free(data->flow_id);
  g_free(data);
}

static gboolean process_sas_request_cb(gpointer data) {
  struct VerificationData *vd = (struct VerificationData *)data;
  PurpleAccount *account = find_matrix_account();
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, vd->user_id, account);
    if (conv) {
        purple_conversation_set_data(conv, "matrix-verification-flow-id", g_strdup(vd->flow_id));
    }

    purple_request_action(NULL, "Verification", "Accept?", vd->user_id, 0, NULL,
                          NULL, NULL, vd, 2, "Accept",
                          G_CALLBACK(accept_verification_cb), "Cancel",
                          G_CALLBACK(cancel_verification_cb));
  } else {
    g_free(vd->user_id);
    g_free(vd->flow_id);
    g_free(vd);
  }
  return FALSE;
}

void sas_request_cb(const char *user_id, const char *target_user_id,
                    const char *flow_id) {
  struct VerificationData *d = g_new0(struct VerificationData, 1);
  d->user_id = g_strdup(target_user_id);
  d->flow_id = g_strdup(flow_id);
  g_idle_add(process_sas_request_cb, d);
}

static gboolean process_sas_emoji_cb(gpointer data) {
  MatrixEmojiData *ed = (MatrixEmojiData *)data;
  struct VerificationData *vd = g_new0(struct VerificationData, 1);
  vd->user_id = g_strdup(ed->user_id);
  vd->flow_id = g_strdup(ed->flow_id);
  PurpleAccount *account = find_matrix_account();
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, ed->user_id, account);
    if (conv) {
        purple_conversation_set_data(conv, "matrix-verification-flow-id", g_strdup(ed->flow_id));
    }

    char *esc_emojis = g_markup_escape_text(ed->emojis, -1);
    char *msg = g_strdup_printf("Match?\n\n%s", esc_emojis);
    purple_request_action(NULL, "Emojis", msg, ed->user_id, 0, NULL, NULL, NULL,
                          vd, 2, "Match", G_CALLBACK(sas_match_cb), "Mismatch",
                          G_CALLBACK(sas_match_cb));
    g_free(msg);
    g_free(esc_emojis);
  } else {
    g_free(vd->user_id);
    g_free(vd->flow_id);
    g_free(vd);
  }
  g_free(ed->user_id);
  g_free(ed->flow_id);
  g_free(ed->emojis);
  g_free(ed);
  return FALSE;
}

void sas_emoji_cb(const char *user_id, const char *target_user_id,
                  const char *flow_id, const char *emojis) {
  MatrixEmojiData *d = g_new0(MatrixEmojiData, 1);
  d->user_id = g_strdup(target_user_id);
  d->flow_id = g_strdup(flow_id);
  d->emojis = g_strdup(emojis);
  g_idle_add(process_sas_emoji_cb, d);
}

static gboolean process_qr_cb(gpointer data) {
  MatrixQrData *d = (MatrixQrData *)data;
  PurpleAccount *account = find_matrix_account();
  if (account && purple_account_get_connection(account) != NULL)
    purple_notify_formatted(purple_account_get_connection(account), "QR",
                            "Scan", NULL, d->html_data, NULL, NULL);
  g_free(d->user_id);
  g_free(d->html_data);
  g_free(d);
  return FALSE;
}

void show_verification_qr_cb(const char *user_id, const char *target_user_id,
                             const char *html_data) {
  MatrixQrData *d = g_new0(MatrixQrData, 1);
  d->user_id = g_strdup(target_user_id);
  d->html_data = g_strdup(html_data);
  g_idle_add(process_qr_cb, d);
}

void poll_creation_callback(const char *user_id, const char *room_id) {
    purple_matrix_rust_ui_action_poll(user_id, room_id);
}

void poll_list_callback(const char *user_id, const char *room_id, const char *event_id,
                        const char *question, const char *sender, const char *options_str) {
    // Implement UI to show polls
}

void thread_list_callback(const char *user_id, const char *room_id, const char *thread_root_id,
                          const char *latest_msg) {
    // Implement UI to show threads
}

void room_list_add_callback(const char *user_id, const char *room_id, const char *name,
                            const char *topic, const char *parent_id) {
    // Implement room list discovery UI
}

static gboolean process_room_dashboard_cb(gpointer data) {
  MatrixDashboardData *d = (MatrixDashboardData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
      GString *s = g_string_new("");
      g_string_append_printf(s, "<b>Room ID:</b> %s<br>", d->room_id);
      if (d->alias) g_string_append_printf(s, "<b>Canonical Alias:</b> %s<br>", d->alias);
      g_string_append_printf(s, "<b>Members:</b> %" G_GUINT64_FORMAT "<br>", d->member_count);
      g_string_append_printf(s, "<b>Encryption:</b> %s<br>", d->encrypted ? "Enabled 🔒" : "Disabled");
      g_string_append_printf(s, "<b>Your Power Level:</b> %" G_GINT64_FORMAT "<br>", d->power_level);
      if (d->topic && strlen(d->topic) > 0) {
          char *esc_topic = g_markup_escape_text(d->topic, -1);
          g_string_append_printf(s, "<br><b>Topic:</b><br>%s", esc_topic);
          g_free(esc_topic);
      }

      purple_notify_formatted(purple_account_get_connection(account), "Room Dashboard", d->name, NULL, s->str, NULL, NULL);
      g_string_free(s, TRUE);
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->name);
  g_free(d->topic);
  g_free(d->alias);
  g_free(d);
  return FALSE;
}

void room_dashboard_info_callback(const char *user_id, const char *room_id,
                                  const char *name, const char *topic,
                                  uint64_t member_count, bool encrypted,
                                  int64_t power_level, const char *alias) {
  MatrixDashboardData *d = g_new0(MatrixDashboardData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->name = g_strdup(name);
  d->topic = g_strdup(topic);
  d->member_count = member_count;
  d->encrypted = encrypted;
  d->power_level = power_level;
  d->alias = g_strdup(alias);
  g_idle_add(process_room_dashboard_cb, d);
}

