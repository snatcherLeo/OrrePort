/**
 * @file fight_range_exact_80220778.c
 * @brief Exact pure-C fight-sequence helper run, 0x80220778 - 0x80220868.
 */
#include "dolphin/types.h"

#define fn_801FEF74 fightOutPokemonGetJoutaiMigawariHp
#define fn_80202810 fightOutPokemonWriteJoutaiDataId
s32 _WsWkcActSubJoutaiMigawari__FPvUsPv(void* ctx, u16 param2, void* param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_801FEF74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80202810();
#endif
    if (fightOutPokemonIsPokemonJoutaiDataId(ctx, PC_PTR(0x14)) == 1 && fn_801FEF74(ctx) <= 0) {
        fn_80202810(ctx, PC_PTR(0x14));
    }
    return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_80202810
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FEF74
#endif

#define fn_8012640C pokemonGetStatus
#define fn_8020147C fightOutPokemonDoItemSoubi
#define fn_801254B4 pokemonSetStatus
#define fn_801FECD4 fightOutPokemonIsUseHensinBuff
#define fn_801FE7EC fightOutPokemonSetHensinPokemonStatusId
s32 _fightOutPokemonDoStockItemSub(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8012640C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8020147C();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801254B4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801FECD4();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801FE7EC();
#endif
    u16 field = (u16)fn_8012640C(PC_PTR(ctx), 0, 0xfb, 0);
    if (field != 0) {
        fn_8020147C(PC_PTR(ctx), field, 1, 0);
        fn_801254B4(PC_PTR(ctx), 0, 0xfb, 0, 0);
        if ((u8)fn_801FECD4(PC_PTR(ctx)) == 1) {
            fn_801FE7EC(PC_PTR(ctx), 0x82, 0, 0);
        }
    }
    return 1;
}
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8012640C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_8020147C
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801254B4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FECD4
#endif
#ifndef TARGET_PC /* PC: the address-name alias (pc/game_pc_alias.h) stays in force */
#undef fn_801FE7EC
#endif
