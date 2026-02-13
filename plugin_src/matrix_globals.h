#ifndef MATRIX_GLOBALS_H
#define MATRIX_GLOBALS_H

#include <libpurple/plugin.h>
#include <glib.h>

extern PurplePlugin *my_plugin;
extern GHashTable *room_id_map; // Maps room_id (string) -> chat_id (int)

#endif // MATRIX_GLOBALS_H
