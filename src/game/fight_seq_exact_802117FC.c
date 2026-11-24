#include "dolphin/types.h"

extern u32 ServerStatusFlag;

u32 fightSeqGetEffectAminFlag(void)
{
    return !(ServerStatusFlag & 0x80);
}
