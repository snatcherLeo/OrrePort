/**
 * @file kaisuu.c
 * @brief kaisuu -- random-count-in-range helper family (Colosseum "menu"
 *        UI subsystem support). Split from the gs_model.c splitter bucket
 *        (address range 0x80101A70 - 0x80101B34, 3 fns); corresponds to
 *        XD's kaisuu.cpp (0x8010C320-0x8010C3E4). XD order is
 *        GetKaisuu/Max/Min; Colosseum's link order rotated to Max/Min/
 *        GetKaisuu (verified single TU via exact size matches).
 */
#include "dolphin/types.h"

/* ===== External SDK / engine functions ===== */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
extern void  GSlogWrite(const char* fmt, ...);         /* OSReport / GSlog */

/* GSmem */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   _toolentryAlloc__FUl(u32 size);                     /* GSmemAllocRaw */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);                   /* GSmemGetPtr */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u16 handle);                   /* GSmemLock */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmemFree(u16 handle);                   /* GSmemFree */
#endif

/* GSgfx state */

/* Matrix math */
extern void  PSMTXIdentity(void);                         /* MTXIdentity */
extern void  PSMTXCopy(void* mtxA, void* mtxB);      /* MTXConcat */
extern void  PSMTXMultVec(void* mtx, void* vec, void* out); /* MTXMultVec3 */

/* Model resource table (BSS) */
extern u8 lbl_80402518[];  /* model resource table -- 0x2400 bytes */

/* Global model system state block */
extern u8 winSysWork[];

typedef struct GSModelStateHeader {
    u16 count;          /* 0x00 */
    u16 entryHandle;    /* 0x02 */
    u32 unk_04;         /* 0x04 */
    void* entries;      /* 0x08 */
} GSModelStateHeader;

#define GS_MODEL_STATE ((GSModelStateHeader*)winSysWork)

/* Resource index table */
extern u32 kaisuu_data_number;   /* max resource index (sda21) */
extern u8  kaisuu_data[]; /* resource table, 8-byte entries */

/* Additional externs used by various functions */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmodelFree(u32);     /* GSmem release/unref */
#endif
extern void  GScameraFree(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuDataBiosGetPtr(void);    /* linked list head */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuItemBiosGetPtr(s16 idx); /* node by index */
#endif
extern void* menuSeBiosGetPtr(s32);
extern u16   menuSeTableLookup(void*, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuSpriteBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int   soundStartSE(u16);
#endif
extern s32   GSthreadGetCurrentThread(void);    /* poll/yield -- 0 if pending */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  _threadSwitch(void);    /* yield */
#endif
extern u32   fn_800BE31C(void);    /* rand or tick */
extern u32   GXSetDrawSyncCallback(void*);   /* register fn, returns handle */
extern void  fn_800BD91C(s32, s32);
extern void  GXSetDrawSync(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSgfxBeginBackFBCapture(u32, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   GStextureCreate(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   GSmodelCanAnimate(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmodelSetAnimIndex(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmodelSetAnimRate(u32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmodelStartAnimation(u32);
#endif
extern void  wazaSequenceOwnerFree(u32);
extern u32   OSGetTick(void);

extern u32 lbl_8047ACF0;
extern u32 lbl_8047ACF4;  /* function pointer for callback */
extern u32 lbl_8047ACF8;  /* saved tick */
extern u32 lbl_8047ACEC;  /* tick counter */
extern u32 lbl_8047ACE8;  /* tick base */
extern u8 lbl_80478B28;   /* max slot byte */
extern u8 _menuOffScreenEnable;
extern u8 _menuOffScreenUsing;
extern u8 _menuOffScreenDisp;
extern u8 lbl_8047AD23;
extern u8 _menuOffScreenType;
extern f32 lbl_8047AD2C;
extern f32 lbl_8047AD30;
extern f32 lbl_8047AD34;
extern f32 lbl_8047AD38;
extern f32 lbl_8047AD3C;
extern u32 _menuOffScreenTex;
extern u16 lbl_8047AD18;  /* GSmem handle */
extern u8* lbl_8047AD1C;  /* object pool pointer */
extern f32 lbl_8047CDC0;  /* sdata2: float constant */
extern f32 lbl_8047CDC4;  /* sdata2: float constant */
extern u16 lbl_8047CDE0;  /* sdata2: */
extern u16 lbl_8047CDE4;  /* sdata2: */
extern f32 lbl_8047CD80;  /* sdata2: float constant */
extern f32 lbl_8047CD84;  /* sdata2: float constant */
extern f32 lbl_8047CD88;  /* sdata2: float constant */
extern f32 lbl_8047CD8C;  /* sdata2: float constant */
extern f32 lbl_8047CD90;  /* sdata2: float constant */
extern f32 lbl_8047CD94;  /* sdata2: float constant */
extern f64 lbl_8047CD98;  /* sdata2: double constant */
extern f32 lbl_8047CE3C;  /* sdata2: float constant */
extern f32 lbl_8047CE50;  /* sdata2: float constant */
extern f32 lbl_8047CE5C;  /* sdata2: float constant */
extern f32 lbl_8047CE70;  /* sdata2: float constant */
extern u8  lbl_80404A98[];  /* table for display */
extern u8  lbl_80271E10[];  /* format string */
extern u8  lbl_80271E4C[];  /* format string */
extern u8  lbl_80271EE8[];  /* format string */
extern u8  lbl_80271F18[];  /* format string */
extern u8  lbl_8035B060[];  /* module name string */
extern u8  lbl_8035B3F0[];  /* module name string */

/* Additional external functions (not already declared above) */
extern void fn_800BF74C(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(f32, f32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos(f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif

/* Forward declarations for functions defined later in this TU */
extern u8    menuOffScreenCheckEnable(u8 param);
extern void  windowClose(void* ptr, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowSearchID(s32 param);
#endif
extern s32   _menuCBOffScreen__FP9GStextureUlPv(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winSpriteSetDisp(void* node, u32 enable);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  windowGetValue(s32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  windowCheckCursor(void* p, u8 flags);
#endif
extern void  windowDrawSprite2(void* r3, void* r4, s16 r5, s16 r6, s32 r7, s32 r8, s32 r9, s32 r10);
extern u8    menuOffScreenFadeSync(u8 param);
extern void  menuOffScreenFadeSet(f32 f1, f32 f2);
extern u8    menuOffScreenCreate(u32 param);
extern void  menuOffScreenRelease(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowGetKeyInfo(void);
#endif
extern void* menuSeqBiosGetPtr(u32 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowSearchItemID(void* head, s32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  menuOpenCustom(void* p, u32 r4, s32 r5, s32 r6, void* r7, s32 r8, ...);
#endif
extern u8    menuOffScreenSetPriority(u8 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    menuOffScreenSetDisp(u8 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   GSgetFrameStep(void);
#endif
extern u8    lbl_80404B68[];  /* scratch table for fn_80107F38, fn_801081F8 */
extern u8    lbl_80404B8C[];  /* scratch table for winSeqSetMenu */
extern u8    _winMsgOpenErrWin;     /* error window open (2026-10-16; was guessed as a resource request gate) */

/* Cross-segment prototypes for menu/window-engine functions defined in
 * sibling segment files split from the gs_model.c splitter bucket. */
extern void wazaSequenceSysClearWork(void);
extern s32 kaisuuBiosGetMax(u32 index);
extern s32 kaisuuBiosGetMin(u32 index);
extern s32 kaisuuGetKaisuu(u32 param);
extern void fn_80101B34(u32 param);
extern void fn_80101B88(u32 val);
extern void fn_80101B90(void);
extern void fn_80101D5C(void);
extern void fn_80101D8C(void);
extern void fn_80101FB8(u8 param);
extern void menuGetOffScreenFlag(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuReleaseOffScreen(f32 f1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCreateOffScreen(f32 param);
#endif
extern s32 menuGetSelectItemNum(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorFromItemID(void* unused, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801021F8(void* p, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuSetDisp(void* p, u32 enable);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuGetCursorItemID(void* p, u32 target);
#endif
extern s32 menuSetCursor(void* p, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursor(void* p);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuCloseSync(void* p, u8 flag);
#endif
extern void menuCloseFloor(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801024E8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose(s32 p);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuCloseCustom(void* p, u32 mode, u8 wait);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuIsCheck(s32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuOpen(void* p, void* q);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuOpenCustom(void* p, u32 r4, s32 r5, s32 r6, void* r7, s32 r8, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuSetPosition(void* p, s16 a, s16 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void* p);
#endif
extern void menuPlaySe(void* p, void* q);
extern void fn_801034DC(void);
extern void _menuGetGcKeyInfo__FlPUs(void);
extern void _menuUpdateKeyInfo__FP15WINDOW_SYS_WORK(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuGetKeyInfo(void);
#endif
extern u8 menuGetEnablePort(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuSetEnablePort(u8 val);
#endif
extern void menuInit(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 cursorBiosGetPos(u16 idx);
#endif
extern u32 cursorBiosSetPos(u16 idx, u16* out);
extern void cursorBiosInit(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80103F74(void* head, u16 key, u32 data);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowGetAllocPtr(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowAllocMemory(void* p, s32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowGetFreeWork(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowSetParam(void* ptr, u32 idx, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 windowGetParam(void* ptr, u32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowDrawSprite(void* p, void* a, void* b, u16 key, u32 data);
#endif
extern void windowDrawSprite2(void* r3, void* r4, s16 r5, s16 r6, s32 r7, s32 r8, s32 r9, s32 r10);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetCursorToItem(u8* arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetValue(s32 param);
#endif
extern s32 fn_801044D0(s32 param, u16* val);
extern void windowGetCursor(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowCheckCursor(void* p, u8 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 windowGetActiveID(void);
#endif
extern void windowCloseMain(void* obj);
extern void windowClose(void* ptr, u32 flags);
extern void _windowCreateItemSprite__FP14tagWINDOW_WORK(void);
extern void windowCreateCursorSprite(void);
extern void windowOpen(void);
extern void _winCalcWindowSize__FlPC13MENU_ITEM_dd_PsPs(void);
extern void windowInit(u16 count);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetPortKeyInfo(void);
#endif
extern void fn_80105634(void);
extern void winMsgDraw(void);
extern void winMsgCtrl(void);
extern void winMsgButton(void* p);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgCloseLevelUpStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenLevelUpFiledStatus(void);
#endif
extern void winMsgOpenLevelUpStatus(void);
extern void winMsgCloseError(void);
extern void winMsgOpenError(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgCloseFight(void);
#endif
extern void winMsgCloseCheckFight(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenFightNoWait(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenFight(void);
#endif
extern void winMsgCheckField(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgCloseField(void);
#endif
extern void winMsgOpenFieldWithSE(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenField(void);
#endif
extern void winMsgCheck(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenWithSE(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen(void);
#endif
extern void fn_80106F98(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 winSeqCheckMove(s32 param);
#endif
extern s32 fn_80107170(s32 r3, s32 r31);
extern void winSeqMoveMenu(void);
extern s32 fn_80107E78(void* r3, s32 r4, u16 r30);
extern s32 winSeqIsCheck(s32 r3, u16 r30);
extern void fn_80107F38(s32 param, u32 key);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqSetMenu(s32 param, u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801081F8(void* r3_arg, u16 r4, u16 r5);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSetSequence(void* out, u32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 winSpriteGetDisp(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp(void* node, u32 enable);
#endif
extern void winSpriteRelease(void* head);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_80109290(void* root);
#endif
extern void winSpriteInit(void);
extern void menuOffScreenDraw(void);
extern u8 menuOffScreenFadeSync(u8 param);
extern void menuOffScreenFadeSet(f32 f1, f32 f2);
extern u8 menuOffScreenSetPriority(u8 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuOffScreenSetDisp(u8 val);
#endif
extern u8 menuOffScreenIsDoing(void);
extern u32 menuOffScreenGetPtr(void);
extern u8 menuOffScreenCheckEnable(u8 param);
extern void menuOffScreenRelease(void);
extern u8 menuOffScreenCreate(u32 param);
extern void menuOffScreenInit(void);
extern s32 _menuCBOffScreen__FP9GStextureUlPv(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuModelSetMotion(void* p, u32 val);
#endif
extern void menuModelRender(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuModelCheck(void* obj, u8 wait);
#endif
extern s32 menuModelFree(void* p);

/* 0x80101A70 | 0x2C */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (dont_inline on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma dont_inline on
s32 kaisuuBiosGetMax(u32 index) {
    struct Entry {
        s32 start;
        s32 end;
    };
    struct Entry* table;
    if (index >= kaisuu_data_number) { return 0; }
    table = (struct Entry*)kaisuu_data;
    return table[index].end;
}

/* 0x80101A9C | 0x28 */
/* Returns value at entry[index].field0 (lwzx from offset 0) in the 8-byte table */
s32 kaisuuBiosGetMin(u32 index) {
    if (index >= kaisuu_data_number) { return 0; }
    return *(s32*)(kaisuu_data + index * 8);
}
#pragma dont_inline reset

/* 0x80101AC4 | 0x70 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSrand(void);  /* random or tick */
#endif
s32 kaisuuGetKaisuu(register u32 param) {
    s32 max;
    int base;
    max = kaisuuBiosGetMax(param);
    base = kaisuuBiosGetMin(param);
    {
        s32 min;
        u32 tick;
        s32 range;
        s32 mod;
        min = base;
        if (max == min)
            return min;
        tick = GSrand();
        range = max - min;
        mod = (s32)(u16)tick % (s32)(range + 1);
        return base + mod;
    }
}
