#ifndef TFWM_H
#define TFWM_H

#include <stdlib.h>
#include <xcb/xproto.h>

/* ======================== SYS CONF ========================= */

#define DEFAULT_WS_WIN_MALLOC  5
#define DEFAULT_WS_WIN_REALLOC 5

/* ========================= TYPEDEF ========================= */

typedef struct {
    uint8_t      is_floating;
    xcb_window_t window;
} tfwm_window_t;

typedef struct {
    uint16_t       default_layout;
    char          *name;
    size_t         window_len;
    size_t         window_cap;
    tfwm_window_t *window_list;
} tfwm_workspace_t;

typedef struct {
    uint16_t     mod;
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

int            tfwm_util_write_error(char *err);
int            tfwm_util_compare_str(char *str1, char *str2);
xcb_keycode_t *tfwm_get_keycode(xcb_keysym_t keysym);
xcb_keysym_t   tfwm_get_keysym(xcb_keycode_t keycode);

/* ===================== WINDOW FUNCTION ===================== */

void tfwm_window_spawn(char **cmd);
void tfwm_window_kill(char **cmd);

void tfwm_window_focus(xcb_window_t window);
void tfwm_window_focus_color(xcb_window_t window, int focus);
void tfwm_window_map(xcb_window_t window);
void tfwm_window_unmap(xcb_window_t window);
void tfwm_window_move(int x, int y);
void tfwm_window_resize(int width, int height);

/* =================== WORKSPACE FUNCTION ==================== */

void tfwm_workspace_remap(void);
void tfwm_workspace_switch(char **cmd);
void tfwm_workspace_next(char **cmd);
void tfwm_workspace_prev(char **cmd);
void tfwm_workspace_swap_prev(char **cmd);
void tfwm_workspace_use_tiling(char **cmd);
void tfwm_workspace_use_floating(char **cmd);
void tfwm_workspace_use_window(char **cmd);

void tfwm_workspace_window_malloc(tfwm_workspace_t *ws);
void tfwm_workspace_window_realloc(tfwm_workspace_t *ws);
void tfwm_workspace_window_append(tfwm_workspace_t *ws, tfwm_window_t w);

/* ====================== EVENT HANDLER ====================== */

void tfwm_handle_keypress(xcb_generic_event_t *event);
void tfwm_handle_map_request(xcb_generic_event_t *event);
void tfwm_handle_focus_in(xcb_generic_event_t *event);
void tfwm_handle_focus_out(xcb_generic_event_t *event);
void tfwm_handle_motion_notify(xcb_generic_event_t *event);
void tfwm_handle_enter_notify(xcb_generic_event_t *event);
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
    {XCB_MOTION_NOTIFY,  tfwm_handle_motion_notify },
    {XCB_ENTER_NOTIFY,   tfwm_handle_enter_notify  },
    {XCB_DESTROY_NOTIFY, tfwm_handle_destroy_notify},
    {XCB_BUTTON_PRESS,   tfwm_handle_button_press  },
    {XCB_BUTTON_RELEASE, tfwm_handle_button_release},
    {XCB_NONE,           NULL                      },
};

/* ========================== SETUP ========================== */

static void tfwm_init(void);
void        tfwm_exit(char **cmd);

#endif  // !TFWM_H
