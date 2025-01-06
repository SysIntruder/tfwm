#include "tfwm.h"

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

static tfwm_xcb_t core;

static void tfwm_util_log(char *log, int exit) {
  char *home = getenv("HOME");
  if (!home) {
    return;
  }
  char *path = malloc(strlen(home) + strlen(TFWM_LOG_FILE) + 2);
  sprintf(path, "%s/%s", home, TFWM_LOG_FILE);

  FILE *file;
  if (access(path, F_OK) != 0) {
    file = fopen(path, "w");
  } else {
    file = fopen(path, "a");
  }
  if (!file) {
    printf("ERROR: can not open log file\n");
    return;
  }
  free(path);

  time_t t = time(NULL);
  struct tm *now = localtime(&t);
  char timestamp[20];
  strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", now);

  fprintf(file, "[%s] %s\n", timestamp, log);
  fclose(file);
  core.exit = exit;
}

static xcb_keycode_t *tfwm_util_keycodes(xcb_keysym_t keysym) {
  xcb_key_symbols_t *k = xcb_key_symbols_alloc(core.c);
  xcb_keycode_t *kc = (!(k) ? NULL : xcb_key_symbols_get_keycode(k, keysym));
  xcb_key_symbols_free(k);
  return kc;
}

static xcb_keysym_t tfwm_util_keysym(xcb_keycode_t keycode) {
  xcb_key_symbols_t *k = xcb_key_symbols_alloc(core.c);
  xcb_keysym_t ks = (!(k) ? 0 : xcb_key_symbols_get_keysym(k, keycode, 0));
  xcb_key_symbols_free(k);
  return ks;
}

static xcb_cursor_t tfwm_util_cursor(char *name) {
  xcb_cursor_context_t *ctx;
  if (xcb_cursor_context_new(core.c, core.sc, &ctx) < 0) {
    return XCB_CURSOR_NONE;
  }
  xcb_cursor_t c = xcb_cursor_load_cursor(ctx, name);
  xcb_cursor_context_free(ctx);
  return c;
}

static char *tfwm_util_window_class(xcb_window_t window) {
  xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(
      core.c, xcb_intern_atom(core.c, 0, strlen("WM_CLASS"), "WM_CLASS"), NULL
  );
  if (!r) {
    return NULL;
  }
  xcb_atom_t a = r->atom;
  free(r);

  xcb_get_property_reply_t *p = xcb_get_property_reply(
      core.c, xcb_get_property(core.c, 0, window, a, XCB_ATOM_STRING, 0, 250), NULL
  );
  if (!p) {
    return NULL;
  }
  if (xcb_get_property_value_length(p) == 0) {
    free(p);
    return NULL;
  }

  char *class = (char *)xcb_get_property_value(p);
  free(p);
  class = class + strlen(class) + 1;

  return class;
}

static int tfwm_util_text_width(char *text) {
  size_t n = strlen(text);
  xcb_char2b_t b[n * sizeof(xcb_char2b_t)];
  for (int i = 0; i < n; i++) {
    b[i].byte1 = 0;
    b[i].byte2 = text[i];
  }

  xcb_query_text_extents_reply_t *r = xcb_query_text_extents_reply(
      core.c, xcb_query_text_extents(core.c, core.font, n, b), NULL
  );
  if (!r) {
    return 0;
  }

  int w = r->overall_width;
  free(r);

  return w;
}

static void tfwm_util_cleanup(void) {
  if (core.ws_list) {
    for (uint32_t i = 0; i < core.ws_len; i++) {
      if (core.ws_list[i].win_list) {
        for (uint32_t j = 0; j < core.ws_list[i].win_cap; j++) {
          if (core.ws_list[i].win_list[j].class) {
            free(core.ws_list[i].win_list[j].class);
          }
        }
        free(core.ws_list[i].win_list);
      }
    }
    free(core.ws_list);
  }

  if (core.font) {
    xcb_close_font(core.c, core.font);
  }
  if (core.gc_active) {
    xcb_free_gc(core.c, core.gc_active);
  }
  if (core.gc_inactive) {
    xcb_free_gc(core.c, core.gc_inactive);
  }
}

void tfwm_exit(char **cmd) {
  if (core.c) {
    tfwm_util_cleanup();
    xcb_disconnect(core.c);
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
  if (0 == core.win) {
    return;
  }
  if ((core.win) == core.sc->root) {
    return;
  }

  xcb_window_t target = core.win;
  tfwm_workspace_window_pop(core.cur_win);
  tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
  if (ws->win_len == 0) {
    tfwm_window_focus(core.sc->root);
  } else if ((core.cur_win + 1) >= ws->win_len) {
    tfwm_window_focus(ws->win_list[ws->win_len - 1].win);
  } else if ((core.cur_win + 1) < ws->win_len) {
    tfwm_window_focus(ws->win_list[core.cur_win].win);
  }
  if (TFWM_LAYOUT_TILING == ws->layout) {
    tfwm_layout_apply_tiling(core.cur_ws);
  }
  xcb_flush(core.c);
  xcb_kill_client(core.c, target);
}

void tfwm_window_next(char **cmd) {
  tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
  uint32_t wid;
  uint8_t ok;
  for (uint32_t i = 0; i < ws->win_len; i++) {
    if (ws->win_list[i].win == core.win) {
      wid = i;
      ok = 1;
      break;
    }
  }
  if (0 == ok) {
    return;
  }

  if ((wid + 1) == ws->win_len) {
    tfwm_window_focus(ws->win_list[0].win);
  } else {
    tfwm_window_focus(ws->win_list[wid + 1].win);
  }
}

void tfwm_window_prev(char **cmd) {
  tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
  uint32_t wid;
  uint8_t ok;
  for (uint32_t i = 0; i < ws->win_len; i++) {
    if (ws->win_list[i].win == core.win) {
      wid = i;
      ok = 1;
      break;
    }
  }
  if (0 == ok) {
    return;
  }

  if (0 == wid) {
    tfwm_window_focus(ws->win_list[ws->win_len - 1].win);
  } else {
    tfwm_window_focus(ws->win_list[wid - 1].win);
  }
}

void tfwm_window_swap_last(char **cmd) {
  if (core.ws_list[core.cur_ws].win_len < 2) {
    return;
  }

  tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
  uint32_t wid;
  uint8_t ok;
  for (uint32_t i = 0; i < ws->win_len; i++) {
    if (ws->win_list[i].win == core.win) {
      wid = i;
      ok = 1;
      break;
    }
  }
  if (0 == ok) {
    return;
  }
  uint32_t last_id = ws->win_len - 1;
  if ((wid) == last_id) {
    return;
  }
  xcb_window_t last = ws->win_list[last_id].win;
  if (core.win == last) {
    return;
  }

  ws->win_list[last_id].win = core.win;
  ws->win_list[wid].win = last;
  if (TFWM_LAYOUT_TILING == ws->layout) {
    tfwm_layout_apply_tiling(core.cur_ws);
  }
  tfwm_window_focus(core.win);
}

void tfwm_window_fullscreen(char **cmd) {
  if (0 == core.win) {
    return;
  }
  if ((core.win) == core.sc->root) {
    return;
  }

  int x = 0 - TFWM_BORDER_WIDTH;
  int y = 0 - TFWM_BORDER_WIDTH;
  int w = core.sc->width_in_pixels;
  int h = core.sc->height_in_pixels;
  tfwm_window_t *win = &core.ws_list[core.cur_ws].win_list[core.cur_win];

  if (1 == win->is_fullscreen) {
    x = win->x;
    y = win->y;
    w = win->w;
    h = win->h;
  }

  tfwm_window_move(core.win, x, y);
  tfwm_window_resize(core.win, w, h);
  win->is_fullscreen ^= 1;
}

void tfwm_window_to_workspace(char **cmd) {
  uint32_t wsid;
  uint8_t ok;
  for (uint32_t i = 0; i < core.ws_len; i++) {
    if (strcmp((char *)cmd[0], core.ws_list[i].name) == 0) {
      if (i == core.cur_ws) {
        return;
      }
      wsid = i;
      ok = 1;
      break;
    }
  }
  if (0 == ok) {
    return;
  }
  if (0 == core.cur_win) {
    return;
  }
  if ((core.win) == core.sc->root) {
    return;
  }

  tfwm_workspace_window_append(
      wsid, core.ws_list[core.cur_ws].win_list[core.cur_win]
  );
  tfwm_workspace_window_pop(core.cur_win);

  core.prv_ws = core.cur_ws;
  core.cur_ws = wsid;
  if (core.ws_list[core.prv_ws].layout != core.ws_list[core.cur_ws].layout) {
    tfwm_layout_update(core.cur_ws);
  }
  tfwm_workspace_window_unmap(core.prv_ws);
  tfwm_workspace_window_map(core.cur_ws);
}

void tfwm_workspace_switch(char **cmd) {
  uint32_t wsid;
  uint8_t ok;
  for (uint32_t i = 0; i < core.ws_len; i++) {
    if (strcmp((char *)cmd[0], core.ws_list[i].name) == 0) {
      if (i == core.cur_ws) {
        return;
      }
      wsid = i;
      ok = 1;
      break;
    }
  }
  if (0 == ok) {
    return;
  }

  core.prv_ws = core.cur_ws;
  core.cur_ws = wsid;
  if (core.ws_list[core.prv_ws].layout != core.ws_list[core.cur_ws].layout) {
    tfwm_layout_update(core.cur_ws);
  }
  tfwm_workspace_window_unmap(core.prv_ws);
  tfwm_workspace_window_map(core.cur_ws);
}

void tfwm_workspace_next(char **cmd) {
  if ((core.cur_ws + 1) == core.ws_len) {
    core.prv_ws = core.cur_ws;
    core.cur_ws = 0;
  } else {
    core.prv_ws = core.cur_ws;
    core.cur_ws++;
  }
  if (core.ws_list[core.prv_ws].layout != core.ws_list[core.cur_ws].layout) {
    tfwm_layout_update(core.cur_ws);
  }
  tfwm_workspace_window_unmap(core.prv_ws);
  tfwm_workspace_window_map(core.cur_ws);
}

void tfwm_workspace_prev(char **cmd) {
  if (0 == core.cur_ws) {
    core.prv_ws = core.cur_ws;
    core.cur_ws = core.ws_len - 1;
  } else {
    core.prv_ws = core.cur_ws;
    core.cur_ws--;
  }
  if (core.ws_list[core.prv_ws].layout != core.ws_list[core.cur_ws].layout) {
    tfwm_layout_update(core.cur_ws);
  }
  tfwm_workspace_window_unmap(core.prv_ws);
  tfwm_workspace_window_map(core.cur_ws);
}

void tfwm_workspace_swap_prev(char **cmd) {
  if ((core.prv_ws) == core.cur_ws) {
    return;
  }

  core.prv_ws = core.prv_ws ^ core.cur_ws;
  core.cur_ws = core.prv_ws ^ core.cur_ws;
  core.prv_ws = core.prv_ws ^ core.cur_ws;
  if (core.ws_list[core.prv_ws].layout != core.ws_list[core.cur_ws].layout) {
    tfwm_layout_update(core.cur_ws);
  }
  tfwm_workspace_window_unmap(core.prv_ws);
  tfwm_workspace_window_map(core.cur_ws);
}

void tfwm_workspace_use_tiling(char **cmd) {
  if (TFWM_LAYOUT_TILING == core.ws_list[core.cur_ws].layout) {
    return;
  }
  core.ws_list[core.cur_ws].layout = TFWM_LAYOUT_TILING;
  tfwm_layout_apply_tiling(core.cur_ws);
}

void tfwm_workspace_use_floating(char **cmd) {
  if (TFWM_LAYOUT_FLOATING == core.ws_list[core.cur_ws].layout) {
    return;
  }
  core.ws_list[core.cur_ws].layout = TFWM_LAYOUT_FLOATING;
}

void tfwm_workspace_use_window(char **cmd) {
  if (TFWM_LAYOUT_WINDOW == core.ws_list[core.cur_ws].layout) {
    return;
  }
  core.ws_list[core.cur_ws].layout = TFWM_LAYOUT_WINDOW;
  tfwm_layout_apply_window(core.cur_ws);
}

static void tfwm_window_focus(xcb_window_t window) {
  if (0 == window) {
    return;
  }

  xcb_set_input_focus(
      core.c, XCB_INPUT_FOCUS_POINTER_ROOT, window, XCB_CURRENT_TIME
  );
  if ((window) == core.sc->root) {
    core.win = window;
    core.cur_win = 0;
  } else {
    tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
    for (uint32_t i = 0; i < ws->win_len; i++) {
      if (ws->win_list[i].win == window) {
        core.win = window;
        core.cur_win = i;
        break;
      }
    }

    uint32_t vals[1] = {XCB_STACK_MODE_ABOVE};
    xcb_configure_window(core.c, core.win, XCB_CONFIG_WINDOW_STACK_MODE, vals);
  }
  xcb_clear_area(core.c, 1, core.bar, 0, 0, 0, 0);
}

static void tfwm_window_color(xcb_window_t window, uint32_t color) {
  if (TFWM_BORDER_WIDTH <= 0) {
    return;
  }
  if (0 == window) {
    return;
  }
  if ((window) == core.sc->root) {
    return;
  }

  uint32_t vals[1] = {color};
  xcb_change_window_attributes(core.c, window, XCB_CW_BORDER_PIXEL, vals);
}

static void tfwm_window_move(xcb_window_t window, int x, int y) {
  uint32_t vals[2] = {x, y};
  xcb_configure_window(
      core.c, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals
  );
}

static void tfwm_window_resize(xcb_window_t window, int w, int h) {
  if ((w < TFWM_MIN_WINDOW_WIDTH) || (h < TFWM_MIN_WINDOW_HEIGHT)) {
    return;
  }

  uint32_t vals[2] = {w, h};
  xcb_configure_window(
      core.c, window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, vals
  );
}

static void tfwm_window_set_attr(xcb_window_t window, int x, int y, int w, int h) {
  if (0 == core.cur_win) {
    return;
  }
  if ((core.win) == core.sc->root) {
    return;
  }

  tfwm_window_t *win = &core.ws_list[core.cur_ws].win_list[core.cur_win];
  win->x = x;
  win->y = y;
  win->w = w;
  win->h = h;
}

static void tfwm_workspace_window_unmap(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  for (uint32_t i = 0; i < ws->win_len; i++) {
    xcb_unmap_window(core.c, ws->win_list[i].win);
    tfwm_window_color(ws->win_list[i].win, TFWM_BORDER_INACTIVE);
  }
}

static void tfwm_workspace_window_map(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (0 == ws->win_len) {
    tfwm_window_focus(core.sc->root);
    return;
  }

  for (uint32_t i = 0; i < ws->win_len; i++) {
    xcb_map_window(core.c, ws->win_list[i].win);
    if (i == (ws->win_len - 1)) {
      tfwm_window_focus(ws->win_list[i].win);
    } else {
      tfwm_window_color(ws->win_list[i].win, TFWM_BORDER_INACTIVE);
    }
  }
}

static void tfwm_workspace_window_malloc(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (ws->win_list) {
    return;
  }

  ws->win_list =
      (tfwm_window_t *)malloc(TFWM_WIN_LIST_ALLOC * sizeof(tfwm_window_t));
  ws->win_cap = TFWM_WIN_LIST_ALLOC;
}

static void tfwm_workspace_window_realloc(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (!ws->win_list) {
    return;
  }

  tfwm_window_t *new_win = (tfwm_window_t *)realloc(
      ws->win_list, (ws->win_cap + TFWM_WIN_LIST_ALLOC) * sizeof(tfwm_window_t)
  );
  if (!new_win) {
    return;
  }

  ws->win_list = new_win;
  ws->win_cap += TFWM_WIN_LIST_ALLOC;
}

static void tfwm_workspace_window_append(uint32_t wsid, tfwm_window_t window) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (!ws->win_list) {
    tfwm_workspace_window_malloc(wsid);
  } else if (ws->win_cap == core.ws_list[wsid].win_len) {
    tfwm_workspace_window_realloc(wsid);
  }

  ws->win_list[ws->win_len++] = window;
}

static void tfwm_workspace_window_pop(uint32_t wid) {
  tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
  if (wid < ws->win_len - 1) {
    for (uint32_t i = wid + 1; i < ws->win_len; i++) {
      ws->win_list[i - 1] = ws->win_list[i];
    }
  }
  ws->win_len--;
}

static void tfwm_layout_apply_tiling(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (0 == ws->win_len) {
    return;
  }
  if (1 == ws->win_len) {
    tfwm_layout_apply_window(wsid);
    return;
  }

  int mx = 0;
  int my = TFWM_BAR_HEIGHT;
  int mw = ((TFWM_TILE_MASTER / 100.0) * core.sc->width_in_pixels) -
           (TFWM_BORDER_WIDTH * 2);
  int mh = core.sc->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);
  int sx = mw + (TFWM_BORDER_WIDTH * 2);
  int sy = my;
  int sw = ((100.0 - TFWM_TILE_MASTER) / 100.0) * core.sc->width_in_pixels -
           (TFWM_BORDER_WIDTH * 2);
  int sh = mh;

  if (ws->win_len > 2) {
    sh = ((core.sc->height_in_pixels - TFWM_BAR_HEIGHT) / (ws->win_len - 1)) -
         (TFWM_BORDER_WIDTH * 2);
  }
  int last_id = core.ws_list[wsid].win_len - 1;

  for (int i = last_id; i >= 0; i--) {
    if (i == last_id) {
      ws->win_list[i].x = mx;
      ws->win_list[i].y = my;
      ws->win_list[i].w = mw;
      ws->win_list[i].h = mh;
      tfwm_window_move(ws->win_list[i].win, mx, my);
      tfwm_window_resize(ws->win_list[i].win, mw, mh);
    } else {
      ws->win_list[i].x = sx;
      ws->win_list[i].y = sy;
      ws->win_list[i].w = sw;
      ws->win_list[i].h = sh;
      tfwm_window_move(ws->win_list[i].win, sx, sy);
      tfwm_window_resize(ws->win_list[i].win, sw, sh);
      sy += (sh + (TFWM_BORDER_WIDTH * 2));
    }
  }
}

static void tfwm_layout_apply_window(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (0 == ws->win_len) {
    return;
  }

  int x = 0;
  int y = TFWM_BAR_HEIGHT;
  int w = core.sc->width_in_pixels - (TFWM_BORDER_WIDTH * 2);
  int h = core.sc->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);
  int last_id = ws->win_len - 1;

  for (int i = last_id; i >= 0; i--) {
    ws->win_list[i].x = x;
    ws->win_list[i].y = y;
    ws->win_list[i].w = w;
    ws->win_list[i].h = h;
    tfwm_window_move(ws->win_list[i].win, x, y);
    tfwm_window_resize(ws->win_list[i].win, w, h);
  }
}

static void tfwm_layout_update(uint32_t wsid) {
  tfwm_workspace_t *ws = &core.ws_list[wsid];
  if (TFWM_LAYOUT_FLOATING == ws->layout) {
    return;
  } else if (TFWM_LAYOUT_WINDOW == ws->layout) {
    tfwm_layout_apply_window(wsid);
  } else if (TFWM_LAYOUT_TILING == ws->layout) {
    tfwm_layout_apply_tiling(wsid);
  }
}

void tfwm_handle_keypress(xcb_generic_event_t *event) {
  xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
  xcb_keysym_t keysym = tfwm_util_keysym(e->detail);

  for (int i = 0; i < ARRAY_LENGTH(cfg_keybinds); i++) {
    if ((cfg_keybinds[i].keysym == keysym) && (cfg_keybinds[i].mod == e->state)) {
      cfg_keybinds[i].func((char **)cfg_keybinds[i].cmd);
      xcb_flush(core.c);
    }
  }
}

void tfwm_handle_map_request(xcb_generic_event_t *event) {
  xcb_map_request_event_t *e = (xcb_map_request_event_t *)event;

  uint32_t vals[5];
  vals[0] = (core.sc->width_in_pixels / 2) - (TFWM_WINDOW_WIDTH / 2);
  vals[1] = (core.sc->height_in_pixels / 2) - (TFWM_WINDOW_HEIGHT / 2);
  vals[2] = TFWM_WINDOW_WIDTH;
  vals[3] = TFWM_WINDOW_HEIGHT;
  vals[4] = TFWM_BORDER_WIDTH;
  xcb_configure_window(
      core.c,
      e->window,
      XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y | XCB_CONFIG_WINDOW_WIDTH |
          XCB_CONFIG_WINDOW_HEIGHT | XCB_CONFIG_WINDOW_BORDER_WIDTH,
      vals
  );
  uint32_t attrvals[1] = {XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE};
  xcb_change_window_attributes(core.c, e->window, XCB_CW_EVENT_MASK, attrvals);
  xcb_map_window(core.c, e->window);
  xcb_flush(core.c);

  tfwm_window_t w;
  w.x = vals[0];
  w.y = vals[1];
  w.w = vals[2];
  w.h = vals[3];
  w.b = vals[4];
  w.win = e->window;
  char *wmc = tfwm_util_window_class(e->window);
  w.class = malloc(strlen(wmc) * sizeof(char));
  strcpy(w.class, wmc);

  tfwm_workspace_window_append(core.cur_ws, w);
  tfwm_layout_update(core.cur_ws);
  tfwm_workspace_window_map(core.cur_ws);
  xcb_clear_area(core.c, 1, core.bar, 0, 0, 0, 0);
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
  if (0 == core.win) {
    return;
  }
  if ((core.win) == core.sc->root) {
    return;
  }
  if (TFWM_LAYOUT_FLOATING != core.ws_list[core.cur_ws].layout) {
    return;
  }

  xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)event;

  xcb_query_pointer_reply_t *point = xcb_query_pointer_reply(
      core.c, xcb_query_pointer(core.c, core.sc->root), NULL
  );
  if (!point) {
    return;
  }

  xcb_get_geometry_reply_t *geo =
      xcb_get_geometry_reply(core.c, xcb_get_geometry(core.c, core.win), NULL);
  if (!geo) {
    free(point);
    return;
  }

  if ((uint32_t)(BTN_LEFT) == core.cur_btn) {
    if ((core.ptr_x == point->root_x) && (core.ptr_y == point->root_y)) {
      free(point);
      free(geo);
      return;
    }

    int x = geo->x + point->root_x - core.ptr_x;
    int y = geo->y + point->root_y - core.ptr_y;
    core.ptr_x = point->root_x;
    core.ptr_y = point->root_y;
    tfwm_window_move(core.win, x, y);
  } else if ((uint32_t)(BTN_RIGHT) == core.cur_btn) {
    if ((point->root_x <= geo->x) || (point->root_y <= geo->y)) {
      free(point);
      free(geo);
      return;
    }

    int w = geo->width + (point->root_x - core.ptr_x);
    int h = geo->height + (point->root_y - core.ptr_y);
    core.ptr_x = point->root_x;
    core.ptr_y = point->root_y;
    tfwm_window_resize(core.win, w, h);
  }

  free(point);
  free(geo);
}

void tfwm_handle_destroy_notify(xcb_generic_event_t *event) {
  xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)event;
  xcb_kill_client(core.c, e->window);
}

void tfwm_handle_button_press(xcb_generic_event_t *event) {
  xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
  core.win = e->child;
  core.ptr_x = e->event_x;
  core.ptr_y = e->event_y;
  tfwm_window_focus(core.win);

  core.cur_btn =
      ((e->detail == BTN_LEFT) ? BTN_LEFT : ((core.win != 0) ? BTN_RIGHT : 0));
  xcb_cursor_t cursor = XCB_NONE;
  if ((uint32_t)BTN_LEFT == core.cur_btn) {
    cursor = tfwm_util_cursor((char *)TFWM_CURSOR_MOVE);
  } else if ((uint32_t)BTN_RIGHT == core.cur_btn) {
    cursor = tfwm_util_cursor((char *)TFWM_CURSOR_RESIZE);
  }
  xcb_grab_pointer(
      core.c,
      0,
      core.sc->root,
      XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
          XCB_EVENT_MASK_POINTER_MOTION_HINT,
      XCB_GRAB_MODE_ASYNC,
      XCB_GRAB_MODE_ASYNC,
      core.sc->root,
      cursor,
      XCB_CURRENT_TIME
  );
}

void tfwm_handle_button_release(xcb_generic_event_t *event) {
  xcb_get_geometry_reply_t *geo =
      xcb_get_geometry_reply(core.c, xcb_get_geometry(core.c, core.win), NULL);
  if (!geo) {
    return;
  }

  tfwm_window_set_attr(core.win, geo->x, geo->y, geo->width, geo->height);
  free(geo);
  xcb_ungrab_pointer(core.c, XCB_CURRENT_TIME);

  return;
}

static int tfwm_handle_event(void) {
  int ret = xcb_connection_has_error(core.c);
  if (ret != 0) {
    return ret;
  }

  xcb_generic_event_t *event = xcb_wait_for_event(core.c);
  if (!event) {
    return ret;
  }

  tfwm_event_handler_t *handler;
  for (handler = event_handlers; handler->func; handler++) {
    uint8_t e = event->response_type & ~0x80;
    if (e == handler->req) {
      handler->func(event);
    }
  }

  free(event);
  xcb_flush(core.c);
  return ret;
}

static void tfwm_bar_render_left(xcb_gcontext_t gc, char *text) {
  xcb_image_text_8(
      core.c, strlen(text), core.bar, gc, core.bar_l, TFWM_FONT_HEIGHT, text
  );
  core.bar_l += tfwm_util_text_width(text);
}

static void tfwm_bar_render_right(xcb_gcontext_t gc, char *text) {
  core.bar_r -= tfwm_util_text_width(text);
  xcb_image_text_8(
      core.c, strlen(text), core.bar, gc, core.bar_r, TFWM_FONT_HEIGHT, text
  );
}

static void tfwm_bar_module_layout(void (*render)(xcb_gcontext_t, char *)) {
  size_t n = ARRAY_LENGTH(cfg_layout);

  for (size_t i = 0; i < n; i++) {
    if (core.ws_list[core.cur_ws].layout == cfg_layout[i].layout) {
      render(core.gc_inactive, cfg_layout[i].sym);
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

static void tfwm_bar_module_wm_info(void (*render)(xcb_gcontext_t, char *)) {
  size_t n1 = strlen(TFWM_NAME);
  size_t n2 = strlen(TFWM_VERSION);
  char s[n1 + n2 + 2];
  memcpy(s, TFWM_NAME, n1);
  s[n1] = '-';
  memcpy(s + n1 + 1, TFWM_VERSION, n2);
  s[n1 + 1 + n2] = '\0';
  render(core.gc_inactive, s);
}

static void tfwm_bar_module_window_tabs() {
  if (0 == core.win) {
    return;
  }
  if ((core.win) == core.sc->root) {
    return;
  }
  tfwm_workspace_t *ws = &core.ws_list[core.cur_ws];
  if (0 == ws->win_len) {
    return;
  }

  char *p_sign = "< ";
  char *n_sign = " >";
  int p_sign_width = tfwm_util_text_width(p_sign);
  int n_sign_width = tfwm_util_text_width(n_sign);
  int sep_width = tfwm_util_text_width(" ");
  int max_width = core.bar_r - (core.bar_l + p_sign_width + n_sign_width);
  int n = tfwm_util_text_width(ws->win_list[core.cur_win].class) + (2 * sep_width);
  int last = ws->win_len - 1;
  int head = core.cur_win > 0 ? core.cur_win - 1 : 0;
  int tail = core.cur_win < last ? core.cur_win + 1 : last;
  int head_done = core.cur_win == head ? 1 : 0;
  int tail_done = core.cur_win == tail ? 1 : 0;

  while (n < max_width) {
    if (head >= 0 && !head_done) {
      n += tfwm_util_text_width(ws->win_list[head].class) + (2 * sep_width);
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
      n += tfwm_util_text_width(ws->win_list[tail].class) + (2 * sep_width);
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
    core.bar_l += p_sign_width;
  }
  if (tail < last) {
    tfwm_bar_render_right(core.gc_inactive, n_sign);
  } else {
    core.bar_r -= n_sign_width;
  }

  for (int i = head; i <= tail; i++) {
    size_t len = strlen(ws->win_list[i].class);
    char c[len + 3];
    c[0] = ' ';
    memcpy(c + 1, ws->win_list[i].class, len);
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
  core.bar_l = 0;
  core.bar_r = core.sc->width_in_pixels;

  tfwm_bar_module_workspace(tfwm_bar_render_left);
  tfwm_bar_module_separator(tfwm_bar_render_left);
  tfwm_bar_module_layout(tfwm_bar_render_left);
  tfwm_bar_module_separator(tfwm_bar_render_left);

  tfwm_bar_module_wm_info(tfwm_bar_render_right);
  tfwm_bar_module_separator(tfwm_bar_render_right);

  tfwm_bar_module_window_tabs();

  xcb_flush(core.c);
}

static void tfwm_ewmh() {
  xcb_window_t wid = xcb_generate_id(core.c);
  xcb_create_window(
      core.c,
      XCB_COPY_FROM_PARENT,
      wid,
      core.sc->root,
      0,
      0,
      1,
      1,
      0,
      XCB_WINDOW_CLASS_INPUT_OUTPUT,
      core.sc->root_visual,
      0,
      NULL
  );

  xcb_intern_atom_reply_t *sc = xcb_intern_atom_reply(
      core.c,
      xcb_intern_atom(
          core.c, 0, strlen("_NET_SUPPORTING_WM_CHECK"), "_NET_SUPPORTING_WM_CHECK"
      ),
      NULL
  );
  if (!sc) {
    return;
  }
  xcb_atom_t wmcheck = sc->atom;
  free(sc);
  xcb_change_property(
      core.c,
      XCB_PROP_MODE_REPLACE,
      core.sc->root,
      wmcheck,
      XCB_ATOM_WINDOW,
      32,
      1,
      &wid
  );
  xcb_change_property(
      core.c, XCB_PROP_MODE_REPLACE, wid, wmcheck, XCB_ATOM_WINDOW, 32, 1, &wid
  );

  xcb_intern_atom_reply_t *wn = xcb_intern_atom_reply(
      core.c,
      xcb_intern_atom(core.c, 0, strlen("_NET_WM_NAME"), "_NET_WM_NAME"),
      NULL
  );
  if (!wn) {
    return;
  }
  xcb_atom_t wmname = wn->atom;
  free(wn);
  xcb_change_property(
      core.c,
      XCB_PROP_MODE_REPLACE,
      wid,
      wmname,
      XCB_ATOM_STRING,
      8,
      strlen(TFWM_NAME),
      TFWM_NAME
  );
}

static void tfwm_init(void) {
  xcb_cursor_t cursor = tfwm_util_cursor((char *)TFWM_CURSOR_DEFAULT);
  uint32_t vals[2] = {
      XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
          XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE,
      cursor
  };
  xcb_change_window_attributes(
      core.c, core.sc->root, XCB_CW_EVENT_MASK | XCB_CW_CURSOR, vals
  );

  xcb_ungrab_key(core.c, XCB_GRAB_ANY, core.sc->root, XCB_MOD_MASK_ANY);
  for (int i = 0; i < ARRAY_LENGTH(cfg_keybinds); i++) {
    xcb_keycode_t *keycode = tfwm_util_keycodes(cfg_keybinds[i].keysym);
    if (keycode != NULL) {
      xcb_grab_key(
          core.c,
          1,
          core.sc->root,
          cfg_keybinds[i].mod,
          *keycode,
          XCB_GRAB_MODE_ASYNC,
          XCB_GRAB_MODE_ASYNC
      );
      free(keycode);
    }
  }
  xcb_flush(core.c);

  xcb_grab_button(
      core.c,
      0,
      core.sc->root,
      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
      XCB_GRAB_MODE_ASYNC,
      XCB_GRAB_MODE_ASYNC,
      core.sc->root,
      XCB_NONE,
      BTN_LEFT,
      MOD_KEY
  );
  xcb_grab_button(
      core.c,
      0,
      core.sc->root,
      XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
      XCB_GRAB_MODE_ASYNC,
      XCB_GRAB_MODE_ASYNC,
      core.sc->root,
      XCB_NONE,
      BTN_RIGHT,
      MOD_KEY
  );
  xcb_flush(core.c);

  core.ws_len = ARRAY_LENGTH(cfg_workspace);
  core.ws_list = malloc(core.ws_len * sizeof(tfwm_workspace_t));
  for (uint32_t i = 0; i < core.ws_len; i++) {
    tfwm_workspace_t ws;
    ws.layout = cfg_layout[0].layout;
    ws.name = cfg_workspace[i];
    core.ws_list[i] = ws;
  }

  core.bar = xcb_generate_id(core.c);
  uint32_t bar_vals[3];
  bar_vals[0] = TFWM_BAR_BACKGROUND;
  bar_vals[1] = 1;
  bar_vals[2] = XCB_EVENT_MASK_EXPOSURE;
  xcb_create_window(
      core.c,
      XCB_COPY_FROM_PARENT,
      core.bar,
      core.sc->root,
      0,
      0,
      core.sc->width_in_pixels,
      TFWM_BAR_HEIGHT,
      0,
      XCB_WINDOW_CLASS_INPUT_OUTPUT,
      core.sc->root_visual,
      XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
      bar_vals
  );
  uint32_t bar_cfg_vals[1] = {XCB_STACK_MODE_ABOVE};
  xcb_configure_window(core.c, core.bar, XCB_CONFIG_WINDOW_STACK_MODE, bar_cfg_vals);
  xcb_map_window(core.c, core.bar);
  xcb_flush(core.c);

  core.font = xcb_generate_id(core.c);
  xcb_open_font(core.c, core.font, strlen(TFWM_FONT), TFWM_FONT);

  core.gc_active = xcb_generate_id(core.c);
  uint32_t active_vals[3];
  active_vals[0] = TFWM_BAR_FOREGROUND_ACTIVE;
  active_vals[1] = TFWM_BAR_BACKGROUND_ACTIVE;
  active_vals[2] = core.font;
  xcb_create_gc(
      core.c,
      core.gc_active,
      core.bar,
      XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT,
      active_vals
  );

  core.gc_inactive = xcb_generate_id(core.c);
  uint32_t inactive_vals[3];
  inactive_vals[0] = TFWM_BAR_FOREGROUND;
  inactive_vals[1] = TFWM_BAR_BACKGROUND;
  inactive_vals[2] = core.font;
  xcb_create_gc(
      core.c,
      core.gc_inactive,
      core.bar,
      XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT,
      inactive_vals
  );

  tfwm_ewmh();
  xcb_flush(core.c);
}

int main(int argc, char *argv[]) {
  core = (tfwm_xcb_t){0};

  if ((2 == argc) && (strcmp("-v", argv[1]) == 0)) {
    printf("tfwm-0.0.1, Copyright (c) 2024 Raihan Rahardyan, MIT License\n");
    return EXIT_SUCCESS;
  }

  if (argc != 1) {
    printf("usage: tfwm [-v]\n");
    return EXIT_SUCCESS;
  }

  core.c = xcb_connect(NULL, NULL);
  core.exit = xcb_connection_has_error(core.c);
  if (core.exit > 0) {
    printf("xcb_connection_has_error\n");
    return core.exit;
  }

  core.sc = xcb_setup_roots_iterator(xcb_get_setup(core.c)).data;
  tfwm_init();

  while (core.exit == EXIT_SUCCESS) {
    core.exit = tfwm_handle_event();

    if (core.sc) {
      tfwm_bar_run();
    }
  }

  return core.exit;
}
