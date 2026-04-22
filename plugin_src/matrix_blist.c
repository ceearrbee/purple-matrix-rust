#include "matrix_blist.h"
#include "matrix_chat.h"
#include "matrix_commands.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/conversation.h>
#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <string.h>
#include <glib/gstdio.h>

typedef struct {
  char *user_id;
  char *room_id;
  char *name;
  char *group_name;
  char *avatar_url;
  char *topic;
  bool encrypted;
  guint64 member_count;
} MatrixRoomData;

typedef struct {
  char *user_id;
  char *name;
  char *id;
  char *topic;
  guint64 count;
  gboolean is_space;
  char *parent_id;
} RoomListData;

typedef struct {
  char *user_id;
  char *room_id;
  char *inviter;
} MatrixInviteData;

static gboolean process_room_cb(gpointer data);

static PurpleRoomlist *active_roomlist = NULL;
static GHashTable *roomlist_nodes = NULL;

typedef struct {
  PurpleAccount *account;
  char *target_user_id;
} ModerateBuddyCtx;

void ensure_room_in_blist(PurpleAccount *account, const char *room_id, const char *name, const char *group_name) {
    MatrixRoomData *d = g_new0(MatrixRoomData, 1);
    d->user_id = g_strdup(purple_account_get_username(account));
    d->room_id = g_strdup(room_id);
    d->name = g_strdup(name);
    d->group_name = g_strdup(group_name);
    process_room_cb(d);
}

static void moderate_buddy_room_cb(void *user_data, const char *room_id) {
  ModerateBuddyCtx *ctx = (ModerateBuddyCtx *)user_data;
  if (ctx && room_id && *room_id && ctx->target_user_id &&
      *ctx->target_user_id) {
    matrix_ui_action_moderate_user(room_id, ctx->target_user_id);
  }
  if (ctx) {
    g_free(ctx->target_user_id);
    g_free(ctx);
  }
}

void cleanup_stale_thread_labels(PurpleAccount *account) {
  if (!account)
    return;
  PurpleBlistNode *gnode = NULL;
  for (gnode = purple_blist_get_root(); gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode))
      continue;
    for (PurpleBlistNode *cnode = gnode->child; cnode; cnode = cnode->next) {
      if (!PURPLE_BLIST_NODE_IS_CHAT(cnode))
        continue;
      PurpleChat *chat = (PurpleChat *)cnode;
      if (purple_chat_get_account(chat) != account)
        continue;
      GHashTable *components = purple_chat_get_components(chat);
      const char *room_id =
          components ? g_hash_table_lookup(components, "room_id") : NULL;
      if (!room_id || is_virtual_room_id(room_id))
        continue;
      if (chat->alias && g_str_has_prefix(chat->alias, "Thread: ")) {
        const char *clean_alias = chat->alias + strlen("Thread: ");
        if (clean_alias && *clean_alias)
          purple_blist_alias_chat(chat, clean_alias);
      }
      PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
      if (parent && PURPLE_BLIST_NODE_IS_GROUP(parent)) {
        const char *group_name = purple_group_get_name((PurpleGroup *)parent);
        if (group_name && strstr(group_name, " / Threads")) {
          char *target_group_name =
              derive_base_group_from_threads_group(group_name);
          PurpleGroup *target_group = purple_find_group(target_group_name);
          if (!target_group) {
            target_group = purple_group_new(target_group_name);
            purple_blist_add_group(target_group, NULL);
          }
          purple_blist_add_chat(chat, target_group, NULL);
          g_free(target_group_name);
        }
      }
    }
  }
}

static PurpleChat *cleanup_duplicates_and_take_over(PurpleAccount *account, const char *room_id) {
  if (!room_id) return NULL;

  /* First, try the fast built-in lookup */
  PurpleChat *chat = purple_blist_find_chat(account, room_id);
  
  /* If we found it, we're mostly good. Just verify the account. */
  if (chat) {
      if (chat->account != account) {
          purple_debug_info("matrix-ffi", "cleanup: Re-assigning chat %s to current account pointer\n", room_id);
          chat->account = account;
      }
      return chat;
  }

  /* If built-in lookup failed, we only then fall back to a manual scan to find 
     hidden duplicates or orphaned nodes from different account instances.
     This only happens ONCE per room if the built-in lookup fails. */
  PurpleBlistNode *gnode, *cnode, *next_cnode;
  PurpleBuddyList *blist = purple_get_blist();
  if (!blist || !blist->root) return NULL;

  PurpleChat *kept_chat = NULL;
  for (gnode = blist->root; gnode; gnode = gnode->next) {
    if (!PURPLE_BLIST_NODE_IS_GROUP(gnode)) continue;
    for (cnode = gnode->child; cnode; cnode = next_cnode) {
      next_cnode = cnode->next;
      if (PURPLE_BLIST_NODE_IS_CHAT(cnode)) {
        PurpleChat *c = (PurpleChat *)cnode;
        GHashTable *components = purple_chat_get_components(c);
        const char *r_id = components ? g_hash_table_lookup(components, "room_id") : NULL;
        
        if (r_id && g_strcmp0(r_id, room_id) == 0) {
          if (!kept_chat) {
            kept_chat = c;
            c->account = account;
          } else {
            purple_debug_info("matrix-ffi", "cleanup: Removing redundant duplicate chat node for %s\n", room_id);
            purple_blist_remove_chat(c);
          }
        }
      }
    }
  }
  return kept_chat;
}

static gboolean process_room_cb(gpointer data) {
  MatrixRoomData *d = (MatrixRoomData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (!account) {
    purple_debug_warning("matrix-ffi", "process_room_cb: No account found for user_id=%s\n", d->user_id);
    goto cleanup;
  }

  PurpleConnection *gc = purple_account_get_connection(account);
  if (!gc) {
    purple_debug_warning("matrix-ffi", "process_room_cb: Account %s is not connected\n", d->user_id);
    goto cleanup;
  }

  /* Atomic safety: ensure we have something to work with */
  if (!d->room_id) {
    purple_debug_error("matrix-ffi", "process_room_cb: room_id is NULL\n");
    goto cleanup;
  }

  purple_debug_info("matrix-ffi", "process_room_cb: Processing room_id=%s name=%s for user_id=%s, group=%s\n", d->room_id, d->name ? d->name : "(null)", d->user_id, d->group_name ? d->group_name : "(null)");

  char *s_name = strip_emojis(d->name ? d->name : "");
  char *s_topic = strip_emojis(d->topic ? d->topic : "");
  char *s_group = strip_emojis(d->group_name ? d->group_name : "Matrix");

  const char *group_name = s_group;
  char *target_group_name = g_strdup(group_name);

  if (strstr(target_group_name, " / Threads")) {
    gchar *clean_group = derive_base_group_from_threads_group(group_name);
    g_free(target_group_name);
    target_group_name = clean_group;
  }

  if (!target_group_name || strlen(target_group_name) == 0) {
    purple_debug_info("matrix-ffi", "process_room_cb: No target group name, skipping blist update for %s\n", d->room_id);
    if (target_group_name) g_free(target_group_name);
    goto cleanup;
  }

  purple_debug_info("matrix-ffi", "process_room_cb: target_group_name=%s\n", target_group_name);

  PurpleGroup *group = purple_find_group(target_group_name);
  if (!group) {
    group = purple_group_new(target_group_name);
    purple_blist_add_group(group, NULL);
  }

  PurpleChat *chat = cleanup_duplicates_and_take_over(account, d->room_id);

  if (!chat) {
    purple_debug_info("matrix-ffi", "process_room_cb: No existing chat found for %s, creating new one in group %s\n", d->room_id, target_group_name);
    GHashTable *components =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(components, g_strdup("room_id"), g_strdup(d->room_id));
    if (d->avatar_url && strlen(d->avatar_url) > 0)
      g_hash_table_insert(components, g_strdup("avatar_path"),
                          g_strdup(d->avatar_url));
    if (s_topic && strlen(s_topic) > 0)
      g_hash_table_insert(components, g_strdup("topic"), g_strdup(s_topic));
    g_hash_table_insert(components, g_strdup("encrypted"),
                        g_strdup(d->encrypted ? "1" : "0"));

    chat = purple_chat_new(account, d->room_id, components);
    purple_blist_add_chat(chat, group, NULL);

    if (d->avatar_url && strlen(d->avatar_url) > 0) {
      purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon",
                                   d->avatar_url);
      purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path",
                                   d->avatar_url);
    }
    if (s_name && strlen(s_name) > 0) {
      purple_blist_alias_chat(chat, s_name);
    }
  } else {
    purple_debug_info("matrix-ffi", "process_room_cb: Found existing chat for %s, updating...\n", d->room_id);
    GHashTable *components = purple_chat_get_components(chat);
    gboolean changed = FALSE;
    if (components) {
      if (d->avatar_url && strlen(d->avatar_url) > 0) {
        const char *old_avatar = g_hash_table_lookup(components, "avatar_path");
        if (g_strcmp0(old_avatar, d->avatar_url) != 0 &&
            g_file_test(d->avatar_url, G_FILE_TEST_EXISTS)) {
          g_hash_table_replace(components, g_strdup("avatar_path"),
                               g_strdup(d->avatar_url));
          purple_blist_node_set_string((PurpleBlistNode *)chat, "buddy_icon",
                                       d->avatar_url);
          purple_blist_node_set_string((PurpleBlistNode *)chat, "icon_path",
                                       d->avatar_url);
          changed = TRUE;
        }
      }
      if (s_topic && strlen(s_topic) > 0) {
        const char *old_topic = g_hash_table_lookup(components, "topic");
        if (g_strcmp0(old_topic, s_topic) != 0) {
          g_hash_table_replace(components, g_strdup("topic"),
                               g_strdup(s_topic));
          changed = TRUE;
        }
      }
      const char *old_enc = g_hash_table_lookup(components, "encrypted");
      const char *new_enc = d->encrypted ? "1" : "0";
      if (g_strcmp0(old_enc, new_enc) != 0) {
        g_hash_table_replace(components, g_strdup("encrypted"),
                             g_strdup(new_enc));
        changed = TRUE;
      }
    }

    PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
    const char *cur_grp = (parent && PURPLE_BLIST_NODE_IS_GROUP(parent))
                              ? purple_group_get_name((PurpleGroup *)parent)
                              : NULL;
    if (!cur_grp || g_strcmp0(cur_grp, target_group_name) != 0) {
      purple_debug_info("matrix-ffi", "process_room_cb: Moving chat %s from group %s to %s\n", d->room_id, cur_grp ? cur_grp : "(null)", target_group_name);
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

  /* Set conversation data if the chat window is open */
  PurpleConversation *conv = purple_find_conversation_with_account(
      PURPLE_CONV_TYPE_CHAT, d->room_id, account);
  if (conv) {
    char mc_buf[32];
    g_snprintf(mc_buf, sizeof(mc_buf), "%" G_GUINT64_FORMAT, d->member_count);

    g_free(purple_conversation_get_data(conv, "matrix_room_encrypted"));
    purple_conversation_set_data(conv, "matrix_room_encrypted",
                                 g_strdup(d->encrypted ? "1" : "0"));

    g_free(purple_conversation_get_data(conv, "matrix_room_member_count"));
    purple_conversation_set_data(conv, "matrix_room_member_count",
                                 g_strdup(mc_buf));

    matrix_ui_refresh_room_chips(conv);
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
  if (d->topic)
    g_free(d->topic);
  g_free(d);
  return FALSE;
}

void room_joined_callback(const char *user_id, const char *room_id,
                          const char *name, const char *group_name,
                          const char *avatar_url, const char *topic,
                          bool encrypted, guint64 member_count) {
  purple_debug_info("matrix-ffi", "room_joined_callback: room_id=%s user_id=%s\n", room_id, user_id);
  MatrixRoomData *d = g_new0(MatrixRoomData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->name = g_strdup(name);
  d->group_name = g_strdup(group_name);
  d->avatar_url = g_strdup(avatar_url);
  d->topic = g_strdup(topic);
  d->encrypted = encrypted;
  d->member_count = member_count;
  
  process_room_cb(d);
}

typedef struct {
  char *user_id;
  char *room_id;
} RoomLeftData;
static gboolean process_room_left_cb(gpointer data) {
  RoomLeftData *d = (RoomLeftData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc)
      serv_got_chat_left(gc, get_chat_id(d->room_id));
    
    PurpleChat *chat = cleanup_duplicates_and_take_over(account, d->room_id);
    if (chat) purple_blist_remove_chat(chat);
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d);
  return FALSE;
}

void room_left_callback(const char *user_id, const char *room_id) {
  RoomLeftData *d = g_new0(RoomLeftData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  g_idle_add(process_room_left_cb, d);
}

void room_mute_callback(const char *user_id, const char *room_id, bool muted) {
  PurpleAccount *account = find_matrix_account_by_id(user_id);
  GList *convs = NULL;
  char state[2];
  if (!account || !room_id || !*room_id)
    return;
  g_snprintf(state, sizeof(state), "%d", muted ? 1 : 0);

  convs = purple_get_conversations();
  for (GList *it = convs; it; it = it->next) {
    PurpleConversation *conv = (PurpleConversation *)it->data;
    const char *cid = NULL;
    if (!conv)
      continue;
    if (purple_conversation_get_account(conv) != account)
      continue;
    cid = purple_conversation_get_name(conv);
    if (!cid || !*cid)
      continue;
    if (strcmp(cid, room_id) == 0 ||
        (g_str_has_prefix(cid, room_id) && cid[strlen(room_id)] == '|')) {
      g_free(purple_conversation_get_data(conv, "matrix_room_muted"));
      purple_conversation_set_data(conv, "matrix_room_muted", g_strdup(state));
    }
  }
  purple_debug_info("matrix-ui-signal",
                    "Dispatching matrix-ui-room-muted room_id=%s muted=%d\n",
                    room_id, muted);
  purple_signal_emit(my_plugin, "matrix-ui-room-muted", room_id, muted);
}

void power_level_update_callback(const char *user_id, const char *room_id,
                                 bool is_admin, bool can_kick, bool can_ban,
                                 bool can_redact, bool can_invite) {
  PurpleAccount *account = find_matrix_account_by_id(user_id);
  GList *convs = NULL;
  char state[2];
  if (!account || !room_id || !*room_id)
    return;

  convs = purple_get_conversations();
  for (GList *it = convs; it; it = it->next) {
    PurpleConversation *conv = (PurpleConversation *)it->data;
    const char *cid = NULL;
    if (!conv)
      continue;
    if (purple_conversation_get_account(conv) != account)
      continue;
    cid = purple_conversation_get_name(conv);
    if (!cid || !*cid)
      continue;
    if (strcmp(cid, room_id) == 0 ||
        (g_str_has_prefix(cid, room_id) && cid[strlen(room_id)] == '|')) {
      g_snprintf(state, sizeof(state), "%d", is_admin ? 1 : 0);
      g_free(purple_conversation_get_data(conv, "matrix_power_admin"));
      purple_conversation_set_data(conv, "matrix_power_admin", g_strdup(state));

      g_snprintf(state, sizeof(state), "%d", can_kick ? 1 : 0);
      g_free(purple_conversation_get_data(conv, "matrix_power_kick"));
      purple_conversation_set_data(conv, "matrix_power_kick", g_strdup(state));

      g_snprintf(state, sizeof(state), "%d", can_ban ? 1 : 0);
      g_free(purple_conversation_get_data(conv, "matrix_power_ban"));
      purple_conversation_set_data(conv, "matrix_power_ban", g_strdup(state));

      g_snprintf(state, sizeof(state), "%d", can_redact ? 1 : 0);
      g_free(purple_conversation_get_data(conv, "matrix_power_redact"));
      purple_conversation_set_data(conv, "matrix_power_redact",
                                   g_strdup(state));

      g_snprintf(state, sizeof(state), "%d", can_invite ? 1 : 0);
      g_free(purple_conversation_get_data(conv, "matrix_power_invite"));
      purple_conversation_set_data(conv, "matrix_power_invite",
                                   g_strdup(state));
    }
  }
}

void room_tag_callback(const char *user_id, const char *room_id,
                       const char *tag) {
  if (tag) {
    MatrixRoomData *d = g_new0(MatrixRoomData, 1);
    d->user_id = g_strdup(user_id);
    d->room_id = g_strdup(room_id);
    d->group_name = g_strdup(tag);
    g_idle_add(process_room_cb, d);
  }
}

void ensure_thread_in_blist(PurpleAccount *account, const char *virtual_id,
                            const char *alias, const char *parent_room_id) {
  if (!account || purple_account_get_connection(account) == NULL ||
      !virtual_id || !strchr(virtual_id, '|'))
    return;
  PurpleConnection *gc = purple_account_get_connection(account);
  if (!gc)
    return;

  char *group_name = NULL;
  if (parent_room_id) {
    PurpleChat *pchat = cleanup_duplicates_and_take_over(account, parent_room_id);
    if (!pchat) {
        purple_debug_info("matrix-ffi", "ensure_thread_in_blist: Skipping thread %s because parent %s is not in blist\n", virtual_id, parent_room_id);
        return;
    }
    const char *p_title = (pchat->alias) ? pchat->alias : parent_room_id;
    const char *p_grp = purple_group_get_name(purple_chat_get_group(pchat));
    
    if (!p_grp) p_grp = "Matrix";
    
    group_name = g_strdup_printf("%s / %s / Threads", p_grp, p_title);
  } else {
    /* No parent room ID provided, can't reliably group it without a space context */
    return;
  }

  PurpleGroup *group = purple_find_group(group_name);
  if (!group) {
    group = purple_group_new(group_name);
    purple_blist_add_group(group, NULL);
  }

  char *nice_alias = NULL;
  char *s_alias = strip_emojis(alias);
  if (s_alias && strlen(s_alias) > 0) {
    char *trunc = g_strndup(s_alias, 40);
    char *end = NULL;
    if (!g_utf8_validate(trunc, -1, (const gchar **)&end))
      if (end)
        *end = '\0';
    nice_alias = g_strdup_printf("Thread: %s%s", trunc,
                                 strlen(s_alias) > 40 ? "..." : "");
    g_free(trunc);
  } else
    nice_alias = g_strdup("Thread");

  PurpleChat *chat = cleanup_duplicates_and_take_over(account, virtual_id);
  if (!chat) {
    GHashTable *comp =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    g_hash_table_insert(comp, g_strdup("room_id"), g_strdup(virtual_id));
    chat = purple_chat_new(account, virtual_id, comp);
    purple_blist_add_chat(chat, group, NULL);
    purple_blist_alias_chat(chat, nice_alias);
  } else {
    PurpleBlistNode *parent = PURPLE_BLIST_NODE(chat)->parent;
    const char *cur_grp = (parent && PURPLE_BLIST_NODE_IS_GROUP(parent))
                              ? purple_group_get_name((PurpleGroup *)parent)
                              : NULL;
    if (!cur_grp || g_strcmp0(cur_grp, group_name) != 0)
      purple_blist_add_chat(chat, group, NULL);
    purple_blist_alias_chat(chat, nice_alias);
  }

  g_free(nice_alias);
  g_free(group_name);
  g_free(s_alias);
}

static gboolean process_roomlist_add_cb(gpointer data) {
  RoomListData *d = (RoomListData *)data;
  // Account lookup not strictly needed for the global roomlist view, but good
  // for context
  if (active_roomlist) {
    PurpleRoomlistRoomType type = d->is_space
                                      ? PURPLE_ROOMLIST_ROOMTYPE_CATEGORY
                                      : PURPLE_ROOMLIST_ROOMTYPE_ROOM;
    PurpleRoomlistRoom *parent = NULL;
    if (d->parent_id && roomlist_nodes) {
        parent = g_hash_table_lookup(roomlist_nodes, d->parent_id);
    }
    PurpleRoomlistRoom *room = purple_roomlist_room_new(type, d->name, parent);
    purple_roomlist_room_add_field(active_roomlist, room, d->id);
    purple_roomlist_room_add_field(active_roomlist, room, d->topic);
    purple_roomlist_room_add(active_roomlist, room);
    if (roomlist_nodes && d->id) {
        g_hash_table_insert(roomlist_nodes, g_strdup(d->id), room);
    }
  }
  g_free(d->user_id);
  g_free(d->name);
  g_free(d->id);
  g_free(d->topic);
  g_free(d->parent_id);
  g_free(d);
  return FALSE;
}
void roomlist_add_cb(const char *user_id, const char *name, const char *id,
                     const char *topic, guint64 count, gboolean is_space,
                     const char *parent_id) {
  RoomListData *d = g_new0(RoomListData, 1);
  d->user_id = g_strdup(user_id);
  d->name = g_strdup(name);
  d->id = g_strdup(id);
  d->topic = g_strdup(topic);
  d->count = count;
  d->is_space = is_space;
  d->parent_id = g_strdup(parent_id);
  g_idle_add(process_roomlist_add_cb, d);
}

PurpleRoomlist *matrix_roomlist_get_list(PurpleConnection *gc) {
  if (active_roomlist)
    purple_roomlist_unref(active_roomlist);
  if (roomlist_nodes)
    g_hash_table_destroy(roomlist_nodes);

  active_roomlist = purple_roomlist_new(purple_connection_get_account(gc));
  roomlist_nodes = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

  GList *fields = NULL;
  fields = g_list_append(
      fields, purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "Room ID",
                                        "room_id", FALSE));
  fields = g_list_append(fields,
                         purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING,
                                                   "Topic", "topic", FALSE));
  purple_roomlist_set_fields(active_roomlist, fields);
  purple_matrix_rust_list_public_rooms(
      purple_account_get_username(purple_connection_get_account(gc)));
  return active_roomlist;
}

void matrix_roomlist_cancel(PurpleRoomlist *list) {
  if (list == active_roomlist) {
    active_roomlist = NULL;
    if (roomlist_nodes) {
        g_hash_table_destroy(roomlist_nodes);
        roomlist_nodes = NULL;
    }
  }
  purple_roomlist_unref(list);
}
const char *matrix_list_icon(PurpleAccount *account, PurpleBuddy *buddy) {
  return "matrix";
}

static const char *matrix_chat_list_emblem(PurpleChat *chat) {
  const char *enc =
      g_hash_table_lookup(purple_chat_get_components(chat), "encrypted");
  return (enc && strcmp(enc, "1") == 0) ? "secure" : NULL;
}

const char *matrix_list_emblem(PurpleBuddy *buddy) {
  return PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)
             ? matrix_chat_list_emblem((PurpleChat *)buddy)
             : NULL;
}

static void matrix_chat_tooltip_text(PurpleChat *chat,
                                     PurpleNotifyUserInfo *user_info,
                                     gboolean full) {
  GHashTable *comp = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(comp, "room_id");
  const char *topic = g_hash_table_lookup(comp, "topic");
  const char *enc = g_hash_table_lookup(comp, "encrypted");
  if (room_id) {
    char *esc_id = g_markup_escape_text(room_id, -1);
    purple_notify_user_info_add_pair(user_info, "Room ID", esc_id);
    g_free(esc_id);
  }
  purple_notify_user_info_add_pair(user_info, "Security",
                                   (enc && strcmp(enc, "1") == 0)
                                       ? "End-to-End Encrypted"
                                       : "Unencrypted / Public");
  if (topic && strlen(topic) > 0) {
    char *clean = sanitize_markup_text(topic);
    char *esc_topic = g_markup_escape_text(clean, -1);
    purple_notify_user_info_add_pair(user_info, "Topic", esc_topic);
    g_free(esc_topic);
    g_free(clean);
  }
}

void matrix_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info,
                         gboolean full) {
  if (PURPLE_BLIST_NODE_IS_CHAT((PurpleBlistNode *)buddy)) {
    matrix_chat_tooltip_text((PurpleChat *)buddy, user_info, full);
    return;
  }
  char *esc_id = g_markup_escape_text(purple_buddy_get_name(buddy), -1);
  purple_notify_user_info_add_pair(user_info, "Matrix ID", esc_id);
  g_free(esc_id);

  PurplePresence *presence = purple_buddy_get_presence(buddy);
  if (presence) {
    PurpleStatus *status = purple_presence_get_active_status(presence);
    if (status) {
      const char *msg = purple_status_get_attr_string(status, "message");
      if (msg) {
        char *esc_msg = g_markup_escape_text(msg, -1);
        purple_notify_user_info_add_pair(user_info, "Status Message", esc_msg);
        g_free(esc_msg);
      }
    }
  }
}

void matrix_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                      PurpleGroup *group) {
  purple_matrix_rust_add_buddy(
      purple_account_get_username(purple_connection_get_account(gc)),
      purple_buddy_get_name(buddy));
}
void matrix_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                         PurpleGroup *group) {
  purple_matrix_rust_remove_buddy(
      purple_account_get_username(purple_connection_get_account(gc)),
      purple_buddy_get_name(buddy));
}

static gboolean process_invite_cb(gpointer data) {
  MatrixInviteData *d = (MatrixInviteData *)data;
  PurpleAccount *account = find_matrix_account_by_id(d->user_id);
  if (account && purple_account_get_connection(account) != NULL) {
    PurpleConnection *gc = purple_account_get_connection(account);
    if (gc) {
      GHashTable *comp =
          g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      g_hash_table_insert(comp, g_strdup("room_id"), g_strdup(d->room_id));
      serv_got_chat_invite(gc, d->room_id, d->inviter, "Incoming Matrix Invite",
                           comp);
      g_hash_table_destroy(comp);
    }
  }
  g_free(d->user_id);
  g_free(d->room_id);
  g_free(d->inviter);
  g_free(d);
  return FALSE;
}

typedef struct {
  char *user_id;
  char *alias;
  char *avatar_url;
} UpdateBuddyData;

static gboolean process_update_buddy_cb(gpointer data) {
  UpdateBuddyData *d = (UpdateBuddyData *)data;
  PurpleAccount *account = find_matrix_account();
  if (account) {
    PurpleBuddy *buddy = purple_find_buddy(account, d->user_id);
    if (!buddy) {
        PurpleGroup *group = purple_find_group("Direct Messages");
        if (!group) {
            group = purple_group_new("Direct Messages");
            purple_blist_add_group(group, NULL);
        }
        purple_debug_info("matrix-ffi", "cleanup: Creating new buddy for DM partner %s\n", d->user_id);
        buddy = purple_buddy_new(account, d->user_id, d->alias);
        purple_blist_add_buddy(buddy, NULL, group, NULL);
        purple_prpl_got_user_status(account, d->user_id, "available", NULL);
    }

    if (buddy) {
      if (d->alias && strlen(d->alias) > 0) {
        purple_blist_alias_buddy(buddy, d->alias);
        if (strcmp(d->user_id, purple_account_get_username(account)) == 0) {
          purple_account_set_alias(account, d->alias);
        }
      }
      if (d->avatar_url && strlen(d->avatar_url) > 0 &&
          g_file_test(d->avatar_url, G_FILE_TEST_EXISTS)) {
        
        const char *current_icon_path = purple_blist_node_get_string((PurpleBlistNode *)buddy, "buddy_icon");
        if (!current_icon_path || g_strcmp0(current_icon_path, d->avatar_url) != 0) {
            GStatBuf st;
            if (g_stat(d->avatar_url, &st) == 0 && st.st_size <= 2 * 1024 * 1024) {
                gchar *contents;
                gsize length;
                if (g_file_get_contents(d->avatar_url, &contents, &length, NULL)) {
                  PurpleBuddyIcon *icon =
                      purple_buddy_icon_new(account, d->user_id, contents, length, NULL);
                  purple_buddy_set_icon(buddy, icon);
                  purple_buddy_icon_unref(icon);
                  
                  if (strcmp(d->user_id, purple_account_get_username(account)) == 0) {
                      purple_account_set_buddy_icon_path(account, d->avatar_url);
                  }
                }
            } else {
                purple_debug_warning("matrix-ffi", "Avatar file too large or unstatable: %s\n", d->avatar_url);
            }
        }
      }
    }
  }
  g_free(d->user_id);
  g_free(d->alias);
  g_free(d->avatar_url);
  g_free(d);
  return FALSE;
}

void update_buddy_callback(const char *user_id, const char *alias,
                           const char *avatar_url) {
  UpdateBuddyData *d = g_new0(UpdateBuddyData, 1);
  d->user_id = g_strdup(user_id);
  d->alias = g_strdup(alias);
  d->avatar_url = g_strdup(avatar_url);
  g_idle_add(process_update_buddy_cb, d);
}

void invite_callback(const char *user_id, const char *room_id,
                     const char *inviter) {
  MatrixInviteData *d = g_new0(MatrixInviteData, 1);
  d->user_id = g_strdup(user_id);
  d->room_id = g_strdup(room_id);
  d->inviter = g_strdup(inviter);
  g_idle_add(process_invite_cb, d);
}

static void menu_action_room_dashboard_blist_cb(PurpleBlistNode *node,
                                                gpointer data) {
  if (!PURPLE_BLIST_NODE_IS_CHAT(node))
    return;
  PurpleChat *chat = (PurpleChat *)node;
  GHashTable *components = purple_chat_get_components(chat);
  const char *room_id = g_hash_table_lookup(components, "room_id");
  open_room_dashboard(purple_chat_get_account(chat), room_id);
}

static void menu_action_my_profile_blist_cb(PurpleBlistNode *node,
                                            gpointer data) {
  purple_matrix_rust_debug_crypto_status(
      NULL); // Needs account context, but global profile handles finding it
  // Actually, just call the action directly if we can
  // action_my_profile_cb(NULL); // It's static in matrix_commands.c
  // Let's call the FFI status first as a proof of concept
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  purple_matrix_rust_debug_crypto_status(
      purple_account_get_username(purple_buddy_get_account(buddy)));
}

static void menu_action_room_leave_blist_cb(PurpleBlistNode *node,
                                            gpointer data) {
  if (!PURPLE_BLIST_NODE_IS_CHAT(node))
    return;
  PurpleChat *chat = (PurpleChat *)node;
  PurpleConnection *gc =
      purple_account_get_connection(purple_chat_get_account(chat));
  if (gc) {
    matrix_chat_leave(gc, get_chat_id(g_hash_table_lookup(
                              purple_chat_get_components(chat), "room_id")));
  }
}

static void menu_action_room_dashboard_buddy_cb(PurpleBlistNode *node,
                                                gpointer data) {
  if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
    return;
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  open_room_dashboard(purple_buddy_get_account(buddy),
                      purple_buddy_get_name(buddy));
}

static void menu_action_user_info_buddy_cb(PurpleBlistNode *node,
                                           gpointer data) {
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  PurpleAccount *account = NULL;
  const char *target = NULL;
  if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
    return;
  account = purple_buddy_get_account(buddy);
  target = purple_buddy_get_name(buddy);
  if (!account || !target || !*target)
    return;
  purple_matrix_rust_get_user_info(purple_account_get_username(account),
                                   target);
}

static void menu_action_moderate_buddy_cb(PurpleBlistNode *node,
                                          gpointer data) {
  PurpleBuddy *buddy = (PurpleBuddy *)node;
  const char *target = NULL;
  PurpleAccount *account = NULL;
  if (!PURPLE_BLIST_NODE_IS_BUDDY(node))
    return;
  account = purple_buddy_get_account(buddy);
  target = purple_buddy_get_name(buddy);
  if (!account || !target || !*target)
    return;

  ModerateBuddyCtx *ctx = g_new0(ModerateBuddyCtx, 1);
  ctx->account = account;
  ctx->target_user_id = g_strdup(target);
  purple_request_input(
      my_plugin, "Moderate User", "Room ID",
      "Enter Matrix room ID to moderate this user (example: !room:server).",
      NULL, FALSE, FALSE, NULL, "_Open Moderation",
      G_CALLBACK(moderate_buddy_room_cb), "_Cancel", NULL, account, NULL, NULL,
      ctx);
}

GList *blist_node_menu_cb(PurpleBlistNode *node) {
  GList *m = NULL;
  PurpleMenuAction *action;

  purple_debug_info("matrix", "blist_node_menu_cb: node type=%d\n", node->type);

  if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
    PurpleChat *chat = (PurpleChat *)node;
    PurpleAccount *account = purple_chat_get_account(chat);
    if (account && strcmp(purple_account_get_protocol_id(account),
                          "prpl-matrix-rust") == 0) {
      action = purple_menu_action_new(
          "Room Settings...", PURPLE_CALLBACK(menu_action_room_settings_cb),
          node, NULL);
      m = g_list_append(m, action);

      action = purple_menu_action_new(
          "Room Dashboard...",
          PURPLE_CALLBACK(menu_action_room_dashboard_blist_cb), node, NULL);
      m = g_list_append(m, action);

      action = purple_menu_action_new(
          "Leave Matrix Room...",
          PURPLE_CALLBACK(menu_action_room_leave_blist_cb), node, NULL);
      m = g_list_append(m, action);
    }
  } else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
    PurpleBuddy *buddy = (PurpleBuddy *)node;
    PurpleAccount *account = purple_buddy_get_account(buddy);
    if (account && strcmp(purple_account_get_protocol_id(account),
                          "prpl-matrix-rust") == 0) {
      action = purple_menu_action_new(
          "Matrix Room Dashboard...",
          PURPLE_CALLBACK(menu_action_room_dashboard_buddy_cb), node, NULL);
      m = g_list_append(m, action);

      action = purple_menu_action_new(
          "Get Matrix User Info",
          PURPLE_CALLBACK(menu_action_user_info_buddy_cb), node, NULL);
      m = g_list_append(m, action);

      action = purple_menu_action_new(
          "Moderate In Room...", PURPLE_CALLBACK(menu_action_moderate_buddy_cb),
          node, NULL);
      m = g_list_append(m, action);

      action = purple_menu_action_new(
          "My Matrix Profile...",
          PURPLE_CALLBACK(menu_action_my_profile_blist_cb), node, NULL);
      m = g_list_append(m, action);
    }
  }
  return m;
}

void matrix_roomlist_expand_category(PurpleRoomlist *list,
                                     PurpleRoomlistRoom *category) {
  PurpleAccount *account = find_matrix_account();
  if (!account)
    return;
  const char *user_id = purple_account_get_username(account);
  GList *fields = purple_roomlist_room_get_fields(category);
  if (fields && fields->data) {
    const char *space_id = (const char *)fields->data;
    purple_matrix_rust_get_hierarchy(user_id, space_id);
  }
}
