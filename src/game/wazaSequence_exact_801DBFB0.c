#include "game/battle/battle_waza_types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif

WazaSequence* wazaSequenceAllocNode(void)
{
    u16 handle;
    WazaSequence* sequence;

    handle = _toolentryAlloc__FUl(sizeof(WazaSequence));
    if (handle != 0) {
        sequence = GSmemLock(handle);
        memset(sequence, 0, sizeof(WazaSequence));
        sequence->handle = handle;
        return sequence;
    }
    return NULL;
}
