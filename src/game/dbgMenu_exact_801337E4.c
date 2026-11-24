#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

void dbgMenuClose(void)
{
    u8 disabled = 0;
    s32 menuId = _menu_data_number;

    lbl_8047AED1 = disabled;
    menuClose(menuId);
}
