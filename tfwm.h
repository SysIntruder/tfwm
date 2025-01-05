#ifndef TFWM_H
#define TFWM_H

#include <stdlib.h>
#include <xcb/xproto.h>

/* ========================= SYS CFG ========================= */

static const int TFWM_WIN_LIST_ALLOC = 5;
static const char *TFWM_NAME = "tfwm";
static const char *TFWM_VERSION = "0.0.1";

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
  int w;
  int h;
  int b;
  xcb_window_t win;
  char *class;
} tfwm_window_t;

typedef struct {
  uint16_t layout;
  char *sym;
} tfwm_layout_t;

typedef struct {
  uint16_t layout;
  uint32_t win_len;
  uint32_t win_cap;
  const char *name;
  tfwm_window_t *win_list;
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
  xcb_window_t win;
  xcb_window_t bar;
  xcb_font_t font;
  xcb_gcontext_t gc_active;
  xcb_gcontext_t gc_inactive;
  int bar_left;
  int bar_right;
  int ptr_x;
  int ptr_y;
  int exit;
  uint32_t cur_btn;
  uint32_t cur_win;
  uint32_t cur_ws;
  uint32_t prv_ws;
  uint32_t ws_len;
  tfwm_workspace_t *ws_list;
} tfwm_xcb_t;

/* ========================== UTILS ========================== */

static void tfwm_util_log(char *log, int exit);
static xcb_keycode_t *tfwm_util_keycodes(xcb_keysym_t keysym);
static xcb_keysym_t tfwm_util_keysym(xcb_keycode_t keycode);
static xcb_cursor_t tfwm_util_cursor(char *name);
static char *tfwm_util_window_class(xcb_window_t window);
static int tfwm_util_text_width(char *text);
static void tfwm_util_cleanup(void);

/* ========================= COMMAND ========================= */

void tfwm_exit(char **cmd);

void tfwm_window_spawn(char **cmd);
void tfwm_window_kill(char **cmd);
void tfwm_window_next(char **cmd);
void tfwm_window_prev(char **cmd);
void tfwm_window_swap_last(char **cmd);
void tfwm_window_fullscreen(char **cmd);
void tfwm_window_to_workspace(char **cmd);

void tfwm_workspace_switch(char **cmd);
void tfwm_workspace_next(char **cmd);
void tfwm_workspace_prev(char **cmd);
void tfwm_workspace_swap_prev(char **cmd);
void tfwm_workspace_use_tiling(char **cmd);
void tfwm_workspace_use_floating(char **cmd);
void tfwm_workspace_use_window(char **cmd);

/* ===================== WINDOW FUNCTION ===================== */

static void tfwm_window_focus(xcb_window_t window);
static void tfwm_window_color(xcb_window_t window, uint32_t color);
static void tfwm_window_move(xcb_window_t window, int x, int y);
static void tfwm_window_resize(xcb_window_t window, int w, int h);
static void tfwm_window_set_attr(xcb_window_t window, int x, int y, int w, int h);

/* =================== WORKSPACE FUNCTION ==================== */

static void tfwm_workspace_window_unmap(uint32_t wsid);
static void tfwm_workspace_window_map(uint32_t wsid);
static void tfwm_workspace_window_malloc(uint32_t wsid);
static void tfwm_workspace_window_realloc(uint32_t wsid);
static void tfwm_workspace_window_append(uint32_t wsid, tfwm_window_t window);
static void tfwm_workspace_window_pop(uint32_t wid);

/* ===================== LAYOUT FUNCTION ===================== */

static void tfwm_layout_apply_tiling(uint32_t wsid);
static void tfwm_layout_apply_window(uint32_t wsid);
static void tfwm_layout_update(uint32_t wsid);

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
static void tfwm_bar_module_wm_info(void (*render)(xcb_gcontext_t, char *));
static void tfwm_bar_module_window();
static void tfwm_bar_run();

/* ======================= VARIABLES ========================= */

static tfwm_event_handler_t event_handlers[] = {
    {XCB_KEY_PRESS, tfwm_handle_keypress},
    {XCB_MAP_REQUEST, tfwm_handle_map_request},
    {XCB_FOCUS_IN, tfwm_handle_focus_in},
    {XCB_FOCUS_OUT, tfwm_handle_focus_out},
    {XCB_ENTER_NOTIFY, tfwm_handle_enter_notify},
    {XCB_LEAVE_NOTIFY, tfwm_handle_leave_notify},
    {XCB_MOTION_NOTIFY, tfwm_handle_motion_notify},
    {XCB_DESTROY_NOTIFY, tfwm_handle_destroy_notify},
    {XCB_BUTTON_PRESS, tfwm_handle_button_press},
    {XCB_BUTTON_RELEASE, tfwm_handle_button_release},
    {XCB_NONE, NULL},
};

/* ========================== SETUP ========================== */

static void tfwm_ewmh();
static void tfwm_init(void);

#endif  // !TFWM_H
