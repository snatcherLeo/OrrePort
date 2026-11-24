/**
 * @file menu_offscreen.c
 * @brief menuOffScreen -- back-framebuffer capture / fade-transition
 *        offscreen texture (Colosseum "menu" UI subsystem). Split from
 *        the gs_model.c splitter bucket (address range 0x801093C8 -
 *        0x80109894, 12 fns). Corresponds to XD's menuOffScreen.cpp
 *        (0x801145EC-0x80114D30); 8 anchor symbols strictly monotonic,
 *        with _menuCBOffScreen (segment's and XD TU's last fn) an exact
 *        size match. menuOffScreenInit is called by menuInit (menu.c).
 *
 *        menuOffScreenDraw is XD's first function here, menuOffScreenDraw
 *        (TeamOrre/xd-decomp config/GXXE01/symbols.txt @4989794e,
 *        0x801145EC): it drives the same fade state (lbl_8047AD2C..AD3C).
 *        The TU owns the .sdata2 literal pool 0x8047CE48-0x8047CE70
 *        (255, 100, 0, 640, 480, 1, and the two int-to-float doubles), in
 *        first-use order; menuOffScreenFadeSet/Create/Init share its 0.0f
 *        and 1.0f.
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
/* Capture-done flag: set by _menuCBOffScreen from the back-framebuffer
 * capture callback and polled by menuOffScreenCheckEnable, so it is
 * volatile (every read reloads it). */
extern volatile u8 _menuOffScreenEnable;
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
extern void gfxVtxPos(f32, f32, f32);
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
#ifdef TARGET_PC
/* frame smoothing's 2D at 120 (round 11; src/game/pc_debug_game.c): names a 2D piece (kind 0 ends it) for
 * OrreHook_2DPiece. Reads only; does nothing unless the hook is set. All platforms. */
extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
#endif
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

extern u8 lbl_80314AE8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxLoadTexture(s32, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxTexCoord(s32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_800D37CC(void);
#endif
extern void gfxSetCoPlanar(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif

/* 0x801093C8 | 0x29C */
void menuOffScreenDraw(void) {
    s32 alpha;
    f32 progress;

    if (_menuOffScreenType != 0) {
        lbl_8047AD3C += (f32)GSgetFrameStep() / (f32)fn_800D37CC();
        if (lbl_8047AD3C >= lbl_8047AD38) {
            lbl_8047AD3C = lbl_8047AD38;
            _menuOffScreenType = 0;
        }
    }
    if (_menuOffScreenDisp == 0) return;
    if (_menuOffScreenEnable == 0) return;

    progress = lbl_8047AD2C +
        (lbl_8047AD3C / lbl_8047AD38) * (lbl_8047AD34 - lbl_8047AD2C);
    lbl_8047AD30 = progress;
    alpha = (s32)(255.0f -
        (255.0f * progress) / 100.0f);

    if (lbl_8047AD23 != 0) {
        fn_800D9ED8(1);
    } else {
        fn_800D9ED8(0);
    }
    gfxSetFlags(3);
    gfxClearFlags(4);
    gfxSetOrtho(0.0f, 0.0f,
                640.0f, 480.0f);
    gfxSetBlendMode(0, 1, 1);
    gfxSetUpdates(1, 1, 1);
    gfxSetZMode(1, 2, 1);
    gfxSetCullMode(0);
    gfxSetPrimitive(4);
    gfxSetVtxDesc(PC_PTR((s32)PC_PTR32(lbl_80314AE8)));
    gfxLoadTexture(0, (void*)PC_PTR(_menuOffScreenTex));
#ifdef TARGET_PC
    /* OrreHook_2DPiece (observation; frame smoothing's 2D at 120, round 11): names the backdrop's darkening (group 3;
     * its brightness, `alpha`, is the four corners' colour), so an in-between can draw it part way between steps */
    pc_orre_2d_tag(0x30000, 0, (const void*)1, (f32)alpha);
#endif
    gfxBegin(4);

    gfxVtxPos(0.0f, 0.0f, 0.0f);
    gfxVtxColor4u8(0, alpha, alpha, alpha, 0xFF);
    gfxVtxTexCoord(0, 0.0f, 0.0f);
    gfxVtxPos(640.0f, 0.0f, 0.0f);
    gfxVtxColor4u8(0, alpha, alpha, alpha, 0xFF);
    gfxVtxTexCoord(0, 1.0f, 0.0f);
    gfxVtxPos(0.0f, 480.0f, 0.0f);
    gfxVtxColor4u8(0, alpha, alpha, alpha, 0xFF);
    gfxVtxTexCoord(0, 0.0f, 1.0f);
    gfxVtxPos(640.0f, 480.0f, 0.0f);
    gfxVtxColor4u8(0, alpha, alpha, alpha, 0xFF);
    gfxVtxTexCoord(0, 1.0f, 1.0f);
    gfxEnd();
#ifdef TARGET_PC
    pc_orre_2d_tag(0, 0, NULL, 0.0f);
#endif
    if (lbl_8047AD23 == 0) gfxSetCoPlanar(0);
}

/* 0x80109664 | 0x48 */
u8 menuOffScreenFadeSync(u8 wait) {
    while (wait) {
        if (_menuOffScreenType == 0) {
            break;
        }
        _threadSwitch();
    }
    return _menuOffScreenType;
}

/* 0x801096AC | 0x3C */
void menuOffScreenFadeSet(f32 target, f32 time) {
    _menuOffScreenType = 1;
    lbl_8047AD2C = lbl_8047AD30;
    lbl_8047AD34 = target;
    lbl_8047AD30 = 0.0f;
    lbl_8047AD38 = time;
    lbl_8047AD3C = 0.0f;
    if (time == 0.0f) {
        lbl_8047AD3C = lbl_8047AD38 = 1.0f;
    }
}

/* 0x801096E8 | 0x10 */
u8 menuOffScreenSetPriority(u8 val) {
    u8 old = lbl_8047AD23;
    lbl_8047AD23 = val;
    return old;
}

/* 0x801096F8 | 0x10 */
u8 menuOffScreenSetDisp(u8 val) {
    u8 old = _menuOffScreenDisp;
    _menuOffScreenDisp = val;
    return old;
}

/* 0x80109708 | 0x8 | sda_getter */
u8 menuOffScreenIsDoing(void) { return _menuOffScreenUsing; }

/* 0x80109710 | 0x8 | sda_getter */
u32 menuOffScreenGetPtr(void) { return _menuOffScreenTex; }

/* 0x80109718 | 0x4C */
u8 menuOffScreenCheckEnable(u8 wait) {
    while (wait) {
        if (_menuOffScreenEnable != 0) {
            break;
        }
        _threadSwitch();
    }
    return _menuOffScreenEnable;
}

/* 0x80109764 | 0x18 */
void menuOffScreenRelease(void) {
    _menuOffScreenType = 0;
    _menuOffScreenUsing = 0;
    _menuOffScreenEnable = 0;
    _menuOffScreenDisp = 0;
}

/* 0x8010977C | 0x94 */
u8 menuOffScreenCreate(u32 wait) {
    _menuOffScreenType = 0;
    _menuOffScreenUsing = 1;
    _menuOffScreenEnable = 0;
    _menuOffScreenDisp = 0;
    lbl_8047AD2C = 0.0f;
    lbl_8047AD30 = 0.0f;
    lbl_8047AD34 = 0.0f;
    lbl_8047AD38 = 0.0f;
    lbl_8047AD3C = 0.0f;
    GSgfxBeginBackFBCapture(PC_PTR(_menuOffScreenTex), _menuCBOffScreen__FP9GStextureUlPv, (void*)0);
    return menuOffScreenCheckEnable(wait);
}

/* 0x80109810 | 0x74 */
void menuOffScreenInit(void) {
    if (_menuOffScreenTex == 0) {
        _menuOffScreenTex = PC_PTR32_IMPLICIT(GStextureCreate(0, 0, 0x44, 0, 0));
    }
    _menuOffScreenUsing = 0;
    _menuOffScreenEnable = 0;
    _menuOffScreenDisp = 0;
    lbl_8047AD23 = 0;
    _menuOffScreenType = 0;
    lbl_8047AD38 = 0.0f;
    lbl_8047AD3C = 0.0f;
    lbl_8047AD2C = 0.0f;
    lbl_8047AD30 = 0.0f;
    lbl_8047AD34 = 0.0f;
}

/* 0x80109884 | 0x10 */
s32 _menuCBOffScreen__FP9GStextureUlPv(void) {
    _menuOffScreenEnable = 1;
    return 0;
}

