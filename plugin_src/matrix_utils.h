#ifndef MATRIX_UTILS_H
#define MATRIX_UTILS_H

#include <glib.h>
#include <libpurple/account.h>
#include <libpurple/request.h>
#include <stdbool.h>

int get_chat_id(const char *room_id);
gboolean is_virtual_room_id(const char *room_id);
char *derive_base_group_from_threads_group(const char *group_name);
char *sanitize_markup_text(const char *input);
void matrix_request_field_set_help_string(PurpleRequestField *field, const char *hint);
PurpleAccount *find_matrix_account(void);
char *matrix_get_chat_name(GHashTable *components);
guint32 get_history_page_size(PurpleAccount *account);

// Callback Declarations
void show_user_info_cb(const char *user_id, const char *display_name, const char *avatar_url, bool is_online);
void room_preview_cb(const char *room_id_or_alias, const char *html_body);
void thread_list_cb(const char *room_id, const char *thread_root_id, const char *latest_msg, guint64 count, guint64 ts);
void poll_list_cb(const char *room_id, const char *event_id, const char *sender, const char *question, const char *options_str);
void chat_topic_callback(const char *room_id, const char *topic, const char *sender);
void chat_user_callback(const char *room_id, const char *user_id, bool add, const char *alias, const char *avatar_path);
void presence_callback(const char *user_id, bool is_online);

#endif // MATRIX_UTILS_H
