#ifndef MATRIX_ACCOUNT_H
#define MATRIX_ACCOUNT_H

#include <libpurple/account.h>
#include <libpurple/plugin.h>
#include <libpurple/status.h>
#include <glib.h>

void matrix_login(PurpleAccount *account);
void matrix_close(PurpleConnection *gc);
void matrix_init_sso_callbacks(void);
GList *matrix_actions(PurplePlugin *plugin, gpointer context);

// Callbacks
void sso_url_cb(const char *url);
void connected_cb(void);
void login_failed_cb(const char *reason);

// PRPL Ops
GList *matrix_status_types(PurpleAccount *account);
char *matrix_status_text(PurpleBuddy *buddy);
void matrix_get_info(PurpleConnection *gc, const char *who);
void matrix_set_status(PurpleAccount *account, PurpleStatus *status);
void matrix_set_idle(PurpleConnection *gc, int time);
void matrix_change_passwd(PurpleConnection *gc, const char *old, const char *new);
void matrix_add_deny(PurpleConnection *gc, const char *who);
void matrix_rem_deny(PurpleConnection *gc, const char *who);
void matrix_unregister_user(PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data);
void matrix_set_public_alias(PurpleConnection *gc, const char *alias);
void matrix_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img);

#endif // MATRIX_ACCOUNT_H
