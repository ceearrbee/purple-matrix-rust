// pidgin-matrix-ui/pidgin_matrix_ui.c
#define PURPLE_PLUGINS
#include <glib.h>
#include <gtk/gtk.h>
#include <libpurple/account.h>
#include <libpurple/conversation.h>
#include <libpurple/core.h>
#include <libpurple/debug.h>
#include <libpurple/plugin.h>
#include <libpurple/signals.h>
#include <libpurple/util.h>
#include <libpurple/version.h>
#include <pidgin/gtkconv.h>
#include <pidgin/gtkimhtml.h>
#include <pidgin/pidgin.h>
#include <string.h>

#define MATRIX_UI_ACTION_BAR_KEY "matrix-ui-action-bar"
#define MATRIX_UI_TYPING_LABEL_KEY "matrix-ui-typing-label"
#define MATRIX_UI_ENCRYPTED_LABEL_KEY "matrix-ui-encrypted-label"
#define MATRIX_UI_MUTED_LABEL_KEY "matrix-ui-muted-label"
#define MATRIX_UI_READ_RECEIPT_LABEL_KEY "matrix-ui-read-receipt-label"
#define MATRIX_UI_TOPIC_LABEL_KEY "matrix-ui-topic-label"
#define MATRIX_UI_POPUP_HOOKED_KEY "matrix-ui-popup-hooked"
#define MATRIX_UI_SELECTED_EVENT_ID_KEY "matrix-ui-selected-event-id"
#define MATRIX_UI_SELECTED_EVENT_SENDER_KEY "matrix-ui-selected-event-sender"

/* Local Utilities */
static char *local_sanitize_markup_text(const char *input) {
  if (!input) return g_strdup("");
  char *tmp = purple_markup_strip_html(input);
  char *ret = g_strdup(tmp ? tmp : "");
  g_free(tmp);
  return ret;
}

static PurpleAccount *local_find_matrix_account(void) {
  GList *it;
  for (it = purple_accounts_get_all(); it != NULL; it = it->next) {
    PurpleAccount *account = (PurpleAccount *)it->data;
    if (strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
      return account;
    }
  }
  return NULL;
}

static void emit_matrix_signal(PurpleConversation *conv, const char *signal_name) {
  PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
  if (matrix_plugin) {
    purple_signal_emit(matrix_plugin, signal_name, purple_conversation_get_name(conv));
  }
}

/* Forward Declarations */
static void on_menu_reply(gpointer d);
static void on_menu_reply_latest(gpointer d);
static void on_menu_start_thread(gpointer d);
static void on_menu_threads(gpointer d);
static void on_menu_poll(gpointer d);
static void on_menu_list_polls(gpointer d);
static void on_menu_react_pick_event(gpointer d);
static void on_menu_edit_pick_event(gpointer d);
static void on_menu_redact_pick_event(gpointer d);
static void on_menu_show_last_event_details(gpointer d);
static void on_menu_dash(gpointer d);
static void on_menu_moderate(gpointer d);
static void on_menu_room_settings(gpointer d);
static void on_menu_invite_user(gpointer d);
static void on_menu_send_file(gpointer d);
static void on_menu_mark_unread(gpointer d);
static void on_menu_mark_read(gpointer d);
static void on_menu_send_location(gpointer d);

static void on_menu_reply(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply"); }
static void on_menu_reply_latest(gpointer d) {
  PurpleConversation *conv = (PurpleConversation *)d;
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, NULL);
  emit_matrix_signal(conv, "matrix-ui-action-reply");
}
static void on_menu_start_thread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-start-thread"); }
static void on_menu_threads(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-threads"); }
static void on_menu_poll(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-poll"); }
static void on_menu_list_polls(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-list-polls"); }
static void on_menu_react_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-react-pick-event"); }
static void on_menu_edit_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-edit-pick-event"); }
static void on_menu_redact_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-redact-pick-event"); }
static void on_menu_show_last_event_details(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-last-event-details"); }
static void on_menu_dash(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-dashboard"); }
static void on_menu_moderate(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-moderate"); }
static void on_menu_room_settings(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-room-settings"); }
static void on_menu_invite_user(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-invite-user"); }
static void on_menu_send_file(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-send-file"); }
static void on_menu_mark_unread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mark-unread"); }
static void on_menu_mark_read(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-mark-read"); }
static void on_menu_send_location(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-send-location"); }

static void on_matrix_message_actions_clicked(GtkWidget *b, PurpleConversation *c) {
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *item;
  item = gtk_menu_item_new_with_label("React...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_react_pick_event), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Edit...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_edit_pick_event), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Redact...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_redact_pick_event), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static void on_matrix_admin_actions_clicked(GtkWidget *b, PurpleConversation *c) {
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *item;
  item = gtk_menu_item_new_with_label("Room Settings...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_room_settings), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Moderate Room...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_moderate), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static void on_matrix_more_actions_clicked(GtkWidget *b, PurpleConversation *c) {
  GtkWidget *menu = gtk_menu_new();
  GtkWidget *item;
  item = gtk_menu_item_new_with_label("Invite User...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_invite_user), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Send File...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_send_file), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Send Location...");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_send_location), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Mark Unread");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_mark_unread), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  item = gtk_menu_item_new_with_label("Mark Read");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_mark_read), c);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
  gtk_widget_show_all(menu);
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
}

static void identify_event_at_iter(PurpleConversation *conv, GtkTextIter *iter) {
  GtkTextIter start = *iter;
  GtkTextIter end = *iter;
  char *line_text = NULL;
  char *clean_line = NULL;
  int i;

  gtk_text_iter_set_line_offset(&start, 0);
  if (!gtk_text_iter_ends_line(&end)) gtk_text_iter_forward_to_line_end(&end);

  line_text = gtk_text_iter_get_text(&start, &end);
  if (!line_text) return;

  clean_line = local_sanitize_markup_text(line_text);
  g_free(line_text);

  g_free(purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY));
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, NULL);
  g_free(purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY));
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY, NULL);

  const char *markers[] = {"_MXID:[", "_MX_ID:", "[MXID:"};
  for (int m=0; m<3; m++) {
    const char *marker_start = g_strrstr(clean_line, markers[m]);
    if (marker_start) {
        const char *id_start = marker_start + strlen(markers[m]);
        char *ev_id = g_strdup(id_start);
        
        char *end_ptr = strchr(ev_id, ']');
        if (end_ptr) *end_ptr = '\0';
        else {
            char *space = strchr(ev_id, ' ');
            if (space) *space = '\0';
        }
        
        if (ev_id && strlen(ev_id) > 5) {
            purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, g_strdup(ev_id));
            for (i = 0; i < 10; i++) {
                char key_id[64], key_sender[64];
                g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
                g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d", i);
                const char *stored_id = purple_conversation_get_data(conv, key_id);
                if (stored_id && strcmp(stored_id, ev_id) == 0) {
                    const char *stored_sender = purple_conversation_get_data(conv, key_sender);
                    if (stored_sender) {
                        purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY, g_strdup(stored_sender));
                    }
                    break;
                }
            }
            g_free(ev_id);
            g_free(clean_line);
            return;
        }
        g_free(ev_id);
    }
  }

  for (i = 0; i < 10; i++) {
    char key_id[64], key_msg[64], key_sender[64];
    const char *ev_id, *ev_msg, *ev_sender;
    g_snprintf(key_id, sizeof(key_id), "matrix_recent_event_id_%d", i);
    g_snprintf(key_msg, sizeof(key_msg), "matrix_recent_event_msg_%d", i);
    g_snprintf(key_sender, sizeof(key_sender), "matrix_recent_event_sender_%d", i);
    ev_id = purple_conversation_get_data(conv, key_id);
    ev_msg = purple_conversation_get_data(conv, key_msg);
    ev_sender = purple_conversation_get_data(conv, key_sender);
    if (ev_id && ev_msg && strlen(ev_msg) > 3) {
      if (strstr(clean_line, ev_msg) != NULL) {
        if (!ev_sender || !*ev_sender || strstr(clean_line, ev_sender) != NULL) {
          purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, g_strdup(ev_id));
          if (ev_sender) purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY, g_strdup(ev_sender));
          break;
        }
      }
    }
  }
  g_free(clean_line);
}

static gboolean imhtml_button_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data) {
  if (event->button != 3) return FALSE;
  GtkTextView *view = GTK_TEXT_VIEW(widget);
  GtkTextIter iter;
  int x, y;
  gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_WIDGET, event->x, event->y, &x, &y);
  gtk_text_view_get_iter_at_location(view, &iter, x, y);
  identify_event_at_iter((PurpleConversation *)data, &iter);
  return FALSE;
}

static void imhtml_populate_popup_cb(GtkWidget *imhtml, GtkMenu *menu, gpointer data) {
  PurpleConversation *conv = (PurpleConversation *)data;
  PurpleAccount *account = purple_conversation_get_account(conv);
  GtkWidget *item, *matrix_item, *matrix_menu;
  if (!conv || !account) return;

  const char *selected_id = purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY);
  const char *selected_sender = purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY);
  const char *my_id = purple_account_get_username(account);
  const char *my_alias = purple_account_get_alias(account);
  gboolean is_mine = (selected_sender && ((my_id && strcmp(selected_sender, my_id) == 0) || (my_alias && strcmp(selected_sender, my_alias) == 0)));

  item = gtk_separator_menu_item_new();
  gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
  gtk_widget_show(item);

  if (selected_id) {
    item = gtk_menu_item_new_with_label("Matrix Reply");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_reply), conv);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    item = gtk_menu_item_new_with_label("Matrix React...");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_react_pick_event), conv);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    item = gtk_menu_item_new_with_label("Matrix Thread");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_start_thread), conv);
    gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    if (is_mine) {
      item = gtk_menu_item_new_with_label("Matrix Edit...");
      g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_edit_pick_event), conv);
      gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
      gtk_widget_show(item);
    }
  }

  matrix_item = gtk_menu_item_new_with_label("Matrix Options");
  gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), matrix_item);
  gtk_widget_show(matrix_item);
  matrix_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(matrix_item), matrix_menu);

  item = gtk_menu_item_new_with_label("Show Last Event Details");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_show_last_event_details), conv);
  gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
  gtk_widget_show(item);
  
  item = gtk_menu_item_new_with_label("Room Dashboard");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_dash), conv);
  gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
  gtk_widget_show(item);
}

static void handle_message_edited_cb(const char *room_id, const char *event_id, const char *new_msg, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->imhtml) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
  if (!buffer) return;

  const char *markers[] = {"_MXID:[", "_MX_ID:", "[MXID:"};
  for (int m=0; m<3; m++) {
    char *search_str;
    search_str = g_strdup_printf("%s%s", markers[m], event_id);
    if (markers[m][0] != '_') {
        char *tmp = search_str;
        search_str = g_strdup_printf("%s]", tmp);
        g_free(tmp);
    }
    
    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    if (gtk_text_iter_forward_search(&start, search_str, 0, &start, &end, NULL)) {
        GtkTextIter line_start = start;
        GtkTextIter line_end = end;
        gtk_text_iter_set_line_offset(&line_start, 0);
        if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);
        
        /* Find colon after sender to preserve prefix (timestamp/sender) */
        GtkTextIter body_start = line_start;
        GtkTextIter paren_end;
        if (gtk_text_iter_forward_search(&body_start, ") ", 0, &paren_end, &body_start, &start)) {
            if (gtk_text_iter_forward_search(&body_start, ": ", 0, &body_start, &body_start, &start)) {
                line_start = body_start;
            }
        }

        char *replacement_html = g_strdup_printf("%s <font color='#fdfdfd' size='1'>%s</font>", new_msg, search_str);
        gtk_text_buffer_delete(buffer, &line_start, &line_end);
        gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), replacement_html, 0, &line_start);
        g_free(replacement_html);
        g_free(search_str);
        return;
    }
    g_free(search_str);
  }
}

static void handle_reactions_changed_cb(const char *room_id, const char *event_id, const char *reactions_text, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->imhtml) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
  if (!buffer) return;

  const char *markers[] = {"_MXID:[", "_MX_ID:", "[MXID:"};
  for (int m=0; m<3; m++) {
    char *search_str;
    search_str = g_strdup_printf("%s%s", markers[m], event_id);
    if (markers[m][0] != '_') {
        char *tmp = search_str;
        search_str = g_strdup_printf("%s]", tmp);
        g_free(tmp);
    }

    GtkTextIter start, end;
    gtk_text_buffer_get_start_iter(buffer, &start);
    
    if (gtk_text_iter_forward_search(&start, search_str, 0, &start, &end, NULL)) {
        GtkTextIter block_end = end;
        if (!gtk_text_iter_ends_line(&block_end)) gtk_text_iter_forward_to_line_end(&block_end);
        
        char *full_text = gtk_text_buffer_get_text(buffer, &end, &block_end, FALSE);
        if (full_text && strstr(full_text, "[Reactions: ")) {
            GtkTextIter del_start = end;
            GtkTextIter del_end = end;
            if (gtk_text_iter_forward_search(&del_start, " [Reactions: ", 0, &del_start, &del_end, &block_end)) {
                if (gtk_text_iter_forward_search(&del_end, "]", 0, &del_start, &del_end, &block_end)) {
                    gtk_text_buffer_delete(buffer, &del_start, &del_end);
                }
            }
        }
        g_free(full_text);

        if (reactions_text && strlen(reactions_text) > 0) {
            const char *raw_text = reactions_text;
            if (g_str_has_prefix(raw_text, "[System] [Reactions] ")) raw_text += 21;
            
            char *esc_reactions = g_markup_escape_text(raw_text, -1);
            char *markup = g_strdup_printf(" <font color='#666666' back='#eeeeee'>&nbsp;[Reactions: %s]&nbsp;</font>", esc_reactions);
            gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), markup, 0, &end);
            g_free(markup);
            g_free(esc_reactions);
        }
        
        g_free(search_str);
        return;
    }
    g_free(search_str);
  }
}

static void handle_read_receipt_cb(const char *room_id, const char *who, const char *event_id, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_READ_RECEIPT_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
    char *esc_who = g_markup_escape_text(who, -1);
    char *txt = g_strdup_printf("<span size='smaller' color='#666'>Last Read: %s</span>", esc_who);
    gtk_label_set_markup(GTK_LABEL(label), txt);
    g_free(txt);
    g_free(esc_who);
  }
}

static void handle_room_activity_cb(const char *room_id, const char *sender, const char *snippet, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) {
    char *safe = g_strndup(snippet ? snippet : "", 80);
    char *txt = g_strdup_printf("Last: %s: %s", sender ? sender : "user", safe);
    purple_conversation_set_data(conv, "matrix_last_activity", txt);
    g_free(safe);
    PurpleChat *chat = purple_blist_find_chat(account, room_id);
    if (chat) purple_blist_update_node_icon((PurpleBlistNode *)chat);
  }
}

static void handle_room_topic_cb(const char *room_id, const char *topic, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (conv) {
    purple_conversation_set_data(conv, "matrix_topic", g_strdup(topic));
    GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_TOPIC_LABEL_KEY);
    if (label && GTK_IS_LABEL(label)) {
        char *esc_topic = g_markup_escape_text(topic ? topic : "", -1);
        char *txt = g_strdup_printf("<span size='smaller' color='#444'><b>Topic:</b> %s</span>", esc_topic);
        gtk_label_set_markup(GTK_LABEL(label), txt);
        g_free(txt);
        g_free(esc_topic);
    }
    PurpleChat *chat = purple_blist_find_chat(account, room_id);
    if (chat) {
        g_hash_table_replace(purple_chat_get_components(chat), g_strdup("topic"), g_strdup(topic ? topic : ""));
        purple_blist_update_node_icon((PurpleBlistNode *)chat);
    }
  }
}

static void handle_room_typing_cb(const char *room_id, const char *who, gboolean is_typing, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_TYPING_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
    if (is_typing) {
      char *esc_who = g_markup_escape_text(who, -1);
      char *txt = g_strdup_printf("<span size='smaller' color='#666'><i>%s is typing...</i></span>", esc_who);
      gtk_label_set_markup(GTK_LABEL(label), txt);
      g_free(txt); g_free(esc_who);
    } else {
      gtk_label_set_text(GTK_LABEL(label), "");
    }
  }
}

static void handle_room_encrypted_cb(const char *room_id, gboolean is_encrypted, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_ENCRYPTED_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
    if (is_encrypted) gtk_label_set_markup(GTK_LABEL(label), "<span color='darkgreen'>🔒</span>");
    else gtk_label_set_text(GTK_LABEL(label), "");
  }
}

static void handle_room_muted_cb(const char *room_id, gboolean muted, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_MUTED_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
    if (muted) gtk_label_set_markup(GTK_LABEL(label), "<span color='#aa6600'>M</span>");
    else gtk_label_set_text(GTK_LABEL(label), "");
  }
}

static gboolean inject_action_bar_idle_cb(gpointer data) {
  PurpleConversation *conv = (PurpleConversation *)data;
  if (!g_list_find(purple_get_conversations(), conv)) return FALSE;
  if (purple_conversation_get_data(conv, MATRIX_UI_ACTION_BAR_KEY)) return FALSE;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->lower_hbox || !GTK_IS_WIDGET(gtkconv->lower_hbox)) return TRUE;
  if (!GTK_WIDGET_REALIZED(gtkconv->lower_hbox)) return TRUE;
  GtkWidget *vbox = gtk_widget_get_parent(gtkconv->lower_hbox);
  if (!vbox || !GTK_IS_VBOX(vbox)) return FALSE;

  if (gtkconv->imhtml && GTK_IS_WIDGET(gtkconv->imhtml)) {
    g_signal_connect(G_OBJECT(gtkconv->imhtml), "populate-popup", G_CALLBACK(imhtml_populate_popup_cb), conv);
    g_signal_connect(G_OBJECT(gtkconv->imhtml), "button-press-event", G_CALLBACK(imhtml_button_press_cb), conv);
    purple_conversation_set_data(conv, MATRIX_UI_POPUP_HOOKED_KEY, GINT_TO_POINTER(1));
  }

  GtkWidget *main_vbox = gtk_vbox_new(FALSE, 0);
  GtkWidget *topic_hbox = gtk_hbox_new(FALSE, 5);
  GtkWidget *topic_label = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(topic_label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment(GTK_MISC(topic_label), 0.0f, 0.5f);
  gtk_box_pack_start(GTK_BOX(topic_hbox), topic_label, TRUE, TRUE, 5);
  purple_conversation_set_data(conv, MATRIX_UI_TOPIC_LABEL_KEY, topic_label);
  
  const char *topic = purple_conversation_get_data(conv, "matrix_topic");
  if (topic) {
      char *esc_topic = g_markup_escape_text(topic, -1);
      char *txt = g_strdup_printf("<span size='smaller' color='#444'><b>Topic:</b> %s</span>", esc_topic);
      gtk_label_set_markup(GTK_LABEL(topic_label), txt);
      g_free(txt);
      g_free(esc_topic);
  }
  gtk_box_pack_start(GTK_BOX(main_vbox), topic_hbox, FALSE, FALSE, 2);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
  GtkWidget *reply_btn = gtk_button_new_with_label("Reply");
  GtkWidget *thread_btn = gtk_button_new_with_label("Thread+");
  GtkWidget *threads_btn = gtk_button_new_with_label("Threads");
  GtkWidget *poll_btn = gtk_button_new_with_label("Poll+");
  GtkWidget *polls_btn = gtk_button_new_with_label("Polls");
  GtkWidget *msg_btn = gtk_button_new_with_label("Msg");
  GtkWidget *admin_btn = gtk_button_new_with_label("Admin");
  GtkWidget *more_btn = gtk_button_new_with_label("More");

  GtkWidget *btns[] = {reply_btn, thread_btn, threads_btn, poll_btn, polls_btn, msg_btn, admin_btn, more_btn};
  for (int i=0; i<8; i++) {
    gtk_button_set_relief(GTK_BUTTON(btns[i]), GTK_RELIEF_NONE);
    GtkWidget *lbl = gtk_bin_get_child(GTK_BIN(btns[i]));
    if (GTK_IS_LABEL(lbl)) {
        PangoFontDescription *fd = pango_font_description_from_string("sans bold 8");
        gtk_widget_modify_font(lbl, fd);
        pango_font_description_free(fd);
    }
    gtk_box_pack_start(GTK_BOX(hbox), btns[i], FALSE, FALSE, 0);
  }

  g_signal_connect_swapped(reply_btn, "clicked", G_CALLBACK(on_menu_reply_latest), conv);
  g_signal_connect_swapped(thread_btn, "clicked", G_CALLBACK(on_menu_start_thread), conv);
  g_signal_connect_swapped(threads_btn, "clicked", G_CALLBACK(on_menu_threads), conv);
  g_signal_connect_swapped(poll_btn, "clicked", G_CALLBACK(on_menu_poll), conv);
  g_signal_connect_swapped(polls_btn, "clicked", G_CALLBACK(on_menu_list_polls), conv);
  g_signal_connect(msg_btn, "clicked", G_CALLBACK(on_matrix_message_actions_clicked), conv);
  g_signal_connect(admin_btn, "clicked", G_CALLBACK(on_matrix_admin_actions_clicked), conv);
  g_signal_connect(more_btn, "clicked", G_CALLBACK(on_matrix_more_actions_clicked), conv);

  GtkWidget *read_receipt_label = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(read_receipt_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start(GTK_BOX(hbox), read_receipt_label, TRUE, TRUE, 8);
  purple_conversation_set_data(conv, MATRIX_UI_READ_RECEIPT_LABEL_KEY, read_receipt_label);

  GtkWidget *typing_label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), typing_label, FALSE, FALSE, 10);
  purple_conversation_set_data(conv, MATRIX_UI_TYPING_LABEL_KEY, typing_label);

  gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), main_vbox, FALSE, FALSE, 0);
  gtk_widget_show_all(main_vbox);
  purple_conversation_set_data(conv, MATRIX_UI_ACTION_BAR_KEY, main_vbox);
  return FALSE;
}

static void conversation_created_cb(PurpleConversation *conv, gpointer data) {
  PurpleAccount *account = purple_conversation_get_account(conv);
  if (account && strcmp(purple_account_get_protocol_id(account), "prpl-matrix-rust") == 0) {
    purple_timeout_add(1000, inject_action_bar_idle_cb, conv);
  }
}

static void conversation_deleted_cb(PurpleConversation *conv, gpointer data) {
  purple_conversation_set_data(conv, MATRIX_UI_ACTION_BAR_KEY, NULL);
  g_free(purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY));
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, NULL);
  g_free(purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY));
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY, NULL);
}

static void connect_ui_signals(PurplePlugin *plugin) {
  void *conv_handle = purple_conversations_get_handle();
  PurplePlugin *matrix_plugin = purple_plugins_find_with_id("prpl-matrix-rust");
  if (!matrix_plugin) {
    GList *pl;
    for (pl = purple_plugins_get_all(); pl; pl = pl->next) {
      PurplePlugin *p = (PurplePlugin *)pl->data;
      if (p->info && p->info->id && strcmp(p->info->id, "prpl-matrix-rust") == 0) {
        matrix_plugin = p; break;
      }
    }
  }
  
  if (conv_handle) {
    purple_signal_connect(conv_handle, "conversation-created", plugin, PURPLE_CALLBACK(conversation_created_cb), NULL);
    purple_signal_connect(conv_handle, "deleting-conversation", plugin, PURPLE_CALLBACK(conversation_deleted_cb), NULL);
  }
  
  if (matrix_plugin) {
    purple_signal_connect(matrix_plugin, "matrix-ui-room-typing", plugin, PURPLE_CALLBACK(handle_room_typing_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-encrypted", plugin, PURPLE_CALLBACK(handle_room_encrypted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-muted", plugin, PURPLE_CALLBACK(handle_room_muted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-activity", plugin, PURPLE_CALLBACK(handle_room_activity_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-reactions-changed", plugin, PURPLE_CALLBACK(handle_reactions_changed_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-message-edited", plugin, PURPLE_CALLBACK(handle_message_edited_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-read-receipt", plugin, PURPLE_CALLBACK(handle_read_receipt_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-topic", plugin, PURPLE_CALLBACK(handle_room_topic_cb), NULL);
  }
  for (GList *it = purple_get_conversations(); it; it = it->next) conversation_created_cb((PurpleConversation *)it->data, NULL);
}

static void plugin_load_cb(PurplePlugin *p, gpointer data) {
  if (p && p->info && p->info->id && strcmp(p->info->id, "prpl-matrix-rust") == 0) {
    connect_ui_signals((PurplePlugin *)data);
  }
}

static gboolean plugin_load(PurplePlugin *plugin) {
  void *plugins_handle = purple_plugins_get_handle();
  purple_signal_connect(plugins_handle, "plugin-load", plugin, PURPLE_CALLBACK(plugin_load_cb), plugin);
  
  if (purple_get_core() != NULL && purple_conversations_get_handle() != NULL) {
    connect_ui_signals(plugin);
  }
  return TRUE;
}

static gboolean plugin_unload(PurplePlugin *plugin) {
  purple_signals_disconnect_by_handle(plugin);
  return TRUE;
}

static void on_action_login_password(PurplePluginAction *action) { PurpleAccount *acc = (PurpleAccount *)action->context; PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-login-password", purple_account_get_username(acc)); }
static void on_action_login_sso(PurplePluginAction *action) { PurpleAccount *acc = (PurpleAccount *)action->context; PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-login-sso", purple_account_get_username(acc)); }
static void on_action_set_account_defaults(PurplePluginAction *action) { PurpleAccount *acc = (PurpleAccount *)action->context; PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-set-account-defaults", purple_account_get_username(acc)); }
static void on_action_clear_session(PurplePluginAction *action) { PurpleAccount *acc = (PurpleAccount *)action->context; PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-clear-session", purple_account_get_username(acc)); }
static void on_action_discover_join(PurplePluginAction *action) { PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-discover-join", NULL); }
static void on_action_discover_search_users(PurplePluginAction *action) { PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-search-users", NULL); }
static void on_action_discover_search_public(PurplePluginAction *action) { PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-search-public", NULL); }
static void on_action_my_profile(PurplePluginAction *action) { PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-my-profile", NULL); }
static void on_action_recover_keys(PurplePluginAction *action) { PurplePlugin *p = purple_plugins_find_with_id("prpl-matrix-rust"); if(p) purple_signal_emit(p, "matrix-ui-action-recover-keys", NULL); }

static GList *ui_actions(PurplePlugin *plugin, gpointer context) {
  GList *m = NULL;
  m = g_list_append(m, purple_plugin_action_new("Login (Password)...", on_action_login_password));
  m = g_list_append(m, purple_plugin_action_new("Login (SSO)...", on_action_login_sso));
  m = g_list_append(m, purple_plugin_action_new("Set Homeserver...", on_action_set_account_defaults));
  m = g_list_append(m, purple_plugin_action_new("Join Room...", on_action_discover_join));
  m = g_list_append(m, purple_plugin_action_new("Search Users...", on_action_discover_search_users));
  m = g_list_append(m, purple_plugin_action_new("Search Public Rooms...", on_action_discover_search_public));
  m = g_list_append(m, purple_plugin_action_new("My Profile", on_action_my_profile));
  m = g_list_append(m, purple_plugin_action_new("Recover Keys...", on_action_recover_keys));
  m = g_list_append(m, purple_plugin_action_new("Clear Session...", on_action_clear_session));
  return m;
}

static PurplePluginInfo info = {
    .magic = PURPLE_PLUGIN_MAGIC, .major_version = PURPLE_MAJOR_VERSION, .minor_version = PURPLE_MINOR_VERSION,
    .type = PURPLE_PLUGIN_STANDARD, .priority = PURPLE_PRIORITY_DEFAULT, .id = "pidgin-matrix-ui",
    .name = "Matrix UI Enhancer", .version = "0.4.0", .summary = "Inline reactions and Matrix actions.",
    .description = "Adds real-time typing indicators, encryption status, and inline reactions.",
    .author = "Author Name", .homepage = "https://matrix.org", .load = plugin_load, .unload = plugin_unload,
    .actions = ui_actions};

static void init_pidgin_ui_plugin(PurplePlugin *plugin) {}
PURPLE_INIT_PLUGIN(pidgin_matrix_ui, init_pidgin_ui_plugin, info)
