#ifndef MATRIX_FFI_WRAPPERS_H
#define MATRIX_FFI_WRAPPERS_H

#include <glib.h>
#include <stdbool.h>

// Initialize the Rust backend
extern void purple_matrix_rust_init(void);

// Account management
extern void purple_matrix_rust_login(const char *user_id, const char *password,
                                     const char *homeserver,
                                     const char *data_dir);
extern void purple_matrix_rust_logout(const char *user_id);
extern void purple_matrix_rust_finish_sso(const char *token);
extern void purple_matrix_rust_destroy_session(const char *user_id);

typedef enum {
  FFI_EVENT_MESSAGE_RECEIVED = 1,
  FFI_EVENT_TYPING = 2,
  FFI_EVENT_ROOM_JOINED = 3,
  FFI_EVENT_ROOM_LEFT = 4,
  FFI_EVENT_READ_MARKER = 5,
  FFI_EVENT_PRESENCE = 6,
  FFI_EVENT_CHAT_TOPIC = 7,
  FFI_EVENT_CHAT_USER = 8,
  FFI_EVENT_INVITE = 9,
  FFI_EVENT_ROOM_LIST_ADD = 10,
  FFI_EVENT_ROOM_PREVIEW = 11,
  FFI_EVENT_LOGIN_FAILED = 12,
  FFI_EVENT_SHOW_USER_INFO = 13,
  FFI_EVENT_THREAD_LIST = 14,
  FFI_EVENT_POLL_LIST = 15,
  FFI_EVENT_SEARCH = 16,
  FFI_EVENT_ROOM_MUTE = 17,
  FFI_EVENT_ROOM_TAG = 18,
  FFI_EVENT_UPDATE_BUDDY = 19,
  FFI_EVENT_STICKER_PACK = 21,
  FFI_EVENT_STICKER = 22,
  FFI_EVENT_STICKER_DONE = 23,
  FFI_EVENT_SSO = 24,
  FFI_EVENT_CONNECTED = 25,
  FFI_EVENT_SAS_REQUEST = 26,
  FFI_EVENT_SAS_HAVE_EMOJI = 27,
  FFI_EVENT_SHOW_VERIFICATION_QR = 28,
  FFI_EVENT_POWER_LEVEL_UPDATE = 29,
} FfiEventType;

typedef struct {
  char *user_id;
  char *sender;
  char *msg;
  char *room_id;
  char *thread_root_id;
  char *event_id;
  guint64 timestamp;
  bool encrypted;
} CMessageReceived;
typedef struct {
  char *user_id;
  char *room_id;
  char *who;
  bool is_typing;
} CTyping;
typedef struct {
  char *user_id;
  char *room_id;
  char *name;
  char *group_name;
  char *avatar_url;
  char *topic;
  bool encrypted;
  guint64 member_count;
} CRoomJoined;
typedef struct {
  char *user_id;
  char *room_id;
} CRoomLeft;
typedef struct {
  char *user_id;
  char *room_id;
  char *event_id;
  char *who;
} CReadMarker;
typedef struct {
  char *user_id;
  char *target_user_id;
  bool is_online;
} CPresence;
typedef struct {
  char *user_id;
  char *room_id;
  char *topic;
  char *sender;
} CChatTopic;
typedef struct {
  char *user_id;
  char *room_id;
  char *member_id;
  bool add;
  char *alias;
  char *avatar_path;
} CChatUser;
typedef struct {
  char *user_id;
  char *room_id;
  char *inviter;
} CInvite;
typedef struct {
  char *user_id;
  char *room_id;
  char *name;
  char *topic;
  guint64 member_count;
} CRoomListAdd;
typedef struct {
  char *user_id;
  char *room_id;
  bool is_admin;
  bool can_kick;
  bool can_ban;
  bool can_redact;
  bool can_invite;
} CPowerLevelUpdate;
typedef struct {
  char *user_id;
  char *room_id_or_alias;
  char *html_body;
} CRoomPreview;
typedef struct {
  char *user_id;
  char *message;
} CLoginFailed;
typedef struct {
  char *user_id;
  char *display_name;
  char *avatar_url;
  char *target_user_id;
  bool is_online;
} CShowUserInfo;
typedef struct {
  char *user_id;
  char *room_id;
  char *thread_root_id;
  char *latest_msg;
  guint64 count;
  guint64 ts;
} CThreadList;
typedef struct {
  char *user_id;
  char *room_id;
  char *event_id;
  char *question;
  char *sender;
  char *options_str;
} CPollList;
typedef struct {
  char *user_id;
  char *room_id;
  char *sender;
  char *message;
  char *timestamp_str;
} CSearch;
typedef struct {
  char *user_id;
  char *room_id;
  bool muted;
} CRoomMute;
typedef struct {
  char *user_id;
  char *room_id;
  char *tag;
} CRoomTag;
typedef struct {
  char *user_id;
  char *alias;
  char *avatar_url;
} CUpdateBuddy;
typedef struct {
  size_t cb_ptr;
  char *user_id;
  char *pack_id;
  char *pack_name;
  size_t user_data;
} CStickerPack;
typedef struct {
  size_t cb_ptr;
  char *user_id;
  char *pack_id;
  char *sticker_id;
  char *url;
  char *description;
  size_t user_data;
} CSticker;
typedef struct {
  size_t cb_ptr;
  size_t user_data;
} CStickerDone;
typedef struct {
  char *url;
} CSso;
typedef struct {
  char *user_id;
} CConnected;
typedef struct {
  char *user_id;
  char *target_user_id;
  char *flow_id;
} CSasRequest;
typedef struct {
  char *user_id;
  char *target_user_id;
  char *flow_id;
  char *emojis;
} CSasHaveEmoji;
typedef struct {
  char *user_id;
  char *target_user_id;
  char *html_data;
} CShowVerificationQr;

// Polling
extern bool purple_matrix_rust_poll_event(int *out_type, void **out_data);
extern void purple_matrix_rust_free_event(int ev_type, void *data);

extern void purple_matrix_rust_set_imgstore_add_callback(
    int (*cb)(const void *data, size_t size));

// Actions
extern void purple_matrix_rust_send_message(const char *user_id,
                                            const char *room_id,
                                            const char *text);
extern void purple_matrix_rust_send_im(const char *account_user_id,
                                       const char *target_user_id,
                                       const char *text);
extern void purple_matrix_rust_send_typing(const char *user_id,
                                           const char *room_id_or_user_id,
                                           bool is_typing);
extern void purple_matrix_rust_fetch_room_members(const char *user_id,
                                                  const char *room_id);
extern void purple_matrix_rust_join_room(const char *user_id,
                                         const char *room_id);
extern void purple_matrix_rust_leave_room(const char *user_id,
                                          const char *room_id);
extern void purple_matrix_rust_fetch_history(const char *user_id,
                                             const char *room_id);
extern void purple_matrix_rust_fetch_more_history(const char *user_id,
                                                  const char *room_id);
extern void purple_matrix_rust_resync_recent_history(const char *user_id,
                                                     const char *room_id);
extern void purple_matrix_rust_send_read_receipt(const char *user_id,
                                                 const char *room_id,
                                                 const char *event_id);
extern void purple_matrix_rust_invite_user(const char *account_user_id,
                                           const char *room_id,
                                           const char *user_id);
extern void purple_matrix_rust_create_room(const char *user_id,
                                           const char *name, const char *topic,
                                           bool is_public);
extern void purple_matrix_rust_fetch_public_rooms_for_list(const char *user_id);
extern void purple_matrix_rust_fetch_room_preview(const char *user_id,
                                                  const char *room_id_or_alias);
extern void purple_matrix_rust_send_file(const char *user_id, const char *who,
                                         const char *filename);
extern void purple_matrix_rust_send_reply(const char *user_id,
                                          const char *room_id,
                                          const char *event_id,
                                          const char *text);
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
                                         const char *text);
extern void purple_matrix_rust_send_location(const char *user_id,
                                             const char *room_id, double lat,
                                             double lon);
extern void purple_matrix_rust_send_sticker(const char *user_id,
                                            const char *room_id,
                                            const char *url);
extern void purple_matrix_rust_poll_vote(const char *user_id,
                                         const char *room_id,
                                         const char *event_id,
                                         guint64 option_index);
extern void purple_matrix_rust_list_threads(const char *user_id,
                                            const char *room_id);
extern void purple_matrix_rust_list_polls(const char *user_id,
                                          const char *room_id);
extern void purple_matrix_rust_poll_end(const char *user_id,
                                        const char *room_id,
                                        const char *poll_id, const char *text);
extern void purple_matrix_rust_search_room_messages(const char *user_id,
                                                    const char *room_id,
                                                    const char *term);
extern void purple_matrix_rust_bulk_redact(const char *user_id,
                                           const char *room_id, int count,
                                           const char *target_user);
extern void purple_matrix_rust_add_buddy(const char *user_id,
                                         const char *target_id);
extern void purple_matrix_rust_remove_buddy(const char *user_id,
                                            const char *target_id);
extern void purple_matrix_rust_set_room_topic(const char *user_id,
                                              const char *room_id,
                                              const char *topic);
extern void purple_matrix_rust_set_room_mute_state(const char *user_id,
                                                   const char *room_id,
                                                   bool muted);
extern void purple_matrix_rust_set_status(const char *user_id, int status,
                                          const char *msg);
extern void purple_matrix_rust_set_idle(const char *user_id, int time);
extern void purple_matrix_rust_change_password(const char *user_id,
                                               const char *old,
                                               const char *new);
extern void purple_matrix_rust_ignore_user(const char *user_id,
                                           const char *who);
extern void purple_matrix_rust_unignore_user(const char *user_id,
                                             const char *who);
extern void purple_matrix_rust_set_display_name(const char *user_id,
                                                const char *name);
extern void purple_matrix_rust_set_avatar(const char *user_id,
                                          const char *path);
extern void purple_matrix_rust_set_avatar_bytes(const char *user_id,
                                                const unsigned char *data,
                                                size_t size);
extern void purple_matrix_rust_get_user_info(const char *user_id,
                                             const char *who);
extern void purple_matrix_rust_get_my_profile(const char *user_id);
extern void purple_matrix_rust_accept_sas(const char *user_id,
                                          const char *target_id,
                                          const char *flow_id);
extern void purple_matrix_rust_confirm_sas(const char *user_id,
                                           const char *target_id,
                                           const char *flow_id, bool match);

// Stickers and Poll management
extern void purple_matrix_rust_list_sticker_packs(
    const char *user_id,
    void (*cb)(const char *user_id, const char *id, const char *name,
               void *u_data),
    void (*done_cb)(void *u_data), void *user_data);
extern void purple_matrix_rust_list_stickers_in_pack(
    const char *user_id, const char *pack_id,
    void (*cb)(const char *user_id, const char *id, const char *name,
               const char *url, void *u_data),
    void (*done_cb)(void *u_data), void *user_data);
extern void purple_matrix_rust_search_stickers(const char *user_id,
                                               const char *term);
extern void purple_matrix_rust_poll_create(const char *user_id,
                                           const char *room_id,
                                           const char *question,
                                           const char *options);
extern void purple_matrix_rust_mark_unread(const char *user_id,
                                           const char *room_id, bool unread);
extern bool purple_matrix_rust_e2ee_status(const char *user_id,
                                           const char *room_id);
extern void purple_matrix_rust_get_power_levels(const char *user_id,
                                                const char *room_id);
extern void purple_matrix_rust_set_room_name(const char *user_id,
                                             const char *room_id,
                                             const char *name);
extern void purple_matrix_rust_who_read(const char *user_id,
                                        const char *room_id);
extern void purple_matrix_rust_kick_user(const char *user_id,
                                         const char *room_id,
                                         const char *target_user_id,
                                         const char *reason);
extern void purple_matrix_rust_ban_user(const char *user_id,
                                        const char *room_id,
                                        const char *target_user_id,
                                        const char *reason);
extern void purple_matrix_rust_unban_user(const char *user_id,
                                          const char *room_id,
                                          const char *target_user_id,
                                          const char *reason);
extern void purple_matrix_rust_set_room_avatar(const char *user_id,
                                               const char *room_id,
                                               const char *path);
extern void purple_matrix_rust_set_room_join_rule(const char *user_id,
                                                  const char *room_id,
                                                  const char *rule);
extern void purple_matrix_rust_set_room_history_visibility(
    const char *user_id, const char *room_id, const char *visibility);
extern void purple_matrix_rust_set_room_guest_access(const char *user_id,
                                                     const char *room_id,
                                                     bool allow);
extern void purple_matrix_rust_upgrade_room(const char *user_id,
                                            const char *room_id,
                                            const char *new_version);
extern void purple_matrix_rust_knock(const char *user_id,
                                     const char *room_id_or_alias,
                                     const char *reason);
extern void purple_matrix_rust_set_room_tag(const char *user_id,
                                            const char *room_id,
                                            const char *tag);
extern void purple_matrix_rust_report_content(const char *user_id,
                                              const char *room_id,
                                              const char *event_id,
                                              const char *reason);
extern void purple_matrix_rust_get_space_hierarchy(const char *user_id,
                                                   const char *space_id);
extern void purple_matrix_rust_search_room_members(const char *user_id,
                                                   const char *room_id,
                                                   const char *term);
extern void purple_matrix_rust_get_supported_versions(const char *user_id);
extern void purple_matrix_rust_get_server_info(const char *user_id);
extern void purple_matrix_rust_search_public_rooms(const char *user_id,
                                                   const char *search_term,
                                                   const char *output_room_id);
extern void purple_matrix_rust_search_users(const char *user_id,
                                            const char *search_term,
                                            const char *room_id);
extern void purple_matrix_rust_create_alias(const char *user_id,
                                            const char *room_id,
                                            const char *alias_localpart);
extern void purple_matrix_rust_delete_alias(const char *user_id,
                                            const char *alias);

// Crypto management
extern void purple_matrix_rust_recover_keys(const char *user_id,
                                            const char *passphrase);
extern void purple_matrix_rust_restore_backup(const char *user_id,
                                              const char *recovery_key);
extern void purple_matrix_rust_resync_room_keys(const char *user_id,
                                                const char *room_id);
extern void purple_matrix_rust_export_keys(const char *user_id,
                                           const char *path,
                                           const char *passphrase);
extern void purple_matrix_rust_bootstrap_cross_signing(const char *user_id);
extern void
purple_matrix_rust_bootstrap_cross_signing_with_password(const char *user_id,
                                                         const char *password);
extern void purple_matrix_rust_verify_user(const char *user_id,
                                           const char *target_user_id);
extern void purple_matrix_rust_list_own_devices(const char *user_id);
extern void purple_matrix_rust_debug_crypto_status(const char *user_id);
extern void purple_matrix_rust_enable_key_backup(const char *user_id);
extern void purple_matrix_rust_ui_show_dashboard(const char *room_id);

#endif // MATRIX_FFI_WRAPPERS_H
