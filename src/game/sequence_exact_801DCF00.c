#include "game/battle/battle_waza_types.h"

extern void wazaSequenceOwnerPlayIdle(void* effect, s32 arg1, s32 arg2);

void wazaSequenceOwnerSetNull(WazaEffect* effect)
{
    u8 flags = effect->flags;

    if ((flags & 2) != 2) {
        effect->flags = flags | 2;
        if ((u8)GSmodelIsRootNullAdded(P32(effect->model)) != 0) {
            GSmodelGetRootPosition(
                P32(effect->model), (GSvec*)((u8*)effect + 0x5c));
        } else {
            GSmodelAddNull(P32(effect->model),
                           (GSvec*)((u8*)effect + 0x5c), NULL, NULL);
        }
        wazaSequenceOwnerPlayIdle(effect, 1, 0);
    }
}
