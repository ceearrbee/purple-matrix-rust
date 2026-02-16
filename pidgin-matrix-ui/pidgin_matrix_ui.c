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

static PurplePlugin *pidgin_matrix_ui_plugin = NULL;

static void
on_show_dashboard_menu_activated(PurpleConversation *conv, gpointer data)
{
    const char *room_id = purple_conversation_get_name(conv);
    if (!room_id) return;

    // Find the Matrix protocol plugin
    PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
    if (matrix_plugin) {
        purple_debug_info("pidgin-matrix-ui", "Emitting dashboard signal for room: %s\n", room_id);
        // Emit signal by name. We pass the room_id string as the argument.
        purple_signal_emit(matrix_plugin, "matrix-ui-action::show-dashboard", room_id);
    } else {
        purple_debug_warning("pidgin-matrix-ui", "Matrix plugin not found, cannot trigger dashboard.\n");
    }
}

static void
conversation_extended_menu_cb(PurpleConversation *conv, GList **list)
{
    // Check if it's a Matrix conversation
    if (strcmp(purple_account_get_protocol_id(purple_conversation_get_account(conv)), "prpl-matrix-rust") != 0) {
        return;
    }

    // Add "Show Dashboard (UI Plugin)" to the conversation's menu
    PurpleMenuAction *action = purple_menu_action_new("Show Dashboard (UI Plugin)",
                                                      PURPLE_CALLBACK(on_show_dashboard_menu_activated),
                                                      NULL, NULL);
    *list = g_list_append(*list, action);
}

static gboolean
plugin_load(PurplePlugin *plugin)
{
    pidgin_matrix_ui_plugin = plugin;
    purple_debug_info("pidgin-matrix-ui", "Matrix UI Enhancer Plugin loaded.\n");

    // Connect to the "conversation-extended-menu" signal
    purple_signal_connect(purple_conversations_get_handle(), "conversation-extended-menu",
                          plugin, PURPLE_CALLBACK(conversation_extended_menu_cb), NULL);

    return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
    purple_debug_info("pidgin-matrix-ui", "Matrix UI Enhancer Plugin unloaded.\n");
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
    .version = "0.1",
    .summary = "A companion UI plugin for Matrix.",
    .description = "Adds native UI elements to Pidgin for Matrix-specific actions using signals.",
    .author = "Author Name <your@email.com>",
    .homepage = "https://example.com",
    .load = plugin_load,
    .unload = plugin_unload,
    .destroy = NULL
};

static void init_pidgin_ui_plugin(PurplePlugin *plugin) {
}

PURPLE_INIT_PLUGIN(pidgin_matrix_ui, init_pidgin_ui_plugin, info)
