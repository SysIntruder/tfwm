#ifndef TFWM_H
#define TFWM_H

#include <stdlib.h>
#include <xcb/xproto.h>

/* ========================= TYPEDEF ========================= */

typedef struct {
    uint16_t default_layout;
    char *name;
} tfwm_workspace_t;

typedef struct {
    uint16_t mod;
    xcb_keysym_t keysym;
    void (*func)(char **cmd);
    char **cmd;
} tfwm_keybind_t;

typedef struct {
    uint32_t request;
    void (*func)(xcb_generic_event_t *ev);
} tfwm_event_handler_t;

/* ========================== ENUMS ========================== */

enum tfwm_layouts {
  TILING,
  FLOATING,
  WINDOW,
};

/* ========================== UTILS ========================== */

static int tfwm_util_write_error(char *err);
static int tfwm_util_compare_str(char *str1, char *str2);
static xcb_keycode_t *tfwm_get_keycode(xcb_keysym_t keysym);
static xcb_keysym_t tfwm_get_keysym(xcb_keycode_t keycode);

/* ===================== WINDOW FUNCTION ===================== */

static void tfwm_exit(char **cmd);
static void tfwm_goto_workspace(char **cmd);

/* ====================== EVENT HANDLER ====================== */

static void tfwm_handle_keypress(xcb_generic_event_t *evt);
static int tfwm_handle_event(void);

/* ======================= VARIABLES ========================= */

static tfwm_event_handler_t event_handlers[] = {
    {XCB_KEY_PRESS, tfwm_handle_keypress}, {XCB_NONE, NULL}};

/* ========================== DEBUG ========================== */

static char *tfwm_debug_workspace_str(void);
static void tfwm_debug_hud(void);

/* ========================== SETUP ========================== */

static void tfwm_init(void);

#endif // !TFWM_H
