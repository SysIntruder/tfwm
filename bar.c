#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#include "bar.h"
#include "config.h"

static int tfwm_bar_text_width(xcb_connection_t *conn, xcb_font_t font, char *txt) {
    size_t        txt_len = strlen(txt);
    xcb_char2b_t *bt = malloc(txt_len * sizeof(xcb_char2b_t));
    for (int i = 0; i < txt_len; i++) {
        bt[i].byte1 = 0;
        bt[i].byte2 = txt[i];
    }

    xcb_query_text_extents_cookie_t c =
        xcb_query_text_extents(conn, font, txt_len, bt);
    xcb_query_text_extents_reply_t *rep = xcb_query_text_extents_reply(conn, c, 0);
    if (!rep) {
        return 0;
    }

    int width = rep->overall_width;
    free(rep);
    return width;
}

static char *tfwm_bar_layout_str(int cur_ws) {
    uint16_t layout = workspaces[cur_ws].default_layout;
    char    *res = malloc(0);
    char    *tmp;
    size_t   len;

    switch (layout) {
        case (TILING):
            len = strlen(LAYOUT_TILING_DISPLAY);
            tmp = realloc(res, (len + 1));
            res = tmp;
            memcpy(res, LAYOUT_TILING_DISPLAY, len);
            break;
        case (FLOATING):
            len = strlen(LAYOUT_FLOATING_DISPLAY);
            tmp = realloc(res, (len + 1));
            res = tmp;
            memcpy(res, LAYOUT_FLOATING_DISPLAY, len);
            break;
        case (WINDOW):
            len = strlen(LAYOUT_WINDOW_DISPLAY);
            tmp = realloc(res, (len + 1));
            res = tmp;
            memcpy(res, LAYOUT_WINDOW_DISPLAY, len);
            break;
    }
    res[len] = '\0';

    return res;
}

static char **tfwm_bar_workspace_str_list(int cur_ws) {
    size_t ws_len = (sizeof(workspaces) / sizeof(*workspaces));
    char **res = malloc(ws_len * sizeof(char *));

    for (size_t i = 0; i < ws_len; i++) {
        size_t len = strlen(workspaces[i].name);
        res[i] = malloc(len + 3);
        res[i][0] = ' ';
        memcpy(res[i] + 1, workspaces[i].name, len);
        res[i][len + 1] = ' ';
        res[i][len + 2] = '\0';
    }

    return res;
}

void tfwm_left_bar(xcb_connection_t *conn, xcb_screen_t *scrn, int cur_ws) {
    xcb_font_t font = xcb_generate_id(conn);
    xcb_open_font(conn, font, strlen(BAR_FONT_NAME), BAR_FONT_NAME);

    xcb_gcontext_t active_gc = xcb_generate_id(conn);
    uint32_t       active_vals[3];
    active_vals[0] = BAR_FOREGROUND_ACTIVE;
    active_vals[1] = BAR_BACKGROUND_ACTIVE;
    active_vals[2] = font;
    xcb_create_gc(conn, active_gc, scrn->root,
                  XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT, active_vals);

    xcb_gcontext_t inactive_gc = xcb_generate_id(conn);
    uint32_t       inactive_vals[3];
    inactive_vals[0] = BAR_FOREGROUND;
    inactive_vals[1] = BAR_BACKGROUND;
    inactive_vals[2] = font;
    xcb_create_gc(conn, inactive_gc, scrn->root,
                  XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT,
                  inactive_vals);

    int   pos_x = 0;
    char *sep = BAR_SEPARATOR;

    char *layout = tfwm_bar_layout_str(cur_ws);
    xcb_image_text_8(conn, strlen(layout), scrn->root, inactive_gc, pos_x,
                     BAR_HEIGHT, layout);
    pos_x += tfwm_bar_text_width(conn, font, layout);
    free(layout);

    xcb_image_text_8(conn, strlen(sep), scrn->root, inactive_gc, pos_x, BAR_HEIGHT,
                     sep);
    pos_x += tfwm_bar_text_width(conn, font, sep);

    char **ws_list = tfwm_bar_workspace_str_list(cur_ws);
    size_t ws_len = (sizeof(workspaces) / sizeof(*workspaces));
    for (size_t i = 0; i < ws_len; i++) {
        if (cur_ws == i) {
            xcb_image_text_8(conn, strlen(ws_list[i]), scrn->root, active_gc, pos_x,
                             BAR_HEIGHT, ws_list[i]);
        } else {
            xcb_image_text_8(conn, strlen(ws_list[i]), scrn->root, inactive_gc,
                             pos_x, BAR_HEIGHT, ws_list[i]);
        }
        pos_x += tfwm_bar_text_width(conn, font, ws_list[i]);
        free(ws_list[i]);
    }
    free(ws_list);

    xcb_close_font(conn, font);
    xcb_free_gc(conn, active_gc);
    xcb_free_gc(conn, inactive_gc);
    xcb_flush(conn);
}
