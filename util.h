#ifndef UTIL_H
#define UTIL_H

#include <xcb/xproto.h>

int            tfwm_util_write_error(char *err);
void           tfwm_util_fwrite_log(char *log);
int            tfwm_util_compare_str(char *str1, char *str2);
xcb_keycode_t *tfwm_util_get_keycodes(xcb_connection_t *conn, xcb_keysym_t keysym);
xcb_keysym_t   tfwm_util_get_keysym(xcb_connection_t *conn, xcb_keycode_t keycode);
char          *tfwm_util_get_wm_class(xcb_connection_t *conn, xcb_window_t window);
void           tfwm_util_change_cursor(xcb_connection_t *conn, xcb_screen_t *screen,
                                       xcb_window_t window, char *name);

#endif  // !UTIL_H
