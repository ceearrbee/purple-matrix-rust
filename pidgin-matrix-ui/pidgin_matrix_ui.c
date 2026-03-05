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
#include "matrix_ffi_wrappers.h"

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
static void on_menu_dash(gpointer d);
static void handle_room_typing_cb(const char *room_id, const char *who, gboolean is_typing, gpointer data);

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

static void on_menu_jump_to_parent(gpointer d) {
  PurpleConversation *conv = (PurpleConversation *)d;
  const char *name = purple_conversation_get_name(conv);
  char *pipe = strchr(name, '|');
  if (!pipe) return;

  char *parent_id = g_strndup(name, pipe - name);
  const char *thread_id = pipe + 1;
  PurpleAccount *account = purple_conversation_get_account(conv);

  PurpleConversation *pconv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, parent_id, account);
  if (!pconv) {
      PurpleConnection *gc = purple_account_get_connection(account);
      if (gc) {
          GHashTable *comps = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
          g_hash_table_insert(comps, g_strdup("room_id"), g_strdup(parent_id));
          serv_join_chat(gc, comps);
          g_hash_table_destroy(comps);
          pconv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, parent_id, account);
      }
  }

  if (pconv) {
      purple_conversation_present(pconv);
      PidginConversation *gtkconv = PIDGIN_CONVERSATION(pconv);
      if (gtkconv && gtkconv->imhtml) {
          GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
          char *mark_name = g_strdup_printf("mxid_%s", thread_id);
          GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, mark_name);
          if (mark) {
              gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(gtkconv->imhtml), mark, 0.0, TRUE, 0.5, 0.0);
              /* Also flash the line if possible, but scrolling is most important */
          }
          g_free(mark_name);
      }
  }
  g_free(parent_id);
}

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
static void on_menu_bootstrap_crypto(gpointer d) {
  PurpleConversation *conv = (PurpleConversation *)d;
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_bootstrap_cross_signing(purple_account_get_username(account));
}

static void on_menu_crypto_status(gpointer d) {
  PurpleConversation *conv = (PurpleConversation *)d;
  PurpleAccount *account = purple_conversation_get_account(conv);
  purple_matrix_rust_debug_crypto_status(purple_account_get_username(account));
}

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
  GtkTextIter line_start = *iter;
  gtk_text_iter_set_line_offset(&line_start, 0);

  g_free(purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY));
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, NULL);
  g_free(purple_conversation_get_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY));
  purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_SENDER_KEY, NULL);

  while (TRUE) {
      GSList *marks = gtk_text_iter_get_marks(&line_start);
      gboolean found = FALSE;
      for (GSList *l = marks; l != NULL; l = l->next) {
          GtkTextMark *mark = GTK_TEXT_MARK(l->data);
          const char *name = gtk_text_mark_get_name(mark);
          if (name && g_str_has_prefix(name, "mxid_")) {
              const char *event_id = name + 5;
              purple_conversation_set_data(conv, MATRIX_UI_SELECTED_EVENT_ID_KEY, g_strdup(event_id));
              found = TRUE;
              break;
          }
      }
      g_slist_free(marks);
      if (found) break;

      if (!gtk_text_iter_backward_line(&line_start)) {
          break;
      }
  }

  // To find sender, we can still use the heuristic loop for the last 10 messages if we want
  // Or just rely on the ID since that's the most critical part. 
  // Let's keep the sender logic via the recent 10 messages array if we really need it, but mostly we only need the ID.
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

  item = gtk_menu_item_new_with_label("Bootstrap Cross-Signing");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_bootstrap_crypto), conv);
  gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
  gtk_widget_show(item);

  item = gtk_menu_item_new_with_label("E2EE Status");
  g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_crypto_status), conv);
  gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
  gtk_widget_show(item);

  if (strchr(purple_conversation_get_name(conv), '|')) {
    item = gtk_menu_item_new_with_label("Jump to Parent Message");
    g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(on_menu_jump_to_parent), conv);
    gtk_menu_shell_append(GTK_MENU_SHELL(matrix_menu), item);
    gtk_widget_show(item);
  }
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

  char *mark_name = g_strdup_printf("mxid_%s", event_id);
  GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, mark_name);
  if (mark) {
      GtkTextIter line_start, line_end, body_start, paren_end;
      gtk_text_buffer_get_iter_at_mark(buffer, &line_start, mark);
      
      line_end = line_start;
      if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);
      
      /* Find colon after sender to preserve prefix (timestamp/sender) */
      body_start = line_start;
      if (gtk_text_iter_forward_search(&body_start, ") ", 0, &paren_end, &body_start, &line_end)) {
          if (gtk_text_iter_forward_search(&body_start, ": ", 0, &body_start, &body_start, &line_end)) {
              line_start = body_start;
          }
      }

      gtk_text_buffer_delete(buffer, &line_start, &line_end);
      gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), new_msg, 0, &line_start);
  }
  g_free(mark_name);
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

  char rx_mark_name[128];
  g_snprintf(rx_mark_name, sizeof(rx_mark_name), "rxmark_%s", event_id);
  GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, rx_mark_name);
  if (mark) {
      GtkTextIter start, block_end;
      gtk_text_buffer_get_iter_at_mark(buffer, &start, mark);
      
      block_end = start;
      if (!gtk_text_iter_ends_line(&block_end)) gtk_text_iter_forward_to_line_end(&block_end);
      
      /* Delete existing reactions if present */
      gtk_text_buffer_delete(buffer, &start, &block_end);

      if (reactions_text && strlen(reactions_text) > 0) {
          const char *raw_text = reactions_text;
          if (g_str_has_prefix(raw_text, "[System] [Reactions] ")) raw_text += 21;
          
          char *esc_reactions = g_markup_escape_text(raw_text, -1);
          char *markup = g_strdup_printf(" <font color='#666666' back='#eeeeee'>&nbsp;[Reactions: %s]&nbsp;</font>", esc_reactions);
          
          gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), markup, 0, &start);
          g_free(markup);
          g_free(esc_reactions);
      }
  }
}

static void handle_message_redacted_cb(const char *room_id, const char *event_id, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->imhtml) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
  if (!buffer) return;

  char *mark_name = g_strdup_printf("mxid_%s", event_id);
  GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, mark_name);
  if (mark) {
      GtkTextIter line_start, line_end;
      gtk_text_buffer_get_iter_at_mark(buffer, &line_start, mark);
      
      line_end = line_start;
      if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);
      
      gtk_text_buffer_delete(buffer, &line_start, &line_end);
      gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), "<span color='#aa0000'><i>[Message deleted]</i></span>", 0, &line_start);
  }
  g_free(mark_name);
}

#if !GLIB_CHECK_VERSION(2, 68, 0)
static gpointer local_memdup2(gconstpointer mem, gsize byte_size) {
  gpointer new_mem;
  if (mem && byte_size != 0) {
    new_mem = g_malloc(byte_size);
    memcpy(new_mem, mem, byte_size);
  } else {
    new_mem = NULL;
  }
  return new_mem;
}
#define g_memdup2 local_memdup2
#endif

static void handle_media_downloaded_cb(const char *room_id, const char *event_id, const unsigned char *data, size_t size, const char *content_type, gpointer user_data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;
  PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
  if (!gtkconv || !gtkconv->imhtml) return;
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
  if (!buffer) return;

  char *mark_name = g_strdup_printf("mxid_%s", event_id);
  GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, mark_name);
  if (mark) {
      GtkTextIter line_start, line_end;
      gtk_text_buffer_get_iter_at_mark(buffer, &line_start, mark);
      
      line_end = line_start;
      if (!gtk_text_iter_ends_line(&line_end)) gtk_text_iter_forward_to_line_end(&line_end);
      
      /* Find placeholder text: " (Downloading...)" and replace the whole line body or just placeholder */
      /* For simplicity, let's replace the whole message content if it was an image placeholder */
      char *line_text = gtk_text_buffer_get_text(buffer, &line_start, &line_end, FALSE);
      if (line_text && strstr(line_text, " (Downloading...)")) {
          /* Extract original description from "🖼️ [Image: DESC] (Downloading...)" */
          const char *start_desc = strstr(line_text, "[Image: ");
          char *desc = NULL;
          if (start_desc) {
              start_desc += 8;
              const char *end_desc = strstr(start_desc, "] (Downloading...)");
              if (end_desc) {
                  desc = g_strndup(start_desc, end_desc - start_desc);
              }
          }

          int img_id = purple_imgstore_add_with_id(g_memdup2(data, size), size, NULL);
          if (img_id > 0) {
              char *img_html = g_strdup_printf("<img id=\"%d\" alt=\"%s\">", img_id, desc ? desc : "image");
              
              /* Preserve the mark by inserting after it */
              gtk_text_buffer_delete(buffer, &line_start, &line_end);
              gtk_imhtml_insert_html_at_iter(GTK_IMHTML(gtkconv->imhtml), img_html, 0, &line_start);
              g_free(img_html);
              
              /* The store now owns the data, we release our reference. 
                 The IMHtml widget will ref it when displayed. */
              purple_imgstore_unref_by_id(img_id);
          }
          g_free(desc);
      }
      g_free(line_text);
  }
  g_free(mark_name);
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

static GHashTable *typing_users_map = NULL;

typedef struct {
    char *room_id;
    char *who;
} TypingTimeoutCtx;

static gboolean typing_remove_timeout_cb(gpointer data) {
    TypingTimeoutCtx *ctx = (TypingTimeoutCtx *)data;
    handle_room_typing_cb(ctx->room_id, ctx->who, FALSE, NULL);
    g_free(ctx->room_id);
    g_free(ctx->who);
    g_free(ctx);
    return FALSE;
}

static void handle_room_typing_cb(const char *room_id, const char *who, gboolean is_typing, gpointer data) {
  PurpleAccount *account = local_find_matrix_account();
  if (!account) return;
  PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_ANY, room_id, account);
  if (!conv) return;

  if (!typing_users_map) {
      typing_users_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_destroy);
  }

  GHashTable *room_typing = g_hash_table_lookup(typing_users_map, room_id);
  if (!room_typing) {
      room_typing = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
      g_hash_table_insert(typing_users_map, g_strdup(room_id), room_typing);
  }

  if (is_typing) {
      /* If there was a pending removal, cancel it */
      guint timeout_id = GPOINTER_TO_UINT(g_hash_table_lookup(room_typing, who));
      if (timeout_id > 1) {
          g_source_remove(timeout_id);
      }
      g_hash_table_insert(room_typing, g_strdup(who), GUINT_TO_POINTER(1));
  } else {
      /* Debounce removal by 1 second */
      if (data == NULL) { /* NULL data means this IS the timeout callback calling us */
          g_hash_table_remove(room_typing, who);
      } else {
          TypingTimeoutCtx *ctx = g_new0(TypingTimeoutCtx, 1);
          ctx->room_id = g_strdup(room_id);
          ctx->who = g_strdup(who);
          guint id = g_timeout_add(1000, typing_remove_timeout_cb, ctx);
          g_hash_table_insert(room_typing, g_strdup(who), GUINT_TO_POINTER(id));
          return; /* Don't update UI yet */
      }
  }

  GtkWidget *label = purple_conversation_get_data(conv, MATRIX_UI_TYPING_LABEL_KEY);
  if (label && GTK_IS_LABEL(label)) {
      guint size = g_hash_table_size(room_typing);
      if (size == 0) {
          gtk_label_set_text(GTK_LABEL(label), "");
      } else if (size == 1) {
          GList *keys = g_hash_table_get_keys(room_typing);
          char *esc_who = g_markup_escape_text((const char *)keys->data, -1);
          char *txt = g_strdup_printf("<span size='smaller' color='#666'><i>%s is typing...</i></span>", esc_who);
          gtk_label_set_markup(GTK_LABEL(label), txt);
          g_free(txt); g_free(esc_who);
          g_list_free(keys);
      } else if (size < 4) {
          GString *s = g_string_new("<span size='smaller' color='#666'><i>");
          GList *keys = g_hash_table_get_keys(room_typing);
          for (GList *it = keys; it != NULL; it = it->next) {
              char *esc = g_markup_escape_text((const char *)it->data, -1);
              g_string_append(s, esc);
              g_free(esc);
              if (it->next) g_string_append(s, ", ");
          }
          g_string_append(s, " are typing...</i></span>");
          gtk_label_set_markup(GTK_LABEL(label), s->str);
          g_string_free(s, TRUE);
          g_list_free(keys);
      } else {
          gtk_label_set_markup(GTK_LABEL(label), "<span size='smaller' color='#666'><i>Several people are typing...</i></span>");
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

typedef struct {
    PurpleConversation *conv;
    char *event_id;
} PendingMark;

static gboolean set_marks_idle_cb(gpointer data) {
    PendingMark *pm = (PendingMark *)data;
    PidginConversation *gtkconv = PIDGIN_CONVERSATION(pm->conv);
    if (gtkconv && gtkconv->imhtml) {
        GtkTextBuffer *tbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtkconv->imhtml));
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(tbuf, &end);
        
        char mark_name[128], rx_mark_name[128];
        g_snprintf(mark_name, sizeof(mark_name), "mxid_%s", pm->event_id);
        g_snprintf(rx_mark_name, sizeof(rx_mark_name), "rxmark_%s", pm->event_id);
        
        /* mxid mark at start of message (already exists if we set it in writing_msg_cb, but let's reposition it) */
        /* Actually, let's find the start of the line. */
        GtkTextIter start = end;
        gtk_text_iter_set_line_offset(&start, 0);
        
        gtk_text_buffer_create_mark(tbuf, mark_name, &start, TRUE);
        gtk_text_buffer_create_mark(tbuf, rx_mark_name, &end, TRUE);
    }
    g_free(pm->event_id);
    g_free(pm);
    return FALSE;
}

static gboolean writing_msg_cb(PurpleAccount *account, const char *who, char **buffer, PurpleConversation *conv, PurpleMessageFlags flags, gpointer data) {
    char *event_id = purple_conversation_get_data(conv, "pending_event_id");
    if (event_id) {
        PendingMark *pm = g_new0(PendingMark, 1);
        pm->conv = conv;
        pm->event_id = g_strdup(event_id);
        g_idle_add(set_marks_idle_cb, pm);
        
        g_free(event_id);
        purple_conversation_set_data(conv, "pending_event_id", NULL);
    }
    return FALSE;
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
    purple_signal_connect(conv_handle, "writing-im-msg", plugin, PURPLE_CALLBACK(writing_msg_cb), NULL);
    purple_signal_connect(conv_handle, "writing-chat-msg", plugin, PURPLE_CALLBACK(writing_msg_cb), NULL);
  }
  
  if (matrix_plugin) {
    purple_signal_connect(matrix_plugin, "matrix-ui-room-typing", plugin, PURPLE_CALLBACK(handle_room_typing_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-encrypted", plugin, PURPLE_CALLBACK(handle_room_encrypted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-muted", plugin, PURPLE_CALLBACK(handle_room_muted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-room-activity", plugin, PURPLE_CALLBACK(handle_room_activity_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-reactions-changed", plugin, PURPLE_CALLBACK(handle_reactions_changed_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-message-edited", plugin, PURPLE_CALLBACK(handle_message_edited_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-message-redacted", plugin, PURPLE_CALLBACK(handle_message_redacted_cb), NULL);
    purple_signal_connect(matrix_plugin, "matrix-ui-media-downloaded", plugin, PURPLE_CALLBACK(handle_media_downloaded_cb), NULL);
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
