#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "bar.h"
#include "config.h"
#include "tfwm.h"

static xcb_connection_t *g_conn;
static xcb_screen_t     *g_screen;
static xcb_window_t      g_window;

static uint32_t g_vals[3];
static int      g_curr_ws;
static int      g_prev_ws;

/* ========================== UTILS ========================== */

int tfwm_util_write_error(char *err) {
    size_t n = 0;
    char  *e = err;
    while ((*(e++)) != 0) {
        ++n;
    }

    ssize_t out = write(STDERR_FILENO, err, n);
    int     ret = 1;
    if (out < 0) {
        ret = -1;
    }
    return ret;
}

int tfwm_util_compare_str(char *str1, char *str2) {
    char *c1 = str1;
    char *c2 = str2;
    while ((*c1) && ((*c1) == (*c2))) {
        ++c1;
        ++c2;
    }

    int n = (*c1) - (*c2);
    return n;
}

xcb_keycode_t *tfwm_util_get_keycodes(xcb_keysym_t keysym) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(g_conn);
    xcb_keycode_t     *keycode =
        (!(ks) ? NULL : xcb_key_symbols_get_keycode(ks, keysym));
    xcb_key_symbols_free(ks);
    return keycode;
}

xcb_keysym_t tfwm_util_get_keysym(xcb_keycode_t keycode) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(g_conn);
    xcb_keysym_t keysym = (!(ks) ? 0 : xcb_key_symbols_get_keysym(ks, keycode, 0));
    xcb_key_symbols_free(ks);
    return keysym;
}

/* ===================== WINDOW FUNCTION ===================== */

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
    if (g_window == g_screen->root) {
        return;
    }

    int ws_id = -1;
    for (int i = 0; i < workspaces[g_curr_ws].window_len; i++) {
        if (workspaces[g_curr_ws].window_list[i].window == g_window) {
            ws_id = i;
            break;
        }
    }
    if (ws_id < 0) {
        return;
    }

    xcb_kill_client(g_conn, g_window);

    if (ws_id < workspaces[g_curr_ws].window_len - 1) {
        for (int i = ws_id + 1; i < workspaces[g_curr_ws].window_len; i++) {
            workspaces[g_curr_ws].window_list[i - 1] =
                workspaces[g_curr_ws].window_list[i];
        }
    }
    workspaces[g_curr_ws].window_len -= 1;

    if (workspaces[g_curr_ws].window_len == 0) {
        return;
    } else if ((ws_id + 1) >= workspaces[g_curr_ws].window_len) {
        tfwm_window_focus(workspaces[g_curr_ws]
                              .window_list[workspaces[g_curr_ws].window_len - 1]
                              .window);
    } else if ((ws_id + 1) < workspaces[g_curr_ws].window_len) {
        tfwm_window_focus(workspaces[g_curr_ws].window_list[ws_id].window);
    }
}

void tfwm_window_focus(xcb_window_t window) {
    if (window == 0) {
        return;
    }
    if (window == g_screen->root) {
        return;
    }

    xcb_set_input_focus(g_conn, XCB_INPUT_FOCUS_POINTER_ROOT, window,
                        XCB_CURRENT_TIME);

    for (int i = 0; i < workspaces[g_curr_ws].window_len; i++) {
        if (workspaces[g_curr_ws].window_list[i].window == window) {
            g_window = window;
            break;
        }
    }
}

void tfwm_window_focus_color(xcb_window_t window, int focus) {
    if (BORDER_WIDTH <= 0) {
        return;
    }
    if (window == 0) {
        return;
    }
    if (window == g_screen->root) {
        return;
    }

    uint32_t vals[1];
    vals[0] = focus ? BORDER_ACTIVE : BORDER_INACTIVE;
    xcb_change_window_attributes(g_conn, window, XCB_CW_BORDER_PIXEL, vals);
    xcb_flush(g_conn);
}

void tfwm_window_map(xcb_window_t window) {
    xcb_map_window(g_conn, window);
    xcb_flush(g_conn);
}

void tfwm_window_unmap(xcb_window_t window) {
    xcb_unmap_window(g_conn, window);
    xcb_flush(g_conn);
}

void tfwm_window_move(int x, int y) {
    g_vals[0] = x;
    g_vals[1] = y;
    xcb_configure_window(g_conn, g_window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                         g_vals);
}

void tfwm_window_resize(int width, int height) {
    if ((width < MIN_WINDOW_WIDTH) || (height < MIN_WINDOW_HEIGHT)) {
        return;
    }

    g_vals[0] = width;
    g_vals[1] = height;
    xcb_configure_window(g_conn, g_window,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, g_vals);
}

/* =================== WORKSPACE FUNCTION ==================== */

void tfwm_workspace_remap(void) {
    if (g_curr_ws == g_prev_ws) {
        return;
    }

    for (int i = 0; i < workspaces[g_prev_ws].window_len; i++) {
        tfwm_window_unmap(workspaces[g_prev_ws].window_list[i].window);
    }

    for (int i = 0; i < workspaces[g_curr_ws].window_len; i++) {
        tfwm_window_map(workspaces[g_curr_ws].window_list[i].window);

        if (i == (workspaces[g_curr_ws].window_len - 1)) {
            tfwm_window_focus(workspaces[g_curr_ws].window_list[i].window);
        }
    }
}

void tfwm_workspace_switch(char **cmd) {
    for (int i = 0; i < sizeof(workspaces) / sizeof(*workspaces); i++) {
        char *ws = (char *)cmd[0];
        if ((tfwm_util_compare_str(ws, workspaces[i].name) == 0) &&
            (i != g_curr_ws)) {
            g_prev_ws = g_curr_ws;
            g_curr_ws = i;
        }
    }

    tfwm_workspace_remap();
}

void tfwm_workspace_next(char **cmd) {
    size_t ws_len = (sizeof(workspaces) / sizeof(*workspaces));
    if ((g_curr_ws + 1) == ws_len) {
        g_prev_ws = g_curr_ws;
        g_curr_ws = 0;
    } else {
        g_prev_ws = g_curr_ws;
        g_curr_ws += 1;
    }

    tfwm_workspace_remap();
}

void tfwm_workspace_prev(char **cmd) {
    size_t ws_len = (sizeof(workspaces) / sizeof(*workspaces));
    if ((g_curr_ws - 1) < 0) {
        g_prev_ws = g_curr_ws;
        g_curr_ws = ws_len - 1;
    } else {
        g_prev_ws = g_curr_ws;
        g_curr_ws -= 1;
    }

    tfwm_workspace_remap();
}

void tfwm_workspace_swap_prev(char **cmd) {
    g_prev_ws = g_prev_ws ^ g_curr_ws;
    g_curr_ws = g_prev_ws ^ g_curr_ws;
    g_prev_ws = g_prev_ws ^ g_curr_ws;

    tfwm_workspace_remap();
}

void tfwm_workspace_use_tiling(char **cmd) {
    workspaces[g_curr_ws].default_layout = TILING;
}

void tfwm_workspace_use_floating(char **cmd) {
    workspaces[g_curr_ws].default_layout = FLOATING;
}

void tfwm_workspace_use_window(char **cmd) {
    workspaces[g_curr_ws].default_layout = WINDOW;
}

void tfwm_workspace_window_malloc(tfwm_workspace_t *ws) {
    if (ws->window_list) {
        return;
    }

    ws->window_list =
        (tfwm_window_t *)malloc(DEFAULT_WS_WIN_MALLOC * sizeof(tfwm_window_t));
    ws->window_cap = DEFAULT_WS_WIN_MALLOC;
}

void tfwm_workspace_window_realloc(tfwm_workspace_t *ws) {
    if (!ws->window_list) {
        return;
    }

    tfwm_window_t *new_win = (tfwm_window_t *)realloc(
        ws->window_list,
        (ws->window_cap + DEFAULT_WS_WIN_REALLOC) * sizeof(tfwm_window_t));
    if (!new_win) {
        return;
    }

    ws->window_list = new_win;
    ws->window_cap += DEFAULT_WS_WIN_REALLOC;
}

void tfwm_workspace_window_append(tfwm_workspace_t *ws, tfwm_window_t w) {
    ws->window_list[ws->window_len++] = w;
}

/* ====================== EVENT HANDLER ====================== */

void tfwm_handle_keypress(xcb_generic_event_t *event) {
    xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
    xcb_keysym_t           keysym = tfwm_util_get_keysym(e->detail);

    for (int i = 0; i < sizeof(keybinds) / sizeof(*keybinds); i++) {
        if ((keybinds[i].keysym == keysym) && (keybinds[i].mod == e->state)) {
            keybinds[i].func(keybinds[i].cmd);
        }
    }
}

void tfwm_handle_map_request(xcb_generic_event_t *event) {
    xcb_map_request_event_t *e = (xcb_map_request_event_t *)event;

    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    if (!ws->window_list) {
        tfwm_workspace_window_malloc(ws);
    } else if (ws->window_cap == ws->window_len) {
        tfwm_workspace_window_realloc(ws);
    }
    tfwm_window_t win = {(ws->default_layout == FLOATING ? 1 : 0), e->window};
    tfwm_workspace_window_append(ws, win);
    g_window = e->window;

    tfwm_window_map(e->window);
    uint32_t vals[5];
    vals[0] = (g_screen->width_in_pixels / 2) - (WINDOW_WIDTH / 2);
    vals[1] = (g_screen->height_in_pixels / 2) - (WINDOW_HEIGHT / 2);
    vals[2] = WINDOW_WIDTH;
    vals[3] = WINDOW_HEIGHT;
    vals[4] = BORDER_WIDTH;
    xcb_configure_window(g_conn, e->window,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         vals);

    g_vals[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_conn, e->window, XCB_CW_EVENT_MASK,
                                         g_vals);
    tfwm_window_focus(e->window);
}

void tfwm_handle_focus_in(xcb_generic_event_t *event) {
    xcb_focus_in_event_t *e = (xcb_focus_in_event_t *)event;
    tfwm_window_focus_color(e->event, 1);
}

void tfwm_handle_focus_out(xcb_generic_event_t *event) {
    xcb_focus_out_event_t *e = (xcb_focus_out_event_t *)event;
    tfwm_window_focus_color(e->event, 0);
}

void tfwm_handle_motion_notify(xcb_generic_event_t *event) {
    if (g_window == 0) {
        return;
    }

    xcb_query_pointer_reply_t *point = xcb_query_pointer_reply(
        g_conn, xcb_query_pointer(g_conn, g_screen->root), 0);
    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply(g_conn, xcb_get_geometry(g_conn, g_window), 0);

    if (g_vals[2] == (uint32_t)(BUTTON_LEFT)) {
        int x = point->root_x;
        int y = point->root_y;
        tfwm_window_move(x, y);
    } else if (g_vals[2] == (uint32_t)(BUTTON_RIGHT)) {
        if ((point->root_x <= geo->x) || (point->root_y <= geo->y)) {
            return;
        }

        int width = point->root_x - geo->x - BORDER_WIDTH;
        int height = point->root_y - geo->y - BORDER_WIDTH;
        tfwm_window_resize(width, height);
    }
}

void tfwm_handle_enter_notify(xcb_generic_event_t *event) {
    xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)event;
    tfwm_window_focus(e->event);
}

void tfwm_handle_destroy_notify(xcb_generic_event_t *event) {
    xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)event;
    xcb_kill_client(g_conn, e->window);
}

void tfwm_handle_button_press(xcb_generic_event_t *event) {
    xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
    g_window = e->child;
    g_vals[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_conn, g_window, XCB_CONFIG_WINDOW_STACK_MODE, g_vals);

    g_vals[2] = ((e->detail == BUTTON_LEFT) ? BUTTON_LEFT
                                            : ((g_window != 0) ? BUTTON_RIGHT : 0));
    xcb_grab_pointer(g_conn, 0, g_screen->root,
                     XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
                         XCB_EVENT_MASK_POINTER_MOTION_HINT,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_screen->root,
                     XCB_NONE, XCB_CURRENT_TIME);
}

void tfwm_handle_button_release(xcb_generic_event_t *event) {
    xcb_ungrab_pointer(g_conn, XCB_CURRENT_TIME);
}

int tfwm_handle_event(void) {
    int ret = xcb_connection_has_error(g_conn);
    if (ret != 0) {
        return ret;
    }

    xcb_generic_event_t *event = xcb_wait_for_event(g_conn);
    if (!event) {
        return ret;
    }

    tfwm_event_handler_t *handler;
    for (handler = event_handlers; handler->func; handler++) {
        if ((event->response_type & ~0x80) == handler->request) {
            handler->func(event);
        }
    }

    free(event);
    xcb_flush(g_conn);
    return ret;
}

/* ========================== SETUP ========================== */

static void tfwm_init(void) {
    g_vals[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
                XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(g_conn, g_screen->root, XCB_CW_EVENT_MASK,
                                         g_vals);

    xcb_ungrab_key(g_conn, XCB_GRAB_ANY, g_screen->root, XCB_MOD_MASK_ANY);
    for (int i = 0; i < sizeof(keybinds) / sizeof(*keybinds); i++) {
        xcb_keycode_t *keycode = tfwm_util_get_keycodes(keybinds[i].keysym);
        if (keycode != NULL) {
            xcb_grab_key(g_conn, 1, g_screen->root, keybinds[i].mod, *keycode,
                         XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }
    }
    xcb_flush(g_conn);

    xcb_grab_button(g_conn, 0, g_screen->root,
                    XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_screen->root,
                    XCB_NONE, BUTTON_LEFT, MOD_KEY);
    xcb_grab_button(g_conn, 0, g_screen->root,
                    XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_screen->root,
                    XCB_NONE, BUTTON_RIGHT, MOD_KEY);
    xcb_flush(g_conn);
}

void tfwm_exit(char **cmd) {
    if (g_conn) {
        xcb_disconnect(g_conn);
    }
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
        g_conn = xcb_connect(NULL, NULL);
        ret = xcb_connection_has_error(g_conn);
        if (ret > 0) {
            ret = tfwm_util_write_error("xcb_connection_has_error\n");
        }
    }

    if (ret == 0) {
        g_screen = xcb_setup_roots_iterator(xcb_get_setup(g_conn)).data;
        tfwm_init();
    }

    while (ret == 0) {
        ret = tfwm_handle_event();

        if (g_screen) {
            tfwm_left_bar(g_conn, g_screen, g_curr_ws);
        }
    }

    return ret;
}
