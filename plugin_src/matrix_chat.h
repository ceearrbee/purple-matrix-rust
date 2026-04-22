#ifndef MATRIX_CHAT_H
#define MATRIX_CHAT_H

#include <libpurple/prpl.h>
#include <stdbool.h>

int matrix_send_im(PurpleConnection *gc, const char *who, const char *message, PurpleMessageFlags flags);
int matrix_send_chat(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
unsigned int matrix_send_typing(PurpleConnection *gc, const char *name, PurpleTypingState state);

// Callbacks from Rust
void msg_callback(const char *user_id, const char *sender, const char *sender_id, const char *msg, const char *room_id, const char *thread_root_id, const char *event_id, guint64 timestamp, bool encrypted, bool is_system, bool is_direct);
void reactions_changed_callback(const char *user_id, const char *room_id,
                                 const char *event_id,
                                 const char *reactions_text);
void message_edited_callback(const char *user_id, const char *room_id,
                              const char *event_id, const char *new_msg);

void message_redacted_callback(const char *user_id, const char *room_id,
                                const char *event_id);

void media_downloaded_callback(const char *user_id, const char *room_id,
                                const char *event_id, const unsigned char *data,
                                size_t size, const char *content_type);void typing_callback(const char *user_id, const char *room_id, const char *who, bool is_typing);
void read_marker_cb(const char *user_id, const char *room_id, const char *event_id, const char *who);
void read_receipt_cb(const char *user_id, const char *room_id, const char *who, const char *event_id);
void poll_creation_callback(const char *user_id, const char *room_id);

GList *matrix_chat_info(PurpleConnection *gc);
GHashTable *matrix_chat_info_defaults(PurpleConnection *gc, const char *chat_name);
void matrix_join_chat(PurpleConnection *gc, GHashTable *data);
void matrix_reject_chat(PurpleConnection *gc, GHashTable *data);
void matrix_chat_invite(PurpleConnection *gc, int id, const char *message, const char *name);
void matrix_chat_leave(PurpleConnection *gc, int id);
void matrix_chat_whisper(PurpleConnection *gc, int id, const char *who, const char *message);
int matrix_chat_send(PurpleConnection *gc, int id, const char *message, PurpleMessageFlags flags);
void matrix_set_chat_topic(PurpleConnection *gc, int id, const char *topic);
void matrix_send_file(PurpleConnection *gc, const char *who, const char *filename);

#endif // MATRIX_CHAT_H
