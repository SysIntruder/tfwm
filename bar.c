#include <string.h>

#include "bar.h"
#include "config.h"

static char *tfwm_bar_workspace_str(int cur_ws) {
  size_t n = 0;

  /* Workspace Layout Display */
  uint16_t layout = workspaces[cur_ws].default_layout;
  char *ld = malloc(0);
  char *ldr;
  switch (layout) {
  case (TILING):
    n += strlen(LAYOUT_TILING_DISPLAY);
    ldr = realloc(ld, (strlen(LAYOUT_TILING_DISPLAY) + 1));
    ld = ldr;
    ld = LAYOUT_TILING_DISPLAY;
    break;
  case (FLOATING):
    n += strlen(LAYOUT_FLOATING_DISPLAY);
    ldr = realloc(ld, (strlen(LAYOUT_FLOATING_DISPLAY) + 1));
    ld = ldr;
    ld = LAYOUT_FLOATING_DISPLAY;
    break;
  case (WINDOW):
    n += strlen(LAYOUT_WINDOW_DISPLAY);
    ldr = realloc(ld, (strlen(LAYOUT_WINDOW_DISPLAY) + 1));
    ld = ldr;
    ld = LAYOUT_WINDOW_DISPLAY;
    break;
  }

  if (strlen(ld) > 0) {
    n += 1;
  }

  /* Workspace List */
  size_t wsize = (sizeof(workspaces) / sizeof(*workspaces));

  for (size_t i = 0; i < wsize; i++) {
    n += strlen(workspaces[i].name);
    if (i < (wsize - 1)) {
      n += 3;
    }
  }
  n += 1;

  /* Combine String */
  char *res = malloc(n);
  res[0] = '\0';

  if (strlen(ld) > 0) {
    strcat(res, ld);
    strcat(res, " ");
  }

  for (size_t i = 0; i < wsize; i++) {
    if (i == cur_ws) {
      strcat(res, "[");
      strcat(res, workspaces[i].name);
      strcat(res, "]");
    } else {
      strcat(res, " ");
      strcat(res, workspaces[i].name);
      strcat(res, " ");
    }

    if (i < (wsize - 1)) {
      strcat(res, " ");
    }
  }

  return res;
}

void tfwm_bar(xcb_connection_t *conn, xcb_screen_t *scrn, int cur_ws) {
  xcb_generic_error_t *err;

  xcb_font_t f = xcb_generate_id(conn);
  xcb_void_cookie_t fc =
      xcb_open_font_checked(conn, f, strlen(BAR_FONT_NAME), BAR_FONT_NAME);
  err = xcb_request_check(conn, fc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_open_font_checked\n");
    return;
  }

  xcb_gcontext_t gc = xcb_generate_id(conn);
  uint32_t vals[4];
  vals[0] = BAR_FOREGROUND;
  vals[1] = BAR_BACKGROUND;
  vals[2] = f;
  vals[3] = 0;
  xcb_void_cookie_t gcc =
      xcb_create_gc_checked(conn, gc, scrn->root,
                            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND |
                                XCB_GC_FONT | XCB_GC_GRAPHICS_EXPOSURES,
                            vals);
  err = xcb_request_check(conn, gcc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_create_gc_checked\n");
    return;
  }

  fc = xcb_close_font_checked(conn, f);
  err = xcb_request_check(conn, fc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_close_font_checked\n");
    return;
  }

  char *label = tfwm_bar_workspace_str(cur_ws);

  xcb_rectangle_t bar = {0, (scrn->height_in_pixels - BAR_HEIGHT),
                         scrn->width_in_pixels, BAR_HEIGHT};

  xcb_void_cookie_t bc =
      xcb_poly_fill_rectangle_checked(conn, scrn->root, gc, 1, &bar);
  err = xcb_request_check(conn, bc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_poly_fill_rectangle_checked\n");
    return;
  }

  xcb_void_cookie_t tc = xcb_image_text_8_checked(
      conn, strlen(label), scrn->root, gc, 0, scrn->height_in_pixels, label);
  err = xcb_request_check(conn, tc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_image_text_8_checked\n");
    return;
  }

  gcc = xcb_free_gc(conn, gc);
  err = xcb_request_check(conn, gcc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_free_gc\n");
    return;
  }

  xcb_flush(conn);
}
