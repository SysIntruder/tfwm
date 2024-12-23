#include <stdlib.h>
#include <string.h>
#include <xcb/xproto.h>

#include "bar.h"
#include "config.h"

static int tfwm_bar_text_width(xcb_connection_t *conn, xcb_font_t f, char *t) {
  int i;
  size_t t_len = strlen(t);
  xcb_char2b_t *bt = malloc(t_len * sizeof(xcb_char2b_t));
  for (i = 0; i < t_len; i++) {
    bt[i].byte1 = 0;
    bt[i].byte2 = t[i];
  }

  xcb_query_text_extents_cookie_t c =
      xcb_query_text_extents(conn, f, t_len, bt);
  xcb_query_text_extents_reply_t *te = xcb_query_text_extents_reply(conn, c, 0);
  if (!te) {
    return 0;
  }

  int width = te->overall_width;
  free(te);
  return width;
}

static char *tfwm_bar_layout_str(int cur_ws) {
  uint16_t layout = workspaces[cur_ws].default_layout;
  char *res = malloc(2);
  res[0] = '\0';
  char *tmp;

  strcat(res, " ");

  switch (layout) {
  case (TILING):
    tmp = realloc(res, (strlen(LAYOUT_TILING_DISPLAY) + 1));
    res = tmp;
    strcat(res, LAYOUT_TILING_DISPLAY);
    break;
  case (FLOATING):
    tmp = realloc(res, (strlen(LAYOUT_FLOATING_DISPLAY) + 1));
    res = tmp;
    strcat(res, LAYOUT_FLOATING_DISPLAY);
    break;
  case (WINDOW):
    tmp = realloc(res, (strlen(LAYOUT_WINDOW_DISPLAY) + 1));
    res = tmp;
    strcat(res, LAYOUT_WINDOW_DISPLAY);
    break;
  }

  strcat(res, " ");

  return res;
}

static char *tfwm_bar_workspace_str(int cur_ws) {
  size_t n = 2;

  size_t w_len = (sizeof(workspaces) / sizeof(*workspaces));

  for (size_t i = 0; i < w_len; i++) {
    n += strlen(workspaces[i].name);
    if (i < (w_len - 1)) {
      n += 3;
    }
  }
  n += 1;

  /* Combine String */
  char *res = malloc(n);
  res[0] = '\0';

  strcat(res, " ");
  for (size_t i = 0; i < w_len; i++) {
    if (i == cur_ws) {
      strcat(res, "[");
      strcat(res, workspaces[i].name);
      strcat(res, "]");
    } else {
      strcat(res, " ");
      strcat(res, workspaces[i].name);
      strcat(res, " ");
    }

    if (i < (w_len - 1)) {
      strcat(res, " ");
    }
  }
  strcat(res, " ");

  return res;
}

void tfwm_bar(xcb_connection_t *conn, xcb_screen_t *scrn, int cur_ws) {
  xcb_generic_error_t *err;

  xcb_font_t f = xcb_generate_id(conn);
  xcb_open_font(conn, f, strlen(BAR_FONT_NAME), BAR_FONT_NAME);

  xcb_gcontext_t gc = xcb_generate_id(conn);
  uint32_t vals[4];
  vals[0] = BAR_FOREGROUND;
  vals[1] = BAR_BACKGROUND;
  vals[2] = f;
  vals[3] = 0;
  xcb_create_gc(conn, gc, scrn->root,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT |
                    XCB_GC_GRAPHICS_EXPOSURES,
                vals);

  int pos_x = 0;

  char *layout = tfwm_bar_layout_str(cur_ws);
  xcb_image_text_8(conn, strlen(layout), scrn->root, gc, pos_x, BAR_POS_Y,
                   layout);
  pos_x += tfwm_bar_text_width(conn, f, layout);

  char *ws_list = tfwm_bar_workspace_str(cur_ws);
  xcb_image_text_8(conn, strlen(ws_list), scrn->root, gc, pos_x, BAR_POS_Y,
                   ws_list);
  pos_x += tfwm_bar_text_width(conn, f, layout);

  xcb_close_font_checked(conn, f);
  xcb_free_gc(conn, gc);

  xcb_flush(conn);
}
