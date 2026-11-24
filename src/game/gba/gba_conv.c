/**
 * @file gba_conv.c
 * @brief GBA data conversion and validation (0x80083AF4-0x80089048)
 *
 * Address range: 0x80083AF4 - 0x80089048
 * Total functions: 28
 */

#include "dolphin/types.h"
#include "game/gba/gba_conv.h"

/* ===== External function declarations ===== */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8001E074();
#endif
extern void fn_8005CF2C();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8006A76C();
#endif
extern void savedataBattleClearFlagC98B();
extern void savedataBattleSetFlagC98B();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataBattleGetFlagC98B();
#endif
extern void savedataBattleSetCouponRun();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataBattleGetCouponRun();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataBattleGetSavedHeroKind();
#endif
extern void menuCommonFree();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCommonAlloc();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuRuleCheckPartyAndRule();
#endif
extern void fn_8008ABA0();
extern void gbaCommStartTransferA();
extern void gbaCommStartTransferB();
extern void gbaCommStartTransferC();
extern void gbaCommStartTransferFile();
extern void gbaCommIsBusy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80093610();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gbaCommEnd();
#endif
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern void SIProbe();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgetFrameStep();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D37CC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemGetHandleByPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemLock();
#endif
extern void GSmemAlloc();
extern void GSmodelIsAnimating();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelStartAnimation();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimRate();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimFrame();
#endif
extern void GSmodelSetAnimType();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetAnimIndex();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch();
#endif
extern void GSresFindById();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharToGBA();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std092_getCurrentRoomID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorBattleEndTerminateScripts();
#endif
extern void std098_mapWarpToMenuRoom();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuIsCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuOpen();
#endif
extern void menuGetEnablePort();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuSetEnablePort();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetFreeWork();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowSearchItemID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowSearchID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetPortKeyInfo();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetKeyInfo();
#endif
extern void winMsgCheck();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801081F8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorGetPrevFloorID();
#endif
extern void wazaDataBiosGetDoc();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaDataBiosGetPtr();
#endif
/* ... and 27 more external functions */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif

/* ===== SDA globals ===== */
extern u8 lbl_80478950;
extern u8 lbl_80478954;
extern u8 lbl_80478958;
extern u8 dbgMenuGBAAddCoupon_value;
extern u8 dbgMenuGBAResetCoupon_on;
extern u8 dbgMenuGBASetClearFlag_on;
extern u8 dbgMenuGBAClearCouponFlag_on;
extern u8 lbl_8047C190;
extern u8 lbl_8047C198;
extern u8 lbl_8047C1A0;
extern u8 lbl_8047C1A8;
extern u8 lbl_8047C1AC;
extern u8 lbl_8047C1B0;
extern u8 lbl_8047C1B8;
extern u8 lbl_8047C1C0;
extern u8 lbl_8047C1C4;
extern u8 lbl_8047C1C8;
extern u8 lbl_8047C1CC;

/* ===== Rodata / data labels ===== */
extern u8 jumptable_802EEB78[];
extern u8 lbl_8026F2E8[];
extern u8 lbl_8026F488[];
extern u8 lbl_8026F4F8[];
extern u8 lbl_803FB2F8[];

/* ===== Forward declarations ===== */
void fn_80083AF4(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80083BF8(void);
#endif
void fn_80083CBC(void);
void fn_80083CFC(void);
void fn_80083D30(void);
void fn_80083ECC(void);
void fn_80084034(void);
void fn_80084038(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_800849B4(void);
#endif
void cardeGbaTransfer(void);
void fn_80087AE8(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80087C64(void);
#endif
void fn_80088428(void);
u32 dnaKeyCodeCheck(u16 a, u16 b, u16 c);
s32 menuSaveLoadReceiveGift(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_80088964(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_800889A4(void);
#endif
s32 menuSaveLoadSaveGame(s32 capture);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 menuSaveLoadCommitA(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_80088D84(void);
#endif
void menuSaveLoadStorePosition(u8* p);
s32 dbgMenuGBAClearCouponFlag(void);
s32 dbgMenuGBASetClearFlag(void);
s32 dbgMenuGBAResetCoupon(void);
s32 dbgMenuGBAAddCoupon(void);
s32 dbgToolBattleDebugSetAGBConnectionMode(s32 a, s32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_80089028(void);
#endif
void fn_80089030(u8 x);

/* ===== Function implementations ===== */

/* A wrapper that defines GBA_CONV_CARVE_<addr> builds only that linked carve
 * (plus the inline helpers it uses); everything else stays out of its object.
 * Those units build with -opt nopeephole, matching the peephole-off state the
 * full file reaches through fn_80088428's pragma. */
#if defined(GBA_CONV_CARVE_800886D0) || defined(GBA_CONV_CARVE_800889E4) || \
    defined(GBA_CONV_CARVE_80088C60) || defined(GBA_CONV_CARVE_800884BC)
#define GBA_CONV_CARVE
#define PR424_GBA_CONV_TAIL_ONLY
#endif

#if !defined(PR424_GBA_CONV_TAIL_ONLY)

/* 0x80083AF4 | size: 0x104 */
void fn_80083AF4(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataGetStatus();
#endif
    u8 sp[0x20];
    u32 r0 = 0;
    u32 r1 = PC_PTR32(sp);
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r7 = 0;
    u32 r31 = 0;

    r31 = r4;
    if (r3 != 0x0) {
    } else {

        r3 = 0x0;
        r4 = 0xd;
        r3 = ((u32 (*)(RegWord, RegWord))savedataGetStatus)(r3, r4);
    }
    r0 = PC_PTR32(sp) + 0x8;
    r6 = r3 + 0x4000;
    if (r0 != 0x0) {
        r0 = 0x0;
        *(u32*)(sp + 0x8) = r0;
    }
    r7 = 0x0;
while (1) {
        r0 = r3 + 0x24;
        if (r6 >= r0) {
            r0 = *(u16*)((u8*)PC_PTR(r3) + 0x0);
            if (r0 != 0x0) {
                r0 = *(u8*)((u8*)PC_PTR(r3) + 0x1B);
                r0 = (s8)r0;
                if ((s32)r0 <= 0x3) {
                    r0 = *(u8*)((u8*)PC_PTR(r3) + 0x1C);
                    r0 = (s8)r0;
                    if ((s32)r0 <= 0x6) {
                        r0 = *(u8*)((u8*)PC_PTR(r3) + 0x1D);
                        r0 = (s8)r0;
                        if ((s32)r0 > 0x5) {
                }
                    }
                    r0 = 0x0;
                    *(u16*)((u8*)PC_PTR(r3) + 0x0) = r0;
            }
            }
            break;
                        }
        if ((s32)r7 == (s32)r31) {
        }
        r4 = *(u8*)((u8*)PC_PTR(r3) + 0x1C);
        r7 = r7 + 0x1;
        r0 = *(u8*)((u8*)PC_PTR(r3) + 0x1D);
        r4 = (s8)r4;
        r5 = *(u8*)((u8*)PC_PTR(r3) + 0x1B);
        r0 = (s8)r0;
        r0 = r4 * r0;
        r5 = (s8)r5;
        r4 = r0 << 4;
        r0 = r4 + 0x76;
        r0 = r5 * r0;
        r3 = r0 + r3;
        r3 = r3 + 0x24;
}

    if ((s32)r31 < 0x0) {
    }
    return;
}

/* 0x80083BF8 | size: 0xC4 */
#ifndef TARGET_PC /* copy: fn_80083BF8 is owned and scored in main/game/menu/cardesavedata_r51_80083AF4_suffix; the PC links that one */
void fn_80083BF8(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataGetStatus();
#endif
    u8 sp[0x10];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r7 = 0;

    if (r3 != 0) {
    } else {

        r3 = 0x0;
        r4 = 0xd;
        r3 = ((u32 (*)(RegWord, RegWord))savedataGetStatus)(r3, r4);
    }
    r6 = r3 + 0x4000;
    r7 = 0x0;
while (1) {
        tmp = r3 + 0x24;
        if (r6 >= tmp) {
            tmp = *(u16*)((u8*)PC_PTR(r3) + 0x0);
            if (tmp != 0) {
                tmp = *(u8*)((u8*)PC_PTR(r3) + 0x1B);
                r5 = (s8)tmp;
                if ((s32)r5 <= 3) {
                    tmp = *(u8*)((u8*)PC_PTR(r3) + 0x1C);
                    r4 = (s8)tmp;
                    if ((s32)r4 <= 6) {
                        tmp = *(u8*)((u8*)PC_PTR(r3) + 0x1D);
                        tmp = (s8)tmp;
                        if ((s32)tmp > 5) {
                }
                    }
                    tmp = 0x0;
                    *(u16*)((u8*)PC_PTR(r3) + 0x0) = tmp;
            }
            }
            break;
                        }
        tmp = r4 * tmp;
        r7 = r7 + 0x1;
        r4 = tmp << 4;
        tmp = r4 + 0x76;
        tmp = r5 * tmp;
        r3 = tmp + r3;
        r3 = r3 + 0x24;
}

    tmp = PC_PTR32(sp) + 0x8;
    if (tmp != 0) {
    }
    return;
}
#endif

/* 0x80083CBC | size: 0x40 */
void fn_80083CBC(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataGetStatus();
#endif
    u8 sp[0x10];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;

    if (r3 != 0) {
    } else {

        r3 = 0x0;
        r4 = 0xd;
        r3 = ((u32 (*)(RegWord, RegWord))savedataGetStatus)(r3, r4);
    }
    r4 = 0x0;
    r5 = 0x49cc;
    memset((void*)PC_PTR(r3), (int)r4, (u32)r5);
    return;
}

/* 0x80083CFC | size: 0x34 */
void fn_80083CFC(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataGetStatus();
#endif
    u8 sp[0x10];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;

    if (r3 != 0) {
    } else {

        r3 = 0x0;
        r4 = 0xd;
        r3 = ((u32 (*)(RegWord, RegWord))savedataGetStatus)(r3, r4);
    }
    return;
}

/* 0x80083D30 | size: 0x19C */
void fn_80083D30(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosGetPokemonWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroBiosGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80135938();
#endif
    u8 sp[0x170];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r25 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;

    r27 = r3;
    r28 = r4;
    r31 = 0x0;
    do {
        r3 = r27;
        r4 = r31 & 0xFFFF;
        r3 = ((u32 (*)(RegWord, RegWord))heroBiosGetPokemonPtr)(r3, r4);
        r30 = 0x0;
        r29 = r3;
        do {
            r3 = r29;
            r4 = r30 & 0xFFFF;
            r3 = ((u32 (*)(RegWord, RegWord))pokemonBiosGetPokemonWazaDataId)(r3, r4);
            r3 = ((u32 (*)(RegWord))wazaDataBiosGetPtr)(r3);
            r26 = r3;
            r3 = r28;
            r4 = 0x0;
            r5 = 0x50;
            memset((void*)PC_PTR(r3), (int)r4, (u32)r5);
            if (r26 != 0) {
                r3 = r26;
                r3 = ((u32 (*)(RegWord))wazaDataBiosGetDoc)(r3);
                r3 = ((u32 (*)(RegWord))GSmsgGetGSchar)(r3);
                r4 = 0x0;
                r26 = r3;
                r3 = PC_PTR32(sp) + 0xac;
                while (1) {

                    if ((s32)r4 < 0x50) {
                        tmp = r4;
                        r4 = r4 + 0x1;
                        tmp = tmp << 1;
                        *(u16*)PC_PTR((r3 + tmp)) = r5;
                    }
                    r26 = r26 + 0x2;

                    r5 = *(u16*)((u8*)PC_PTR(r26) + 0x0);
                    if (r5 != 0) {
                        if (r5 == 0xffff) break;
                }
                }
                tmp = r4 << 1;
                r4 = PC_PTR32(sp) + 0xac;
                r5 = 0x0;
                r3 = 0x0;
                *(u16*)PC_PTR((r4 + tmp)) = r5;
                r4 = 0x5;
#ifdef TARGET_PC /* gamedataGetStatus(r3 = 0, r4 = 5); its result goes on in r3 (assembly) */
                r3 = fn_80135938((struct GameData*)PC_PTR(r3), r4);
#else
                fn_80135938();
#endif
                r5 = r3;
                r3 = r28;
                r4 = PC_PTR32(sp) + 0xac;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord))GScharToGBA)(r3, r4, r5);
                tmp = *(u16*)((u8*)PC_PTR(r26) + 0x0);
                r25 = r28 + r3;
                if (tmp == 0xffff) {
                    tmp = 0xfe;
                    r6 = r26 + 0x3;
                    *(u8*)((u8*)PC_PTR(r25) + 0x0) = tmp;
                    r25 = r25 + 0x1;
                    r4 = 0x0;
                    r3 = PC_PTR32(sp) + 0x8;
                    while (1) {

                        if ((s32)r4 < 0x50) {
                            tmp = r4;
                            r4 = r4 + 0x1;
                            tmp = tmp << 1;
                            *(u16*)PC_PTR((r3 + tmp)) = r5;
                        }
                        r6 = r6 + 0x2;

                        r5 = *(u16*)((u8*)PC_PTR(r6) + 0x0);
                        if (r5 != 0) {
                            if (r5 == 0xffff) break;
                    }
                    }
                    tmp = r4 << 1;
                    r4 = PC_PTR32(sp) + 0x8;
                    r5 = 0x0;
                    r3 = 0x0;
                    *(u16*)PC_PTR((r4 + tmp)) = r5;
                    r4 = 0x5;
#ifdef TARGET_PC /* gamedataGetStatus(r3 = 0, r4 = 5); its result goes on in r3 (assembly) */
                    r3 = fn_80135938((struct GameData*)PC_PTR(r3), r4);
#else
                    fn_80135938();
#endif
                    r5 = r3;
                    r3 = r25;
                    r4 = PC_PTR32(sp) + 0x8;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord))GScharToGBA)(r3, r4, r5);
                    r25 = r25 + r3;
                }
                tmp = 0xff;
                *(u8*)((u8*)PC_PTR(r25) + 0x0) = tmp;
            }
            r28 = r28 + 0x50;
            r30 = r30 + 0x1;
        } while ((s32)r30 < 4);
        r31 = r31 + 0x1;
    } while ((s32)r31 < 6);
    r3 = 0x0;
    return;
}

/* 0x80083ECC | size: 0x168 */
void fn_80083ECC(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80135938();
#endif
    u8 sp[0x160];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r30 = 0;
    u32 r31 = 0;

    r31 = r3;
    r3 = r4;
    r3 = ((u32 (*)(RegWord))wazaDataBiosGetPtr)(r3);
    tmp = r3;
    r3 = r31;
    r30 = tmp;
    r4 = 0x0;
    r5 = 0x50;
    memset((void*)PC_PTR(r3), (int)r4, (u32)r5);
    if (r30 == 0) { r3 = 0x0; return; }
    r3 = r30;
    r3 = ((u32 (*)(RegWord))wazaDataBiosGetDoc)(r3);
    r3 = ((u32 (*)(RegWord))GSmsgGetGSchar)(r3);
    r4 = 0x0;
    r30 = r3;
    r3 = PC_PTR32(sp) + 0xac;
    while (1) {

        if ((s32)r4 < 0x50) {
            tmp = r4;
            r4 = r4 + 0x1;
            tmp = tmp << 1;
            *(u16*)PC_PTR((r3 + tmp)) = r5;
        }
        r30 = r30 + 0x2;

        r5 = *(u16*)((u8*)PC_PTR(r30) + 0x0);
        if (r5 != 0) {
            if (r5 == 0xffff) break;
    }
    }
    tmp = r4 << 1;
    r4 = PC_PTR32(sp) + 0xac;
    r5 = 0x0;
    r3 = 0x0;
    *(u16*)PC_PTR((r4 + tmp)) = r5;
    r4 = 0x5;
#ifdef TARGET_PC /* gamedataGetStatus(r3 = 0, r4 = 5); its result goes on in r3 (assembly) */
    r3 = fn_80135938((struct GameData*)PC_PTR(r3), r4);
#else
    fn_80135938();
#endif
    r5 = r3;
    r3 = r31;
    r4 = PC_PTR32(sp) + 0xac;
    r3 = ((u32 (*)(RegWord, RegWord, RegWord))GScharToGBA)(r3, r4, r5);
    tmp = *(u16*)((u8*)PC_PTR(r30) + 0x0);
    r31 = r31 + r3;
    if (tmp == 0xffff) {
        tmp = 0xfe;
        r6 = r30 + 0x3;
        *(u8*)((u8*)PC_PTR(r31) + 0x0) = tmp;
        r31 = r31 + 0x1;
        r4 = 0x0;
        r3 = PC_PTR32(sp) + 0x8;
        while (1) {

            if ((s32)r4 < 0x50) {
                tmp = r4;
                r4 = r4 + 0x1;
                tmp = tmp << 1;
                *(u16*)PC_PTR((r3 + tmp)) = r5;
            }
            r6 = r6 + 0x2;

            r5 = *(u16*)((u8*)PC_PTR(r6) + 0x0);
            if (r5 != 0) {
                if (r5 == 0xffff) break;
        }
        }
        tmp = r4 << 1;
        r4 = PC_PTR32(sp) + 0x8;
        r5 = 0x0;
        r3 = 0x0;
        *(u16*)PC_PTR((r4 + tmp)) = r5;
        r4 = 0x5;
#ifdef TARGET_PC /* gamedataGetStatus(r3 = 0, r4 = 5); its result goes on in r3 (assembly) */
        r3 = fn_80135938((struct GameData*)PC_PTR(r3), r4);
#else
        fn_80135938();
#endif
        r5 = r3;
        r3 = r31;
        r4 = PC_PTR32(sp) + 0x8;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord))GScharToGBA)(r3, r4, r5);
        r31 = r31 + r3;
    }
    tmp = 0xff;
    *(u8*)((u8*)PC_PTR(r31) + 0x0) = tmp;

    r3 = 0x0;
    return;
}

/* 0x80084034 | size: 0x4 */
void fn_80084034(void) {
}

/* 0x80084038 | size: 0x97C */
void fn_80084038(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void __assert();
#endif
    u8 sp[0x30];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r23 = 0;
    u32 r24 = 0;
    u32 r25 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;
    f32 f0 = 0.0f;
    f32 f1 = 0.0f;
    f32 f2 = 0.0f;
    f32 f3 = 0.0f;
    f32 f4 = 0.0f;
    f32 f5 = 0.0f;
    f32 f6 = 0.0f;
    f32 f7 = 0.0f;

    r29 = r3;
    r4 = PC_PTR32(&lbl_8026F2E8);
    r25 = PC_PTR32(&lbl_8026F2E8);
    if (r29 == 0) {
        r3 = 0xa6;
        r3 = ((u32 (*)(RegWord))windowSearchID)(r3);
    }
    ((void(*)(void))windowGetFreeWork)();
    tmp = *(u8*)((u8*)PC_PTR(r29) + 0x1);
    r26 = *(u32*)((u8*)PC_PTR(r3) + 0x0);
    tmp = (s8)tmp;
    r30 = r26;
    if ((s32)tmp != 3) {
        if ((s32)tmp < 3) {
            if ((s32)tmp != 0) {
                goto L_80084458;
            }
            if ((s32)tmp != 5) {
                goto L_80084458;
                }
            tmp = *(u8*)((u8*)PC_PTR(r29) + 0x2);
            tmp = (s8)tmp;
            if ((s32)tmp == 0) {
                r3 = 0xe0;
                r4 = 0x20;
                r3 = ((u32 (*)(RegWord, RegWord))GSmemAlloc)(r3, r4);
                r26 = r3;
                tmp = r26 & 0xFFFF;
                if (tmp == 0) {
                    r3 = r25 + 0x184;
                    r4 = 0xea;
                    r5 = PC_PTR32(&lbl_8047C198);
                    ((void (*)(RegWord, RegWord, RegWord))__assert)(r3, r4, r5);
                }
                r3 = r26;
                r3 = ((u32 (*)(RegWord))GSmemLock)(r3);
                r4 = 0x0;
                r26 = r3;
                r5 = 0xcc;
                memset((void*)PC_PTR(r3), (int)r4, (u32)r5);
                r30 = r26;
                r3 = r29;
                r3 = ((u32 (*)(RegWord))windowGetFreeWork)(r3);
                *(u32*)((u8*)PC_PTR(r3) + 0x0) = r26;
                r5 = 0x0;
                tmp = 0x1;
                r3 = r29;
                *(u32*)((u8*)PC_PTR(r26) + 0x10) = r5;
                r4 = 0x10f6;
                *(u32*)((u8*)PC_PTR(r26) + 0x0) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0x14) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0x4) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0x18) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0x8) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0x1C) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0xC) = r5;
                *(u8*)((u8*)PC_PTR(r26) + 0x20) = tmp;
                *(u8*)((u8*)PC_PTR(r26) + 0x21) = tmp;
                *(u32*)((u8*)PC_PTR(r26) + 0x24) = r5;
                *(u32*)((u8*)PC_PTR(r26) + 0x28) = r5;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x30) = r3;
                r3 = r29;
                r4 = 0x10f7;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x34) = r3;
                r3 = r29;
                r4 = 0x10d5;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x38) = r3;
                r3 = r29;
                r4 = 0x10da;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x3C) = r3;
                r3 = r29;
                r4 = 0x10e3;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x40) = r3;
                r3 = r29;
                r4 = 0x10f0;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x44) = r3;
                r3 = r29;
                r4 = 0x10f5;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x48) = r3;
                r3 = r29;
                r4 = 0x10d1;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x4C) = r3;
                r3 = r29;
                r4 = 0x10d6;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x5C) = r3;
                r3 = r29;
                r4 = 0x10db;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x6C) = r3;
                r3 = r29;
                r4 = 0x10df;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x7C) = r3;
                r3 = r29;
                r4 = 0x10e4;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x8C) = r3;
                r3 = r29;
                r4 = 0x10e8;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x9C) = r3;
                r3 = r29;
                r4 = 0x10ec;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xAC) = r3;
                r3 = r29;
                r4 = 0x10f1;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xBC) = r3;
                r3 = r29;
                r4 = 0x10d2;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x50) = r3;
                r3 = r29;
                r4 = 0x10d7;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x60) = r3;
                r3 = r29;
                r4 = 0x10dc;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x70) = r3;
                r3 = r29;
                r4 = 0x10e0;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x80) = r3;
                r3 = r29;
                r4 = 0x10e5;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x90) = r3;
                r3 = r29;
                r4 = 0x10e9;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xA0) = r3;
                r3 = r29;
                r4 = 0x10ed;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xB0) = r3;
                r3 = r29;
                r4 = 0x10f2;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xC0) = r3;
                r3 = r29;
                r4 = 0x10d3;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x54) = r3;
                r3 = r29;
                r4 = 0x10d8;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x64) = r3;
                r3 = r29;
                r4 = 0x10dd;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x74) = r3;
                r3 = r29;
                r4 = 0x10e1;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x84) = r3;
                r3 = r29;
                r4 = 0x10e6;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x94) = r3;
                r3 = r29;
                r4 = 0x10ea;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xA4) = r3;
                r3 = r29;
                r4 = 0x10ee;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xB4) = r3;
                r3 = r29;
                r4 = 0x10f3;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xC4) = r3;
                r3 = r29;
                r4 = 0x10d4;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x58) = r3;
                r3 = r29;
                r4 = 0x10d9;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x68) = r3;
                r3 = r29;
                r4 = 0x10de;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x78) = r3;
                r3 = r29;
                r4 = 0x10e2;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x88) = r3;
                r3 = r29;
                r4 = 0x10e7;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0x98) = r3;
                r3 = r29;
                r4 = 0x10eb;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xA8) = r3;
                r3 = r29;
                r4 = 0x10ef;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xB8) = r3;
                r3 = r29;
                r4 = 0x10f4;
                r3 = ((u32 (*)(RegWord, RegWord))windowSearchItemID)(r3, r4);
                *(u32*)((u8*)PC_PTR(r26) + 0xC8) = r3;
                r24 = r25 + 0x70;
                r26 = 0x0;
                do {
                    r4 = *(u16*)((u8*)PC_PTR(r24) + 0x0);
                    r3 = r29;
                    r5 = *(u16*)((u8*)PC_PTR(r24) + 0x2);
                    ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
                    r24 = r24 + 0x6;
                    r26 = r26 + 0x1;
                } while (r26 < 0x2e);
            }
            goto L_80084458;
        }
        tmp = *(u8*)((u8*)PC_PTR(r29) + 0x2);
        tmp = (s8)tmp;
        if ((s32)tmp == 0) {
            r24 = r25 + 0x70;
            r27 = 0x0;
            do {
                r4 = *(u16*)((u8*)PC_PTR(r24) + 0x0);
                r3 = r29;
                r5 = *(u16*)((u8*)PC_PTR(r24) + 0x4);
                ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
                r24 = r24 + 0x6;
                r27 = r27 + 0x1;
            } while (r27 < 0x2e);
            tmp = 0x1;
            *(u8*)((u8*)PC_PTR(r29) + 0x2) = tmp;
            *(u8*)((u8*)PC_PTR(r26) + 0x20) = tmp;
        }
        goto L_80084458;
            }
    r3 = r26;
    r3 = ((u32 (*)(RegWord))GSmemGetHandleByPtr)(r3);
    r26 = r3;
    tmp = r26 & 0xFFFF;
    if (tmp == 0) {
        r3 = r25 + 0x184;
        r4 = 0xf3;
        r5 = PC_PTR32(&lbl_8047C198);
        ((void (*)(RegWord, RegWord, RegWord))__assert)(r3, r4, r5);
    }
    r3 = r26;
    r3 = ((u32 (*)(RegWord))fn_800E24B0)(r3);
    r3 = r26;
    r3 = ((u32 (*)(RegWord))GSmemFree)(r3);
    return;
L_80084458:
    tmp = *(u8*)((u8*)PC_PTR(r29) + 0x1);
    r27 = 0x0;
    tmp = (s8)tmp;
    if ((s32)tmp == 2) {
        tmp = *(u8*)((u8*)PC_PTR(r30) + 0x20);
        if (tmp != 0) {
            tmp = 0x0;
            r27 = 0x1;
            *(u8*)((u8*)PC_PTR(r30) + 0x20) = tmp;
    }
    }
    r24 = r30;
    r31 = 0x0;
    r23 = PC_PTR32(&lbl_80478950);
    r28 = 0x7;
    r26 = 0x8;
    do {
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
        if ((s32)tmp != 5) {
            if ((s32)tmp == 4) {
            }
            r3 = r31 + 0x1;
            r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                r3 = ((u32 (*)(void))menuGetEnablePort)();
                tmp = *(u8*)((u8*)PC_PTR(r23) + 0x0);
                tmp = r3 & ~tmp;
                r3 = tmp & 0xFF;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                *(u32*)((u8*)PC_PTR(r24) + 0x0) = r28;
                *(u32*)((u8*)PC_PTR(r30) + 0x28) = r26;
            }
            }
        r24 = r24 + 0x4;
        r23 = r23 + 0x1;
        r31 = r31 + 0x1;
    } while ((s32)r31 <= 3);
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x10);
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x0);
    if ((s32)r3 != (s32)tmp) {
        r27 = 0x1;
    }
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x0);
    *(u32*)((u8*)PC_PTR(r30) + 0x10) = tmp;
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x14);
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x4);
    if ((s32)r3 != (s32)tmp) {
        r27 = 0x1;
    }
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x4);
    *(u32*)((u8*)PC_PTR(r30) + 0x14) = tmp;
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x18);
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x8);
    if ((s32)r3 != (s32)tmp) {
        r27 = 0x1;
    }
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x8);
    *(u32*)((u8*)PC_PTR(r30) + 0x18) = tmp;
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x1C);
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0xC);
    if ((s32)r3 != (s32)tmp) {
        r27 = 0x1;
    }
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0xC);
    r26 = 0x4;
    *(u32*)((u8*)PC_PTR(r30) + 0x1C) = tmp;
    tmp = r27 & 0xFF;
    if (tmp == 0) return;
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x30);
    if (r3 != 0) {
        r4 = 0x1;
        ((void(*)(void))winSpriteSetDisp)();
    }
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x34);
    if (r3 != 0) {
        r4 = 0x1;
        ((void(*)(void))winSpriteSetDisp)();
    }
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x0);
    r4 = r25 + 0x0;
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x38);
    tmp = tmp << 2;
    r27 = *(u32*)PC_PTR((r4 + tmp));
    if (r3 != 0) {
        r4 = r27 & 0x00000100;
        tmp = -r4;
        tmp = tmp | r4;
        r4 = (u32)tmp >> 31;
        ((void(*)(void))winSpriteSetDisp)();
    }
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x3C);
    if (r3 != 0) {
        r4 = r27 & 0x00000200;
        tmp = -r4;
        tmp = tmp | r4;
        r4 = (u32)tmp >> 31;
        ((void(*)(void))winSpriteSetDisp)();
    }
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x40);
    if (r3 != 0) {
        r4 = r27 & 0x00000400;
        tmp = -r4;
        tmp = tmp | r4;
        r4 = (u32)tmp >> 31;
        ((void(*)(void))winSpriteSetDisp)();
    }
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x44);
    if (r3 != 0) {
        r4 = r27 & 0x00000800;
        tmp = -r4;
        tmp = tmp | r4;
        r4 = (u32)tmp >> 31;
        ((void(*)(void))winSpriteSetDisp)();
    }
    r3 = *(u32*)((u8*)PC_PTR(r30) + 0x48);
    if (r3 != 0) {
        r4 = r27 & 0x00001000;
        tmp = -r4;
        tmp = tmp | r4;
        r4 = (u32)tmp >> 31;
        ((void(*)(void))winSpriteSetDisp)();
    }
    tmp = *(u8*)((u8*)PC_PTR(r30) + 0x20);
    if (tmp == 0) {
        tmp = r26 << 2;
        r26 = *(u32*)((u8*)PC_PTR(r30) + 0x38);
        tmp = *(u32*)PC_PTR((r30 + tmp));
        tmp = 0x9 - tmp;
        tmp = __cntlzw(tmp);
        tmp = (u32)tmp >> 5;
        r23 = tmp & 0xFF;
        if (r26 != 0) {
            if ((s32)r23 != 0) {
                tmp = *(s16*)((u8*)PC_PTR(r26) + 0x6);
                r3 = r29;
                r5 = 0x1ba;
                r4 = tmp & 0xFFFF;
                ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
        }

            } else {
        tmp = *(s16*)((u8*)PC_PTR(r26) + 0x6);
        r3 = r29;
        r5 = 0x0;
        r4 = tmp & 0xFFFF;
        ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
        tmp = -0x1;
        *(u32*)((u8*)PC_PTR(r26) + 0x64) = tmp;
            }
        r26 = *(u32*)((u8*)PC_PTR(r30) + 0x40);
        if (r26 != 0) {
            if ((s32)r23 != 0) {
                tmp = *(s16*)((u8*)PC_PTR(r26) + 0x6);
                r3 = r29;
                r5 = 0x1ba;
                r4 = tmp & 0xFFFF;
                ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
        }
        }

            } else {
    tmp = *(s16*)((u8*)PC_PTR(r26) + 0x6);
    r3 = r29;
    r5 = 0x0;
    r4 = tmp & 0xFFFF;
    ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
    tmp = -0x1;
    *(u32*)((u8*)PC_PTR(r26) + 0x64) = tmp;
            }
    r31 = r30;
    r27 = 0x0;
    r26 = PC_PTR32(&lbl_8047C190);
    r25 = r25 + 0x0;
    do {
        tmp = *(u32*)((u8*)PC_PTR(r31) + 0x0);
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0x4C);
        tmp = tmp << 2;
        r28 = *(u32*)PC_PTR((r25 + tmp));
        if (r3 != 0) {
            r4 = r28 & 0x1;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0x5C);
        if (r3 != 0) {
            r4 = r28 & 0x00000002;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0x6C);
        if (r3 != 0) {
            r4 = r28 & 0x00000004;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0x7C);
        if (r3 != 0) {
            r4 = r28 & 0x00000008;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0x8C);
        if (r3 != 0) {
            r4 = r28 & 0x00000010;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0x9C);
        if (r3 != 0) {
            r4 = r28 & 0x00000020;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0xAC);
        if (r3 != 0) {
            r4 = r28 & 0x00000040;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r31) + 0xBC);
        if (r3 != 0) {
            r4 = r28 & 0x00000080;
            tmp = -r4;
            tmp = tmp | r4;
            r4 = (u32)tmp >> 31;
            ((void(*)(void))winSpriteSetDisp)();
        }
        tmp = *(u32*)((u8*)PC_PTR(r31) + 0x0);
        if ((s32)tmp != 0xb) {
            if ((s32)tmp >= 0xb || (s32)tmp >= 8) goto L_80084864;

            if ((s32)tmp < 6) {
                goto L_80084864;
            }
        }
        r3 = *(u32*)((u8*)PC_PTR(r30) + 0x30);
        if (r3 != 0) {
            r4 = 0x0;
            ((void(*)(void))winSpriteSetDisp)();
        }
        r3 = *(u32*)((u8*)PC_PTR(r30) + 0x34);
        if (r3 != 0) {
            r4 = 0x0;
            ((void(*)(void))winSpriteSetDisp)();
        }
    L_80084864:
        tmp = *(u8*)((u8*)PC_PTR(r30) + 0x20);
        if (tmp == 0) {
            tmp = *(u32*)((u8*)PC_PTR(r31) + 0x0);
            r24 = *(u32*)((u8*)PC_PTR(r31) + 0x4C);
            tmp = 0x2 - tmp;
            tmp = __cntlzw(tmp);
            tmp = (u32)tmp >> 5;
            r23 = tmp & 0xFF;
            if (r24 != 0) {
                if ((s32)r23 != 0) {
                    tmp = *(s16*)((u8*)PC_PTR(r24) + 0x6);
                    r3 = r29;
                    r5 = 0x1ba;
                    r4 = tmp & 0xFFFF;
                    ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
                }

                } else {
            tmp = *(s16*)((u8*)PC_PTR(r24) + 0x6);
            r3 = r29;
            r5 = 0x0;
            r4 = tmp & 0xFFFF;
            ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
            tmp = -0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x64) = tmp;
                }
            r24 = *(u32*)((u8*)PC_PTR(r31) + 0x7C);
            if (r24 != 0) {
                if ((s32)r23 != 0) {
                    tmp = *(s16*)((u8*)PC_PTR(r24) + 0x6);
                    r3 = r29;
                    r5 = 0x1ba;
                    r4 = tmp & 0xFFFF;
                    ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
                }

                } else {
            tmp = *(s16*)((u8*)PC_PTR(r24) + 0x6);
            r3 = r29;
            r5 = 0x0;
            r4 = tmp & 0xFFFF;
            ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
            tmp = -0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x64) = tmp;
                }
            r24 = *(u32*)((u8*)PC_PTR(r31) + 0x8C);
            if (r24 != 0) {
                if ((s32)r23 != 0) {
                    tmp = *(s16*)((u8*)PC_PTR(r24) + 0x6);
                    r3 = r29;
                    r5 = 0x1ba;
                    r4 = tmp & 0xFFFF;
                    ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
                }

                } else {
            tmp = *(s16*)((u8*)PC_PTR(r24) + 0x6);
            r3 = r29;
            r5 = 0x0;
            r4 = tmp & 0xFFFF;
            ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
            tmp = -0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x64) = tmp;
                }
            tmp = r28 & 0x00000004;
            if ((s32)tmp != 0) {
                r3 = *(u32*)((u8*)PC_PTR(r31) + 0x6C);
                tmp = *(u32*)((u8*)PC_PTR(r3) + 0xC);
                if (tmp == 0) {
                    tmp = *(s16*)((u8*)PC_PTR(r3) + 0x6);
                    r3 = r29;
                    r5 = *(u16*)((u8*)PC_PTR(r26) + 0x0);
                    r4 = tmp & 0xFFFF;
                    ((void (*)(RegWord, RegWord, RegWord))fn_801081F8)(r3, r4, r5);
            }
            }
        }
        r31 = r31 + 0x4;
        r26 = r26 + 0x2;
        r27 = r27 + 0x1;
    } while ((s32)r27 <= 3);

    return;
}

/* 0x800849B4 | size: 0xD8 */
#ifndef TARGET_PC /* copy: fn_800849B4 is owned and scored in main/game/menu/cardesavedata_exact_800849B4; the PC links that one */
void fn_800849B4(void) {
    extern void cardeGbaTransfer();
    u8 sp[0x20];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;

    r30 = r3;
    r28 = r4;
    r29 = r5;
    r31 = r6;
    r3 = 0x1;
    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
    tmp = r3;
    r3 = r30;
    r30 = tmp;
    r4 = r28;
    r5 = r29;
    r6 = r31;
    cardeGbaTransfer();
    r31 = r3;
    r3 = 0x0;
    ((void (*)(RegWord))winMsgClose)(r3);
    r3 = 0xe4;
    r3 = ((u32 (*)(RegWord))menuIsCheck)(r3);
    tmp = r3 & 0xFF;
    if (tmp != 0) {
        r3 = 0xe4;
        r4 = 0x0;
        r5 = 0x1;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord))menuCloseCustom)(r3, r4, r5);
    }
    r3 = r30;
    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
    r30 = 0x0;
    do {
        r3 = r30;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        r30 = r30 + 0x1;
    } while ((s32)r30 < 3);
    tmp = r31 & 0xFF;
    if (tmp != 0) {
        r3 = 0x0;
    } else {

        r3 = -0x1;
    }
    return;
}
#endif

/* 0x80084A8C | size: 0x305C */
#ifndef TARGET_PC /* copy: cardeGbaTransfer is owned and scored in main/game/menu/cardesavedata_candidate_80084A8C_o3; the PC links that one */
void cardeGbaTransfer(void) {
    extern void fn_80087AE8();
    extern void savedataBiosGetGamedataPtr();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataBiosGetNowSavedataPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataGetStatus();
#endif
    extern void heroInit();
    extern void heroBiosCopy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gamedataAttestBiosGetLangareaId();
#endif
    extern void gamedataBiosGetGamedataAtttestPtr();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void __assert();
#endif
    extern u8 jumptable_802EEB78[];
    u8 sp[0xCD0];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r15 = 0;
    u32 r16 = 0;
    u32 r17 = 0;
    u32 r18 = 0;
    u32 r19 = 0;
    u32 r20 = 0;
    u32 r21 = 0;
    u32 r22 = 0;
    u32 r23 = 0;
    u32 r24 = 0;
    u32 r25 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;
    f32 f0 = 0.0f;
    f32 f4 = 0.0f;
    f32 f27 = 0.0f;
    f32 f28 = 0.0f;
    f32 f29 = 0.0f;
    f32 f30 = 0.0f;
    f32 f31 = 0.0f;
    void (*ctr_fn)(void) = 0;

    r3 = PC_PTR32(&lbl_8026F2E8);
    r15 = 0x0;
    tmp = PC_PTR32(&lbl_8026F2E8);
    *(u32*)(sp + 0xC34) = tmp;
    r3 = 0x1;
    r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
    while (1) {
        r5 = 0x0;
        r4 = r3 + 0x190;
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord))gbaCommStartTransferFile)(r3, r4, r5);
        if ((s32)r3 != 0) break;
        ((void (*)(void))_threadSwitch)();


    }
    r3 = 0xe4;
    r4 = 0x0;
    ((void (*)(RegWord, RegWord))menuOpen)(r3, r4);
    r3 = 0xe4;
    r3 = ((u32 (*)(RegWord))windowSearchID)(r3);
    r16 = r3;
    if (r16 == 0) {
        r4 = 0x1f4;
        r5 = PC_PTR32(&lbl_8047C1A0);
        r3 = r3 + 0x184;
        ((void (*)(RegWord, RegWord, RegWord))__assert)(r3, r4, r5);
    }
    if (r16 == 0) {
        r3 = 0xa6;
        r3 = ((u32 (*)(RegWord))windowSearchID)(r3);
        r16 = r3;
    }
    r3 = r16;
    r3 = ((u32 (*)(RegWord))windowGetFreeWork)(r3);
    r5 = 0x0;
    r16 = *(u32*)((u8*)PC_PTR(r3) + 0x0);
    r30 = PC_PTR32(&lbl_80478954);
    r4 = tmp << 4;
    *(u32*)((u8*)PC_PTR(r16) + 0x24) = tmp;
    r29 = r3 + 0x30;
    r29 = r29 + r4;
    *(u32*)((u8*)PC_PTR(r16) + 0x2C) = tmp;
    tmp = *(u8*)&lbl_80478954;
    *(u8*)((u8*)PC_PTR(r16) + 0x21) = tmp;
    tmp = 0x5;
    r3 = *(u8*)((u8*)PC_PTR(r30) + 0x0);
    r3 = (s8)r3;
    r5 = r3 << 2;
    r3 = *(u32*)PC_PTR((r29 + r5));
    *(u32*)PC_PTR((r16 + r5)) = r3;
    if ((s32)r5 < 0) {
        r3 = *(u32*)PC_PTR((r16 + r5));
        if ((s32)r3 == 1) {
            *(u32*)PC_PTR((r16 + r5)) = tmp;
    }
    }
    r4 = r30 + 0x1;
    r5 = 0x1;
    r3 = *(u8*)((u8*)PC_PTR(r4) + 0x0);
    r3 = (s8)r3;
    r5 = r3 << 2;
    r3 = *(u32*)PC_PTR((r29 + r5));
    *(u32*)PC_PTR((r16 + r5)) = r3;
    if ((s32)r5 < 0) {
        r3 = *(u32*)PC_PTR((r16 + r5));
        if ((s32)r3 == 1) {
            *(u32*)PC_PTR((r16 + r5)) = tmp;
    }
    }
    r4 = r4 + 0x1;
    r5 = 0x2;
    r3 = *(u8*)((u8*)PC_PTR(r4) + 0x0);
    r3 = (s8)r3;
    r5 = r3 << 2;
    r3 = *(u32*)PC_PTR((r29 + r5));
    *(u32*)PC_PTR((r16 + r5)) = r3;
    if ((s32)r5 < 0) {
        r3 = *(u32*)PC_PTR((r16 + r5));
        if ((s32)r3 == 1) {
            *(u32*)PC_PTR((r16 + r5)) = tmp;
    }
    }
    r4 = r4 + 0x1;
    r5 = 0x3;
    r3 = *(u8*)((u8*)PC_PTR(r4) + 0x0);
    r3 = (s8)r3;
    r5 = r3 << 2;
    r3 = *(u32*)PC_PTR((r29 + r5));
    *(u32*)PC_PTR((r16 + r5)) = r3;
    if ((s32)r5 < 0) {
        r3 = *(u32*)PC_PTR((r16 + r5));
        if ((s32)r3 == 1) {
            *(u32*)PC_PTR((r16 + r5)) = tmp;
    }
    }
    r24 = r16;
    tmp = tmp & 0x00000010;
    if (tmp != 0) {
        if ((s32)tmp != 0) {
        }
        if ((s32)tmp == 2) {
            }
        r3 = 0x0;
        r4 = 0x2;
        r3 = ((u32 (*)(RegWord, RegWord))savedataGetStatus)(r3, r4);
        r4 = 0x0;
        *(u8*)((u8*)PC_PTR(r16) + 0x21) = r4;
        r4 = 0x8;
        tmp = tmp & 0x00000002;
        r18 = r3;
        *(u32*)((u8*)PC_PTR(r16) + 0x0) = r4;
        if (tmp != 0) {
            r3 = 0x2f;
            r4 = 0x1;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x3d88;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            f27 = *(f32*)&lbl_8047C1A8;
            f31 = *(f64*)&lbl_8047C1B0;
            r17 = 0x43300000;
            f29 = *(f64*)&lbl_8047C1B8;
            f28 = *(f32*)&lbl_8047C1AC;
            while (f27 < f28) {

                ((void (*)(void))_threadSwitch)();
                r3 = ((u32 (*)(void))fn_800D37CC)();
                *(u32*)(sp + 0xC14) = tmp;
                f30 = f0 - f31;
                r3 = ((u32 (*)(void))GSgetFrameStep)();
                f0 = f0 - f29;
                f0 = f0 / f30;
                f27 = f27 + f0;

            }
            r3 = r18;
            r3 = ((u32 (*)(RegWord))menuRuleCheckPartyAndRule)(r3);
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                r3 = 0xe4;
                r4 = 0x0;
                r5 = 0x1;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord))menuCloseCustom)(r3, r4, r5);
                r3 = r18;
                r4 = 0x0;
                ((void (*)(RegWord, RegWord))fn_8005CF2C)(r3, r4);
                r3 = 0x0;
                return;
        }
        }
        if (tmp != 0) {
            r3 = tmp;
            r3 = *(u32*)((u8*)PC_PTR(r3) + 0x0);
            if (r3 != 0) {
                r4 = r18;
                heroBiosCopy();
        }
        }
        tmp = 0xa;
        *(u32*)((u8*)PC_PTR(r16) + 0x0) = tmp;
        }
    r25 = 0x0;
    tmp = tmp & 0x00000040;
    *(u32*)(sp + 0xC2C) = tmp;
    tmp = tmp & 0x1;
    *(u32*)(sp + 0xC28) = tmp;
    tmp = tmp & 0x00000002;
    *(u32*)(sp + 0xC24) = tmp;
    tmp = tmp & 0x00000008;
    *(u32*)(sp + 0xC20) = tmp;
    r31 = tmp & 0x00000020;
    do {
        r23 = *(u8*)((u8*)PC_PTR(r3) + 0x0);
        *(u8*)((u8*)PC_PTR(r24) + 0x21) = r23;
        if (tmp != 0) {
            tmp = (s8)r23;
            tmp = tmp << 2;
            tmp = *(u32*)PC_PTR((r3 + tmp));
            if (tmp != 0) {
                r22 = tmp;
                goto L_80084E14;
            }
        }
        r22 = PC_PTR32(sp) + 0xf4;
    L_80084E14:
        if (tmp != 0) {
            tmp = (s8)r23;
            if ((s32)tmp == 1) {
                goto L_80084E38;
            }
        }
        r21 = PC_PTR32(sp) + 0x1c;
    L_80084E38:
        r28 = (s8)r23;
        r26 = r28 + 0x1;
        r27 = r28 << 2;
    L_80084E44:
        r3 = ((u32 (*)(void))menuGetEnablePort)();
        r4 = PC_PTR32(&lbl_80478950);
        tmp = *(u8*)PC_PTR((r4 + r28));
        tmp = r3 & ~tmp;
        r3 = tmp & 0xFF;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
        if (tmp != 4) {
            tmp = 0x2;
            r4 = r26;
            *(u32*)PC_PTR((r27 + r24)) = tmp;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x3c42;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            r3 = r24;
            r4 = 0x6;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r15 = 0x0;
            L_80084EB4:
                r16 = 0x0;
                r18 = r24;
                r17 = r16;
                r19 = PC_PTR32(&lbl_80478950);
                do {
                    tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                    if ((s32)tmp != 5) {
                        if ((s32)tmp == 4) {
                        }
                        r3 = r17 + 0x1;
                        r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                        tmp = r3 & 0xFF;
                        if (tmp == 0) {
                            r3 = ((u32 (*)(void))menuGetEnablePort)();
                            tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                            tmp = r3 & ~tmp;
                            r3 = tmp & 0xFF;
                            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                            r3 = 0x7;
                            tmp = 0x8;
                            *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                        }
                    tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                    if ((s32)tmp == 7) {
                        r16 = 0x1;
                    }
                    r18 = r18 + 0x4;
                    r19 = r19 + 0x1;
                    r17 = r17 + 0x1;
                } while ((s32)r17 <= 3);
                tmp = r16 & 0xFF;
                if (tmp == 0) {
                    ((void (*)(void))_threadSwitch)();
                    r15 = r15 + 0x1;
                    if ((s32)r15 < 0xf) goto L_80084EB4;
                }
                r3 = 0x26;
                ((void (*)(RegWord))soundStartSE)(r3);
                r15 = 0x0;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
                if ((s32)tmp != 7) {
                    r15 = 0x1;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                    if ((s32)tmp != 7) {
                        r15 = 0x2;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                        if ((s32)tmp != 7) {
                            r15 = 0x3;
                            tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                            if ((s32)tmp != 7) {
                                r15 = 0x4;
                }
                }
                }
                }
                if ((s32)r15 > 3) {
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                    tmp = tmp & 0x00000008;
                    if (tmp == 0) {
                        r3 = 0x0;
                        goto L_80085010;
                }
                }
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r4 = r15 + 0x1;
                r3 = 0x2f;
                ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
                if ((s32)r15 == 0) {
                    r3 = 0x7;
                    r4 = 0x44c0;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44b8;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                tmp = 0x8;
                r3 = 0x1;
                *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            L_80085010:
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                    r3 = 0x6;
                    tmp = (s8)tmp;
                    tmp = tmp << 2;
                    *(u32*)PC_PTR((r24 + tmp)) = r3;
                }
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
                if ((s32)tmp == 3) {
                    r3 = 0x7;
                    r4 = 0x44e7;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44e6;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = (s8)r3;
                r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
                r3 = 0x0;
                return;
            }
        }
        r4 = r26;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        r3 = 0x7;
        r4 = 0x3c43;
        r5 = 0x0;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        tmp = r15 & 0xFF;
        r3 = 0x3;
        *(u32*)PC_PTR((r27 + r24)) = r3;
        if (tmp == 0) {
            tmp = 0x0;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            goto L_80085114;
        L_800850BC:
            r3 = 0x10c;
            r3 = ((u32 (*)(RegWord))menuIsCheck)(r3);
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                ((void (*)(void))_threadSwitch)();
                goto L_80085114;
            }
            r3 = ((u32 (*)(void))windowGetKeyInfo)();
            tmp = *(u16*)((u8*)PC_PTR(r3) + 0x4);
            tmp = tmp & 0x00000020;
            if ((s32)tmp != 0) {
                tmp = 0x2;
                r3 = 0xe;
                *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                goto L_8008513C;
            }
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
            if (tmp == 8) {
                r3 = 0xe;
                goto L_8008513C;
            }
            ((void (*)(void))_threadSwitch)();
        L_80085114:
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommIsBusy)(r3);
            if ((s32)r3 == 0) goto L_800850BC;
            tmp = 0x0;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))fn_80093610)(r3);
        L_8008513C:
            if ((s32)r3 == 0xe) {
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r15 = 0x0;
            L_80085150:
                r16 = 0x0;
                r18 = r24;
                r17 = r16;
                r19 = PC_PTR32(&lbl_80478950);
                do {
                    tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                    if ((s32)tmp != 5) {
                        if ((s32)tmp == 4) {
                        }
                        r3 = r17 + 0x1;
                        r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                        tmp = r3 & 0xFF;
                        if (tmp == 0) {
                            r3 = ((u32 (*)(void))menuGetEnablePort)();
                            tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                            tmp = r3 & ~tmp;
                            r3 = tmp & 0xFF;
                            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                            r3 = 0x7;
                            tmp = 0x8;
                            *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                        }
                        }
                    tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                    if ((s32)tmp == 7) {
                        r16 = 0x1;
                    }
                    r18 = r18 + 0x4;
                    r19 = r19 + 0x1;
                    r17 = r17 + 0x1;
                } while ((s32)r17 <= 3);
                tmp = r16 & 0xFF;
                if (tmp == 0) {
                    ((void (*)(void))_threadSwitch)();
                    r15 = r15 + 0x1;
                    if ((s32)r15 < 0xf) goto L_80085150;
                }
                r3 = 0x26;
                ((void (*)(RegWord))soundStartSE)(r3);
                r15 = 0x0;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
                if ((s32)tmp != 7) {
                    r15 = 0x1;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                    if ((s32)tmp != 7) {
                        r15 = 0x2;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                        if ((s32)tmp != 7) {
                            r15 = 0x3;
                            tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                            if ((s32)tmp != 7) {
                                r15 = 0x4;
                }
                }
                }
                }
                if ((s32)r15 > 3) {
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                    tmp = tmp & 0x00000008;
                    if (tmp == 0) {
                        r3 = 0x0;
                        goto L_800852AC;
                    }
                }
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r4 = r15 + 0x1;
                r3 = 0x2f;
                ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
                if ((s32)r15 == 0) {
                    r3 = 0x7;
                    r4 = 0x44c0;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44b8;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                tmp = 0x8;
                r3 = 0x1;
                *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            L_800852AC:
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                    r3 = 0x6;
                    tmp = (s8)tmp;
                    tmp = tmp << 2;
                    *(u32*)PC_PTR((r24 + tmp)) = r3;
                }
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
                if ((s32)tmp == 3) {
                    r3 = 0x7;
                    r4 = 0x44e7;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44e6;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = (s8)r3;
                r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
                r3 = 0x0;
                return;
            }
            r15 = 0x1;
        }
        r3 = r28;
        r4 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord))gbaCommStartTransferC)(r3, r4);
        tmp = 0x0;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        goto L_8008538C;
    L_80085334:
        r3 = 0x10c;
        r3 = ((u32 (*)(RegWord))menuIsCheck)(r3);
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            ((void (*)(void))_threadSwitch)();
            goto L_8008538C;
        }
        r3 = ((u32 (*)(void))windowGetKeyInfo)();
        tmp = *(u16*)((u8*)PC_PTR(r3) + 0x4);
        tmp = tmp & 0x00000020;
        if ((s32)tmp != 0) {
            tmp = 0x2;
            r3 = 0xe;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            goto L_800853B4;
        }
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
        if (tmp == 8) {
            r3 = 0xe;
            goto L_800853B4;
        }
        ((void (*)(void))_threadSwitch)();
    L_8008538C:
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommIsBusy)(r3);
        if ((s32)r3 == 0) goto L_80085334;
        tmp = 0x0;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))fn_80093610)(r3);
    L_800853B4:
        if ((s32)r3 != 0xe) {
        do {
            if ((s32)r3 < 0xe) {
                if ((s32)r3 != 2) {
                    break;
                }
                r4 = 0x20000;
                tmp = r4 + 0x2;
                if ((s32)r3 == (s32)tmp) break;
                break;
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r15 = 0x0;
        L_800853EC:
            r16 = 0x0;
            r18 = r24;
            r17 = r16;
            r19 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r17 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp == 7) {
                    r16 = 0x1;
                }
                r18 = r18 + 0x4;
                r19 = r19 + 0x1;
                r17 = r17 + 0x1;
            } while ((s32)r17 <= 3);
            tmp = r16 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r15 = r15 + 0x1;
                if ((s32)r15 < 0xf) goto L_800853EC;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r15 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r15 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r15 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r15 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r15 = 0x4;
            }
            }
            }
            }
            if ((s32)r15 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80085548;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r15 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r15 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80085548:
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = 0x6;
                tmp = (s8)tmp;
                tmp = tmp << 2;
                *(u32*)PC_PTR((r24 + tmp)) = r3;
            }
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
            if ((s32)tmp == 3) {
                r3 = 0x7;
                r4 = 0x44e7;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44e6;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            r3 = 0x0;
            return;
        } while (0);
            r20 = 0x0;
        L_800855B8:
            r19 = 0x0;
            r17 = r24;
            r18 = r19;
            r16 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r17) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r18 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r16) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r17) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r17) + 0x0);
                if ((s32)tmp == 7) {
                    r19 = 0x1;
                }
                r17 = r17 + 0x4;
                r16 = r16 + 0x1;
                r18 = r18 + 0x1;
            } while ((s32)r18 <= 3);
            tmp = r19 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r20 = r20 + 0x1;
                if ((s32)r20 < 0xf) goto L_800855B8;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r16 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r16 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r16 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r16 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r16 = 0x4;
            }
            }
            }
            }
            if ((s32)r16 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80085714;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r16 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r16 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80085714:
        do {
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                tmp = tmp & 0x00000008;
                if (tmp != 0) {
                    tmp = 0x1;
                    break;
                }
                tmp = 0x0;
                break;
            }
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = 0x6;
            r3 = 0x2f;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r4;
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = (s8)tmp;
            r4 = r4 + 0x1;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x3c47;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = r24;
                r4 = 0x1;
                fn_80087AE8();

            } else {
            r3 = r24;
            r4 = 0x7;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            tmp = 0x0;
        } while (0);
            tmp = tmp & 0xFF;
            if (tmp != 0) goto L_80084E44;
            r3 = 0x0;
            return;
                }
        tmp = 0x4;
        r16 = 0x0;
        *(u32*)PC_PTR((r27 + r24)) = tmp;
        do {
            r3 = r26;
            r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                r3 = ((u32 (*)(void))menuGetEnablePort)();
                r4 = PC_PTR32(&lbl_80478950);
                tmp = *(u8*)PC_PTR((r4 + r28));
                tmp = tmp | r3;
                r3 = tmp & 0xFF;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                break;
            }
            ((void (*)(void))_threadSwitch)();
            r16 = r16 + 0x1;
        } while ((s32)r16 < 0x12c);

        if (tmp == 0) {
            r4 = r26;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x3c4d;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            f27 = *(f32*)&lbl_8047C1A8;
            f28 = *(f64*)&lbl_8047C1B0;
            r16 = 0x43300000;
            f30 = *(f64*)&lbl_8047C1B8;
            f31 = *(f32*)&lbl_8047C1AC;
            while (f27 < f31) {

                ((void (*)(void))_threadSwitch)();
                r3 = ((u32 (*)(void))fn_800D37CC)();
                *(u32*)(sp + 0xC1C) = tmp;
                f29 = f0 - f28;
                r3 = ((u32 (*)(void))GSgetFrameStep)();
                f0 = f0 - f30;
                f0 = f0 / f29;
                f27 = f27 + f0;

            }
        }
        r3 = r22;
        ((void (*)(RegWord))heroInit)(r3);
        tmp = 0x0;
        r3 = r28;
        *(u32*)(sp + 0x18) = tmp;
        r4 = r22;
        r5 = PC_PTR32(sp) + 0x18;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord))gbaCommStartTransferB)(r3, r4, r5);
        tmp = 0x0;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        goto L_80085934;
    L_800858DC:
        r3 = 0x10c;
        r3 = ((u32 (*)(RegWord))menuIsCheck)(r3);
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            ((void (*)(void))_threadSwitch)();
            goto L_80085934;
        }
        r3 = ((u32 (*)(void))windowGetKeyInfo)();
        tmp = *(u16*)((u8*)PC_PTR(r3) + 0x4);
        tmp = tmp & 0x00000020;
        if ((s32)tmp != 0) {
            tmp = 0x2;
            r16 = 0xe;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            goto L_80085960;
        }
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
        if (tmp == 8) {
            r16 = 0xe;
            goto L_80085960;
        }
        ((void (*)(void))_threadSwitch)();
    L_80085934:
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommIsBusy)(r3);
        if ((s32)r3 == 0) goto L_800858DC;
        tmp = 0x0;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))fn_80093610)(r3);
        r16 = r3;
    L_80085960:
        if ((s32)r16 != 0xe) {
            goto L_80085B40;
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r15 = 0x0;
    L_80085978:
        r16 = 0x0;
        r18 = r24;
        r17 = r16;
        r19 = PC_PTR32(&lbl_80478950);
        do {
            tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
            if ((s32)tmp != 5) {
                if ((s32)tmp == 4) {
                }
                r3 = r17 + 0x1;
                r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    r3 = ((u32 (*)(void))menuGetEnablePort)();
                    tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                    tmp = r3 & ~tmp;
                    r3 = tmp & 0xFF;
                    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                    r3 = 0x7;
                    tmp = 0x8;
                    *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                    *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                }
                }
            tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
            if ((s32)tmp == 7) {
                r16 = 0x1;
            }
            r18 = r18 + 0x4;
            r19 = r19 + 0x1;
            r17 = r17 + 0x1;
        } while ((s32)r17 <= 3);
        tmp = r16 & 0xFF;
        if (tmp == 0) {
            ((void (*)(void))_threadSwitch)();
            r15 = r15 + 0x1;
            if ((s32)r15 < 0xf) goto L_80085978;
        }
        r3 = 0x26;
        ((void (*)(RegWord))soundStartSE)(r3);
        r15 = 0x0;
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
        if ((s32)tmp != 7) {
            r15 = 0x1;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
            if ((s32)tmp != 7) {
                r15 = 0x2;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                if ((s32)tmp != 7) {
                    r15 = 0x3;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                    if ((s32)tmp != 7) {
                        r15 = 0x4;
        }
        }
        }
        }
        if ((s32)r15 > 3) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = 0x0;
                goto L_80085AD4;
            }
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r4 = r15 + 0x1;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        if ((s32)r15 == 0) {
            r3 = 0x7;
            r4 = 0x44c0;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44b8;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        tmp = 0x8;
        r3 = 0x1;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
    L_80085AD4:
        tmp = r3 & 0xFF;
        if (tmp == 0) {
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = 0x6;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r3;
        }
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
        if ((s32)tmp == 3) {
            r3 = 0x7;
            r4 = 0x44e7;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44e6;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        r3 = 0x0;
        return;
    L_80085B40:
        /* extrwi tmp, r3, 2, 22 */;
        if (tmp != 0) {
            r20 = 0x0;
        L_80085B54:
            r18 = 0x0;
            r16 = r24;
            r19 = r18;
            r17 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r19 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp == 7) {
                    r18 = 0x1;
                }
                r16 = r16 + 0x4;
                r17 = r17 + 0x1;
                r19 = r19 + 0x1;
            } while ((s32)r19 <= 3);
            tmp = r18 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r20 = r20 + 0x1;
                if ((s32)r20 < 0xf) goto L_80085B54;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r16 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r16 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r16 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r16 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r16 = 0x4;
            }
            }
            }
            }
            if ((s32)r16 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80085CB0;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r16 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r16 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80085CB0:
        do {
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                tmp = tmp & 0x00000008;
                if (tmp != 0) {
                    tmp = 0x1;
                    break;
                }
                tmp = 0x0;
                break;
            }
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = 0x6;
            r3 = 0x2f;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r4;
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = (s8)tmp;
            r4 = r4 + 0x1;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x3c49;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = r24;
                r4 = 0x1;
                fn_80087AE8();

            } else {
            r3 = r24;
            r4 = 0x7;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            tmp = 0x0;
        } while (0);
            tmp = tmp & 0xFF;
            if (tmp != 0) goto L_80084E44;
            r3 = 0x0;
            return;
        }
        /* extrwi tmp, r3, 4, 24 */;
        if (tmp <= 7) {
            r3 = PC_PTR32(jumptable_802EEB78);
            tmp = tmp << 2;
            r3 = PC_PTR32(jumptable_802EEB78);
            tmp = *(u32*)PC_PTR((r3 + tmp));
            ctr_fn = (void(*)(void))PC_PTR(tmp);
            r17 = 0x1;


        } else {
        tmp = 0x0;
        goto L_80085E1C;
        }
        r3 = ((u32 (*)(void))savedataBiosGetNowSavedataPtr)();
        if (r3 != 0) {
            savedataBiosGetGamedataPtr(r3);
            if (r3 != 0) {
                gamedataBiosGetGamedataAtttestPtr();
                if (r3 != 0) {
                    gamedataAttestBiosGetLangareaId();
                    r3 = r3 & 0xFF;
                    tmp = r17 & 0xFF;
                    if (r3 == tmp) {
                        tmp = 0x1;
                        goto L_80085E1C;
            }
            }
            }
        }
        tmp = 0x0;
    L_80085E1C:
        tmp = tmp & 0xFF;
        if (tmp == 0) {
            r20 = 0x0;
        L_80085E2C:
            r18 = 0x0;
            r16 = r24;
            r19 = r18;
            r17 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r19 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp == 7) {
                    r18 = 0x1;
                }
                r16 = r16 + 0x4;
                r17 = r17 + 0x1;
                r19 = r19 + 0x1;
            } while ((s32)r19 <= 3);
            tmp = r18 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r20 = r20 + 0x1;
                if ((s32)r20 < 0xf) goto L_80085E2C;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r16 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r16 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r16 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r16 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r16 = 0x4;
            }
            }
            }
            }
            if ((s32)r16 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80085F88;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r16 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r16 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80085F88:
        do {
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                tmp = tmp & 0x00000008;
                if (tmp != 0) {
                    tmp = 0x1;
                    break;
                }
                tmp = 0x0;
                break;
            }
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = 0x6;
            r3 = 0x2f;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r4;
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = (s8)tmp;
            r4 = r4 + 0x1;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x44f0;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = r24;
                r4 = 0x1;
                fn_80087AE8();

            } else {
            r3 = r24;
            r4 = 0x7;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            tmp = 0x0;
        } while (0);
            tmp = tmp & 0xFF;
            if (tmp != 0) goto L_80084E44;
            r3 = 0x0;
            return;
        }
        if ((s32)r16 != 4) {

        } else {
        tmp = r3 & 0x00000002;
        if (tmp != 0) goto L_8008629C;
        }
        r20 = 0x0;
    L_80086074:
        r18 = 0x0;
        r16 = r24;
        r19 = r18;
        r17 = PC_PTR32(&lbl_80478950);
        do {
            tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
            if ((s32)tmp != 5) {
                if ((s32)tmp == 4) {
                }
                r3 = r19 + 0x1;
                r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    r3 = ((u32 (*)(void))menuGetEnablePort)();
                    tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                    tmp = r3 & ~tmp;
                    r3 = tmp & 0xFF;
                    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                    r3 = 0x7;
                    tmp = 0x8;
                    *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                    *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                }
                }
            tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
            if ((s32)tmp == 7) {
                r18 = 0x1;
            }
            r16 = r16 + 0x4;
            r17 = r17 + 0x1;
            r19 = r19 + 0x1;
        } while ((s32)r19 <= 3);
        tmp = r18 & 0xFF;
        if (tmp == 0) {
            ((void (*)(void))_threadSwitch)();
            r20 = r20 + 0x1;
            if ((s32)r20 < 0xf) goto L_80086074;
        }
        r3 = 0x26;
        ((void (*)(RegWord))soundStartSE)(r3);
        r16 = 0x0;
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
        if ((s32)tmp != 7) {
            r16 = 0x1;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
            if ((s32)tmp != 7) {
                r16 = 0x2;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                if ((s32)tmp != 7) {
                    r16 = 0x3;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                    if ((s32)tmp != 7) {
                        r16 = 0x4;
        }
        }
        }
        }
        if ((s32)r16 > 3) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = 0x0;
                goto L_800861D0;
            }
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r4 = r16 + 0x1;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        if ((s32)r16 == 0) {
            r3 = 0x7;
            r4 = 0x44c0;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44b8;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        tmp = 0x8;
        r3 = 0x1;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
    L_800861D0:
    do {
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            tmp = 0x0;
            break;
        }
        tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r4 = 0x6;
        r3 = 0x2f;
        tmp = (s8)tmp;
        tmp = tmp << 2;
        *(u32*)PC_PTR((r24 + tmp)) = r4;
        tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r4 = (s8)tmp;
        r4 = r4 + 0x1;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        r3 = 0x7;
        r4 = 0x3c49;
        r5 = 0x0;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
        tmp = tmp & 0x00000008;
        if (tmp == 0) {
            r3 = r24;
            r4 = 0x1;
            fn_80087AE8();

        } else {
        r3 = r24;
        r4 = 0x7;
        fn_80087AE8();
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            tmp = 0x1;
            break;
        }
        }
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        tmp = 0x0;
    } while (0);
        tmp = tmp & 0xFF;
        if (tmp != 0) goto L_80084E44;
        r3 = 0x0;
        return;
    L_8008629C:
        if (tmp != 0) {
            tmp = r3 & 0x00000004;
            r4 = 0x1;
            if (tmp != 0) {
                tmp = r3 & 0x00000008;
                if (tmp == 0) {
                    r4 = 0x0;
                }

            } else {
            tmp = r3 & 0x1;
            if (tmp == 0) {
                r4 = 0x0;
        }
            }
            tmp = r4 & 0xFF;
        }
        if (tmp == 0) {
            r20 = 0x0;
        L_800862EC:
            r18 = 0x0;
            r16 = r24;
            r19 = r18;
            r17 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r19 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp == 7) {
                    r18 = 0x1;
                }
                r16 = r16 + 0x4;
                r17 = r17 + 0x1;
                r19 = r19 + 0x1;
            } while ((s32)r19 <= 3);
            tmp = r18 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r20 = r20 + 0x1;
                if ((s32)r20 < 0xf) goto L_800862EC;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r16 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r16 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r16 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r16 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r16 = 0x4;
            }
            }
            }
            }
            if ((s32)r16 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80086448;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r16 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r16 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80086448:
        do {
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                tmp = tmp & 0x00000008;
                if (tmp != 0) {
                    tmp = 0x1;
                    break;
                }
                tmp = 0x0;
                break;
            }
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = 0x6;
            r3 = 0x2f;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r4;
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = (s8)tmp;
            r4 = r4 + 0x1;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x44c3;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = r24;
                r4 = 0x1;
                fn_80087AE8();

            } else {
            r3 = r24;
            r4 = 0x7;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            tmp = 0x0;
        } while (0);
            tmp = tmp & 0xFF;
            if (tmp != 0) goto L_80084E44;
            r3 = 0x0;
            return;
        }
        do {
            if (tmp == 0) break;
            tmp = r3 & 0x1;
            if (tmp == 0) {
                r20 = 0x0;
            L_80086530:
                r18 = 0x0;
                r16 = r24;
                r19 = r18;
                r17 = PC_PTR32(&lbl_80478950);
                do {
                    tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                    if ((s32)tmp != 5) {
                        if ((s32)tmp == 4) {
                        }
                        r3 = r19 + 0x1;
                        r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                        tmp = r3 & 0xFF;
                        if (tmp == 0) {
                            r3 = ((u32 (*)(void))menuGetEnablePort)();
                            tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                            tmp = r3 & ~tmp;
                            r3 = tmp & 0xFF;
                            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                            r3 = 0x7;
                            tmp = 0x8;
                            *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                        }
                        }
                    tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                    if ((s32)tmp == 7) {
                        r18 = 0x1;
                    }
                    r16 = r16 + 0x4;
                    r17 = r17 + 0x1;
                    r19 = r19 + 0x1;
                } while ((s32)r19 <= 3);
                tmp = r18 & 0xFF;
                if (tmp == 0) {
                    ((void (*)(void))_threadSwitch)();
                    r20 = r20 + 0x1;
                    if ((s32)r20 < 0xf) goto L_80086530;
                }
                r3 = 0x26;
                ((void (*)(RegWord))soundStartSE)(r3);
                r16 = 0x0;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
                if ((s32)tmp != 7) {
                    r16 = 0x1;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                    if ((s32)tmp != 7) {
                        r16 = 0x2;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                        if ((s32)tmp != 7) {
                            r16 = 0x3;
                            tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                            if ((s32)tmp != 7) {
                                r16 = 0x4;
                }
                }
                }
                }
                if ((s32)r16 > 3) {
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                    tmp = tmp & 0x00000008;
                    if (tmp == 0) {
                        r3 = 0x0;
                        goto L_8008668C;
                    }
                }
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r4 = r16 + 0x1;
                r3 = 0x2f;
                ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
                if ((s32)r16 == 0) {
                    r3 = 0x7;
                    r4 = 0x44c0;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44b8;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                tmp = 0x8;
                r3 = 0x1;
                *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            L_8008668C:
            do {
                tmp = r3 & 0xFF;
                if (tmp != 0) {
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                    tmp = tmp & 0x00000008;
                    if (tmp != 0) {
                        tmp = 0x1;
                        break;
                    }
                    tmp = 0x0;
                    break;
                }
                tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r4 = 0x6;
                r3 = 0x2f;
                tmp = (s8)tmp;
                tmp = tmp << 2;
                *(u32*)PC_PTR((r24 + tmp)) = r4;
                tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r4 = (s8)tmp;
                r4 = r4 + 0x1;
                ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
                r3 = 0x7;
                r4 = 0x44c3;
                r5 = 0x0;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = r24;
                    r4 = 0x1;
                    fn_80087AE8();

                } else {
                r3 = r24;
                r4 = 0x7;
                fn_80087AE8();
                tmp = r3 & 0xFF;
                if (tmp != 0) {
                    tmp = 0x1;
                    break;
                }
                }
                r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = (s8)r3;
                r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
                tmp = 0x0;
            } while (0);
                tmp = tmp & 0xFF;
                if (tmp != 0) goto L_80084E44;
                r3 = 0x0;
                return;
            }
            r3 = r22;
            r3 = ((u32 (*)(RegWord))menuRuleCheckPartyAndRule)(r3);
            tmp = r3 & 0xFF;
            if (tmp != 0) break;
            r3 = 0xe4;
            r4 = 0x0;
            r5 = 0x1;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord))menuCloseCustom)(r3, r4, r5);
            r3 = r22;
            r4 = 0x1;
            ((void (*)(RegWord, RegWord))fn_8005CF2C)(r3, r4);
            if (tmp == 0) {
                r3 = 0x0;
                return;
            }
            r3 = 0xe4;
            r4 = 0x0;
            ((void (*)(RegWord, RegWord))menuOpen)(r3, r4);
            r3 = 0xe4;
            r3 = ((u32 (*)(RegWord))windowSearchID)(r3);
            r16 = r3;
            if (r16 == 0) {
                r4 = 0x1f4;
                r5 = PC_PTR32(&lbl_8047C1A0);
                r3 = r3 + 0x184;
                ((void (*)(RegWord, RegWord, RegWord))__assert)(r3, r4, r5);
            }
            if (r16 == 0) {
                r3 = 0xa6;
                r3 = ((u32 (*)(RegWord))windowSearchID)(r3);
                r16 = r3;
            }
            r3 = r16;
            r3 = ((u32 (*)(RegWord))windowGetFreeWork)(r3);
            r5 = *(u32*)((u8*)PC_PTR(r3) + 0x0);
            r4 = 0x0;
            *(u32*)((u8*)PC_PTR(r5) + 0x24) = tmp;
            *(u32*)((u8*)PC_PTR(r5) + 0x2C) = tmp;
            tmp = *(u8*)((u8*)PC_PTR(r3) + 0x0);
            *(u8*)((u8*)PC_PTR(r5) + 0x21) = tmp;
            tmp = 0x5;
            r3 = *(u8*)((u8*)PC_PTR(r30) + 0x0);
            r3 = (s8)r3;
            r4 = r3 << 2;
            r3 = *(u32*)PC_PTR((r29 + r4));
            *(u32*)PC_PTR((r5 + r4)) = r3;
            if ((s32)r4 < (s32)r25) {
                r3 = *(u32*)PC_PTR((r5 + r4));
                if ((s32)r3 == 1) {
                    *(u32*)PC_PTR((r5 + r4)) = tmp;
            }
            }
            r6 = r30 + 0x1;
            r4 = 0x1;
            r3 = *(u8*)((u8*)PC_PTR(r6) + 0x0);
            r3 = (s8)r3;
            r4 = r3 << 2;
            r3 = *(u32*)PC_PTR((r29 + r4));
            *(u32*)PC_PTR((r5 + r4)) = r3;
            if ((s32)r4 < (s32)r25) {
                r3 = *(u32*)PC_PTR((r5 + r4));
                if ((s32)r3 == 1) {
                    *(u32*)PC_PTR((r5 + r4)) = tmp;
            }
            }
            r6 = r6 + 0x1;
            r4 = 0x2;
            r3 = *(u8*)((u8*)PC_PTR(r6) + 0x0);
            r3 = (s8)r3;
            r4 = r3 << 2;
            r3 = *(u32*)PC_PTR((r29 + r4));
            *(u32*)PC_PTR((r5 + r4)) = r3;
            if ((s32)r4 < (s32)r25) {
                r3 = *(u32*)PC_PTR((r5 + r4));
                if ((s32)r3 == 1) {
                    *(u32*)PC_PTR((r5 + r4)) = tmp;
            }
            }
            r6 = r6 + 0x1;
            r4 = 0x3;
            r3 = *(u8*)((u8*)PC_PTR(r6) + 0x0);
            r3 = (s8)r3;
            r4 = r3 << 2;
            r3 = *(u32*)PC_PTR((r29 + r4));
            *(u32*)PC_PTR((r5 + r4)) = r3;
            if ((s32)r4 < (s32)r25) {
                r3 = *(u32*)PC_PTR((r5 + r4));
                if ((s32)r3 == 1) {
                    *(u32*)PC_PTR((r5 + r4)) = tmp;
            }
            }
            tmp = 0x6;
            r24 = r5;
            *(u32*)PC_PTR((r27 + r5)) = tmp;
            goto L_80084E44;
        } while (0);
        if (tmp != 0) {
            tmp = (s8)r23;
            if ((s32)tmp != 1) {
            }
            if (r31 == 0) goto L_80086DBC;
            }
        r3 = r28;
        r4 = r21;
        r3 = ((u32 (*)(RegWord, RegWord))gbaCommStartTransferA)(r3, r4);
        tmp = 0x0;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        goto L_8008697C;
    L_80086924:
        r3 = 0x10c;
        r3 = ((u32 (*)(RegWord))menuIsCheck)(r3);
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            ((void (*)(void))_threadSwitch)();
            goto L_8008697C;
        }
        r3 = ((u32 (*)(void))windowGetKeyInfo)();
        tmp = *(u16*)((u8*)PC_PTR(r3) + 0x4);
        tmp = tmp & 0x00000020;
        if ((s32)tmp != 0) {
            tmp = 0x2;
            r3 = 0xe;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            goto L_800869A4;
        }
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
        if (tmp == 8) {
            r3 = 0xe;
            goto L_800869A4;
        }
        ((void (*)(void))_threadSwitch)();
    L_8008697C:
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommIsBusy)(r3);
        if ((s32)r3 == 0) goto L_80086924;
        tmp = 0x0;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))fn_80093610)(r3);
    L_800869A4:
        if ((s32)r3 != 0xe) {
            if ((s32)r3 < 0xe) {
                if ((s32)r3 == 0xb) goto L_80086DBC;
            }
            goto L_80086B90;
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r15 = 0x0;
    L_800869C8:
        r16 = 0x0;
        r18 = r24;
        r17 = r16;
        r19 = PC_PTR32(&lbl_80478950);
        do {
            tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
            if ((s32)tmp != 5) {
                if ((s32)tmp == 4) {
                }
                r3 = r17 + 0x1;
                r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    r3 = ((u32 (*)(void))menuGetEnablePort)();
                    tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                    tmp = r3 & ~tmp;
                    r3 = tmp & 0xFF;
                    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                    r3 = 0x7;
                    tmp = 0x8;
                    *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                    *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                }
                }
            tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
            if ((s32)tmp == 7) {
                r16 = 0x1;
            }
            r18 = r18 + 0x4;
            r19 = r19 + 0x1;
            r17 = r17 + 0x1;
        } while ((s32)r17 <= 3);
        tmp = r16 & 0xFF;
        if (tmp == 0) {
            ((void (*)(void))_threadSwitch)();
            r15 = r15 + 0x1;
            if ((s32)r15 < 0xf) goto L_800869C8;
        }
        r3 = 0x26;
        ((void (*)(RegWord))soundStartSE)(r3);
        r15 = 0x0;
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
        if ((s32)tmp != 7) {
            r15 = 0x1;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
            if ((s32)tmp != 7) {
                r15 = 0x2;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                if ((s32)tmp != 7) {
                    r15 = 0x3;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                    if ((s32)tmp != 7) {
                        r15 = 0x4;
        }
        }
        }
        }
        if ((s32)r15 > 3) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = 0x0;
                goto L_80086B24;
            }
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r4 = r15 + 0x1;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        if ((s32)r15 == 0) {
            r3 = 0x7;
            r4 = 0x44c0;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44b8;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        tmp = 0x8;
        r3 = 0x1;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
    L_80086B24:
        tmp = r3 & 0xFF;
        if (tmp == 0) {
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = 0x6;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r3;
        }
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
        if ((s32)tmp == 3) {
            r3 = 0x7;
            r4 = 0x44e7;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44e6;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        r3 = 0x0;
        return;
    L_80086B90:
        r20 = 0x0;
    L_80086B94:
        r18 = 0x0;
        r16 = r24;
        r19 = r18;
        r17 = PC_PTR32(&lbl_80478950);
        do {
            tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
            if ((s32)tmp != 5) {
                if ((s32)tmp == 4) {
                }
                r3 = r19 + 0x1;
                r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    r3 = ((u32 (*)(void))menuGetEnablePort)();
                    tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                    tmp = r3 & ~tmp;
                    r3 = tmp & 0xFF;
                    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                    r3 = 0x7;
                    tmp = 0x8;
                    *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                    *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                }
                }
            tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
            if ((s32)tmp == 7) {
                r18 = 0x1;
            }
            r16 = r16 + 0x4;
            r17 = r17 + 0x1;
            r19 = r19 + 0x1;
        } while ((s32)r19 <= 3);
        tmp = r18 & 0xFF;
        if (tmp == 0) {
            ((void (*)(void))_threadSwitch)();
            r20 = r20 + 0x1;
            if ((s32)r20 < 0xf) goto L_80086B94;
        }
        r3 = 0x26;
        ((void (*)(RegWord))soundStartSE)(r3);
        r16 = 0x0;
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
        if ((s32)tmp != 7) {
            r16 = 0x1;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
            if ((s32)tmp != 7) {
                r16 = 0x2;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                if ((s32)tmp != 7) {
                    r16 = 0x3;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                    if ((s32)tmp != 7) {
                        r16 = 0x4;
        }
        }
        }
        }
        if ((s32)r16 > 3) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = 0x0;
                goto L_80086CF0;
            }
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r4 = r16 + 0x1;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        if ((s32)r16 == 0) {
            r3 = 0x7;
            r4 = 0x44c0;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44b8;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        tmp = 0x8;
        r3 = 0x1;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
    L_80086CF0:
    do {
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            tmp = 0x0;
            break;
        }
        tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r4 = 0x6;
        r3 = 0x2f;
        tmp = (s8)tmp;
        tmp = tmp << 2;
        *(u32*)PC_PTR((r24 + tmp)) = r4;
        tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r4 = (s8)tmp;
        r4 = r4 + 0x1;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        r3 = 0x7;
        r4 = 0x3c47;
        r5 = 0x0;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
        tmp = tmp & 0x00000008;
        if (tmp == 0) {
            r3 = r24;
            r4 = 0x1;
            fn_80087AE8();

        } else {
        r3 = r24;
        r4 = 0x7;
        fn_80087AE8();
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            tmp = 0x1;
            break;
        }
        }
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        tmp = 0x0;
    } while (0);
        tmp = tmp & 0xFF;
        if (tmp != 0) goto L_80084E44;
        r3 = 0x0;
        return;
    L_80086DBC:
        if (r31 != 0) {
            tmp = *(u32*)((u8*)PC_PTR(r21) + 0x8);
            tmp = tmp & 0x00000010;
        }
        if (tmp == 0) {
            r20 = 0x0;
        L_80086DD8:
            r18 = 0x0;
            r16 = r24;
            r19 = r18;
            r17 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r19 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r17) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r16) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r16) + 0x0);
                if ((s32)tmp == 7) {
                    r18 = 0x1;
                }
                r16 = r16 + 0x4;
                r17 = r17 + 0x1;
                r19 = r19 + 0x1;
            } while ((s32)r19 <= 3);
            tmp = r18 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r20 = r20 + 0x1;
                if ((s32)r20 < 0xf) goto L_80086DD8;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r16 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r16 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r16 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r16 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r16 = 0x4;
            }
            }
            }
            }
            if ((s32)r16 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80086F34;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r16 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r16 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80086F34:
        do {
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
                tmp = tmp & 0x00000008;
                if (tmp != 0) {
                    tmp = 0x1;
                    break;
                }
                tmp = 0x0;
                break;
            }
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = 0x6;
            r3 = 0x2f;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r4;
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r4 = (s8)tmp;
            r4 = r4 + 0x1;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x4417;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x24);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = r24;
                r4 = 0x1;
                fn_80087AE8();

            } else {
            r3 = r24;
            r4 = 0x7;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                tmp = 0x1;
                break;
            }
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            tmp = 0x0;
        } while (0);
            tmp = tmp & 0xFF;
            if (tmp != 0) goto L_80084E44;
            r3 = 0x2f;
            r4 = 0x0;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            r3 = 0x7;
            r4 = 0x44cf;
            r5 = 0x0;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            r3 = r24;
            r4 = 0x1;
            fn_80087AE8();
            r3 = 0x0;
            return;
        }
    do {
        r3 = r28;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        r3 = 0x3cc;
        ((void (*)(RegWord))soundStartSE)(r3);
        tmp = 0x5;
        *(u32*)PC_PTR((r24 + r27)) = tmp;
        r3 = ((u32 (*)(void))menuGetEnablePort)();
        r4 = PC_PTR32(&lbl_80478950);
        tmp = *(u8*)PC_PTR((r4 + r28));
        tmp = tmp | r3;
        r3 = tmp & 0xFF;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r4 = r26;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        r3 = 0x7;
        r4 = 0x3c4b;
        r5 = 0x0;
        r6 = 0x0;
        r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        if ((s32)tmp != 2) {
            if ((s32)tmp < 2) {
                if ((s32)tmp != 0) {
                    if ((s32)tmp < 0) {
                        break;
                    }
                    if ((s32)tmp >= 4) break;
                    goto L_8008769C;
                    }
                r3 = r24;
                r4 = 0x3;
                fn_80087AE8();
                tmp = r3 & 0xFF;
                if (tmp != 0) {
                    r3 = 0x1;
                    return;
                }
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r15 = 0x0;
            L_800870D4:
                r16 = 0x0;
                r18 = r24;
                r17 = r16;
                r19 = PC_PTR32(&lbl_80478950);
                do {
                    tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                    if ((s32)tmp != 5) {
                        if ((s32)tmp == 4) {
                        }
                        r3 = r17 + 0x1;
                        r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                        tmp = r3 & 0xFF;
                        if (tmp == 0) {
                            r3 = ((u32 (*)(void))menuGetEnablePort)();
                            tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                            tmp = r3 & ~tmp;
                            r3 = tmp & 0xFF;
                            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                            r3 = 0x7;
                            tmp = 0x8;
                            *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                        }
                        }
                    tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                    if ((s32)tmp == 7) {
                        r16 = 0x1;
                    }
                    r18 = r18 + 0x4;
                    r19 = r19 + 0x1;
                    r17 = r17 + 0x1;
                } while ((s32)r17 <= 3);
                tmp = r16 & 0xFF;
                if (tmp == 0) {
                    ((void (*)(void))_threadSwitch)();
                    r15 = r15 + 0x1;
                    if ((s32)r15 < 0xf) goto L_800870D4;
                }
                r3 = 0x26;
                ((void (*)(RegWord))soundStartSE)(r3);
                r15 = 0x0;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
                if ((s32)tmp != 7) {
                    r15 = 0x1;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                    if ((s32)tmp != 7) {
                        r15 = 0x2;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                        if ((s32)tmp != 7) {
                            r15 = 0x3;
                            tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                            if ((s32)tmp != 7) {
                                r15 = 0x4;
                }
                }
                }
                }
                if ((s32)r15 > 3) {
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                    tmp = tmp & 0x00000008;
                    if (tmp == 0) {
                        r3 = 0x0;
                        goto L_80087230;
                    }
                }
                r3 = 0x1;
                r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                r4 = r15 + 0x1;
                r3 = 0x2f;
                ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
                if ((s32)r15 == 0) {
                    r3 = 0x7;
                    r4 = 0x44c0;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44b8;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                tmp = 0x8;
                r3 = 0x1;
                *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
            L_80087230:
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                    r3 = 0x6;
                    tmp = (s8)tmp;
                    tmp = tmp << 2;
                    *(u32*)PC_PTR((r24 + tmp)) = r3;
                }
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
                if ((s32)tmp == 3) {
                    r3 = 0x7;
                    r4 = 0x44e7;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                } else {

                    r3 = 0x7;
                    r4 = 0x44e6;
                    r5 = 0x1;
                    r6 = 0x0;
                    r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
                }
                r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = (s8)r3;
                r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
                r3 = 0x0;
                return;
                    }
            tmp = (s8)r23;
            if ((s32)tmp != 2) break;
            r3 = r24;
            r4 = 0x3;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                r3 = 0x1;
                return;
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r15 = 0x0;
        L_800872D4:
            r16 = 0x0;
            r18 = r24;
            r17 = r16;
            r19 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r17 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp == 7) {
                    r16 = 0x1;
                }
                r18 = r18 + 0x4;
                r19 = r19 + 0x1;
                r17 = r17 + 0x1;
            } while ((s32)r17 <= 3);
            tmp = r16 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r15 = r15 + 0x1;
                if ((s32)r15 < 0xf) goto L_800872D4;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r15 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r15 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r15 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r15 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r15 = 0x4;
            }
            }
            }
            }
            if ((s32)r15 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80087430;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r15 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r15 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80087430:
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = 0x6;
                tmp = (s8)tmp;
                tmp = tmp << 2;
                *(u32*)PC_PTR((r24 + tmp)) = r3;
            }
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
            if ((s32)tmp == 3) {
                r3 = 0x7;
                r4 = 0x44e7;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44e6;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            r3 = 0x0;
            return;
        }
        tmp = (s8)r23;
        if ((s32)tmp != 3) break;
        r3 = r24;
        r4 = 0x3;
        fn_80087AE8();
        tmp = r3 & 0xFF;
        if (tmp != 0) {
            r3 = 0x1;
            return;
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r15 = 0x0;
    L_800874D4:
        r16 = 0x0;
        r18 = r24;
        r17 = r16;
        r19 = PC_PTR32(&lbl_80478950);
        do {
            tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
            if ((s32)tmp != 5) {
                if ((s32)tmp == 4) {
                }
                r3 = r17 + 0x1;
                r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                tmp = r3 & 0xFF;
                if (tmp == 0) {
                    r3 = ((u32 (*)(void))menuGetEnablePort)();
                    tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                    tmp = r3 & ~tmp;
                    r3 = tmp & 0xFF;
                    r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                    r3 = 0x7;
                    tmp = 0x8;
                    *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                    *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                }
                }
            tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
            if ((s32)tmp == 7) {
                r16 = 0x1;
            }
            r18 = r18 + 0x4;
            r19 = r19 + 0x1;
            r17 = r17 + 0x1;
        } while ((s32)r17 <= 3);
        tmp = r16 & 0xFF;
        if (tmp == 0) {
            ((void (*)(void))_threadSwitch)();
            r15 = r15 + 0x1;
            if ((s32)r15 < 0xf) goto L_800874D4;
        }
        r3 = 0x26;
        ((void (*)(RegWord))soundStartSE)(r3);
        r15 = 0x0;
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
        if ((s32)tmp != 7) {
            r15 = 0x1;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
            if ((s32)tmp != 7) {
                r15 = 0x2;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                if ((s32)tmp != 7) {
                    r15 = 0x3;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                    if ((s32)tmp != 7) {
                        r15 = 0x4;
        }
        }
        }
        }
        if ((s32)r15 > 3) {
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
            tmp = tmp & 0x00000008;
            if (tmp == 0) {
                r3 = 0x0;
                goto L_80087630;
            }
        }
        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
        r4 = r15 + 0x1;
        r3 = 0x2f;
        ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
        if ((s32)r15 == 0) {
            r3 = 0x7;
            r4 = 0x44c0;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44b8;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        tmp = 0x8;
        r3 = 0x1;
        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
    L_80087630:
        tmp = r3 & 0xFF;
        if (tmp == 0) {
            tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = 0x6;
            tmp = (s8)tmp;
            tmp = tmp << 2;
            *(u32*)PC_PTR((r24 + tmp)) = r3;
        }
        tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
        if ((s32)tmp == 3) {
            r3 = 0x7;
            r4 = 0x44e7;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        } else {

            r3 = 0x7;
            r4 = 0x44e6;
            r5 = 0x1;
            r6 = 0x0;
            r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
        }
        r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
        r3 = 0x0;
        return;
    L_8008769C:
        tmp = (s8)r23;
        if ((s32)tmp == 0) {
            r3 = r24;
            r4 = 0x3;
            fn_80087AE8();
            tmp = r3 & 0xFF;
            if (tmp != 0) {
                r3 = 0x1;
                return;
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r15 = 0x0;
        L_800876D4:
            r16 = 0x0;
            r18 = r24;
            r17 = r16;
            r19 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r17 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp == 7) {
                    r16 = 0x1;
                }
                r18 = r18 + 0x4;
                r19 = r19 + 0x1;
                r17 = r17 + 0x1;
            } while ((s32)r17 <= 3);
            tmp = r16 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r15 = r15 + 0x1;
                if ((s32)r15 < 0xf) goto L_800876D4;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r15 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r15 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r15 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r15 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r15 = 0x4;
            }
            }
            }
            }
            if ((s32)r15 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80087830;
            }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r15 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r15 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80087830:
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = 0x6;
                tmp = (s8)tmp;
                tmp = tmp << 2;
                *(u32*)PC_PTR((r24 + tmp)) = r3;
            }
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
            if ((s32)tmp == 3) {
                r3 = 0x7;
                r4 = 0x44e7;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44e6;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            r3 = 0x0;
            return;
        }
    } while (0);
        r4 = 0x7;
        tmp = *(u8*)((u8*)PC_PTR(r3) + 0x1);
        r3 = r24;
        *(u8*)((u8*)PC_PTR(r24) + 0x21) = tmp;
        fn_80087AE8();
        tmp = r3 & 0xFF;
        if (tmp == 0) {
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r15 = 0x0;
        L_800878CC:
            r16 = 0x0;
            r18 = r24;
            r17 = r16;
            r19 = PC_PTR32(&lbl_80478950);
            do {
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp != 5) {
                    if ((s32)tmp == 4) {
                    }
                    r3 = r17 + 0x1;
                    r3 = ((u32 (*)(RegWord))fn_8008ABA0)(r3);
                    tmp = r3 & 0xFF;
                    if (tmp == 0) {
                        r3 = ((u32 (*)(void))menuGetEnablePort)();
                        tmp = *(u8*)((u8*)PC_PTR(r19) + 0x0);
                        tmp = r3 & ~tmp;
                        r3 = tmp & 0xFF;
                        r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
                        r3 = 0x7;
                        tmp = 0x8;
                        *(u32*)((u8*)PC_PTR(r18) + 0x0) = r3;
                        *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
                    }
                    }
                tmp = *(u32*)((u8*)PC_PTR(r18) + 0x0);
                if ((s32)tmp == 7) {
                    r16 = 0x1;
                }
                r18 = r18 + 0x4;
                r19 = r19 + 0x1;
                r17 = r17 + 0x1;
            } while ((s32)r17 <= 3);
            tmp = r16 & 0xFF;
            if (tmp == 0) {
                ((void (*)(void))_threadSwitch)();
                r15 = r15 + 0x1;
                if ((s32)r15 < 0xf) goto L_800878CC;
            }
            r3 = 0x26;
            ((void (*)(RegWord))soundStartSE)(r3);
            r15 = 0x0;
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x0);
            if ((s32)tmp != 7) {
                r15 = 0x1;
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x4);
                if ((s32)tmp != 7) {
                    r15 = 0x2;
                    tmp = *(u32*)((u8*)PC_PTR(r24) + 0x8);
                    if ((s32)tmp != 7) {
                        r15 = 0x3;
                        tmp = *(u32*)((u8*)PC_PTR(r24) + 0xC);
                        if ((s32)tmp != 7) {
                            r15 = 0x4;
            }
            }
            }
            }
            if ((s32)r15 > 3) {
                tmp = *(u32*)((u8*)PC_PTR(r24) + 0x28);
                tmp = tmp & 0x00000008;
                if (tmp == 0) {
                    r3 = 0x0;
                    goto L_80087A28;
                }
            }
            r3 = 0x1;
            r3 = ((u32 (*)(RegWord))menuSetEnablePort)(r3);
            r4 = r15 + 0x1;
            r3 = 0x2f;
            ((void (*)(RegWord, RegWord))msgctrlSetValue)(r3, r4);
            if ((s32)r15 == 0) {
                r3 = 0x7;
                r4 = 0x44c0;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44b8;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            tmp = 0x8;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r24) + 0x28) = tmp;
        L_80087A28:
            tmp = r3 & 0xFF;
            if (tmp == 0) {
                tmp = *(u8*)((u8*)PC_PTR(r24) + 0x21);
                r3 = 0x6;
                tmp = (s8)tmp;
                tmp = tmp << 2;
                *(u32*)PC_PTR((r24 + tmp)) = r3;
            }
            tmp = *(u32*)((u8*)PC_PTR(r24) + 0x2C);
            if ((s32)tmp == 3) {
                r3 = 0x7;
                r4 = 0x44e7;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            } else {

                r3 = 0x7;
                r4 = 0x44e6;
                r5 = 0x1;
                r6 = 0x0;
                r3 = ((u32 (*)(RegWord, RegWord, RegWord, RegWord))winMsgOpen)(r3, r4, r5, r6);
            }
            r3 = *(u8*)((u8*)PC_PTR(r24) + 0x21);
            r3 = (s8)r3;
            r3 = ((u32 (*)(RegWord))gbaCommEnd)(r3);
            r3 = 0x0;
            return;
        }
        r25 = r25 + 0x1;
        r4 = r4 + 0x1;
    } while (r25 < 4);

    return;
}
#endif

/* 0x80087AE8 | size: 0x17C */
void fn_80087AE8(void) {
    u8 sp[0x20];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;

    r30 = r3;
    r26 = r4;
    tmp = 0x0;
    r31 = 0x1;
    *(u32*)((u8*)PC_PTR(r30) + 0x28) = tmp;
    tmp = *(u8*)((u8*)PC_PTR(r30) + 0x21);
    r3 = (s8)tmp;
    if ((s32)r3 >= 0) {
        ((void(*)(void))SIProbe)();
        /* subis tmp, r3, 0x4 */;
        if (tmp == 0) {
            r31 = 0x0;
    }
    }
    r28 = r26 & 0x00000002;
    r27 = r26 & 0x1;
    r26 = r26 & 0x00000004;
L_80087B3C:
    r3 = 0x10c;
    r3 = ((u32 (*)(RegWord))menuIsCheck)(r3);
    tmp = r3 & 0xFF;
    if (tmp != 0) {
        ((void (*)(void))_threadSwitch)();
        goto L_80087B3C;
    }
    if ((s32)r28 != 0) {
        r3 = ((u32 (*)(void))windowGetKeyInfo)();
        tmp = *(u16*)((u8*)PC_PTR(r3) + 0x4);
        tmp = tmp & 0x00000020;
        if ((s32)tmp != 0) {
            tmp = 0x2;
            r3 = 0x0;
            *(u32*)((u8*)PC_PTR(r30) + 0x28) = tmp;
            return;
    }
    }
    tmp = *(u32*)((u8*)PC_PTR(r30) + 0x28);
    if (tmp == 8) {
        r3 = 0x0;
        return;
    }
    if ((s32)r27 != 0) {
        r3 = ((u32 (*)(void))winMsgCheck)();
        r29 = r3;
        tmp = (s8)r29;
        if ((s32)tmp == 0) {
            tmp = 0x1;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r30) + 0x28) = tmp;
            return;
        }
        r3 = ((u32 (*)(void))windowGetKeyInfo)();
        tmp = *(u16*)((u8*)PC_PTR(r3) + 0x4);
        tmp = tmp & 0x00000010;
        if ((s32)tmp != 0) {
            tmp = (s8)r29;
            if ((s32)tmp == (s32)-0x1) {
                tmp = 0x1;
                r3 = 0x1;
                *(u32*)((u8*)PC_PTR(r30) + 0x28) = tmp;
                return;
    }
    }
    }
    if ((s32)r26 != 0) {
        r3 = *(u8*)((u8*)PC_PTR(r30) + 0x21);
        r3 = (s8)r3;
        r3 = ((u32 (*)(RegWord))SIProbe)(r3);
        if (r3 != 0x80) {
            tmp = r31 & 0xFF;
            if (tmp != 0) {
                /* subis tmp, r3, 0x4 */;
        }
        }
        if (tmp == 0) {
            tmp = 0x4;
            r3 = 0x1;
            *(u32*)((u8*)PC_PTR(r30) + 0x28) = tmp;
            return;
                }
        /* subis tmp, r3, 0x4 */;
        if (tmp != 0) {
            r31 = 0x1;
        }
        }
    ((void (*)(void))_threadSwitch)();
    goto L_80087B3C;


}

/* 0x80087C64 | size: 0x7C4 */
#ifndef TARGET_PC /* copy: fn_80087C64 is defined (and scored) in cardesavedata; no GameCube unit compiles this one */
void fn_80087C64(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSsndGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE();
#endif
    u8 sp[0x1C0];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    u32 r7 = 0;
    u32 r8 = 0;
    u32 r9 = 0;
    u32 r10 = 0;
    u32 r11 = 0;
    u32 r12 = 0;
    u32 r18 = 0;
    u32 r19 = 0;
    u32 r20 = 0;
    u32 r21 = 0;
    u32 r22 = 0;
    u32 r23 = 0;
    u32 r24 = 0;
    u32 r25 = 0;
    u32 r26 = 0;
    u32 r27 = 0;
    u32 r28 = 0;
    u32 r29 = 0;
    u32 r30 = 0;
    u32 r31 = 0;
    f32 f0 = 0.0f;
    f32 f1 = 0.0f;
    f32 f4 = 0.0f;
    void (*ctr_fn)(void) = 0;
    u32 ctr = 0;

    r25 = r3;
    r4 = PC_PTR32(&lbl_8026F488);
    r3 = 0x1;
    r30 = PC_PTR32(&lbl_8026F488);
    r3 = ((u32 (*)(RegWord))windowGetPortKeyInfo)(r3);
    r29 = PC_PTR32(sp) + 0x88;
    r28 = 0x1;
    r27 = 0x1;
    r26 = 0x0;
    tmp = 0x0;
    *(u32*)(sp + 0x88) = tmp;
    *(u32*)(sp + 0x8C) = tmp;
    *(u32*)(sp + 0x90) = tmp;
    *(u32*)(sp + 0x94) = tmp;
    *(u32*)(sp + 0x98) = tmp;
    *(u32*)(sp + 0x9C) = tmp;
    *(u32*)(sp + 0xA0) = tmp;
    *(u32*)(sp + 0xA4) = tmp;
    *(u32*)(sp + 0xA8) = tmp;
    while ((s32)r26 < 3) {

        r3 = 0x1;
        r3 = ((u32 (*)(RegWord))windowGetPortKeyInfo)(r3);
        r31 = r3;
        r6 = 0x0;
        tmp = *(u16*)((u8*)PC_PTR(r31) + 0x6);
        tmp = tmp & 0x00000010;
        if ((s32)tmp != 0) {
            if ((s32)r28 >= 0 && (s32)r28 < 3 && (s32)r27 >= 0) {


                if ((s32)r27 >= 3) {
                }
                tmp = 0x0;
                goto L_80087DD4;
                }
            r3 = r27 * 0xc;
            tmp = r28 << 2;
            r24 = PC_PTR32(sp) + 0x88;
            r20 = r3 + tmp;
            tmp = *(u32*)PC_PTR((r24 + r20));
            if ((s32)tmp != 0) {
                tmp = 0x0;
                goto L_80087DD4;
            }
            tmp = 0x9;
            r5 = PC_PTR32(sp) + 0xf0;
            r4 = r30 + 0x10;
            ctr_fn = (void(*)(void))PC_PTR(tmp);
            do {
                r3 = *(u32*)((u8*)PC_PTR(r4) + 0x4);
                tmp = *(u32*)((u8*)PC_PTR(r4) + 0x8);
                *(u32*)((u8*)PC_PTR(r5) + 0x4) = r3;
                r5 += 8; *(u32*)PC_PTR(r5) = tmp;
            } while (--ctr != 0);
            if ((s32)r28 >= 0) {
                if ((s32)r28 < 3) {
                    if ((s32)r27 >= 0) {
                        if ((s32)r27 < 3) {
                            r4 = r27 * 0x18;
                            tmp = r28 << 3;
                            r3 = PC_PTR32(sp) + 0xf4;
                            r4 = r4 + tmp;
                            r4 = r3 + r4;
                            r19 = *(s16*)((u8*)PC_PTR(r4) + 0x4);
                            r3 = *(u32*)((u8*)PC_PTR(r4) + 0x0);
                            r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                            r21 = r3;
                            if (r21 != 0) {
                                if (r21 != 0) {
                                    r4 = (s16)r19;
                                    ((void(*)(void))GSmodelSetAnimIndex)();
                                    f1 = *(f32*)&lbl_8047C1CC;
                                    r3 = r21;
                                    ((void(*)(void))GSmodelSetAnimFrame)();
                                    f1 = *(f32*)&lbl_8047C1C8;
                                    r3 = r21;
                                    ((void(*)(void))GSmodelSetAnimRate)();
                                    r3 = r21;
                                    ((void (*)(RegWord))GSmodelStartAnimation)(r3);
            }
            }
            }
            }
            }
            }
            tmp = 0x1;
            *(u32*)PC_PTR((r24 + r20)) = tmp;
        L_80087DD4:
            if ((s32)tmp != 0) {
                tmp = r26 << 3;
                r4 = PC_PTR32(sp) + 0x70;
                r4 = r4 + tmp;
                r26 = r26 + 0x1;
                *(u32*)((u8*)PC_PTR(r4) + 0x0) = r28;
                r3 = *(u32*)&lbl_8047C1C0;
                tmp = *(u32*)&lbl_8047C1C4;
                *(u32*)((u8*)PC_PTR(r4) + 0x4) = r27;
                *(u32*)(sp + 0x1C) = tmp;
                if ((s32)r26 >= 0) {
                    if ((s32)r26 < 4) {
                        tmp = r26 << 1;
                        r4 = PC_PTR32(sp) + 0x18;
                        r3 = 0x107E0000;
                        r19 = *(s16*)PC_PTR((r4 + tmp));
                        r3 = r3 + 0x100b;
                        r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                        r20 = r3;
                        if (r20 != 0) {
                            if (r20 != 0) {
                                r4 = (s16)r19;
                                ((void(*)(void))GSmodelSetAnimIndex)();
                                f1 = *(f32*)&lbl_8047C1CC;
                                r3 = r20;
                                ((void(*)(void))GSmodelSetAnimFrame)();
                                f1 = *(f32*)&lbl_8047C1C8;
                                r3 = r20;
                                ((void(*)(void))GSmodelSetAnimRate)();
                                r3 = r20;
                                ((void (*)(RegWord))GSmodelStartAnimation)(r3);
                }
                }
                }
                }
                if ((s32)r26 >= 3) {
                    r6 = *(u32*)((u8*)PC_PTR(r30) + 0x0);
                    r7 = 0x0;
                    r5 = *(u32*)((u8*)PC_PTR(r30) + 0x4);
                    r8 = r7;
                    r4 = *(u32*)((u8*)PC_PTR(r30) + 0x8);
                    r12 = r29;
                    r3 = *(u32*)((u8*)PC_PTR(r30) + 0xC);
                    r24 = PC_PTR32(sp) + 0x5c;
                    tmp = *(u16*)((u8*)PC_PTR(r30) + 0x10);
                    *(u16*)(sp + 0x30) = tmp;
                    tmp = tmp & 0xFFFF;
                    do {
                        r10 = r12;
                        r11 = r24;
                        r9 = 0x0;
                        *(u16*)(sp + 0x44) = tmp;
                        r20 = 0x3;
                        ctr_fn = (void(*)(void))PC_PTR(r20);
                        do {
                            r19 = *(u32*)((u8*)PC_PTR(r10) + 0x0);
                            if ((s32)r19 != 0) {
                                r19 = *(u16*)(sp + 0x44);
                                *(u16*)(sp + 0x6C) = r19;
                                if ((s32)r9 >= 0 && (s32)r9 < 3 && (s32)r8 >= 0) {


                                    if ((s32)r8 >= 3) {
                                    }
                                    r19 = 0x0;

                                    } else {
                                r19 = *(u16*)((u8*)PC_PTR(r11) + 0x0);
                                    }
                                r20 = r19 & 0xFFFF;
                                r19 = *(u16*)((u8*)PC_PTR(r25) + 0x0);
                                if (r20 == r19) {
                                    r7 = r7 + 0x1;
                            }
                                goto L_80087F6C;
                                }
                            r19 = *(u16*)((u8*)PC_PTR(r25) + 0x2);
                            if (r20 == r19) {
                                r7 = r7 + 0x1;
                                goto L_80087F6C;
                            }
                            r19 = *(u16*)((u8*)PC_PTR(r25) + 0x4);
                            if (r20 == r19) {
                                r7 = r7 + 0x1;
                        }
                        L_80087F6C:
                            r10 = r10 + 0x4;
                            r11 = r11 + 0x2;
                            r9 = r9 + 0x1;
                        } while (--ctr != 0);
                        r12 = r12 + 0xc;
                        r24 = r24 + 0x6;
                        r8 = r8 + 0x1;
                    } while ((s32)r8 < 3);
                    if ((s32)r7 < 3) {
                        r3 = 0x26;
                        ((void (*)(RegWord))soundStartSE)(r3);
                        while (1) {
                            r3 = 0x26;
                            r3 = ((u32 (*)(RegWord))GSsndGetStatus)(r3);
                            if ((s32)r3 != 2) break;
                            ((void (*)(void))_threadSwitch)();


                        }
                        r21 = r29;
                        r20 = PC_PTR32(sp) + 0xac;
                        r24 = 0x0;
                        do {
                            r23 = r21;
                            r22 = r20;
                            r26 = 0x0;
                            do {
                                tmp = *(u32*)((u8*)PC_PTR(r23) + 0x0);
                                if ((s32)tmp != 0) {
                                    tmp = 0x9;
                                    r5 = PC_PTR32(sp) + 0xa8;
                                    r4 = r30 + 0x10;
                                    ctr_fn = (void(*)(void))PC_PTR(tmp);
                                    do {
                                        r3 = *(u32*)((u8*)PC_PTR(r4) + 0x4);
                                        tmp = *(u32*)((u8*)PC_PTR(r4) + 0x8);
                                        *(u32*)((u8*)PC_PTR(r5) + 0x4) = r3;
                                        r5 += 8; *(u32*)PC_PTR(r5) = tmp;
                                    } while (--ctr != 0);
                                    if ((s32)r26 >= 0) {
                                        if ((s32)r26 < 3) {
                                            if ((s32)r24 >= 0) {
                                                if ((s32)r24 < 3) {
                                                    r19 = *(s16*)((u8*)PC_PTR(r22) + 0x6);
                                                    r3 = *(u32*)((u8*)PC_PTR(r22) + 0x0);
                                                    r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                                                    r18 = r3;
                                                    if (r18 != 0) {
                                                        if (r18 != 0) {
                                                            r4 = (s16)r19;
                                                            ((void(*)(void))GSmodelSetAnimIndex)();
                                                            f1 = *(f32*)&lbl_8047C1CC;
                                                            r3 = r18;
                                                            ((void(*)(void))GSmodelSetAnimFrame)();
                                                            f1 = *(f32*)&lbl_8047C1C8;
                                                            r3 = r18;
                                                            ((void(*)(void))GSmodelSetAnimRate)();
                                                            r3 = r18;
                                                            ((void (*)(RegWord))GSmodelStartAnimation)(r3);
                                    }
                                    }
                                    }
                                    }
                                    }
                                    }
                                    tmp = 0x0;
                                    *(u32*)((u8*)PC_PTR(r23) + 0x0) = tmp;
                                }
                                r23 = r23 + 0x4;
                                r22 = r22 + 0x8;
                                r26 = r26 + 0x1;
                            } while ((s32)r26 < 3);
                            r21 = r21 + 0xc;
                            r20 = r20 + 0x18;
                            r24 = r24 + 0x1;
                        } while ((s32)r24 < 3);
                        r4 = *(u32*)&lbl_8047C1C0;
                        r3 = 0x107E0000;
                        tmp = *(u32*)&lbl_8047C1C4;
                        r3 = r3 + 0x100b;
                        r26 = 0x0;
                        *(u32*)(sp + 0x14) = tmp;
                        r20 = *(s16*)((u8*)PC_PTR(PC_PTR32(sp)) + 0x10);
                        r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                        r19 = r3;
                        if (r19 != 0 && r19 != 0) {

                            r4 = (s16)r20;
                            ((void(*)(void))GSmodelSetAnimIndex)();
                            f1 = *(f32*)&lbl_8047C1CC;
                            r3 = r19;
                            ((void(*)(void))GSmodelSetAnimFrame)();
                            f1 = *(f32*)&lbl_8047C1C8;
                            r3 = r19;
                            ((void(*)(void))GSmodelSetAnimRate)();
                            r3 = r19;
                            ((void (*)(RegWord))GSmodelStartAnimation)(r3);
                    }
                    }
                    goto L_80088120;
                    }
                r3 = 0x4a1;
                ((void (*)(RegWord))soundStartSE)(r3);
                while (1) {
                    r3 = 0x4a1;
                    r3 = ((u32 (*)(RegWord))GSsndGetStatus)(r3);
                    if ((s32)r3 != 2) break;
                    ((void (*)(void))_threadSwitch)();


                }
                goto L_80088120;
                }
            r3 = 0x3c6;
            ((void (*)(RegWord))soundStartSE)(r3);
        L_80088120:
            r6 = 0x1;
        }
        tmp = *(u16*)((u8*)PC_PTR(r31) + 0x4);
        tmp = tmp & 0x00000020;
        if ((s32)tmp != 0 && (s32)r6 == 0) {

            if ((s32)r26 < 0) break;
            r3 = 0x3c7;
            ((void (*)(RegWord))soundStartSE)(r3);
            tmp = r26 << 3;
            r20 = PC_PTR32(sp) + 0x70;
            r20 = r20 + tmp;
            tmp = 0x9;
            r5 = PC_PTR32(sp) + 0x138;
            r4 = r30 + 0x10;
            r7 = *(u32*)((u8*)PC_PTR(r20) + 0x4);
            r6 = *(u32*)((u8*)PC_PTR(r20) + 0x0);
            ctr_fn = (void(*)(void))PC_PTR(tmp);
            do {
                r3 = *(u32*)((u8*)PC_PTR(r4) + 0x4);
                tmp = *(u32*)((u8*)PC_PTR(r4) + 0x8);
                *(u32*)((u8*)PC_PTR(r5) + 0x4) = r3;
                r5 += 8; *(u32*)PC_PTR(r5) = tmp;
            } while (--ctr != 0);
            if ((s32)r6 >= 0) {
                if ((s32)r6 < 3) {
                    if ((s32)r7 >= 0) {
                        if ((s32)r7 < 3) {
                            r4 = r7 * 0x18;
                            tmp = r6 << 3;
                            r3 = PC_PTR32(sp) + 0x13c;
                            r4 = r4 + tmp;
                            r4 = r3 + r4;
                            r21 = *(s16*)((u8*)PC_PTR(r4) + 0x6);
                            r3 = *(u32*)((u8*)PC_PTR(r4) + 0x0);
                            r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                            r19 = r3;
                            if (r19 != 0) {
                                if (r19 != 0) {
                                    r4 = (s16)r21;
                                    ((void(*)(void))GSmodelSetAnimIndex)();
                                    f1 = *(f32*)&lbl_8047C1CC;
                                    r3 = r19;
                                    ((void(*)(void))GSmodelSetAnimFrame)();
                                    f1 = *(f32*)&lbl_8047C1C8;
                                    r3 = r19;
                                    ((void(*)(void))GSmodelSetAnimRate)();
                                    r3 = r19;
                                    ((void (*)(RegWord))GSmodelStartAnimation)(r3);
            }
            }
            }
            }
            }
            }
            r3 = *(u32*)&lbl_8047C1C0;
            tmp = *(u32*)&lbl_8047C1C4;
            *(u32*)(sp + 0xC) = tmp;
            if ((s32)r26 >= 0) {
                if ((s32)r26 < 4) {
                    tmp = r26 << 1;
                    r4 = PC_PTR32(sp) + 0x8;
                    r3 = 0x107E0000;
                    r21 = *(s16*)PC_PTR((r4 + tmp));
                    r3 = r3 + 0x100b;
                    r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                    r19 = r3;
                    if (r19 != 0) {
                        if (r19 != 0) {
                            r4 = (s16)r21;
                            ((void(*)(void))GSmodelSetAnimIndex)();
                            f1 = *(f32*)&lbl_8047C1CC;
                            r3 = r19;
                            ((void(*)(void))GSmodelSetAnimFrame)();
                            f1 = *(f32*)&lbl_8047C1C8;
                            r3 = r19;
                            ((void(*)(void))GSmodelSetAnimRate)();
                            r3 = r19;
                            ((void (*)(RegWord))GSmodelStartAnimation)(r3);
            }
            }
            }
            }
            r4 = *(u32*)((u8*)PC_PTR(r20) + 0x4);
            r3 = PC_PTR32(sp) + 0x88;
            tmp = *(u32*)((u8*)PC_PTR(r20) + 0x0);
            r5 = 0x0;
            r4 = r4 * 0xc;
            r6 = 0x1;
            tmp = tmp << 2;
            tmp = r4 + tmp;
            *(u32*)PC_PTR((r3 + tmp)) = r5;
        }
        if ((s32)r6 == 0) {
            r4 = *(u16*)((u8*)PC_PTR(r31) + 0x6);
            r31 = r28;
            r24 = r27;
            r3 = 0x0;
            tmp = r4 & 0x1;
            if (((s32)tmp != 0) && ((s32)r27 > 0)) {

                r3 = 0x1;
            }
            tmp = r4 & 0x00000002;
            if (((s32)tmp != 0) && ((s32)r24 < 2)) {

                r24 = r24 + 0x1;
                r3 = 0x1;
            }
            tmp = r4 & 0x00000004;
            if (((s32)tmp != 0) && ((s32)r28 > 0)) {

                r3 = 0x1;
            }
            tmp = r4 & 0x00000008;
            if (((s32)tmp != 0) && ((s32)r31 < 2)) {

                r31 = r31 + 0x1;
                r3 = 0x1;
            }
            if ((s32)r3 != 0) {
                r6 = *(u32*)((u8*)PC_PTR(r30) + 0x5C);
                r5 = *(u32*)((u8*)PC_PTR(r30) + 0x60);
                r4 = *(u32*)((u8*)PC_PTR(r30) + 0x64);
                r3 = *(u32*)((u8*)PC_PTR(r30) + 0x68);
                tmp = *(u16*)((u8*)PC_PTR(r30) + 0x6C);
                *(u16*)(sp + 0x58) = tmp;
                if ((s32)r31 >= 0 && (s32)r31 < 3 && (s32)r24 >= 0 && (s32)r24 < 3) {


                    r5 = r24 * 0x6;
                    tmp = r31 << 1;
                    r3 = 0x107E0000;
                    r4 = PC_PTR32(sp) + 0x48;
                    tmp = r5 + tmp;
                    r3 = r3 + 0x1009;
                    r20 = *(s16*)PC_PTR((r4 + tmp));
                    r3 = ((u32 (*)(RegWord))GSresFindById)(r3);
                    r18 = r3;
                    if (r18 != 0) {
                        if (r18 != 0) {
                            r4 = (s16)r20;
                            ((void(*)(void))GSmodelSetAnimIndex)();
                            f1 = *(f32*)&lbl_8047C1CC;
                            r3 = r18;
                            ((void(*)(void))GSmodelSetAnimFrame)();
                            f1 = *(f32*)&lbl_8047C1C8;
                            r3 = r18;
                            ((void(*)(void))GSmodelSetAnimRate)();
                            r3 = r18;
                            ((void (*)(RegWord))GSmodelStartAnimation)(r3);
                        }
                        r3 = r18;
                        r4 = 0x0;
                        ((void (*)(RegWord, RegWord))GSmodelSetAnimType)(r3, r4);
                        while (1) {
                            r3 = r18;
                            r3 = ((u32 (*)(RegWord))GSmodelIsAnimating)(r3);
                            tmp = r3 & 0xFF;
                            if (tmp == 0) break;
                            ((void (*)(void))_threadSwitch)();


                        }
                }
                }
                r28 = r31;
                r27 = r24;
            }
        }
        ((void (*)(void))_threadSwitch)();

    }

    if ((s32)r26 < 0) {
        r3 = 0x1;
    } else {

        r3 = 0x0;
    }
    return;
}
#endif

#endif

/* Renumber the four party slot ids in the live save (inlined at each use). */
static inline void gbaConvResetSlotIds(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(s32 side, s32 slotType);
#endif
    u32 i;

    for (i = 0; i < 4; i++) {
        u16 slot = i;
        *(u16*)PC_PTR((savedataGetStatus(0, 0xE) + i * 0x1660 + 0x59AA)) = slot;
        *(u16*)PC_PTR((savedataGetStatus(0, 0xE) + i * 0x1660 + 0x26)) = slot;
    }
}

/* Fill the player memo from the hero's current state (inlined). */
static inline void gbaConvCaptureMemo(GbaConvPlayerMemo* memo) {
    extern void heroMoveGetHeroRot(f32*);
    extern void heroMoveGetHeroPos(f32*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std092_getCurrentRoomID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 floorGetPrevFloorID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std082_flagGetValue(s32);
#endif
    f32 a[3];
    f32 b[3];

    heroMoveGetHeroPos(a);
    heroMoveGetHeroRot(b);
    memo->posX = a[0];
    memo->posY = a[1];
    memo->posZ = a[2];
    memo->rotX = b[0];
    memo->rotY = b[1];
    memo->rotZ = b[2];
    memo->unk04 = std092_getCurrentRoomID();
    memo->prevFloorId = floorGetPrevFloorID();
    memo->valid = 1;
    memo->flagAfc = std082_flagGetValue(0xafc);
    memo->flagAfd = std082_flagGetValue(0xafd);
    memo->flagB11 = std082_flagGetValue(0xb11);
    memo->flagDe1 = std082_flagGetValue(0xde1);
}

/* Snapshot the save image, renumber the slots, run link mode 6 and restore. */
static inline s32 gbaConvRunLinkMode(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8006A76C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* menuCommonAlloc(u32 size);
#endif
    extern void menuCommonFree(void* p);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataBiosGetNowSavedataPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(s32 side, s32 slotType);
#endif
    extern void _flagSet(s32 id, s32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 memcardExecTask(s32 a, s32 b, s32 c);
#endif
    void* buf;
    s32 result;

    if (fn_8006A76C() != 0) {
        return 0;
    }
    buf = PC_PTR(menuCommonAlloc(0x1DFD0));
    memcpy(buf, PC_PTR(savedataBiosGetNowSavedataPtr()), 0x1DFD0);
    gbaConvResetSlotIds();
    ((u8*)PC_PTR(savedataGetStatus(0, 0xE)))[0x1C] = 0;
    _flagSet(0x8AE, 0);
    result = memcardExecTask(6, 2, 0);
    memcpy(PC_PTR(savedataBiosGetNowSavedataPtr()), buf, 0x1DFD0);
    menuCommonFree(buf);
    if (result == 4) {
        return 0;
    }
    return -1;
}

/* The species grid's initializer lands in .rodata ahead of menuSaveLoadSaveGame's
 * pooled assert strings; that unit's wrapper leaves dnaKeyCodeCheck out so the
 * strings keep their retail base. */
#if !defined(GBA_CONV_800889E4_ONLY)
/* 3x3 table of species ids searched by dnaKeyCodeCheck (passed by value). */
typedef struct GbaConvSpeciesGrid {
    u16 ids[3][3];
} GbaConvSpeciesGrid;

static inline u16 gbaConvGridAt(GbaConvSpeciesGrid grid, s32 row, s32 col) {
    if (col < 0 || col >= 3 || row < 0 || row >= 3) {
        return 0;
    }
    return grid.ids[row][col];
}

/* Checks that every id in state->ids appears in the species grid; returns 2
 * when one is missing, otherwise waits for the GBA side and returns its
 * result. */
static inline u32 gbaConvCheckIds(GbaConvChannelState* state) {
    extern void std098_mapWarpToMenuRoom(s32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
    s32 i;
    GbaConvSpeciesGrid grid;
    u16* id;

    /* lbl_8026F488 starts with the 3x3 grid {{0xB9, 0xC8, 0xF4}, {0x11F, 0x9C, 0xF3}, {0x99, 0xF5, 0x9F}}. */
    /* RULE-EXCEPTION(user-approved): typed view of the retail rodata blob lbl_8026F488 - see docs/RULE_EXCEPTIONS.md */
    grid = *(const GbaConvSpeciesGrid*)lbl_8026F488;
    id = state->ids;
    for (i = 0; i < 3; id++, i++) {
        u16 target = *id;
        GbaConvSpeciesGrid search = grid;
        /* RULE-EXCEPTION(user-approved): carrier copy of the grid per id - see docs/RULE_EXCEPTIONS.md */
        GbaConvSpeciesGrid rows = search;
        s32 found;
        s32 row;

        for (row = 0; row < 3; row++) {
            /* RULE-EXCEPTION(user-approved): carrier copy of the grid per row - see docs/RULE_EXCEPTIONS.md */
            GbaConvSpeciesGrid g = rows;
            s32 col;

            for (col = 0; col < 3; col++) {
                /* RULE-EXCEPTION(user-approved): carrier copy of the grid per column - see docs/RULE_EXCEPTIONS.md */
                GbaConvSpeciesGrid h = g;
                u16 v = gbaConvGridAt(h, row, col);

                if (v == target) {
                    found = 1;
                    goto checked;
                }
            }
        }
        found = 0;
    checked:
        if (!found) {
            return 2;
        }
    }
    std098_mapWarpToMenuRoom(0x387);
    _threadSwitch();
    return ((GbaConvChannelState*)lbl_803FB2F8)->result;
}

#endif

#if !defined(GBA_CONV_CARVE)
/* 0x80088428 | size: 0x94 */
void fn_80088428(void) {
    #pragma peephole off
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80087C64(void* p);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE(s32);
#endif
    extern void cameraWaitSyncAnime(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorBattleEndTerminateScripts(void);
#endif
    GbaConvChannelState* state = (GbaConvChannelState*)&lbl_803FB2F8;
    u32 r31;

    soundStartSE(0x27);
    cameraPlayAnime(0x62d, 0x107C1800, 0, 0);
    cameraWaitSyncAnime(1);
    r31 = fn_80087C64(&lbl_803FB2F8);
    soundStartSE(0x28);
    cameraPlayAnime(0x62d, 0x107D1800, 0, 0);
    cameraWaitSyncAnime(1);
    floorBattleEndTerminateScripts();
    state->result = r31;
}
#endif

#if !defined(GBA_CONV_CARVE) || defined(GBA_CONV_CARVE_800884BC)
#if !defined(GBA_CONV_800889E4_ONLY)
/* 0x800884BC | size: 0x214 */
u32 dnaKeyCodeCheck(u16 a, u16 b, u16 c) {
    GbaConvChannelState* state = (GbaConvChannelState*)lbl_803FB2F8;

    state->ids[0] = a;
    state->ids[1] = b;
    state->ids[2] = c;
    return gbaConvCheckIds(state);
}
#endif
#endif

#if !defined(GBA_CONV_CARVE) || defined(GBA_CONV_CARVE_800886D0)
/* 0x800886D0 | size: 0x294 */
s32 menuSaveLoadReceiveGift(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8006A76C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 savedataBattleGetSavedHeroKind(void);
#endif
    extern u8 fn_801EE398(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 std081_flagCheckTrue(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* menuCommonAlloc(u32 size);
#endif
    extern void menuCommonFree(void* p);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataBiosGetNowSavedataPtr(void);
#endif
    extern s16 std217_playerReceiveGiftPokemon(s32 kind);
    extern void d2presentOpen(s32 mode);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 savedataBattleGetCouponRun(void);
#endif
    extern void savedataBattleSetCouponRun(s32 value);
    extern void heroAddPokecoupon(s32 hero, s32 amount);
    extern void std078_flagSetFalse(s32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpen(s32 a, s32 msg, s32 c, s32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 menuSubOpenYesNo(s32 a, s32 b, s32 c, s32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(s32 side, s32 slotType);
#endif
    extern void _flagSet(s32 id, s32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 memcardExecTask(s32 a, s32 b, s32 c);
#endif
    void* buf;
    s32 mode;
    s32 result;
    s32 coupons;

    if (fn_8006A76C() != 0) {
        return 0;
    }
    if (savedataBattleGetSavedHeroKind() != 0) {
        return 0;
    }
    if (fn_801EE398() == 0) {
        return 0;
    }
    if (std081_flagCheckTrue(0x478) == 0) {
        return 0;
    }
    if (std081_flagCheckTrue(0xAFD) != 0) {
        return 0;
    }
    buf = PC_PTR(menuCommonAlloc(0x1DFD0));
    memcpy(buf, PC_PTR(savedataBiosGetNowSavedataPtr()), 0x1DFD0);
    switch (std217_playerReceiveGiftPokemon(4)) {
    case -1:
        mode = 1;
        break;
    case -2:
        mode = 2;
        break;
    default:
        mode = 0;
        break;
    }
    d2presentOpen(mode);
    if (mode == 2) {
        memcpy(PC_PTR(savedataBiosGetNowSavedataPtr()), buf, 0x1DFD0);
        menuCommonFree(buf);
        return 0;
    }
    coupons = savedataBattleGetCouponRun();
    savedataBattleSetCouponRun(0);
    heroAddPokecoupon(0, coupons);
    std078_flagSetFalse(0xAFD);
    for (;;) {
        winMsgOpen(2, 0x3C37, 1, 0);
        if (menuSubOpenYesNo(0, -1, -1, 0) == 0) {
            gbaConvResetSlotIds();
            ((u8*)PC_PTR(savedataGetStatus(0, 0xE)))[0x1C] = 0;
            _flagSet(0x8AE, 0);
            result = memcardExecTask(4, 2, 0);
            if (result == 4 || result == 5) {
                break;
            }
        }
        winMsgOpen(2, 0x3C0F, 1, 0);
        if (menuSubOpenYesNo(0, -1, -1, 1) == 0) {
            memcpy(PC_PTR(savedataBiosGetNowSavedataPtr()), buf, 0x1DFD0);
            savedataBattleSetCouponRun(0);
            menuCommonFree(buf);
            return 0;
        }
    }
    menuCommonFree(buf);
    return 1;
}
#endif

#if !defined(GBA_CONV_CARVE)
/* 0x80088964 | size: 0x40 */
s32 fn_80088964(void) {
    #pragma peephole off
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 memcardExecTask(s32, s32, s32);
#endif

    if (memcardExecTask(0xd, 2, 0) == 4) {
        return 0;
    } else {
        return -1;
    }
}

/* 0x800889A4 | size: 0x40 */
s32 fn_800889A4(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 memcardExecTask(s32, s32, s32);
#endif

    if (memcardExecTask(5, 2, 0) == 4) {
        return 0;
    } else {
        return -1;
    }
}
#endif

#if !defined(GBA_CONV_CARVE) || defined(GBA_CONV_CARVE_800889E4)
/* 0x800889E4 | size: 0x27C */
s32 menuSaveLoadSaveGame(s32 capture) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(s32 side, s32 slotType);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 savedataBattleGetFlagC98B(u8* status);
#endif
    extern void savedataBattleSetFlagC98B(u8* status);
    extern void savedataBattleClearFlagC98B(u8* status);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void __assert(const char* file, s32 line, const char* expr);
#endif
    u8* status;
    GbaConvSaveHeader* hdr;
    GbaConvPlayerMemo* memo;
    s32 result;

    status = PC_PTR(savedataGetStatus(0, 0xE));
    hdr = (GbaConvSaveHeader*)(status + 0xC98C);
    memo = (GbaConvPlayerMemo*)(status + 0xC9A8);
    if (savedataBattleGetFlagC98B(status) != 0) {
        __assert("menuCB_SaveLoad.c", 0x7B, "!menuCBBios_SaveDataAvailable(p)");
    }
    hdr->kind = ((GbaConvSaveHeader*)status)->kind;
    hdr->unk04[0] = ((GbaConvSaveHeader*)status)->unk04[0];
    hdr->unk04[1] = ((GbaConvSaveHeader*)status)->unk04[1];
    hdr->unk04[2] = ((GbaConvSaveHeader*)status)->unk04[2];
    hdr->unk04[3] = ((GbaConvSaveHeader*)status)->unk04[3];
    hdr->unk04[4] = ((GbaConvSaveHeader*)status)->unk04[4];
    hdr->unk04[5] = ((GbaConvSaveHeader*)status)->unk04[5];
    if (capture != 0) {
        if (hdr->kind != 1) {
            __assert("menuCB_SaveLoad.c", 0x89, "state->m_eBattleMode == BATTLEMODE_BATTLEYAMA100");
        }
        gbaConvCaptureMemo(memo);
    } else if (hdr->kind == 1) {
        memo->valid = 1;
    } else {
        memset(memo, 0, sizeof(GbaConvPlayerMemo));
    }
    savedataBattleSetFlagC98B(status);
    result = gbaConvRunLinkMode();
    if (result < 0) {
        savedataBattleClearFlagC98B(status);
    }
    return result;
}
#endif

#if !defined(GBA_CONV_CARVE) || defined(GBA_CONV_CARVE_80088C60)
/* 0x80088C60 | size: 0x124 */
s32 menuSaveLoadCommitA(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_8006A76C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* menuCommonAlloc(u32 size);
#endif
    extern void menuCommonFree(void* p);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataBiosGetNowSavedataPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataGetStatus(s32 side, s32 slotType);
#endif
    extern void _flagSet(s32 id, s32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 memcardExecTask(s32 a, s32 b, s32 c);
#endif
    void* buf;
    s32 result;

    if (fn_8006A76C() != 0) {
        return 0;
    }
    buf = PC_PTR(menuCommonAlloc(0x1DFD0));
    memcpy(buf, PC_PTR(savedataBiosGetNowSavedataPtr()), 0x1DFD0);
    gbaConvResetSlotIds();
    ((u8*)PC_PTR(savedataGetStatus(0, 0xE)))[0x1C] = 0;
    _flagSet(0x8AE, 0);
    result = memcardExecTask(6, 2, 0);
    memcpy(PC_PTR(savedataBiosGetNowSavedataPtr()), buf, 0x1DFD0);
    menuCommonFree(buf);
    if (result == 4) {
        return 0;
    }
    return -1;
}

/* 0x80088D84 | size: 0x124 */

s32 fn_80088D84(void) {
    return gbaConvRunLinkMode();
}
#endif

#if !defined(GBA_CONV_CARVE)
/* 0x80088EA8 | size: 0xB0 */
void menuSaveLoadStorePosition(u8* p) {
    extern void heroMoveGetHeroRot(f32*);
    extern void heroMoveGetHeroPos(f32*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std092_getCurrentRoomID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 floorGetPrevFloorID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std082_flagGetValue(s32);
#endif
    GbaConvPlayerMemo* memo = (GbaConvPlayerMemo*)p;
    f32 a[3];
    f32 b[3];

    heroMoveGetHeroPos(a);
    heroMoveGetHeroRot(b);
    memo->posX = a[0];
    memo->posY = a[1];
    memo->posZ = a[2];
    memo->rotX = b[0];
    memo->rotY = b[1];
    memo->rotZ = b[2];
    memo->unk04 = std092_getCurrentRoomID();
    memo->prevFloorId = floorGetPrevFloorID();
    memo->valid = 1;
    memo->flagAfc = std082_flagGetValue(0xafc);
    memo->flagAfd = std082_flagGetValue(0xafd);
    memo->flagB11 = std082_flagGetValue(0xb11);
    memo->flagDe1 = std082_flagGetValue(0xde1);
}

/* 0x80088F58 | size: 0x1C */
/* NAME-T1 fn_80088F58 */ /* XD's map, dbgMenuToolBattle.o: the GBA debug entries */
s32 dbgMenuGBAClearCouponFlag(void) {
    u32 count;

    count = *(volatile u32*)&dbgMenuGBAClearCouponFlag_on;
    count++;
    *(volatile u32*)&dbgMenuGBAClearCouponFlag_on = count;
    *(volatile u32*)&dbgMenuGBASetClearFlag_on = 0;
    return 0;
}

/* 0x80088F74 | size: 0x14 */
/* NAME-T1 fn_80088F74 */
s32 dbgMenuGBASetClearFlag(void) {
    *(u32*)&dbgMenuGBASetClearFlag_on += 1;
    return 0;
}

/* 0x80088F88 | size: 0x1C */
/* NAME-T1 fn_80088F88 */
s32 dbgMenuGBAResetCoupon(void) {
    u32 count;

    count = *(volatile u32*)&dbgMenuGBAResetCoupon_on;
    count++;
    *(volatile u32*)&dbgMenuGBAResetCoupon_on = count;
    *(volatile u32*)&dbgMenuGBAAddCoupon_value = 0;
    return 0;
}

/* 0x80088FA4 | size: 0x54 */
s32 dbgMenuGBAAddCoupon(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32);
#endif
    s32 r31;

    r31 = menuOpen(2, 1);
    menuClose(2);
    if (r31 >= 0) {
        *(u32*)&dbgMenuGBAAddCoupon_value += r31;
    }
    return 0;
}

/* 0x80088FF8 | size: 0x30 */
s32 dbgToolBattleDebugSetAGBConnectionMode(s32 a, s32 b) {
    fn_80089030(b == 0);
    return 0;
}

/* 0x80089028 | size: 0x8 */
s32 fn_80089028(void) {
    return lbl_80478958;
}

/* 0x80089030 | size: 0x18 */
void fn_80089030(u8 x) {
    lbl_80478958 = (x == 0);
}
#endif
