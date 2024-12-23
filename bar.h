#ifndef BAR_H
#define BAR_H

#include <xcb/xcb.h>

static int tfwm_bar_text_width(xcb_connection_t *conn, xcb_font_t f, char *t);
static char *tfwm_bar_layout_str(int cur_ws);
static char *tfwm_bar_workspace_str(int cur_ws);
void tfwm_bar(xcb_connection_t *conn, xcb_screen_t *scrn, int cur_ws);

#endif // !BAR_H
