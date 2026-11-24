/** Exact PDA-mail state/open helpers, 0x8004BDB8 - 0x8004BFB0. */
#include "dolphin/types.h"

extern u8 lbl_803A6A60[];
extern void std098_mapWarpToMenuRoom(s32 floorId);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorSetFadeScript(s32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 mailGetNbMailInMailbox(void);
#endif
extern u16* lbl_8047A500;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 mailGetSubject(s32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 mailGetSenderName(s32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GScharCmp(void* a, void* b);
#endif
extern s32 mailGetReceiveNumber(s32 mailId);

void fn_8004BDB8(s8 a, s8 b)
{
    if (a >= 0) {
        *(s8*) &lbl_803A6A60[1] = a;
    }
    if (b < 0) {
        return;
    }
    *(s8*) &lbl_803A6A60[1] = b;
}

u8 fn_8004BDEC(void)
{
    return lbl_803A6A60[0];
}

u8 fn_8004BDFC(void)
{
    return lbl_803A6A60[1];
}

void menuPdaOpen(void)
{
    std098_mapWarpToMenuRoom(0x392);
    floorSetFadeScript(0, 0);
    _threadSwitch();
}

s32 pdaMailGetMailID(s32 index)
{
    if (index < 0 || index >= mailGetNbMailInMailbox()) {
        return -1;
    }
    return lbl_8047A500[index];
}

s32 pdaMailCompareSubject(u16* a, u16* b)
{
    s32 idA = *a;
    s32 idB = *b;
    s32 new_var2;
    s32 cmp;
    s32* new_var;
    void* msgA = GSmsgGetGSchar(mailGetSubject(idA));
    void* msgB = GSmsgGetGSchar(mailGetSubject(idB));
    cmp = GScharCmp(msgA, msgB);
    if (cmp != 0) {
        new_var = &cmp;
        return *new_var;
    }
    new_var2 = mailGetReceiveNumber(idA);
    return mailGetReceiveNumber(idB) - new_var2;
}

s32 fn_8004BF20(u16* a, u16* b)
{
    s32 idA = *a;
    s32 idB = *b;
    s32 cmp;
    void* msgA = GSmsgGetGSchar(mailGetSenderName(idA));
    s32 new_var;
    void* msgB = GSmsgGetGSchar(mailGetSenderName(idB));
    cmp = GScharCmp(msgA, msgB);
    if (cmp != 0) {
        return cmp;
    }
    new_var = mailGetReceiveNumber(idA);
    return mailGetReceiveNumber(idB) - new_var;
}
