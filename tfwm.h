#ifndef TFWM_H
#define TFWM_H

#include <stdlib.h>
#include <xcb/xproto.h>

/* ========================= SYS CFG ========================= */

static const int TFWM_DEFAULT_WS_WIN_ALLOC = 5;

/* ========================== ENUMS ========================== */

enum {
  TFWM_LAYOUT_TILING,
  TFWM_LAYOUT_FLOATING,
  TFWM_LAYOUT_WINDOW,
};

/* ========================= TYPEDEF ========================= */

typedef struct {
  uint8_t is_fullscreen;
  int x;
  int y;
  int width;
  int height;
  int border_width;
  xcb_window_t window;
  char *class;
} tfwm_window_t;

typedef struct {
  uint16_t layout;
  char *symbol;
} tfwm_layout_t;

typedef struct {
  uint16_t layout;
  uint32_t window_len;
  uint32_t window_cap;
  const char *name;
  tfwm_window_t *window_list;
} tfwm_workspace_t;

typedef struct {
  uint16_t mod;
  xcb_keysym_t keysym;
  void (*func)(char **cmd);
  const char **cmd;
} tfwm_keybind_t;

typedef struct {
  uint32_t request;
  void (*func)(xcb_generic_event_t *evt);
} tfwm_event_handler_t;

typedef struct {
  xcb_connection_t *conn;
  xcb_screen_t *screen;
  xcb_window_t window;
  xcb_window_t bar;
  xcb_font_t font;
  xcb_gcontext_t gc_active;
  xcb_gcontext_t gc_inactive;
  int bar_x_left;
  int bar_x_right;
  int pointer_x;
  int pointer_y;
  int exit;
  uint32_t cur_button;
  uint32_t cur_window;
  uint32_t cur_workspace;
  uint32_t prv_workspace;
  uint32_t workspace_len;
  tfwm_workspace_t *workspace_list;
} tfwm_xcb_t;

/* ========================== UTILS ========================== */

static void tfwm_util_log(char *log, int exit);
static xcb_keycode_t *tfwm_util_get_keycodes(xcb_keysym_t keysym);
static xcb_keysym_t tfwm_util_get_keysym(xcb_keycode_t keycode);
static char *tfwm_util_get_wm_class(xcb_window_t window);
static void tfwm_util_set_cursor(xcb_window_t window, char *name);
static int tfwm_util_text_width(char *text);
static void tfwm_util_redraw_bar(void);

/* ========================= COMMAND ========================= */

void tfwm_exit(char **cmd);

void tfwm_window_spawn(char **cmd);
void tfwm_window_kill(char **cmd);
void tfwm_window_next(char **cmd);
void tfwm_window_prev(char **cmd);
void tfwm_window_swap_last(char **cmd);
void tfwm_window_toggle_fullscreen(char **cmd);

void tfwm_workspace_switch(char **cmd);
void tfwm_workspace_next(char **cmd);
void tfwm_workspace_prev(char **cmd);
void tfwm_workspace_swap_prev(char **cmd);
void tfwm_workspace_use_tiling(char **cmd);
void tfwm_workspace_use_floating(char **cmd);
void tfwm_workspace_use_window(char **cmd);

/* ===================== WINDOW FUNCTION ===================== */

static void tfwm_window_raise(xcb_window_t window);
static void tfwm_window_focus(xcb_window_t window);
static void tfwm_window_focus_color(xcb_window_t window, int focus);
static void tfwm_window_move(xcb_window_t window, int x, int y);
static void tfwm_window_resize(xcb_window_t window, int width, int height);
static void tfwm_window_set_attr(xcb_window_t window, int x, int y, int width,
                                 int height);

/* =================== WORKSPACE FUNCTION ==================== */

static void tfwm_workspace_remap(void);

static void tfwm_workspace_window_malloc(void);
static void tfwm_workspace_window_realloc(void);
static void tfwm_workspace_window_append(tfwm_window_t window);

/* ===================== LAYOUT FUNCTION ===================== */

static void tfwm_layout_apply_tiling(void);
static void tfwm_layout_apply_window(void);
static void tfwm_layout_update(void);

/* ====================== EVENT HANDLER ====================== */

void tfwm_handle_keypress(xcb_generic_event_t *event);
void tfwm_handle_map_request(xcb_generic_event_t *event);
void tfwm_handle_focus_in(xcb_generic_event_t *event);
void tfwm_handle_focus_out(xcb_generic_event_t *event);
void tfwm_handle_enter_notify(xcb_generic_event_t *event);
void tfwm_handle_leave_notify(xcb_generic_event_t *event);
void tfwm_handle_motion_notify(xcb_generic_event_t *event);
void tfwm_handle_destroy_notify(xcb_generic_event_t *event);
void tfwm_handle_button_press(xcb_generic_event_t *event);
void tfwm_handle_button_release(xcb_generic_event_t *event);

static int tfwm_handle_event(void);

/* =========================== BAR =========================== */

static void tfwm_bar_render_left(xcb_gcontext_t gc, char *text);
static void tfwm_bar_render_right(xcb_gcontext_t gc, char *text);
static void tfwm_bar_module_layout(void (*render)(xcb_gcontext_t, char *));
static void tfwm_bar_module_separator(void (*render)(xcb_gcontext_t, char *));
static void tfwm_bar_module_workspace(void (*render)(xcb_gcontext_t, char *));
static void tfwm_bar_module_window();
static void tfwm_bar_run();

/* ======================= VARIABLES ========================= */

static tfwm_event_handler_t event_handlers[] = {
    {XCB_KEY_PRESS,      tfwm_handle_keypress      },
    {XCB_MAP_REQUEST,    tfwm_handle_map_request   },
    {XCB_FOCUS_IN,       tfwm_handle_focus_in      },
    {XCB_FOCUS_OUT,      tfwm_handle_focus_out     },
    {XCB_ENTER_NOTIFY,   tfwm_handle_enter_notify  },
    {XCB_LEAVE_NOTIFY,   tfwm_handle_leave_notify  },
    {XCB_MOTION_NOTIFY,  tfwm_handle_motion_notify },
    {XCB_DESTROY_NOTIFY, tfwm_handle_destroy_notify},
    {XCB_BUTTON_PRESS,   tfwm_handle_button_press  },
    {XCB_BUTTON_RELEASE, tfwm_handle_button_release},
    {XCB_NONE,           NULL                      },
};

/* ========================== SETUP ========================== */

static void tfwm_init(void);
static void tfwm_init_bar(void);

#endif  // !TFWM_H
