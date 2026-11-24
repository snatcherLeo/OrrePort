/**
 * @file gba_conv_r49_80088428_prefix.c
 * @brief gba_conv.c carve: fn_80088428, 0x80088428 - 0x800884BC.
 *
 * Standalone: gba_conv.c's declarations plus only these bodies, copied
 * from gba_conv.c.
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
void dnaKeyCodeCheck(void);
void menuSaveLoadReceiveGift(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_80088964(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fn_800889A4(void);
#endif
void menuSaveLoadSaveGame(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void menuSaveLoadCommitA(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80088D84(void);
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


/* 0x80088428 | size: 0x94 */
void fn_80088428(void) {
    /* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
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

