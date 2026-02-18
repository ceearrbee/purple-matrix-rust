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

typedef struct {
    PurpleConversation *conv;
    char *signal_name;
} SignalIdleData;

/* Forward Declarations */
static void on_menu_dash(gpointer d);
static void on_menu_reply(gpointer d);
static void on_menu_threads(gpointer d);
static void on_menu_sticker(gpointer d);
static void on_menu_poll(gpointer d);

/* Helper to find a Matrix account */
static PurpleAccount *
find_matrix_account(void)
{
    GList *l;
    for (l = purple_accounts_get_all(); l != NULL; l = l->next) {
        PurpleAccount *account = (PurpleAccount *)l->data;
        if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
            return account;
        }
    }
    return NULL;
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

static void
emit_matrix_signal(PurpleConversation *conv, const char *signal_name)
{
    if (!conv || !signal_name) return;
    SignalIdleData *sd = g_new0(SignalIdleData, 1);
    sd->conv = conv;
    sd->signal_name = g_strdup(signal_name);
    g_idle_add(emit_signal_idle_cb, sd);
}

static void on_info_clicked(GtkWidget *b, PurpleConversation *c) { on_menu_dash(c); }
static void on_reply_clicked(GtkWidget *b, PurpleConversation *c) { on_menu_reply(c); }
static void on_threads_clicked(GtkWidget *b, PurpleConversation *c) { on_menu_threads(c); }
static void on_sticker_clicked(GtkWidget *b, PurpleConversation *c) { on_menu_sticker(c); }
static void on_poll_clicked(GtkWidget *b, PurpleConversation *c) { on_menu_poll(c); }

static void on_menu_dash(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-dashboard"); }
static void on_menu_reply(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply"); }
static void on_menu_threads(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-threads"); }
static void on_menu_sticker(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-sticker"); }
static void on_menu_poll(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-poll"); }

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
                    } else {
                        gtk_label_set_text(GTK_LABEL(label), "");
                    }
                }
            }
        }
    }
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

    const struct { const char *label; GCallback cb; const char *tooltip; } btns[] = {
        { "Info", G_CALLBACK(on_info_clicked), "Room Details & Dashboard" },
        { "Reply", G_CALLBACK(on_reply_clicked), "Reply to latest message" },
        { "List Threads", G_CALLBACK(on_threads_clicked), "List active threads" },
        { "Sticker", G_CALLBACK(on_sticker_clicked), "Send a sticker" },
        { "Poll", G_CALLBACK(on_poll_clicked), "Create a poll" },
        { NULL, NULL, NULL }
    };

    GtkTooltips *tooltips = gtk_tooltips_new();

    for (int i = 0; btns[i].label; i++) {
        GtkWidget *btn = gtk_button_new_with_label(btns[i].label);
        gtk_button_set_relief(GTK_BUTTON(btn), GTK_RELIEF_NONE);
        gtk_tooltips_set_tip(tooltips, btn, btns[i].tooltip, NULL);
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(btn));
        if (GTK_IS_LABEL(label)) {
            PangoFontDescription *fd = pango_font_description_from_string("sans bold 8");
            gtk_widget_modify_font(label, fd);
            pango_font_description_free(fd);
        }
        g_signal_connect(btn, "clicked", btns[i].cb, conv);
        gtk_box_pack_start(GTK_BOX(hbox), btn, FALSE, FALSE, 0);
    }

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
    .description = "Adds real-time typing indicators and encryption status to the conversation window.",
    .author = "Author Name", .homepage = "https://matrix.org", .load = plugin_load, .unload = plugin_unload, .destroy = NULL
};

static void init_pidgin_ui_plugin(PurplePlugin *plugin) {}
PURPLE_INIT_PLUGIN(pidgin_matrix_ui, init_pidgin_ui_plugin, info)
