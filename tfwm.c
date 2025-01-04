#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "config.h"
#include "tfwm.h"

static tfwm_xcb_t core;

/* ========================== UTILS ========================== */

static void tfwm_util_log(char *log, int exit) {
  char *homedir = getenv("HOME");
  if (!homedir) {
    return;
  }

  size_t fpath_len = strlen(homedir) + strlen(TFWM_LOG_FILE) + 2;
  char *fpath = malloc(fpath_len);
  snprintf(fpath, fpath_len, "%s/%s", homedir, TFWM_LOG_FILE);
  FILE *file;
  if (access(fpath, F_OK) != 0) {
    file = fopen(fpath, "w");
  } else {
    file = fopen(fpath, "a");
  }
  if (!file) {
    printf("ERROR: can not open log file\n");
    return;
  }
  free(fpath);

  size_t n = strlen(log);
  char timestamp[20];
  time_t now = time(NULL);
  struct tm *local_time = localtime(&now);
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", local_time);
  fprintf(file, "[%s] %s\n", timestamp, log);
  fclose(file);
  core.exit = exit;
}

static xcb_keycode_t *tfwm_util_get_keycodes(xcb_keysym_t keysym) {
  xcb_key_symbols_t *ks = xcb_key_symbols_alloc(core.conn);
  xcb_keycode_t *keycode = (!(ks) ? NULL : xcb_key_symbols_get_keycode(ks, keysym));
  xcb_key_symbols_free(ks);
  return keycode;
}

static xcb_keysym_t tfwm_util_get_keysym(xcb_keycode_t keycode) {
  xcb_key_symbols_t *ks = xcb_key_symbols_alloc(core.conn);
  xcb_keysym_t keysym = (!(ks) ? 0 : xcb_key_symbols_get_keysym(ks, keycode, 0));
  xcb_key_symbols_free(ks);
  return keysym;
}

static char *tfwm_util_window_class(xcb_window_t window) {
  xcb_intern_atom_reply_t *rep = xcb_intern_atom_reply(
      core.conn, xcb_intern_atom(core.conn, 0, strlen("WM_CLASS"), "WM_CLASS"),
      NULL);
  if (!rep) {
    return NULL;
  }
  xcb_atom_t atom = rep->atom;
  free(rep);

  xcb_get_property_reply_t *prep = xcb_get_property_reply(
      core.conn,
      xcb_get_property(core.conn, 0, window, atom, XCB_ATOM_STRING, 0, 250), NULL);
  if (!prep) {
    return NULL;
  }
  if (xcb_get_property_value_length(prep) == 0) {
    free(prep);
    return NULL;
  }

  char *wm_class = (char *)xcb_get_property_value(prep);
  wm_class = wm_class + strlen(wm_class) + 1;
  free(prep);
  if (!wm_class) {
    return "-";
  } else {
    return wm_class;
  }
}

static void tfwm_util_set_cursor(xcb_window_t win, char *name) {
  xcb_cursor_context_t *cursor_ctx;
  if (xcb_cursor_context_new(core.conn, core.screen, &cursor_ctx) < 0) {
    return;
  }
  xcb_cursor_t cursor = xcb_cursor_load_cursor(cursor_ctx, name);
  xcb_change_window_attributes(core.conn, win, XCB_CW_CURSOR, &cursor);
  xcb_cursor_context_free(cursor_ctx);
}

static int tfwm_util_text_width(char *text) {
  size_t txt_len = strlen(text);
  xcb_char2b_t *bt = malloc(txt_len * sizeof(xcb_char2b_t));
  for (int i = 0; i < txt_len; i++) {
    bt[i].byte1 = 0;
    bt[i].byte2 = text[i];
  }

  xcb_query_text_extents_reply_t *rep = xcb_query_text_extents_reply(
      core.conn, xcb_query_text_extents(core.conn, core.font, txt_len, bt), 0);
  if (!rep) {
    return 0;
  }

  int width = rep->overall_width;
  free(rep);
  return width;
}

static void tfwm_util_redraw_bar(void) {
  xcb_clear_area(core.conn, 1, core.bar, 0, 0, 0, 0);
}

static void tfwm_util_cleanup(void) {
  if (core.ws_list) {
    for (uint32_t i = 0; i < core.ws_len; i++) {
      if (core.ws_list[i].win_list) {
        free(core.ws_list[i].win_list);
      }
    }
    free(core.ws_list);
  }

  if (core.font) {
    xcb_close_font(core.conn, core.font);
  }
  if (core.gc_active) {
    xcb_free_gc(core.conn, core.gc_active);
  }
  if (core.gc_inactive) {
    xcb_free_gc(core.conn, core.gc_inactive);
  }
}

/* ========================= COMMAND ========================= */

void tfwm_exit(char **cmd) {
  if (core.conn) {
    tfwm_util_cleanup();

    xcb_disconnect(core.conn);
  }
}

void tfwm_window_spawn(char **cmd) {
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

void tfwm_window_kill(char **cmd) {
  int wid;
  uint8_t found = 0;

  for (uint32_t i = 0; i < core.ws_list[core.cur_ws].win_len; i++) {
    if (core.ws_list[core.cur_ws].win_list[i].win == core.window) {
      wid = i;
      found = 1;
      break;
    }
  }
  if (found == 0) {
    return;
  }

  xcb_window_t target = core.window;
  if (wid < core.ws_list[core.cur_ws].win_len - 1) {
    for (uint32_t i = wid + 1; i < core.ws_list[core.cur_ws].win_len; i++) {
      core.ws_list[core.cur_ws].win_list[i - 1] =
          core.ws_list[core.cur_ws].win_list[i];
    }
  }
  core.ws_list[core.cur_ws].win_len--;

  if (core.ws_list[core.cur_ws].win_len == 0) {
  } else if ((wid + 1) >= core.ws_list[core.cur_ws].win_len) {
    tfwm_window_focus(core.ws_list[core.cur_ws]
                          .win_list[core.ws_list[core.cur_ws].win_len - 1]
                          .win);
  } else if ((wid + 1) < core.ws_list[core.cur_ws].win_len) {
    tfwm_window_focus(core.ws_list[core.cur_ws].win_list[wid].win);
  }
  tfwm_layout_update();
  tfwm_util_redraw_bar();
  xcb_flush(core.conn);

  xcb_kill_client(core.conn, target);
}

void tfwm_window_next(char **cmd) {
  int wid;
  uint8_t found;
  for (uint32_t i = 0; i < core.ws_list[core.cur_ws].win_len; i++) {
    if (core.ws_list[core.cur_ws].win_list[i].win == core.window) {
      wid = i;
      found = 1;
      break;
    }
  }
  if (found == 0) {
    return;
  }

  if ((wid + 1) == core.ws_list[core.cur_ws].win_len) {
    tfwm_window_focus(core.ws_list[core.cur_ws].win_list[0].win);
  } else {
    tfwm_window_focus(core.ws_list[core.cur_ws].win_list[wid + 1].win);
  }
}

void tfwm_window_prev(char **cmd) {
  int wid;
  uint8_t found;
  for (uint32_t i = 0; i < core.ws_list[core.cur_ws].win_len; i++) {
    if (core.ws_list[core.cur_ws].win_list[i].win == core.window) {
      wid = i;
      found = 1;
      break;
    }
  }
  if (found == 0) {
    return;
  }

  if ((wid - 1) < 0) {
    tfwm_window_focus(core.ws_list[core.cur_ws]
                          .win_list[core.ws_list[core.cur_ws].win_len - 1]
                          .win);
  } else {
    tfwm_window_focus(core.ws_list[core.cur_ws].win_list[wid - 1].win);
  }
}

void tfwm_window_swap_last(char **cmd) {
  if (core.ws_list[core.cur_ws].win_len < 2) {
    return;
  }

  int wid;
  uint8_t found;
  for (uint32_t i = 0; i < core.ws_list[core.cur_ws].win_len; i++) {
    if (core.ws_list[core.cur_ws].win_list[i].win == core.window) {
      wid = i;
      found = 1;
      break;
    }
  }
  if (found == 0) {
    return;
  }

  uint32_t last_id = core.ws_list[core.cur_ws].win_len - 1;
  if (wid == last_id) {
    return;
  }

  xcb_window_t last = core.ws_list[core.cur_ws].win_list[last_id].win;
  if (core.window == last) {
    return;
  }

  core.ws_list[core.cur_ws].win_list[last_id].win = core.window;
  core.ws_list[core.cur_ws].win_list[wid].win = last;
  if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_TILING) {
    tfwm_layout_apply_tiling();
  }
  tfwm_window_focus(core.window);
}

void tfwm_window_toggle_fullscreen(char **cmd) {
  if (core.window == 0) {
    return;
  }
  if (core.window == core.screen->root) {
    return;
  }

  int x = 0 - TFWM_BORDER_WIDTH;
  int y = 0 - TFWM_BORDER_WIDTH;
  int width = core.screen->width_in_pixels;
  int height = core.screen->height_in_pixels;

  if (core.ws_list[core.cur_ws].win_list[core.cur_win].is_fullscreen == 1) {
    x = core.ws_list[core.cur_ws].win_list[core.cur_win].x;
    y = core.ws_list[core.cur_ws].win_list[core.cur_win].y;
    width = core.ws_list[core.cur_ws].win_list[core.cur_win].width;
    height = core.ws_list[core.cur_ws].win_list[core.cur_win].height;
  }

  tfwm_window_move(core.window, x, y);
  tfwm_window_resize(core.window, width, height);
  core.ws_list[core.cur_ws].win_list[core.cur_win].is_fullscreen ^= 1;
}

void tfwm_workspace_switch(char **cmd) {
  for (uint32_t i = 0; i < core.ws_len; i++) {
    char *wsname = (char *)cmd[0];
    if ((strcmp(wsname, core.ws_list[i].name) == 0) && (i != core.cur_ws)) {
      core.prv_ws = core.cur_ws;
      core.cur_ws = i;
    }
  }
  tfwm_workspace_remap();
}

void tfwm_workspace_next(char **cmd) {
  if ((core.cur_ws + 1) == core.ws_len) {
    core.prv_ws = core.cur_ws;
    core.cur_ws = 0;
  } else {
    core.prv_ws = core.cur_ws;
    core.cur_ws++;
  }
  tfwm_workspace_remap();
}

void tfwm_workspace_prev(char **cmd) {
  if ((core.cur_ws - 1) < 0) {
    core.prv_ws = core.cur_ws;
    core.cur_ws = core.ws_len - 1;
  } else {
    core.prv_ws = core.cur_ws;
    core.cur_ws--;
  }
  tfwm_workspace_remap();
}

void tfwm_workspace_swap_prev(char **cmd) {
  if (core.prv_ws == core.cur_ws) {
    return;
  }

  core.prv_ws = core.prv_ws ^ core.cur_ws;
  core.cur_ws = core.prv_ws ^ core.cur_ws;
  core.prv_ws = core.prv_ws ^ core.cur_ws;
  tfwm_workspace_remap();
}

void tfwm_workspace_use_tiling(char **cmd) {
  if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_TILING) {
    return;
  }
  core.ws_list[core.cur_ws].layout = TFWM_LAYOUT_TILING;
  tfwm_layout_apply_tiling();
}

void tfwm_workspace_use_floating(char **cmd) {
  if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_FLOATING) {
    return;
  }
  core.ws_list[core.cur_ws].layout = TFWM_LAYOUT_FLOATING;
}

void tfwm_workspace_use_window(char **cmd) {
  if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_WINDOW) {
    return;
  }
  core.ws_list[core.cur_ws].layout = TFWM_LAYOUT_WINDOW;
  tfwm_layout_apply_window();
}

/* ===================== WINDOW FUNCTION ===================== */

static void tfwm_window_raise(xcb_window_t window) {
  uint32_t vals[1];
  vals[0] = XCB_STACK_MODE_ABOVE;
  xcb_configure_window(core.conn, window, XCB_CONFIG_WINDOW_STACK_MODE, vals);
}

static void tfwm_window_focus(xcb_window_t window) {
  if (window == 0) {
    return;
  }
  if (window == core.screen->root) {
    return;
  }

  xcb_set_input_focus(core.conn, XCB_INPUT_FOCUS_POINTER_ROOT, window,
                      XCB_CURRENT_TIME);
  for (uint32_t i = 0; i < core.ws_list[core.cur_ws].win_len; i++) {
    if (core.ws_list[core.cur_ws].win_list[i].win == window) {
      core.window = window;
      core.cur_win = i;
      break;
    }
  }
  tfwm_window_raise(core.window);
  tfwm_util_redraw_bar();
}

static void tfwm_window_color(xcb_window_t window, uint32_t color) {
  if (TFWM_BORDER_WIDTH <= 0) {
    return;
  }
  if (window == 0) {
    return;
  }
  if (window == core.screen->root) {
    return;
  }

  uint32_t vals[1] = {color};
  xcb_change_window_attributes(core.conn, window, XCB_CW_BORDER_PIXEL, vals);
}

static void tfwm_window_move(xcb_window_t window, int x, int y) {
  uint32_t vals[2] = {x, y};
  xcb_configure_window(core.conn, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                       vals);
}

static void tfwm_window_resize(xcb_window_t window, int width, int height) {
  if ((width < TFWM_MIN_WINDOW_WIDTH) || (height < TFWM_MIN_WINDOW_HEIGHT)) {
    return;
  }

  uint32_t vals[2] = {width, height};
  xcb_configure_window(core.conn, window,
                       XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals);
}

static void tfwm_window_set_attr(xcb_window_t window, int x, int y, int width,
                                 int height) {
  if (core.cur_win == 0) {
    return;
  }
  if (core.cur_win == core.screen->root) {
    return;
  }

  core.ws_list[core.cur_ws].win_list[core.cur_win].x = x;
  core.ws_list[core.cur_ws].win_list[core.cur_win].y = y;
  core.ws_list[core.cur_ws].win_list[core.cur_win].width = width;
  core.ws_list[core.cur_ws].win_list[core.cur_win].height = height;
}

/* =================== WORKSPACE FUNCTION ==================== */

static void tfwm_workspace_remap(void) {
  if (core.cur_ws == core.prv_ws) {
    return;
  }

  for (uint32_t i = 0; i < core.ws_list[core.prv_ws].win_len; i++) {
    xcb_unmap_window(core.conn, core.ws_list[core.prv_ws].win_list[i].win);
  }

  for (uint32_t i = 0; i < core.ws_list[core.cur_ws].win_len; i++) {
    xcb_map_window(core.conn, core.ws_list[core.cur_ws].win_list[i].win);
    if (i == (core.ws_list[core.cur_ws].win_len - 1)) {
      tfwm_window_focus(core.ws_list[core.cur_ws].win_list[i].win);
    }
  }
  tfwm_util_redraw_bar();
}

static void tfwm_workspace_window_malloc(uint32_t wsid) {
  if (core.ws_list[wsid].win_list) {
    return;
  }

  core.ws_list[wsid].win_list =
      (tfwm_window_t *)malloc(TFWM_DEFAULT_WS_WIN_ALLOC * sizeof(tfwm_window_t));
  core.ws_list[wsid].win_cap = TFWM_DEFAULT_WS_WIN_ALLOC;
}

static void tfwm_workspace_window_realloc(uint32_t wsid) {
  if (!core.ws_list[wsid].win_list) {
    return;
  }

  tfwm_window_t *new_win = (tfwm_window_t *)realloc(
      core.ws_list[wsid].win_list,
      (core.ws_list[wsid].win_cap + TFWM_DEFAULT_WS_WIN_ALLOC) *
          sizeof(tfwm_window_t));
  if (!new_win) {
    return;
  }

  core.ws_list[wsid].win_list = new_win;
  core.ws_list[wsid].win_cap += TFWM_DEFAULT_WS_WIN_ALLOC;
}

static void tfwm_workspace_window_append(tfwm_window_t window) {
  if (!core.ws_list[core.cur_ws].win_list) {
    tfwm_workspace_window_malloc(core.cur_ws);
  } else if (core.ws_list[core.cur_ws].win_cap ==
             core.ws_list[core.cur_ws].win_len) {
    tfwm_workspace_window_realloc(core.cur_ws);
  }

  core.ws_list[core.cur_ws].win_list[core.ws_list[core.cur_ws].win_len++] = window;
}

/* ===================== LAYOUT FUNCTION ===================== */

static void tfwm_layout_apply_tiling(void) {
  if (core.ws_list[core.cur_ws].win_len == 0) {
    return;
  }
  if (core.ws_list[core.cur_ws].win_len == 1) {
    tfwm_layout_apply_window();
    return;
  }

  int master_x = 0;
  int master_y = TFWM_BAR_HEIGHT;
  int master_width =
      ((TFWM_TILE_MASTER_RATIO / 100.0) * core.screen->width_in_pixels) -
      (TFWM_BORDER_WIDTH * 2);
  int master_height =
      core.screen->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);
  int slave_x = master_width + (TFWM_BORDER_WIDTH * 2);
  int slave_y = master_y;
  int slave_width =
      (((100.0 - TFWM_TILE_MASTER_RATIO) / 100.0) * core.screen->width_in_pixels -
       (TFWM_BORDER_WIDTH * 2));
  int slave_height = master_height;

  if (core.ws_list[core.cur_ws].win_len > 2) {
    slave_height = ((core.screen->height_in_pixels - TFWM_BAR_HEIGHT) /
                    (core.ws_list[core.cur_ws].win_len - 1)) -
                   (TFWM_BORDER_WIDTH * 2);
  }
  int last_id = core.ws_list[core.cur_ws].win_len - 1;

  for (int i = last_id; i >= 0; i--) {
    if (i == last_id) {
      core.ws_list[core.cur_ws].win_list[i].x = master_x;
      core.ws_list[core.cur_ws].win_list[i].y = master_y;
      core.ws_list[core.cur_ws].win_list[i].width = master_width;
      core.ws_list[core.cur_ws].win_list[i].height = master_height;
      tfwm_window_move(core.ws_list[core.cur_ws].win_list[i].win, master_x,
                       master_y);
      tfwm_window_resize(core.ws_list[core.cur_ws].win_list[i].win, master_width,
                         master_height);
      tfwm_window_color(core.ws_list[core.cur_ws].win_list[i].win,
                        TFWM_BORDER_ACTIVE);
    } else {
      core.ws_list[core.cur_ws].win_list[i].x = slave_x;
      core.ws_list[core.cur_ws].win_list[i].y = slave_y;
      core.ws_list[core.cur_ws].win_list[i].width = slave_width;
      core.ws_list[core.cur_ws].win_list[i].height = slave_height;
      tfwm_window_move(core.ws_list[core.cur_ws].win_list[i].win, slave_x, slave_y);
      tfwm_window_resize(core.ws_list[core.cur_ws].win_list[i].win, slave_width,
                         slave_height);
      tfwm_window_color(core.ws_list[core.cur_ws].win_list[i].win,
                        TFWM_BORDER_INACTIVE);
      slave_y += (slave_height + (TFWM_BORDER_WIDTH * 2));
    }
  }
}

static void tfwm_layout_apply_window(void) {
  if (core.ws_list[core.cur_ws].win_len == 0) {
    return;
  }

  int x = 0;
  int y = TFWM_BAR_HEIGHT;
  int width = core.screen->width_in_pixels - (TFWM_BORDER_WIDTH * 2);
  int height =
      core.screen->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);
  int last_id = core.ws_list[core.cur_ws].win_len - 1;

  for (int i = last_id; i >= 0; i--) {
    core.ws_list[core.cur_ws].win_list[i].x = x;
    core.ws_list[core.cur_ws].win_list[i].y = y;
    core.ws_list[core.cur_ws].win_list[i].width = width;
    core.ws_list[core.cur_ws].win_list[i].height = height;
    tfwm_window_move(core.ws_list[core.cur_ws].win_list[i].win, x, y);
    tfwm_window_resize(core.ws_list[core.cur_ws].win_list[i].win, width, height);
    if (core.ws_list[core.cur_ws].win_list[i].win == core.window) {
      tfwm_window_raise(core.ws_list[core.cur_ws].win_list[i].win);
    }
  }
}

static void tfwm_layout_update(void) {
  if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_FLOATING) {
    return;
  } else if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_WINDOW) {
    tfwm_layout_apply_window();
  } else if (core.ws_list[core.cur_ws].layout == TFWM_LAYOUT_TILING) {
    if (core.ws_list[core.cur_ws].win_len == 1) {
      tfwm_layout_apply_window();
      return;
    }

    tfwm_layout_apply_tiling();
  }
}

/* ====================== EVENT HANDLER ====================== */

void tfwm_handle_keypress(xcb_generic_event_t *event) {
  xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
  xcb_keysym_t keysym = tfwm_util_get_keysym(e->detail);

  for (int i = 0; i < sizeof(cfg_keybinds) / sizeof(*cfg_keybinds); i++) {
    if ((cfg_keybinds[i].keysym == keysym) && (cfg_keybinds[i].mod == e->state)) {
      cfg_keybinds[i].func((char **)cfg_keybinds[i].cmd);
      xcb_flush(core.conn);
    }
  }
}

void tfwm_handle_map_request(xcb_generic_event_t *event) {
  xcb_map_request_event_t *e = (xcb_map_request_event_t *)event;

  uint32_t vals[5];
  vals[0] = (core.screen->width_in_pixels / 2) - (TFWM_WINDOW_WIDTH / 2);
  vals[1] = (core.screen->height_in_pixels / 2) - (TFWM_WINDOW_HEIGHT / 2);
  vals[2] = TFWM_WINDOW_WIDTH;
  vals[3] = TFWM_WINDOW_HEIGHT;
  vals[4] = TFWM_BORDER_WIDTH;
  xcb_configure_window(core.conn, e->window,
                       XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                           XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                           XCB_CONFIG_WINDOW_BORDER_WIDTH,
                       vals);
  uint32_t attrvals[1] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
  xcb_change_window_attributes_checked(core.conn, e->window, XCB_CW_EVENT_MASK,
                                       attrvals);
  xcb_map_window(core.conn, e->window);
  xcb_flush(core.conn);

  tfwm_window_t w;
  w.x = vals[0];
  w.y = vals[1];
  w.width = vals[2];
  w.height = vals[3];
  w.border = vals[4];
  w.win = e->window;
  char *wmc = tfwm_util_window_class(e->window);
  w.class = malloc(strlen(wmc) * sizeof(char));
  strcpy(w.class, wmc);
  tfwm_workspace_window_append(w);
  tfwm_window_focus(e->window);
  tfwm_layout_update();
  tfwm_util_redraw_bar();
}

void tfwm_handle_focus_in(xcb_generic_event_t *event) {
  xcb_focus_in_event_t *e = (xcb_focus_in_event_t *)event;
  tfwm_window_color(e->event, TFWM_BORDER_ACTIVE);
}

void tfwm_handle_focus_out(xcb_generic_event_t *event) {
  xcb_focus_out_event_t *e = (xcb_focus_out_event_t *)event;
  tfwm_window_color(e->event, TFWM_BORDER_INACTIVE);
}

void tfwm_handle_enter_notify(xcb_generic_event_t *event) {
  xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)event;
}

void tfwm_handle_leave_notify(xcb_generic_event_t *event) {
  xcb_leave_notify_event_t *e = (xcb_leave_notify_event_t *)event;
}

void tfwm_handle_motion_notify(xcb_generic_event_t *event) {
  if (core.window == 0) {
    return;
  }
  if (core.ws_list[core.cur_ws].layout != TFWM_LAYOUT_FLOATING) {
    return;
  }

  xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)event;

  xcb_query_pointer_reply_t *point = xcb_query_pointer_reply(
      core.conn, xcb_query_pointer(core.conn, core.screen->root), NULL);
  if (!point) {
    return;
  }

  xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply(
      core.conn, xcb_get_geometry(core.conn, core.window), NULL);
  if (!geo) {
    goto clean_point;
  }

  if (core.cur_btn == (uint32_t)(BUTTON_LEFT)) {
    if ((core.ptr_x_post == point->root_x) && (core.ptr_y_pos == point->root_y)) {
      return;
    }

    int x = geo->x + point->root_x - core.ptr_x_post;
    int y = geo->y + point->root_y - core.ptr_y_pos;
    core.ptr_x_post = point->root_x;
    core.ptr_y_pos = point->root_y;
    tfwm_window_move(core.window, x, y);
  } else if (core.cur_btn == (uint32_t)(BUTTON_RIGHT)) {
    if ((point->root_x <= geo->x) || (point->root_y <= geo->y)) {
      return;
    }

    int width = geo->width + (point->root_x - core.ptr_x_post);
    int height = geo->height + (point->root_y - core.ptr_y_pos);
    core.ptr_x_post = point->root_x;
    core.ptr_y_pos = point->root_y;
    tfwm_window_resize(core.window, width, height);
  }

  free(geo);
clean_point:
  free(point);
}

void tfwm_handle_destroy_notify(xcb_generic_event_t *event) {
  xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)event;
  xcb_kill_client(core.conn, e->window);
}

void tfwm_handle_button_press(xcb_generic_event_t *event) {
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
  core.window = e->child;
  core.ptr_x_post = e->event_x;
  core.ptr_y_pos = e->event_y;
  tfwm_window_focus(core.window);

  core.cur_btn =
      ((e->detail == BUTTON_LEFT) ? BUTTON_LEFT
                                  : ((core.window != 0) ? BUTTON_RIGHT : 0));
  xcb_grab_pointer(core.conn, 0, core.screen->root,
                   XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
                       XCB_EVENT_MASK_POINTER_MOTION_HINT,
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, core.screen->root,
                   XCB_NONE, XCB_CURRENT_TIME);

  if (core.cur_btn == (uint32_t)BUTTON_LEFT) {
    tfwm_util_set_cursor(core.window, (char *)TFWM_CURSOR_MOVE);
  } else if (core.cur_btn == (uint32_t)BUTTON_RIGHT) {
    tfwm_util_set_cursor(core.window, (char *)TFWM_CURSOR_RESIZE);
  }
}

void tfwm_handle_button_release(xcb_generic_event_t *event) {
  xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply(
      core.conn, xcb_get_geometry(core.conn, core.window), NULL);
  if (!geo) {
    return;
  }

  tfwm_window_set_attr(core.window, geo->x, geo->y, geo->width, geo->height);
  xcb_ungrab_pointer(core.conn, XCB_CURRENT_TIME);
  tfwm_util_set_cursor(core.window, (char *)TFWM_CURSOR_DEFAULT);

  free(geo);
}

static int tfwm_handle_event(void) {
  int ret = xcb_connection_has_error(core.conn);
  if (ret != 0) {
    return ret;
  }

  xcb_generic_event_t *event = xcb_wait_for_event(core.conn);
  if (!event) {
    return ret;
  }

  tfwm_event_handler_t *handler;
  for (handler = event_handlers; handler->func; handler++) {
    uint8_t e = event->response_type & ~0x80;
    if (e == handler->request) {
      handler->func(event);
    }
  }

  free(event);
  xcb_flush(core.conn);
  return ret;
}

/* =========================== BAR =========================== */

static void tfwm_bar_render_left(xcb_gcontext_t gc, char *text) {
  xcb_image_text_8(core.conn, strlen(text), core.bar, gc, core.bar_x_left,
                   TFWM_FONT_HEIGHT, text);
  core.bar_x_left += tfwm_util_text_width(text);
}

static void tfwm_bar_render_right(xcb_gcontext_t gc, char *text) {
  core.bar_x_right -= tfwm_util_text_width(text);
  xcb_image_text_8(core.conn, strlen(text), core.bar, gc, core.bar_x_right,
                   TFWM_FONT_HEIGHT, text);
}

static void tfwm_bar_module_layout(void (*render)(xcb_gcontext_t, char *)) {
  size_t n = sizeof(cfg_layout) / sizeof(*cfg_layout);

  for (size_t i = 0; i < n; i++) {
    if (core.ws_list[core.cur_ws].layout == cfg_layout[i].layout) {
      render(core.gc_inactive, cfg_layout[i].symbol);
      break;
    }
  }
}

static void tfwm_bar_module_separator(void (*render)(xcb_gcontext_t, char *)) {
  render(core.gc_inactive, (char *)TFWM_BAR_SEPARATOR);
}

static void tfwm_bar_module_workspace(void (*render)(xcb_gcontext_t, char *)) {
  for (uint32_t i = 0; i < core.ws_len; i++) {
    size_t len = strlen(core.ws_list[i].name);
    char ws[len + 3];
    ws[0] = ' ';
    memcpy(ws + 1, core.ws_list[i].name, len);
    ws[len + 1] = ' ';
    ws[len + 2] = '\0';
    if (i == core.cur_ws) {
      render(core.gc_active, ws);
    } else {
      render(core.gc_inactive, ws);
    }
  }
}

static void tfwm_bar_module_window_tabs() {
  if (core.window == 0) {
    return;
  }
  if (core.cur_win == core.screen->root) {
    return;
  }
  if (core.ws_list[core.cur_ws].win_len == 0) {
    return;
  }

  char *p_sign = "< ";
  char *n_sign = " >";
  int p_sign_width = tfwm_util_text_width(p_sign);
  int n_sign_width = tfwm_util_text_width(n_sign);
  int sep_width = tfwm_util_text_width(" ");
  int max_width = core.bar_x_right - (core.bar_x_left + p_sign_width + n_sign_width);
  int n =
      tfwm_util_text_width(core.ws_list[core.cur_ws].win_list[core.cur_win].class) +
      (2 * sep_width);
  int last = core.ws_list[core.cur_ws].win_len - 1;
  int head = core.cur_win > 0 ? core.cur_win - 1 : 0;
  int tail = core.cur_win < last ? core.cur_win + 1 : last;
  int head_done = core.cur_win == head ? 1 : 0;
  int tail_done = core.cur_win == tail ? 1 : 0;

  while (n < max_width) {
    if (head >= 0 && !head_done) {
      n += (tfwm_util_text_width(core.ws_list[core.cur_ws].win_list[head].class) +
            (2 * sep_width));
      if (n >= max_width) {
        tail -= !tail_done ? 1 : 0;
        break;
      } else if (head == 0) {
        head_done = 1;
      } else {
        head--;
      }
    }

    if (tail <= last && !tail_done) {
      n += (tfwm_util_text_width(core.ws_list[core.cur_ws].win_list[tail].class) +
            (2 * sep_width));
      if (n >= max_width) {
        head += !head_done ? 1 : 0;
        break;
      } else if (tail == last) {
        tail_done = 1;
      } else {
        tail++;
      }
    }

    if (head_done && tail_done) {
      break;
    }
  }

  if (head > 0) {
    tfwm_bar_render_left(core.gc_inactive, p_sign);
  } else {
    core.bar_x_left += p_sign_width;
  }
  if (tail < last) {
    tfwm_bar_render_right(core.gc_inactive, n_sign);
  } else {
    core.bar_x_right -= n_sign_width;
  }

  for (int i = head; i <= tail; i++) {
    size_t len = strlen(core.ws_list[core.cur_ws].win_list[i].class);
    char c[len + 3];
    c[0] = ' ';
    memcpy(c + 1, core.ws_list[core.cur_ws].win_list[i].class, len);
    c[len + 1] = ' ';
    c[len + 2] = '\0';
    if (i == core.cur_win) {
      tfwm_bar_render_left(core.gc_active, c);
    } else {
      tfwm_bar_render_left(core.gc_inactive, c);
    }
  }
}

static void tfwm_bar_run() {
  core.bar_x_left = 0;
  core.bar_x_right = core.screen->width_in_pixels;

  tfwm_bar_module_workspace(tfwm_bar_render_left);
  tfwm_bar_module_separator(tfwm_bar_render_left);
  tfwm_bar_module_layout(tfwm_bar_render_left);
  tfwm_bar_module_separator(tfwm_bar_render_left);

  tfwm_bar_render_right(core.gc_inactive, "tfwm-0.0.1");
  tfwm_bar_module_separator(tfwm_bar_render_right);

  tfwm_bar_module_window_tabs();

  xcb_flush(core.conn);
}

/* ========================== SETUP ========================== */

static void tfwm_init(void) {
  uint32_t vals[1] = {
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
      XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE};
  xcb_change_window_attributes_checked(core.conn, core.screen->root,
                                       XCB_CW_EVENT_MASK, vals);

  xcb_ungrab_key(core.conn, XCB_GRAB_ANY, core.screen->root, XCB_MOD_MASK_ANY);
  for (int i = 0; i < sizeof(cfg_keybinds) / sizeof(*cfg_keybinds); i++) {
    xcb_keycode_t *keycode = tfwm_util_get_keycodes(cfg_keybinds[i].keysym);
    if (keycode != NULL) {
      xcb_grab_key(core.conn, 1, core.screen->root, cfg_keybinds[i].mod, *keycode,
                   XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
    }
  }
  xcb_flush(core.conn);

  xcb_grab_button(core.conn, 0, core.screen->root,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, core.screen->root,
                  XCB_NONE, BUTTON_LEFT, MOD_KEY);
  xcb_grab_button(core.conn, 0, core.screen->root,
                  XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                  XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, core.screen->root,
                  XCB_NONE, BUTTON_RIGHT, MOD_KEY);
  tfwm_util_set_cursor(core.screen->root, (char *)TFWM_CURSOR_DEFAULT);
  xcb_flush(core.conn);

  core.ws_len = sizeof(cfg_workspace_list) / sizeof(*cfg_workspace_list);
  core.ws_list = malloc(core.ws_len * sizeof(tfwm_workspace_t));
  for (uint32_t i = 0; i < core.ws_len; i++) {
    tfwm_workspace_t ws;
    ws.layout = cfg_layout[0].layout;
    ws.name = cfg_workspace_list[i];
    core.ws_list[i] = ws;
  }

  core.bar = xcb_generate_id(core.conn);
  uint32_t bar_vals[3];
  bar_vals[0] = TFWM_BAR_BG;
  bar_vals[1] = 1;
  bar_vals[2] = XCB_EVENT_MASK_EXPOSURE;
  xcb_create_window(core.conn, XCB_COPY_FROM_PARENT, core.bar, core.screen->root, 0,
                    0, core.screen->width_in_pixels, TFWM_BAR_HEIGHT, 0,
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, core.screen->root_visual,
                    XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
                    bar_vals);
  uint32_t bar_cfg_vals[1];
  bar_vals[0] = XCB_STACK_MODE_ABOVE;
  xcb_configure_window(core.conn, core.bar, XCB_CONFIG_WINDOW_STACK_MODE,
                       bar_cfg_vals);
  xcb_map_window(core.conn, core.bar);
  xcb_flush(core.conn);

  core.font = xcb_generate_id(core.conn);
  xcb_open_font(core.conn, core.font, strlen(TFWM_FONT), TFWM_FONT);

  core.gc_active = xcb_generate_id(core.conn);
  uint32_t active_vals[3];
  active_vals[0] = TFWM_BAR_FG_ACTIVE;
  active_vals[1] = TFWM_BAR_BG_ACTIVE;
  active_vals[2] = core.font;
  xcb_create_gc(core.conn, core.gc_active, core.bar,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, active_vals);

  core.gc_inactive = xcb_generate_id(core.conn);
  uint32_t inactive_vals[3];
  inactive_vals[0] = TFWM_BAR_FG;
  inactive_vals[1] = TFWM_BAR_BG;
  inactive_vals[2] = core.font;
  xcb_create_gc(core.conn, core.gc_inactive, core.bar,
                XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, inactive_vals);
}

int main(int argc, char *argv[]) {
  core = (tfwm_xcb_t){};

  if ((argc == 2) && (strcmp("-v", argv[1]) == 0)) {
    printf("tfwm-0.0.1, Copyright (c) 2024 Raihan Rahardyan, MIT License\n");
    return EXIT_SUCCESS;
  }

  if (argc != 1) {
    printf("usage: tfwm [-v]\n");
    return EXIT_SUCCESS;
  }

  core.conn = xcb_connect(NULL, NULL);
  core.exit = xcb_connection_has_error(core.conn);
  if (core.exit > 0) {
    printf("xcb_connection_has_error\n");
    return core.exit;
  }

  core.screen = xcb_setup_roots_iterator(xcb_get_setup(core.conn)).data;
  tfwm_init();

  while (core.exit == EXIT_SUCCESS) {
    core.exit = tfwm_handle_event();

    if (core.screen) {
      tfwm_bar_run();
    }
  }

  return core.exit;
}
