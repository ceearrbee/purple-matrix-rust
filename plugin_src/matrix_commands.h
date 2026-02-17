#ifndef MATRIX_COMMANDS_H
#define MATRIX_COMMANDS_H

#include <libpurple/plugin.h>
#include <libpurple/conversation.h>

void register_matrix_commands(PurplePlugin *plugin);
GList *matrix_actions(PurplePlugin *plugin, gpointer context);
void conversation_extended_menu_cb(PurpleConversation *conv, GList **list);
void open_room_dashboard(PurpleAccount *account, const char *room_id);
void menu_action_reply_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_thread_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_thread_start_cb(PurpleConversation *conv, gpointer data);
void menu_action_sticker_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_poll_conv_cb(PurpleConversation *conv, gpointer data);
void menu_action_room_settings_cb(PurpleBlistNode *node, gpointer data);

#endif // MATRIX_COMMANDS_H
