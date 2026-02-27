// pidgin-matrix-ui/pidgin_matrix_ui.c
#define PURPLE_PLUGINS
#include <glib.h>
#include <libpurple/plugin.h>
#include <libpurple/version.h>
#include <libpurple/debug.h>
#include <libpurple/util.h>
#include <libpurple/signals.h>
#include <libpurple/conversation.h>
#include <libpurple/account.h>
#include <pidgin/pidgin.h>
#include <pidgin/gtkconv.h>
#include <gtk/gtk.h>
#include <string.h>

#define MATRIX_UI_ACTION_BAR_KEY "matrix-ui-action-bar"
#define MATRIX_UI_TYPING_LABEL_KEY "matrix-ui-typing-label"
#define MATRIX_UI_ENCRYPTED_LABEL_KEY "matrix-ui-encrypted-label"
#define MATRIX_UI_MUTED_LABEL_KEY "matrix-ui-muted-label"
#define MATRIX_UI_ACTIVITY_LABEL_KEY "matrix-ui-activity-label"
#define MATRIX_UI_USERLIST_HOOKED_KEY "matrix-ui-userlist-hooked"

typedef struct {
    PurpleConversation *conv;
    char *signal_name;
} SignalIdleData;

typedef struct {
    char *signal_name;
    char *payload;
} GlobalSignalIdleData;

typedef struct {
    char *room_id;
    char *user_id;
    char *signal_name;
} RoomUserActionData;

/* Forward Declarations */
static void on_menu_dash(gpointer d);
static void on_menu_reply(gpointer d);
static void on_menu_start_thread(gpointer d);
static void on_menu_threads(gpointer d);
static void on_menu_sticker(gpointer d);
static void on_menu_poll(gpointer d);
static void on_menu_clear_session(gpointer d);
static void on_menu_show_members(gpointer d);
static void on_menu_moderate(gpointer d);
static void on_menu_kick(gpointer d);
static void on_menu_ban(gpointer d);
static void on_menu_unban(gpointer d);
static void on_menu_leave_room(gpointer d);
static void on_menu_verify_self(gpointer d);
static void on_menu_crypto_status(gpointer d);
static void on_menu_list_devices(gpointer d);
static void on_menu_room_settings(gpointer d);
static void on_menu_invite_user(gpointer d);
static void on_menu_send_file(gpointer d);
static void on_menu_mark_unread(gpointer d);
static void on_menu_mark_read(gpointer d);
static void on_menu_mute_room(gpointer d);
static void on_menu_unmute_room(gpointer d);
static void on_menu_search_room(gpointer d);
static void on_menu_list_polls(gpointer d);
static void on_menu_react(gpointer d);
static void on_menu_send_location(gpointer d);
static void on_menu_who_read(gpointer d);
static void on_menu_report_event(gpointer d);
static void on_menu_message_inspector(gpointer d);
static void on_menu_show_last_event_details(gpointer d);
static void on_menu_reply_pick_event(gpointer d);
static void on_menu_thread_pick_event(gpointer d);
static void on_menu_react_pick_event(gpointer d);
static void on_menu_edit_pick_event(gpointer d);
static void on_menu_redact_pick_event(gpointer d);
static void on_menu_report_pick_event(gpointer d);
static void on_menu_redact_event(gpointer d);
static void on_menu_edit_event(gpointer d);
static void on_menu_react_latest(gpointer d);
static void on_menu_edit_latest(gpointer d);
static void on_menu_redact_latest(gpointer d);
static void on_menu_report_latest(gpointer d);
static void on_menu_versions(gpointer d);
static void on_menu_enable_key_backup(gpointer d);
static void on_menu_my_profile(gpointer d);
static void on_menu_server_info(gpointer d);
static void on_menu_resync_recent(gpointer d);
static void on_menu_search_stickers(gpointer d);
static void on_menu_recover_keys(gpointer d);
static void on_menu_export_keys(gpointer d);
static void on_menu_set_avatar(gpointer d);
static void on_menu_room_tag(gpointer d);
static void on_menu_upgrade_room(gpointer d);
static void on_menu_alias_create(gpointer d);
static void on_menu_alias_delete(gpointer d);
static void on_menu_space_hierarchy(gpointer d);
static void on_menu_user_info(gpointer d);
static void on_menu_ignore_user(gpointer d);
static void on_menu_unignore_user(gpointer d);
static void on_action_login_password(PurplePluginAction *action);
static void on_action_login_sso(PurplePluginAction *action);
static void on_action_set_account_defaults(PurplePluginAction *action);
static void on_action_clear_session(PurplePluginAction *action);
static void on_action_discover_join(PurplePluginAction *action);
static void on_action_discover_knock(PurplePluginAction *action);
static void on_action_discover_search_users(PurplePluginAction *action);
static void on_action_discover_search_public(PurplePluginAction *action);
static void on_action_discover_search_members_global(PurplePluginAction *action);
static void on_action_discover_search_room_global(PurplePluginAction *action);
static void on_action_discover_browse_public_rooms(PurplePluginAction *action);
static void on_action_discover_preview_room(PurplePluginAction *action);
static void on_action_my_profile(PurplePluginAction *action);
static void on_action_server_info(PurplePluginAction *action);
static void on_action_recover_keys(PurplePluginAction *action);
static void on_action_export_keys(PurplePluginAction *action);
static void on_action_set_avatar(PurplePluginAction *action);
static GList *ui_actions(PurplePlugin *plugin, gpointer context);
static gboolean hook_chat_user_list_idle_cb(gpointer data);
static void emit_matrix_signal(PurpleConversation *conv, const char *signal_name);
static gboolean on_activity_label_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data);

static gboolean
is_conv_room_muted(PurpleConversation *conv)
{
    const char *v = NULL;
    if (!conv) return FALSE;
    v = purple_conversation_get_data(conv, "matrix_room_muted");
    return (v && strcmp(v, "1") == 0);
}

static gboolean
on_activity_label_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    PurpleConversation *conv = (PurpleConversation *)data;
    (void)widget;
    if (!conv || !event || event->type != GDK_BUTTON_PRESS) return FALSE;
    if (event->button == 1) {
        emit_matrix_signal(conv, "matrix-ui-action-show-last-event-details");
        return TRUE;
    }
    if (event->button == 3) {
        emit_matrix_signal(conv, "matrix-ui-action-message-inspector");
        return TRUE;
    }
    return FALSE;
}

static gboolean
emit_signal_idle_cb(gpointer data)
{
    SignalIdleData *sd = (SignalIdleData *)data;
    if (g_list_find(purple_get_conversations(), sd->conv)) {
        const char *room_id = purple_conversation_get_name(sd->conv);
        PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
        if (matrix_plugin && purple_plugin_is_loaded(matrix_plugin)) {
            purple_debug_info("matrix-ui", "emit_signal_idle_cb: Emitting signal %s for room %s\n", sd->signal_name, room_id);
            purple_signal_emit(matrix_plugin, sd->signal_name, room_id);
        } else {
            purple_debug_warning("matrix-ui", "emit_signal_idle_cb: Matrix plugin not found or not loaded (signal %s failed)\n", sd->signal_name);
        }
    }
    g_free(sd->signal_name);
    g_free(sd);
    return FALSE;
}

static gboolean
emit_global_signal_idle_cb(gpointer data)
{
    GlobalSignalIdleData *sd = (GlobalSignalIdleData *)data;
    PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
    if (matrix_plugin && purple_plugin_is_loaded(matrix_plugin)) {
        purple_debug_info("matrix-ui", "emit_global_signal_idle_cb: Emitting signal %s\n", sd->signal_name);
        purple_signal_emit(matrix_plugin, sd->signal_name, sd->payload ? sd->payload : "");
    } else {
        purple_debug_warning("matrix-ui", "emit_global_signal_idle_cb: Matrix plugin not found or not loaded (signal %s failed)\n", sd->signal_name);
    }
    g_free(sd->signal_name);
    g_free(sd->payload);
    g_free(sd);
    return FALSE;
}

static void
emit_matrix_signal(PurpleConversation *conv, const char *signal_name)
{
    if (!conv || !signal_name) return;
    SignalIdleData *sd = g_new0(SignalIdleData, 1);
    sd->conv = conv;
    sd->signal_name = g_strdup(signal_name);
    g_idle_add(emit_signal_idle_cb, sd);
}

static void
emit_matrix_global_signal(const char *signal_name, const char *payload)
{
    if (!signal_name) return;
    GlobalSignalIdleData *sd = g_new0(GlobalSignalIdleData, 1);
    sd->signal_name = g_strdup(signal_name);
    sd->payload = g_strdup(payload ? payload : "");
    g_idle_add(emit_global_signal_idle_cb, sd);
}

static void
emit_matrix_room_user_signal(const char *signal_name, const char *room_id, const char *user_id)
{
    char *payload = NULL;
    if (!signal_name || !room_id || !*room_id || !user_id || !*user_id) return;
    payload = g_strdup_printf("%s\n%s", room_id, user_id);
    emit_matrix_global_signal(signal_name, payload);
    g_free(payload);
}

static PurpleAccount *
find_matrix_account(void)
{
    GList *accounts = purple_accounts_get_all();
    for (GList *l = accounts; l != NULL; l = l->next) {
        PurpleAccount *account = (PurpleAccount *)l->data;
        if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            if (purple_account_is_connected(account)) return account;
        }
    }
    for (GList *l = accounts; l != NULL; l = l->next) {
        PurpleAccount *account = (PurpleAccount *)l->data;
        if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            return account;
        }
    }
    return NULL;
}

static void on_matrix_moderation_actions_clicked(GtkWidget *b, PurpleConversation *c);
static void on_matrix_admin_actions_clicked(GtkWidget *b, PurpleConversation *c);
static void on_matrix_message_actions_clicked(GtkWidget *b, PurpleConversation *c);
static void on_matrix_actions_clicked(GtkWidget *b, PurpleConversation *c);

static void on_menu_dash(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-dashboard"); }
static void on_menu_reply(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply"); }
static void on_menu_start_thread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-start-thread"); }
static void on_menu_threads(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-threads"); }
static void on_menu_sticker(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-sticker"); }
static void on_menu_poll(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-poll"); }
static void on_menu_clear_session(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-clear-session"); }
static void on_menu_show_members(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-members"); }
static void on_menu_moderate(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-moderate"); }
static void on_menu_kick(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-kick"); }
static void on_menu_ban(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-ban"); }
static void on_menu_unban(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-unban"); }
static void on_menu_leave_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-leave-room"); }
static void on_menu_verify_self(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-verify-self"); }
static void on_menu_crypto_status(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-crypto-status"); }
static void on_menu_list_devices(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-devices"); }
static void on_menu_room_settings(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-room-settings"); }
static void on_menu_invite_user(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-invite-user"); }
static void on_menu_send_file(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-send-file"); }
static void on_menu_mark_unread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mark-unread"); }
static void on_menu_mark_read(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mark-read"); }
static void on_menu_mute_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mute-room"); }
static void on_menu_unmute_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-unmute-room"); }
static void on_menu_search_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-search-room"); }
static void on_menu_list_polls(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-polls"); }
static void on_menu_react(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-react"); }
static void on_menu_send_location(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-send-location"); }
static void on_menu_who_read(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-who-read"); }
static void on_menu_report_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-report-event"); }
static void on_menu_message_inspector(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-message-inspector"); }
static void on_menu_show_last_event_details(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-last-event-details"); }
static void on_menu_reply_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply-pick-event"); }
static void on_menu_thread_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-thread-pick-event"); }
static void on_menu_react_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-react-pick-event"); }
static void on_menu_edit_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-edit-pick-event"); }
static void on_menu_redact_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-redact-pick-event"); }
static void on_menu_report_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-report-pick-event"); }
static void on_menu_redact_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-redact-event"); }
static void on_menu_edit_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-edit-event"); }
static void on_menu_react_latest(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-react-latest"); }
static void on_menu_edit_latest(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-edit-latest"); }
static void on_menu_redact_latest(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-redact-latest"); }
static void on_menu_report_latest(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-report-latest"); }
static void on_menu_versions(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-versions"); }
static void on_menu_enable_key_backup(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-enable-key-backup"); }
static void on_menu_my_profile(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-my-profile"); }
static void on_menu_server_info(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-server-info"); }
static void on_menu_resync_recent(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-resync-recent"); }
static void on_menu_search_stickers(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-search-stickers"); }
static void on_menu_recover_keys(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-recover-keys"); }
static void on_menu_export_keys(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-export-keys"); }
static void on_menu_set_avatar(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-set-avatar"); }
static void on_menu_room_tag(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-room-tag"); }
static void on_menu_upgrade_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-upgrade-room"); }
static void on_menu_alias_create(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-alias-create"); }
static void on_menu_alias_delete(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-alias-delete"); }
static void on_menu_space_hierarchy(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-space-hierarchy"); }

static void on_menu_user_info(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-user-info"); }
static void on_menu_ignore_user(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-ignore-user"); }
static void on_menu_unignore_user(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-unignore-user"); }

static void room_user_action_data_free(RoomUserActionData *d) {
    if (!d) return;
    g_free(d->room_id);
    g_free(d->user_id);
    g_free(d->signal_name);
    g_free(d);
}

static void on_chat_user_action_activate(GtkMenuItem *item, gpointer data) {
    RoomUserActionData *d = (RoomUserActionData *)data;
    if (!d) return;
    emit_matrix_room_user_signal(d->signal_name, d->room_id, d->user_id);
}

static gboolean
chat_user_list_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    PurpleConversation *conv = (PurpleConversation *)data;
    GtkTreeView *view = NULL;
    GtkTreePath *path = NULL;
    GtkTreeViewColumn *column = NULL;
    GtkTreeIter iter;
    GtkTreeModel *model = NULL;
    GtkWidget *menu = NULL;
    GtkWidget *item = NULL;
    char *user_id = NULL;
    const char *room_id = NULL;

    if (!conv || !event || event->type != GDK_BUTTON_PRESS || event->button != 3)
        return FALSE;
    if (!GTK_IS_TREE_VIEW(widget))
        return FALSE;
    view = GTK_TREE_VIEW(widget);
    if (!gtk_tree_view_get_path_at_pos(view, (gint)event->x, (gint)event->y, &path,
                                       &column, NULL, NULL))
        return FALSE;

    model = gtk_tree_view_get_model(view);
    if (!model || !gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_path_free(path);
        return FALSE;
    }
    gtk_tree_view_set_cursor(view, path, column, FALSE);
    gtk_tree_model_get(model, &iter, CHAT_USERS_NAME_COLUMN, &user_id, -1);
    gtk_tree_path_free(path);

    room_id = purple_conversation_get_name(conv);
    if (!room_id || !*room_id || !user_id || !*user_id) {
        g_free(user_id);
        return FALSE;
    }

    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label("Matrix User Info");
    RoomUserActionData *info_data = g_new0(RoomUserActionData, 1);
    info_data->room_id = g_strdup(room_id);
    info_data->user_id = g_strdup(user_id);
    info_data->signal_name = g_strdup("matrix-ui-action-user-info");
    g_signal_connect_data(item, "activate", G_CALLBACK(on_chat_user_action_activate),
                          info_data, (GClosureNotify)room_user_action_data_free, 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Moderate User");
    RoomUserActionData *mod_data = g_new0(RoomUserActionData, 1);
    mod_data->room_id = g_strdup(room_id);
    mod_data->user_id = g_strdup(user_id);
    mod_data->signal_name = g_strdup("matrix-ui-action-moderate-user");
    g_signal_connect_data(item, "activate", G_CALLBACK(on_chat_user_action_activate),
                          mod_data, (GClosureNotify)room_user_action_data_free, 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Ignore User");
    RoomUserActionData *ignore_data = g_new0(RoomUserActionData, 1);
    ignore_data->room_id = g_strdup(room_id);
    ignore_data->user_id = g_strdup(user_id);
    ignore_data->signal_name = g_strdup("matrix-ui-action-ignore-user");
    g_signal_connect_data(item, "activate", G_CALLBACK(on_chat_user_action_activate),
                          ignore_data, (GClosureNotify)room_user_action_data_free, 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Unignore User");
    RoomUserActionData *unignore_data = g_new0(RoomUserActionData, 1);
    unignore_data->room_id = g_strdup(room_id);
    unignore_data->user_id = g_strdup(user_id);
    unignore_data->signal_name = g_strdup("matrix-ui-action-unignore-user");
    g_signal_connect_data(item, "activate", G_CALLBACK(on_chat_user_action_activate),
                          unignore_data, (GClosureNotify)room_user_action_data_free, 0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
    g_free(user_id);
    return TRUE;
}
static void on_action_login_password(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    emit_matrix_global_signal("matrix-ui-action-login-password",
                              account ? purple_account_get_username(account) : "");
}
static void on_action_login_sso(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    emit_matrix_global_signal("matrix-ui-action-login-sso",
                              account ? purple_account_get_username(account) : "");
}
static void on_action_set_account_defaults(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    emit_matrix_global_signal("matrix-ui-action-set-account-defaults",
                              account ? purple_account_get_username(account) : "");
}
static void on_action_clear_session(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    emit_matrix_global_signal("matrix-ui-action-clear-session",
                              account ? purple_account_get_username(account) : "");
}

static void on_action_discover_join(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-join-room", "");
}

static void on_action_discover_knock(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-knock", "");
}

static void on_action_discover_search_users(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-search-users", "");
}

static void on_action_discover_search_public(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-search-public", "");
}

static void on_action_discover_search_members_global(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-search-members-global", "");
}

static void on_action_discover_search_room_global(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-search-room-global", "");
}

static void on_action_discover_browse_public_rooms(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-discover-public-rooms", "");
}

static void on_action_discover_preview_room(PurplePluginAction *action) {
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-discover-room-preview", "");
}

static void on_action_my_profile(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-my-profile",
                              account ? purple_account_get_username(account) : "");
}

static void on_action_server_info(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-server-info",
                              account ? purple_account_get_username(account) : "");
}

static void on_action_recover_keys(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-recover-keys",
                              account ? purple_account_get_username(account) : "");
}

static void on_action_export_keys(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-export-keys",
                              account ? purple_account_get_username(account) : "");
}

static void on_action_set_avatar(PurplePluginAction *action) {
    PurpleAccount *account = find_matrix_account();
    (void)action;
    emit_matrix_global_signal("matrix-ui-action-set-avatar",
                              account ? purple_account_get_username(account) : "");
}

static void on_matrix_moderation_actions_clicked(GtkWidget *b, PurpleConversation *c) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Kick User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_kick), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Ban User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_ban), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Unban User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_unban), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Ignore User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_ignore_user), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Unignore User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_unignore_user), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Advanced Moderation...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_moderate), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static void on_matrix_admin_actions_clicked(GtkWidget *b, PurpleConversation *c) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Server Versions");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_versions), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Server Info");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_server_info), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("My Profile");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_my_profile), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Set Avatar");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_set_avatar), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Enable Key Backup");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_enable_key_backup), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Recover Keys");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_recover_keys), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Export Keys");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_export_keys), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Recover Keys");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_recover_keys), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Export Keys");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_export_keys), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Set Avatar");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_set_avatar), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("My Profile");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_my_profile), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Server Info");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_server_info), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Set Room Tag");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_room_tag), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Upgrade Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_upgrade_room), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Create Alias");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_alias_create), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Delete Alias");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_alias_delete), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Space Hierarchy");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_space_hierarchy), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static void on_matrix_message_actions_clicked(GtkWidget *b, PurpleConversation *c) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Message Inspector...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_message_inspector), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Show Last Event Details");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_show_last_event_details), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("React to Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_react_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Edit Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_edit_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Redact Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_redact_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Report Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_report_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Add Reaction (Event ID)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_react), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Reply (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_reply_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Thread (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_thread_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("React (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_react_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Edit (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_edit_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Redact (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_redact_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Report (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_report_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Edit Event");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_edit_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Redact Event");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_redact_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Report Event");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_report_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Re-sync Recent");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_resync_recent), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Search Stickers");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_search_stickers), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static void on_matrix_actions_clicked(GtkWidget *b, PurpleConversation *c) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Room Dashboard");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_dash), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Start Thread");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_start_thread), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Show Members");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_show_members), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("User Info");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_user_info), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Ignore User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_ignore_user), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Unignore User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_unignore_user), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Moderate Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_moderate), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Kick User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_kick), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Ban User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_ban), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Unban User...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_unban), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Invite User");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_invite_user), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Send File");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_send_file), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Search in Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_search_room), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Message Inspector...");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_message_inspector), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Re-sync Recent");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_resync_recent), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("List Polls");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_list_polls), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Add Reaction");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_react), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Reply (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_reply_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Thread (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_thread_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("React (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_react_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Search Stickers");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_search_stickers), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("React to Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_react_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Send Location");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_send_location), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Who Read");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_who_read), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Report Event");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_report_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Edit Event");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_edit_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Edit (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_edit_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Edit Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_edit_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Redact Event");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_redact_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Redact (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_redact_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Redact Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_redact_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Report Latest");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_report_latest), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Report (Pick Event)");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_report_pick_event), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Server Versions");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_versions), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Enable Key Backup");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_enable_key_backup), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Set Room Tag");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_room_tag), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Upgrade Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_upgrade_room), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Create Alias");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_alias_create), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Delete Alias");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_alias_delete), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Space Hierarchy");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_space_hierarchy), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Room Settings");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_room_settings), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Verify This Device");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_verify_self), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Crypto Status");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_crypto_status), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("List My Devices");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_list_devices), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Mark Unread");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_mark_unread), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Mark Read");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_mark_read), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    if (is_conv_room_muted(c)) {
        item = gtk_menu_item_new_with_label("Unmute Room");
        g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_unmute_room), c);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    } else {
        item = gtk_menu_item_new_with_label("Mute Room");
        g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_mute_room), c);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    }

    item = gtk_menu_item_new_with_label("Clear Session Cache");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_clear_session), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Leave Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_leave_room), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static GList *
ui_actions(PurplePlugin *plugin, gpointer context)
{
    GList *m = NULL;
    m = g_list_append(m, purple_plugin_action_new("Login (Password)...", on_action_login_password));
    m = g_list_append(m, purple_plugin_action_new("Login (SSO)...", on_action_login_sso));
    m = g_list_append(m, purple_plugin_action_new("Set Homeserver / Username...", on_action_set_account_defaults));
    m = g_list_append(m, purple_plugin_action_new("Discover: Join Room...", on_action_discover_join));
    m = g_list_append(m, purple_plugin_action_new("Discover: Knock Room...", on_action_discover_knock));
    m = g_list_append(m, purple_plugin_action_new("Discover: Browse Public Rooms...", on_action_discover_browse_public_rooms));
    m = g_list_append(m, purple_plugin_action_new("Discover: Preview Room/Alias...", on_action_discover_preview_room));
    m = g_list_append(m, purple_plugin_action_new("Discover: Search Users...", on_action_discover_search_users));
    m = g_list_append(m, purple_plugin_action_new("Discover: Search Public Rooms...", on_action_discover_search_public));
    m = g_list_append(m, purple_plugin_action_new("Discover: Search Members in Room...", on_action_discover_search_members_global));
    m = g_list_append(m, purple_plugin_action_new("Discover: Search Messages in Room...", on_action_discover_search_room_global));
    m = g_list_append(m, purple_plugin_action_new("Account: My Profile", on_action_my_profile));
    m = g_list_append(m, purple_plugin_action_new("Account: Set Avatar...", on_action_set_avatar));
    m = g_list_append(m, purple_plugin_action_new("Server: Info", on_action_server_info));
    m = g_list_append(m, purple_plugin_action_new("Security: Recover Keys...", on_action_recover_keys));
    m = g_list_append(m, purple_plugin_action_new("Security: Export Keys...", on_action_export_keys));
    m = g_list_append(m, purple_plugin_action_new("Clear Session Cache...", on_action_clear_session));
    return m;
}

static void
handle_room_typing_cb(const char *room_id, const char *who, gboolean is_typing, gpointer data)
{
    GList *convs = purple_get_conversations();
    for (GList *it = convs; it; it = it->next) {
        PurpleConversation *conv = (PurpleConversation *)it->data;
        PurpleAccount *account = purple_conversation_get_account(conv);
        if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            if (strcmp(purple_conversation_get_name(conv), room_id) == 0) {
                GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_TYPING_LABEL_KEY);
                if (label && GTK_IS_LABEL(label)) {
                    if (is_typing) {
                        char *txt = g_strdup_printf("<span size='smaller' color='#666'><i>%s is typing...</i></span>", who);
                        gtk_label_set_markup(GTK_LABEL(label), txt);
                        g_free(txt);
                    } else {
                        gtk_label_set_text(GTK_LABEL(label), "");
                    }
                }
            }
        }
    }
}

static void
handle_room_encrypted_cb(const char *room_id, gboolean is_encrypted, gpointer data)
{
    GList *convs = purple_get_conversations();
    for (GList *it = convs; it; it = it->next) {
        PurpleConversation *conv = (PurpleConversation *)it->data;
        PurpleAccount *account = purple_conversation_get_account(conv);
        if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            if (strcmp(purple_conversation_get_name(conv), room_id) == 0) {
                GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_ENCRYPTED_LABEL_KEY);
                if (label && GTK_IS_LABEL(label)) {
                    if (is_encrypted) {
                        gtk_label_set_markup(GTK_LABEL(label), "<span color='darkgreen'>🔒</span>");
                        /* Use Pidgin's toolkit for the tooltip if possible, or just standard GTK */
                        gtk_widget_set_tooltip_text(label, "This room is End-to-End Encrypted.");
                        gtk_widget_set_has_tooltip(label, TRUE);
                    } else {
                        gtk_label_set_text(GTK_LABEL(label), "");
                    }
                }
            }
        }
    }
}

static void
handle_room_muted_cb(const char *room_id, gboolean muted, gpointer data)
{
    GList *convs = purple_get_conversations();
    for (GList *it = convs; it; it = it->next) {
        PurpleConversation *conv = (PurpleConversation *)it->data;
        PurpleAccount *account = purple_conversation_get_account(conv);
        if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            const char *cid = purple_conversation_get_name(conv);
            if (cid && (strcmp(cid, room_id) == 0 ||
                        (g_str_has_prefix(cid, room_id) && cid[strlen(room_id)] == '|'))) {
                GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_MUTED_LABEL_KEY);
                if (label && GTK_IS_LABEL(label)) {
                    if (muted) {
                        gtk_label_set_markup(GTK_LABEL(label), "<span color='#aa6600'>M</span>");
                        gtk_widget_set_tooltip_text(label, "This room is muted.");
                        gtk_widget_set_has_tooltip(label, TRUE);
                    } else {
                        gtk_label_set_text(GTK_LABEL(label), "");
                    }
                }
            }
        }
    }
}

static void
handle_room_activity_cb(const char *room_id, const char *sender, const char *snippet, gpointer data)
{
    GList *convs = purple_get_conversations();
    for (GList *it = convs; it; it = it->next) {
        PurpleConversation *conv = (PurpleConversation *)it->data;
        PurpleAccount *account = purple_conversation_get_account(conv);
        if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            if (strcmp(purple_conversation_get_name(conv), room_id) == 0) {
                GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_ACTIVITY_LABEL_KEY);
                if (label && GTK_IS_LABEL(label)) {
                    char *msg = NULL;
                    char *safe = NULL;
                    safe = g_strdup(snippet ? snippet : "");
                    if ((int)strlen(safe) > 80) safe[80] = '\0';
                    msg = g_strdup_printf("<span size='smaller' color='#666'>Last: %s: %s</span>",
                                          sender ? sender : "user", safe);
                    gtk_label_set_markup(GTK_LABEL(label), msg);
                    g_free(msg);
                    g_free(safe);
                }
            }
        }
    }
}

static gboolean
hook_chat_user_list_idle_cb(gpointer data)
{
    PurpleConversation *conv = (PurpleConversation *)data;
    PidginConversation *gtkconv = NULL;
    GtkWidget *list = NULL;

    if (!g_list_find(purple_get_conversations(), conv))
        return FALSE;
    if (purple_conversation_get_data(conv, MATRIX_UI_USERLIST_HOOKED_KEY))
        return FALSE;

    gtkconv = PIDGIN_CONVERSATION(conv);
    if (!gtkconv || !gtkconv->u.chat || !gtkconv->u.chat->list)
        return TRUE;
    list = gtkconv->u.chat->list;
    if (!GTK_IS_TREE_VIEW(list))
        return FALSE;

    g_signal_connect(list, "button-press-event",
                     G_CALLBACK(chat_user_list_button_press_cb), conv);
    purple_conversation_set_data(conv, MATRIX_UI_USERLIST_HOOKED_KEY,
                                 GINT_TO_POINTER(1));
    return FALSE;
}

static gboolean
inject_action_bar_idle_cb(gpointer data)
{
    PurpleConversation *conv = (PurpleConversation *)data;
    if (!g_list_find(purple_get_conversations(), conv)) return FALSE;
    if (purple_conversation_get_data(conv, MATRIX_UI_ACTION_BAR_KEY)) return FALSE;

    PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
    if (!gtkconv || !gtkconv->lower_hbox || !GTK_IS_WIDGET(gtkconv->lower_hbox)) {
        purple_debug_info("matrix-ui", "inject_action_bar_idle_cb: lower_hbox not ready yet, retrying...\n");
        return TRUE; 
    }
    
    if (!GTK_WIDGET_REALIZED(gtkconv->lower_hbox)) {
        purple_debug_info("matrix-ui", "inject_action_bar_idle_cb: lower_hbox not realized yet, retrying...\n");
        return TRUE; 
    }

    purple_debug_info("matrix-ui", "inject_action_bar_idle_cb: Injecting action bar into room %s\n", purple_conversation_get_name(conv));

    GtkWidget *vbox = gtk_widget_get_parent(gtkconv->lower_hbox);
    if (!vbox || !GTK_IS_VBOX(vbox)) return FALSE;

    GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
    
    /* Encryption / Mute Status */
    GtkWidget *enc_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), enc_label, FALSE, FALSE, 5);
    purple_conversation_set_data(conv, MATRIX_UI_ENCRYPTED_LABEL_KEY, enc_label);
    GtkWidget *muted_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), muted_label, FALSE, FALSE, 2);
    purple_conversation_set_data(conv, MATRIX_UI_MUTED_LABEL_KEY, muted_label);
    if (is_conv_room_muted(conv)) {
        gtk_label_set_markup(GTK_LABEL(muted_label), "<span color='#aa6600'>M</span>");
        gtk_widget_set_tooltip_text(muted_label, "This room is muted.");
        gtk_widget_set_has_tooltip(muted_label, TRUE);
    }
    GtkTooltips *tooltips = gtk_tooltips_new();
    GtkWidget *reply_btn = gtk_button_new_with_label("Reply");
    GtkWidget *thread_start_btn = gtk_button_new_with_label("Thread+");
    GtkWidget *threads_btn = gtk_button_new_with_label("Threads");
    GtkWidget *poll_btn = gtk_button_new_with_label("Poll+");
    GtkWidget *polls_btn = gtk_button_new_with_label("Polls");
    GtkWidget *msg_btn = gtk_button_new_with_label("Msg");
    GtkWidget *admin_btn = gtk_button_new_with_label("Admin");
    GtkWidget *moderate_btn = gtk_button_new_with_label("Moderate");
    GtkWidget *more_btn = gtk_button_new_with_label("More");
    GtkWidget *btns[] = {reply_btn, thread_start_btn, threads_btn, poll_btn, polls_btn, msg_btn, admin_btn, moderate_btn, more_btn};

    for (guint i = 0; i < G_N_ELEMENTS(btns); i++) {
        GtkWidget *btn = btns[i];
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        GtkWidget *lbl = gtk_bin_get_child(GTK_BIN(btn));
        if (GTK_IS_LABEL(lbl)) {
            PangoFontDescription *fd = pango_font_description_from_string("sans bold 8");
            gtk_widget_modify_font(lbl, fd);
            pango_font_description_free(fd);
        }
        gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    }

    gtk_tooltips_set_tip(tooltips, reply_btn, "Reply to latest event", NULL);
    gtk_tooltips_set_tip(tooltips, thread_start_btn, "Start a new thread from latest event", NULL);
    gtk_tooltips_set_tip(tooltips, threads_btn, "Show room threads", NULL);
    gtk_tooltips_set_tip(tooltips, poll_btn, "Create a poll", NULL);
    gtk_tooltips_set_tip(tooltips, polls_btn, "List polls and vote", NULL);
    gtk_tooltips_set_tip(tooltips, msg_btn, "React/edit/redact/report actions", NULL);
    gtk_tooltips_set_tip(tooltips, admin_btn, "Room admin and server actions", NULL);
    gtk_tooltips_set_tip(tooltips, moderate_btn, "Kick, ban, or unban users", NULL);
    gtk_tooltips_set_tip(tooltips, more_btn, "More Matrix actions", NULL);

    g_signal_connect_swapped(reply_btn, "clicked", G_CALLBACK(on_menu_reply), conv);
    g_signal_connect_swapped(thread_start_btn, "clicked", G_CALLBACK(on_menu_start_thread), conv);
    g_signal_connect_swapped(threads_btn, "clicked", G_CALLBACK(on_menu_threads), conv);
    g_signal_connect_swapped(poll_btn, "clicked", G_CALLBACK(on_menu_poll), conv);
    g_signal_connect_swapped(polls_btn, "clicked", G_CALLBACK(on_menu_list_polls), conv);
    g_signal_connect(msg_btn, "clicked", G_CALLBACK(on_matrix_message_actions_clicked), conv);
    g_signal_connect(admin_btn, "clicked", G_CALLBACK(on_matrix_admin_actions_clicked), conv);
    g_signal_connect(moderate_btn, "clicked", G_CALLBACK(on_matrix_moderation_actions_clicked), conv);
    g_signal_connect(more_btn, "clicked", G_CALLBACK(on_matrix_actions_clicked), conv);

    /* Last Activity Preview (left-click: details, right-click: inspector) */
    GtkWidget *activity_event_box = gtk_event_box_new();
    GtkWidget *activity_label = gtk_label_new("");
    gtk_container_add(GTK_CONTAINER(activity_event_box), activity_label);
    gtk_misc_set_alignment(GTK_MISC(activity_label), 0.0f, 0.5f);
    gtk_label_set_ellipsize(GTK_LABEL(activity_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(activity_label), 46);
    gtk_widget_set_tooltip_text(activity_event_box,
                                "Left click: show last event details. Right click: open Message Inspector.");
    g_signal_connect(activity_event_box, "button-press-event",
                     G_CALLBACK(on_activity_label_button_press_cb), conv);
    gtk_box_pack_start(GTK_BOX(hbox), activity_event_box, TRUE, TRUE, 8);
    purple_conversation_set_data(conv, MATRIX_UI_ACTIVITY_LABEL_KEY, activity_label);

    /* Typing Indicator */
    GtkWidget *typing_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), typing_label, FALSE, FALSE, 10);
    purple_conversation_set_data(conv, MATRIX_UI_TYPING_LABEL_KEY, typing_label);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show_all(hbox);
    purple_conversation_set_data(conv, MATRIX_UI_ACTION_BAR_KEY, hbox);
    return FALSE;
}

static void
conversation_created_cb(PurpleConversation *conv, gpointer data)
{
    PurpleAccount *account = purple_conversation_get_account(conv);
    if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
        g_timeout_add(1000, inject_action_bar_idle_cb, conv);
        g_timeout_add(1000, hook_chat_user_list_idle_cb, conv);
    }
}

static void
conversation_extended_menu_cb(PurpleConversation *conv, GList **list)
{
    PurpleAccount *account = purple_conversation_get_account(conv);
    if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
        PurpleMenuAction *matrix_menu = purple_menu_action_new("Matrix", NULL, NULL, NULL);
        GList *sub_actions = NULL;
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Room Dashboard", PURPLE_CALLBACK(on_menu_dash), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Reply to Latest", PURPLE_CALLBACK(on_menu_reply), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Reply (Pick Event)", PURPLE_CALLBACK(on_menu_reply_pick_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Start Thread", PURPLE_CALLBACK(on_menu_start_thread), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Start Thread (Pick Event)", PURPLE_CALLBACK(on_menu_thread_pick_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("View Threads", PURPLE_CALLBACK(on_menu_threads), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Send Sticker", PURPLE_CALLBACK(on_menu_sticker), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Create Poll", PURPLE_CALLBACK(on_menu_poll), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("User Info", PURPLE_CALLBACK(on_menu_user_info), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Ignore User...", PURPLE_CALLBACK(on_menu_ignore_user), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Unignore User...", PURPLE_CALLBACK(on_menu_unignore_user), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Moderate Room", PURPLE_CALLBACK(on_menu_moderate), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Kick User...", PURPLE_CALLBACK(on_menu_kick), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Ban User...", PURPLE_CALLBACK(on_menu_ban), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Unban User...", PURPLE_CALLBACK(on_menu_unban), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Show Members", PURPLE_CALLBACK(on_menu_show_members), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Invite User", PURPLE_CALLBACK(on_menu_invite_user), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Send File", PURPLE_CALLBACK(on_menu_send_file), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Search in Room", PURPLE_CALLBACK(on_menu_search_room), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Message Inspector...", PURPLE_CALLBACK(on_menu_message_inspector), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Show Last Event Details", PURPLE_CALLBACK(on_menu_show_last_event_details), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Re-sync Recent", PURPLE_CALLBACK(on_menu_resync_recent), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("List Polls", PURPLE_CALLBACK(on_menu_list_polls), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Add Reaction", PURPLE_CALLBACK(on_menu_react), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("React (Pick Event)", PURPLE_CALLBACK(on_menu_react_pick_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Search Stickers", PURPLE_CALLBACK(on_menu_search_stickers), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("React to Latest", PURPLE_CALLBACK(on_menu_react_latest), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Send Location", PURPLE_CALLBACK(on_menu_send_location), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Who Read", PURPLE_CALLBACK(on_menu_who_read), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Report Event", PURPLE_CALLBACK(on_menu_report_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Report (Pick Event)", PURPLE_CALLBACK(on_menu_report_pick_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Edit Event", PURPLE_CALLBACK(on_menu_edit_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Edit (Pick Event)", PURPLE_CALLBACK(on_menu_edit_pick_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Redact Event", PURPLE_CALLBACK(on_menu_redact_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Redact (Pick Event)", PURPLE_CALLBACK(on_menu_redact_pick_event), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Edit Latest", PURPLE_CALLBACK(on_menu_edit_latest), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Redact Latest", PURPLE_CALLBACK(on_menu_redact_latest), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Report Latest", PURPLE_CALLBACK(on_menu_report_latest), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Server Versions", PURPLE_CALLBACK(on_menu_versions), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Server Info", PURPLE_CALLBACK(on_menu_server_info), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("My Profile", PURPLE_CALLBACK(on_menu_my_profile), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Set Avatar", PURPLE_CALLBACK(on_menu_set_avatar), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Enable Key Backup", PURPLE_CALLBACK(on_menu_enable_key_backup), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Recover Keys", PURPLE_CALLBACK(on_menu_recover_keys), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Export Keys", PURPLE_CALLBACK(on_menu_export_keys), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Set Room Tag", PURPLE_CALLBACK(on_menu_room_tag), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Upgrade Room", PURPLE_CALLBACK(on_menu_upgrade_room), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Create Alias", PURPLE_CALLBACK(on_menu_alias_create), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Delete Alias", PURPLE_CALLBACK(on_menu_alias_delete), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Space Hierarchy", PURPLE_CALLBACK(on_menu_space_hierarchy), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Room Settings", PURPLE_CALLBACK(on_menu_room_settings), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Verify This Device", PURPLE_CALLBACK(on_menu_verify_self), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Crypto Status", PURPLE_CALLBACK(on_menu_crypto_status), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("List My Devices", PURPLE_CALLBACK(on_menu_list_devices), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Mark Unread", PURPLE_CALLBACK(on_menu_mark_unread), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Mark Read", PURPLE_CALLBACK(on_menu_mark_read), conv, NULL));
        if (is_conv_room_muted(conv)) {
            sub_actions = g_list_append(sub_actions, purple_menu_action_new("Unmute Room", PURPLE_CALLBACK(on_menu_unmute_room), conv, NULL));
        } else {
            sub_actions = g_list_append(sub_actions, purple_menu_action_new("Mute Room", PURPLE_CALLBACK(on_menu_mute_room), conv, NULL));
        }
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Leave Room", PURPLE_CALLBACK(on_menu_leave_room), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Clear Session Cache", PURPLE_CALLBACK(on_menu_clear_session), conv, NULL));
        matrix_menu->children = sub_actions;
        *list = g_list_append(*list, matrix_menu);
    }
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
    void *conv_handle = purple_conversations_get_handle();
    PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
    
    purple_signal_connect(conv_handle, "conversation-created", plugin, PURPLE_CALLBACK(conversation_created_cb), NULL);
    purple_signal_connect(conv_handle, "conversation-extended-menu", plugin, PURPLE_CALLBACK(conversation_extended_menu_cb), NULL);
    
    if (matrix_plugin) {
        purple_signal_connect(matrix_plugin, "matrix-ui-room-typing", plugin, PURPLE_CALLBACK(handle_room_typing_cb), NULL);
        purple_signal_connect(matrix_plugin, "matrix-ui-room-encrypted", plugin, PURPLE_CALLBACK(handle_room_encrypted_cb), NULL);
        purple_signal_connect(matrix_plugin, "matrix-ui-room-muted", plugin, PURPLE_CALLBACK(handle_room_muted_cb), NULL);
        purple_signal_connect(matrix_plugin, "matrix-ui-room-activity", plugin, PURPLE_CALLBACK(handle_room_activity_cb), NULL);
    }

    for (GList *it = purple_get_conversations(); it; it = it->next) conversation_created_cb((PurpleConversation *)it->data, NULL);
    return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
    for (GList *it = purple_get_conversations(); it != NULL; it = it->next) {
        PurpleConversation *conv = (PurpleConversation *)it->data;
        GtkWidget *bar = purple_conversation_get_data(conv, MATRIX_UI_ACTION_BAR_KEY);
        if (bar && GTK_IS_WIDGET(bar)) { gtk_widget_destroy(bar); purple_conversation_set_data(conv, MATRIX_UI_ACTION_BAR_KEY, NULL); }
    }
    return TRUE;
}

static PurplePluginInfo info = {
    .magic = PURPLE_PLUGIN_MAGIC, .major_version = PURPLE_MAJOR_VERSION, .minor_version = PURPLE_MINOR_VERSION,
    .type = PURPLE_PLUGIN_STANDARD, .priority = PURPLE_PRIORITY_DEFAULT, .id = "pidgin-matrix-ui",
    .name = "Matrix UI Enhancer", .version = "0.3.0", .summary = "Modern Action Bar with Typing & Encryption info.",
    .description = "Adds real-time typing indicators, encryption status, and Matrix account actions.",
    .author = "Author Name", .homepage = "https://matrix.org", .load = plugin_load, .unload = plugin_unload, .destroy = NULL,
    .actions = ui_actions
};

static void init_pidgin_ui_plugin(PurplePlugin *plugin) {}
PURPLE_INIT_PLUGIN(pidgin_matrix_ui, init_pidgin_ui_plugin, info)
