#ifndef BAR_H
#define BAR_H

#include <xcb/xcb.h>

static int tfwm_bar_text_width(xcb_connection_t *conn, xcb_font_t font, char *text);
static char  *tfwm_bar_layout_str(int curr_ws);
static char **tfwm_bar_workspace_str_list(void);
void tfwm_left_bar(xcb_connection_t *conn, xcb_screen_t *screen, int curr_ws);

#endif  // !BAR_H
