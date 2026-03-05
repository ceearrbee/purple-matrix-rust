#ifndef MATRIX_UTILS_H
#define MATRIX_UTILS_H

#include <glib.h>
#include <libpurple/account.h>
#include <libpurple/request.h>
#include <stdbool.h>

int get_chat_id(const char *room_id);
gboolean is_virtual_room_id(const char *room_id);
char *dup_base_room_id(const char *room_id);
char *derive_base_group_from_threads_group(const char *group_name);
char *sanitize_markup_text(const char *input);
char *strip_emojis(const char *input);
void matrix_request_field_set_help_string(PurpleRequestField *field,
                                          const char *hint);
PurpleAccount *find_matrix_account(void);
PurpleAccount *find_matrix_account_by_id(const char *user_id);
char *matrix_get_chat_name(GHashTable *components);
guint32 get_history_page_size(PurpleAccount *account);
void matrix_ui_refresh_room_chips(PurpleConversation *conv);
void matrix_utils_cleanup(void);

#endif // MATRIX_UTILS_H
