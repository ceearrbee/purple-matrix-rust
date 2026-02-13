#ifndef MATRIX_BLIST_H
#define MATRIX_BLIST_H

#include <libpurple/blist.h>
#include <libpurple/account.h>
#include <libpurple/roomlist.h>
#include <glib.h>
#include <stdbool.h>

void ensure_thread_in_blist(PurpleAccount *account, const char *virtual_id, const char *alias, const char *parent_room_id);
PurpleChat *find_chat_manual(PurpleAccount *account, const char *room_id);
void room_joined_callback(const char *room_id, const char *name, const char *group_name, const char *avatar_url, const char *topic, bool encrypted);
void room_left_callback(const char *room_id);
void roomlist_add_cb(const char *name, const char *id, const char *topic, guint64 count);
void cleanup_stale_thread_labels(PurpleAccount *account);

PurpleRoomlist *matrix_roomlist_get_list(PurpleConnection *gc);
void matrix_roomlist_cancel(PurpleRoomlist *list);

// PRPL Ops
const char *matrix_list_icon(PurpleAccount *account, PurpleBuddy *buddy);
const char *matrix_list_emblem(PurpleBuddy *buddy);
void matrix_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full);
void matrix_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
void matrix_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group);
GList *blist_node_menu_cb(PurpleBlistNode *node);

#endif // MATRIX_BLIST_H
