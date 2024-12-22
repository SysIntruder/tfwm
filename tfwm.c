#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "config.h"
#include "tfwm.h"

static xcb_connection_t *gp_conn;
static xcb_screen_t *gp_scrn;
static xcb_drawable_t gp_win;
static uint32_t gp_vals[3];

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

  while (ret == 0) {
    ret = tfwm_handle_event();
  }

  return ret;
}
