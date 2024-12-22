#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "config.h"
#include "tfwm.h"

static xcb_connection_t *gp_conn;
static xcb_screen_t *gp_scrn;
static xcb_window_t gp_win;

static uint32_t gp_vals[3];
static int gp_cur_ws;
static int gp_prev_ws;
static int gp_debug_on;

/* ========================== UTILS ========================== */

static int tfwm_util_write_error(char *err) {
  size_t n = 0;
  char *e = err;
  while ((*(e++)) != 0) {
    ++n;
  }

  ssize_t out = write(STDERR_FILENO, err, n);
  int ret = 1;
  if (out < 0) {
    ret = -1;
  }
  return ret;
}

static int tfwm_util_compare_str(char *str1, char *str2) {
  char *c1 = str1;
  char *c2 = str2;
  while ((*c1) && ((*c1) == (*c2))) {
    ++c1;
    ++c2;
  }

  int n = (*c1) - (*c2);
  return n;
}

static xcb_keycode_t *tfwm_get_keycodes(xcb_keysym_t keysym) {
  xcb_key_symbols_t *ks = xcb_key_symbols_alloc(gp_conn);
  xcb_keycode_t *keycode =
      (!(ks) ? NULL : xcb_key_symbols_get_keycode(ks, keysym));
  xcb_key_symbols_free(ks);
  return keycode;
}

static xcb_keysym_t tfwm_get_keysym(xcb_keycode_t keycode) {
  xcb_key_symbols_t *ks = xcb_key_symbols_alloc(gp_conn);
  xcb_keysym_t keysym =
      (!(ks) ? 0 : xcb_key_symbols_get_keysym(ks, keycode, 0));
  xcb_key_symbols_free(ks);
  return keysym;
}

/* ===================== WINDOW FUNCTION ===================== */

static void tfwm_exit(char **cmd) {
  if (gp_conn) {
    xcb_disconnect(gp_conn);
  }
}

static void tfwm_spawn(char **cmd) {
  if (fork() == 0) {
    setsid();
    if (fork() != 0) {
      _exit(0);
    }
    execvp((char *)cmd[0], (char **)cmd);
    _exit(0);
  }
  wait(NULL);
}

static void tfwm_kill(char **cmd) { xcb_kill_client(gp_conn, gp_win); }

static void tfwm_focus_window(xcb_window_t win) {
  if (win == 0) {
    return;
  }
  if (win == gp_scrn->root) {
    return;
  }

  xcb_set_input_focus(gp_conn, XCB_INPUT_FOCUS_POINTER_ROOT, win,
                      XCB_CURRENT_TIME);
}

static void tfwm_focus_color_window(xcb_window_t win, int focus) {
  if (BORDER_WIDTH <= 0) {
    return;
  }
  if (win == 0) {
    return;
  }
  if (win == gp_scrn->root) {
    return;
  }

  uint32_t vals[1];
  vals[0] = focus ? BORDER_ACTIVE : BORDER_INACTIVE;
  xcb_change_window_attributes(gp_conn, win, XCB_CW_BORDER_PIXEL, vals);
  xcb_flush(gp_conn);
}

static void tfwm_map_window(xcb_window_t win) {
  xcb_map_window(gp_conn, win);
  xcb_flush(gp_conn);
}

static void tfwm_unmap_window(xcb_window_t win) {
  xcb_unmap_window(gp_conn, win);
  xcb_flush(gp_conn);
}

/* =================== WORKSPACE FUNCTION ==================== */

static void tfwm_goto_workspace(char **cmd) {
  gp_prev_ws = gp_cur_ws;
  for (int i = 0; i < sizeof(workspaces) / sizeof(*workspaces); ++i) {
    char *ws = (char *)cmd[0];
    if (tfwm_util_compare_str(ws, workspaces[i].name) == 0) {
      gp_cur_ws = i;
    }
  }

  tfwm_remap_workspace();
}

static void tfwm_swap_prev_workspace(char **cmd) {
  gp_prev_ws = gp_prev_ws ^ gp_cur_ws;
  gp_cur_ws = gp_prev_ws ^ gp_cur_ws;
  gp_prev_ws = gp_prev_ws ^ gp_cur_ws;

  tfwm_remap_workspace();
}

static void tfwm_remap_workspace(void) {
  if (gp_cur_ws == gp_prev_ws) {
    return;
  }

  for (int i = 0; i < workspaces[gp_prev_ws].windows_len; i++) {
    tfwm_unmap_window(workspaces[gp_prev_ws].windows[i]);
  }

  for (int i = 0; i < workspaces[gp_cur_ws].windows_len; i++) {
    tfwm_map_window(workspaces[gp_cur_ws].windows[i]);
  }
}

static void tfwm_workspace_use_tiling(char **cmd) {
  workspaces[gp_cur_ws].default_layout = TILING;
}

static void tfwm_workspace_use_floating(char **cmd) {
  workspaces[gp_cur_ws].default_layout = FLOATING;
}

static void tfwm_workspace_use_window(char **cmd) {
  workspaces[gp_cur_ws].default_layout = WINDOW;
}

static void tfwm_workspace_window_malloc(tfwm_workspace_t *ws) {
  if (ws->windows) {
    return;
  }

  ws->windows = (xcb_window_t *)malloc(1 * sizeof(xcb_window_t));
  ws->windows_cap = 1;
}

static void tfwm_workspace_window_realloc(tfwm_workspace_t *ws) {
  if (!ws->windows) {
    return;
  }

  xcb_window_t *nwin = (xcb_window_t *)realloc(
      ws->windows, (ws->windows_cap + 1) * sizeof(xcb_window_t));
  if (!nwin) {
    return;
  }

  ws->windows = nwin;
  ws->windows_cap += 1;
}

static void tfwm_workspace_window_append(tfwm_workspace_t *ws, xcb_window_t w) {
  ws->windows[ws->windows_len++] = w;
}

/* ====================== EVENT HANDLER ====================== */

static void tfwm_handle_keypress(xcb_generic_event_t *evt) {
  xcb_key_press_event_t *e = (xcb_key_press_event_t *)evt;
  xcb_keysym_t keysym = tfwm_get_keysym(e->detail);
  gp_win = e->child;
  for (int i = 0; i < sizeof(keybinds) / sizeof(*keybinds); ++i) {
    if ((keybinds[i].keysym == keysym) && (keybinds[i].mod == e->state)) {
      keybinds[i].func(keybinds[i].cmd);
    }
  }
}

static void tfwm_handle_map_request(xcb_generic_event_t *evt) {
  xcb_map_request_event_t *e = (xcb_map_request_event_t *)evt;

  tfwm_workspace_t *ws = &workspaces[gp_cur_ws];
  if (!ws->windows) {
    tfwm_workspace_window_malloc(ws);
  } else if (ws->windows_cap == ws->windows_len) {
    tfwm_workspace_window_realloc(ws);
  }
  tfwm_workspace_window_append(ws, e->window);

  tfwm_map_window(e->window);
  uint32_t vals[5];
  vals[0] = (gp_scrn->width_in_pixels / 2) - (WINDOW_WIDTH / 2);
  vals[1] = (gp_scrn->height_in_pixels / 2) - (WINDOW_HEIGHT / 2);
  vals[2] = WINDOW_WIDTH;
  vals[3] = WINDOW_HEIGHT;
  vals[4] = BORDER_WIDTH;
  xcb_configure_window(gp_conn, e->window,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                           XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                           XCB_CONFIG_WINDOW_BORDER_WIDTH,
                       vals);

  gp_vals[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
  xcb_change_window_attributes_checked(gp_conn, e->window, XCB_CW_EVENT_MASK,
                                       gp_vals);
  tfwm_focus_window(e->window);
}

static void tfwm_handle_focus_in(xcb_generic_event_t *evt) {
  xcb_focus_in_event_t *e = (xcb_focus_in_event_t *)evt;
  tfwm_focus_color_window(e->event, 1);
}

static void tfwm_handle_focus_out(xcb_generic_event_t *evt) {
  xcb_focus_out_event_t *e = (xcb_focus_out_event_t *)evt;
  tfwm_focus_color_window(e->event, 0);
}

static int tfwm_handle_event(void) {
  int ret = xcb_connection_has_error(gp_conn);
  if (ret != 0) {
    return ret;
  }

  xcb_generic_event_t *evt = xcb_wait_for_event(gp_conn);
  if (!evt) {
    return ret;
  }

  tfwm_event_handler_t *handler;
  for (handler = event_handlers; handler->func; handler++) {
    if ((evt->response_type & ~0x80) == handler->request) {
      handler->func(evt);
    }
  }

  free(evt);
  xcb_flush(gp_conn);
  return ret;
}

/* ========================== DEBUG ========================== */

static char *tfwm_debug_workspace_str(void) {
  size_t n = 0;

  /* Workspace Layout Display */
  uint16_t layout = workspaces[gp_cur_ws].default_layout;
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
      n += 1;
    }
  }
  n += 3;

  /* Combine String */
  char *res = malloc(n);
  res[0] = '\0';

  if (strlen(ld) > 0) {
    strcat(res, ld);
    strcat(res, " ");
  }

  for (size_t i = 0; i < wsize; i++) {
    if (i == gp_cur_ws) {
      strcat(res, "[");
      strcat(res, workspaces[i].name);
      strcat(res, "]");
    } else {
      strcat(res, workspaces[i].name);
    }

    if (i < (wsize - 1)) {
      strcat(res, " ");
    }
  }

  return res;
}

static void tfwm_debug_hud(void) {
  xcb_generic_error_t *err;

  xcb_font_t f = xcb_generate_id(gp_conn);
  xcb_void_cookie_t fc =
      xcb_open_font_checked(gp_conn, f, strlen(BAR_FONT_NAME), BAR_FONT_NAME);
  err = xcb_request_check(gp_conn, fc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_open_font_checked\n");
    return;
  }

  xcb_gcontext_t gc = xcb_generate_id(gp_conn);
  uint32_t vals[4];
  vals[0] = BAR_FOREGROUND;
  vals[1] = BAR_BACKGROUND;
  vals[2] = f;
  vals[3] = 0;
  xcb_void_cookie_t gcc =
      xcb_create_gc_checked(gp_conn, gc, gp_scrn->root,
                            XCB_GC_FOREGROUND | XCB_GC_BACKGROUND |
                                XCB_GC_FONT | XCB_GC_GRAPHICS_EXPOSURES,
                            vals);
  err = xcb_request_check(gp_conn, gcc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_create_gc_checked\n");
    return;
  }

  fc = xcb_close_font_checked(gp_conn, f);
  err = xcb_request_check(gp_conn, fc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_close_font_checked\n");
    return;
  }

  char *label = tfwm_debug_workspace_str();

  xcb_void_cookie_t tc =
      xcb_image_text_8_checked(gp_conn, strlen(label), gp_scrn->root, gc, 0,
                               gp_scrn->height_in_pixels, label);
  err = xcb_request_check(gp_conn, tc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_image_text_8_checked\n");
    return;
  }

  gcc = xcb_free_gc(gp_conn, gc);
  err = xcb_request_check(gp_conn, gcc);
  if (err) {
    tfwm_util_write_error("ERROR: xcb_free_gc\n");
    return;
  }

  xcb_flush(gp_conn);
}

/* ========================== SETUP ========================== */

static void tfwm_init(void) {
  gp_vals[0] =
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
  xcb_change_window_attributes_checked(gp_conn, gp_scrn->root,
                                       XCB_CW_EVENT_MASK, gp_vals);

  xcb_ungrab_key(gp_conn, XCB_GRAB_ANY, gp_scrn->root, XCB_MOD_MASK_ANY);
  for (int i = 0; i < sizeof(keybinds) / sizeof(*keybinds); ++i) {
    xcb_keycode_t *keycode = tfwm_get_keycodes(keybinds[i].keysym);
    if (keycode != NULL) {
      xcb_grab_key(gp_conn, 1, gp_scrn->root, keybinds[i].mod, *keycode,
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
  }
  xcb_flush(gp_conn);

  xcb_grab_button(gp_conn, 0, gp_scrn->root,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, gp_scrn->root,
                  XCB_NONE, 1, MOD_KEY);
  xcb_grab_button(gp_conn, 0, gp_scrn->root,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, gp_scrn->root,
                  XCB_NONE, 3, MOD_KEY);
  xcb_flush(gp_conn);
}

int main(int argc, char *argv[]) {
  int ret = 0;
  if ((argc == 2) && (tfwm_util_compare_str("-v", argv[1]) == 0)) {
    ret = tfwm_util_write_error(
        "tfwm-0.0.1, Copyright (c) 2024 Raihan Rahardyan, MIT License\n");
  }

  if ((ret == 0) && (argc != 1)) {
    ret = tfwm_util_write_error("usage: tfwm [-v]\n");
  }

  if (ret == 0) {
    gp_conn = xcb_connect(NULL, NULL);
    ret = xcb_connection_has_error(gp_conn);
    if (ret > 0) {
      ret = tfwm_util_write_error("xcb_connection_has_error\n");
    }
  }

  if (ret == 0) {
    gp_scrn = xcb_setup_roots_iterator(xcb_get_setup(gp_conn)).data;
    tfwm_init();
  }

  gp_debug_on = 1;

  while (ret == 0) {
    ret = tfwm_handle_event();

    if (gp_debug_on == 1) {
      if (gp_scrn) {
        tfwm_debug_hud();
      }
    }
  }

  return ret;
}
