/**
 * @file fight_range_802373B0.c
 * @brief Exact AI threshold comparison and its private small-data constants.
 */

#include "dolphin/types.h"

#pragma section ".sdata2"
#define SDATA2 SECTION(".sdata2")

/*
 * Keep the zero initializer in the file-backed section. Without this pragma,
 * MWCC moves lbl_8047E61C to .sbss2 and breaks the original contiguous pool.
 */
SDATA2 f32 lbl_8047E618 = 1.0f;
#pragma explicit_zero_data on
SDATA2 f32 lbl_8047E61C = 0.0f;
#pragma explicit_zero_data reset

#define fn_801FB1C0 fightTrainerGetStatus
#define fn_8012640C pokemonGetStatus
#define fn_80205BE8 fightPokemonGetPokemonPtr
int fightTrainerAiGetHpPercent(u32 ctx, u32 fightOut, int direction, f32 ratio)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801FB1C0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80205BE8();
#endif
    u32 pokemon;
    u32 pokemonPtr;
    u16 trainer;
    s32 current;
    u16 maximum;
    s32 scaled;
    u32 result;

    pokemon = (0, fn_8012640C(PC_PTR(fightOut), 0, 0xD6, 0));
    trainer = fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0);
    fn_801FB1C0(0, trainer, 2, 0);
    trainer = fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0);
    fn_801FB1C0(0, trainer, 2, 0);
    pokemonPtr = PC_PTR32_IMPLICIT(fn_80205BE8(pokemon));
    current = (u16)fn_8012640C(PC_PTR(pokemonPtr), 0, 0x83, 0);
    trainer = fn_801FB1C0(PC_PTR(ctx), 0, 0x43, 0);
    fn_801FB1C0(0, trainer, 2, 0);
    pokemonPtr = PC_PTR32_IMPLICIT(fn_80205BE8(pokemon));
    maximum = fn_8012640C(PC_PTR(pokemonPtr), 0, 0x87, 0);

    if (lbl_8047E61C == ratio) {
        ratio = lbl_8047E618;
    }
    scaled = (s32)((f32)maximum * ratio);

    if ((s8)direction == 0) {
        if (current == scaled) {
            result = 1;
            goto done;
        }
    } else if ((s8)direction > 0) {
        if (current >= scaled) {
            result = 1;
            goto done;
        }
    } else if ((s8)direction < 0) {
        if (current <= scaled) {
            result = 1;
            goto done;
        }
    }
    result = 0;
done:
    return result;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80205BE8
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FB1C0
#endif
