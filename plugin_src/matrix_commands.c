#include "matrix_commands.h"
#include "matrix_ffi_wrappers.h"
#include "matrix_globals.h"
#include "matrix_types.h"
#include "matrix_utils.h"

#include <libpurple/debug.h>
#include <libpurple/notify.h>
#include <libpurple/request.h>
#include <libpurple/server.h>
#include <libpurple/util.h>
#include <libpurple/version.h>
#include <string.h>

typedef struct {
  char *room_id;
  char *event_id;
  int action_map[20];
} MatrixEventPickCtx;

typedef struct {
  char *room_id;
  char *event_id;
} MatrixUiReactCtx;

static void matrix_ui_react_custom_cb(void *user_data, const char *text) {
  MatrixUiReactCtx *ctx = (MatrixUiReactCtx *)user_data;
  if (ctx) {
    if (text && *text) {
      purple_matrix_rust_send_reaction(purple_account_get_username(find_matrix_account()),
                                       ctx->room_id, ctx->event_id, text);
    }
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_react_action_cb(void *user_data, int action) {
  MatrixUiReactCtx *ctx = (MatrixUiReactCtx *)user_data;
  if (!ctx) return;

  const char *emojis[] = {"👍", "❤️", "😂", "😮", "😢", "🔥", "✅", "❌"};
  
  if (action >= 0 && action < 8) {
    purple_matrix_rust_send_reaction(purple_account_get_username(find_matrix_account()),
                                     ctx->room_id, ctx->event_id, emojis[action]);
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  } else if (action == 8) {
    /* Custom emoji path - keep ctx and open input dialog */
    purple_request_input(
        my_plugin, "Add Reaction", "Enter Emoji or Text",
        "Type or paste an emoji to react to this message.", NULL, FALSE, FALSE,
        NULL, "_React", G_CALLBACK(matrix_ui_react_custom_cb), "_Cancel",
        G_CALLBACK(matrix_ui_react_custom_cb), find_matrix_account(), NULL,
        NULL, ctx);
  } else {
    g_free(ctx->room_id);
    g_free(ctx->event_id);
    g_free(ctx);
  }
}

static void matrix_ui_react_action_0_cb(void *d) { matrix_ui_react_action_cb(d, 0); }
static void matrix_ui_react_action_1_cb(void *d) { matrix_ui_react_action_cb(d, 1); }
static void matrix_ui_react_action_2_cb(void *d) { matrix_ui_react_action_cb(d, 2); }
static void matrix_ui_react_action_3_cb(void *d) { matrix_ui_react_action_cb(d, 3); }
static void matrix_ui_react_action_4_cb(void *d) { matrix_ui_react_action_cb(d, 4); }
static void matrix_ui_react_action_5_cb(void *d) { matrix_ui_react_action_cb(d, 5); }
static void matrix_ui_react_action_6_cb(void *d) { matrix_ui_react_action_cb(d, 6); }
static void matrix_ui_react_action_7_cb(void *d) { matrix_ui_react_action_cb(d, 7); }
static void matrix_ui_react_action_8_cb(void *d) { matrix_ui_react_action_cb(d, 8); }
static void matrix_ui_react_cancel_cb(void *d) { matrix_ui_react_action_cb(d, -1); }

static void matrix_ui_open_reaction_picker(const char *room_id,
                                           const char *event_id) {
  MatrixUiReactCtx *ctx = g_new0(MatrixUiReactCtx, 1);
  ctx->room_id = g_strdup(room_id);
  ctx->event_id = g_strdup(event_id);

  /* Hybrid UX: Fast buttons for common emojis + Custom option */
  purple_request_action(
      my_plugin, "Add Reaction", "Choose a reaction",
      "Select one of the common emojis or enter a custom one.\n(Tip: For custom emojis, you can use Super+. to open your system picker and paste it here.)", 0,
      find_matrix_account(), NULL, NULL, ctx, 10, 
      "👍", G_CALLBACK(matrix_ui_react_action_0_cb), 
      "❤️", G_CALLBACK(matrix_ui_react_action_1_cb), 
      "😂", G_CALLBACK(matrix_ui_react_action_2_cb), 
      "😮", G_CALLBACK(matrix_ui_react_action_3_cb), 
      "😢", G_CALLBACK(matrix_ui_react_action_4_cb), 
      "🔥", G_CALLBACK(matrix_ui_react_action_5_cb), 
      "✅", G_CALLBACK(matrix_ui_react_action_6_cb), 
      "❌", G_CALLBACK(matrix_ui_react_action_7_cb),
      "Custom...", G_CALLBACK(matrix_ui_react_action_8_cb), 
      "_Cancel", G_CALLBACK(matrix_ui_react_cancel_cb));
}

static void matrix_ui_open_event_action_dialog(const char *room_id,
                                               const char *event_id,
                                               MatrixEventPickAction action) {
  /* This is a placeholder for a more unified event action dialog if needed */
}

void register_matrix_commands(PurplePlugin *plugin) {
  /* Commands already registered in matrix_core.c via call to this */
}
