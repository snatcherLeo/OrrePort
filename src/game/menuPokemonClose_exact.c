/**
 * @file menuPokemonClose_exact.c
 * @brief Close the party menu and restore its field state.
 */
#include "dolphin/types.h"

extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorBattleEndTerminateScripts();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorSetFadeScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch();
#endif

void menuPokemonClose(void)
{
    menuCloseCustom(0x63, 0, 1);
    if (lbl_803A1D40[2] != 0) {
        floorBattleEndTerminateScripts();
        if (lbl_803A1D40[3] != 1) {
            floorSetFadeScript(0, 0);
        }
        _threadSwitch();
    }
}
