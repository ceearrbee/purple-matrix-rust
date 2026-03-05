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

PurplePlugin *my_plugin = NULL;

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
                   s->event_id, s->timestamp, s->encrypted, s->is_system);
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
                           s->avatar_url, s->topic, s->encrypted, s->member_count);
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
      poll_list_cb(s->user_id, s->room_id, s->event_id, s->sender, s->question,
                   s->options_str);
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
        cb(s->user_id, s->sticker_id, s->description, s->uri,
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
    case FFI_EVENT_SSO_URL: {
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
    case FFI_EVENT_READ_RECEIPT: {
      CReadReceipt *s = (CReadReceipt *)data;
      read_receipt_cb(s->user_id, s->room_id, s->receipt_user_id,
                            s->event_id);
      break;
    }
    case 33: {
      CPollCreationRequested *s = (CPollCreationRequested *)data;
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

static gboolean plugin_load(PurplePlugin *plugin) {
  my_plugin = plugin;
  purple_matrix_rust_init();
  register_signals(plugin);
  register_matrix_commands(plugin);
  g_timeout_add(100, poll_rust_channel_cb, NULL);
  return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
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
    .get_info = matrix_get_info,
    .set_status = matrix_set_status,
    .set_idle = matrix_set_idle,
    .change_passwd = matrix_change_passwd,
    .add_buddy = matrix_add_buddy,
    .remove_buddy = matrix_remove_buddy,
    .join_chat = matrix_join_chat,
    .reject_chat = matrix_reject_chat,
    .get_chat_name = matrix_get_chat_name,
    .chat_invite = matrix_chat_invite,
    .chat_leave = matrix_chat_leave,
    .chat_whisper = matrix_chat_whisper,
    .chat_send = matrix_chat_send,
    .set_chat_topic = matrix_set_chat_topic,
    .send_file = matrix_send_file,
    .roomlist_get_list = matrix_roomlist_get_list,
    .roomlist_cancel = matrix_roomlist_cancel,
    .roomlist_expand_category = matrix_roomlist_expand_category,
    .struct_size = sizeof(PurplePluginProtocolInfo)};

static PurplePluginInfo info = {
    .magic = PURPLE_PLUGIN_MAGIC,
    .major_version = PURPLE_MAJOR_VERSION,
    .minor_version = PURPLE_MINOR_VERSION,
    .type = PURPLE_PLUGIN_PROTOCOL,
    .priority = PURPLE_PRIORITY_DEFAULT,
    .id = "prpl-matrix-rust",
    .name = "Matrix (Rust)",
    .version = "0.1.0",
    .summary = "Matrix Protocol Plugin using matrix-rust-sdk",
    .description = "Connect to Matrix with E2EE, Reactions, and More.",
    .author = "crb",
    .homepage = "https://matrix.org",
    .load = plugin_load,
    .unload = plugin_unload,
    .extra_info = &prpl_info,
};

static void init_plugin(PurplePlugin *plugin) {
  PurpleAccountOption *option;

  option = purple_account_option_string_new("Homeserver", "homeserver",
                                            "https://matrix.org");
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

  option = purple_account_option_bool_new("Use SSO", "use_sso", FALSE);
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(matrix_rust, init_plugin, info)
