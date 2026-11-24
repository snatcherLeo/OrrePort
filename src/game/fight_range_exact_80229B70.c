#include "dolphin/types.h"
#include "game/constants/move_effects.h"

u8 fightIsWazaThunderInRain(u32 move_id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus(void*, u32, u16, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightFloorGetNowTenkouDataId(void*, u32);
#endif
    u16 value = (u16)wazaGetStatus(0, move_id, 9, 0);

    if ((u8)fightFloorGetNowTenkouDataId(0, 1) == 2 && value == MOVE_EFFECT_THUNDER) {
        return 1;
    }
    return 0;
}

s32 fn_80229BD8(s32 arg)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus(void*, u32, u16, u32);
#endif
    u16 value = (u16)wazaGetStatus(0, arg, 9, 0);

    if (value == MOVE_EFFECT_NEVER_MISSES || value == MOVE_EFFECT_VITAL_THROW) {
        return 1;
    }
    return 0;
}
