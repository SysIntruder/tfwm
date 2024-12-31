#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "bar.h"
#include "config.h"
#include "tfwm.h"

static tfwm_xcb_t core;

/* ========================== UTILS ========================== */

void tfwm_util_log(char *log, int exit) {
    char *homedir = getenv("HOME");
    if (!homedir) {
        return;
    }

    size_t fpath_len = strlen(homedir) + strlen(TFWM_LOG_FILE) + 2;
    char *fpath = malloc(fpath_len);
    snprintf(fpath, fpath_len, "%s/%s", homedir, TFWM_LOG_FILE);
    FILE *file;
    if (access(fpath, F_OK) != 0) {
        file = fopen(fpath, "w");
    } else {
        file = fopen(fpath, "a");
    }
    if (!file) {
        printf("ERROR: can not open log file\n");
        return;
    }
    free(fpath);

    size_t n = strlen(log);
    char timestamp[20];
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", local_time);
    fprintf(file, "[%s] %s\n", timestamp, log);
    fclose(file);
    core.exit = exit;
}

xcb_keycode_t *tfwm_util_get_keycodes(xcb_keysym_t keysym) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(core.conn);
    xcb_keycode_t *keycode =
        (!(ks) ? NULL : xcb_key_symbols_get_keycode(ks, keysym));
    xcb_key_symbols_free(ks);
    return keycode;
}

xcb_keysym_t tfwm_util_get_keysym(xcb_keycode_t keycode) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(core.conn);
    xcb_keysym_t keysym = (!(ks) ? 0 : xcb_key_symbols_get_keysym(ks, keycode, 0));
    xcb_key_symbols_free(ks);
    return keysym;
}

char *tfwm_util_get_wm_class(xcb_window_t window) {
    xcb_intern_atom_reply_t *rep = xcb_intern_atom_reply(
        core.conn, xcb_intern_atom(core.conn, 0, strlen("WM_CLASS"), "WM_CLASS"),
        NULL);
    if (!rep) {
        return NULL;
    }
    xcb_atom_t atom = rep->atom;
    free(rep);

    xcb_get_property_reply_t *prep = xcb_get_property_reply(
        core.conn,
        xcb_get_property(core.conn, 0, window, atom, XCB_ATOM_STRING, 0, 250), NULL);
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
    if (xcb_cursor_context_new(core.conn, core.screen, &cursor_ctx) < 0) {
        return;
    }
    xcb_cursor_t cursor = xcb_cursor_load_cursor(cursor_ctx, name);
    xcb_change_window_attributes(core.conn, window, XCB_CW_CURSOR, &cursor);
    xcb_cursor_context_free(cursor_ctx);
}

size_t tfwm_util_get_workspaces_len(void) {
    return core.workspace_len;
}

tfwm_workspace_t *tfwm_util_get_workspaces(void) {
    return core.workspace_list;
}

tfwm_workspace_t *tfwm_util_find_workspace(size_t wsid) {
    return &core.workspace_list[wsid];
}

tfwm_workspace_t *tfwm_util_get_current_workspace(void) {
    return &core.workspace_list[core.curr_workspace];
}

int tfwm_util_get_current_window_id(void) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    int wid = -1;
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == core.window) {
            wid = i;
            break;
        }
    }
    return wid;
}

int tfwm_util_check_current_workspace(size_t wsid) {
    return wsid == core.curr_workspace;
}

int tfwm_util_check_current_window(xcb_window_t window) {
    return core.window == window;
}

void tfwm_util_redraw_bar(void) {
    xcb_clear_area(core.conn, 1, core.bar, 0, 0, 0, 0);
}

/* ========================= COMMAND ========================= */

void tfwm_exit(char **cmd) {
    if (core.conn) {
        xcb_disconnect(core.conn);

        for (size_t i = 0; i < core.workspace_len; i++) {
            free(core.workspace_list[i].window_list);
        }
        free(core.workspace_list);
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
    for (int i = 0; i < core.workspace_list[core.curr_workspace].window_len; i++) {
        if (core.workspace_list[core.curr_workspace].window_list[i].window ==
            core.window) {
            window_id = i;
            break;
        }
    }
    if (window_id < 0) {
        return;
    }

    xcb_window_t target = core.window;
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (window_id < ws->window_len - 1) {
        for (int i = window_id + 1; i < ws->window_len; i++) {
            ws->window_list[i - 1] = ws->window_list[i];
        }
    }
    ws->window_len -= 1;

    if (core.workspace_list[core.curr_workspace].window_len == 0) {
    } else if ((window_id + 1) >= ws->window_len) {
        tfwm_window_focus(ws->window_list[ws->window_len - 1].window);
    } else if ((window_id + 1) < ws->window_len) {
        tfwm_window_focus(ws->window_list[window_id].window);
    }
    tfwm_layout_update();
    tfwm_util_redraw_bar();
    xcb_flush(core.conn);

    xcb_kill_client(core.conn, target);
}

void tfwm_window_next(char **cmd) {
    int wid = -1;
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == core.window) {
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
    int wid = -1;
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == core.window) {
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
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (ws->window_len < 2) {
        return;
    }

    int wid = -1;
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == core.window) {
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
    if (core.window == last) {
        return;
    }

    ws->window_list[last_id].window = core.window;
    ws->window_list[wid].window = last;
    if (ws->layout == TFWM_LAYOUT_TILING) {
        tfwm_layout_apply_tiling();
    }
    tfwm_window_focus(core.window);
}

void tfwm_window_toggle_fullscreen(char **cmd) {
    if (core.window == 0) {
        return;
    }
    if (core.window == core.screen->root) {
        return;
    }

    tfwm_workspace_t *ws = tfwm_util_get_current_workspace();
    tfwm_window_t *w = &ws->window_list[tfwm_util_get_current_window_id()];
    int x = 0 - TFWM_BORDER_WIDTH;
    int y = 0 - TFWM_BORDER_WIDTH;
    int width = core.screen->width_in_pixels;
    int height = core.screen->height_in_pixels;

    if (w->is_fullscreen == 1) {
        x = w->x;
        y = w->y;
        width = w->width;
        height = w->height;
    }

    tfwm_window_move(core.window, x, y);
    tfwm_window_resize(core.window, width, height);
    w->is_fullscreen ^= 1;
}

void tfwm_workspace_switch(char **cmd) {
    size_t wslen = tfwm_util_get_workspaces_len();
    for (size_t i = 0; i < wslen; i++) {
        char *wsname = (char *)cmd[0];
        if ((strcmp(wsname, core.workspace_list[i].name) == 0) &&
            (i != core.curr_workspace)) {
            core.prev_workspace = core.curr_workspace;
            core.curr_workspace = i;
        }
    }
    tfwm_workspace_remap();
}

void tfwm_workspace_next(char **cmd) {
    size_t wslen = tfwm_util_get_workspaces_len();
    if ((core.curr_workspace + 1) == wslen) {
        core.prev_workspace = core.curr_workspace;
        core.curr_workspace = 0;
    } else {
        core.prev_workspace = core.curr_workspace;
        core.curr_workspace += 1;
    }
    tfwm_workspace_remap();
}

void tfwm_workspace_prev(char **cmd) {
    size_t wslen = tfwm_util_get_workspaces_len();
    if ((core.curr_workspace - 1) < 0) {
        core.prev_workspace = core.curr_workspace;
        core.curr_workspace = wslen - 1;
    } else {
        core.prev_workspace = core.curr_workspace;
        core.curr_workspace -= 1;
    }
    tfwm_workspace_remap();
}

void tfwm_workspace_swap_prev(char **cmd) {
    if (core.prev_workspace == core.curr_workspace) {
        return;
    }

    core.prev_workspace = core.prev_workspace ^ core.curr_workspace;
    core.curr_workspace = core.prev_workspace ^ core.curr_workspace;
    core.prev_workspace = core.prev_workspace ^ core.curr_workspace;
    tfwm_workspace_remap();
}

void tfwm_workspace_use_tiling(char **cmd) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (ws->layout == TFWM_LAYOUT_TILING) {
        return;
    }
    ws->layout = TFWM_LAYOUT_TILING;
    tfwm_layout_apply_tiling();
}

void tfwm_workspace_use_floating(char **cmd) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (ws->layout == TFWM_LAYOUT_FLOATING) {
        return;
    }
    ws->layout = TFWM_LAYOUT_FLOATING;
}

void tfwm_workspace_use_window(char **cmd) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (ws->layout == TFWM_LAYOUT_WINDOW) {
        return;
    }
    ws->layout = TFWM_LAYOUT_WINDOW;
    tfwm_layout_apply_window();
}

/* ===================== WINDOW FUNCTION ===================== */

void tfwm_window_raise(xcb_window_t window) {
    uint32_t vals[1];
    vals[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(core.conn, window, XCB_CONFIG_WINDOW_STACK_MODE, vals);
}

void tfwm_window_focus(xcb_window_t window) {
    if (window == 0) {
        return;
    }
    if (window == core.screen->root) {
        return;
    }

    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    xcb_set_input_focus(core.conn, XCB_INPUT_FOCUS_POINTER_ROOT, window,
                        XCB_CURRENT_TIME);
    for (int i = 0; i < ws->window_len; i++) {
        if (ws->window_list[i].window == window) {
            core.window = window;
            break;
        }
    }
    tfwm_window_raise(core.window);
}

void tfwm_window_focus_color(xcb_window_t window, int focus) {
    if (TFWM_BORDER_WIDTH <= 0) {
        return;
    }
    if (window == 0) {
        return;
    }
    if (window == core.screen->root) {
        return;
    }

    uint32_t vals[1];
    vals[0] = focus ? TFWM_BORDER_ACTIVE : TFWM_BORDER_INACTIVE;
    xcb_change_window_attributes(core.conn, window, XCB_CW_BORDER_PIXEL, vals);
}

void tfwm_window_move(xcb_window_t window, int x, int y) {
    core.vals[0] = x;
    core.vals[1] = y;
    xcb_configure_window(core.conn, window,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, core.vals);
}

void tfwm_window_resize(xcb_window_t window, int width, int height) {
    if ((width < TFWM_MIN_WINDOW_WIDTH) || (height < TFWM_MIN_WINDOW_HEIGHT)) {
        return;
    }

    core.vals[0] = width;
    core.vals[1] = height;
    xcb_configure_window(core.conn, window,
                         XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT,
                         core.vals);
}

void tfwm_window_set_attr(xcb_window_t window, int x, int y, int width, int height) {
    tfwm_workspace_t *ws = tfwm_util_get_current_workspace();
    int wid = tfwm_util_get_current_window_id();
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
    if (core.curr_workspace == core.prev_workspace) {
        return;
    }

    for (int i = 0; i < core.workspace_list[core.prev_workspace].window_len; i++) {
        xcb_unmap_window(
            core.conn,
            core.workspace_list[core.prev_workspace].window_list[i].window);
    }

    for (int i = 0; i < core.workspace_list[core.curr_workspace].window_len; i++) {
        xcb_map_window(
            core.conn,
            core.workspace_list[core.curr_workspace].window_list[i].window);
        if (i == (core.workspace_list[core.curr_workspace].window_len - 1)) {
            tfwm_window_focus(
                core.workspace_list[core.curr_workspace].window_list[i].window);
        }
    }
    tfwm_util_redraw_bar();
}

void tfwm_workspace_window_malloc() {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (ws->window_list) {
        return;
    }

    ws->window_list =
        (tfwm_window_t *)malloc(TFWM_DEFAULT_WS_WIN_ALLOC * sizeof(tfwm_window_t));
    ws->window_cap = TFWM_DEFAULT_WS_WIN_ALLOC;
}

void tfwm_workspace_window_realloc() {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
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

void tfwm_workspace_window_append(tfwm_window_t window) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];

    if (!ws->window_list) {
        tfwm_workspace_window_malloc();
    } else if (ws->window_cap == ws->window_len) {
        tfwm_workspace_window_realloc();
    }

    ws->window_list[ws->window_len++] = window;
}

/* ===================== LAYOUT FUNCTION ===================== */

void tfwm_layout_apply_tiling(void) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    int master_x = 0;
    int master_y = TFWM_BAR_HEIGHT;
    int master_width =
        ((TFWM_TILE_MASTER_RATIO / 100.0) * core.screen->width_in_pixels) -
        (TFWM_BORDER_WIDTH * 2);
    int master_height =
        core.screen->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);

    int slave_x = master_width + (TFWM_BORDER_WIDTH * 2);
    int slave_y = master_y;
    int slave_width =
        (((100.0 - TFWM_TILE_MASTER_RATIO) / 100.0) * core.screen->width_in_pixels -
         (TFWM_BORDER_WIDTH * 2));
    int slave_height = master_height;

    if (ws->window_len > 2) {
        slave_height = ((core.screen->height_in_pixels - TFWM_BAR_HEIGHT) /
                        (ws->window_len - 1)) -
                       (TFWM_BORDER_WIDTH * 2);
    }
    int last_id = ws->window_len - 1;

    for (int i = last_id; i >= 0; i--) {
        tfwm_window_t *w = &ws->window_list[i];
        if (i == last_id) {
            w->x = master_x;
            w->y = master_y;
            w->width = master_width;
            w->height = master_height;
            tfwm_window_move(w->window, master_x, master_y);
            tfwm_window_resize(w->window, master_width, master_height);
            tfwm_window_focus_color(w->window, 1);
        } else {
            w->x = slave_x;
            w->y = slave_y;
            w->width = slave_width;
            w->height = slave_height;
            tfwm_window_move(w->window, slave_x, slave_y);
            tfwm_window_resize(w->window, slave_width, slave_height);
            tfwm_window_focus_color(w->window, 0);
            slave_y += (slave_height + (TFWM_BORDER_WIDTH * 2));
        }
    }
}

void tfwm_layout_apply_window(void) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    int x = 0;
    int y = TFWM_BAR_HEIGHT;
    int width = core.screen->width_in_pixels - (TFWM_BORDER_WIDTH * 2);
    int height =
        core.screen->height_in_pixels - TFWM_BAR_HEIGHT - (TFWM_BORDER_WIDTH * 2);
    int last_id = ws->window_len - 1;

    for (int i = last_id; i >= 0; i--) {
        tfwm_window_t *w = &ws->window_list[i];
        w->x = x;
        w->y = y;
        w->width = width;
        w->height = height;
        tfwm_window_move(w->window, x, y);
        tfwm_window_resize(w->window, width, height);
        if (w->window == core.window) {
            tfwm_window_raise(w->window);
        }
    }
}

void tfwm_layout_update(void) {
    tfwm_workspace_t *ws = &core.workspace_list[core.curr_workspace];
    if (ws->window_len == 0 || ws->layout == TFWM_LAYOUT_FLOATING) {
        return;
    } else if (ws->layout == TFWM_LAYOUT_WINDOW) {
        tfwm_layout_apply_window();
    } else if (ws->layout == TFWM_LAYOUT_TILING) {
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
    xcb_keysym_t keysym = tfwm_util_get_keysym(e->detail);

    for (int i = 0; i < sizeof(cfg_keybinds) / sizeof(*cfg_keybinds); i++) {
        if ((cfg_keybinds[i].keysym == keysym) &&
            (cfg_keybinds[i].mod == e->state)) {
            cfg_keybinds[i].func((char **)cfg_keybinds[i].cmd);
            xcb_flush(core.conn);
        }
    }
}

void tfwm_handle_map_request(xcb_generic_event_t *event) {
    xcb_map_request_event_t *e = (xcb_map_request_event_t *)event;

    uint32_t vals[5];
    vals[0] = (core.screen->width_in_pixels / 2) - (TFWM_WINDOW_WIDTH / 2);
    vals[1] = (core.screen->height_in_pixels / 2) - (TFWM_WINDOW_HEIGHT / 2);
    vals[2] = TFWM_WINDOW_WIDTH;
    vals[3] = TFWM_WINDOW_HEIGHT;
    vals[4] = TFWM_BORDER_WIDTH;
    xcb_configure_window(core.conn, e->window,
                         XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
                             XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
                             XCB_CONFIG_WINDOW_BORDER_WIDTH,
                         vals);
    tfwm_window_t w = {e->window, vals[0], vals[1], vals[2], vals[3], vals[4]};
    tfwm_workspace_window_append(w);
    xcb_map_window(core.conn, e->window);
    xcb_flush(core.conn);

    core.vals[0] = XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_FOCUS_CHANGE;
    xcb_change_window_attributes_checked(core.conn, e->window, XCB_CW_EVENT_MASK,
                                         core.vals);

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

void tfwm_handle_enter_notify(xcb_generic_event_t *event) {
    xcb_enter_notify_event_t *e = (xcb_enter_notify_event_t *)event;
}

void tfwm_handle_leave_notify(xcb_generic_event_t *event) {
    xcb_leave_notify_event_t *e = (xcb_leave_notify_event_t *)event;
}

void tfwm_handle_motion_notify(xcb_generic_event_t *event) {
    if (core.window == 0) {
        return;
    }
    if (core.workspace_list[core.curr_workspace].layout != TFWM_LAYOUT_FLOATING) {
        return;
    }

    xcb_motion_notify_event_t *e = (xcb_motion_notify_event_t *)event;

    xcb_query_pointer_reply_t *point = xcb_query_pointer_reply(
        core.conn, xcb_query_pointer(core.conn, core.screen->root), NULL);
    if (!point) {
        return;
    }

    xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply(
        core.conn, xcb_get_geometry(core.conn, core.window), NULL);
    if (!geo) {
        goto clean_point;
    }

    if (core.vals[2] == (uint32_t)(BUTTON_LEFT)) {
        if ((core.pointer_x == point->root_x) && (core.pointer_y == point->root_y)) {
            return;
        }

        int x = geo->x + point->root_x - core.pointer_x;
        int y = geo->y + point->root_y - core.pointer_y;
        core.pointer_x = point->root_x;
        core.pointer_y = point->root_y;
        tfwm_window_move(core.window, x, y);
    } else if (core.vals[2] == (uint32_t)(BUTTON_RIGHT)) {
        if ((point->root_x <= geo->x) || (point->root_y <= geo->y)) {
            return;
        }

        int width = geo->width + (point->root_x - core.pointer_x);
        int height = geo->height + (point->root_y - core.pointer_y);
        core.pointer_x = point->root_x;
        core.pointer_y = point->root_y;
        tfwm_window_resize(core.window, width, height);
    }

    free(geo);
clean_point:
    free(point);
}

void tfwm_handle_destroy_notify(xcb_generic_event_t *event) {
    xcb_destroy_notify_event_t *e = (xcb_destroy_notify_event_t *)event;
    xcb_kill_client(core.conn, e->window);
}

void tfwm_handle_button_press(xcb_generic_event_t *event) {
    xcb_button_press_event_t *e = (xcb_button_press_event_t *)event;
    core.window = e->child;
    core.pointer_x = e->event_x;
    core.pointer_y = e->event_y;
    tfwm_window_focus(core.window);

    core.vals[2] =
        ((e->detail == BUTTON_LEFT) ? BUTTON_LEFT
                                    : ((core.window != 0) ? BUTTON_RIGHT : 0));
    xcb_grab_pointer(core.conn, 0, core.screen->root,
                     XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_BUTTON_MOTION |
                         XCB_EVENT_MASK_POINTER_MOTION_HINT,
                     XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, core.screen->root,
                     XCB_NONE, XCB_CURRENT_TIME);

    if (core.vals[2] == (uint32_t)BUTTON_LEFT) {
        tfwm_util_set_cursor(core.window, (char *)TFWM_CURSOR_MOVE);
    } else if (core.vals[2] == (uint32_t)BUTTON_RIGHT) {
        tfwm_util_set_cursor(core.window, (char *)TFWM_CURSOR_RESIZE);
    }
}

void tfwm_handle_button_release(xcb_generic_event_t *event) {
    xcb_get_geometry_reply_t *geo = xcb_get_geometry_reply(
        core.conn, xcb_get_geometry(core.conn, core.window), NULL);
    if (!geo) {
        return;
    }

    tfwm_window_set_attr(core.window, geo->x, geo->y, geo->width, geo->height);
    xcb_ungrab_pointer(core.conn, XCB_CURRENT_TIME);
    tfwm_util_set_cursor(core.window, (char *)TFWM_CURSOR_DEFAULT);

    free(geo);
}

int tfwm_handle_event(void) {
    int ret = xcb_connection_has_error(core.conn);
    if (ret != 0) {
        return ret;
    }

    xcb_generic_event_t *event = xcb_wait_for_event(core.conn);
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
    xcb_flush(core.conn);
    return ret;
}

/* ========================== SETUP ========================== */

static void tfwm_init(void) {
    core.vals[0] =
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
    xcb_change_window_attributes_checked(core.conn, core.screen->root,
                                         XCB_CW_EVENT_MASK, core.vals);

    xcb_ungrab_key(core.conn, XCB_GRAB_ANY, core.screen->root, XCB_MOD_MASK_ANY);
    for (int i = 0; i < sizeof(cfg_keybinds) / sizeof(*cfg_keybinds); i++) {
        xcb_keycode_t *keycode = tfwm_util_get_keycodes(cfg_keybinds[i].keysym);
        if (keycode != NULL) {
            xcb_grab_key(core.conn, 1, core.screen->root, cfg_keybinds[i].mod,
                         *keycode, XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        }
    }
    xcb_flush(core.conn);

    xcb_grab_button(core.conn, 0, core.screen->root,
                    XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, core.screen->root,
                    XCB_NONE, BUTTON_LEFT, MOD_KEY);
    xcb_grab_button(core.conn, 0, core.screen->root,
                    XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE,
                    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, core.screen->root,
                    XCB_NONE, BUTTON_RIGHT, MOD_KEY);
    tfwm_util_set_cursor(core.screen->root, (char *)TFWM_CURSOR_DEFAULT);
    xcb_flush(core.conn);

    core.workspace_len = sizeof(cfg_workspace_list) / sizeof(*cfg_workspace_list);
    core.workspace_list = malloc(core.workspace_len * sizeof(tfwm_workspace_t));
    for (size_t i = 0; i < core.workspace_len; i++) {
        core.workspace_list[i] = (tfwm_workspace_t){cfg_active_layout[0].layout,
                                                    (char *)cfg_workspace_list[i]};
    }

    core.bar = xcb_generate_id(core.conn);
    uint32_t bar_vals[3];
    bar_vals[0] = TFWM_BAR_BG;
    bar_vals[1] = 1;
    bar_vals[2] = XCB_EVENT_MASK_EXPOSURE;
    xcb_create_window(
        core.conn, XCB_COPY_FROM_PARENT, core.bar, core.screen->root, 0, 0,
        core.screen->width_in_pixels, TFWM_BAR_HEIGHT, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT, core.screen->root_visual,
        XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK, bar_vals);
    uint32_t bar_cfg_vals[1];
    bar_vals[0] = XCB_STACK_MODE_ABOVE;
    xcb_configure_window(core.conn, core.bar, XCB_CONFIG_WINDOW_STACK_MODE,
                         bar_cfg_vals);
    xcb_map_window(core.conn, core.bar);
    xcb_flush(core.conn);
}

int main(int argc, char *argv[]) {
    core = (tfwm_xcb_t){};

    if ((argc == 2) && (strcmp("-v", argv[1]) == 0)) {
        printf("tfwm-0.0.1, Copyright (c) 2024 Raihan Rahardyan, MIT License\n");
        return EXIT_SUCCESS;
    }

    if (argc != 1) {
        printf("usage: tfwm [-v]\n");
        return EXIT_SUCCESS;
    }

    core.conn = xcb_connect(NULL, NULL);
    core.exit = xcb_connection_has_error(core.conn);
    if (core.exit > 0) {
        printf("xcb_connection_has_error\n");
        return core.exit;
    }

    core.screen = xcb_setup_roots_iterator(xcb_get_setup(core.conn)).data;
    tfwm_init();

    while (core.exit == EXIT_SUCCESS) {
        core.exit = tfwm_handle_event();

        if (core.screen) {
            tfwm_bar(core.conn, core.screen, core.bar);
        }
    }

    return core.exit;
}
