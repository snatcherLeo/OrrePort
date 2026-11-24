#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

u8 dbgMenuGetEnable(void)
{
    return _dbgMenuEnable;
}

void dbgMenuSetEnable(u8 enabled)
{
    _dbgMenuEnable = enabled;
}

u32 dbgMenuIsOpen(void)
{
    u8 result = menuIsCheck(_menu_data_number);
    return result != 0 ? 1 : 0;
}
