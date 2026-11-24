/* Synchronous name-entry dialog, 0x800281F4-0x80028444. */
#include "dolphin/types.h"
#include "game/name_entry_arg.h" /* NAME_ENTRY_ARG (32-bit pointer slots on the PC) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharCpy(void*, u8*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(s32, void*);
#endif
extern u8 lbl_8047A3D4[4], lbl_8047A3D0[4], lbl_8047A3CC[4];
extern u8 lbl_8047A3C8[4], lbl_8047A3C4[4];
extern u32 lbl_8047A3C0, lbl_8047A3BC, lbl_8047A3B8;
extern u32 lbl_8047A3B4, lbl_8047A3B0;
extern u8 lbl_803A2068[];
extern const f32 lbl_8047B930;

s32 inputName__FPUsPUsiii(u16* name, u16* defaultName, s32 kind, s32 index, s32 canCancel)
{
    extern void dbgMenuSetEnable(s32 enable);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuOpenCustom(s32 menuId, u32 parentId, s32, s32, s32, s32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE(u32 se);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpen(s32 slot, s32 msgId, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgClose(s32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32 menuId, s32 sync);
#endif

    NAME_ENTRY_ARG arg;
    s32 blanks;
    u16* letters;
    s32 length;
    u16* result;
    s32 valid;
    u16* blank;
    s32 accepted;
    s32 done;
    s32 yes;
    NAME_ENTRY_ARG* argp;
    s32 answer;

    done = 0;
    GScharCpy(arg.buffer, (u8*)defaultName);
    name[0] = 0;
    P32_SET(arg.name, name);
    arg.kind = kind;
    arg.index = index;
    *(s32*)lbl_8047A3D4 = 0;
    P32_SET(arg.row, (s32*)lbl_8047A3D4);
    *(s32*)lbl_8047A3D0 = 0;
    P32_SET(arg.letter, (s32*)lbl_8047A3D0);
    *(s32*)lbl_8047A3CC = 0;
    P32_SET(arg.column, (s32*)lbl_8047A3CC);
    *(f32*)lbl_8047A3C8 = lbl_8047B930;
    P32_SET(arg.fade, (f32*)lbl_8047A3C8);
    *(s32*)lbl_8047A3C4 = 0;
    P32_SET(arg.state, (s32*)lbl_8047A3C4);
    P32_SET(arg.work0, &lbl_8047A3C0);
    P32_SET(arg.work1, &lbl_8047A3BC);
    P32_SET(arg.work2, &lbl_8047A3B8);
    P32_SET(arg.work3, &lbl_8047A3B4);
    P32_SET(arg.work4, &lbl_8047A3B0);
    dbgMenuSetEnable(0);
    argp = &arg;
    while (!done) {
        menuOpenCustom(0x6e, windowGetActiveID(), 0, 0, 1, 1, argp);

        letters = P32(arg.name);
        for (length = 0; letters[length] != 0; length++) {
        }
        if (length <= 0) {
            valid = 0;
        } else {
            blank = (u16*)GSmsgGetGSchar(0x2ef9);
            for (blanks = 0; blanks < length; blanks++, letters++) {
                if (*letters != *blank) {
                    break;
                }
            }
            if (blanks >= length) {
                valid = 0;
            } else {
                valid = 1;
            }
        }
        if (valid) {
            result = P32(arg.name);
        } else {
            result = arg.buffer;
        }

        soundStartSE(0x440);
        msgctrlSetValue(0x4d, PC_PTR32_IMPLICIT(result));
        winMsgOpen(2, 0x2ef6, 1, 0);
        answer = menuSubOpenYesNo(0, -1, -1, 0);
        winMsgClose(1);
        if (answer == 1 || answer == -1) {
            yes = 0;
        } else {
            yes = 1;
        }
        accepted = yes;
        if (canCancel == 0) {
            done = 1;
        } else if (yes) {
            done = 1;
        }
    }
    dbgMenuSetEnable(1);
    menuClose(0x6e);
    menuCloseSync(0x6e, 1);
    if (accepted) {
        GScharCpy(lbl_803A2068, (u8*)result);
        return 1;
    }
    return 0;
}
