#ifndef MATRIX_TYPES_H
#define MATRIX_TYPES_H

#include <glib.h>
#include <stdbool.h>
#include <stdint.h>

// FfiEventType enum matching src/ffi/events.rs
typedef enum {
    FFI_EVENT_MESSAGE_RECEIVED = 1,
    FFI_EVENT_TYPING_STATE = 2,
    FFI_EVENT_ROOM_JOINED = 3,
    FFI_EVENT_ROOM_LEFT = 4,
    FFI_EVENT_READ_MARKER = 5,
    FFI_EVENT_PRESENCE_UPDATE = 6,
    FFI_EVENT_CHAT_TOPIC_UPDATE = 7,
    FFI_EVENT_CHAT_MEMBER_UPDATE = 8,
    FFI_EVENT_INVITE_RECEIVED = 9,
    FFI_EVENT_ROOM_LIST_ADDED = 10,
    FFI_EVENT_ROOM_PREVIEW = 11,
    FFI_EVENT_LOGIN_FAILED = 12,
    FFI_EVENT_USER_INFO_RECEIVED = 13,
    FFI_EVENT_THREAD_LIST_RECEIVED = 14,
    FFI_EVENT_POLL_LIST_RECEIVED = 15,
    FFI_EVENT_SEARCH_RESULT_RECEIVED = 16,
    FFI_EVENT_ROOM_MUTE_UPDATE = 17,
    FFI_EVENT_ROOM_TAG_UPDATE = 18,
    FFI_EVENT_BUDDY_UPDATE = 19,
    FFI_EVENT_SSO_URL_RECEIVED = 20,
    FFI_EVENT_CONNECTED = 21,
    FFI_EVENT_VERIFICATION_REQUEST = 22,
    FFI_EVENT_VERIFICATION_EMOJI = 23,
    FFI_EVENT_VERIFICATION_QR = 24,
    FFI_EVENT_MESSAGE_EDITED = 25,
    FFI_EVENT_REACTION_UPDATE = 26,
    FFI_EVENT_RECEIPT_RECEIVED = 27,
    FFI_EVENT_POWER_LEVEL_UPDATE = 28,
    FFI_EVENT_MESSAGE_REDACTED = 29,
    FFI_EVENT_MEDIA_DOWNLOADED = 30,
    FFI_EVENT_ROOM_DASHBOARD_INFO = 31,
} FfiEventType;

// Structs matching src/ffi/events.rs exactly for FFI safety

typedef struct {
    char *user_id;
    char *sender;
    char *msg;
    char *room_id;
    char *thread_root_id;
    char *event_id;
    uint64_t timestamp;
    bool encrypted;
} MessageEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *event_id;
    char *new_msg;
} MessageEditedData;

typedef struct {
    char *user_id;
    char *room_id;
    char *who;
    bool is_typing;
} TypingEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *name;
    char *group_name;
    char *avatar_url;
    char *topic;
    bool encrypted;
    uint64_t member_count;
} RoomJoinedEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *topic;
    char *sender;
} ChatTopicEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *member_id;
    bool add;
    char *alias;
    char *avatar_path;
} ChatUserEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *inviter;
} InviteEventData;

typedef struct {
    char *user_id;
    char *alias;
    char *avatar_url;
} UpdateBuddyEventData;

typedef struct {
    char *error_msg;
} LoginFailedEventData;

typedef struct {
    char *url;
} SsoUrlEventData;

typedef struct {
    char *user_id;
} ConnectedEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *event_id;
    char *who;
} ReadMarkerEventData;

typedef struct {
    char *user_id;
    char *room_id;
    bool is_admin;
    bool can_kick;
    bool can_ban;
    bool can_redact;
    bool can_invite;
} PowerLevelUpdateEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *event_id;
    char *reactions_text;
} ReactionsChangedEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *event_id;
} MessageRedactedEventData;

typedef struct {
    char *user_id;
    char *target_user_id;
    int status;
    char *status_msg;
} PresenceEventData;

typedef struct {
    char *user_id;
    char *target_user_id;
    char *flow_id;
} SasRequestEventData;

typedef struct {
    char *user_id;
    char *target_user_id;
    char *flow_id;
    char *emojis;
} SasHaveEmojiEventData;

typedef struct {
    char *user_id;
    char *target_user_id;
    char *html_data;
} ShowVerificationQrEventData;

typedef struct {
    char *user_id;
    char *target_user_id;
    char *display_name;
    char *avatar_url;
} ShowUserInfoEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *event_id;
    char *question;
    char *sender;
    char *options_str;
} PollListEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *thread_root_id;
    char *latest_msg;
} ThreadListEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *name;
    char *topic;
    char *parent_id;
} RoomListAddEventData;

typedef struct {
    char *user_id;
    char *room_id_or_alias;
    char *html_body;
} RoomPreviewEventData;

typedef struct {
    char *user_id;
    char *room_id;
} RoomLeftEventData;

typedef struct {
    char *user_id;
    char *room_id;
} PollCreationRequestedEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *event_id;
    unsigned char *media_data;
    size_t media_size;
    char *content_type;
} MediaDownloadedEventData;

typedef struct {
    char *user_id;
    char *room_id;
    char *name;
    char *topic;
    uint64_t member_count;
    bool encrypted;
    int64_t power_level;
    char *alias;
} RoomDashboardInfoEventData;

#endif // MATRIX_TYPES_H
