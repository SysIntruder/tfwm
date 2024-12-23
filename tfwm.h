#ifndef TFWM_H
#define TFWM_H

#include <stdlib.h>
#include <xcb/xproto.h>

/* ========================= TYPEDEF ========================= */

typedef struct {
    uint8_t is_floating;
    xcb_window_t window;
} tfwm_window_t;

typedef struct {
    uint16_t default_layout;
    char *name;
    size_t win_len;
    size_t win_cap;
    tfwm_window_t *win_list;
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

int tfwm_util_write_error(char *err);
int tfwm_util_compare_str(char *str1, char *str2);
xcb_keycode_t *tfwm_get_keycode(xcb_keysym_t keysym);
xcb_keysym_t tfwm_get_keysym(xcb_keycode_t keycode);

/* ===================== WINDOW FUNCTION ===================== */

void tfwm_exit(char **cmd);
void tfwm_spawn(char **cmd);
void tfwm_kill(char **cmd);

void tfwm_focus_window(xcb_window_t win);
void tfwm_focus_color_window(xcb_window_t win, int focus);
void tfwm_map_window(xcb_window_t win);
void tfwm_unmap_window(xcb_window_t win);

/* =================== WORKSPACE FUNCTION ==================== */

void tfwm_remap_workspace(void);
void tfwm_goto_workspace(char **cmd);
void tfwm_next_workspace(char **cmd);
void tfwm_prev_workspace(char **cmd);
void tfwm_swap_prev_workspace(char **cmd);
void tfwm_workspace_use_tiling(char **cmd);
void tfwm_workspace_use_floating(char **cmd);
void tfwm_workspace_use_window(char **cmd);

void tfwm_workspace_window_malloc(tfwm_workspace_t *ws);
void tfwm_workspace_window_realloc(tfwm_workspace_t *ws);
void tfwm_workspace_window_append(tfwm_workspace_t *ws, tfwm_window_t w);

/* ====================== EVENT HANDLER ====================== */

void tfwm_handle_keypress(xcb_generic_event_t *evt);
void tfwm_handle_map_request(xcb_generic_event_t *evt);
void tfwm_handle_focus_in(xcb_generic_event_t *evt);
void tfwm_handle_focus_out(xcb_generic_event_t *evt);
int tfwm_handle_event(void);

/* ======================= VARIABLES ========================= */

static tfwm_event_handler_t event_handlers[] = {
    {XCB_KEY_PRESS, tfwm_handle_keypress},
    {XCB_MAP_REQUEST, tfwm_handle_map_request},
    {XCB_FOCUS_IN, tfwm_handle_focus_in},
    {XCB_FOCUS_OUT, tfwm_handle_focus_out},
    {XCB_NONE, NULL},
};

/* ========================== SETUP ========================== */

static void tfwm_init(void);

#endif // !TFWM_H
