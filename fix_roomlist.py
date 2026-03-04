import re

# Update matrix_blist.h
with open("plugin_src/matrix_blist.h", "r") as f:
    h_cont = f.read()

h_cont = h_cont.replace(
    "void roomlist_add_cb(const char *user_id, const char *name, const char *id,\n                     const char *topic, guint64 count);",
    "void roomlist_add_cb(const char *user_id, const char *name, const char *id,\n                     const char *topic, guint64 count, gboolean is_space);\nvoid matrix_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *category);"
)
with open("plugin_src/matrix_blist.h", "w") as f:
    f.write(h_cont)

# Update matrix_blist.c
with open("plugin_src/matrix_blist.c", "r") as f:
    c_cont = f.read()

c_cont = c_cont.replace(
    "  guint64 count;\n} RoomListData;",
    "  guint64 count;\n  gboolean is_space;\n} RoomListData;"
)

c_cont = c_cont.replace(
    "void roomlist_add_cb(const char *user_id, const char *name, const char *id,\n                     const char *topic, guint64 count) {",
    "void roomlist_add_cb(const char *user_id, const char *name, const char *id,\n                     const char *topic, guint64 count, gboolean is_space) {"
)

c_cont = c_cont.replace(
    "  d->count = count;\n  g_idle_add",
    "  d->count = count;\n  d->is_space = is_space;\n  g_idle_add"
)

old_add = """  if (active_roomlist) {
    PurpleRoomlistRoom *room =
        purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, d->name, NULL);"""
new_add = """  if (active_roomlist) {
    PurpleRoomlistRoomType type = d->is_space ? PURPLE_ROOMLIST_ROOMTYPE_CATEGORY : PURPLE_ROOMLIST_ROOMTYPE_ROOM;
    PurpleRoomlistRoom *room =
        purple_roomlist_room_new(type, d->name, NULL);"""
c_cont = c_cont.replace(old_add, new_add)

expand_impl = """
void matrix_roomlist_expand_category(PurpleRoomlist *list, PurpleRoomlistRoom *category) {
    PurpleAccount *account = purple_roomlist_get_account(list);
    const char *user_id = purple_account_get_username(account);
    GList *fields = purple_roomlist_room_get_fields(category);
    if (fields && fields->data) {
        const char *space_id = (const char *)fields->data;
        purple_matrix_rust_get_space_hierarchy(user_id, space_id);
    }
}
"""
c_cont += expand_impl

with open("plugin_src/matrix_blist.c", "w") as f:
    f.write(c_cont)

# Update matrix_core.c
with open("plugin_src/matrix_core.c", "r") as f:
    core_cont = f.read()

core_cont = core_cont.replace(
    "      roomlist_add_cb(s->user_id, s->name, s->room_id, s->topic,\n                      s->member_count);",
    "      roomlist_add_cb(s->user_id, s->name, s->room_id, s->topic,\n                      s->member_count, s->is_space);"
)

core_cont = core_cont.replace(
    "    .roomlist_cancel = matrix_roomlist_cancel,",
    "    .roomlist_cancel = matrix_roomlist_cancel,\n    .roomlist_expand_category = matrix_roomlist_expand_category,"
)

with open("plugin_src/matrix_core.c", "w") as f:
    f.write(core_cont)

