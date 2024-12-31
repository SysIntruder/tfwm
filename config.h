#ifndef CONFIG_H
#define CONFIG_H

#include "tfwm.h"

/* ========================= MOD KEY ========================= */

#define MOD_KEY   XCB_MOD_MASK_4
#define MOD_SHIFT XCB_MOD_MASK_SHIFT
#define MOD_CTRL  XCB_MOD_MASK_SHIFT

#define BUTTON_LEFT  1
#define BUTTON_RIGHT 3

/* ========================== CURSOR ========================= */

static const char *TFWM_CURSOR_DEFAULT = "left_ptr";
static const char *TFWM_CURSOR_MOVE = "fleur";
static const char *TFWM_CURSOR_RESIZE = "bottom_right_corner";

/* ========================== COLORS ========================= */

static const uint32_t BLACK = 0x000000;
static const uint32_t GRAY = 0x696969;
static const uint32_t WHITE = 0xFFFFFF;
static const uint32_t RED = 0xFF0000;
static const uint32_t GREEN = 0x00FF00;
static const uint32_t BLUE = 0x0000FF;

/* ========================== WINDOW ========================= */

static const uint32_t TFWM_WINDOW_WIDTH = 600;
static const uint32_t TFWM_WINDOW_HEIGHT = 400;
static const uint32_t TFWM_MIN_WINDOW_WIDTH = 60;
static const uint32_t TFWM_MIN_WINDOW_HEIGHT = 40;

static const uint32_t TFWM_BORDER_WIDTH = 1;
static const uint32_t TFWM_BORDER_ACTIVE = WHITE;
static const uint32_t TFWM_BORDER_INACTIVE = GRAY;

static const double TFWM_TILE_MASTER_RATIO = 50.0;

/* =========================== BAR =========================== */

static const char *TFWM_FONT = "fixed";
static const int   TFWM_FONT_HEIGHT = 13;

static const int      TFWM_BAR_HEIGHT = TFWM_FONT_HEIGHT + 2;
static const char    *TFWM_BAR_SEPARATOR = " ";
static const uint32_t TFWM_BAR_FG = WHITE;
static const uint32_t TFWM_BAR_BG = BLACK;
static const uint32_t TFWM_BAR_FG_ACTIVE = BLACK;
static const uint32_t TFWM_BAR_BG_ACTIVE = WHITE;

/* ======================== WORKSPACES ======================= */

static const char         *cfg_workspace_list[] = {"1", "2", "3", "4", "5",
                                                   "6", "7", "8", "9"};
static const tfwm_layout_t cfg_active_layout[] = {
    {TFWM_LAYOUT_WINDOW,   "[W]"},
    {TFWM_LAYOUT_TILING,   "[T]"},
    {TFWM_LAYOUT_FLOATING, "[F]"},
};

/* ========================= COMMAND ========================= */

static const char *cmd_term[] = {"st", NULL};

static const char *cmd_ws1[] = {"1", NULL};
static const char *cmd_ws2[] = {"2", NULL};
static const char *cmd_ws3[] = {"3", NULL};
static const char *cmd_ws4[] = {"4", NULL};
static const char *cmd_ws5[] = {"5", NULL};
static const char *cmd_ws6[] = {"6", NULL};
static const char *cmd_ws7[] = {"7", NULL};
static const char *cmd_ws8[] = {"8", NULL};
static const char *cmd_ws9[] = {"9", NULL};

/* ========================= KEYBIND ========================= */

static const tfwm_keybind_t cfg_keybinds[] = {
    {MOD_KEY | MOD_SHIFT, 0x0071, tfwm_exit,                     NULL    }, /* 0x0071 = q */

    /* Application */
    {MOD_KEY,             0x0071, tfwm_window_kill,              NULL    }, /* 0x0071 = q */
    {MOD_KEY,             0xff0d, tfwm_window_spawn,             cmd_term}, /* 0xff0d = Return */

    /* Window Navigation */
    {MOD_KEY,             0x0068, tfwm_window_prev,              NULL    }, /* 0x0068 = h */
    {MOD_KEY,             0x006a, tfwm_window_prev,              NULL    }, /* 0x006a = j */
    {MOD_KEY,             0x006b, tfwm_window_next,              NULL    }, /* 0x006b = k */
    {MOD_KEY,             0x006c, tfwm_window_next,              NULL    }, /* 0x006c = l */
    {MOD_KEY,             0x0020, tfwm_window_swap_last,         NULL    }, /* 0x0020 = Space */

    /* Window Manipulation */
    {MOD_KEY,             0x006d, tfwm_window_toggle_fullscreen, NULL    }, /* 0x006d = m */

    /* Workspace Navigation */
    {MOD_KEY,             0x0060, tfwm_workspace_swap_prev,      NULL    }, /* 0x0060 = grave */
    {MOD_KEY,             0x005d, tfwm_workspace_next,           NULL    }, /* 0x005d = bracketright */
    {MOD_KEY,             0x005b, tfwm_workspace_prev,           NULL    }, /* 0x005b = bracketleft */
    {MOD_KEY,             0x0031, tfwm_workspace_switch,         cmd_ws1 }, /* 0x0031 = 1 */
    {MOD_KEY,             0x0032, tfwm_workspace_switch,         cmd_ws2 }, /* 0x0032 = 2 */
    {MOD_KEY,             0x0033, tfwm_workspace_switch,         cmd_ws3 }, /* 0x0033 = 3 */
    {MOD_KEY,             0x0034, tfwm_workspace_switch,         cmd_ws4 }, /* 0x0034 = 4 */
    {MOD_KEY,             0x0035, tfwm_workspace_switch,         cmd_ws5 }, /* 0x0035 = 5 */
    {MOD_KEY,             0x0036, tfwm_workspace_switch,         cmd_ws6 }, /* 0x0036 = 6 */
    {MOD_KEY,             0x0037, tfwm_workspace_switch,         cmd_ws7 }, /* 0x0037 = 7 */
    {MOD_KEY,             0x0038, tfwm_workspace_switch,         cmd_ws8 }, /* 0x0038 = 8 */
    {MOD_KEY,             0x0039, tfwm_workspace_switch,         cmd_ws9 }, /* 0x0039 = 9 */

    /* Workspace Layout */
    {MOD_KEY,             0x0074, tfwm_workspace_use_tiling,     NULL    }, /* 0x0074 = t */
    {MOD_KEY,             0x0066, tfwm_workspace_use_floating,   NULL    }, /* 0x0066 = f */
    {MOD_KEY,             0x0077, tfwm_workspace_use_window,     NULL    }, /* 0x0077 = w */
};

/* =========================== MISC ========================== */

static const char *TFWM_LOG_FILE = ".local/share/tfwm.0.log";

#endif  // !CONFIG_H
