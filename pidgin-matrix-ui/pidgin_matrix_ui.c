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
static void on_menu_threads(gpointer d);
static void on_menu_sticker(gpointer d);
static void on_menu_poll(gpointer d);
static void on_menu_clear_session(gpointer d);
static void on_menu_show_members(gpointer d);
static void on_menu_moderate(gpointer d);
static void on_menu_leave_room(gpointer d);
static void on_menu_verify_self(gpointer d);
static void on_menu_crypto_status(gpointer d);
static void on_menu_list_devices(gpointer d);
static void on_menu_room_settings(gpointer d);
static void on_menu_invite_user(gpointer d);
static void on_menu_send_file(gpointer d);
static void on_menu_mark_unread(gpointer d);
static void on_menu_mute_room(gpointer d);
static void on_menu_unmute_room(gpointer d);
static void on_menu_search_room(gpointer d);
static void on_action_login_password(PurplePluginAction *action);
static void on_action_login_sso(PurplePluginAction *action);
static void on_action_set_account_defaults(PurplePluginAction *action);
static void on_action_clear_session(PurplePluginAction *action);
static GList *ui_actions(PurplePlugin *plugin, gpointer context);
static gboolean hook_chat_user_list_idle_cb(gpointer data);

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

static void on_matrix_actions_clicked(GtkWidget *b, PurpleConversation *c);

static void on_menu_dash(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-dashboard"); }
static void on_menu_reply(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply"); }
static void on_menu_threads(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-threads"); }
static void on_menu_sticker(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-sticker"); }
static void on_menu_poll(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-poll"); }
static void on_menu_clear_session(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-clear-session"); }
static void on_menu_show_members(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-members"); }
static void on_menu_moderate(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-moderate"); }
static void on_menu_leave_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-leave-room"); }
static void on_menu_verify_self(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-verify-self"); }
static void on_menu_crypto_status(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-crypto-status"); }
static void on_menu_list_devices(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-devices"); }
static void on_menu_room_settings(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-room-settings"); }
static void on_menu_invite_user(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-invite-user"); }
static void on_menu_send_file(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-send-file"); }
static void on_menu_mark_unread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mark-unread"); }
static void on_menu_mute_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mute-room"); }
static void on_menu_unmute_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-unmute-room"); }
static void on_menu_search_room(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-search-room"); }

static void on_menu_user_info(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-user-info"); }

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

static void on_matrix_actions_clicked(GtkWidget *b, PurpleConversation *c) {
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *item;

    item = gtk_menu_item_new_with_label("Room Dashboard");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_dash), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Show Members");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_show_members), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("User Info");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_user_info), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Moderate Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_moderate), c);
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

    item = gtk_menu_item_new_with_label("Mute Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_mute_room), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label("Unmute Room");
    g_signal_connect_swapped(item, "activate", G_CALLBACK(on_menu_unmute_room), c);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

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
    
    /* Encryption Status */
    GtkWidget *enc_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox), enc_label, FALSE, FALSE, 5);
    purple_conversation_set_data(conv, MATRIX_UI_ENCRYPTED_LABEL_KEY, enc_label);
    GtkTooltips *tooltips = gtk_tooltips_new();
    GtkWidget *reply_btn = gtk_button_new_with_label("Reply");
    GtkWidget *threads_btn = gtk_button_new_with_label("Threads");
    GtkWidget *poll_btn = gtk_button_new_with_label("Poll");
    GtkWidget *moderate_btn = gtk_button_new_with_label("Moderate");
    GtkWidget *more_btn = gtk_button_new_with_label("More");
    GtkWidget *btns[] = {reply_btn, threads_btn, poll_btn, moderate_btn, more_btn};

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
    gtk_tooltips_set_tip(tooltips, threads_btn, "Show room threads", NULL);
    gtk_tooltips_set_tip(tooltips, poll_btn, "Create a poll", NULL);
    gtk_tooltips_set_tip(tooltips, moderate_btn, "Kick, ban, or unban users", NULL);
    gtk_tooltips_set_tip(tooltips, more_btn, "More Matrix actions", NULL);

    g_signal_connect_swapped(reply_btn, "clicked", G_CALLBACK(on_menu_reply), conv);
    g_signal_connect_swapped(threads_btn, "clicked", G_CALLBACK(on_menu_threads), conv);
    g_signal_connect_swapped(poll_btn, "clicked", G_CALLBACK(on_menu_poll), conv);
    g_signal_connect_swapped(moderate_btn, "clicked", G_CALLBACK(on_menu_moderate), conv);
    g_signal_connect(more_btn, "clicked", G_CALLBACK(on_matrix_actions_clicked), conv);

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
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("View Threads", PURPLE_CALLBACK(on_menu_threads), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Send Sticker", PURPLE_CALLBACK(on_menu_sticker), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Create Poll", PURPLE_CALLBACK(on_menu_poll), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("User Info", PURPLE_CALLBACK(on_menu_user_info), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Moderate Room", PURPLE_CALLBACK(on_menu_moderate), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Show Members", PURPLE_CALLBACK(on_menu_show_members), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Invite User", PURPLE_CALLBACK(on_menu_invite_user), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Send File", PURPLE_CALLBACK(on_menu_send_file), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Search in Room", PURPLE_CALLBACK(on_menu_search_room), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Room Settings", PURPLE_CALLBACK(on_menu_room_settings), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Verify This Device", PURPLE_CALLBACK(on_menu_verify_self), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Crypto Status", PURPLE_CALLBACK(on_menu_crypto_status), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("List My Devices", PURPLE_CALLBACK(on_menu_list_devices), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Mark Unread", PURPLE_CALLBACK(on_menu_mark_unread), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Mute Room", PURPLE_CALLBACK(on_menu_mute_room), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Unmute Room", PURPLE_CALLBACK(on_menu_unmute_room), conv, NULL));
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
