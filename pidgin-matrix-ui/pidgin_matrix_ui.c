// pidgin-matrix-ui/pidgin_matrix_ui.c
#define PURPLE_PLUGINS
#include <glib.h>
#include <libpurple/plugin.h>
#include <libpurple/version.h>
#include <libpurple/debug.h>
#include <libpurple/util.h>
#include <libpurple/signals.h>
#include <libpurple/conversation.h>
#include <pidgin/pidgin.h>
#include <pidgin/gtkconv.h>
#include <gtk/gtk.h>
#include <string.h>

typedef struct {
    PurpleConversation *conv;
    char *signal_name;
} SignalIdleData;

static gboolean
emit_signal_idle_cb(gpointer data)
{
    SignalIdleData *sd = (SignalIdleData *)data;
    if (g_list_find(purple_get_conversations(), sd->conv)) {
        const char *room_id = purple_conversation_get_name(sd->conv);
        PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
        if (matrix_plugin && purple_plugin_is_loaded(matrix_plugin)) {
            purple_signal_emit(matrix_plugin, sd->signal_name, room_id);
        }
    }
    g_free(sd->signal_name);
    g_free(sd);
    return FALSE;
}

static void
emit_matrix_signal(PurpleConversation *conv, const char *signal_name)
{
    if (!conv) return;
    SignalIdleData *sd = g_new0(SignalIdleData, 1);
    sd->conv = conv;
    sd->signal_name = g_strdup(signal_name);
    g_idle_add(emit_signal_idle_cb, sd);
}

/* Sub-menu callbacks */
static void on_menu_dash(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-dashboard"); }
static void on_menu_reply(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply"); }
static void on_menu_thread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-thread"); }
static void on_menu_sticker(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-sticker"); }
static void on_menu_poll(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-poll"); }

static void
conversation_extended_menu_cb(PurpleConversation *conv, GList **list)
{
    PurpleAccount *account = purple_conversation_get_account(conv);
    if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
        
        /* Create a "Matrix" sub-menu for safety and cleanliness */
        PurpleMenuAction *matrix_menu = purple_menu_action_new("Matrix", NULL, NULL, NULL);
        *list = g_list_append(*list, matrix_menu);

        GList *sub_actions = NULL;
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Room Dashboard", PURPLE_CALLBACK(on_menu_dash), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Reply to Message", PURPLE_CALLBACK(on_menu_reply), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Start Thread", PURPLE_CALLBACK(on_menu_thread), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Send Sticker", PURPLE_CALLBACK(on_menu_sticker), conv, NULL));
        sub_actions = g_list_append(sub_actions, purple_menu_action_new("Create Poll", PURPLE_CALLBACK(on_menu_poll), conv, NULL));
        
        matrix_menu->children = sub_actions;
    }
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
    void *conv_handle = purple_conversations_get_handle();
    purple_signal_connect(conv_handle, "conversation-extended-menu",
                          plugin, PURPLE_CALLBACK(conversation_extended_menu_cb), NULL);
    purple_debug_info("pidgin-matrix-ui", "Matrix UI Enhancer loaded (Sub-menu mode).\n");
    return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
    return TRUE;
}

static PurplePluginInfo info = {
    .magic = PURPLE_PLUGIN_MAGIC,
    .major_version = PURPLE_MAJOR_VERSION,
    .minor_version = PURPLE_MINOR_VERSION,
    .type = PURPLE_PLUGIN_STANDARD,
    .priority = PURPLE_PRIORITY_DEFAULT,
    .id = "pidgin-matrix-ui",
    .name = "Matrix UI Enhancer",
    .version = "0.2.0",
    .summary = "Nuclear-stable Matrix UI integration.",
    .description = "Injects a safe 'Matrix' sub-menu into conversations. Zero GTK hierarchy mutation.",
    .author = "Author Name",
    .homepage = "https://matrix.org",
    .load = plugin_load,
    .unload = plugin_unload,
    .destroy = NULL
};

static void init_pidgin_ui_plugin(PurplePlugin *plugin) {}

PURPLE_INIT_PLUGIN(pidgin_matrix_ui, init_pidgin_ui_plugin, info)
