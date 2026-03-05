#define PURPLE_PLUGINS
#include "matrix_account.h"
#include "matrix_blist.h"
#include "matrix_chat.h"
#include "matrix_commands.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/plugin.h>
#include <libpurple/version.h>
#include <libpurple/accountopt.h>

#include <string.h>

/* Custom marshaller for media-downloaded signal */
static void
purple_marshal_VOID__POINTER_POINTER_POINTER_UINT_POINTER(PurpleCallback cb,
                                                         va_list args,
                                                         void *data,
                                                         void **return_val) {
  void (*callback)(gpointer, gpointer, gpointer, guint, gpointer, gpointer) =
      (void (*)(gpointer, gpointer, gpointer, guint, gpointer,
                gpointer))cb;
  gpointer arg1 = va_arg(args, gpointer);
  gpointer arg2 = va_arg(args, gpointer);
  gpointer arg3 = va_arg(args, gpointer);
  guint arg4 = va_arg(args, guint);
  gpointer arg5 = va_arg(args, gpointer);

  callback(arg1, arg2, arg3, arg4, arg5, data);
}

PurplePlugin *my_plugin = NULL;
GHashTable *room_id_map = NULL;
time_t plugin_start_time = 0;

static gboolean poll_rust_channel_cb(gpointer user_data) {
  int ev_type = -1;
  void *data = NULL;

  int events_processed = 0;
  while (purple_matrix_rust_poll_event(&ev_type, &data) &&
         events_processed < 2000) {
    if (!data)
      continue;

    fprintf(stderr, "[Matrix FFI] Received event type %d\n", ev_type);
    purple_debug_info("matrix-ffi", "Received FFI event type %d\n", ev_type);
    switch (ev_type) {
    case FFI_EVENT_MESSAGE_RECEIVED: {
      MessageEventData *s = (MessageEventData *)data;
      purple_debug_info("matrix-ffi", "FFI_EVENT_MESSAGE_RECEIVED: room_id=%s\n", s->room_id);
      msg_callback(s->user_id, s->sender, s->msg, s->room_id, s->thread_root_id,
                   s->event_id, s->timestamp, s->encrypted, false);
      break;
    }
    case FFI_EVENT_TYPING_STATE: {
      TypingEventData *s = (TypingEventData *)data;
      typing_callback(s->user_id, s->room_id, s->who, s->is_typing);
      break;
    }
    case FFI_EVENT_ROOM_JOINED: {
      RoomJoinedEventData *s = (RoomJoinedEventData *)data;
      purple_debug_info("matrix-ffi", "FFI_EVENT_ROOM_JOINED: room_id=%s name=%s\n", s->room_id, s->name ? s->name : "(null)");
      room_joined_callback(s->user_id, s->room_id, s->name, s->group_name,
                           s->avatar_url, s->topic, s->encrypted, s->member_count);
      break;
    }
    case FFI_EVENT_ROOM_LEFT: {
      // Assuming CRoomLeft matches what I might add later, but using existing callback
      // My RoomJoinedEventData has more fields, I should be careful.
      break;
    }
    case FFI_EVENT_READ_MARKER: {
      ReadMarkerEventData *s = (ReadMarkerEventData *)data;
      read_marker_cb(s->user_id, s->room_id, s->event_id, s->who);
      break;
    }
    case FFI_EVENT_CHAT_TOPIC_UPDATE: {
      ChatTopicEventData *s = (ChatTopicEventData *)data;
      chat_topic_callback(s->user_id, s->room_id, s->topic, s->sender);
      break;
    }
    case FFI_EVENT_CHAT_MEMBER_UPDATE: {
      ChatUserEventData *s = (ChatUserEventData *)data;
      chat_user_callback(s->user_id, s->room_id, s->member_id, s->add, s->alias,
                         s->avatar_path);
      break;
    }
    case FFI_EVENT_INVITE_RECEIVED: {
      InviteEventData *s = (InviteEventData *)data;
      invite_callback(s->user_id, s->room_id, s->inviter);
      break;
    }
    case FFI_EVENT_LOGIN_FAILED: {
      LoginFailedEventData *s = (LoginFailedEventData *)data;
      login_failed_cb(s->error_msg);
      break;
    }
    case FFI_EVENT_SSO_URL_RECEIVED: {
      SsoUrlEventData *s = (SsoUrlEventData *)data;
      sso_url_cb(s->url);
      break;
    }
    case FFI_EVENT_CONNECTED: {
      ConnectedEventData *s = (ConnectedEventData *)data;
      connected_cb(s->user_id);
      break;
    }
    case FFI_EVENT_MESSAGE_EDITED: {
      MessageEditedData *s = (MessageEditedData *)data;
      message_edited_callback(s->user_id, s->room_id, s->event_id, s->new_msg);
      break;
    }
    case FFI_EVENT_MESSAGE_REDACTED: {
      MessageRedactedEventData *s = (MessageRedactedEventData *)data;
      message_redacted_callback(s->user_id, s->room_id, s->event_id);
      break;
    }
    case FFI_EVENT_MEDIA_DOWNLOADED: {
      MediaDownloadedEventData *s = (MediaDownloadedEventData *)data;
      media_downloaded_callback(s->user_id, s->room_id, s->event_id, s->media_data, s->media_size, s->content_type);
      break;
    }
    case FFI_EVENT_REACTION_UPDATE: {
      ReactionsChangedEventData *s = (ReactionsChangedEventData *)data;
      reactions_changed_callback(s->user_id, s->room_id, s->event_id, s->reactions_text);
      break;
    }
    case FFI_EVENT_POWER_LEVEL_UPDATE: {
      PowerLevelUpdateEventData *s = (PowerLevelUpdateEventData *)data;
      power_level_update_callback(s->user_id, s->room_id, s->is_admin,
                                  s->can_kick, s->can_ban, s->can_redact,
                                  s->can_invite);
      break;
    }
    case FFI_EVENT_BUDDY_UPDATE: {
      UpdateBuddyEventData *s = (UpdateBuddyEventData *)data;
      update_buddy_callback(s->user_id, s->alias, s->avatar_url);
      break;
    }
    case FFI_EVENT_PRESENCE_UPDATE: {
      PresenceEventData *s = (PresenceEventData *)data;
      presence_callback(s->user_id, s->target_user_id, s->status, s->status_msg);
      break;
    }
    case FFI_EVENT_VERIFICATION_REQUEST: {
      SasRequestEventData *s = (SasRequestEventData *)data;
      sas_request_cb(s->user_id, s->target_user_id, s->flow_id);
      break;
    }
    case FFI_EVENT_VERIFICATION_EMOJI: {
      SasHaveEmojiEventData *s = (SasHaveEmojiEventData *)data;
      sas_emoji_cb(s->user_id, s->target_user_id, s->flow_id, s->emojis);
      break;
    }
    case FFI_EVENT_VERIFICATION_QR: {
      ShowVerificationQrEventData *s = (ShowVerificationQrEventData *)data;
      show_verification_qr_cb(s->user_id, s->target_user_id, s->html_data);
      break;
    }
    case FFI_EVENT_USER_INFO_RECEIVED: {
      ShowUserInfoEventData *s = (ShowUserInfoEventData *)data;
      show_user_info_cb(s->user_id, s->target_user_id, s->display_name, s->avatar_url);
      break;
    }
    case FFI_EVENT_POLL_LIST_RECEIVED: {
      PollListEventData *s = (PollListEventData *)data;
      poll_list_callback(s->user_id, s->room_id, s->event_id, s->question, s->sender, s->options_str);
      break;
    }
    case FFI_EVENT_THREAD_LIST_RECEIVED: {
      ThreadListEventData *s = (ThreadListEventData *)data;
      thread_list_callback(s->user_id, s->room_id, s->thread_root_id, s->latest_msg);
      break;
    }
    case FFI_EVENT_ROOM_LIST_ADDED: {
      RoomListAddEventData *s = (RoomListAddEventData *)data;
      room_list_add_callback(s->user_id, s->room_id, s->name, s->topic, s->parent_id);
      break;
    }
    case FFI_EVENT_ROOM_PREVIEW: {
      RoomPreviewEventData *s = (RoomPreviewEventData *)data;
      room_preview_cb(s->user_id, s->room_id_or_alias, s->html_body);
      break;
    }
    case FFI_EVENT_ROOM_DASHBOARD_INFO: {
      RoomDashboardInfoEventData *s = (RoomDashboardInfoEventData *)data;
      room_dashboard_info_callback(s->user_id, s->room_id, s->name, s->topic, s->member_count, s->encrypted, s->power_level, s->alias);
      break;
    }
    case 33: { // PollCreationRequested
      PollCreationRequestedEventData *s = (PollCreationRequestedEventData *)data;
      poll_creation_callback(s->user_id, s->room_id);
      break;
    }
    default:
      purple_debug_warning("matrix-ffi", "Unhandled event type %d\n", ev_type);
      break;
    }

    purple_matrix_rust_free_event(ev_type, data);
    events_processed++;
  }

  return TRUE;
}

static void register_signals(PurplePlugin *plugin) {
  purple_signal_register(plugin, "matrix-ui-room-activity",
                         purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_register(plugin, "matrix-ui-room-typing",
                         purple_marshal_VOID__POINTER_POINTER_UINT, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_BOOLEAN));

  purple_signal_register(plugin, "matrix-ui-room-encrypted",
                         purple_marshal_VOID__POINTER_UINT, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_BOOLEAN));

  purple_signal_register(plugin, "matrix-ui-room-muted",
                         purple_marshal_VOID__POINTER_UINT, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_BOOLEAN));

  purple_signal_register(plugin, "matrix-ui-reactions-changed",
                         purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_register(plugin, "matrix-ui-message-edited",
                         purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_register(plugin, "matrix-ui-message-redacted",
                         purple_marshal_VOID__POINTER_POINTER, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_register(plugin, "matrix-ui-media-downloaded",
                         purple_marshal_VOID__POINTER_POINTER_POINTER_UINT_POINTER, NULL, 5,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_POINTER),
                         purple_value_new(PURPLE_TYPE_UINT),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_register(plugin, "matrix-ui-read-receipt",
                         purple_marshal_VOID__POINTER_POINTER_POINTER, NULL, 3,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  purple_signal_register(plugin, "matrix-ui-room-topic",
                         purple_marshal_VOID__POINTER_POINTER, NULL, 2,
                         purple_value_new(PURPLE_TYPE_STRING),
                         purple_value_new(PURPLE_TYPE_STRING));

  /* UI Actions from context menu or dashboard */
  purple_signal_register(plugin, "matrix-ui-action-reply",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-react-pick-event",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-edit-pick-event",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-redact-pick-event",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-poll",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-list-polls",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-start-thread",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-list-threads",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-show-dashboard",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-moderate",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-room-settings",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-invite-user",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-send-file",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-mark-unread",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-mark-read",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-send-location",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
  purple_signal_register(plugin, "matrix-ui-action-show-last-event-details",
                         purple_marshal_VOID__POINTER, NULL, 1,
                         purple_value_new(PURPLE_TYPE_STRING));
}

static void conversation_deleted_cb(PurpleConversation *conv, gpointer data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (account &&
      strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
    purple_matrix_rust_close_conversation(purple_account_get_username(account),
                                          purple_conversation_get_name(conv));
  }
}

static gboolean plugin_load_complete_cb(gpointer data) {
  PurplePlugin *plugin = (PurplePlugin *)data;
  void *conv_handle = purple_conversations_get_handle();
  if (conv_handle) {
    /* Try to connect. We use g_signal_lookup or similar if we wanted to be sure, 
       but libpurple's signal system doesn't have a simple 'exists' check easily accessible here.
       Instead, we rely on purple_signal_connect not failing silently if it can. */
    purple_signal_connect(conv_handle, "deleting-conversation", plugin,
                          PURPLE_CALLBACK(conversation_deleted_cb), NULL);
    purple_debug_info("matrix-ffi", "plugin_load_complete_cb: Connected signals\n");
    return FALSE;
  }
  return TRUE; /* Retry if handle not yet available */
}

static gboolean plugin_load(PurplePlugin *plugin) {
  my_plugin = plugin;
  plugin_start_time = time(NULL);
  room_id_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  purple_matrix_rust_init();
  register_signals(plugin);
  register_matrix_commands(plugin);
  g_timeout_add(100, poll_rust_channel_cb, NULL);

  /* Use a repeating timer to ensure we catch the conversation handle initialization */
  g_timeout_add(500, plugin_load_complete_cb, plugin);

  return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
  if (room_id_map) {
    g_hash_table_destroy(room_id_map);
    room_id_map = NULL;
  }
  return TRUE;
}

static PurplePluginProtocolInfo prpl_info = {
    .options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD |
               OPT_PROTO_SLASH_COMMANDS_NATIVE,
    .icon_spec = {"png", 0, 0, 48, 48, 0, PURPLE_ICON_SCALE_DISPLAY},
    .list_icon = matrix_list_icon,
    .login = matrix_login,
    .close = matrix_close,
    .status_types = matrix_status_types,
    .blist_node_menu = blist_node_menu_cb,
    .chat_info = matrix_chat_info,
    .chat_info_defaults = matrix_chat_info_defaults,
    .send_im = matrix_send_im,
    .send_typing = matrix_send_typing,
    .join_chat = matrix_join_chat,
    .reject_chat = matrix_reject_chat,
    .get_chat_name = matrix_get_chat_name,
    .chat_invite = matrix_chat_invite,
    .chat_leave = matrix_chat_leave,
    .chat_whisper = matrix_chat_whisper,
    .chat_send = matrix_send_chat,
    .set_chat_topic = matrix_set_chat_topic,
    .send_file = matrix_send_file,
    .roomlist_get_list = matrix_roomlist_get_list,
    .roomlist_cancel = matrix_roomlist_cancel,
    .set_status = matrix_set_status,
    .set_idle = matrix_set_idle,
    .add_buddy = matrix_add_buddy,
    .remove_buddy = matrix_remove_buddy,
    .struct_size = sizeof(PurplePluginProtocolInfo)};

static PurplePluginInfo info = {
    PURPLE_PLUGIN_MAGIC,
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,
    NULL,
    0,
    NULL,
    PURPLE_PRIORITY_DEFAULT,
    "prpl-matrix-rust",
    "Matrix (Rust)",
    "0.1.0",
    "Matrix Protocol Plugin (Rust Edition)",
    "Matrix Protocol Plugin using matrix-rust-sdk",
    "Author <author@example.com>",
    "https://github.com/example/purple-matrix-rust",
    plugin_load,
    plugin_unload,
    NULL,
    NULL,
    &prpl_info,
    NULL,
    matrix_actions,
    NULL,
    NULL,
    NULL,
    NULL};

static void init_plugin(PurplePlugin *plugin) {
  PurpleAccountOption *option;

  option = purple_account_option_string_new("Homeserver URL", "server",
                                            "https://matrix.org");
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

  option = purple_account_option_bool_new("Separate thread tabs",
                                          "separate_threads", FALSE);
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(matrix_rust, init_plugin, info)
