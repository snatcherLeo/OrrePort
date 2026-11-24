/** Exact e-Reader menu callbacks, 0x80075A34 - 0x80075DC8. */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std112_modelGetResourceID(u32 resourceId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSresGetResource(u32 archive, u32 resource);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void cameraPlayAnime(s32 cameraId, u32 animationId, s32 frame, s32 loop);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSscene_SetMode(u32 mode);
#endif
extern s32 std078_flagSetFalse(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 std081_flagCheckTrue(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std082_flagGetValue(s32);
#endif
extern void _flagSet(s32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fadeCheck(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorLink(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(s32 id, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpenCustom(s32 slot, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 memcardExecTask(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* gamedatasaveGetStatus(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(s32);
#endif
extern u8 lbl_8047A5D0;

void fn_80075A34(void)
{
    u32 archive;

    archive = std093_getCurrentGroupID();
    GSresGetResource(archive,
                     *(u32*)&lbl_8047A5D0 = std112_modelGetResourceID(PC_PTR(0x10801000)));
    cameraPlayAnime(0x5E0, 0x10821800, 0, 1);
    GSscene_SetMode(4);
}

s32 fn_80075A9C(void) { return std078_flagSetFalse(0xab5); }
u8 fn_80075AC0(void) { return std081_flagCheckTrue(0xab5); }
s32 fn_80075AE4(void) { return std078_flagSetFalse(0xab4); }
u8 fn_80075B08(void) { return std081_flagCheckTrue(0xab4); }
s32 fn_80075B2C(void) { return std078_flagSetFalse(0xab3); }
u8 fn_80075B50(void) { return std081_flagCheckTrue(0xab3); }

s32 exShrineIncCounter(void)
{
    s32 result;
    u32 value;

    value = std082_flagGetValue(0xab2) + 1;
    result = 1;
    if (value > 0x30) {
        value = 0x30;
        result = 0;
    }
    _flagSet(0xab2, value);
    return result;
}

s32 fn_80075BC4(void)
{
    u32 value;

    value = std082_flagGetValue(0xab2);
    if (value > 0x30) {
        return 0;
    } else {
        return 0x30 - value;
    }
}

s32 fn_80075BFC(void) { return std078_flagSetFalse(0xab1); }
u8 fn_80075C20(void) { return std081_flagCheckTrue(0xab1); }
u8 fn_80075C44(void) { return std081_flagCheckTrue(0xa14); }

/* NAME-T3 fn_80075C68 */ /* exit callback of room 0x321 ex_top (bonus disc) */
s32 exDiscTopExit(void)
{
    fadeCheck(1);
#ifdef TARGET_PC /* menuClose leaves windowSearchID(id) in r3 (the window if still open, else 0): verified from the assembly */
    menuClose(0xe0);
    return (s32)PC_PTR32(windowSearchID(0xe0));
#else
    return menuClose(0xe0);
#endif
}

void exDiscCheckStorySave(void)
{
    s32 result;

    for (;;) {
        msgctrlSetValue(0x37, 0);
        result = menuOpenCustom(0xE0, 0, 0, 0x10, 1, 0);
        fadeCheck(1);

        switch (result) {
        case 0:
            floorLink(0x322, 0);
            return;
        case 1:
            result = memcardExecTask(2, 2, 0);
            if (result != 3 || gamedatasaveGetStatus(0, 4) == 0) {
                if (result == -1) {
                    continue;
                }
                winMsgOpen(2, 0x44DB, 1, 0);
                winMsgClose(1);
                continue;
            }
            floorLink(0x323, 0);
            return;
        case -1:
            break;
        case 2:
            break;
        default:
            break;
        }

        floorLink(0x320, 0);
        return;
    }
}

/* NAME-T3 fn_80075D98 */ /* init callback (empty) of room 0x321 ex_top */
void exDiscTopInit(void)
{
}

/* NAME-T3 fn_80075D9C */ /* exit callback of room 0x320 ex_title */
s32 exDiscTitleExit(void)
{
    fadeCheck(1);
#ifdef TARGET_PC /* menuClose leaves windowSearchID(id) in r3 (the window if still open, else 0): verified from the assembly */
    menuClose(0xe2);
    return (s32)PC_PTR32(windowSearchID(0xe2));
#else
    return menuClose(0xe2);
#endif
}
