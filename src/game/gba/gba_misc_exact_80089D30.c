#include "dolphin/types.h"

/* Retail stores this six-entry retry table in small uninitialized data. */
extern u16 lbl_8047A684[6] __attribute__((section(".sdata")));

/*
 * Runs fn_80071AE4 for a 1-based port on the caller's upload block
 * (gbaCommunication.c passes p + 0x4344) and clears the port's retry
 * counter; returns fn_80071AE4's result.
 */
s32 fn_80089D30(s32 index, const u8* data)
{
    extern s32 fn_80071AE4(s32 chan, const u8* data);
    s32 result;

    result = fn_80071AE4(index - 1, data);
    lbl_8047A684[index - 1] = 0;
    return result;
}

#ifdef TARGET_PC
/* PC: returns fn_800722A0's status (the GameCube body leaves its r3; memcard.c tests it; port-gaps 37) */
/* NAME-T1 fn_80089D74 */
s32 gbaCommandChangePokemonEnd(s32 index)
{
    extern s32 fn_800722A0(s32);
    return fn_800722A0(index - 1);
}
#else
/* NAME-T1 fn_80089D74 */
void gbaCommandChangePokemonEnd(s32 index)
{
    extern void fn_800722A0(s32);
    fn_800722A0(index - 1);
}
#endif
