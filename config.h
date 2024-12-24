#ifndef CONFIG_H
#define CONFIG_H

#include "tfwm.h"

/* ========================= MOD KEY ========================= */

#define MOD_KEY XCB_MOD_MASK_4
#define MOD_SHIFT XCB_MOD_MASK_SHIFT
#define MOD_CTRL XCB_MOD_MASK_SHIFT

#define BUTTON_LEFT 1
#define BUTTON_RIGHT 3

/* ========================== WINDOW ========================= */

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400
#define MIN_WINDOW_WIDTH 60
#define MIN_WINDOW_HEIGHT 40
#define BORDER_WIDTH 1
#define BORDER_ACTIVE 0xFFFFFF
#define BORDER_INACTIVE 0x696969

/* ======================= BAR CONTENT ======================= */

#define BAR_HEIGHT 12
#define BAR_FONT_NAME                                                          \
  "-xos4-terminus-medium-r-normal--14-140-72-72-c-80-iso10646-1"
#define BAR_FOREGROUND 0xFFFFFF
#define BAR_BACKGROUND 0x000000
#define BAR_FOREGROUND_ACTIVE 0x000000
#define BAR_BACKGROUND_ACTIVE 0xFFFFFF

#define LAYOUT_TILING_DISPLAY "[T]"
#define LAYOUT_FLOATING_DISPLAY "[F]"
#define LAYOUT_WINDOW_DISPLAY "[W]"

/* ======================== WORKSPACES ======================= */

static tfwm_workspace_t workspaces[] = {
    {TILING, "1", 0}, {WINDOW, "2", 0}, {FLOATING, "3", 0},
    {TILING, "4", 0}, {TILING, "5", 0}, {TILING, "6", 0},
    {TILING, "7", 0}, {TILING, "8", 0}, {TILING, "9", 0},
};

/* ========================= COMMAND ========================= */

static char *cmd_term[] = {"st", NULL};

static char *goto_ws1[] = {"1", NULL};
static char *goto_ws2[] = {"2", NULL};
static char *goto_ws3[] = {"3", NULL};
static char *goto_ws4[] = {"4", NULL};
static char *goto_ws5[] = {"5", NULL};
static char *goto_ws6[] = {"6", NULL};
static char *goto_ws7[] = {"7", NULL};
static char *goto_ws8[] = {"8", NULL};
static char *goto_ws9[] = {"9", NULL};

/* ========================= KEYBIND ========================= */

static tfwm_keybind_t keybinds[] = {
    /* Application */
    {MOD_KEY | MOD_SHIFT, 0x0071, tfwm_exit, NULL}, /* 0x0071 = q */
    {MOD_KEY, 0x0071, tfwm_kill, NULL},             /* 0x0071 = q */
    {MOD_KEY, 0xff0d, tfwm_spawn, cmd_term},        /* 0xff0d = Return */

    /* Workspace Navigation */
    {MOD_KEY, 0x005d, tfwm_next_workspace, NULL}, /* 0x005d = bracketright */
    {MOD_KEY, 0x005b, tfwm_prev_workspace, NULL}, /* 0x005b = bracketleft */
    {MOD_KEY, 0x0060, tfwm_swap_prev_workspace, NULL}, /* 0x0060 = grave */
    {MOD_KEY, 0x0031, tfwm_goto_workspace, goto_ws1},  /* 0x0031 = 1 */
    {MOD_KEY, 0x0032, tfwm_goto_workspace, goto_ws2},  /* 0x0032 = 2 */
    {MOD_KEY, 0x0033, tfwm_goto_workspace, goto_ws3},  /* 0x0033 = 3 */
    {MOD_KEY, 0x0034, tfwm_goto_workspace, goto_ws4},  /* 0x0034 = 4 */
    {MOD_KEY, 0x0035, tfwm_goto_workspace, goto_ws5},  /* 0x0035 = 5 */
    {MOD_KEY, 0x0036, tfwm_goto_workspace, goto_ws6},  /* 0x0036 = 6 */
    {MOD_KEY, 0x0037, tfwm_goto_workspace, goto_ws7},  /* 0x0037 = 7 */
    {MOD_KEY, 0x0038, tfwm_goto_workspace, goto_ws8},  /* 0x0038 = 8 */
    {MOD_KEY, 0x0039, tfwm_goto_workspace, goto_ws9},  /* 0x0039 = 9 */

    /* Workspace Layout */
    {MOD_KEY, 0x0074, tfwm_workspace_use_tiling, NULL},   /* 0x0074 = t */
    {MOD_KEY, 0x0066, tfwm_workspace_use_floating, NULL}, /* 0x0066 = f */
    {MOD_KEY, 0x0077, tfwm_workspace_use_window, NULL},   /* 0x0077 = w */
};

#endif // !CONFIG_H
