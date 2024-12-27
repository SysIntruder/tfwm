#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <xcb/xcb_cursor.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xproto.h>

#include "config.h"
#include "util.h"

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

void tfwm_util_fwrite_log(char *log) {
    char *homedir = getenv("HOME");
    if (!homedir) {
        return;
    }

    size_t fpath_len = strlen(homedir) + strlen(LOG_FILE) + 2;
    char  *fpath = malloc(fpath_len);
    snprintf(fpath, fpath_len, "%s/%s", homedir, LOG_FILE);
    FILE *file = fopen(fpath, "a");
    if (!file) {
        if (errno == ENOENT) {
            file = fopen(fpath, "w");
        }

        if (!file) {
            return;
        }
    }

    size_t n = 0;
    char  *l = log;
    while ((*(l++)) != 0) {
        ++n;
    }

    char       timestamp[20];
    time_t     now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(timestamp, 20, "%Y-%m-%d %H:%M:%S", local_time);

    fprintf(file, "[%s] %s\n", timestamp, log);
    fclose(file);
    free(fpath);
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

xcb_keycode_t *tfwm_util_get_keycodes(xcb_connection_t *conn, xcb_keysym_t keysym) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(conn);
    xcb_keycode_t     *keycode =
        (!(ks) ? NULL : xcb_key_symbols_get_keycode(ks, keysym));
    xcb_key_symbols_free(ks);
    return keycode;
}

xcb_keysym_t tfwm_util_get_keysym(xcb_connection_t *conn, xcb_keycode_t keycode) {
    xcb_key_symbols_t *ks = xcb_key_symbols_alloc(conn);
    xcb_keysym_t keysym = (!(ks) ? 0 : xcb_key_symbols_get_keysym(ks, keycode, 0));
    xcb_key_symbols_free(ks);
    return keysym;
}

char *tfwm_util_get_wm_class(xcb_connection_t *conn, xcb_window_t window) {
    xcb_intern_atom_reply_t *rep = xcb_intern_atom_reply(
        conn, xcb_intern_atom(conn, 0, strlen("WM_CLASS"), "WM_CLASS"), NULL);
    if (!rep) {
        return NULL;
    }
    xcb_atom_t atom = rep->atom;
    free(rep);

    xcb_get_property_reply_t *prep = xcb_get_property_reply(
        conn, xcb_get_property(conn, 0, window, atom, XCB_ATOM_STRING, 0, 250),
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

void tfwm_util_change_cursor(xcb_connection_t *conn, xcb_screen_t *screen,
                             xcb_window_t window, char *name) {
    xcb_cursor_context_t *cursor_ctx;
    if (xcb_cursor_context_new(conn, screen, &cursor_ctx) < 0) {
        return;
    }
    xcb_cursor_t cursor = xcb_cursor_load_cursor(cursor_ctx, name);
    xcb_change_window_attributes(conn, window, XCB_CW_CURSOR, &cursor);
    xcb_cursor_context_free(cursor_ctx);
}
