#include "matrix_account.h"
#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_types.h"
#include "matrix_blist.h"

#include <libpurple/util.h>
#include <libpurple/debug.h>
#include <libpurple/request.h>
#include <libpurple/notify.h>
#include <libpurple/version.h>
#include <libpurple/imgstore.h>
#include <libpurple/prpl.h>
#include <libpurple/server.h>
#include <string.h>
#include <stdlib.h>

void matrix_login(PurpleAccount *account) {
  PurpleConnection *gc = purple_account_get_connection(account);
  const char *username = purple_account_get_username(account);
  const char *password = purple_account_get_password(account);
  char *homeserver_derived = NULL;
  const char *homeserver = purple_account_get_string(account, "server", NULL);
  if (!homeserver || strcmp(homeserver, "https://matrix.org") == 0 || strlen(homeserver) == 0) {
    const char *delimiter = strchr(username, ':');
    if (delimiter) {
      const char *domain = delimiter + 1;
      if (g_ascii_strcasecmp(domain, "matrix.org") != 0) homeserver_derived = g_strdup_printf("https://%s", domain);
    }
  }
  if (homeserver_derived) homeserver = homeserver_derived;
  if (!homeserver) homeserver = "https://matrix.org";
  purple_connection_set_state(gc, PURPLE_CONNECTING);
  char *safe_username = g_strdup(username);
  for (char *p = safe_username; *p; p++) if (*p == ':' || *p == '/' || *p == '\\') *p = '_';
  char *data_dir = g_build_filename(purple_user_dir(), "matrix_rust_data", safe_username, NULL);
  if (!g_file_test(data_dir, G_FILE_TEST_EXISTS)) g_mkdir_with_parents(data_dir, 0700);
  int success = purple_matrix_rust_login(username, password, homeserver, data_dir);
  g_free(safe_username); g_free(data_dir); if (homeserver_derived) g_free(homeserver_derived);
  if (success == 1) purple_connection_set_state(gc, PURPLE_CONNECTED);
  else if (success != 2) purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "Failed to login");
}

void matrix_close(PurpleConnection *gc) {
  PurpleAccount *account = purple_connection_get_account(gc);
  purple_matrix_rust_logout(purple_account_get_username(account));
}

static void manual_sso_token_action_cb(void *user_data, PurpleRequestFields *fields) {
  const char *token = purple_request_fields_get_string(fields, "sso_token");
  if (token && *token) purple_matrix_rust_finish_sso(token);
}

static gboolean process_sso_cb(gpointer data) {
  char *url = (char *)data; PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    purple_notify_uri(gc ? (void *)gc : (void *)my_plugin, url);
    PurpleRequestFields *fields = purple_request_fields_new();
    PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
    purple_request_fields_add_group(fields, group);
    PurpleRequestField *field = purple_request_field_string_new("sso_token", "Login Token", NULL, FALSE);
    matrix_request_field_set_help_string(field, "Paste loginToken=...");
    purple_request_field_group_add_field(group, field);
    purple_request_fields(gc, "SSO Login", "Auth Required", url, fields, "Submit", G_CALLBACK(manual_sso_token_action_cb), "Cancel", NULL, account, NULL, NULL, account);
  }
  g_free(url); return FALSE;
}

void sso_url_cb(const char *url) { g_idle_add(process_sso_cb, g_strdup(url)); }

static gboolean process_connected_cb(gpointer data) {
  GList *connections = purple_connections_get_all();
  for (GList *l = connections; l != NULL; l = l->next) {
    PurpleConnection *gc = (PurpleConnection *)l->data;
    PurpleAccount *account = purple_connection_get_account(gc);
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
      cleanup_stale_thread_labels(account);
      if (purple_connection_get_state(gc) == PURPLE_CONNECTING || purple_connection_get_state(gc) == PURPLE_DISCONNECTED)
        purple_connection_set_state(gc, PURPLE_CONNECTED);
    }
  }
  return FALSE;
}

void connected_cb(void) { g_idle_add(process_connected_cb, NULL); }

static gboolean process_login_failed_cb(gpointer data) {
  char *reason = (char *)data; PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) purple_connection_error_reason(gc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR, reason);
  }
  g_free(reason); return FALSE;
}

void login_failed_cb(const char *reason) { g_idle_add(process_login_failed_cb, g_strdup(reason)); }

GList *matrix_status_types(PurpleAccount *account) {
  GList *types = NULL;
  types = g_list_append(types, purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE, "available", NULL, TRUE, TRUE, FALSE, "message", "Message", purple_value_new(PURPLE_TYPE_STRING), NULL));
  types = g_list_append(types, purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY, "away", NULL, TRUE, TRUE, FALSE, "message", "Message", purple_value_new(PURPLE_TYPE_STRING), NULL));
  types = g_list_append(types, purple_status_type_new(PURPLE_STATUS_OFFLINE, "offline", NULL, TRUE));
  return types;
}

char *matrix_status_text(PurpleBuddy *buddy) {
  PurplePresence *presence = purple_buddy_get_presence(buddy);
  if (!presence) return NULL;
  PurpleStatus *status = purple_presence_get_active_status(presence);
  if (!status) return NULL;
  const char *msg = purple_status_get_attr_string(status, "message");
  return msg ? g_strdup(msg) : NULL;
}

void matrix_get_info(PurpleConnection *gc, const char *who) { purple_matrix_rust_get_user_info(purple_account_get_username(purple_connection_get_account(gc)), who); }

void matrix_set_status(PurpleAccount *account, PurpleStatus *status) {
  const char *id = purple_status_get_id(status); const char *message = purple_status_get_attr_string(status, "message");
  int mat_status = 1;
  if (!strcmp(id, "available")) mat_status = 1;
  else if (!strcmp(id, "away")) mat_status = 2;
  else if (!strcmp(id, "offline")) mat_status = 0;
  purple_matrix_rust_set_status(purple_account_get_username(account), mat_status, message);
}

void matrix_set_idle(PurpleConnection *gc, int time) { purple_matrix_rust_set_idle(purple_account_get_username(purple_connection_get_account(gc)), time); }
void matrix_change_passwd(PurpleConnection *gc, const char *old, const char *new) { purple_matrix_rust_change_password(purple_account_get_username(purple_connection_get_account(gc)), old, new); }
void matrix_add_deny(PurpleConnection *gc, const char *who) { purple_matrix_rust_ignore_user(purple_account_get_username(purple_connection_get_account(gc)), who); }
void matrix_rem_deny(PurpleConnection *gc, const char *who) { purple_matrix_rust_unignore_user(purple_account_get_username(purple_connection_get_account(gc)), who); }
void matrix_unregister_user(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data) { purple_matrix_rust_deactivate_account(TRUE); if (cb) cb(account, TRUE, user_data); }
void matrix_set_public_alias(PurpleConnection *gc, const char *alias) { purple_matrix_rust_set_display_name(purple_account_get_username(purple_connection_get_account(gc)), alias); }
void matrix_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img) { if (img) purple_matrix_rust_set_avatar_bytes(purple_account_get_username(purple_connection_get_account(gc)), (const unsigned char *)purple_imgstore_get_data(img), purple_imgstore_get_size(img)); }

static gboolean process_user_info_cb(gpointer data) {
  MatrixUserInfoData *d = (MatrixUserInfoData *)data; PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleNotifyUserInfo *info = purple_notify_user_info_new();
    purple_notify_user_info_add_pair(info, "Matrix ID", d->user_id);
    if (d->display_name) purple_notify_user_info_add_pair(info, "Display Name", d->display_name);
    if (d->avatar_url) purple_notify_user_info_add_pair(info, "Avatar URL", d->avatar_url);
    purple_notify_userinfo(purple_account_get_connection(account), d->user_id, info, NULL, NULL);
    purple_notify_user_info_destroy(info);
  }
  g_free(d->user_id); g_free(d->display_name); g_free(d->avatar_url); g_free(d); return FALSE;
}

void show_user_info_cb(const char *user_id, const char *display_name, const char *avatar_url, bool is_online) {
  MatrixUserInfoData *d = g_new0(MatrixUserInfoData, 1); d->user_id = g_strdup(user_id); d->display_name = g_strdup(display_name); d->avatar_url = g_strdup(avatar_url); d->is_online = is_online;
  g_idle_add(process_user_info_cb, d);
}

static gboolean process_room_preview_cb(gpointer data) {
  MatrixRoomPreviewData *d = (MatrixRoomPreviewData *)data;
  purple_debug_info("purple-matrix-rust", "Room preview: %s\n", d->room_id_or_alias);
  g_free(d->room_id_or_alias); g_free(d->html_body); g_free(d); return FALSE;
}

void room_preview_cb(const char *room_id_or_alias, const char *html_body) {
  MatrixRoomPreviewData *d = g_new0(MatrixRoomPreviewData, 1); d->room_id_or_alias = g_strdup(room_id_or_alias); d->html_body = g_strdup(html_body);
  g_idle_add(process_room_preview_cb, d);
}

static gboolean process_chat_topic_cb(gpointer data) {
  MatrixTopicData *d = (MatrixTopicData *)data; PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConversation *conv = purple_find_chat(purple_account_get_connection(account), get_chat_id(d->room_id));
    if (conv) purple_conv_chat_set_topic(PURPLE_CONV_CHAT(conv), d->sender, d->topic);
  }
  g_free(d->room_id); g_free(d->topic); g_free(d->sender); g_free(d); return FALSE;
}

void chat_topic_callback(const char *room_id, const char *topic, const char *sender) {
  MatrixTopicData *d = g_new0(MatrixTopicData, 1); d->room_id = g_strdup(room_id); d->topic = g_strdup(topic); d->sender = g_strdup(sender);
  g_idle_add(process_chat_topic_cb, d);
}

static gboolean process_chat_user_cb(gpointer data) {
  MatrixChatUserData *d = (MatrixChatUserData *)data; PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConversation *conv = purple_find_chat(purple_account_get_connection(account), get_chat_id(d->room_id));
    if (conv) {
      if (d->add) {
        if (!purple_conv_chat_find_user(PURPLE_CONV_CHAT(conv), d->user_id))
          purple_conv_chat_add_user(PURPLE_CONV_CHAT(conv), d->user_id, d->alias, PURPLE_CBFLAGS_NONE, FALSE);
      } else purple_conv_chat_remove_user(PURPLE_CONV_CHAT(conv), d->user_id, NULL);
    }
  }
  g_free(d->room_id); g_free(d->user_id); if (d->alias) g_free(d->alias); if (d->avatar_path) g_free(d->avatar_path); g_free(d); return FALSE;
}

void chat_user_callback(const char *room_id, const char *user_id, bool add, const char *alias, const char *avatar_path) {
  MatrixChatUserData *d = g_new0(MatrixChatUserData, 1); d->room_id = g_strdup(room_id); d->user_id = g_strdup(user_id); d->add = add; d->alias = g_strdup(alias); d->avatar_path = g_strdup(avatar_path);
  g_idle_add(process_chat_user_cb, d);
}

static gboolean process_presence_cb(gpointer data) {
  MatrixPresenceData *d = (MatrixPresenceData *)data; PurpleAccount *account = find_matrix_account();
  if (account) purple_prpl_got_user_status(account, d->user_id, d->is_online ? "available" : "offline", NULL);
  g_free(d->user_id); g_free(d); return FALSE;
}

void presence_callback(const char *user_id, bool is_online) {
  MatrixPresenceData *d = g_new0(MatrixPresenceData, 1); d->user_id = g_strdup(user_id); d->is_online = is_online;
  g_idle_add(process_presence_cb, d);
}

static void accept_verification_cb(struct VerificationData *data, int action) { purple_matrix_rust_accept_sas(purple_account_get_username(find_matrix_account()), data->user_id, data->flow_id); g_free(data->user_id); g_free(data->flow_id); g_free(data); }
static void cancel_verification_cb(struct VerificationData *data, int action) { g_free(data->user_id); g_free(data->flow_id); g_free(data); }
static void sas_match_cb(struct VerificationData *data, int action) { purple_matrix_rust_confirm_sas(purple_account_get_username(find_matrix_account()), data->user_id, data->flow_id, (action == 0)); g_free(data->user_id); g_free(data->flow_id); g_free(data); }

static gboolean process_sas_request_cb(gpointer data) {
  struct VerificationData *vd = (struct VerificationData *)data;
  purple_request_action(NULL, "Verification", "Accept?", vd->user_id, 0, NULL, NULL, NULL, vd, 2, "Accept", G_CALLBACK(accept_verification_cb), "Cancel", G_CALLBACK(cancel_verification_cb));
  return FALSE;
}

void sas_request_cb(const char *user_id, const char *flow_id) {
  struct VerificationData *d = g_new0(struct VerificationData, 1); d->user_id = g_strdup(user_id); d->flow_id = g_strdup(flow_id);
  g_idle_add(process_sas_request_cb, d);
}

static gboolean process_sas_emoji_cb(gpointer data) {
  MatrixEmojiData *ed = (MatrixEmojiData *)data; struct VerificationData *vd = g_new0(struct VerificationData, 1); vd->user_id = g_strdup(ed->user_id); vd->flow_id = g_strdup(ed->flow_id);
  char *msg = g_strdup_printf("Match?\n\n%s", ed->emojis);
  purple_request_action(NULL, "Emojis", msg, ed->user_id, 0, NULL, NULL, NULL, vd, 2, "Match", G_CALLBACK(sas_match_cb), "Mismatch", G_CALLBACK(sas_match_cb));
  g_free(msg); g_free(ed->user_id); g_free(ed->flow_id); g_free(ed->emojis); g_free(ed); return FALSE;
}

void sas_emoji_cb(const char *user_id, const char *flow_id, const char *emojis) {
  MatrixEmojiData *d = g_new0(MatrixEmojiData, 1); d->user_id = g_strdup(user_id); d->flow_id = g_strdup(flow_id); d->emojis = g_strdup(emojis);
  g_idle_add(process_sas_emoji_cb, d);
}

static gboolean process_qr_cb(gpointer data) {
  MatrixQrData *d = (MatrixQrData *)data; PurpleAccount *account = find_matrix_account();
  if (account) purple_notify_formatted(purple_account_get_connection(account), "QR", "Scan", NULL, d->html_data, NULL, NULL);
  g_free(d->user_id); g_free(d->html_data); g_free(d); return FALSE;
}

void show_verification_qr_cb(const char *user_id, const char *html_data) {
  MatrixQrData *d = g_new0(MatrixQrData, 1); d->user_id = g_strdup(user_id); d->html_data = g_strdup(html_data);
  g_idle_add(process_qr_cb, d);
}

static void action_reconnect_cb(PurplePluginAction *action) { PurpleAccount *account = purple_connection_get_account((PurpleConnection *)action->context); if (account) matrix_login(account); }
GList *matrix_actions(PurplePlugin *plugin, gpointer context) {
  GList *l = NULL; l = g_list_append(l, purple_plugin_action_new("Reconnect", action_reconnect_cb));
  return l;
}

void matrix_init_sso_callbacks(void) {
  purple_matrix_rust_init_sso_cb(sso_url_cb);
  purple_matrix_rust_init_connected_cb(connected_cb);
  purple_matrix_rust_init_login_failed_cb(login_failed_cb);
  purple_matrix_rust_init_verification_cbs(sas_request_cb, sas_emoji_cb, show_verification_qr_cb);
}
