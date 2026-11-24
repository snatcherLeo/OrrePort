/**
 * @file menuPokemonOpen_exact.c
 * @brief Party-menu entry-point wrappers.
 */
#include "dolphin/types.h"

/* PC: the three wrappers below are written void, but every caller uses their result (the chosen slot:
 * gs_event_exec.c's item give / use, menuFight.c's PKMN menu). On the GameCube each ends right after the call to
 * menuPokemonOpenSub, so its result stays in r3 (all three match 100%); on the PC a void function returns garbage, and
 * using a Potion in Willie's battle never got its Pokémon list (2026-10-17). Return the sub-call's result on the PC;
 * the GameCube text is unchanged. */
#ifdef TARGET_PC
#define MPO_RESULT u32
#define MPO_RETURN return
#else
#define MPO_RESULT void
#define MPO_RETURN
#endif

MPO_RESULT menuPokemonOpenItemGive(u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuPokemonOpenSub(u32, u32, u32, u32, u32, u32);
#endif

    lbl_803A1D40[4] = 1;
    MPO_RETURN menuPokemonOpenSub(5, arg0, arg1, arg2, arg3, 1);
}

MPO_RESULT menuPokemonOpenItemUse(u32 arg0, u32 arg1, u32 arg2, u32 arg3)
{
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonOpenSub();
#endif

    lbl_803A1D40[4] = 1;
    MPO_RETURN menuPokemonOpenSub(arg0, 0, 0, arg2, arg3, arg1);
}

MPO_RESULT menuPokemonOpenFight(u8 arg0, u8 arg1, u32 arg2, u32 arg3)
{
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuPokemonOpenSub(u32, u32, u32, u32, u32, u32);
#endif

    if (arg0 == 1) {
        lbl_803A1D40[4] = 0;
    } else {
        lbl_803A1D40[4] = 1;
    }
    *(u32*)(lbl_803A1D40 + 0x18) = arg3;
    /* Corrected 2026-10-19 (verified from snatcherLeo's stuck battles): an automated prototype-wrapping pass had put
     * this call and the two above inside "#ifndef TARGET_PC" (it read "MPO_RETURN name(...);" as a declaration), so
     * on the PC no party menu opened: the battle's "choose your next Pokémon" loop spun forever after a faint, and
     * item give / use got no list. They are statements: always compiled. */
    MPO_RETURN menuPokemonOpenSub(2, 0, 0, arg1, arg2, 1);
}

u32 menuPokemonOpen(u32 arg0, u32 arg1, u32 arg2) /* returns the sub-call's result (r3 untouched before blr) */
{
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuPokemonOpenSub(u32, u32, u32, u32, u32, u32);
#endif

    lbl_803A1D40[4] = 1;
    return menuPokemonOpenSub(arg0, 0, 0, arg1, arg2, 1);
}
