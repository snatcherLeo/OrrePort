#include "dolphin/types.h"
#include "game/constants/move_effects.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus(void* ptr, u16 dataId, u16 status, u32 index);
#endif

u32 fightSeqRendouWazaCheck(u16 id)
{
    if (id == 0 || id == 0x165 || id == 0xd6 || id == 0x112 || id == 0x77 ||
        id == 0x76) {
        return 1;
    }
    return 0;
}

u8 fn_8021901C(u16 dataId)
{
    u16 effect = (u16)wazaGetStatus(0, dataId, 9, 0);

    if (effect == MOVE_EFFECT_SKULL_BASH || effect == MOVE_EFFECT_RAZOR_WIND || effect == MOVE_EFFECT_SKY_ATTACK ||
        effect == MOVE_EFFECT_SOLARBEAM || effect == MOVE_EFFECT_SEMI_INVULNERABLE || effect == MOVE_EFFECT_BIDE) {
        return 1;
    }
    return 0;
}
