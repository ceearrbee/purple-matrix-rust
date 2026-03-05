#ifndef MATRIX_BLIST_H
#define MATRIX_BLIST_H

#include <glib.h>
#include <libpurple/account.h>
#include <libpurple/blist.h>
#include <libpurple/roomlist.h>
#include <stdbool.h>

void ensure_room_in_blist(PurpleAccount *account, const char *room_id,
                          const char *name, const char *group_name);
void ensure_thread_in_blist(PurpleAccount *account, const char *virtual_id,
                            const char *alias, const char *parent_room_id);
void room_joined_callback(const char *user_id, const char *room_id,
                          const char *name, const char *group_name,
                          const char *avatar_url, const char *topic,
                          bool encrypted, guint64 member_count);
void room_left_callback(const char *user_id, const char *room_id);
void room_mute_callback(const char *user_id, const char *room_id, bool muted);
void room_tag_callback(const char *user_id, const char *room_id,
                       const char *tag);
void power_level_update_callback(const char *user_id, const char *room_id,
                                 bool is_admin, bool can_kick, bool can_ban,
                                 bool can_redact, bool can_invite);
void invite_callback(const char *user_id, const char *room_id,
                     const char *inviter);
void roomlist_add_cb(const char *user_id, const char *name, const char *id,
                     const char *topic, guint64 count, gboolean is_space,
                     const char *parent_id);
void matrix_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *category);
void update_buddy_callback(const char *user_id, const char *alias,
                           const char *avatar_url);
void cleanup_stale_thread_labels(PurpleAccount *account);

PurpleRoomlist *matrix_roomlist_get_list(PurpleConnection *gc);
void matrix_roomlist_cancel(PurpleRoomlist *list);

// PRPL Ops
const char *matrix_list_icon(PurpleAccount *account, PurpleBuddy *buddy);
const char *matrix_list_emblem(PurpleBuddy *buddy);
char *matrix_status_text(PurpleBuddy *buddy);
void matrix_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info,
                         gboolean full);
void matrix_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                      PurpleGroup *group);
void matrix_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                         PurpleGroup *group);
GList *blist_node_menu_cb(PurpleBlistNode *node);

#endif // MATRIX_BLIST_H
