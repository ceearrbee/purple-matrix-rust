#include "matrix_blist.h"
#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_commands.h"

#include <libpurple/util.h>
#include <libpurple/debug.h>
#include <libpurple/server.h>
#include <libpurple/notify.h>
#include <string.h>

static PurpleRoomlist *active_roomlist = NULL;

PurpleChat *find_chat_manual(PurpleAccount *account, const char *room_id) {
  PurpleBlistNode *gnode, *cnode;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode)) continue;
    for (cnode = gnode->child; cnode; cnode = cnode->next) {
      if (!PURPLE_BLIST_NODE_IS_CHAT(cnode)) continue;
      PurpleChat *chat = (PurpleChat *)cnode;
      if (purple_chat_get_account(chat) != account) continue;
      GHashTable *components = purple_chat_get_components(chat);
      if (!components) continue;
      const char *id = g_hash_table_lookup(components, "room_id");
      if (id && strcmp(id, room_id) == 0) return chat;
    }
  }
  return NULL;
}

void cleanup_stale_thread_labels(PurpleAccount *account) {
  if (!account) return;
  PurpleBlistNode *gnode = NULL;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode)) continue;
    for (PurpleBlistNode *cnode = gnode->child; cnode; cnode = cnode->next) {
      if (!PURPLE_BLIST_NODE_IS_CHAT(cnode)) continue;
      PurpleChat *chat = (PurpleChat *)cnode;
      if (purple_chat_get_account(chat) != account) continue;
      GHashTable *components = purple_chat_get_components(chat);
      const char *room_id = components ? g_hash_table_lookup(components, "room_id") : NULL;
      if (!room_id || is_virtual_room_id(room_id)) continue;
      if (chat->alias && g_str_has_prefix(chat->alias, "Thread: ")) {
        const char *clean_alias = chat->alias + strlen("Thread: ");
        if (clean_alias && *clean_alias) purple_blist_alias_chat(chat, clean_alias);
      }
      PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
      if (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) {
        const char *group_name = purple_group_get_name((PurpleGroup *)parent);
        if (group_name && strstr(group_name, " / Threads")) {
          char *target_group_name = derive_base_group_from_threads_group(group_name);
          PurpleGroup *target_group = purple_find_group(target_group_name);
          if (!target_group) { target_group = purple_group_new(target_group_name); purple_blist_add_group(target_group, NULL); }
          purple_blist_add_chat(chat, target_group, NULL);
          g_free(target_group_name);
        }
      }
    }
  }
}

static gboolean process_room_cb(gpointer data) {
  MatrixRoomData *d = (MatrixRoomData *)data;
  PurpleAccount *account = find_matrix_account();
  if (d->room_id && strchr(d->room_id, '|')) {
    g_free(d->room_id); g_free(d->name); g_free(d->group_name); g_free(d->avatar_url);
    if (d->topic) g_free(d->topic);
    g_free(d);
    return FALSE;
  }
  const char *group_name = (d->group_name && strlen(d->group_name) > 0) ? d->group_name : "Matrix Rooms";
  char *target_group_name = g_strdup(group_name);
  if (strstr(target_group_name, " / Threads")) {
    gchar *clean_group = derive_base_group_from_threads_group(group_name);
    g_free(target_group_name); target_group_name = clean_group;
  }
  if (account) {
    PurpleGroup *group = purple_find_group(target_group_name);
    if (!group) { group = purple_group_new(target_group_name); purple_blist_add_group(group, NULL); }
    PurpleChat *chat = find_chat_manual(account, d->room_id);
    if (!chat) {
      GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert(components, g_strdup("room_id"), g_strdup(d->room_id));
      if (d->avatar_url && strlen(d->avatar_url) > 0) g_hash_table_insert(components, g_strdup("avatar_path"), g_strdup(d->avatar_url));
      if (d->topic && strlen(d->topic) > 0) g_hash_table_insert(components, g_strdup("topic"), g_strdup(d->topic));
      g_hash_table_insert(components, g_strdup("encrypted"), g_strdup(d->encrypted ? "1" : "0"));
      chat = purple_chat_new(account, d->room_id, components);
      purple_blist_add_chat(chat, group, NULL);
      if (d->avatar_url && strlen(d->avatar_url) > 0) {
        purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon", d->avatar_url);
        purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path", d->avatar_url);
      }
    } else {
      GHashTable *components = purple_chat_get_components(chat);
      if (d->avatar_url && strlen(d->avatar_url) > 0) {
        g_hash_table_replace(components, g_strdup("avatar_path"), g_strdup(d->avatar_url));
        purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon", d->avatar_url);
        purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path", d->avatar_url);
      }
      if (d->topic && strlen(d->topic) > 0) g_hash_table_replace(components, g_strdup("topic"), g_strdup(d->topic));
      g_hash_table_replace(components, g_strdup("encrypted"), g_strdup(d->encrypted ? "1" : "0"));
      PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
      const char *cur_grp = (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) ? purple_group_get_name((PurpleGroup *)parent) : NULL;
      if (!cur_grp || g_strcmp0(cur_grp, target_group_name) != 0) purple_blist_add_chat(chat, group, NULL);
    }
    if (chat && d->name && strlen(d->name) > 0) purple_blist_alias_chat(chat, d->name);
  }
  g_free(target_group_name); g_free(d->room_id); g_free(d->name); g_free(d->group_name); g_free(d->avatar_url);
  if (d->topic) g_free(d->topic);
  g_free(d);
  return FALSE;
}

void room_joined_callback(const char *room_id, const char *name, const char *group_name, const char *avatar_url, const char *topic, bool encrypted) {
  MatrixRoomData *d = g_new0(MatrixRoomData, 1);
  d->room_id = g_strdup(room_id); d->name = g_strdup(name); d->group_name = g_strdup(group_name); d->avatar_url = g_strdup(avatar_url); d->topic = g_strdup(topic); d->encrypted = encrypted;
  g_idle_add(process_room_cb, d);
}

static gboolean process_room_left_cb(gpointer data) {
  char *room_id = (char *)data;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) serv_got_chat_left(gc, get_chat_id(room_id));
  }
  g_free(room_id); return FALSE;
}

void room_left_callback(const char *room_id) { g_idle_add(process_room_left_cb, g_strdup(room_id)); }

void ensure_thread_in_blist(PurpleAccount *account, const char *virtual_id, const char *alias, const char *parent_room_id) {
  if (!account || !virtual_id || !strchr(virtual_id, '|')) return;
  PurpleConnection *gc = purple_account_get_connection(account);
  if (!gc) return;
  char *group_name = NULL;
  if (parent_room_id) {
    PurpleConversation *pconv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_CHAT, parent_room_id, account);
    const char *p_title = NULL; const char *p_grp = NULL;
    if (pconv) {
      p_title = purple_conversation_get_title(pconv);
      PurpleChat *pchat = find_chat_manual(account, parent_room_id);
      if (pchat) { PurpleGroup *pg = purple_chat_get_group(pchat); if (pg) p_grp = purple_group_get_name(pg); }
    } else {
      PurpleChat *pchat = find_chat_manual(account, parent_room_id);
      if (pchat) { p_title = pchat->alias ? pchat->alias : purple_chat_get_name(pchat); PurpleGroup *pg = purple_chat_get_group(pchat); if (pg) p_grp = purple_group_get_name(pg); }
    }
    if (!p_grp || !*p_grp) p_grp = "Matrix Rooms";
    if (!p_title || !*p_title) p_title = parent_room_id;
    group_name = g_strdup_printf("%s / %s / Threads", p_grp, p_title);
  } else group_name = g_strdup("Matrix Rooms / Unknown Room / Threads");
  PurpleGroup *group = purple_find_group(group_name);
  if (!group) { group = purple_group_new(group_name); purple_blist_add_group(group, NULL); }
  PurpleChat *chat = find_chat_manual(account, virtual_id);
  char *nice_alias = NULL;
  if (alias) {
    char *trunc = g_strndup(alias, 50); char *end = NULL; if (!g_utf8_validate(trunc, -1, (const gchar **)&end)) if (end) *end = '\0';
    nice_alias = g_strdup_printf("Thread: %s%s", trunc, strlen(alias) > 50 ? "..." : ""); g_free(trunc);
  } else nice_alias = g_strdup("Thread");
  if (!chat) {
    GHashTable *comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free); g_hash_table_insert(comp, g_strdup("room_id"), g_strdup(virtual_id));
    chat = purple_chat_new(account, virtual_id, comp); purple_blist_add_chat(chat, group, NULL); purple_blist_alias_chat(chat, nice_alias);
  } else {
    PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
    const char *cur_grp = (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) ? purple_group_get_name((PurpleGroup *)parent) : NULL;
    if (!cur_grp || g_strcmp0(cur_grp, group_name) != 0) purple_blist_add_chat(chat, group, NULL);
    if (alias && strlen(alias) > 0) purple_blist_alias_chat(chat, nice_alias);
  }
  g_free(nice_alias); g_free(group_name);
}

static gboolean process_roomlist_add_cb(gpointer data) {
  RoomListData *d = (RoomListData *)data;
  if (active_roomlist) {
    PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, d->name, NULL);
    purple_roomlist_room_add_field(active_roomlist, room, d->id);
    purple_roomlist_room_add_field(active_roomlist, room, d->topic);
    purple_roomlist_room_add(active_roomlist, room);
  }
  g_free(d->name); g_free(d->id); g_free(d->topic); g_free(d); return FALSE;
}

void roomlist_add_cb(const char *name, const char *id, const char *topic, guint64 count) {
  RoomListData *d = g_new0(RoomListData, 1); d->name = g_strdup(name); d->id = g_strdup(id); d->topic = g_strdup(topic); d->count = count; g_idle_add(process_roomlist_add_cb, d);
}

PurpleRoomlist *matrix_roomlist_get_list(PurpleConnection *gc) {
  if (active_roomlist) purple_roomlist_unref(active_roomlist);
  active_roomlist = purple_roomlist_new(purple_connection_get_account(gc));
  GList *fields = NULL;
  fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Room ID", "room_id", FALSE));
  fields = g_list_append(fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Topic", "topic", FALSE));
  purple_roomlist_set_fields(active_roomlist, fields);
  purple_matrix_rust_fetch_public_rooms_for_list(purple_account_get_username(purple_connection_get_account(gc)));
  return active_roomlist;
}

void matrix_roomlist_cancel(PurpleRoomlist *list) { if (list == active_roomlist) active_roomlist = NULL; purple_roomlist_unref(list); }
const char *matrix_list_icon(PurpleAccount *account, PurpleBuddy *buddy) { return "matrix"; }

static const char *matrix_chat_list_emblem(PurpleChat *chat) {
  const char *enc = g_hash_table_lookup(purple_chat_get_components(chat), "encrypted");
  return (enc && strcmp(enc, "1") == 0) ? "secure" : NULL;
}

const char *matrix_list_emblem(PurpleBuddy *buddy) { return PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy) ? matrix_chat_list_emblem((PurpleChat *)buddy) : NULL; }

static void matrix_chat_tooltip_text(PurpleChat *chat, PurpleNotifyUserInfo *user_info, gboolean full) {
  GHashTable *comp = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(comp, "room_id");
  const char *topic = g_hash_table_lookup(comp, "topic");
  const char *enc = g_hash_table_lookup(comp, "encrypted");
  if (room_id) purple_notify_user_info_add_pair(user_info, "Room ID", room_id);
  purple_notify_user_info_add_pair(user_info, "Security", (enc && strcmp(enc, "1") == 0) ? "End-to-End Encrypted" : "Unencrypted / Public");
  if (topic && strlen(topic) > 0) { char *clean = sanitize_markup_text(topic); purple_notify_user_info_add_pair(user_info, "Topic", clean); g_free(clean); }
}

void matrix_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full) {
  if (PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)) { matrix_chat_tooltip_text((PurpleChat *)buddy, user_info, full); return; }
  purple_notify_user_info_add_pair(user_info, "Matrix ID", purple_buddy_get_name(buddy));
  PurpleStatus *status = purple_presence_get_active_status(purple_buddy_get_presence(buddy));
  const char *msg = purple_status_get_attr_string(status, "message");
  if (msg) purple_notify_user_info_add_pair(user_info, "Status Message", msg);
}

void matrix_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group) { purple_matrix_rust_add_buddy(purple_account_get_username(purple_connection_get_account(gc)), purple_buddy_get_name(buddy)); }
void matrix_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group) { purple_matrix_rust_remove_buddy(purple_account_get_username(purple_connection_get_account(gc)), purple_buddy_get_name(buddy)); }

static gboolean process_invite_cb(gpointer data) {
  MatrixInviteData *d = (MatrixInviteData *)data;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      GHashTable *comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free); g_hash_table_insert(comp, g_strdup("room_id"), g_strdup(d->room_id));
      serv_got_chat_invite(gc, d->room_id, d->inviter, "Incoming Matrix Invite", comp);
    }
  }
  g_free(d->room_id); g_free(d->inviter); g_free(d); return FALSE;
}

void invite_callback(const char *room_id, const char *inviter) {
  MatrixInviteData *d = g_new0(MatrixInviteData, 1); d->room_id = g_strdup(room_id); d->inviter = g_strdup(inviter); g_idle_add(process_invite_cb, d);
}
