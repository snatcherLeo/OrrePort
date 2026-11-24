#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* darkPokemonSaveGetPtr(u32 data);
#endif

void darkPokemonSaveSet4A6(u16 id, u16 value)
{
    u8* data = darkPokemonSaveGetPtr(0);

    if (data != NULL) {
        *(u16*)(data + id * 0x18 + 0x4A6) = value;
    }
}
