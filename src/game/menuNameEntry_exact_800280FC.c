/* Name-entry controller and button callback, 0x800280FC-0x800281F4.
 * RULE-EXCEPTION(user-approved): retained volatile state/fade reads;
 * see docs/RULE_EXCEPTIONS.md.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqSetMenu(void*, s32);
#endif
extern f32 lbl_8047B930;
extern f32 lbl_8047B950;
extern f32 lbl_8047B934;

s32 menuNameEntryCtrl(void* r3) {
    u8* r30;
    u8* r31;
    f32* fptr;
    f32 f0;
    f32 f1;
    f32 f2;
    s32 state;
    s32 flag;
    u8 one;

    r30 = (u8*)r3;
    state = (s32)(s8)*(volatile u8*)(r30 + 1);
    r31 = PTR32_AT_T(u8, r30, 0x60);
    switch (state) {
    case 0:
        flag = (s32)(s8)*(volatile u8*)(r30 + 2);
        if (flag == 0) {
#ifdef TARGET_PC
            /* the window's +0x04 is its 32-bit menu id (win_sequence.h tagWINDOW_WORK). Corrected 2026-10-21 (
             * verified): a void* read took 8 bytes on the PC, the id plus the next field; it worked for Wes's name
             * while that field was 0 and failed the pointer check at Rui's name (0x010000000000006E) */
            winSeqSetMenu(*(s32*)(r30 + 4), 0x56);
#else
            winSeqSetMenu(PC_PTR32_IMPLICIT(*(void**)(r30 + 4)), 0x56);
#endif
            one = 1;
            *PTR32_AT_T(f32, r31, 0x30) = lbl_8047B930; /* `fade`, a 32-bit slot on the PC (name_entry_arg.h) */
            r30[2] = one;
        }
        break;
    case 2:
        fptr = PTR32_AT_T(f32, r31, 0x30);
        f0 = lbl_8047B950;
        f2 = *(volatile f32*)fptr;
        f1 = lbl_8047B934;
        f0 = f2 + f0;
        *fptr = f0;
        if (f0 >= f1) {
            fptr = PTR32_AT_T(f32, r31, 0x30);
            *fptr = *fptr - f1;
        }
        break;
    case 3:
        flag = (s32)(s8)*(volatile u8*)(r30 + 2);
        if (flag == 0) {
#ifdef TARGET_PC
            winSeqSetMenu(*(s32*)(r30 + 4), 0x5a); /* the 32-bit menu id, as above */
#else
            winSeqSetMenu(PC_PTR32_IMPLICIT(*(void**)(r30 + 4)), 0x5a);
#endif
            r30[2] = 1;
        }
        break;
    }
    return 0;
}

void menuNameEntryButton(void) { }
