#ifndef BAR_H
#define BAR_H

#include <xcb/xcb.h>
#include "tfwm.h"

static int tfwm_bar_text_width(xcb_connection_t *conn, xcb_font_t font, char *text);
static char  *tfwm_bar_layout_str(tfwm_workspace_t *ws);
static char **tfwm_bar_workspace_str_list(tfwm_workspace_t *ws, size_t ws_len);
static char **tfwm_bar_window_str_list(tfwm_workspace_t *ws);
void          tfwm_left_bar(xcb_connection_t *conn, xcb_screen_t *screen,
                            xcb_window_t window);

#endif  // !BAR_H
