#ifndef TFWM_H
#define TFWM_H

#include <stdlib.h>
#include <xcb/xproto.h>

/* ========================= SYS CFG ========================= */

static const int TFWM_DEFAULT_WS_WIN_ALLOC = 5;

/* ========================= TYPEDEF ========================= */

typedef struct {
    xcb_window_t window;
    int          x;
    int          y;
    int          width;
    int          height;
    int          border_width;
    uint8_t      is_floating;
    uint8_t      is_fullscreen;
} tfwm_window_t;

typedef enum {
    TFWM_TILING,
    TFWM_FLOATING,
    TFWM_WINDOW,
} tfwm_layout_t;

typedef struct {
    uint16_t       layout;
    char          *name;
    size_t         window_len;
    size_t         window_cap;
    tfwm_window_t *window_list;
} tfwm_workspace_t;

typedef struct {
    uint16_t     mod;
    xcb_keysym_t keysym;
    void (*func)(char **cmd);
    const char **cmd;
} tfwm_keybind_t;

typedef struct {
    uint32_t request;
    void (*func)(xcb_generic_event_t *evt);
} tfwm_event_handler_t;

/* ========================== UTILS ========================== */

int               tfwm_util_write_error(char *err);
void              tfwm_util_fwrite_log(char *log);
xcb_keycode_t    *tfwm_util_get_keycodes(xcb_keysym_t keysym);
xcb_keysym_t      tfwm_util_get_keysym(xcb_keycode_t keycode);
char             *tfwm_util_get_wm_class(xcb_window_t window);
void              tfwm_util_set_cursor(xcb_window_t window, char *name);
size_t            tfwm_util_get_workspaces_len(void);
tfwm_workspace_t *tfwm_util_get_workspaces(void);
tfwm_workspace_t *tfwm_util_find_workspace(size_t wsid);
tfwm_workspace_t *tfwm_util_get_current_workspace(void);
int               tfwm_util_get_current_window_id(void);
int               tfwm_util_check_current_workspace(size_t wsid);
int               tfwm_util_check_current_window(xcb_window_t window);
void              tfwm_util_redraw_bar(void);

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

void tfwm_window_raise(xcb_window_t window);
void tfwm_window_focus(xcb_window_t window);
void tfwm_window_focus_color(xcb_window_t window, int focus);
void tfwm_window_move(xcb_window_t window, int x, int y);
void tfwm_window_resize(xcb_window_t window, int width, int height);
void tfwm_window_set_attr(xcb_window_t window, int x, int y, int width, int height);

/* =================== WORKSPACE FUNCTION ==================== */

void tfwm_workspace_remap(void);

void tfwm_workspace_window_malloc(void);
void tfwm_workspace_window_realloc(void);
void tfwm_workspace_window_append(tfwm_window_t window);

/* ===================== LAYOUT FUNCTION ===================== */

void tfwm_layout_apply_tiling(void);
void tfwm_layout_apply_window(void);
void tfwm_layout_update(void);

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
int  tfwm_handle_event(void);

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

#endif  // !TFWM_H
