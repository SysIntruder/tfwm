#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "bar.h"
#include "config.h"
#include "tfwm.h"

static xcb_connection_t *g_conn;
static xcb_screen_t     *g_screen;
static xcb_window_t      g_window;
static xcb_window_t      g_bar;

static uint32_t g_vals[3];
static int      g_curr_ws;
static int      g_prev_ws;

static int g_pointer_x;
static int g_pointer_y;

/* ========================== UTILS ========================== */

int tfwm_util_write_error(char *err) {
    size_t  n = strlen(err);
    ssize_t out = write(STDERR_FILENO, err, n);
    int     ret = 1;
    if (out < 0) {
        ret = -1;
    }
    return ret;
}

void tfwm_util_fwrite_log(char *log) {
    char *homedir = getenv("HOME");
    if (!homedir) {
        return;
    }

    size_t fpath_len = strlen(homedir) + strlen(TFWM_LOG_FILE) + 2;
    char  *fpath = malloc(fpath_len);
    snprintf(fpath, fpath_len, "%s/%s", homedir, TFWM_LOG_FILE);
    FILE *file = fopen(fpath, "a");
    if (!file) {
        if (errno == ENOENT) {
            file = fopen(fpath, "w");
        }

        if (!file) {
            return;
        }
    }

    size_t     n = strlen(log);
    char       timestamp[20];
    time_t     now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", local_time);
    fprintf(file, "[%s] %s\n", timestamp, log);
    fclose(file);
    free(fpath);
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

char *tfwm_util_get_wm_class(xcb_window_t window) {
    xcb_intern_atom_reply_t *rep = xcb_intern_atom_reply(
        g_conn, xcb_intern_atom(g_conn, 0, strlen("WM_CLASS"), "WM_CLASS"), NULL);
    if (!rep) {
        return NULL;
    }
    xcb_atom_t atom = rep->atom;
    free(rep);

    xcb_get_property_reply_t *prep = xcb_get_property_reply(
        g_conn, xcb_get_property(g_conn, 0, window, atom, XCB_ATOM_STRING, 0, 250),
        NULL);
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
    return wm_class;
}

void tfwm_util_set_cursor(xcb_window_t window, char *name) {
    xcb_cursor_context_t *cursor_ctx;
    if (xcb_cursor_context_new(g_conn, g_screen, &cursor_ctx) < 0) {
        return;
    }
    xcb_cursor_t cursor = xcb_cursor_load_cursor(cursor_ctx, name);
    xcb_change_window_attributes(g_conn, window, XCB_CW_CURSOR, &cursor);
    xcb_cursor_context_free(cursor_ctx);
}

size_t tfwm_util_get_workspaces_len(void) {
    return (sizeof(workspaces) / sizeof(*workspaces));
}

tfwm_workspace_t *tfwm_util_get_workspaces(void) {
    return workspaces;
}

tfwm_workspace_t *tfwm_util_find_workspace(size_t wsid) {
    return &workspaces[wsid];
}

tfwm_workspace_t *tfwm_util_get_current_workspace(void) {
    return &workspaces[g_curr_ws];
}

int tfwm_util_get_current_window_id(void) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    int               wid = -1;
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == g_window) {
            wid = i;
            break;
        }
    }
    return wid;
}

int tfwm_util_check_current_workspace(size_t wsid) {
    return wsid == g_curr_ws;
}

int tfwm_util_check_current_window(xcb_window_t window) {
    return g_window == window;
}

void tfwm_util_redraw_bar(void) {
    xcb_clear_area(g_conn, 1, g_bar, 0, 0, 0, 0);
}

/* ========================= COMMAND ========================= */

void tfwm_exit(char **cmd) {
    if (g_conn) {
        xcb_disconnect(g_conn);
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
    int window_id = -1;
    for (int i = 0; i < workspaces[g_curr_ws].window_len; i++) {
        if (workspaces[g_curr_ws].window_list[i].window == g_window) {
            window_id = i;
            break;
        }
    }
    if (window_id < 0) {
        return;
    }
    xcb_kill_client(g_conn, g_window);
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];

    if (window_id < ws->window_len - 1) {
        for (int i = window_id + 1; i < ws->window_len; i++) {
            ws->window_list[i - 1] = ws->window_list[i];
        }
    }
    ws->window_len -= 1;
    tfwm_util_redraw_bar();

    if (workspaces[g_curr_ws].window_len == 0) {
    } else if ((window_id + 1) >= ws->window_len) {
        tfwm_window_focus(ws->window_list[ws->window_len - 1].window);
    } else if ((window_id + 1) < ws->window_len) {
        tfwm_window_focus(ws->window_list[window_id].window);
    }
    tfwm_layout_update();
}

void tfwm_window_next(char **cmd) {
    int               wid = -1;
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == g_window) {
            wid = i;
            break;
        }
    }
    if (wid < 0) {
        return;
    }

    if ((wid + 1) == ws->window_len) {
        tfwm_window_focus(ws->window_list[0].window);
    } else {
        tfwm_window_focus(ws->window_list[wid + 1].window);
    }
}

void tfwm_window_prev(char **cmd) {
    int               wid = -1;
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == g_window) {
            wid = i;
            break;
        }
    }
    if (wid < 0) {
        return;
    }

    if ((wid - 1) < 0) {
        tfwm_window_focus(ws->window_list[ws->window_len - 1].window);
    } else {
        tfwm_window_focus(ws->window_list[wid - 1].window);
    }
}

void tfwm_window_swap_last(char **cmd) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    if (ws->window_len < 2) {
        return;
    }

    int wid = -1;
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == g_window) {
            wid = i;
            break;
        }
    }
    if (wid < 0) {
        return;
    }

    size_t last_id = ws->window_len - 1;
    if (wid == last_id) {
        return;
    }

    xcb_window_t last = ws->window_list[last_id].window;
    if (g_window == last) {
        return;
    }

    ws->window_list[last_id].window = g_window;
    ws->window_list[wid].window = last;
    if (ws->layout == TFWM_TILING) {
        tfwm_layout_apply_tiling();
    }
    tfwm_window_focus(g_window);
}

void tfwm_window_toggle_fullscreen(char **cmd) {
    if (g_window == 0) {
        return;
    }
    if (g_window == g_screen->root) {
        return;
    }

    tfwm_workspace_t *ws = tfwm_util_get_current_workspace();
    int               wid = tfwm_util_get_current_window_id();
    int               x = 0;
    int               y = 0;
    int               width = g_screen->width_in_pixels;
    int               height = g_screen->height_in_pixels;

    if (ws->window_list[wid].is_fullscreen == 1) {
        x = ws->window_list[wid].x;
        y = ws->window_list[wid].y;
        width = ws->window_list[wid].width;
        height = ws->window_list[wid].height;
    }

    tfwm_window_move(g_window, x, y);
    tfwm_window_resize(g_window, width, height);
}

void tfwm_workspace_switch(char **cmd) {
    size_t wslen = tfwm_util_get_workspaces_len();
    for (size_t i = 0; i < wslen; i++) {
        char *wsname = (char *)cmd[0];
        if ((strcmp(wsname, workspaces[i].name) == 0) && (i != g_curr_ws)) {
            g_prev_ws = g_curr_ws;
            g_curr_ws = i;
        }
    }
    tfwm_workspace_remap();
}

void tfwm_workspace_next(char **cmd) {
    size_t wslen = tfwm_util_get_workspaces_len();
    if ((g_curr_ws + 1) == wslen) {
        g_prev_ws = g_curr_ws;
        g_curr_ws = 0;
    } else {
        g_prev_ws = g_curr_ws;
        g_curr_ws += 1;
    }
    tfwm_workspace_remap();
}

void tfwm_workspace_prev(char **cmd) {
    size_t wslen = tfwm_util_get_workspaces_len();
    if ((g_curr_ws - 1) < 0) {
        g_prev_ws = g_curr_ws;
        g_curr_ws = wslen - 1;
    } else {
        g_prev_ws = g_curr_ws;
        g_curr_ws -= 1;
    }
    tfwm_workspace_remap();
}

void tfwm_workspace_swap_prev(char **cmd) {
    if (g_prev_ws == g_curr_ws) {
        return;
    }

    g_prev_ws = g_prev_ws ^ g_curr_ws;
    g_curr_ws = g_prev_ws ^ g_curr_ws;
    g_prev_ws = g_prev_ws ^ g_curr_ws;
    tfwm_workspace_remap();
}

void tfwm_workspace_use_tiling(char **cmd) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    if (ws->layout == TFWM_TILING) {
        return;
    }
    ws->layout = TFWM_TILING;
    tfwm_layout_apply_tiling();
}

void tfwm_workspace_use_floating(char **cmd) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    if (ws->layout == TFWM_FLOATING) {
        return;
    }
    ws->layout = TFWM_FLOATING;
}

void tfwm_workspace_use_window(char **cmd) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    if (ws->layout == TFWM_WINDOW) {
        return;
    }
    ws->layout = TFWM_WINDOW;
    tfwm_layout_apply_window();
}

/* ===================== WINDOW FUNCTION ===================== */

void tfwm_window_raise(xcb_window_t window) {
    uint32_t vals[1];
    vals[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_conn, window, XCB_CONFIG_WINDOW_STACK_MODE, vals);
}

void tfwm_window_focus(xcb_window_t window) {
    if (window == 0) {
        return;
    }
    if (window == g_screen->root) {
        return;
    }

    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    xcb_set_input_focus(g_conn, XCB_INPUT_FOCUS_POINTER_ROOT, window,
                        XCB_CURRENT_TIME);
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == window) {
            g_window = window;
            break;
        }
    }
}

void tfwm_window_focus_color(xcb_window_t window, int focus) {
    if (TFWM_BORDER_WIDTH <= 0) {
        return;
    }
    if (window == 0) {
        return;
    }
    if (window == g_screen->root) {
        return;
    }

    uint32_t vals[1];
    vals[0] = focus ? TFWM_BORDER_ACTIVE : TFWM_BORDER_INACTIVE;
    xcb_change_window_attributes(g_conn, window, XCB_CW_BORDER_PIXEL, vals);
}

void tfwm_window_move(xcb_window_t window, int x, int y) {
    g_vals[0] = x;
    g_vals[1] = y;
    xcb_configure_window(g_conn, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y,
                         g_vals);
}

void tfwm_window_resize(xcb_window_t window, int width, int height) {
    if ((width < TFWM_MIN_WINDOW_WIDTH) || (height < TFWM_MIN_WINDOW_HEIGHT)) {
        return;
    }

    g_vals[0] = width;
    g_vals[1] = height;
    xcb_configure_window(g_conn, window,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, g_vals);
}

void tfwm_window_set_attr(xcb_window_t window, int x, int y, int width, int height) {
    tfwm_workspace_t *ws = tfwm_util_get_current_workspace();
    int               wid = tfwm_util_get_current_window_id();
    if (wid < 0) {
        return;
    }

    ws->window_list[wid].x = x;
    ws->window_list[wid].y = y;
    ws->window_list[wid].width = width;
    ws->window_list[wid].height = height;
}

/* =================== WORKSPACE FUNCTION ==================== */

void tfwm_workspace_remap(void) {
    if (g_curr_ws == g_prev_ws) {
        return;
    }

    for (int i = 0; i < workspaces[g_prev_ws].window_len; i++) {
        xcb_unmap_window(g_conn, workspaces[g_prev_ws].window_list[i].window);
    }

    for (int i = 0; i < workspaces[g_curr_ws].window_len; i++) {
        xcb_map_window(g_conn, workspaces[g_curr_ws].window_list[i].window);
        if (i == (workspaces[g_curr_ws].window_len - 1)) {
            tfwm_window_focus(workspaces[g_curr_ws].window_list[i].window);
        }
    }
    tfwm_util_redraw_bar();
}

void tfwm_workspace_window_malloc(tfwm_workspace_t *ws) {
    if (ws->window_list) {
        return;
    }

    ws->window_list =
        (tfwm_window_t *)malloc(TFWM_DEFAULT_WS_WIN_ALLOC * sizeof(tfwm_window_t));
    ws->window_cap = TFWM_DEFAULT_WS_WIN_ALLOC;
}

void tfwm_workspace_window_realloc(tfwm_workspace_t *ws) {
    if (!ws->window_list) {
        return;
    }

    tfwm_window_t *new_win = (tfwm_window_t *)realloc(
        ws->window_list,
        (ws->window_cap + TFWM_DEFAULT_WS_WIN_ALLOC) * sizeof(tfwm_window_t));
    if (!new_win) {
        return;
    }

    ws->window_list = new_win;
    ws->window_cap += TFWM_DEFAULT_WS_WIN_ALLOC;
}

void tfwm_workspace_window_append(tfwm_workspace_t *ws, tfwm_window_t window) {
    ws->window_list[ws->window_len++] = window;
}

/* ===================== LAYOUT FUNCTION ===================== */

void tfwm_layout_apply_tiling(void) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    int               master_x = 0;
    int               master_y = TFWM_BAR_HEIGHT;
    int               master_width =
        ((TFWM_TILE_MASTER_RATIO / 100.0) * g_screen->width_in_pixels) -
        (TFWM_BORDER_WIDTH * 2);
    int master_height =
        g_screen->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);

    int slave_x = master_width + TFWM_BORDER_WIDTH;
    int slave_y = master_y;
    int slave_width =
        (((100.0 - TFWM_TILE_MASTER_RATIO) / 100.0) * g_screen->width_in_pixels -
         (TFWM_BORDER_WIDTH * 2));
    int slave_height = master_height;

    if (ws->window_len > 2) {
        slave_height = master_height / (ws->window_len - 1);
    }

    int last_id = ws->window_len - 1;
    for (int i = last_id; i >= 0; i--) {
        xcb_window_t w = ws->window_list[i].window;
        if (i == last_id) {
            tfwm_window_move(w, master_x, master_y);
            tfwm_window_resize(w, master_width, master_height);
        } else {
            tfwm_window_move(w, slave_x, slave_y);
            tfwm_window_resize(w, slave_width, slave_height);
            slave_y += (slave_height + TFWM_BORDER_WIDTH);
        }
    }
}

void tfwm_layout_apply_window(void) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    int               x = 0;
    int               y = TFWM_BAR_HEIGHT;
    int               width = g_screen->width_in_pixels - (TFWM_BORDER_WIDTH * 2);
    int               height =
        g_screen->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);

    int last_id = ws->window_len - 1;
    for (int i = last_id; i >= 0; i--) {
        xcb_window_t w = ws->window_list[i].window;
        tfwm_window_move(w, x, y);
        tfwm_window_resize(w, width, height);
        if (w == g_window) {
            tfwm_window_raise(w);
        }
    }
}

void tfwm_layout_update(void) {
    tfwm_workspace_t *ws = &workspaces[g_curr_ws];
    if (ws->window_len == 0 || ws->layout == TFWM_FLOATING) {
        return;
    } else if (ws->layout == TFWM_WINDOW) {
        tfwm_layout_apply_window();
    } else if (ws->layout == TFWM_TILING) {
        if (ws->window_len == 1) {
            tfwm_layout_apply_window();
            return;
        }

        tfwm_layout_apply_tiling();
    }
}

/* ====================== EVENT HANDLER ====================== */

void tfwm_handle_keypress(xcb_generic_event_t *event) {
    xcb_key_press_event_t *e = (xcb_key_press_event_t *)event;
    xcb_keysym_t           keysym = tfwm_util_get_keysym(e->detail);

    for (int i = 0; i < sizeof(keybinds) / sizeof(*keybinds); i++) {
        if ((keybinds[i].keysym == keysym) && (keybinds[i].mod == e->state)) {
            keybinds[i].func((char **)keybinds[i].cmd);
            xcb_flush(g_conn);
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
    tfwm_window_t win = {e->window};
    tfwm_workspace_window_append(ws, win);
    g_window = e->window;

    xcb_map_window(g_conn, e->window);
    uint32_t vals[5];
    vals[0] = (g_screen->width_in_pixels / 2) - (TFWM_WINDOW_WIDTH / 2);
    vals[1] = (g_screen->height_in_pixels / 2) - (TFWM_WINDOW_HEIGHT / 2);
    vals[2] = TFWM_WINDOW_WIDTH;
    vals[3] = TFWM_WINDOW_HEIGHT;
    vals[4] = TFWM_BORDER_WIDTH;
    xcb_configure_window(g_conn, e->window,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         vals);

    g_vals[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(g_conn, e->window, XCB_CW_EVENT_MASK,
                                         g_vals);
    xcb_flush(g_conn);

    tfwm_window_focus(e->window);
    tfwm_layout_update();
    tfwm_util_redraw_bar();
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
    if (workspaces[g_curr_ws].layout != TFWM_FLOATING) {
        return;
    }

    xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)event;

    xcb_query_pointer_reply_t *point = xcb_query_pointer_reply(
        g_conn, xcb_query_pointer(g_conn, g_screen->root), NULL);
    if (!point) {
        return;
    }

    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply(g_conn, xcb_get_geometry(g_conn, g_window), NULL);
    if (!geo) {
        goto clean_point;
    }

    if (g_vals[2] == (uint32_t)(BUTTON_LEFT)) {
        if ((g_pointer_x == point->root_x) && (g_pointer_y == point->root_y)) {
            return;
        }

        int x = geo->x + point->root_x - g_pointer_x;
        int y = geo->y + point->root_y - g_pointer_y;
        g_pointer_x = point->root_x;
        g_pointer_y = point->root_y;
        tfwm_window_move(g_window, x, y);
    } else if (g_vals[2] == (uint32_t)(BUTTON_RIGHT)) {
        if ((point->root_x <= geo->x) || (point->root_y <= geo->y)) {
            return;
        }

        int width = geo->width + (point->root_x - g_pointer_x);
        int height = geo->height + (point->root_y - g_pointer_y);
        g_pointer_x = point->root_x;
        g_pointer_y = point->root_y;
        tfwm_window_resize(g_window, width, height);
    }

    free(geo);
clean_point:
    free(point);
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
    g_pointer_x = e->event_x;
    g_pointer_y = e->event_y;
    g_vals[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_conn, g_window, XCB_CONFIG_WINDOW_STACK_MODE, g_vals);

    g_vals[2] = ((e->detail == BUTTON_LEFT) ? BUTTON_LEFT
                                            : ((g_window != 0) ? BUTTON_RIGHT : 0));
    xcb_grab_pointer(g_conn, 0, g_screen->root,
                     XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
                         XCB_EVENT_MASK_POINTER_MOTION_HINT,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, g_screen->root,
                     XCB_NONE, XCB_CURRENT_TIME);

    if (g_vals[2] == (uint32_t)BUTTON_LEFT) {
        tfwm_util_set_cursor(g_window, (char *)TFWM_CURSOR_MOVE);
    } else if (g_vals[2] == (uint32_t)BUTTON_RIGHT) {
        tfwm_util_set_cursor(g_window, (char *)TFWM_CURSOR_RESIZE);
    }
}

void tfwm_handle_button_release(xcb_generic_event_t *event) {
    xcb_get_geometry_reply_t *geo =
        xcb_get_geometry_reply(g_conn, xcb_get_geometry(g_conn, g_window), NULL);
    if (!geo) {
        return;
    }

    tfwm_window_set_attr(g_window, geo->x, geo->y, geo->width, geo->height);
    xcb_ungrab_pointer(g_conn, XCB_CURRENT_TIME);
    tfwm_util_set_cursor(g_window, (char *)TFWM_CURSOR_DEFAULT);

    free(geo);
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
        uint8_t e = event->response_type & ~0x80;
        if (e == handler->request) {
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
    tfwm_util_set_cursor(g_screen->root, (char *)TFWM_CURSOR_DEFAULT);
    xcb_flush(g_conn);

    g_bar = xcb_generate_id(g_conn);
    uint32_t bar_vals[3];
    bar_vals[0] = TFWM_BAR_BACKGROUND;
    bar_vals[1] = 1;
    bar_vals[2] = XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(
        g_conn, XCB_COPY_FROM_PARENT, g_bar, g_screen->root, 0, 0,
        g_screen->width_in_pixels, TFWM_BAR_HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
        g_screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, bar_vals);
    uint32_t bar_cfg_vals[1];
    bar_vals[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(g_conn, g_bar, XCB_CONFIG_WINDOW_STACK_MODE, bar_cfg_vals);
    xcb_map_window(g_conn, g_bar);
    xcb_flush(g_conn);
}

int main(int argc, char *argv[]) {
    int ret = 0;
    if ((argc == 2) && (strcmp("-v", argv[1]) == 0)) {
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
            tfwm_bar(g_conn, g_screen, g_bar);
        }
    }

    return ret;
}
