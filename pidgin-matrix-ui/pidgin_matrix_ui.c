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
#define MATRIX_UI_ACTIVITY_LABEL_KEY "matrix-ui-activity-label"
#define MATRIX_UI_USERLIST_HOOKED_KEY "matrix-ui-userlist-hooked"
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

static void on_menu_reply(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-reply"); }
static void on_menu_reply_latest(gpointer d) {
  PurpleConversation *conv = (PurpleConversation *)d;
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, NULL);
  emit_matrix_signal(conv, "matrix-ui-action-reply");
}
static void on_menu_start_thread(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-start-thread"); }
static void on_menu_poll(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-poll"); }
static void on_menu_react_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-react-pick-event"); }
static void on_menu_edit_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-edit-pick-event"); }
static void on_menu_redact_pick_event(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-redact-pick-event"); }
static void on_menu_show_last_event_details(gpointer d) { emit_matrix_signal((PurpleConversation *)d, "matrix-ui-action-show-last-event-details"); }

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

  // Search for the marker (M:event_id)
  const char *marker_start = strstr(clean_line, "(M:");
  if (marker_start) {
    const char *marker_end = strchr(marker_start + 3, ')');
    if (marker_end) {
        char *ev_id = g_strndup(marker_start + 3, marker_end - (marker_start + 3));
        purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, g_strdup(ev_id));
        purple_debug_info("matrix-ui", "identify_event_at_iter: Found event_id from marker: %s\n", ev_id);
        
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
  }

  // Fallback to snippet matching
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
  gboolean is_mine = (selected_sender && my_id && g_strcmp0(selected_sender, my_id) == 0);

  item = gtk_separator_menu_item_new();
  gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), item);
  gtk_widget_show(item);

  if (selected_id) {
    item = gtk_menu_item_new_with_label("Matrix Reply");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_reply), conv);
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

  if (selected_id) {
    item = gtk_menu_item_new_with_label("Reply to this message");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_reply), conv);
    gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
    gtk_widget_show(item);

    item = gtk_menu_item_new_with_label("React to this message...");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_react_pick_event), conv);
    gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
    gtk_widget_show(item);

    item = gtk_menu_item_new_with_label("Start Thread from here");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_start_thread), conv);
    gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
    gtk_widget_show(item);

    if (is_mine) {
      item = gtk_menu_item_new_with_label("Edit this message...");
      g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_edit_pick_event), conv);
      gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
      gtk_widget_show(item);

      item = gtk_menu_item_new_with_label("Redact this message...");
      g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_redact_pick_event), conv);
      gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
      gtk_widget_show(item);
    }
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
    gtk_widget_show(item);
  }

  item = gtk_menu_item_new_with_label("Reply to Latest");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_reply_latest), conv);
  gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
  gtk_widget_show(item);

  item = gtk_menu_item_new_with_label("Show Last Event Details");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_show_last_event_details), conv);
  gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
  gtk_widget_show(item);
}

static void handle_message_edited_cb(const char *room_id, const char *event_id, const char *new_msg, gpointer data) {
  purple_debug_info("matrix-ui", "handle_message_edited_cb: room=%s event=%s\n", room_id, event_id);
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->imhtml) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
  if (!buffer) return;

  char *search_str = g_strdup_printf("(M:%s)", event_id);
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  if (gtk_text_iter_forward_search(&start, search_str, 0, &start, &end, NULL)) {
    purple_debug_info("matrix-ui", "handle_message_edited_cb: Found marker, updating message!\n");
    GtkTextIter line_start = start;
    GtkTextIter line_end = end;
    gtk_text_iter_set_line_offset(&line_start, 0);
    if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);
    
    /* Preserve the marker at the end of the new content */
    char *replacement_html = g_strdup_printf("%s <font color='#ffffff' size='1'>%s</font>", new_msg, search_str);
    
    gtk_text_buffer_delete(buffer, &line_start, &line_end);
    gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), replacement_html, 0, &line_start);
    g_free(replacement_html);
  } else {
    purple_debug_info("matrix-ui", "handle_message_edited_cb: Marker %s NOT FOUND\n", search_str);
  }
  g_free(search_str);
}

static void handle_reactions_changed_cb(const char *room_id, const char *event_id, const char *reactions_text, gpointer data) {
  purple_debug_info("matrix-ui", "handle_reactions_changed_cb: room=%s event=%s\n", room_id, event_id);
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->imhtml) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
  if (!buffer) return;

  char *search_str = g_strdup_printf("(M:%s)", event_id);
  GtkTextIter start, end;
  gtk_text_buffer_get_start_iter(buffer, &start);
  if (gtk_text_iter_forward_search(&start, search_str, 0, &start, &end, NULL)) {
    purple_debug_info("matrix-ui", "handle_reactions_changed_cb: Found marker!\n");
    GtkTextIter block_end = end;
    if (!gtk_text_iter_ends_line(&block_end)) gtk_text_iter_forward_to_line_end(&block_end);
    char *existing = gtk_text_buffer_get_text(buffer, &end, &block_end, FALSE);
    if (existing && strstr(existing, "[Reactions: ")) gtk_text_buffer_delete(buffer, &end, &block_end);
    g_free(existing);
    const char *raw_text = reactions_text;
    if (g_str_has_prefix(raw_text, "[System] [Reactions] ")) raw_text += 21;
    char *esc_reactions = g_markup_escape_text(raw_text, -1);
    char *markup = g_strdup_printf(" <span size='small' color='#666'><i>[Reactions: %s]</i></span>", esc_reactions);
    gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), markup, 0, &end);
    g_free(markup);
    g_free(esc_reactions);
  } else {
    purple_debug_info("matrix-ui", "handle_reactions_changed_cb: Marker %s NOT FOUND\n", search_str);
  }
  g_free(search_str);
}

static void handle_room_activity_cb(const char *room_id, const char *sender, const char *snippet, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_ACTIVITY_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
    char *safe = g_strndup(snippet ? snippet : "", 80);
    char *esc_sender = g_markup_escape_text(sender ? sender : "user", -1);
    char *esc_snippet = g_markup_escape_text(safe, -1);
    char *msg = g_strdup_printf("<span size='smaller' color='#666'>Last: %s: %s</span>", esc_sender, esc_snippet);
    gtk_label_set_markup(GTK_LABEL(label), msg);
    g_free(msg); g_free(esc_sender); g_free(esc_snippet); g_free(safe);
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
    if (is_encrypted) {
      gtk_label_set_markup(GTK_LABEL(label), "<span color='darkgreen'>🔒</span>");
    } else {
      gtk_label_set_text(GTK_LABEL(label), "");
    }
  }
}

static void handle_room_muted_cb(const char *room_id, gboolean muted, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_MUTED_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
    if (muted) {
      gtk_label_set_markup(GTK_LABEL(label), "<span color='#aa6600'>M</span>");
    } else {
      gtk_label_set_text(GTK_LABEL(label), "");
    }
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

  GtkWidget *hbox = gtk_hbox_new(FALSE, 2);
  GtkWidget *reply_btn = gtk_button_new_with_label("Reply");
  GtkWidget *thread_btn = gtk_button_new_with_label("Thread");
  GtkWidget *poll_btn = gtk_button_new_with_label("Poll");
  GtkWidget *btns[] = {reply_btn, thread_btn, poll_btn};
  for (int i=0; i<3; i++) {
    gtk_button_set_relief(GTK_BUTTON(btns[i]), GTK_RELIEF_NONE);
    gtk_box_pack_start(GTK_BOX(hbox), btns[i], FALSE, FALSE, 0);
  }
  g_signal_connect_swapped(reply_btn, "clicked", G_CALLBACK(on_menu_reply_latest), conv);
  g_signal_connect_swapped(thread_btn, "clicked", G_CALLBACK(on_menu_start_thread), conv);
  g_signal_connect_swapped(poll_btn, "clicked", G_CALLBACK(on_menu_poll), conv);

  GtkWidget *activity_label = gtk_label_new("");
  gtk_label_set_ellipsize(GTK_LABEL(activity_label), PANGO_ELLIPSIZE_END);
  gtk_box_pack_start(GTK_BOX(hbox), activity_label, TRUE, TRUE, 8);
  purple_conversation_set_data(conv, MATRIX_UI_ACTIVITY_LABEL_KEY, activity_label);

  GtkWidget *typing_label = gtk_label_new("");
  gtk_box_pack_start(GTK_BOX(hbox), typing_label, FALSE, FALSE, 10);
  purple_conversation_set_data(conv, MATRIX_UI_TYPING_LABEL_KEY, typing_label);

  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show_all(hbox);
  purple_conversation_set_data(conv, MATRIX_UI_ACTION_BAR_KEY, hbox);
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
  
  purple_debug_info("matrix-ui", "connect_ui_signals: conv_handle=%p matrix_plugin=%p\n", conv_handle, matrix_plugin);

  purple_signal_connect(conv_handle, "conversation-created", plugin, PURPLE_CALLBACK(conversation_created_cb), NULL);
  purple_signal_connect(conv_handle, "deleting-conversation", plugin, PURPLE_CALLBACK(conversation_deleted_cb), NULL);
  
  if (matrix_plugin && purple_plugin_is_loaded(matrix_plugin)) {
    purple_debug_info("matrix-ui", "connect_ui_signals: Matrix plugin is loaded, connecting to Matrix signals\n");
    purple_signal_connect(matrix_plugin, "matrix-ui-room-typing", plugin, PURPLE_CALLBACK(handle_room_typing_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-encrypted", plugin, PURPLE_CALLBACK(handle_room_encrypted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-muted", plugin, PURPLE_CALLBACK(handle_room_muted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-activity", plugin, PURPLE_CALLBACK(handle_room_activity_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-reactions-changed", plugin, PURPLE_CALLBACK(handle_reactions_changed_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-message-edited", plugin, PURPLE_CALLBACK(handle_message_edited_cb), NULL);
  } else {
    purple_debug_info("matrix-ui", "connect_ui_signals: Matrix plugin not loaded yet (or not found). Will connect when loaded.\n");
  }
  for (GList *it = purple_get_conversations(); it; it = it->next) conversation_created_cb((PurpleConversation *)it->data, NULL);
}

static void plugin_load_cb(PurplePlugin *p, gpointer data) {
  if (p && p->info && p->info->id && strcmp(p->info->id, "prpl-matrix-rust") == 0) {
    purple_debug_info("matrix-ui", "Matrix plugin JUST LOADED! Connecting signals now.\n");
    connect_ui_signals((PurplePlugin *)data);
  }
}

static gboolean plugin_load(PurplePlugin *plugin) {
  purple_debug_info("matrix-ui", "Matrix UI Enhancer plugin loading... (plugin handle=%p)\n", plugin);
  void *plugins_handle = purple_plugins_get_handle();
  purple_signal_connect(plugins_handle, "plugin-load", plugin, PURPLE_CALLBACK(plugin_load_cb), plugin);
  
  connect_ui_signals(plugin);
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
