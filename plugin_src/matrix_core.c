#define PURPLE_PLUGINS
#include "matrix_account.h"
#include "matrix_blist.h"
#include "matrix_chat.h"
#include "matrix_commands.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/plugin.h>
#include <libpurple/pluginpref.h>
#include <libpurple/prefs.h>
#include <libpurple/prpl.h>
#include <libpurple/signals.h>
#include <libpurple/version.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

PurplePlugin *my_plugin = NULL;
GHashTable *room_id_map = NULL;

#define MATRIX_PREF_ROOT "/plugins/prpl/matrix_rust"
#define MATRIX_PREF_HOMESERVER "/plugins/prpl/matrix_rust/homeserver"
#define MATRIX_PREF_USERNAME "/plugins/prpl/matrix_rust/username"
#define MATRIX_PREF_SESSION_ACTION "/plugins/prpl/matrix_rust/session_action"

static PurplePluginPrefFrame *
matrix_get_plugin_pref_frame(PurplePlugin *plugin);
static void matrix_pref_changed_cb(const char *name, PurplePrefType type,
                                    gconstpointer val, gpointer data);
static gboolean plugin_unload(PurplePlugin *plugin);

static PurplePluginUiInfo prefs_info = {    .get_plugin_pref_frame = matrix_get_plugin_pref_frame,
};

static PurplePluginPrefFrame *
matrix_get_plugin_pref_frame(PurplePlugin *plugin) {
  PurplePluginPrefFrame *frame = purple_plugin_pref_frame_new();
  PurplePluginPref *pref = NULL;

  pref = purple_plugin_pref_new_with_label(
      "Matrix Preferences (global defaults). Account-specific settings still "
      "apply.");
  purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_INFO);
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_name_and_label(MATRIX_PREF_HOMESERVER,
                                                    "Homeserver URL (default)");
  purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_STRING_FORMAT);
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_name_and_label(MATRIX_PREF_USERNAME,
                                                    "Username (default)");
  purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_STRING_FORMAT);
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_name_and_label(MATRIX_PREF_SESSION_ACTION,
                                                    "Session Cache Action");
  purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_CHOICE);
  purple_plugin_pref_add_choice(pref, "No action", "none");
  purple_plugin_pref_add_choice(pref, "Clear session cache now", "clear");
  purple_plugin_pref_frame_add(frame, pref);

  pref = purple_plugin_pref_new_with_label(
      "Tip: use /matrix_clear_session or 'Clear Session Cache...' in account "
      "actions for immediate reset.");
  purple_plugin_pref_set_type(pref, PURPLE_PLUGIN_PREF_INFO);
  purple_plugin_pref_frame_add(frame, pref);

  return frame;
}

static void matrix_pref_changed_cb(const char *name, PurplePrefType type,
                                   gconstpointer val, gpointer data) {
  const char *choice = purple_prefs_get_string(MATRIX_PREF_SESSION_ACTION);
  if (choice && strcmp(choice, "clear") == 0) {
    PurpleAccount *account = find_matrix_account();
    if (account) {
      purple_matrix_rust_destroy_session(purple_account_get_username(account));
      purple_notify_info(
          my_plugin, "Matrix Session Cache", "Session cache cleared",
          "Local Matrix session removed. Reconnect to authenticate again.");
    }
    purple_prefs_set_string(MATRIX_PREF_SESSION_ACTION, "none");
  }
}

static void conversation_displayed_cb(PurpleConversation *conv) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (account && strcmp(purple_account_get_protocol_id(account),
                        "prpl-matrix-rust") == 0) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id) {
      purple_matrix_rust_send_read_receipt(purple_account_get_username(account),
                                           purple_conversation_get_name(conv),
                                           last_id);
    }
  }
}

static void conversation_created_cb(PurpleConversation *conv) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (account && strcmp(purple_account_get_protocol_id(account),
                        "prpl-matrix-rust") == 0) {
    if (purple_account_get_bool(account, "auto_fetch_history_on_open", TRUE)) {
      purple_matrix_rust_fetch_history(purple_account_get_username(account),
                                       purple_conversation_get_name(conv));
    }
  }
}

static int imgstore_add_cb(const void *data, size_t size) {
  if (!data || size == 0)
    return 0;
  return purple_imgstore_add_with_id(g_memdup2(data, size), size, NULL);
}

static void connect_signals(void) {
  void *conv_handle = purple_conversations_get_handle();
  if (conv_handle) {
    purple_signal_connect(conv_handle, "conversation-created", my_plugin,
                          PURPLE_CALLBACK(conversation_created_cb), NULL);
    purple_signal_connect(conv_handle, "conversation-displayed", my_plugin,
                          PURPLE_CALLBACK(conversation_displayed_cb), NULL);
    purple_signal_connect(conv_handle, "conversation-extended-menu", my_plugin,
                          PURPLE_CALLBACK(conversation_extended_menu_cb), NULL);
  }
}

static void core_initialized_cb(void) { connect_signals(); }

static void handle_show_dashboard_signal(const char *room_id,
                                         gpointer user_data) {
  if (!room_id)
    return;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    open_room_dashboard(account, room_id);
    purple_matrix_rust_ui_show_dashboard(room_id);
  }
}

static void handle_reply_signal(const char *room_id, gpointer user_data) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv)
    menu_action_reply_conv_cb(conv, NULL);
}

static void handle_start_thread_signal(const char *room_id,
                                       gpointer user_data) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv)
    menu_action_thread_start_cb(conv, NULL);
}

static void handle_sticker_signal(const char *room_id, gpointer user_data) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv)
    menu_action_sticker_conv_cb(conv, NULL);
}

static void handle_poll_signal(const char *room_id, gpointer user_data) {
  PurpleAccount *account = find_matrix_account();
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv)
    menu_action_poll_conv_cb(conv, NULL);
}

static void handle_clear_session_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_action_clear_session_cache_for_user(room_id);
}

static void handle_login_password_signal(const char *user_id,
                                         gpointer user_data) {
  matrix_action_login_password_for_user(user_id);
}

static void handle_login_sso_signal(const char *user_id, gpointer user_data) {
  matrix_action_login_sso_for_user(user_id);
}

static void handle_set_account_defaults_signal(const char *user_id,
                                               gpointer user_data) {
  matrix_action_set_account_defaults_for_user(user_id);
}

static void handle_show_members_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_show_members(room_id);
}

static void handle_join_room_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_join_room_prompt(room_id);
}

static gboolean split_room_and_user(const char *payload, char **room_id,
                                    char **user_id) {
  const char *sep = NULL;
  if (!payload || !*payload || !room_id || !user_id)
    return FALSE;
  sep = strchr(payload, '\n');
  if (!sep)
    return FALSE;
  *room_id = g_strndup(payload, (gsize)(sep - payload));
  *user_id = g_strdup(sep + 1);
  if (!*room_id || !**room_id || !*user_id || !**user_id) {
    g_free(*room_id);
    g_free(*user_id);
    *room_id = NULL;
    *user_id = NULL;
    return FALSE;
  }
  return TRUE;
}

static void handle_moderate_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_moderate(room_id);
}

static void handle_kick_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_kick_prompt(room_id);
}

static void handle_ban_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_ban_prompt(room_id);
}

static void handle_unban_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_unban_prompt(room_id);
}

static void handle_moderate_user_signal(const char *payload,
                                        gpointer user_data) {
  char *room_id = NULL;
  char *user_id = NULL;
  if (!split_room_and_user(payload, &room_id, &user_id))
    return;
  matrix_ui_action_moderate_user(room_id, user_id);
  g_free(room_id);
  g_free(user_id);
}

static void handle_user_info_signal(const char *payload, gpointer user_data) {
  char *room_id = NULL;
  char *user_id = NULL;
  if (split_room_and_user(payload, &room_id, &user_id)) {
    matrix_ui_action_user_info(room_id, user_id);
    g_free(room_id);
    g_free(user_id);
    return;
  }
  matrix_ui_action_user_info(payload, NULL);
}

static void handle_ignore_user_signal(const char *payload, gpointer user_data) {
  char *room_id = NULL;
  char *user_id = NULL;
  if (split_room_and_user(payload, &room_id, &user_id)) {
    matrix_ui_action_ignore_user(room_id, user_id);
    g_free(room_id);
    g_free(user_id);
    return;
  }
  matrix_ui_action_ignore_user(payload, NULL);
}

static void handle_unignore_user_signal(const char *payload,
                                        gpointer user_data) {
  char *room_id = NULL;
  char *user_id = NULL;
  if (split_room_and_user(payload, &room_id, &user_id)) {
    matrix_ui_action_unignore_user(room_id, user_id);
    g_free(room_id);
    g_free(user_id);
    return;
  }
  matrix_ui_action_unignore_user(payload, NULL);
}

static void handle_leave_room_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_leave_room(room_id);
}

static void handle_verify_self_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_verify_self(room_id);
}

static void handle_crypto_status_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_crypto_status(room_id);
}

static void handle_list_devices_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_list_devices(room_id);
}

static void handle_room_settings_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_room_settings(room_id);
}

static void handle_invite_user_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_invite_user(room_id);
}

static void handle_send_file_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_send_file(room_id);
}

static void handle_mark_unread_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_mark_unread(room_id);
}

static void handle_mark_read_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_mark_read(room_id);
}

static void handle_mute_room_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_set_room_mute(room_id, true);
}

static void handle_unmute_room_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_set_room_mute(room_id, false);
}

static void handle_search_room_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_search_room(room_id);
}

static void handle_list_polls_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_list_polls(room_id);
}

static void handle_react_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_react(room_id);
}

static void handle_send_location_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_send_location(room_id);
}

static void handle_who_read_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_who_read(room_id);
}

static void handle_report_event_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_report_event(room_id);
}

static void handle_message_inspector_signal(const char *room_id,
                                            gpointer user_data) {
  matrix_ui_action_message_inspector(room_id);
}

static void handle_show_last_event_details_signal(const char *room_id,
                                                  gpointer user_data) {
  matrix_ui_action_show_last_event_details(room_id);
}

static void handle_reply_pick_event_signal(const char *room_id,
                                           gpointer user_data) {
  matrix_ui_action_reply_pick_event(room_id);
}

static void handle_thread_pick_event_signal(const char *room_id,
                                            gpointer user_data) {
  matrix_ui_action_thread_pick_event(room_id);
}

static void handle_react_pick_event_signal(const char *room_id,
                                           gpointer user_data) {
  matrix_ui_action_react_pick_event(room_id);
}

static void handle_edit_pick_event_signal(const char *room_id,
                                          gpointer user_data) {
  matrix_ui_action_edit_pick_event(room_id);
}

static void handle_redact_pick_event_signal(const char *room_id,
                                            gpointer user_data) {
  matrix_ui_action_redact_pick_event(room_id);
}

static void handle_report_pick_event_signal(const char *room_id,
                                            gpointer user_data) {
  matrix_ui_action_report_pick_event(room_id);
}

static void handle_search_users_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_search_users(room_id);
}

static void handle_search_public_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_search_public(room_id);
}

static void handle_search_members_global_signal(const char *room_id,
                                                gpointer user_data) {
  matrix_ui_action_search_members_global(room_id);
}

static void handle_search_room_global_signal(const char *room_id,
                                             gpointer user_data) {
  matrix_ui_action_search_room_global(room_id);
}

static void handle_discover_public_rooms_signal(const char *room_id,
                                                gpointer user_data) {
  matrix_ui_action_discover_public_rooms(room_id);
}

static void handle_discover_room_preview_signal(const char *room_id,
                                                gpointer user_data) {
  matrix_ui_action_discover_room_preview(room_id);
}

static void handle_redact_event_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_redact_event_prompt(room_id);
}

static void handle_edit_event_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_edit_event_prompt(room_id);
}

static void handle_react_latest_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_react_latest(room_id);
}

static void handle_edit_latest_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_edit_latest(room_id);
}

static void handle_redact_latest_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_redact_latest(room_id);
}

static void handle_report_latest_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_report_latest(room_id);
}

static void handle_versions_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_versions(room_id);
}

static void handle_enable_key_backup_signal(const char *room_id,
                                            gpointer user_data) {
  matrix_ui_action_enable_key_backup(room_id);
}

static void handle_my_profile_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_my_profile(room_id);
}

static void handle_server_info_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_server_info(room_id);
}

static void handle_resync_recent_signal(const char *room_id,
                                        gpointer user_data) {
  matrix_ui_action_resync_recent_history(room_id);
}

static void handle_search_stickers_signal(const char *room_id,
                                          gpointer user_data) {
  matrix_ui_action_search_stickers(room_id);
}

static void handle_recover_keys_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_recover_keys_prompt(room_id);
}

static void handle_export_keys_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_export_keys_prompt(room_id);
}

static void handle_set_avatar_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_set_avatar_prompt(room_id);
}

static void handle_room_tag_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_room_tag_prompt(room_id);
}

static void handle_upgrade_room_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_upgrade_room_prompt(room_id);
}

static void handle_alias_create_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_alias_create_prompt(room_id);
}

static void handle_alias_delete_signal(const char *room_id,
                                       gpointer user_data) {
  matrix_ui_action_alias_delete_prompt(room_id);
}

static void handle_space_hierarchy_signal(const char *room_id,
                                          gpointer user_data) {
  matrix_ui_action_space_hierarchy_prompt(room_id);
}

static void handle_knock_signal(const char *room_id, gpointer user_data) {
  matrix_ui_action_knock_prompt(room_id);
}

static void handle_list_threads_signal(const char *room_id,
                                       gpointer user_data) {
  PurpleAccount *account = find_matrix_account();
  if (account && room_id) {
    purple_matrix_rust_list_threads(purple_account_get_username(account),
                                    room_id);
  }
}

/**
 * Custom marshallers for UI signals.
 */
static void handle_ui_action_marshal(PurpleCallback cb, va_list args,
                                     void *data, void **return_val) {
  void (*callback)(const char *, void *) = (void (*)(const char *, void *))cb;
  const char *arg1 = va_arg(args, const char *);
  callback(arg1, data);
}

static void handle_room_typing_marshal(PurpleCallback cb, va_list args,
                                       void *data, void **return_val) {
  void (*callback)(const char *, const char *, gboolean, void *) =
      (void (*)(const char *, const char *, gboolean, void *))cb;
  const char *arg1 = va_arg(args, const char *);
  const char *arg2 = va_arg(args, const char *);
  gboolean arg3 = va_arg(args, gboolean);
  callback(arg1, arg2, arg3, data);
}

static void handle_room_encrypted_marshal(PurpleCallback cb, va_list args,
                                          void *data, void **return_val) {
  void (*callback)(const char *, gboolean, void *) =
      (void (*)(const char *, gboolean, void *))cb;
  const char *arg1 = va_arg(args, const char *);
  gboolean arg2 = va_arg(args, gboolean);
  callback(arg1, arg2, data);
}

static void handle_room_muted_marshal(PurpleCallback cb, va_list args,
                                      void *data, void **return_val) {
  void (*callback)(const char *, gboolean, void *) =
      (void (*)(const char *, gboolean, void *))cb;
  const char *arg1 = va_arg(args, const char *);
  gboolean arg2 = va_arg(args, gboolean);
  callback(arg1, arg2, data);
}

static void handle_room_activity_marshal(PurpleCallback cb, va_list args,
                                         void *data, void **return_val) {
  void (*callback)(const char *, const char *, const char *, void *) =
      (void (*)(const char *, const char *, const char *, void *))cb;
  const char *arg1 = va_arg(args, const char *);
  const char *arg2 = va_arg(args, const char *);
  const char *arg3 = va_arg(args, const char *);
  callback(arg1, arg2, arg3, data);
}

static void handle_room_topic_marshal(PurpleCallback cb, va_list args,
                                      void *data, void **return_val) {
  void (*callback)(const char *, const char *, void *) =
      (void (*)(const char *, const char *, void *))cb;
  const char *arg1 = va_arg(args, const char *);
  const char *arg2 = va_arg(args, const char *);
  callback(arg1, arg2, data);
}

static gboolean poll_rust_channel_cb(gpointer user_data) {
  int ev_type = -1;
  void *data = NULL;

  int events_processed = 0;
  while (purple_matrix_rust_poll_event(&ev_type, &data) &&
         events_processed < 50) {
    if (!data)
      continue;

    purple_debug_info("matrix-ffi", "Received FFI event type %d\n", ev_type);
    switch (ev_type) {
    case FFI_EVENT_MESSAGE_RECEIVED: {
      CMessageReceived *s = (CMessageReceived *)data;
      msg_callback(s->user_id, s->sender, s->msg, s->room_id, s->thread_root_id,
                   s->event_id, s->timestamp, s->encrypted);
      break;
    }
    case FFI_EVENT_TYPING: {
      CTyping *s = (CTyping *)data;
      typing_callback(s->user_id, s->room_id, s->who, s->is_typing);
      break;
    }
    case FFI_EVENT_ROOM_JOINED: {
      CRoomJoined *s = (CRoomJoined *)data;
      room_joined_callback(s->user_id, s->room_id, s->name, s->group_name,
                           s->avatar_url, s->topic, s->encrypted,
                           s->member_count);
      break;
    }
    case FFI_EVENT_ROOM_LEFT: {
      CRoomLeft *s = (CRoomLeft *)data;
      room_left_callback(s->user_id, s->room_id);
      break;
    }
    case FFI_EVENT_READ_MARKER: {
      CReadMarker *s = (CReadMarker *)data;
      read_marker_cb(s->user_id, s->room_id, s->event_id, s->who);
      break;
    }
    case FFI_EVENT_PRESENCE: {
      CPresence *s = (CPresence *)data;
      presence_callback(s->user_id, s->target_user_id, s->is_online);
      break;
    }
    case FFI_EVENT_CHAT_TOPIC: {
      CChatTopic *s = (CChatTopic *)data;
      chat_topic_callback(s->user_id, s->room_id, s->topic, s->sender);
      break;
    }
    case FFI_EVENT_CHAT_USER: {
      CChatUser *s = (CChatUser *)data;
      chat_user_callback(s->user_id, s->room_id, s->member_id, s->add, s->alias,
                         s->avatar_path);
      break;
    }
    case FFI_EVENT_INVITE: {
      CInvite *s = (CInvite *)data;
      invite_callback(s->user_id, s->room_id, s->inviter);
      break;
    }
    case FFI_EVENT_ROOM_LIST_ADD: {
      CRoomListAdd *s = (CRoomListAdd *)data;
      roomlist_add_cb(s->user_id, s->name, s->room_id, s->topic,
                      s->member_count, s->is_space, s->parent_id);
      break;
    }
    case FFI_EVENT_ROOM_PREVIEW: {
      CRoomPreview *s = (CRoomPreview *)data;
      room_preview_cb(s->user_id, s->room_id_or_alias, s->html_body);
      break;
    }
    case FFI_EVENT_LOGIN_FAILED: {
      CLoginFailed *s = (CLoginFailed *)data;
      login_failed_cb(s->message);
      break;
    }
    case FFI_EVENT_SHOW_USER_INFO: {
      CShowUserInfo *s = (CShowUserInfo *)data;
      show_user_info_cb(s->user_id, s->display_name, s->avatar_url,
                        s->target_user_id, s->is_online);
      break;
    }
    case FFI_EVENT_THREAD_LIST: {
      CThreadList *s = (CThreadList *)data;
      thread_list_cb(s->user_id, s->room_id, s->thread_root_id, s->latest_msg,
                     s->count, s->ts);
      break;
    }
    case FFI_EVENT_POLL_LIST: {
      CPollList *s = (CPollList *)data;
      poll_list_cb(s->user_id, s->room_id, s->event_id, s->question, s->sender,
                   s->options_str);
      break;
    }
    case FFI_EVENT_SEARCH: {
      CSearch *s = (CSearch *)data;
      search_result_cb(s->user_id, s->room_id, s->sender, s->message,
                       s->timestamp_str);
      break;
    }
    case FFI_EVENT_REACTIONS_CHANGED: {
      CReactionsChanged *s = (CReactionsChanged *)data;
      reactions_changed_callback(s->user_id, s->room_id, s->event_id,
                                 s->reactions_text);
      break;
    }
    case FFI_EVENT_MESSAGE_EDITED: {
      CMessageEdited *s = (CMessageEdited *)data;
      message_edited_callback(s->user_id, s->room_id, s->event_id, s->new_msg);
      break;
    }
    case FFI_EVENT_POWER_LEVEL_UPDATE: {
      CPowerLevelUpdate *s = (CPowerLevelUpdate *)data;
      power_level_update_callback(s->user_id, s->room_id, s->is_admin,
                                  s->can_kick, s->can_ban, s->can_redact,
                                  s->can_invite);
      break;
    }
    case FFI_EVENT_UPDATE_BUDDY: {
      CUpdateBuddy *s = (CUpdateBuddy *)data;
      update_buddy_callback(s->user_id, s->alias, s->avatar_url);
      break;
    }
    case FFI_EVENT_STICKER_PACK: {
      CStickerPack *s = (CStickerPack *)data;
      void (*cb)(const char *, const char *, const char *, void *) =
          (void *)s->cb_ptr;
      if (cb)
        cb(s->user_id, s->pack_id, s->pack_name, (void *)s->user_data);
      break;
    }
    case FFI_EVENT_STICKER: {
      CSticker *s = (CSticker *)data;
      void (*cb)(const char *, const char *, const char *, const char *,
                 void *) = (void *)s->cb_ptr;
      if (cb)
        cb(s->user_id, s->sticker_id, s->description, s->url,
           (void *)s->user_data);
      break;
    }
    case FFI_EVENT_STICKER_DONE: {
      CStickerDone *s = (CStickerDone *)data;
      void (*cb)(void *) = (void *)s->cb_ptr;
      if (cb)
        cb((void *)s->user_data);
      break;
    }
    case FFI_EVENT_SSO: {
      CSso *s = (CSso *)data;
      sso_url_cb(s->url);
      break;
    }
    case FFI_EVENT_CONNECTED: {
      CConnected *s = (CConnected *)data;
      connected_cb(s->user_id);
      break;
    }
    case FFI_EVENT_SAS_REQUEST: {
      CSasRequest *s = (CSasRequest *)data;
      sas_request_cb(s->user_id, s->target_user_id, s->flow_id);
      break;
    }
    case FFI_EVENT_SAS_HAVE_EMOJI: {
      CSasHaveEmoji *s = (CSasHaveEmoji *)data;
      sas_emoji_cb(s->user_id, s->target_user_id, s->flow_id, s->emojis);
      break;
    }
    case FFI_EVENT_SHOW_VERIFICATION_QR: {
      CShowVerificationQr *s = (CShowVerificationQr *)data;
      show_verification_qr_cb(s->user_id, s->target_user_id, s->html_data);
      break;
    }
    default:
      purple_debug_error("matrix-rust", "Ignored unknown event type %d\n",
                         ev_type);
    }

    purple_matrix_rust_free_event(ev_type, data);
    events_processed++;
  }

  return TRUE; // keep timer running
}

static guint rust_poll_timer_id = 0;

static gboolean plugin_load(PurplePlugin *plugin) {
  my_plugin = plugin;
  purple_matrix_rust_init();

  rust_poll_timer_id = purple_timeout_add(50, poll_rust_channel_cb, NULL);

  purple_matrix_rust_set_imgstore_add_callback(imgstore_add_cb);
  register_matrix_commands(plugin);

  /* Register Matrix UI Internal Signals */
  purple_signal_register(plugin, "matrix-ui-action-show-dashboard",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-reply",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-start-thread",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-list-threads",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-sticker",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-poll",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-clear-session",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-login-password",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-login-sso",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-set-account-defaults",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-show-members",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-join-room",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-moderate",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-kick",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-ban",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-unban",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-moderate-user",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-user-info",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-ignore-user",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-unignore-user",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-leave-room",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-verify-self",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-crypto-status",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-list-devices",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-room-settings",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-invite-user",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-send-file",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-mark-unread",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-mark-read",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-mute-room",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-unmute-room",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-search-room",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-list-polls",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-react",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-send-location",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-who-read",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-report-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-message-inspector",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-show-last-event-details",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-reply-pick-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-thread-pick-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-react-pick-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-edit-pick-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-redact-pick-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-report-pick-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-search-users",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-search-public",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-search-members-global",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-search-room-global",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-discover-public-rooms",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-discover-room-preview",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-redact-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-edit-event",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-react-latest",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-edit-latest",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-redact-latest",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-report-latest",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-versions",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-enable-key-backup",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-my-profile",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-server-info",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-resync-recent",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-search-stickers",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-recover-keys",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-export-keys",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-set-avatar",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-room-tag",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-upgrade-room",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-alias-create",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-alias-delete",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-space-hierarchy",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-knock",
                         handle_ui_action_marshal, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));

  /* Signals from Core to UI plugin */
  purple_signal_register(plugin, "matrix-ui-room-typing",
                         handle_room_typing_marshal, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_BOOLEAN));
  purple_signal_register(plugin, "matrix-ui-room-encrypted",
                         handle_room_encrypted_marshal, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_BOOLEAN));
  purple_signal_register(plugin, "matrix-ui-room-muted",
                         handle_room_muted_marshal, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_BOOLEAN));
  purple_signal_register(plugin, "matrix-ui-room-activity",
                         handle_room_activity_marshal, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-reactions-changed",
                         handle_room_activity_marshal, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-message-edited",
                         handle_room_activity_marshal, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-read-receipt",
                         handle_room_activity_marshal, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-room-topic",
                         handle_room_topic_marshal, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_connect(plugin, "matrix-ui-action-show-dashboard", plugin,
                        PURPLE_CALLBACK(handle_show_dashboard_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-reply", plugin,
                        PURPLE_CALLBACK(handle_reply_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-start-thread", plugin,
                        PURPLE_CALLBACK(handle_start_thread_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-list-threads", plugin,
                        PURPLE_CALLBACK(handle_list_threads_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-sticker", plugin,
                        PURPLE_CALLBACK(handle_sticker_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-poll", plugin,
                        PURPLE_CALLBACK(handle_poll_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-clear-session", plugin,
                        PURPLE_CALLBACK(handle_clear_session_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-login-password", plugin,
                        PURPLE_CALLBACK(handle_login_password_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-login-sso", plugin,
                        PURPLE_CALLBACK(handle_login_sso_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-set-account-defaults", plugin,
                        PURPLE_CALLBACK(handle_set_account_defaults_signal),
                        NULL);
  purple_signal_connect(plugin, "matrix-ui-action-show-members", plugin,
                        PURPLE_CALLBACK(handle_show_members_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-join-room", plugin,
                        PURPLE_CALLBACK(handle_join_room_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-moderate", plugin,
                        PURPLE_CALLBACK(handle_moderate_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-kick", plugin,
                        PURPLE_CALLBACK(handle_kick_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-ban", plugin,
                        PURPLE_CALLBACK(handle_ban_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-unban", plugin,
                        PURPLE_CALLBACK(handle_unban_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-moderate-user", plugin,
                        PURPLE_CALLBACK(handle_moderate_user_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-user-info", plugin,
                        PURPLE_CALLBACK(handle_user_info_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-ignore-user", plugin,
                        PURPLE_CALLBACK(handle_ignore_user_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-unignore-user", plugin,
                        PURPLE_CALLBACK(handle_unignore_user_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-leave-room", plugin,
                        PURPLE_CALLBACK(handle_leave_room_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-verify-self", plugin,
                        PURPLE_CALLBACK(handle_verify_self_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-crypto-status", plugin,
                        PURPLE_CALLBACK(handle_crypto_status_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-list-devices", plugin,
                        PURPLE_CALLBACK(handle_list_devices_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-room-settings", plugin,
                        PURPLE_CALLBACK(handle_room_settings_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-invite-user", plugin,
                        PURPLE_CALLBACK(handle_invite_user_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-send-file", plugin,
                        PURPLE_CALLBACK(handle_send_file_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-mark-unread", plugin,
                        PURPLE_CALLBACK(handle_mark_unread_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-mark-read", plugin,
                        PURPLE_CALLBACK(handle_mark_read_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-mute-room", plugin,
                        PURPLE_CALLBACK(handle_mute_room_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-unmute-room", plugin,
                        PURPLE_CALLBACK(handle_unmute_room_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-search-room", plugin,
                        PURPLE_CALLBACK(handle_search_room_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-list-polls", plugin,
                        PURPLE_CALLBACK(handle_list_polls_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-react", plugin,
                        PURPLE_CALLBACK(handle_react_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-send-location", plugin,
                        PURPLE_CALLBACK(handle_send_location_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-who-read", plugin,
                        PURPLE_CALLBACK(handle_who_read_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-report-event", plugin,
                        PURPLE_CALLBACK(handle_report_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-message-inspector", plugin,
                        PURPLE_CALLBACK(handle_message_inspector_signal), NULL);
  purple_signal_connect(
      plugin, "matrix-ui-action-show-last-event-details", plugin,
      PURPLE_CALLBACK(handle_show_last_event_details_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-reply-pick-event", plugin,
                        PURPLE_CALLBACK(handle_reply_pick_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-thread-pick-event", plugin,
                        PURPLE_CALLBACK(handle_thread_pick_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-react-pick-event", plugin,
                        PURPLE_CALLBACK(handle_react_pick_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-edit-pick-event", plugin,
                        PURPLE_CALLBACK(handle_edit_pick_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-redact-pick-event", plugin,
                        PURPLE_CALLBACK(handle_redact_pick_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-report-pick-event", plugin,
                        PURPLE_CALLBACK(handle_report_pick_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-search-users", plugin,
                        PURPLE_CALLBACK(handle_search_users_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-search-public", plugin,
                        PURPLE_CALLBACK(handle_search_public_signal), NULL);
  purple_signal_connect(
      plugin, "matrix-ui-action-search-members-global", plugin,
      PURPLE_CALLBACK(handle_search_members_global_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-search-room-global", plugin,
                        PURPLE_CALLBACK(handle_search_room_global_signal),
                        NULL);
  purple_signal_connect(
      plugin, "matrix-ui-action-discover-public-rooms", plugin,
      PURPLE_CALLBACK(handle_discover_public_rooms_signal), NULL);
  purple_signal_connect(
      plugin, "matrix-ui-action-discover-room-preview", plugin,
      PURPLE_CALLBACK(handle_discover_room_preview_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-redact-event", plugin,
                        PURPLE_CALLBACK(handle_redact_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-edit-event", plugin,
                        PURPLE_CALLBACK(handle_edit_event_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-react-latest", plugin,
                        PURPLE_CALLBACK(handle_react_latest_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-edit-latest", plugin,
                        PURPLE_CALLBACK(handle_edit_latest_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-redact-latest", plugin,
                        PURPLE_CALLBACK(handle_redact_latest_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-report-latest", plugin,
                        PURPLE_CALLBACK(handle_report_latest_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-versions", plugin,
                        PURPLE_CALLBACK(handle_versions_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-enable-key-backup", plugin,
                        PURPLE_CALLBACK(handle_enable_key_backup_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-my-profile", plugin,
                        PURPLE_CALLBACK(handle_my_profile_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-server-info", plugin,
                        PURPLE_CALLBACK(handle_server_info_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-resync-recent", plugin,
                        PURPLE_CALLBACK(handle_resync_recent_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-search-stickers", plugin,
                        PURPLE_CALLBACK(handle_search_stickers_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-recover-keys", plugin,
                        PURPLE_CALLBACK(handle_recover_keys_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-export-keys", plugin,
                        PURPLE_CALLBACK(handle_export_keys_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-set-avatar", plugin,
                        PURPLE_CALLBACK(handle_set_avatar_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-room-tag", plugin,
                        PURPLE_CALLBACK(handle_room_tag_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-upgrade-room", plugin,
                        PURPLE_CALLBACK(handle_upgrade_room_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-alias-create", plugin,
                        PURPLE_CALLBACK(handle_alias_create_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-alias-delete", plugin,
                        PURPLE_CALLBACK(handle_alias_delete_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-space-hierarchy", plugin,
                        PURPLE_CALLBACK(handle_space_hierarchy_signal), NULL);
  purple_signal_connect(plugin, "matrix-ui-action-knock", plugin,
                        PURPLE_CALLBACK(handle_knock_signal), NULL);

  if (purple_get_core() != NULL && purple_conversations_get_handle() != NULL)
    connect_signals();
  else
    purple_signal_connect(purple_get_core(), "core-initialized", plugin,
                          PURPLE_CALLBACK(core_initialized_cb), NULL);

  purple_prefs_connect_callback(plugin, MATRIX_PREF_SESSION_ACTION,
                                matrix_pref_changed_cb, NULL);

  return TRUE;
}

static PurplePluginProtocolInfo prpl_info = {
    .options =
        OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL | OPT_PROTO_IM_IMAGE,
    .user_splits = NULL,
    .protocol_options = NULL,
    .icon_spec = {.format = "png",
                  .min_width = 30,
                  .min_height = 30,
                  .max_width = 96,
                  .max_height = 96,
                  .scale_rules =
                      PURPLE_ICON_SCALE_SEND | PURPLE_ICON_SCALE_DISPLAY},
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
    .send_typing = matrix_send_typing,
    .get_info = matrix_get_info,
    .set_status = matrix_set_status,
    .set_idle = matrix_set_idle,
    .change_passwd = matrix_change_passwd,
    .add_buddy = matrix_add_buddy,
    .remove_buddy = matrix_remove_buddy,
    .add_deny = matrix_add_deny,
    .rem_deny = matrix_rem_deny,
    .join_chat = matrix_join_chat,
    .reject_chat = matrix_reject_chat,
    .get_chat_name = matrix_get_chat_name,
    .chat_invite = matrix_chat_invite,
    .chat_leave = NULL,
    .chat_whisper = matrix_chat_whisper,
    .chat_send = matrix_chat_send,
    .set_chat_topic = matrix_set_chat_topic,
    .send_file = matrix_send_file,
    .roomlist_get_list = matrix_roomlist_get_list,
    .roomlist_cancel = matrix_roomlist_cancel,
    .roomlist_expand_category = matrix_roomlist_expand_category,
    .unregister_user = matrix_unregister_user,
    .set_public_alias = (void *)matrix_set_public_alias,
    .set_buddy_icon = matrix_set_buddy_icon,
    .struct_size = sizeof(PurplePluginProtocolInfo)};

static PurplePluginInfo info = {.magic = PURPLE_PLUGIN_MAGIC,
                                .major_version = PURPLE_MAJOR_VERSION,
                                .minor_version = PURPLE_MINOR_VERSION,
                                .type = PURPLE_PLUGIN_PROTOCOL,
                                .priority = PURPLE_PRIORITY_DEFAULT,
                                .id = "prpl-matrix-rust",
                                .name = "Matrix (Rust)",
                                .version = "0.4.0",
                                .summary = "Matrix Protocol Plugin",
                                .description = "Using matrix-rust-sdk",
                                .load = plugin_load,
                                .unload = plugin_unload,
                                .extra_info = &prpl_info,
                                .prefs_info = &prefs_info,
                                .actions = matrix_actions};

static void init_plugin(PurplePlugin *plugin) {
  PurpleAccountOption *o;
  o = purple_account_option_string_new("Homeserver URL", "server",
                                       "https://matrix.org");
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, o);
  o = purple_account_option_bool_new("Separate Thread Tabs", "separate_threads",
                                     FALSE);
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, o);
  o = purple_account_option_bool_new("Auto Fetch History On Open",
                                     "auto_fetch_history_on_open", TRUE);
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, o);
  o = purple_account_option_string_new("History Page Size", "history_page_size",
                                       "50");
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, o);

  if (!purple_prefs_exists(MATRIX_PREF_ROOT))
    purple_prefs_add_none(MATRIX_PREF_ROOT);
  if (!purple_prefs_exists(MATRIX_PREF_HOMESERVER))
    purple_prefs_add_string(MATRIX_PREF_HOMESERVER, "https://matrix.org");
  if (!purple_prefs_exists(MATRIX_PREF_USERNAME))
    purple_prefs_add_string(MATRIX_PREF_USERNAME, "");
  if (!purple_prefs_exists(MATRIX_PREF_SESSION_ACTION))
    purple_prefs_add_string(MATRIX_PREF_SESSION_ACTION, "none");
}

static gboolean plugin_unload(PurplePlugin *plugin) {
  if (rust_poll_timer_id > 0) {
    purple_timeout_remove(rust_poll_timer_id);
    rust_poll_timer_id = 0;
  }

  purple_signals_disconnect_by_handle(plugin);
  purple_signals_unregister_by_instance(plugin);
  purple_prefs_disconnect_by_handle(plugin);

  if (room_id_map) {
    g_hash_table_destroy(room_id_map);
    room_id_map = NULL;
  }

  matrix_utils_cleanup();

  if (prpl_info.protocol_options) {
    g_list_free_full(prpl_info.protocol_options,
                     (GDestroyNotify)purple_account_option_destroy);
    prpl_info.protocol_options = NULL;
  }

  return TRUE;
}

PURPLE_INIT_PLUGIN(matrix_rust, init_plugin, info)
