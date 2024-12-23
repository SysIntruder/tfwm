#ifndef BAR_H
#define BAR_H

#include <xcb/xcb.h>

static char *tfwm_bar_workspace_str(int cur_ws);
void tfwm_bar(xcb_connection_t *conn, xcb_screen_t *scrn, int cur_ws);

#endif // !BAR_H
