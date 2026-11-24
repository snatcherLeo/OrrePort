/**
 * @file menuNameEntry_exact_80026478.c
 * @brief Gender-symbol visibility callbacks for the selected Pokemon.
 */
#include "dolphin/types.h"
#include "game/constants/gender.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* heroGetStatus(s32, s32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(u8* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonGetSex(void* pokemon);
#endif

s32 fn_80026478(void* owner, u8* draw)
{
    void* context;
    void* pokemon;
    u8 visible;

    context = PTR32_AT_T(void, (u8*)owner, 0x60);
    visible = 0;
    if (*(s32*)((u8*)context + 0x1C) != 2) {
        draw[0x67] = 0;
        return 0;
    }
    pokemon = PC_PTR(heroGetStatus(0, 3, (u16)*(u32*)((u8*)context + 0x20)));
    if ((u8)pokemonCheckValid((u8*)pokemon) == 0) {
        goto done;
    }
    if ((u32)(pokemonGetSex(pokemon) & 0xFF) != GENDER_FEMALE) {
        goto done;
    }
    visible = 0xFF;
done:
    draw[0x67] = visible;
    return 0;
}

s32 fn_8002651C(void* owner, u8* draw)
{
    void* context;
    void* pokemon;
    u8 visible;

    context = PTR32_AT_T(void, (u8*)owner, 0x60);
    visible = 0;
    if (*(s32*)((u8*)context + 0x1C) != 2) {
        draw[0x67] = 0;
        return 0;
    }
    pokemon = PC_PTR(heroGetStatus(0, 3, (u16)*(u32*)((u8*)context + 0x20)));
    if ((u8)pokemonCheckValid((u8*)pokemon) == 0) {
        goto done;
    }
    if ((u32)(pokemonGetSex(pokemon) & 0xFF) != GENDER_MALE) {
        goto done;
    }
    visible = 0xFF;
done:
    draw[0x67] = visible;
    return 0;
}
