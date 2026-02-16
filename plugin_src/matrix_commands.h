#ifndef MATRIX_COMMANDS_H
#define MATRIX_COMMANDS_H

#include <libpurple/plugin.h>
#include <libpurple/conversation.h>

void register_matrix_commands(PurplePlugin *plugin);
GList *matrix_actions(PurplePlugin *plugin, gpointer context);
void conversation_extended_menu_cb(PurpleConversation *conv, GList **list);

#endif // MATRIX_COMMANDS_H
