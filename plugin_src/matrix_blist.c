#include "matrix_blist.h"
#include "matrix_utils.h"
#include "matrix_globals.h"
#include "matrix_chat.h"
#include "matrix_types.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_commands.h"

#include <libpurple/util.h>
#include <libpurple/debug.h>
#include <libpurple/server.h>
#include <libpurple/notify.h>
#include <string.h>

static PurpleRoomlist *active_roomlist = NULL;

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
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  
  if (!account || purple_account_get_connection(account) == NULL) {
    goto cleanup;
  }

  if (d->room_id && strchr(d->room_id, '|')) {
    goto cleanup;
  }

  /* Atomic safety: ensure we have something to work with */
  if (!d->room_id) goto cleanup;

  char *s_name = strip_emojis(d->name ? d->name : "");
  char *s_topic = strip_emojis(d->topic ? d->topic : "");
  char *s_group = strip_emojis(d->group_name ? d->group_name : "Matrix Rooms");

  const char *group_name = (s_group && strlen(s_group) > 0) ? s_group : "Matrix Rooms";
  char *target_group_name = g_strdup(group_name);
  
  if (strstr(target_group_name, " / Threads")) {
    gchar *clean_group = derive_base_group_from_threads_group(group_name);
    g_free(target_group_name); target_group_name = clean_group;
  }

  PurpleGroup *group = purple_find_group(target_group_name);
  if (!group) { 
    group = purple_group_new(target_group_name); 
    purple_blist_add_group(group, NULL); 
  }

  PurpleChat *chat = purple_blist_find_chat(account, d->room_id);
  if (!chat) {
    GHashTable *components = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(components, g_strdup("room_id"), g_strdup(d->room_id));
    if (d->avatar_url && strlen(d->avatar_url) > 0) g_hash_table_insert(components, g_strdup("avatar_path"), g_strdup(d->avatar_url));
    if (s_topic && strlen(s_topic) > 0) g_hash_table_insert(components, g_strdup("topic"), g_strdup(s_topic));
    g_hash_table_insert(components, g_strdup("encrypted"), g_strdup(d->encrypted ? "1" : "0"));
    
    chat = purple_chat_new(account, d->room_id, components);
    purple_blist_add_chat(chat, group, NULL);
    
    if (d->avatar_url && strlen(d->avatar_url) > 0) {
      purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon", d->avatar_url);
      purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path", d->avatar_url);
    }
    if (s_name && strlen(s_name) > 0) {
        purple_blist_alias_chat(chat, s_name);
    }
  } else {
    GHashTable *components = purple_chat_get_components(chat);
    gboolean changed = FALSE;
    if (components) {
        if (d->avatar_url && strlen(d->avatar_url) > 0) {
          const char *old_avatar = g_hash_table_lookup(components, "avatar_path");
          if (g_strcmp0(old_avatar, d->avatar_url) != 0 && g_file_test(d->avatar_url, G_FILE_TEST_EXISTS)) {
              g_hash_table_replace(components, g_strdup("avatar_path"), g_strdup(d->avatar_url));
              purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon", d->avatar_url);
              purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path", d->avatar_url);
              changed = TRUE;
          }
        }
        if (s_topic && strlen(s_topic) > 0) {
            const char *old_topic = g_hash_table_lookup(components, "topic");
            if (g_strcmp0(old_topic, s_topic) != 0) {
                g_hash_table_replace(components, g_strdup("topic"), g_strdup(s_topic));
                changed = TRUE;
            }
        }
        const char *old_enc = g_hash_table_lookup(components, "encrypted");
        const char *new_enc = d->encrypted ? "1" : "0";
        if (g_strcmp0(old_enc, new_enc) != 0) {
            g_hash_table_replace(components, g_strdup("encrypted"), g_strdup(new_enc));
            changed = TRUE;
        }
    }
    
    PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
    const char *cur_grp = (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) ? purple_group_get_name((PurpleGroup *)parent) : NULL;
    if (!cur_grp || g_strcmp0(cur_grp, target_group_name) != 0) {
      purple_blist_add_chat(chat, group, NULL);
      changed = TRUE;
    }

    if (s_name && strlen(s_name) > 0) {
      const char *old_alias = chat->alias;
      if (g_strcmp0(old_alias, s_name) != 0) {
          purple_blist_alias_chat(chat, s_name);
          changed = TRUE;
      }
    }
    
    if (changed) {
        purple_blist_update_node_icon((PurpleBlistNode *)chat);
    }
  }

  g_free(target_group_name);
  g_free(s_name);
  g_free(s_topic);
  g_free(s_group);

cleanup:
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->name);
  g_free(d->group_name);
  g_free(d->avatar_url);
  if (d->topic) g_free(d->topic);
  g_free(d);
  return FALSE;
}

void room_joined_callback(const char *user_id, const char *room_id, const char *name, const char *group_name, const char *avatar_url, const char *topic, bool encrypted) {
  MatrixRoomData *d = g_new0(MatrixRoomData, 1);
  d->user_id = g_strdup(user_id); d->room_id = g_strdup(room_id); d->name = g_strdup(name); d->group_name = g_strdup(group_name); d->avatar_url = g_strdup(avatar_url); d->topic = g_strdup(topic); d->encrypted = encrypted;
  g_idle_add(process_room_cb, d);
}

typedef struct { char *user_id; char *room_id; } RoomLeftData;
static gboolean process_room_left_cb(gpointer data) {
  RoomLeftData *d = (RoomLeftData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) serv_got_chat_left(gc, get_chat_id(d->room_id));
  }
  g_free(d->user_id); g_free(d->room_id); g_free(d); return FALSE;
}

void room_left_callback(const char *user_id, const char *room_id) {
  RoomLeftData *d = g_new0(RoomLeftData, 1); d->user_id = g_strdup(user_id); d->room_id = g_strdup(room_id);
  g_idle_add(process_room_left_cb, d);
}

void room_mute_callback(const char *user_id, const char *room_id, bool muted) {
}

void room_tag_callback(const char *user_id, const char *room_id, const char *tag) {
  if (tag) {
    MatrixRoomData *d = g_new0(MatrixRoomData, 1);
    d->user_id = g_strdup(user_id);
    d->room_id = g_strdup(room_id);
    d->group_name = g_strdup(tag);
    g_idle_add(process_room_cb, d);
  }
}

void ensure_thread_in_blist(PurpleAccount *account, const char *virtual_id, const char *alias, const char *parent_room_id) {
  if (!account || purple_account_get_connection(account) == NULL || !virtual_id || !strchr(virtual_id, '|')) return;
  PurpleConnection *gc = purple_account_get_connection(account);
  if (!gc) return;
  
  char *group_name = NULL;
  if (parent_room_id) {
    PurpleChat *pchat = purple_blist_find_chat(account, parent_room_id);
    const char *p_title = (pchat && pchat->alias) ? pchat->alias : parent_room_id;
    const char *p_grp = (pchat) ? purple_group_get_name(purple_chat_get_group(pchat)) : "Matrix Rooms";
    group_name = g_strdup_printf("%s / %s / Threads", p_grp, p_title);
  } else group_name = g_strdup("Matrix Rooms / Unknown Room / Threads");
  
  PurpleGroup *group = purple_find_group(group_name);
  if (!group) { group = purple_group_new(group_name); purple_blist_add_group(group, NULL); }
  
  char *nice_alias = NULL;
  char *s_alias = strip_emojis(alias);
  if (s_alias && strlen(s_alias) > 0) {
    char *trunc = g_strndup(s_alias, 40); 
    char *end = NULL; 
    if (!g_utf8_validate(trunc, -1, (const gchar **)&end)) if (end) *end = '\0';
    nice_alias = g_strdup_printf("Thread: %s%s", trunc, strlen(s_alias) > 40 ? "..." : ""); 
    g_free(trunc);
  } else nice_alias = g_strdup("Thread");

  PurpleChat *chat = purple_blist_find_chat(account, virtual_id);
  if (!chat) {
    GHashTable *comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free); 
    g_hash_table_insert(comp, g_strdup("room_id"), g_strdup(virtual_id));
    chat = purple_chat_new(account, virtual_id, comp); 
    purple_blist_add_chat(chat, group, NULL); 
    purple_blist_alias_chat(chat, nice_alias);
  } else {
    PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
    const char *cur_grp = (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) ? purple_group_get_name((PurpleGroup *)parent) : NULL;
    if (!cur_grp || g_strcmp0(cur_grp, group_name) != 0) purple_blist_add_chat(chat, group, NULL);
    purple_blist_alias_chat(chat, nice_alias);
  }
  
  /* Atomic UI pop - ensures history is triggered via conversation-created signal in core */
  serv_got_joined_chat(gc, get_chat_id(virtual_id), virtual_id);
  
  g_free(nice_alias); g_free(group_name); g_free(s_alias);
}

static gboolean process_roomlist_add_cb(gpointer data) {
  RoomListData *d = (RoomListData *)data;
  // Account lookup not strictly needed for the global roomlist view, but good for context
  if (active_roomlist) {
    PurpleRoomlistRoom *room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, d->name, NULL);
    purple_roomlist_room_add_field(active_roomlist, room, d->id);
    purple_roomlist_room_add_field(active_roomlist, room, d->topic);
    purple_roomlist_room_add(active_roomlist, room);
  }
  g_free(d->name); g_free(d->id); g_free(d->topic); g_free(d); return FALSE;
}

void roomlist_add_cb(const char *user_id, const char *name, const char *id, const char *topic, guint64 count) {
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
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      GHashTable *comp = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free); g_hash_table_insert(comp, g_strdup("room_id"), g_strdup(d->room_id));
      serv_got_chat_invite(gc, d->room_id, d->inviter, "Incoming Matrix Invite", comp);
    }
  }
  g_free(d->user_id); g_free(d->room_id); g_free(d->inviter); g_free(d); return FALSE;
}

typedef struct {
    char *user_id;
    char *alias;
    char *avatar_url;
} UpdateBuddyData;

static gboolean process_update_buddy_cb(gpointer data) {
    UpdateBuddyData *d = (UpdateBuddyData *)data;
    PurpleAccount *account = find_matrix_account(); // Assuming single account or global update
    if (account) {
        PurpleBuddy *buddy = purple_find_buddy(account, d->user_id);
        if (buddy) {
            if (d->alias && strlen(d->alias) > 0) {
                purple_blist_alias_buddy(buddy, d->alias);
            }
            if (d->avatar_url && strlen(d->avatar_url) > 0 && g_file_test(d->avatar_url, G_FILE_TEST_EXISTS)) {
                purple_buddy_set_icon(buddy, purple_buddy_icon_new(account, d->user_id, (void *)g_strdup(d->avatar_url), strlen(d->avatar_url), NULL));
                /* In libpurple 2.x, buddy icons are usually managed via set_buddy_icon_path if custom */
                purple_blist_node_set_string((PurpleBlistNode *)buddy, "buddy_icon", d->avatar_url);
            }
        }
    }
    g_free(d->user_id);
    g_free(d->alias);
    g_free(d->avatar_url);
    g_free(d);
    return FALSE;
}

void update_buddy_callback(const char *user_id, const char *alias, const char *avatar_url) {
    UpdateBuddyData *d = g_new0(UpdateBuddyData, 1);
    d->user_id = g_strdup(user_id);
    d->alias = g_strdup(alias);
    d->avatar_url = g_strdup(avatar_url);
    g_idle_add(process_update_buddy_cb, d);
}

void invite_callback(const char *user_id, const char *room_id, const char *inviter) {
  MatrixInviteData *d = g_new0(MatrixInviteData, 1); d->user_id = g_strdup(user_id); d->room_id = g_strdup(room_id); d->inviter = g_strdup(inviter); g_idle_add(process_invite_cb, d);
}

static void menu_action_room_dashboard_blist_cb(PurpleBlistNode *node, gpointer data) {
    if (!PURPLE_BLIST_NODE_IS_CHAT(node)) return;
    PurpleChat *chat = (PurpleChat *)node;
    GHashTable *components = purple_chat_get_components(chat);
    const char *room_id = g_hash_table_lookup(components, "room_id");
    open_room_dashboard(purple_chat_get_account(chat), room_id);
}

static void menu_action_my_profile_blist_cb(PurpleBlistNode *node, gpointer data) {
    purple_matrix_rust_debug_crypto_status(NULL); // Needs account context, but global profile handles finding it
    // Actually, just call the action directly if we can
    // action_my_profile_cb(NULL); // It's static in matrix_commands.c
    // Let's call the FFI status first as a proof of concept
    PurpleBuddy *buddy = (PurpleBuddy *)node;
    purple_matrix_rust_debug_crypto_status(purple_account_get_username(purple_buddy_get_account(buddy)));
}

static void menu_action_room_leave_blist_cb(PurpleBlistNode *node, gpointer data) {
    if (!PURPLE_BLIST_NODE_IS_CHAT(node)) return;
    PurpleChat *chat = (PurpleChat *)node;
    PurpleConnection *gc = purple_account_get_connection(purple_chat_get_account(chat));
    if (gc) {
        matrix_chat_leave(gc, get_chat_id(g_hash_table_lookup(purple_chat_get_components(chat), "room_id")));
    }
}

static void menu_action_room_dashboard_buddy_cb(PurpleBlistNode *node, gpointer data) {
    if (!PURPLE_BLIST_NODE_IS_BUDDY(node)) return;
    PurpleBuddy *buddy = (PurpleBuddy *)node;
    open_room_dashboard(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
}

GList *blist_node_menu_cb(PurpleBlistNode *node) {
    GList *m = NULL;
    PurpleMenuAction *action;

    purple_debug_info("matrix", "blist_node_menu_cb: node type=%d\n", node->type);

    if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
        action = purple_menu_action_new("Room Settings...", PURPLE_CALLBACK(menu_action_room_settings_cb), node, NULL);
        m = g_list_append(m, action);
        
        action = purple_menu_action_new("Room Dashboard...", PURPLE_CALLBACK(menu_action_room_dashboard_blist_cb), node, NULL);
        m = g_list_append(m, action);

        action = purple_menu_action_new("Leave Matrix Room...", PURPLE_CALLBACK(menu_action_room_leave_blist_cb), node, NULL);
        m = g_list_append(m, action);
    } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
        PurpleBuddy *buddy = (PurpleBuddy *)node;
        PurpleAccount *account = purple_buddy_get_account(buddy);
        if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            action = purple_menu_action_new("Matrix Room Dashboard...", PURPLE_CALLBACK(menu_action_room_dashboard_buddy_cb), node, NULL);
            m = g_list_append(m, action);
            
            action = purple_menu_action_new("My Matrix Profile...", PURPLE_CALLBACK(menu_action_my_profile_blist_cb), node, NULL);
            m = g_list_append(m, action);
        }
    }
    return m;
}
