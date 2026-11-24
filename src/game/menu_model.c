/**
 * @file menu_model.c
 * @brief menuModel -- GSmodel-backed menu 3D model motion/render/free
 *        wrapper (Colosseum "menu" UI subsystem; the only segment of
 *        this bucket that actually touches GSmodel). Split from the
 *        gs_model.c splitter bucket (address range 0x80109894 -
 *        0x80109C88, 4 fns). Corresponds to XD's menuModel.cpp
 *        (0x801121E8-0x80114D30, bounded by the __sinit_menuModel_cpp
 *        marker); menuModelSetMotion is an exact size match. This is a
 *        truncated TU head -- the remainder of menuModel.cpp's
 *        functions live past this bucket's end, in
 *        gs_range_80109C88.c (0x80109C88-0x8010C364).
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
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaSequenceOwnerGetModel(u32 sequence);
#endif
extern void fn_800EC134(u32 model);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetPEdescr(u32 model, void* descriptor);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelResetPEdescr(u32 model);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelDrawModel(u32 model, u32 flags);
#endif
extern void GSlightSetActive(u32 light, u32 active);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GScameraGetActiveCamera(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D4604(u32 layer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D377C(u32 state);
#endif
extern void fn_800D9B24(u16* left, u16* top, u16* right, u16* bottom);
extern void fn_800D9AF0(u16* left, u16* top, u16* right, u16* bottom);
extern void fn_800D258C(u32 camera);
extern u32 GStextureGetXsize(u32 texture);
extern u32 GStextureGetYsize(u32 texture);
extern void gfxSetScissor(u16 left, u16 top, u16 right, u16 bottom);
extern void gfxSetViewport(u16 left, u16 top, u16 right, u16 bottom);
extern void _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgfxRenderLayer(u32 texture, u32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerSetVisibility(u32 sequence, u32 active);
#endif
extern void wazaSequenceOwnerSetParticleVisibility(u32 sequence, u32 active);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceSysUpdate(void);
#endif
extern void wazaSequenceOwnerDraw(u32 sequence);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgfxRenderEnd(void);
#endif
extern u8 lbl_8035B468[];

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
extern u8    _winMsgOpenErrWin;     /* resource request gate byte (sda21) - authoritative decl, use as-is */

/* Cross-segment prototypes for menu/window-engine functions defined in
 * sibling segment files split from the gs_model.c splitter bucket. */
extern void wazaSequenceSysClearWork(void);
extern void* kaisuuBiosGetMax(u32 index);
extern void* kaisuuBiosGetMin(u32 index);
extern u32 kaisuuGetKaisuu(u32 param);
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
extern u32 menuModelRender(u8* work);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuModelCheck(void* obj, u8 wait);
#endif
extern s32 menuModelFree(void* p);

/* 0x80109894 | 0xA0 */
#if !defined(MENU_MODEL_SPLIT) || defined(MENU_MODEL_SETMOTION)
/* RULE-EXCEPTION(user-approved): local peephole control -- see docs/RULE_EXCEPTIONS.md. */
#if !defined(NO_LOCAL_PRAGMAS_MENU_MODEL_EXACT_SETMOTION)
#endif
#if !defined(NO_LOCAL_PRAGMAS_MENU_MODEL_EXACT_SETMOTION)
#endif
s32 menuModelSetMotion(void* p, u32 val) {
    void* r31 = p;
    if (r31 == (void*)0) { return 0; }
    if (*(u8*)((u8*)r31 + 0x1) != 0) {
        if (*(u8*)((u8*)r31 + 0x4) == 0) {
            *(u32*)((u8*)r31 + 0xc) = val;
        }
    } else {
        if (*(u8*)((u8*)r31 + 0x14) == 0) {
            *(u32*)((u8*)r31 + 0x1c) = val;
            {
                u32 r3 = *(u32*)((u8*)r31 + 0x24);
                if ((u8)GSmodelCanAnimate(PC_PTR(r3)) != 0) {
                    GSmodelSetAnimIndex(PC_PTR(*(u32*)((u8*)r31 + 0x24)), *(u32*)((u8*)r31 + 0x1c));
                    GSmodelSetAnimRate(PC_PTR(*(u32*)((u8*)r31 + 0x24)), lbl_8047CE70);
                    GSmodelStartAnimation(PC_PTR(*(u32*)((u8*)r31 + 0x24)));
                }
            }
        }
    }
    return 1;
}
#if !defined(NO_LOCAL_PRAGMAS_MENU_MODEL_EXACT_SETMOTION)
#endif
#endif

/* 0x80109934 | 0x25C */
#if !defined(MENU_MODEL_SPLIT) || defined(MENU_MODEL_RENDER)
/* RULE-EXCEPTION(title-path): copy propagation is disabled for this carve,
 * and width also holds the first light-loop index to preserve register
 * lifetimes. See docs/RULE_EXCEPTIONS.md. */
u32 menuModelRender(u8* work) {
    u8* lightCursor;
    s32 i;
    u32 resetModel;
    u32 renderModel;
    u32 previousCamera;
    u16 viewLeft;
    u16 viewTop;
    u16 viewRight;
    u16 viewBottom;
    u16 scissorLeft;
    u16 scissorTop;
    u16 scissorRight;
    u16 scissorBottom;
    u16 height;
    s32 width;

    if (work == NULL) {
        return 0;
    }
    if (*(u32*)(work + 0x34) == 0) {
        return 0;
    }
    if (work[0] != 2) {
        return 0;
    }
    renderModel = work[0x14] != 0 ? wazaSequenceOwnerGetModel(PC_PTR(*(u32*)(work + 0x24))) : *(u32*)(work + 0x24);
    resetModel = renderModel;
    if (resetModel == 0) return 0;

#ifdef TARGET_PC
    if (work[0x14] != 0) { /* a menu Pokemon model: show it at the console's whole frames (port-gaps 78; the fix and why
                            * are in gs_model_anim_exact_800EC4D0.c, modelApplyAnimation; all platforms) */
        extern void* pc_console_cadence_model;
        extern u32 pc_console_cadence_frame;
        extern volatile u32 pc_vi_heartbeat;
        pc_console_cadence_model = (void*)PC_PTR(resetModel);
        pc_console_cadence_frame = pc_vi_heartbeat;
    }
#endif
    fn_800EC134(resetModel);
    GSmodelSetPEdescr(resetModel, lbl_8035B468);
    {
        lightCursor = work;
        for (width = 0; width < 3; lightCursor += 4, width++) {
            u32 light = *(u32*)(lightCursor + 0x3C);
            if (light != 0) GSlightSetActive(light, 1);
        }
    }
    previousCamera = PC_PTR32_IMPLICIT(GScameraGetActiveCamera());
    fn_800D4604(2);
    fn_800D377C(2);
    fn_800D9B24(&viewLeft, &viewTop, &viewRight, &viewBottom);
    fn_800D9AF0(&scissorLeft, &scissorTop, &scissorRight, &scissorBottom);
    fn_800D258C(*(u32*)(work + 0x38));
    fn_800D258C(*(u32*)(work + 0x38));
    width = (u16)(GStextureGetXsize(*(u32*)(work + 0x34)) - 1);
    height = (u16)(GStextureGetYsize(*(u32*)(work + 0x34)) - 1);
#ifdef TARGET_PC
    { /* PC (observe only, 2026-11-04): ORRE_PDA_MODEL_LOG=1 logs each menu-model render's texture size and the
       * view it restores, when they change */
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        static int lines;
        static u32 last;
        if (getenv("ORRE_PDA_MODEL_LOG") != NULL && lines < 40 && (u32)((width << 16) | height) != last) {
            lines++;
            last = (u32)((width << 16) | height);
            OSReport("[menumodel] render texture %u x %u (viewport 0,0,%u,%u); view %u,%u,%u,%u scissor %u,%u,%u,%u\n",
                     (unsigned)width + 1, (unsigned)height + 1, (unsigned)width, (unsigned)height, viewLeft, viewTop,
                     viewRight, viewBottom, scissorLeft, scissorTop, scissorRight, scissorBottom);
        }
    }
    { /* PC (observe only, 2026-11-13, port-gaps 78: Espeon's summary model jitters): ORRE_MENUMODEL_LOG=1 logs
       * each render of a menu model whose animation runs here (wazaSequenceSysUpdate below), with the game frame, the
       * retrace count and the game's frame step, to see how often per frame the animation is advanced */
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        extern volatile u32 pc_vi_heartbeat;
        extern u32 VIGetRetraceCount(void);
        extern u32 GSgetFrameStep(void);
        static int mm_lines = -1;
        if (mm_lines < 0) mm_lines = getenv("ORRE_MENUMODEL_LOG") != NULL ? 0 : 100000;
        if (mm_lines < 3000 && work[0x14] != 0) {
            /* names this model for the per-apply log in gs_model_anim_exact_800EC4D0.c (from the next frame on) */
            extern void* pc_menumodel_watch;
            pc_menumodel_watch = (void*)PC_PTR(resetModel);
            mm_lines++;
            OSReport("[menumodel] anim render: game frame %u, retrace %u, step %u\n", (unsigned)pc_vi_heartbeat,
                     (unsigned)VIGetRetraceCount(), (unsigned)GSgetFrameStep());
        }
    }
#endif
    gfxSetScissor(0, 0, width, height);
    gfxSetViewport(0, 0, width, height);
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
    GSgfxRenderLayer(PC_PTR(*(u32*)(work + 0x34)), 0);
    if (work[0x14] != 0) {
        wazaSequenceOwnerSetVisibility(PC_PTR(*(u32*)(work + 0x24)), 1);
        wazaSequenceOwnerSetParticleVisibility(*(u32*)(work + 0x24), 1);
        wazaSequenceSysUpdate();
        wazaSequenceOwnerDraw(*(u32*)(work + 0x24));
        wazaSequenceOwnerSetVisibility(PC_PTR(*(u32*)(work + 0x24)), 0);
        wazaSequenceOwnerSetParticleVisibility(*(u32*)(work + 0x24), 0);
    } else {
        GSmodelDrawModel(PC_PTR(*(u32*)(work + 0x24)), 0x3010);
    }
    GSgfxRenderEnd();
    fn_800D377C(1);
    fn_800D258C(previousCamera);
    gfxSetScissor(viewLeft, viewTop, viewRight, viewBottom);
    gfxSetViewport(scissorLeft, scissorTop, scissorRight, scissorBottom);
    _cameraLoadCameraMatrix__FP9_GScamera12GSgfxLayerID();
    fn_800D4604(1);
    {
        lightCursor = work;
        for (i = 0; i < 3; lightCursor += 4, i++) {
            u32 light = *(u32*)(lightCursor + 0x3C);
            if (light != 0) GSlightSetActive(light, 0);
        }
    }
    GSmodelResetPEdescr(PC_PTR(resetModel));
    return *(u32*)(work + 0x34);
}
#endif

/* 0x80109B90 | 0x6C */
#if !defined(MENU_MODEL_SPLIT) || defined(MENU_MODEL_CHECK)
/* RULE-EXCEPTION(user-approved): local peephole control -- see docs/RULE_EXCEPTIONS.md. */
#if !defined(NO_LOCAL_PRAGMAS_MENU_MODEL_EXACT_CHECK)
#endif
#if !defined(NO_LOCAL_PRAGMAS_MENU_MODEL_EXACT_CHECK)
#endif
s32 menuModelCheck(void* obj, u8 wait) {
    void* r30 = obj;
    if (r30 == (void*)0) { return 0; }
    {
        u8 r31 = (u8)wait;
        goto check;
    check:
        if (*(u8*)r30 != 1) { return 0; }
        if (r31 == 0) { goto done; }
        _threadSwitch();
        goto check;
    done:
        return 1;
    }
}
#if !defined(NO_LOCAL_PRAGMAS_MENU_MODEL_EXACT_CHECK)
#endif
#endif

/* 0x80109BFC | 0x8C */
#if !defined(MENU_MODEL_SPLIT) || defined(MENU_MODEL_FREE)
/* RULE-EXCEPTION(user-approved): local peephole control -- see docs/RULE_EXCEPTIONS.md. */
#pragma push
#pragma peephole off
s32 menuModelFree(void* p) {
    void* r31 = p;
    if (r31 == (void*)0) { return 0; }
    *(u8*)((u8*)r31 + 0x1) = 0;
    /* +0x24 is a 32-bit slot on the PC too (the model or move-sequence owner, written as a u32 by
     * _menuPokemonModelLoadThread): an 8-byte test also saw the load-thread word at +0x28 */
    if (*(u8*)((u8*)r31 + 0x14) != 0) {
        /* has some kind */
        if (PTR32_AT_T(void, (u8*)r31, 0x24) != (void*)0) {
            wazaSequenceOwnerFree(*(u32*)((u8*)r31 + 0x24));
            *(u32*)((u8*)r31 + 0x24) = 0;
        }
    } else {
        if (PTR32_AT_T(void, (u8*)r31, 0x24) != (void*)0) {
            GSmodelFree(PC_PTR(*(u32*)((u8*)r31 + 0x24)));
            *(u32*)((u8*)r31 + 0x24) = 0;
        }
    }
    *(u8*)((u8*)r31 + 0x0) = 0;
    return 1;
}
#pragma pop
#endif
