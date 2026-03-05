#ifndef MATRIX_ACCOUNT_H
#define MATRIX_ACCOUNT_H

#include <glib.h>
#include <libpurple/account.h>
#include <libpurple/plugin.h>
#include <libpurple/request.h>
#include <libpurple/status.h>

void matrix_login(PurpleAccount *account);
void matrix_close(PurpleConnection *gc);
void matrix_init_sso_callbacks(void);
void manual_sso_token_action_cb(void *user_data, PurpleRequestFields *fields);
GList *matrix_actions(PurplePlugin *plugin, gpointer context);

// Callbacks
void sso_url_cb(const char *url);
void connected_cb(const char *user_id);
void login_failed_cb(const char *reason);
void sas_request_cb(const char *user_id, const char *target_user_id,
                    const char *flow_id);
void sas_emoji_cb(const char *user_id, const char *target_user_id,
                  const char *flow_id, const char *emojis);
void show_verification_qr_cb(const char *user_id, const char *target_user_id,
                             const char *html_data);
void show_user_info_cb(const char *user_id, const char *target_user_id,
                       const char *display_name, const char *avatar_url);
void presence_callback(const char *user_id, const char *target_user_id,
                       int status, const char *status_msg);
void chat_topic_callback(const char *user_id, const char *room_id,
                         const char *topic, const char *sender);
void chat_user_callback(const char *user_id, const char *room_id,
                        const char *member_id, bool add, const char *alias,
                        const char *avatar_path);
void room_left_callback(const char *user_id, const char *room_id);
void poll_creation_callback(const char *user_id, const char *room_id);
void poll_list_callback(const char *user_id, const char *room_id, const char *event_id,
                        const char *question, const char *sender, const char *options_str);
void thread_list_callback(const char *user_id, const char *room_id, const char *thread_root_id,
                          const char *latest_msg);
void room_list_add_callback(const char *user_id, const char *room_id, const char *name,
                            const char *topic, const char *parent_id);
void room_preview_cb(const char *user_id, const char *room_id_or_alias,
                     const char *html_body);

// PRPL Ops
GList *matrix_status_types(PurpleAccount *account);
char *matrix_status_text(PurpleBuddy *buddy);
void matrix_get_info(PurpleConnection *gc, const char *who);
void matrix_set_status(PurpleAccount *account, PurpleStatus *status);
void matrix_set_idle(PurpleConnection *gc, int time);
void matrix_change_passwd(PurpleConnection *gc, const char *old,
                          const char *new);
void matrix_add_deny(PurpleConnection *gc, const char *who);
void matrix_rem_deny(PurpleConnection *gc, const char *who);
void matrix_unregister_user(PurpleAccount *account,
                            PurpleAccountUnregistrationCb cb, void *user_data);
void matrix_set_public_alias(PurpleConnection *gc, const char *alias);
void matrix_set_buddy_icon(PurpleConnection *gc, PurpleStoredImage *img);

#endif // MATRIX_ACCOUNT_H
