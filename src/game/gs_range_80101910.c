/**
 * @file gs_range_80101910.c
 * @brief gs-engine code, 0x80101910 - 0x80101A70 (4 fns).
 *
 * Range unit assigned from the propagated subsystem map
 * (tools/subsystem_propagation.py, >=80% single-label dominance;
 * campaign 2026-07-01). All functions asm-only until matched; the
 * range name stays honest until internal TU structure is proven.
 *
 * Extended by the gs_model.c splitter-bucket physical split
 * (2026-07-07) to absorb its cut-off tail (0x801019F8 - 0x80101A70,
 * 3 fns, 1 with a real C body): wazaSequenceSysClearWork memsets the 0x2400 bss
 * buffer lbl_80402518, otherwise referenced only by this unit's and
 * gs_floor.c's tail functions; fn_80101A28/floorModelFreeCB (asm-only) are
 * taken by address as registered module callbacks in gs_floor.c.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
extern u8 lbl_80402518[];  /* model resource table -- 0x2400 bytes */

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
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowSearchItemID(void* head, s32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowSearchID(s32 param);
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
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowGetKeyInfo(void);
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

/* 0x80101910 | 0xE8 */
#ifdef TARGET_PC
/* PC: the parsed-archive cache lbl_80402518 holds {HSD_Archive; s32 refs} entries (gs_floor.c GSArchiveCacheEntry),
 * larger than the GameCube's 0x48 because HSD_Archive holds pointers. Walking it with the GameCube offsets (stride
 * 0x48, refs +0x44, top_ptr +0x40) never found the entry, so references were never dropped; a later file image loaded
 * at a reused address then matched the stale entry and was used unrelocated (Willie's send-out crash reading 0x1AD4,
 * snatcherLeo's playtest 2026-10-17 08:16, once the arena became the GameCube's size). Same type as gs_floor.c. */
#include "hsd/hsd_archive.h"
typedef struct PcArchiveCacheEntry {
    HSD_Archive archive;
    s32 refs;
} PcArchiveCacheEntry;

s32 floorModelRefDec(void* obj) {
    PcArchiveCacheEntry* entry = (PcArchiveCacheEntry*)lbl_80402518;
    s32 i;

    for (i = 0; i < 128; i++, entry++) {
        if (entry->refs != 0 && entry->archive.top_ptr == ((HSD_Archive*)obj)->top_ptr) {
            entry->refs--;
            return 1;
        }
    }
    return 1;
}

void wazaSequenceSysClearWork(void) {
    memset(lbl_80402518, 0, 128 * sizeof(PcArchiveCacheEntry));
}
#else
s32 floorModelRefDec(void* obj) {
    s32 count;
    u8* entry = lbl_80402518;
    s32 i;

    for (i = 0; i < 128; i++, entry += 0x48) {
        count = *(s32*)(entry + 0x44);

        if (count != 0 && *(u32*)(entry + 0x40) == *(u32*)((u8*)obj + 0x40)) {
            *(s32*)(entry + 0x44) = count - 1;
            return 1;
        }
    }
    return 1;
}

/* 0x801019F8 | 0x30 */
void wazaSequenceSysClearWork(void) {
    memset(lbl_80402518, 0, 0x2400);
}
#endif

/* 0x80101A28 | 0x24 */
#ifdef TARGET_PC
/* PC: a GSres release callback; the GameCube passes its first argument (the resource: the room camera) on to
 * GScameraFree in r3 (assembly: no register set before the bl); the PC has to name it (zeroargs.py class) */
s32 fn_80101A28(void* resource) {
    extern void GScameraFree(void*);

    GScameraFree(resource);
#else
s32 fn_80101A28(void) {
    extern void GScameraFree(void);

    GScameraFree();
#endif
    return 1;
}

/* 0x80101A4C | 0x24 */
s32 floorModelFreeCB(void* model) { /* hands its argument (r3) straight to GSmodelFree (assembly) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelFree(void*);
#endif

    GSmodelFree(model);
    return 1;
}
