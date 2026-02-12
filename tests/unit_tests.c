#define TESTING
#define PURPLE_PLUGINS

#include <glib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Include real headers
#include <libpurple/account.h>
#include <libpurple/accountopt.h>
#include <libpurple/blist.h>
#include <libpurple/cmds.h>
#include <libpurple/connection.h>
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

// Mock Global State
GHashTable *mock_blist_chats = NULL;

// --- Mocks for Libpurple Functions (needed by plugin.c) ---

void purple_debug_info(const char *cat, const char *format, ...) {}

void purple_debug_error(const char *cat, const char *format, ...) {
  va_list args;
  va_start(args, format);
  printf("[ERROR] %s: ", cat);
  vprintf(format, args);
  printf("\n");
  va_end(args);
}

void purple_debug_warning(const char *cat, const char *format, ...) {}

// Conversation Mocks
PurpleConversation *purple_find_chat(const PurpleConnection *gc, int id) {
  return NULL;
}
PurpleConversation *
purple_find_conversation_with_account(PurpleConversationType type,
                                      const char *name,
                                      const PurpleAccount *account) {
  return NULL;
}
void purple_conversation_set_title(PurpleConversation *conv,
                                   const char *title) {}
const char *purple_conversation_get_title(const PurpleConversation *conv) {
  return "test_title";
}
void purple_conv_chat_set_id(PurpleConvChat *chat, int id) {}
PurpleConvChat *purple_conv_chat_new(PurpleConversation *conv) { return NULL; }
void purple_conv_chat_add_user(PurpleConvChat *chat, const char *user,
                               const char *alias,
                               PurpleConvChatBuddyFlags flags,
                               gboolean new_arrival) {}
void purple_conv_chat_write(PurpleConvChat *chat, const char *who,
                            const char *message, PurpleMessageFlags flags,
                            time_t mtime) {}
void purple_conversation_write(PurpleConversation *conv, const char *who,
                               const char *message, PurpleMessageFlags flags,
                               time_t mtime) {}
void purple_conversation_set_data(PurpleConversation *conv, const char *key,
                                  gpointer data) {}
gpointer purple_conversation_get_data(PurpleConversation *conv,
                                      const char *key) {
  return NULL;
}
const char *purple_conversation_get_name(const PurpleConversation *conv) {
  return "test_room";
}
PurpleAccount *purple_conversation_get_account(const PurpleConversation *conv) {
  return NULL;
}
PurpleConversationType
purple_conversation_get_type(const PurpleConversation *conv) {
  return PURPLE_CONV_TYPE_CHAT;
}
void purple_conversation_present(PurpleConversation *conv) {}
void purple_conv_chat_set_topic(PurpleConvChat *chat, const char *who,
                                const char *topic) {}
void purple_conv_chat_remove_user(PurpleConvChat *chat, const char *user,
                                  const char *reason) {}
PurpleConversation *purple_conversation_new(PurpleConversationType type,
                                            PurpleAccount *account,
                                            const char *name) {
  return NULL;
}
void purple_conversation_set_name(PurpleConversation *conv, const char *name) {}
gboolean purple_conv_chat_find_user(PurpleConvChat *chat, const char *user) {
  return FALSE;
}
PurpleConvChat *
purple_conversation_get_chat_data(const PurpleConversation *conv) {
  return (PurpleConvChat *)1;
}

// Blist Mocks
PurpleBlistNode *purple_blist_get_root() { return NULL; }
void purple_blist_add_group(PurpleGroup *group, PurpleBlistNode *node) {}
void purple_blist_add_chat(PurpleChat *chat, PurpleGroup *group,
                           PurpleBlistNode *node) {}
void purple_blist_alias_chat(PurpleChat *chat, const char *alias) {}
PurpleGroup *purple_find_group(const char *name) { return NULL; }
PurpleGroup *purple_group_new(const char *name) { return NULL; }
PurpleChat *purple_chat_new(PurpleAccount *account, const char *alias,
                            GHashTable *components) {
  // Return a dummy pointer that is NOT a real struct, but valid for pointer
  // arithmetic/checks in mocks We allocate enough to not crash immediate access
  // if any
  return (PurpleChat *)g_new0(char, 128);
}
GHashTable *mock_chat_components = NULL;
GHashTable *purple_chat_get_components(PurpleChat *chat) {
  if (!mock_chat_components)
    mock_chat_components =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  return mock_chat_components;
}
PurpleAccount *purple_chat_get_account(PurpleChat *chat) { return NULL; }
const char *purple_chat_get_name(PurpleChat *chat) { return "test_chat"; }
void purple_blist_node_set_string(PurpleBlistNode *node, const char *key,
                                  const char *value) {}
PurpleChat *purple_blist_find_chat(PurpleAccount *account, const char *name) {
  return NULL;
}
void purple_blist_alias_buddy(PurpleBuddy *buddy, const char *alias) {}
PurpleBuddy *purple_find_buddy(PurpleAccount *account, const char *name) {
  return NULL;
}
const char *purple_buddy_get_name(const PurpleBuddy *buddy) {
  return "test_user";
}
PurpleAccount *purple_buddy_get_account(const PurpleBuddy *buddy) {
  return NULL;
}
PurplePresence *purple_buddy_get_presence(const PurpleBuddy *buddy) {
  return NULL;
}
PurpleGroup *purple_chat_get_group(PurpleChat *chat) { return NULL; }
PurpleBlistNodeType purple_blist_node_get_type(PurpleBlistNode *node) {
  // Hack: assume it's a chat if we are testing chats
  return PURPLE_BLIST_CHAT_NODE;
}

// Account/Connection Mocks
PurpleConnection *purple_account_get_connection(const PurpleAccount *account) {
  return (PurpleConnection *)0x1;
}
const char *purple_account_get_protocol_id(const PurpleAccount *account) {
  return "prpl-matrix-rust";
}
const char *purple_account_get_username(const PurpleAccount *account) {
  return "test_user";
}
const char *purple_account_get_password(const PurpleAccount *account) {
  return "password";
}
const char *purple_account_get_string(const PurpleAccount *account,
                                      const char *name,
                                      const char *default_val) {
  return default_val;
}
gboolean purple_account_get_bool(const PurpleAccount *account, const char *name,
                                 gboolean default_val) {
  return default_val;
}
PurpleAccount *purple_connection_get_account(const PurpleConnection *gc) {
  return NULL;
}
void purple_connection_update_progress(PurpleConnection *gc, const char *text,
                                       size_t step, size_t count) {}
void purple_connection_set_state(PurpleConnection *gc,
                                 PurpleConnectionState state) {}
void purple_connection_error_reason(PurpleConnection *gc,
                                    PurpleConnectionError reason,
                                    const char *description) {}
GList *purple_accounts_get_all() { return NULL; }
GList *purple_connections_get_all() { return NULL; }
PurpleConnectionState purple_connection_get_state(const PurpleConnection *gc) {
  return PURPLE_CONNECTED;
}
PurpleAccountOption *
purple_account_option_string_new(const char *pref_name, const char *label,
                                 const char *default_value) {
  return NULL;
}
PurpleAccountOption *purple_account_option_bool_new(const char *pref_name,
                                                    const char *label,
                                                    gboolean default_value) {
  return NULL;
}

// Notify/Request Mocks
GString *mock_notify_buffer = NULL;
void *purple_notify_userinfo(PurpleConnection *gc, const char *who,
                             PurpleNotifyUserInfo *user_info,
                             PurpleNotifyCloseCallback cb, gpointer user_data) {
  return NULL;
}
PurpleNotifyUserInfo *purple_notify_user_info_new() { return NULL; }
void purple_notify_user_info_add_pair(PurpleNotifyUserInfo *user_info,
                                      const char *label, const char *value) {
  if (mock_notify_buffer) {
    g_string_append_printf(mock_notify_buffer, "%s: %s\n", label, value);
  }
}
void purple_notify_user_info_destroy(PurpleNotifyUserInfo *user_info) {}
void *purple_notify_uri(void *handle, const char *uri) { return NULL; }
void *purple_request_input(void *handle, const char *title, const char *primary,
                           const char *secondary, const char *default_value,
                           gboolean multiline, gboolean masked, gchar *hint,
                           const char *ok_text, GCallback ok_cb,
                           const char *cancel_text, GCallback cancel_cb,
                           PurpleAccount *account, const char *who,
                           PurpleConversation *conv, void *user_data) {
  return NULL;
}
void *purple_request_action(void *handle, const char *title,
                            const char *primary, const char *secondary,
                            int default_action, PurpleAccount *account,
                            const char *who, PurpleConversation *conv,
                            void *user_data, size_t action_count, ...) {
  return NULL;
}
PurpleRequestFields *purple_request_fields_new() { return NULL; }
PurpleRequestFieldGroup *purple_request_field_group_new(const char *title) {
  return NULL;
}
void purple_request_fields_add_group(PurpleRequestFields *fields,
                                     PurpleRequestFieldGroup *group) {}
void purple_request_field_group_add_field(PurpleRequestFieldGroup *group,
                                          PurpleRequestField *field) {}
PurpleRequestField *purple_request_field_string_new(const char *id,
                                                    const char *text,
                                                    const char *default_value,
                                                    gboolean multiline) {
  return NULL;
}
void purple_request_field_set_required(PurpleRequestField *field,
                                       gboolean required) {}
PurpleRequestField *purple_request_field_choice_new(const char *id,
                                                    const char *text,
                                                    int default_value) {
  return NULL;
}
void purple_request_field_choice_add(PurpleRequestField *field,
                                     const char *label) {}
void *purple_request_fields(void *handle, const char *title,
                            const char *primary, const char *secondary,
                            PurpleRequestFields *fields, const char *ok_text,
                            GCallback ok_cb, const char *cancel_text,
                            GCallback cancel_cb, PurpleAccount *account,
                            const char *who, PurpleConversation *conv,
                            void *user_data) {
  return NULL;
}
const char *purple_request_fields_get_string(const PurpleRequestFields *fields,
                                             const char *id) {
  return "test";
}
int purple_request_fields_get_choice(const PurpleRequestFields *fields,
                                     const char *id) {
  return 0;
}
void *purple_notify_message(void *handle, PurpleNotifyMsgType type,
                            const char *title, const char *primary,
                            const char *secondary, void (*cb)(void *),
                            void *user_data) {
  return NULL;
}

// Roomlist Mocks
PurpleRoomlist *purple_roomlist_new(PurpleAccount *account) { return NULL; }
void purple_roomlist_unref(PurpleRoomlist *list) {}
void purple_roomlist_set_fields(PurpleRoomlist *list, GList *fields) {}
PurpleRoomlistField *purple_roomlist_field_new(PurpleRoomlistFieldType type,
                                               const char *label,
                                               const char *name,
                                               gboolean hidden) {
  return NULL;
}
PurpleRoomlistRoom *purple_roomlist_room_new(PurpleRoomlistRoomType type,
                                             const char *name,
                                             PurpleRoomlistRoom *parent) {
  return NULL;
}
void purple_roomlist_room_add(PurpleRoomlist *list, PurpleRoomlistRoom *room) {}
void purple_roomlist_room_add_field(PurpleRoomlist *list,
                                    PurpleRoomlistRoom *room,
                                    gconstpointer field) {}

// Other Mocks
PurpleCmdId purple_cmd_register(const gchar *cmd, const gchar *args,
                                PurpleCmdPriority p, PurpleCmdFlag f,
                                const gchar *prpl_id, PurpleCmdFunc func,
                                const gchar *helpstr, void *data) {
  return 0;
}
PurpleStatusType *purple_status_type_new_with_attrs(
    PurpleStatusPrimitive type, const char *id, const char *name,
    gboolean saveable, gboolean user_settable, gboolean independent,
    const char *attr_id, const char *attr_name, PurpleValue *attr_value, ...) {
  return NULL;
}
PurpleStatusType *purple_status_type_new(PurpleStatusPrimitive type,
                                         const char *id, const char *name,
                                         gboolean saveable) {
  return NULL;
}
PurpleValue *purple_value_new(PurpleType type, ...) { return NULL; }
const char *purple_status_get_id(const PurpleStatus *status) {
  return "available";
}
const char *purple_status_get_attr_string(const PurpleStatus *status,
                                          const char *id) {
  return NULL;
}
void purple_prpl_got_user_status(PurpleAccount *account, const char *name,
                                 const char *id, ...) {}
gulong purple_signal_connect(void *instance, const char *signal, void *handle,
                             PurpleCallback func, void *data) {
  return 0;
}
void *purple_conversations_get_handle() { return NULL; }
const char *purple_user_dir() { return "/tmp"; }
PurpleMenuAction *purple_menu_action_new(const char *label,
                                         PurpleCallback callback, gpointer data,
                                         GList *children) {
  return NULL;
}
PurpleStatus *
purple_presence_get_active_status(const PurplePresence *presence) {
  return NULL;
}
void purple_buddy_icons_set_for_user(PurpleAccount *account, const char *who,
                                     void *icon_data, size_t icon_len,
                                     const char *checksum) {}
gconstpointer purple_imgstore_get_data(PurpleStoredImage *i) { return NULL; }
size_t purple_imgstore_get_size(PurpleStoredImage *i) { return 0; }
PurpleStoredImage *purple_imgstore_find_by_id(int id) { return NULL; }
PurpleConversation *serv_got_joined_chat(PurpleConnection *gc, int id,
                                         const char *name) {
  return NULL;
}
void serv_got_chat_in(PurpleConnection *g, int id, const char *who,
                      PurpleMessageFlags flags, const char *message,
                      time_t mtime) {}
void serv_got_chat_invite(PurpleConnection *gc, const char *name,
                          const char *who, const char *message,
                          GHashTable *data) {}
char *purple_markup_strip_html(const char *str) {
  // Basic mock implementation: remove anything between < and >
  if (!str)
    return NULL;
  GString *out = g_string_new("");
  gboolean in_tag = FALSE;
  for (const char *p = str; *p; p++) {
    if (*p == '<')
      in_tag = TRUE;
    else if (*p == '>')
      in_tag = FALSE;
    else if (!in_tag)
      g_string_append_c(out, *p);
  }
  return g_string_free(out, FALSE);
}
PurplePluginAction *
purple_plugin_action_new(const char *label,
                         void (*callback)(PurplePluginAction *)) {
  return NULL;
}

// INCLUDE PLUGIN SOURCE
#include "../plugin.c"

// --- Mocks for Rust FFI (using types from plugin.c) ---

void purple_matrix_rust_init() {}
void purple_matrix_rust_set_msg_callback(MsgCallback cb) {}
void purple_matrix_rust_set_typing_callback(TypingCallback cb) {}
void purple_matrix_rust_set_room_joined_callback(RoomJoinedCallback cb) {}
void purple_matrix_rust_set_update_buddy_callback(UpdateBuddyCallback cb) {}
void purple_matrix_rust_set_presence_callback(PresenceCallback cb) {}
void purple_matrix_rust_set_chat_topic_callback(ChatTopicCallback cb) {}
void purple_matrix_rust_set_chat_user_callback(ChatUserCallback cb) {}
void purple_matrix_rust_init_invite_cb(void (*cb)(const char *, const char *)) {
}
void purple_matrix_rust_init_verification_cbs(
    void (*req_cb)(const char *, const char *),
    void (*emoji_cb)(const char *, const char *, const char *),
    void (*qr_cb)(const char *, const char *)) {}
void purple_matrix_rust_init_sso_cb(void (*cb)(const char *)) {}
void purple_matrix_rust_init_connected_cb(void (*cb)()) {}
void purple_matrix_rust_init_login_failed_cb(void (*cb)(const char *)) {}
void purple_matrix_rust_set_read_marker_callback(ReadMarkerCallback cb) {}
void purple_matrix_rust_set_show_user_info_callback(
    void (*cb)(const char *, const char *, const char *, gboolean)) {}
void purple_matrix_rust_set_roomlist_add_callback(
    void (*cb)(const char *, const char *, const char *, guint64)) {}
int purple_matrix_rust_login(const char *user, const char *pass, const char *hs,
                             const char *data_dir) {
  return 1;
}
void purple_matrix_rust_send_message(const char *user_id, const char *room_id,
                                     const char *text) {
  printf("[Rust Mock] send_message: %s -> %s\n", room_id, text);
}
void purple_matrix_rust_send_typing(const char *user_id, const char *room_id,
                                    gboolean is_typing) {}
void purple_matrix_rust_join_room(const char *user_id, const char *room_id) {}
void purple_matrix_rust_leave_room(const char *user_id, const char *room_id) {}
void purple_matrix_rust_invite_user(const char *user_id, const char *room_id,
                                    const char *invitee_id) {}
void purple_matrix_rust_finish_sso(const char *token) {}
void purple_matrix_rust_set_display_name(const char *user_id,
                                         const char *name) {}
void purple_matrix_rust_set_avatar(const char *user_id, const char *path) {}
void purple_matrix_rust_bootstrap_cross_signing(const char *user_id) {}
void purple_matrix_rust_e2ee_status(const char *user_id, const char *room_id) {}
void purple_matrix_rust_verify_user(const char *user_id,
                                    const char *other_user_id) {}
void purple_matrix_rust_recover_keys(const char *user_id,
                                     const char *passphrase) {}
void purple_matrix_rust_logout(const char *user_id) {}
void purple_matrix_rust_send_location(const char *user_id, const char *room_id,
                                      const char *body, const char *geo_uri) {}
void purple_matrix_rust_poll_create(const char *user_id, const char *room_id,
                                    const char *question, const char *options) {
}
void purple_matrix_rust_poll_vote(const char *user_id, const char *room_id,
                                  const char *poll_event_id,
                                  const char *option_text,
                                  const char *selection_index_str) {}
void purple_matrix_rust_poll_end(const char *user_id, const char *room_id,
                                 const char *poll_event_id) {}
void purple_matrix_rust_change_password(const char *user_id, const char *old_pw,
                                        const char *new_pw) {}
void purple_matrix_rust_add_buddy(const char *user_id,
                                  const char *other_user_id) {}
void purple_matrix_rust_remove_buddy(const char *user_id,
                                     const char *other_user_id) {}
void purple_matrix_rust_set_idle(const char *user_id, int seconds) {}
void purple_matrix_rust_send_sticker(const char *user_id, const char *room_id,
                                     const char *url) {}
void purple_matrix_rust_unignore_user(const char *user_id,
                                      const char *other_user_id) {}
void purple_matrix_rust_set_avatar_bytes(const char *user_id,
                                         const unsigned char *data,
                                         size_t len) {}
void purple_matrix_rust_deactivate_account(bool erase_data) {}
void purple_matrix_rust_fetch_public_rooms_for_list(const char *user_id) {}
void purple_matrix_rust_send_read_receipt(const char *user_id,
                                          const char *room_id,
                                          const char *event_id) {}
void purple_matrix_rust_send_reaction(const char *user_id, const char *room_id,
                                      const char *event_id, const char *key) {}
void purple_matrix_rust_redact_event(const char *user_id, const char *room_id,
                                     const char *event_id, const char *reason) {
}
void purple_matrix_rust_fetch_more_history(const char *user_id,
                                           const char *room_id) {}
void purple_matrix_rust_create_room(const char *user_id, const char *name,
                                    const char *topic, bool is_public) {}
void purple_matrix_rust_search_public_rooms(const char *user_id,
                                            const char *search_term,
                                            const char *output_room_id) {}
void purple_matrix_rust_search_users(const char *user_id,
                                     const char *search_term,
                                     const char *room_id) {}
void purple_matrix_rust_kick_user(const char *user_id, const char *room_id,
                                  const char *target_id, const char *reason) {}
void purple_matrix_rust_ban_user(const char *user_id, const char *room_id,
                                 const char *target_id, const char *reason) {}
void purple_matrix_rust_set_room_tag(const char *user_id, const char *room_id,
                                     const char *tag_name) {}
void purple_matrix_rust_ignore_user(const char *user_id,
                                    const char *target_id) {}
void purple_matrix_rust_get_power_levels(const char *user_id,
                                         const char *room_id) {}
void purple_matrix_rust_set_power_level(const char *user_id,
                                        const char *room_id,
                                        const char *target_id,
                                        long long level) {}
void purple_matrix_rust_set_room_name(const char *user_id, const char *room_id,
                                      const char *name) {}
void purple_matrix_rust_set_room_topic(const char *user_id, const char *room_id,
                                       const char *topic) {}
void purple_matrix_rust_create_alias(const char *user_id, const char *room_id,
                                     const char *alias) {}
void purple_matrix_rust_delete_alias(const char *user_id, const char *alias) {}
void purple_matrix_rust_report_content(const char *user_id, const char *room_id,
                                       const char *event_id,
                                       const char *reason) {}
void purple_matrix_rust_export_room_keys(const char *user_id, const char *path,
                                         const char *passphrase) {}
void purple_matrix_rust_bulk_redact(const char *user_id, const char *room_id,
                                    int count, const char *reason) {}
void purple_matrix_rust_knock(const char *user_id, const char *room_id_or_alias,
                              const char *reason) {}
void purple_matrix_rust_unban_user(const char *user_id, const char *room_id,
                                   const char *target_id, const char *reason) {}
void purple_matrix_rust_set_room_avatar(const char *user_id,
                                        const char *room_id,
                                        const char *filename) {}
void purple_matrix_rust_set_room_mute_state(const char *user_id,
                                            const char *room_id, bool muted) {}
void purple_matrix_rust_destroy_session(const char *user_id) {}
void purple_matrix_rust_send_file(const char *user_id, const char *room_id,
                                  const char *filename) {}
void purple_matrix_rust_send_reply(const char *user_id, const char *room_id,
                                   const char *event_id, const char *text) {}
void purple_matrix_rust_send_edit(const char *user_id, const char *room_id,
                                  const char *event_id, const char *text) {}
void purple_matrix_rust_get_user_info(const char *user_id,
                                      const char *target_id) {}
void purple_matrix_rust_set_status(const char *user_id, MatrixStatus status,
                                   const char *msg) {}
void purple_matrix_rust_send_im(const char *user_id, const char *target_id,
                                const char *text) {}
void purple_matrix_rust_fetch_room_members(const char *user_id,
                                           const char *room_id) {}
void purple_matrix_rust_fetch_history(const char *user_id,
                                      const char *room_id) {}
void purple_matrix_rust_accept_sas(const char *user_id, const char *target_id,
                                   const char *flow_id) {}
void purple_matrix_rust_confirm_sas(const char *user_id, const char *target_id,
                                    const char *flow_id, bool match) {}

// --- Tests ---

void test_matrix_get_chat_name() {
  GHashTable *comp = g_hash_table_new(g_str_hash, g_str_equal);
  g_hash_table_insert(comp, "room_id", "123");

  char *name = matrix_get_chat_name(comp);
  g_assert_cmpstr(name, ==, "123");
  g_free(name);
  g_hash_table_destroy(comp);
}

void test_matrix_chat_info_defaults() {
  GHashTable *defaults = matrix_chat_info_defaults(NULL, "test_room");
  g_assert_nonnull(defaults);
  g_assert_cmpstr(g_hash_table_lookup(defaults, "room_id"), ==, "test_room");
  g_hash_table_destroy(defaults);
}

void test_matrix_tooltip_text_encrypted() {
  PurpleChat *chat = purple_chat_new(NULL, "Room 1", NULL);
  GHashTable *comp = purple_chat_get_components(chat);
  g_hash_table_insert(comp, g_strdup("room_id"), g_strdup("!abc:matrix.org"));
  g_hash_table_insert(comp, g_strdup("topic"), g_strdup("Secure Room"));
  g_hash_table_insert(comp, g_strdup("encrypted"), g_strdup("1"));

  mock_notify_buffer = g_string_new("");

  // We cast PurpleChat* to PurpleBuddy* because the function signature (and
  // caller) uses PurpleBuddy* generically for blist nodes, but checks type
  // internally.
  matrix_tooltip_text((PurpleBuddy *)chat, NULL, TRUE);

  // Verify output
  g_assert_true(strstr(mock_notify_buffer->str, "Room ID: !abc:matrix.org") !=
                NULL);
  g_assert_true(strstr(mock_notify_buffer->str, "Topic: Secure Room") != NULL);
  g_assert_true(strstr(mock_notify_buffer->str,
                       "Security: End-to-End Encrypted") != NULL);

  g_string_free(mock_notify_buffer, TRUE);
  mock_notify_buffer = NULL;
}

void test_matrix_list_emblem_encrypted() {
  PurpleChat *chat = purple_chat_new(NULL, "Room 1", NULL);
  GHashTable *comp = purple_chat_get_components(chat);
  g_hash_table_insert(comp, g_strdup("encrypted"), g_strdup("1"));

  const char *emblem = matrix_list_emblem((PurpleBuddy *)chat);
  g_assert_cmpstr(emblem, ==, "secure");
}

void test_matrix_status_text_topic() {
  PurpleChat *chat = purple_chat_new(NULL, "Room 1", NULL);
  GHashTable *comp = purple_chat_get_components(chat);
  g_hash_table_insert(comp, g_strdup("topic"), g_strdup("<b>Bold Topic</b>"));

  char *status = matrix_status_text((PurpleBuddy *)chat);
  g_assert_cmpstr(status, ==, "Bold Topic"); // HTML stripped
  g_free(status);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/matrix/get_chat_name", test_matrix_get_chat_name);
  g_test_add_func("/matrix/chat_info_defaults", test_matrix_chat_info_defaults);
  g_test_add_func("/matrix/ui/tooltip_encrypted",
                  test_matrix_tooltip_text_encrypted);
  g_test_add_func("/matrix/ui/emblem_encrypted",
                  test_matrix_list_emblem_encrypted);
  g_test_add_func("/matrix/ui/status_text_topic",
                  test_matrix_status_text_topic);
  return g_test_run();
}
// Additional Libpurple Mocks
PurpleRequestField *
purple_request_fields_get_field(const PurpleRequestFields *fields,
                                const char *id) {
  return NULL;
}
int purple_request_field_choice_get_value(const PurpleRequestField *field) {
  return 0;
}
void *purple_notify_formatted(void *handle, const char *title,
                              const char *primary, const char *secondary,
                              const char *text, PurpleNotifyCloseCallback cb,
                              gpointer user_data) {
  return NULL;
}
void purple_request_field_set_help_string(PurpleRequestField *field,
                                          const char *hint) {}

// Additional Rust FFI Mocks
void purple_matrix_rust_enable_key_backup(const char *user_id) {}
void purple_matrix_rust_restore_from_backup(const char *user_id,
                                            const char *recovery_key) {}
void purple_matrix_rust_list_sticker_packs(
    const char *user_id, void (*cb)(const char *, const char *, void *),
    void (*done_cb)(void *), void *user_data) {}
void purple_matrix_rust_list_stickers_in_pack(
    const char *user_id, const char *pack_id,
    void (*cb)(const char *, const char *, const char *, void *),
    void (*done_cb)(void *), void *user_data) {}
void purple_matrix_rust_list_threads(const char *user_id, const char *room_id) {
}
void purple_matrix_rust_get_active_polls(const char *user_id,
                                         const char *room_id) {}
void purple_matrix_rust_set_thread_list_callback(
    void (*cb)(const char *room_id, const char *thread_root_id,
               const char *latest_msg, guint64 count, guint64 ts)) {}
void purple_matrix_rust_set_poll_list_callback(
    void (*cb)(const char *room_id, const char *event_id, const char *sender,
               const char *question, const char *options_str)) {}
