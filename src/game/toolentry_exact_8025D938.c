#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* savedataBattleGetEntryPtr(s32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroBiosGetPokemonPtr(void* hero, u32 slot);
#endif

void* toolentryTaisenGetEntryPokemonPtr(s32 index, u16 slot)
{
    return heroBiosGetPokemonPtr(savedataBattleGetEntryPtr(index) + 0x2C, slot);
}

void* toolentryTaisenGetPokemonPtr(s32 index, u16 slot)
{
    return heroBiosGetPokemonPtr(savedataBattleGetEntryPtr(index) + 0xB44, slot);
}
