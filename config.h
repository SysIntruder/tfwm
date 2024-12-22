#ifndef CONFIG_H
#define CONFIG_H

#include "tfwm.h"

/* ========================= MOD KEY ========================= */

#define MOD_KEY XCB_MOD_MASK_4
#define MOD_SHIFT XCB_MOD_MASK_SHIFT
#define MOD_CTRL XCB_MOD_MASK_SHIFT

/* ========================= KEYBIND ========================= */

static tfwm_keybind_t keybinds[] = {
    {MOD_KEY | MOD_SHIFT, 0x0071, tfwm_exit, NULL}, /* 0x0071 = q */
};

#endif // !CONFIG_H
