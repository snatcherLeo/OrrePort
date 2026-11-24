/** Byte-exact PDA button callback, 0x80039F44 - 0x80039F70. */
#include "dolphin/types.h"

extern s32 lbl_8047A4B8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void* button);
#endif

/* NAME-T3 fn_80039F44 */ /* button callback of the PC item list (menu 0x25) */
void itemStorageListButtonCB(void* button)
{
    if (lbl_8047A4B8 < 0) {
        menuButtonNormal(button);
    }
}
