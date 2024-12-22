#ifndef CONFIG_H
#define CONFIG_H

#include "tfwm.h"

/* ========================= MOD KEY ========================= */

#define MOD_KEY XCB_MOD_MASK_4
#define MOD_SHIFT XCB_MOD_MASK_SHIFT
#define MOD_CTRL XCB_MOD_MASK_SHIFT

/* ========================== WINDOW ========================= */

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400
#define MIN_WINDOW_WIDTH 60
#define MIN_WINDOW_HEIGHT 40
#define BORDER_WIDTH 1
#define BORDER_ACTIVE 0xFFFFFF
#define BORDER_INACTIVE 0x696969

/* ======================= BAR CONTENT ======================= */

#define BAR_FONT_NAME "fixed"
#define BAR_FOREGROUND 0xFFFFFF
#define BAR_BACKGROUND 0x000000

#define LAYOUT_TILING_DISPLAY "[T]"
#define LAYOUT_FLOATING_DISPLAY "[F]"
#define LAYOUT_WINDOW_DISPLAY "[W]"

/* ======================== WORKSPACES ======================= */

static tfwm_workspace_t workspaces[] = {
    {TILING, "1", 0},
    {FLOATING, "2", 0},
    {WINDOW, "3", 0},
};

/* ========================= COMMAND ========================= */

static char *cmd_term[] = {"st", NULL};

static char *goto_ws1[] = {"1", NULL};
static char *goto_ws2[] = {"2", NULL};
static char *goto_ws3[] = {"3", NULL};

/* ========================= KEYBIND ========================= */

static tfwm_keybind_t keybinds[] = {
    {MOD_KEY | MOD_SHIFT, 0x0071, tfwm_exit, NULL},       /* 0x0071 = q */
    {MOD_KEY, 0x0071, tfwm_kill, NULL},                   /* 0x0071 = q */
    {MOD_KEY, 0xff0d, tfwm_spawn, cmd_term},              /* 0xff0d = Return */
    {MOD_KEY, 0x0031, tfwm_goto_workspace, goto_ws1},     /* 0x0031 = 1 */
    {MOD_KEY, 0x0032, tfwm_goto_workspace, goto_ws2},     /* 0x0032 = 2 */
    {MOD_KEY, 0x0033, tfwm_goto_workspace, goto_ws3},     /* 0x0033 = 3 */
    {MOD_KEY, 0x0060, tfwm_swap_prev_workspace, NULL},    /* 0x0060 = grave */
    {MOD_KEY, 0x0074, tfwm_workspace_use_tiling, NULL},   /* 0x0074 = t */
    {MOD_KEY, 0x0066, tfwm_workspace_use_floating, NULL}, /* 0x0066 = f */
    {MOD_KEY, 0x0077, tfwm_workspace_use_window, NULL},   /* 0x0077 = w */
};

#endif // !CONFIG_H
