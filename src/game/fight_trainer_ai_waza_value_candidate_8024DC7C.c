/**
 * @file fight_trainer_ai_waza_value_candidate_8024DC7C.c
 * @brief fightTrainerAiWazaValueKiaipanti (Focus Punch), 0x8024DC7C -
 *        0x8024DE8C.
 *
 * fightFloorGetFightTrainerFightOutPokemonPtrAry returns a u16 count (as
 * declared in fightTrainerAiWazaValueKonoyubitomare). With that prototype
 * the count keeps the raw return register and each loop masks it, which is
 * retail's allocation; the old u32 form walled at 98.56%.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fightFloorGetFightTrainerFightOutPokemonPtrAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightAiGetLastUseWaza(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif

/* Address: 0x8024DC7C | Size: 0x210 (528 bytes) */
u32 fightTrainerAiWazaValueKiaipanti(void* ctx, u32 param1, u32 param2, u32 param3)
{
    u32 entries[8];
    u32 handle;
    u16 count;
    u16 i;

    handle = 0;
    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), entries, 0, 1);
    if ((u16)fightAiGetLastUseWaza(ctx, param1) == 0x117) {
        handle = fn_80239984(0, ctx, 0x5B);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x5B);
    }
    for (i = 0; i < count; i++) {
        if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(entries[i]), PC_PTR(8)) == 1) {
            handle = fn_80239984(handle, ctx, 0x5C);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x5C);
            break;
        }
    }
    for (i = 0; i < count; i++) {
        if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(entries[i]), PC_PTR(7)) == 1) {
            handle = fn_80239984(handle, ctx, 0x5D);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x5D);
            break;
        }
    }
    handle = fn_80239984(handle, ctx, 0x5E);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x5E);
    return handle;
}
