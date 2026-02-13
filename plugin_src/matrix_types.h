#ifndef MATRIX_TYPES_H
#define MATRIX_TYPES_H

#include <glib.h>
#include <stdbool.h>

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

typedef struct {
  char *room_id;
  char *name;
  char *group_name;
  char *avatar_url;
  char *topic;
  gboolean encrypted;
} MatrixRoomData;

typedef struct {
  char *user_id;
  char *display_name;
  char *avatar_url;
  gboolean is_online;
} MatrixUserInfoData;

typedef struct {
  char *name;
  char *id;
  char *topic;
  guint64 count;
} RoomListData;

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

typedef struct {
  char *room_id;
  char *event_id;
  char *sender;
  char *question;
  char *options_str;
} PollListItem;

typedef struct {
  char *room_id;
  char *event_id;
  char *options_str;
} PollVoteContext;

typedef struct {
  char *room_id;
  GList *polls;
} PollDisplayContext;

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
  char *room_id;
  char *inviter;
} MatrixInviteData;

typedef struct {
  char *room_id;
  char *event_id;
  char *user_id;
} MatrixReadMarkerData;

typedef struct {
  char *user_id;
  char *alias;
  char *icon_path;
} MatrixBuddyUpdateData;

typedef struct {
  char *user_id;
  bool is_online;
} MatrixPresenceData;

typedef struct {
  char *room_id;
  char *topic;
  char *sender;
} MatrixTopicData;

typedef struct {
  char *room_id;
  char *user_id;
  bool add;
  char *alias;
  char *avatar_path;
} MatrixChatUserData;

typedef struct {
  char *user_id;
  char *html_data;
} MatrixQrData;

typedef struct {
  char *output_room_id;
  PurpleAccount *account;
} PreviewRoomContext;

typedef struct {
  char *room_id_or_alias;
  char *html_body;
} MatrixRoomPreviewData;

#endif // MATRIX_TYPES_H
