#ifndef MATRIX_CHAT_H
#define MATRIX_CHAT_H

#include <libpurple/conversation.h>
#include <libpurple/plugin.h>
#include <glib.h>
#include <stdbool.h>

int matrix_send_im(PurpleConnection *gc, const char *who, const char *message, PurpleMessageFlags flags);
int matrix_send_chat(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags); // Note: renamed to match logic
unsigned int matrix_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState state);
void msg_callback(const char *sender, const char *msg, const char *room_id, const char *thread_root_id, const char *event_id, guint64 timestamp);
void typing_callback(const char *room_id, const char *user_id, bool is_typing);
void read_marker_cb(const char *room_id, const char *event_id, const char *user_id);
void register_chat_commands(void);

// PRPL Ops
GList *matrix_chat_info(PurpleConnection *gc);
GHashTable *matrix_chat_info_defaults(PurpleConnection *gc, const char *chat_name);
void matrix_join_chat(PurpleConnection *gc, GHashTable *data);
void matrix_chat_invite(PurpleConnection *gc, int id, const char *message, const char *name);
void matrix_chat_leave(PurpleConnection *gc, int id);
void matrix_chat_whisper(PurpleConnection *gc, int id, const char *who, const char *message);
int matrix_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags); // Duplicate of matrix_send_chat? Yes.
void matrix_set_chat_topic(PurpleConnection *gc, int id, const char *topic);
void matrix_send_file(PurpleConnection *gc, const char *who, const char *filename);

#endif // MATRIX_CHAT_H
