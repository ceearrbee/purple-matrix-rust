#define PURPLE_PLUGINS
#include "matrix_globals.h"
#include "matrix_account.h"
#include "matrix_blist.h"
#include "matrix_chat.h"
#include "matrix_commands.h"
#include "matrix_utils.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_types.h"

#include <libpurple/plugin.h>
#include <libpurple/version.h>
#include <libpurple/prpl.h>
#include <libpurple/accountopt.h>
#include <string.h>
#include <stdio.h>

PurplePlugin *my_plugin = NULL;
GHashTable *room_id_map = NULL;

static PurplePluginProtocolInfo prpl_info = {
    .options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD,
    .user_splits = NULL,
    .protocol_options = NULL,
    .icon_spec = {
        .format = "png",
        .min_width = 30, .min_height = 30,
        .max_width = 96, .max_height = 96,
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
    .get_chat_name = matrix_get_chat_name,
    .chat_invite = matrix_chat_invite,
    .chat_leave = matrix_chat_leave,
    .chat_whisper = matrix_chat_whisper,
    .chat_send = matrix_chat_send,
    .set_chat_topic = matrix_set_chat_topic,
    .send_file = matrix_send_file,
    .roomlist_get_list = matrix_roomlist_get_list,
    .roomlist_cancel = matrix_roomlist_cancel,
    .unregister_user = matrix_unregister_user,
    .set_public_alias = (void *)matrix_set_public_alias,
    .set_buddy_icon = matrix_set_buddy_icon,
    .struct_size = sizeof(PurplePluginProtocolInfo)
};

static void conversation_displayed_cb(PurpleConversation *conv) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
    const char *last_id = purple_conversation_get_data(conv, "last_event_id");
    if (last_id) {
      purple_matrix_rust_send_read_receipt(purple_account_get_username(account), purple_conversation_get_name(conv), last_id);
    }
  }
}

static void conversation_created_cb(PurpleConversation *conv) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
    if (purple_account_get_bool(account, "auto_fetch_history_on_open", TRUE)) {
      purple_matrix_rust_fetch_history(purple_account_get_username(account), purple_conversation_get_name(conv));
    }
  }
}

static gboolean plugin_load(PurplePlugin *plugin) {
  my_plugin = plugin;
  purple_matrix_rust_init();
  purple_matrix_rust_set_msg_callback(msg_callback);
  purple_matrix_rust_set_typing_callback(typing_callback);
  purple_matrix_rust_set_room_joined_callback(room_joined_callback);
  purple_matrix_rust_set_room_left_callback(room_left_callback);
  purple_matrix_rust_set_read_marker_callback(read_marker_cb);
  purple_matrix_rust_set_presence_callback(presence_callback);
  purple_matrix_rust_set_chat_topic_callback(chat_topic_callback);
  purple_matrix_rust_set_chat_user_callback(chat_user_callback);
  purple_matrix_rust_set_show_user_info_callback(show_user_info_cb);
  purple_matrix_rust_set_roomlist_add_callback(roomlist_add_cb);
  purple_matrix_rust_set_room_preview_callback(room_preview_cb);
  purple_matrix_rust_set_thread_list_callback(thread_list_cb);
  purple_matrix_rust_set_poll_list_callback(poll_list_cb);
  matrix_init_sso_callbacks();
  register_matrix_commands(plugin);

  purple_signal_connect(purple_conversations_get_handle(), "conversation-created",
                        plugin, PURPLE_CALLBACK(conversation_created_cb), NULL);
  purple_signal_connect(purple_conversations_get_handle(), "conversation-displayed",
                        plugin, PURPLE_CALLBACK(conversation_displayed_cb), NULL);

  return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
  purple_signals_disconnect_by_handle(plugin);
  return TRUE;
}

static PurplePluginInfo info = {
    .magic = PURPLE_PLUGIN_MAGIC,
    .major_version = PURPLE_MAJOR_VERSION,
    .minor_version = PURPLE_MINOR_VERSION,
    .type = PURPLE_PLUGIN_PROTOCOL,
    .priority = PURPLE_PRIORITY_DEFAULT,
    .id = "prpl-matrix-rust",
    .name = "Matrix (Rust)",
    .version = "0.1",
    .summary = "Matrix Protocol Plugin (Rust Backend)",
    .description = "Using matrix-rust-sdk",
    .author = "Author Name",
    .homepage = "https://matrix.org",
    .load = plugin_load,
    .unload = plugin_unload,
    .extra_info = &prpl_info,
    .actions = matrix_actions
};

static void init_plugin(PurplePlugin *plugin) {
  PurpleAccountOption *option;
  option = purple_account_option_string_new("Homeserver URL", "server", "https://matrix.org");
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
  option = purple_account_option_bool_new("Separate Thread Tabs", "separate_threads", FALSE);
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
  option = purple_account_option_bool_new("Auto Fetch History On Open", "auto_fetch_history_on_open", TRUE);
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
  option = purple_account_option_string_new("History Page Size", "history_page_size", "50");
  prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(matrix_rust, init_plugin, info)
