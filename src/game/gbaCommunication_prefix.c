/**
 * @file gbaCommunication_prefix.c
 * @brief gbaCommunication carve, 0x8008C7B0 - 0x80090720 (16 functions,
 *        linked).
 *
 * The functions are defined in address order. Every one of them needs the
 * peephole pass off, so the unit is built with -opt nopeephole (configure.py)
 * instead of per-function pragmas. The float literals are the gbaCommunication
 * pool at 0x8047C1D0, which this carve shares with
 * gbaCommunication_exact_80090720.c; it stays in sdata2_8047C1A0.c.
 */
#include "dolphin/types.h"
#include "game/constants/rooms.h"
#include "game/gs_material.h"
/* The resources 0x0CE61000 / 0x0B631000 below are map-object models (std115_mapObjectPlayAnimation): their +0x144 is
 * GSmodel's linked particle bank. They were viewed through gs_material.h's documentation-only GSmaterialEntry, whose
 * "texture" is also at +0x144 on the GameCube but at another offset on the PC (pointer audit, 2026-10-19). */
#include "game/gs_model_struct.h"

extern s32 fn_800D37CC(void);
extern u32 GSgetFrameStep(void);
extern void GSmodelSetAnimIndex(struct GSmodel* model, u32 index);
extern void GSmodelStartAnimation(struct GSmodel* model);
extern void floorRequestChange(u32 floorId);
extern void gsParticleKill(u8* list, u32 notify);

#define GBA_DATA_OFFSET 0x20
#define GBA_STATE_PORT 0x4338
#define GBA_STATE_TIMEOUT 0x433C
#define GBA_STATE_PHASE 0x4340
#define GBA_THREAD_PRIORITY 8

extern u32 lbl_8047A690;
extern u32 lbl_8047A694;
extern f32 lbl_8047C1D0; /* 0.833333313f -- PAL-adjusted 1-unit wait */
/* RULE-EXCEPTION(user-approved): extern named stand-in for the TU's pool
 * literal - see docs/RULE_EXCEPTIONS.md. The gbaCommunication pool
 * (0x8047C1D0-0x8047C1E8) is shared by this carve and
 * gbaCommunication_exact_80090720.c and stays in sdata2_8047C1A0.c; const
 * gives the literal's load schedule (a plain 0.0f would emit a local
 * .sdata2 the carve does not own). */
extern const f32 lbl_8047C1D4; /* 0.0f */
extern f32 lbl_8047C1D8; /* 1.0f */
extern f32 lbl_8047C1DC; /* 83.3333282f -- PAL-adjusted 100-unit wait */
extern f32 lbl_8047C1E0; /* {41.6666641f, 0.0f} -- PAL-adjusted 50-unit wait */

/* Additional data labels referenced by the ported gba_comm_ext.c /
 * late_game.c bodies below (GBA link-cable state machine + battle-status
 * window helpers living in this same address range per the current
 * object map). */
extern u8 lbl_803FB328[];
extern u8 lbl_803FB338[];
extern u8 lbl_803FB380[];
extern u8 lbl_8047C1E8;
extern u8 lbl_8026F5A8[];
extern u8 lbl_8026F5C0[];
extern u8 lbl_80314F98[];
extern u16 lbl_802EED28[];

/* Common callees needed by the ported bodies below that are not already
 * declared with a full prototype at the point of use. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowSearchID(u32 id);
#endif
extern void OSLockMutex(void *p);
extern void OSUnlockMutex(void *p);
extern void OSSetThreadPriority(void *p, u32 b);
extern void fn_800716E8(u32 port, u32 val);
extern void OSSignalCond(void *p);
extern void fn_800A1E54(void *p, u32 v);
extern void fn_800716C8(u32 port, void *a, void *b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmemGetHandleByPtr(void *p);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void __assert(const u8 *file, u32 line, const u8 *msg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0(u32 status);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u32 status);
#endif
extern u32 OSGetCurrentThread(void);
extern void OSYieldThread(void);
extern void std098_mapWarpToMenuRoom(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSresGetResource(u32 ctx, u32 id);
#endif

/* Storage used by the battle-status window renderer below. */
extern u8 lbl_8047C200;
extern u8 lbl_8047C204;
extern f32 lbl_8047C208;
extern u8 lbl_8047C20C;
extern u8 lbl_8047C210;
extern u8 lbl_8047C214;
extern u8 lbl_8047C218;
extern u8 lbl_8047C21C;
extern u8 lbl_8047C220;
extern u8 lbl_8047C228;
extern f32 lbl_8047C230;
extern f32 lbl_8047C234;
extern f32 lbl_8047C238;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801040F0();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp();
#endif
extern void menuDrawRectOutline();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FA280();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FA444();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintRight();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintRect();
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonDataBiosGetPtr(u32 id);
#endif
extern u8 pokemonBiosGetCatchBallId(void* pokemon);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonGetSoubiItemDataId(void* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonDataBiosGetName(void* bios);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetGSchar(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(u32 id, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowDrawSprite(s32 x, s32 y, void* win, u16 sprite, u32 data);
#endif
extern void windowDrawSprite2(s32 x, s32 y, s16 w, s16 h, s32 color, s32 data, s32 sprite, s32 arg7);
extern void* menuModelRender(void* data);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(void* arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxLoadTexture(u32 arg0, void* arg1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos2s16(s32 x, s32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(u32 arg0, u32 r, u32 g, u32 b, u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxTexCoord(u32 arg0, f32 s, f32 t);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif
extern u32 menuPokemonGetStatusSprite(void* pokemon, u32 arg);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuSubGetPokemonSexForDisp(void* pokemon);
#endif

/* 0x8008C7B0 */
void demoScene30(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
    extern void std078_flagSetFalse(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconHandle2;
    u32 b2;
    u32 tmpA;
    u32 tmpB;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    iconHandle2 = std112_modelGetResourceID(PC_PTR(0x0B720400));

    b2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(b2), 2);
    GSmodelSetShadowLight(b2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(b2), 1, &lbl_8047A694);

    b2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle2));
    GSmodelSetShadowFlags(PC_PTR(b2), 2);
    GSmodelSetShadowLight(b2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(b2), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0D041800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    tmpA = std112_modelGetResourceID(PC_PTR(0x0D0D1000));
    tmpB = std112_modelGetResourceID(PC_PTR(0x0D0D1001));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, tmpA, 0);
    std070_characterAttachToCharacter(ctx, iconHandle2, ctx, tmpB, 0);
    std115_mapObjectPlayAnimation(iconHandle, 9, 0, 1);
    std115_mapObjectPlayAnimation(iconHandle2, 6, 0, 1);

    cameraWaitSyncAnime(1);
    std078_flagSetFalse(0x8D0);
    floorRequestChange(ROOM_S1_OUT);
    floorSetFadeScript(0, 0);
}

/* 0x8008CACC */
void demoScene29(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11211400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 2, 0, 0);
    waitFrames = 100;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1DC;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    cameraPlayAnime(ctx, 0x0D021800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0D0C1000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    std115_mapObjectPlayAnimation(iconHandle, 4, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008CDD8 */
void demoScene28(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void scriptWaitSyncMotion(u32 id, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0D011800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0D0B1000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    std115_mapObjectPlayAnimation(iconHandle, 0xB, 0, 0);
    scriptWaitSyncMotion(iconHandle, 1);
    std115_mapObjectPlayAnimation(iconHandle, 0xC, 0, 0);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008D0A0 */
void demoScene27(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0D001800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0D0A1000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    std115_mapObjectPlayAnimation(iconHandle, 9, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008D348 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void demoScene26(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 mode);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(u32 ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 model;
    u32 base;
    f32 frame;
    u32 h06af0400;
    u32 h06bc0400;
    u32 h06be0400a;
    u32 h06be0400b;
    u32 h06be0400c;
    u32 h06be0400d;
    u32 h06be0400e;
    u32 h0d091000;
    u32 h0d091001;
    u32 h0d091006;
    u32 h0d091002;
    u32 h0d091003;
    u32 h0d091004;
    u32 h0d091005;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    base = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(PC_PTR(*(u32*)PC_PTR((base + 0x144))), 1);
    *(u32*)PC_PTR((base + 0x144)) = 0;

    frame = lbl_8047C1D4;
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(model), 0);
    GSmodelGetFrameCount(PC_PTR(model), &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(model), 0);
    GSmodelSetAnimFrame(PC_PTR(model), frame);
    GSmodelSetAnimType(model, 0);
    GSmodelStartAnimation(PC_PTR(model));

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11201400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    h06af0400 = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h06be0400a = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400b = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400c = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400d = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400e = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400c));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400d));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400e));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), 4);

    cameraPlayAnime(ctx, 0x0CFF1800, 0, 0);

    waitFrames2 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1D0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    h0d091000 = std112_modelGetResourceID(PC_PTR(0x0D091000));
    h0d091001 = std112_modelGetResourceID(PC_PTR(0x0D091001));
    h0d091006 = std112_modelGetResourceID(PC_PTR(0x0D091006));
    h0d091002 = std112_modelGetResourceID(PC_PTR(0x0D091002));
    h0d091003 = std112_modelGetResourceID(PC_PTR(0x0D091003));
    h0d091004 = std112_modelGetResourceID(PC_PTR(0x0D091004));
    h0d091005 = std112_modelGetResourceID(PC_PTR(0x0D091005));

    std070_characterAttachToCharacter(ctx, h06af0400, ctx, h0d091000, 0);
    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0d091001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400a, ctx, h0d091006, 0);
    std070_characterAttachToCharacter(ctx, h06be0400b, ctx, h0d091002, 0);
    std070_characterAttachToCharacter(ctx, h06be0400c, ctx, h0d091003, 0);
    std070_characterAttachToCharacter(ctx, h06be0400d, ctx, h0d091004, 0);
    std070_characterAttachToCharacter(ctx, h06be0400e, ctx, h0d091005, 0);

    std115_mapObjectPlayAnimation(0x0CE61000, 1, 0, 0);
    std115_mapObjectPlayAnimation(h06af0400, 9, 0, 0);
    std115_mapObjectPlayAnimation(h06bc0400, 7, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400a, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400b, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400c, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400d, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400e, 0xA, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#else
void demoScene26(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 mode);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(u32 ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 model;
    u32 base;
    f32 frame;
    u32 h06af0400;
    u32 h06bc0400;
    u32 h06be0400a;
    u32 h06be0400b;
    u32 h06be0400c;
    u32 h06be0400d;
    u32 h06be0400e;
    u32 h0d091000;
    u32 h0d091001;
    u32 h0d091006;
    u32 h0d091002;
    u32 h0d091003;
    u32 h0d091004;
    u32 h0d091005;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    base = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(PC_PTR(*(u32*)PC_PTR((base + 0x144))), 1);
    *(u32*)PC_PTR((base + 0x144)) = 0;

    frame = lbl_8047C1D4;
    model = GSresGetResource(ctx, 0x0CE61004);
    GSmodelSetAnimIndex((struct GSmodel*)model, 0);
    GSmodelGetFrameCount(model, &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex((struct GSmodel*)model, 0);
    GSmodelSetAnimFrame(model, frame);
    GSmodelSetAnimType(model, 0);
    GSmodelStartAnimation((struct GSmodel*)model);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11201400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    h06af0400 = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h06be0400a = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400b = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400c = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400d = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400e = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400c));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400d));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400e));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), 4);

    cameraPlayAnime(ctx, 0x0CFF1800, 0, 0);

    waitFrames2 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1D0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    h0d091000 = std112_modelGetResourceID(PC_PTR(0x0D091000));
    h0d091001 = std112_modelGetResourceID(PC_PTR(0x0D091001));
    h0d091006 = std112_modelGetResourceID(PC_PTR(0x0D091006));
    h0d091002 = std112_modelGetResourceID(PC_PTR(0x0D091002));
    h0d091003 = std112_modelGetResourceID(PC_PTR(0x0D091003));
    h0d091004 = std112_modelGetResourceID(PC_PTR(0x0D091004));
    h0d091005 = std112_modelGetResourceID(PC_PTR(0x0D091005));

    std070_characterAttachToCharacter(ctx, h06af0400, ctx, h0d091000, 0);
    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0d091001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400a, ctx, h0d091006, 0);
    std070_characterAttachToCharacter(ctx, h06be0400b, ctx, h0d091002, 0);
    std070_characterAttachToCharacter(ctx, h06be0400c, ctx, h0d091003, 0);
    std070_characterAttachToCharacter(ctx, h06be0400d, ctx, h0d091004, 0);
    std070_characterAttachToCharacter(ctx, h06be0400e, ctx, h0d091005, 0);

    std115_mapObjectPlayAnimation(0x0CE61000, 1, 0, 0);
    std115_mapObjectPlayAnimation(h06af0400, 9, 0, 0);
    std115_mapObjectPlayAnimation(h06bc0400, 7, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400a, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400b, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400c, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400d, 0xA, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400e, 0xA, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#endif

/* 0x8008D938 */
void demoScene25(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 model, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 model, u32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 motion, u32 arg2, u32 arg3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void scriptWaitSyncMotion(u32 id, u32 sync);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 model, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 model, u32 light);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 model, u32 count, u32 *surface);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 arg2, u32 arg3);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 model, u32 ctx2, u32 motion, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 arg0, u32 arg1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 model, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 model, f32 *frame, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 model, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 model, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 model);
#endif

    u32 elapsed;
    u32 waitFrames;
    u32 cameraElapsed;
    u32 cameraWaitFrames;
    u32 elapsedLater;
    u32 waitFramesLater;
    u32 elapsedFinal;
    u32 waitFramesFinal;
    GSmodel *material;
    f32 frame;
    u32 model0;
    u32 model1;
    u32 model2;
    u32 model3;
    u32 model4;
    u32 model5;
    u32 model6;
    u32 model7;
    u32 model8;
    u32 motion0;
    u32 motion8;
    u32 motion1;
    u32 motion2;
    u32 motion3;
    u32 motion4;
    u32 motion5;
    u32 motion6;
    u32 motion7;
    u32 resource;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);
    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    resource = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(resource), 0);
    GSmodelGetFrameCount(PC_PTR(resource), &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(resource), 0);
    GSmodelSetAnimFrame(PC_PTR(resource), frame);
    GSmodelSetAnimType(resource, 0);
    GSmodelStartAnimation(PC_PTR(resource));

    resource = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400));
    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR(resource));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);
    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);

    waitFramesLater = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFramesLater = (u32)lbl_8047C1E0;
        if (waitFramesLater < 1) {
            waitFramesLater = 1;
        }
    }
    for (elapsedLater = 0; elapsedLater < waitFramesLater; ) {
        _threadSwitch();
        elapsedLater += GSgetFrameStep();
    }

    resource = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111F1400));
    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR(resource));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    model0 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    model1 = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    model2 = std112_modelGetResourceID(PC_PTR(0x0D290400));
    model3 = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    model4 = std112_modelGetResourceID(PC_PTR(0x0D240400));
    model5 = std112_modelGetResourceID(PC_PTR(0x0D290400));
    model6 = std112_modelGetResourceID(PC_PTR(0x0D240400));
    model7 = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    model8 = std112_modelGetResourceID(PC_PTR(0x0D240400));

#define SET_MODEL_SHADOW(model)                                                    \
    resource = GSresGetResource(ctx, (model));                                     \
    GSmodelSetShadowFlags(resource, 2);                                            \
    GSmodelSetShadowLight(resource, lbl_8047A690);                                 \
    GSmodelSetShadowSurface(resource, 1, &lbl_8047A694)

    SET_MODEL_SHADOW(model0);
    SET_MODEL_SHADOW(model1);
    SET_MODEL_SHADOW(model2);
    SET_MODEL_SHADOW(model3);
    SET_MODEL_SHADOW(model4);
    SET_MODEL_SHADOW(model5);
    SET_MODEL_SHADOW(model6);
    SET_MODEL_SHADOW(model7);
    SET_MODEL_SHADOW(model8);

#undef SET_MODEL_SHADOW

    cameraPlayAnime(ctx, 0x0CFE1800, 0, 0);
    waitFramesFinal = 1;
    if (fn_800D37CC() == 0x32) {
        waitFramesFinal = (u32)lbl_8047C1D0;
        if (waitFramesFinal < 1) {
            waitFramesFinal = 1;
        }
    }
    for (elapsedFinal = 0; elapsedFinal < waitFramesFinal; ) {
        _threadSwitch();
        elapsedFinal += GSgetFrameStep();
    }

    motion0 = std112_modelGetResourceID(PC_PTR(0x0D081000));
    motion8 = std112_modelGetResourceID(PC_PTR(0x0D081008));
    motion1 = std112_modelGetResourceID(PC_PTR(0x0D081001));
    motion2 = std112_modelGetResourceID(PC_PTR(0x0D081002));
    motion3 = std112_modelGetResourceID(PC_PTR(0x0D081003));
    motion4 = std112_modelGetResourceID(PC_PTR(0x0D081004));
    motion5 = std112_modelGetResourceID(PC_PTR(0x0D081005));
    motion6 = std112_modelGetResourceID(PC_PTR(0x0D081006));
    motion7 = std112_modelGetResourceID(PC_PTR(0x0D081007));

    std070_characterAttachToCharacter(ctx, model0, ctx, motion0, 0);
    std070_characterAttachToCharacter(ctx, model1, ctx, motion8, 0);
    std070_characterAttachToCharacter(ctx, model2, ctx, motion1, 0);
    std070_characterAttachToCharacter(ctx, model3, ctx, motion2, 0);
    std070_characterAttachToCharacter(ctx, model4, ctx, motion3, 0);
    std070_characterAttachToCharacter(ctx, model5, ctx, motion4, 0);
    std070_characterAttachToCharacter(ctx, model6, ctx, motion5, 0);
    std070_characterAttachToCharacter(ctx, model7, ctx, motion6, 0);
    std070_characterAttachToCharacter(ctx, model8, ctx, motion7, 0);

    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);
    std115_mapObjectPlayAnimation(model0, 6, 0, 0);
    std115_mapObjectPlayAnimation(model1, 9, 0, 0);
    std115_mapObjectPlayAnimation(model2, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model3, 9, 0, 0);
    std115_mapObjectPlayAnimation(model4, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model5, 9, 0, 0);
    std115_mapObjectPlayAnimation(model6, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model7, 9, 0, 0);
    std115_mapObjectPlayAnimation(model7, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model8, 0xB, 0, 0);

    scriptWaitSyncMotion(model0, 1);
    scriptWaitSyncMotion(model1, 1);
    scriptWaitSyncMotion(model2, 1);
    scriptWaitSyncMotion(model3, 1);
    scriptWaitSyncMotion(model4, 1);
    scriptWaitSyncMotion(model5, 1);
    scriptWaitSyncMotion(model6, 1);
    scriptWaitSyncMotion(model7, 1);
    scriptWaitSyncMotion(model7, 1);
    scriptWaitSyncMotion(model8, 1);

    std115_mapObjectPlayAnimation(model0, 7, 0, 0);
    std115_mapObjectPlayAnimation(model1, 0xA, 0, 0);
    std115_mapObjectPlayAnimation(model2, 0xC, 0, 0);
    std115_mapObjectPlayAnimation(model3, 0xA, 0, 0);
    std115_mapObjectPlayAnimation(model5, 0xA, 0, 0);
    std115_mapObjectPlayAnimation(model4, 0xC, 0, 0);
    std115_mapObjectPlayAnimation(model6, 0xC, 0, 0);
    std115_mapObjectPlayAnimation(model7, 0xA, 0, 0);
    std115_mapObjectPlayAnimation(model7, 0xC, 0, 0);
    std115_mapObjectPlayAnimation(model8, 0xC, 0, 0);

    scriptWaitSyncMotion(model0, 1);
    scriptWaitSyncMotion(model1, 1);
    scriptWaitSyncMotion(model2, 1);
    scriptWaitSyncMotion(model3, 1);
    scriptWaitSyncMotion(model4, 1);
    scriptWaitSyncMotion(model5, 1);
    scriptWaitSyncMotion(model6, 1);
    scriptWaitSyncMotion(model7, 1);
    scriptWaitSyncMotion(model7, 1);
    scriptWaitSyncMotion(model8, 1);

    std115_mapObjectPlayAnimation(model0, 6, 0, 0);
    std115_mapObjectPlayAnimation(model1, 9, 0, 0);
    std115_mapObjectPlayAnimation(model2, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model3, 9, 0, 0);
    std115_mapObjectPlayAnimation(model4, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model5, 9, 0, 0);
    std115_mapObjectPlayAnimation(model6, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model7, 9, 0, 0);
    std115_mapObjectPlayAnimation(model7, 0xB, 0, 0);
    std115_mapObjectPlayAnimation(model8, 0xB, 0, 0);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008E320 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void demoScene24(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 mode);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 elapsed3;
    u32 waitFrames3;
    f32 frame;
    u32 handle2;
    u32 h06af0400;
    u32 h06bc0400;
    u32 h06be0400a;
    u32 h06be0400b;
    u32 h0d071000;
    u32 h0d071001;
    u32 h0d071003;
    u32 h0d071002;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank, 1);
    ((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);

    waitFrames2 = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1E0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    h06af0400 = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h06be0400a = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400b = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400a));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400b));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), 4);

    cameraPlayAnime(ctx, 0x0CFD1800, 0, 0);

    waitFrames3 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames3 = (u32)lbl_8047C1D0;
        if (waitFrames3 < 1) {
            waitFrames3 = 1;
        }
    }
    for (elapsed3 = 0; elapsed3 < waitFrames3; ) {
        _threadSwitch();
        elapsed3 += GSgetFrameStep();
    }

    h0d071000 = std112_modelGetResourceID(PC_PTR(0x0D071000));
    h0d071001 = std112_modelGetResourceID(PC_PTR(0x0D071001));
    h0d071003 = std112_modelGetResourceID(PC_PTR(0x0D071003));
    h0d071002 = std112_modelGetResourceID(PC_PTR(0x0D071002));

    std070_characterAttachToCharacter(ctx, h06af0400, ctx, h0d071000, 0);
    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0d071001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400a, ctx, h0d071003, 0);
    std070_characterAttachToCharacter(ctx, h06be0400b, ctx, h0d071002, 0);

    std115_mapObjectPlayAnimation(h06af0400, 0xA, 0, 0);
    std115_mapObjectPlayAnimation(h06bc0400, 5, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400a, 0xE, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400b, 0xE, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#else
void demoScene24(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 mode);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 elapsed3;
    u32 waitFrames3;
    f32 frame;
    u32 handle2;
    u32 h06af0400;
    u32 h06bc0400;
    u32 h06be0400a;
    u32 h06be0400b;
    u32 h0d071000;
    u32 h0d071001;
    u32 h0d071003;
    u32 h0d071002;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank, 1);
    ((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = GSresGetResource(ctx, 0x0CE61004);
    GSmodelSetAnimIndex((struct GSmodel*)handle2, 0);
    GSmodelGetFrameCount(handle2, &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex((struct GSmodel*)handle2, 0);
    GSmodelSetAnimFrame(handle2, frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation((struct GSmodel*)handle2);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);

    waitFrames2 = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1E0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    h06af0400 = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h06be0400a = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h06be0400b = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400a));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400b));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), 4);

    cameraPlayAnime(ctx, 0x0CFD1800, 0, 0);

    waitFrames3 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames3 = (u32)lbl_8047C1D0;
        if (waitFrames3 < 1) {
            waitFrames3 = 1;
        }
    }
    for (elapsed3 = 0; elapsed3 < waitFrames3; ) {
        _threadSwitch();
        elapsed3 += GSgetFrameStep();
    }

    h0d071000 = std112_modelGetResourceID(PC_PTR(0x0D071000));
    h0d071001 = std112_modelGetResourceID(PC_PTR(0x0D071001));
    h0d071003 = std112_modelGetResourceID(PC_PTR(0x0D071003));
    h0d071002 = std112_modelGetResourceID(PC_PTR(0x0D071002));

    std070_characterAttachToCharacter(ctx, h06af0400, ctx, h0d071000, 0);
    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0d071001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400a, ctx, h0d071003, 0);
    std070_characterAttachToCharacter(ctx, h06be0400b, ctx, h0d071002, 0);

    std115_mapObjectPlayAnimation(h06af0400, 0xA, 0, 0);
    std115_mapObjectPlayAnimation(h06bc0400, 5, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400a, 0xE, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400b, 0xE, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#endif

/* 0x8008E7D4 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void demoScene23(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 mode);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void scriptWaitSyncMotion(u32 id, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 elapsed3;
    u32 waitFrames3;
    f32 frame;
    u32 handle2;
    u32 h06af0400;
    u32 h06bc0400;
    u32 h06be0400;
    u32 h0d061000;
    u32 h0d061001;
    u32 h0d061002;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank, 1);
    ((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);

    waitFrames2 = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1E0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    h06af0400 = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h06be0400 = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), 4);

    cameraPlayAnime(ctx, 0x0CFC1800, 0, 0);

    waitFrames3 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames3 = (u32)lbl_8047C1D0;
        if (waitFrames3 < 1) {
            waitFrames3 = 1;
        }
    }
    for (elapsed3 = 0; elapsed3 < waitFrames3; ) {
        _threadSwitch();
        elapsed3 += GSgetFrameStep();
    }

    h0d061000 = std112_modelGetResourceID(PC_PTR(0x0D061000));
    h0d061001 = std112_modelGetResourceID(PC_PTR(0x0D061001));
    h0d061002 = std112_modelGetResourceID(PC_PTR(0x0D061002));

    std070_characterAttachToCharacter(ctx, h06af0400, ctx, h0d061000, 2);
    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0d061001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400, ctx, h0d061002, 0);

    std115_mapObjectPlayAnimation(h06af0400, 7, 0, 0);
    scriptWaitSyncMotion(h06af0400, 1);
    std115_mapObjectPlayAnimation(h06af0400, 8, 0, 0);
    std115_mapObjectPlayAnimation(h06bc0400, 5, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400, 0xE, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#else
void demoScene23(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 bank);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 mode);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void scriptWaitSyncMotion(u32 id, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 elapsed3;
    u32 waitFrames3;
    f32 frame;
    u32 handle2;
    u32 h06af0400;
    u32 h06bc0400;
    u32 h06be0400;
    u32 h0d061000;
    u32 h0d061001;
    u32 h0d061002;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank, 1);
    ((GSmodel*)PC_PTR(handle2))->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = GSresGetResource(ctx, 0x0CE61004);
    GSmodelSetAnimIndex((struct GSmodel*)handle2, 0);
    GSmodelGetFrameCount(handle2, &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex((struct GSmodel*)handle2, 0);
    GSmodelSetAnimFrame(handle2, frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation((struct GSmodel*)handle2);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);

    waitFrames2 = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1E0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    h06af0400 = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h06be0400 = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400));
    GSmodelSetShadowFlags(PC_PTR(handle2), 2);
    GSmodelSetShadowLight(handle2, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(handle2), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06af0400)), 4);

    cameraPlayAnime(ctx, 0x0CFC1800, 0, 0);

    waitFrames3 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames3 = (u32)lbl_8047C1D0;
        if (waitFrames3 < 1) {
            waitFrames3 = 1;
        }
    }
    for (elapsed3 = 0; elapsed3 < waitFrames3; ) {
        _threadSwitch();
        elapsed3 += GSgetFrameStep();
    }

    h0d061000 = std112_modelGetResourceID(PC_PTR(0x0D061000));
    h0d061001 = std112_modelGetResourceID(PC_PTR(0x0D061001));
    h0d061002 = std112_modelGetResourceID(PC_PTR(0x0D061002));

    std070_characterAttachToCharacter(ctx, h06af0400, ctx, h0d061000, 2);
    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0d061001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400, ctx, h0d061002, 0);

    std115_mapObjectPlayAnimation(h06af0400, 7, 0, 0);
    scriptWaitSyncMotion(h06af0400, 1);
    std115_mapObjectPlayAnimation(h06af0400, 8, 0, 0);
    std115_mapObjectPlayAnimation(h06bc0400, 5, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400, 0xE, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#endif

/* 0x8008EC28 */
void demoScene22(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0CFB1800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0D051000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    std115_mapObjectPlayAnimation(iconHandle, 7, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008EED0 */
void demoScene21(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void scriptWaitSyncMotion(u32 id, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    std115_mapObjectPlayAnimation(iconHandle, 4, 0, 0);
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0CFA1800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0D031000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    scriptWaitSyncMotion(iconHandle, 1);
    std115_mapObjectPlayAnimation(iconHandle, 5, 0, 0);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008F190 */
void demoScene20(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void scriptWaitSyncMotion(u32 id, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);
    waitFrames = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1E0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    std115_mapObjectPlayAnimation(iconHandle, 0, 0, 0);
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0CF91800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0CF81000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    std115_mapObjectPlayAnimation(iconHandle, 1, 0, 0);
    scriptWaitSyncMotion(iconHandle, 1);
    std115_mapObjectPlayAnimation(iconHandle, 2, 0, 0);
    scriptWaitSyncMotion(iconHandle, 1);
    std115_mapObjectPlayAnimation(iconHandle, 3, 0, 0);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008F524 */
void demoScene19(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 el1;
    u32 wf1;
    u32 elapsed;
    u32 waitFrames;
    u32 sA;
    u32 sB;
    u32 sC;
    f32 frame;
    u32 cameraElapsed;
    u32 cameraWaitFrames;
    u32 handle2;
    u32 modelA;
    u32 modelB;
    u32 modelC;
    u32 animA;
    u32 animB;
    u32 animC;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    wf1 = 1;
    if (fn_800D37CC() == 0x32) {
        wf1 = (u32)lbl_8047C1D0;
        if (wf1 < 1) {
            wf1 = 1;
        }
    }
    for (el1 = 0; el1 < wf1; ) {
        _threadSwitch();
        el1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(((GSmodel *)PC_PTR(handle2))->linkedGSparticleBank, 1);
    ((GSmodel *)PC_PTR(handle2))->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x111B1400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000)), 4);

    std115_mapObjectPlayAnimation(0x0CE61000, 3, 0, 0);
    waitFrames = 0x32;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1E0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    modelA = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    modelB = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    modelC = std112_modelGetResourceID(PC_PTR(0x06BE0400));

    sA = PC_PTR32_IMPLICIT(GSresGetResource(ctx, modelA));
    GSmodelSetShadowFlags(PC_PTR(sA), 2);
    GSmodelSetShadowLight(sA, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(sA), 1, &lbl_8047A694);

    sB = PC_PTR32_IMPLICIT(GSresGetResource(ctx, modelB));
    GSmodelSetShadowFlags(PC_PTR(sB), 2);
    GSmodelSetShadowLight(sB, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(sB), 1, &lbl_8047A694);

    sC = PC_PTR32_IMPLICIT(GSresGetResource(ctx, modelC));
    GSmodelSetShadowFlags(PC_PTR(sC), 2);
    GSmodelSetShadowLight(sC, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(sC), 1, &lbl_8047A694);

    cameraPlayAnime(ctx, 0x0CF71800, 0, 0);
    cameraWaitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        cameraWaitFrames = (u32)lbl_8047C1D0;
        if (cameraWaitFrames < 1) {
            cameraWaitFrames = 1;
        }
    }
    for (cameraElapsed = 0; cameraElapsed < cameraWaitFrames; ) {
        _threadSwitch();
        cameraElapsed += GSgetFrameStep();
    }

    animA = std112_modelGetResourceID(PC_PTR(0x0CEE1000));
    animB = std112_modelGetResourceID(PC_PTR(0x0CEE1002));
    animC = std112_modelGetResourceID(PC_PTR(0x0CEE1001));

    std070_characterAttachToCharacter(ctx, modelA, ctx, animA, 0);
    std070_characterAttachToCharacter(ctx, modelB, ctx, animB, 0);
    std070_characterAttachToCharacter(ctx, modelC, ctx, animC, 0);

    std115_mapObjectPlayAnimation(modelA, 3, 0, 1);
    std115_mapObjectPlayAnimation(modelB, 4, 0, 1);
    std115_mapObjectPlayAnimation(modelC, 4, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008F91C */
void demoScene18(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 elapsed;
    u32 waitFrames;
    u32 cameraElapsed;
    u32 cameraWaitFrames;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 modelA;
    u32 modelB;
    u32 animA;
    u32 animB;
    u32 shadowB;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    modelA = std112_modelGetResourceID(PC_PTR(0x06BD0400));
    modelB = std112_modelGetResourceID(PC_PTR(0x06BA0400));

    animA = PC_PTR32_IMPLICIT(GSresGetResource(ctx, modelA));
    GSmodelSetShadowFlags(PC_PTR(animA), 2);
    GSmodelSetShadowLight(animA, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(animA), 1, &lbl_8047A694);

    shadowB = PC_PTR32_IMPLICIT(GSresGetResource(ctx, modelB));
    GSmodelSetShadowFlags(PC_PTR(shadowB), 2);
    GSmodelSetShadowLight(shadowB, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(shadowB), 1, &lbl_8047A694);

    cameraPlayAnime(ctx, 0x0CF61800, 0, 0);
    cameraWaitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        cameraWaitFrames = (u32)lbl_8047C1D0;
        if (cameraWaitFrames < 1) {
            cameraWaitFrames = 1;
        }
    }
    for (cameraElapsed = 0; cameraElapsed < cameraWaitFrames; ) {
        _threadSwitch();
        cameraElapsed += GSgetFrameStep();
    }

    animA = std112_modelGetResourceID(PC_PTR(0x0CED1000));
    animB = std112_modelGetResourceID(PC_PTR(0x0CED1001));

    std070_characterAttachToCharacter(ctx, modelB, ctx, animA, 0);
    std070_characterAttachToCharacter(ctx, modelA, ctx, animB, 0);

    std115_mapObjectPlayAnimation(modelB, 2, 0, 1);
    std115_mapObjectPlayAnimation(modelA, 2, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008FBF4 */
void demoScene17(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06AF0400));
    std115_mapObjectPlayAnimation(iconHandle, 0, 0, 1);
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x11511400)));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle)), 4);

    cameraPlayAnime(ctx, 0x0CF51800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0CEC1000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x8008FE94 */
void demoScene16(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32 ctx, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32 *param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
    extern void cameraWaitSyncAnime(s32 sync);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorRequestChange(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(u32 a, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(void *texture, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimIndex(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32 *out, u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 val);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelStartAnimation(u32 handle);
#endif

    u32 waitFrames;
    u32 elapsed;
    GSmodel *material;
    f32 frame;
    u32 handle2;
    u32 iconHandle;
    u32 iconResult;
    u32 finalResult;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    material = (GSmodel *)PC_PTR(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(material->linkedGSparticleBank, 1);
    material->linkedGSparticleBank = NULL;

    frame = lbl_8047C1D4;
    handle2 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelGetFrameCount(PC_PTR(handle2), &frame, 0);
    frame = frame - lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(handle2), 0);
    GSmodelSetAnimFrame(PC_PTR(handle2), frame);
    GSmodelSetAnimType(handle2, 0);
    GSmodelStartAnimation(PC_PTR(handle2));

    iconHandle = std112_modelGetResourceID(PC_PTR(0x06BD0400));
    iconResult = PC_PTR32_IMPLICIT(GSresGetResource(ctx, iconHandle));
    GSmodelSetShadowFlags(PC_PTR(iconResult), 2);
    GSmodelSetShadowLight(iconResult, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(iconResult), 1, &lbl_8047A694);

    cameraPlayAnime(ctx, 0x0CF41800, 0, 0);
    waitFrames = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames = (u32)lbl_8047C1D0;
        if (waitFrames < 1) {
            waitFrames = 1;
        }
    }
    for (elapsed = 0; elapsed < waitFrames; ) {
        _threadSwitch();
        elapsed += GSgetFrameStep();
    }

    finalResult = std112_modelGetResourceID(PC_PTR(0x0CEB1000));
    std070_characterAttachToCharacter(ctx, iconHandle, ctx, finalResult, 0);
    std115_mapObjectPlayAnimation(iconHandle, 2, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}

/* 0x80090100 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void demoScene15(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(u32 ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 elapsed3;
    u32 waitFrames3;
    u32 model;
    u32 base;
    f32 frame;
    u32 h06bc0400;
    u32 h0cea1000;
    u32 h0d290400a;
    u32 h06be0400a;
    u32 h0d240400a;
    u32 h06be0400b;
    u32 h0d240400b;
    u32 h0d290400b;
    u32 h0d240400c;
    u32 h0cea1006;
    u32 h0cea1007;
    u32 h0cea1001;
    u32 h0cea1002;
    u32 h0cea1003;
    u32 h0cea1004;
    u32 h0cea1005;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    waitFrames2 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1D0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    base = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(PC_PTR(*(u32*)PC_PTR((base + 0x144))), 1);
    *(u32*)PC_PTR((base + 0x144)) = 0;

    frame = lbl_8047C1D4;
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61004));
    GSmodelSetAnimIndex(PC_PTR(model), 0);
    GSmodelGetFrameCount(PC_PTR(model), &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex(PC_PTR(model), 0);
    GSmodelSetAnimFrame(PC_PTR(model), frame);
    GSmodelSetAnimType(model, 0);
    GSmodelStartAnimation(PC_PTR(model));

    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h0cea1000 = std112_modelGetResourceID(PC_PTR(0x0CEA1000));

    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);

    h0d290400a = std112_modelGetResourceID(PC_PTR(0x0D290400));
    h06be0400a = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h0d240400a = std112_modelGetResourceID(PC_PTR(0x0D240400));
    h06be0400b = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h0d240400b = std112_modelGetResourceID(PC_PTR(0x0D240400));
    h0d290400b = std112_modelGetResourceID(PC_PTR(0x0D290400));
    h0d240400c = std112_modelGetResourceID(PC_PTR(0x0D240400));

    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d290400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d240400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d240400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d290400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d240400c));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);

    cameraPlayAnime(ctx, 0x0CF31800, 0, 0);

    waitFrames3 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames3 = (u32)lbl_8047C1D0;
        if (waitFrames3 < 1) {
            waitFrames3 = 1;
        }
    }
    for (elapsed3 = 0; elapsed3 < waitFrames3; ) {
        _threadSwitch();
        elapsed3 += GSgetFrameStep();
    }

    h0cea1006 = std112_modelGetResourceID(PC_PTR(0x0CEA1006));
    h0cea1007 = std112_modelGetResourceID(PC_PTR(0x0CEA1007));
    h0cea1001 = std112_modelGetResourceID(PC_PTR(0x0CEA1001));
    h0cea1002 = std112_modelGetResourceID(PC_PTR(0x0CEA1002));
    h0cea1003 = std112_modelGetResourceID(PC_PTR(0x0CEA1003));
    h0cea1004 = std112_modelGetResourceID(PC_PTR(0x0CEA1004));
    h0cea1005 = std112_modelGetResourceID(PC_PTR(0x0CEA1005));

    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0cea1000, 0);
    std070_characterAttachToCharacter(ctx, h0d290400a, ctx, h0cea1006, 0);
    std070_characterAttachToCharacter(ctx, h06be0400a, ctx, h0cea1007, 0);
    std070_characterAttachToCharacter(ctx, h0d240400a, ctx, h0cea1001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400b, ctx, h0cea1002, 0);
    std070_characterAttachToCharacter(ctx, h0d240400b, ctx, h0cea1003, 0);
    std070_characterAttachToCharacter(ctx, h0d290400b, ctx, h0cea1004, 0);
    std070_characterAttachToCharacter(ctx, h0d240400c, ctx, h0cea1005, 0);

    std115_mapObjectPlayAnimation(h06bc0400, 3, 0, 1);
    std115_mapObjectPlayAnimation(h0d290400a, 4, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400a, 3, 0, 1);
    std115_mapObjectPlayAnimation(h0d240400a, 4, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400b, 3, 0, 1);
    std115_mapObjectPlayAnimation(h0d240400b, 5, 0, 1);
    std115_mapObjectPlayAnimation(h0d290400b, 4, 0, 1);
    std115_mapObjectPlayAnimation(h0d240400c, 4, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#else
void demoScene15(u32 ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(u32 w, u32 h);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32 handle, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32 handle, u32 val, u32* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetFrameCount(u32 handle, f32* out, u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetAnimFrame(u32 handle, f32 frame);
#endif
    extern void GSmodelSetAnimType(u32 handle, u32 type);
    extern void std116_mapObjectStopAnimation(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32 id, u32 slot, u32 x, u32 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKill(u32 ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32 id);
#endif
    extern void std070_characterAttachToCharacter(u32 ctx, u32 modelHandle, u32 ctx2, u32 handle, u32 flags);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32 ctx, u32 id, u32 a, u32 b);
#endif
    extern void cameraWaitSyncAnime(s32 sync);

    u32 elapsed1;
    u32 waitFrames1;
    u32 elapsed2;
    u32 waitFrames2;
    u32 elapsed3;
    u32 waitFrames3;
    u32 model;
    u32 base;
    f32 frame;
    u32 h06bc0400;
    u32 h0cea1000;
    u32 h0d290400a;
    u32 h06be0400a;
    u32 h0d240400a;
    u32 h06be0400b;
    u32 h0d240400b;
    u32 h0d290400b;
    u32 h0d240400c;
    u32 h0cea1006;
    u32 h0cea1007;
    u32 h0cea1001;
    u32 h0cea1002;
    u32 h0cea1003;
    u32 h0cea1004;
    u32 h0cea1005;

    lbl_8047A690 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61602));
    lbl_8047A694 = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61002));
    GSmodelSetShadowTextureSize(0x280, 0x1E0);
    std115_mapObjectPlayAnimation(0x0CE61000, 0, 0, 0);

    waitFrames1 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames1 = (u32)lbl_8047C1D0;
        if (waitFrames1 < 1) {
            waitFrames1 = 1;
        }
    }
    for (elapsed1 = 0; elapsed1 < waitFrames1; ) {
        _threadSwitch();
        elapsed1 += GSgetFrameStep();
    }

    std116_mapObjectStopAnimation(0x0CE61000);

    waitFrames2 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames2 = (u32)lbl_8047C1D0;
        if (waitFrames2 < 1) {
            waitFrames2 = 1;
        }
    }
    for (elapsed2 = 0; elapsed2 < waitFrames2; ) {
        _threadSwitch();
        elapsed2 += GSgetFrameStep();
    }

    base = PC_PTR32_IMPLICIT(GSresGetResource(ctx, 0x0CE61000));
    gsParticleKill(PC_PTR(*(u32*)PC_PTR((base + 0x144))), 1);
    *(u32*)PC_PTR((base + 0x144)) = 0;

    frame = lbl_8047C1D4;
    model = GSresGetResource(ctx, 0x0CE61004);
    GSmodelSetAnimIndex((struct GSmodel*)model, 0);
    GSmodelGetFrameCount(model, &frame, 0);
    frame -= lbl_8047C1D8;
    GSmodelSetAnimIndex((struct GSmodel*)model, 0);
    GSmodelSetAnimFrame(model, frame);
    GSmodelSetAnimType(model, 0);
    GSmodelStartAnimation((struct GSmodel*)model);

    h06bc0400 = std112_modelGetResourceID(PC_PTR(0x06BC0400));
    h0cea1000 = std112_modelGetResourceID(PC_PTR(0x0CEA1000));

    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06bc0400));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);

    h0d290400a = std112_modelGetResourceID(PC_PTR(0x0D290400));
    h06be0400a = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h0d240400a = std112_modelGetResourceID(PC_PTR(0x0D240400));
    h06be0400b = std112_modelGetResourceID(PC_PTR(0x06BE0400));
    h0d240400b = std112_modelGetResourceID(PC_PTR(0x0D240400));
    h0d290400b = std112_modelGetResourceID(PC_PTR(0x0D290400));
    h0d240400c = std112_modelGetResourceID(PC_PTR(0x0D240400));

    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d290400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d240400a));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h06be0400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d240400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d290400b));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);
    model = PC_PTR32_IMPLICIT(GSresGetResource(ctx, h0d240400c));
    GSmodelSetShadowFlags(PC_PTR(model), 2);
    GSmodelSetShadowLight(model, lbl_8047A690);
    GSmodelSetShadowSurface(PC_PTR(model), 1, &lbl_8047A694);

    cameraPlayAnime(ctx, 0x0CF31800, 0, 0);

    waitFrames3 = 1;
    if (fn_800D37CC() == 0x32) {
        waitFrames3 = (u32)lbl_8047C1D0;
        if (waitFrames3 < 1) {
            waitFrames3 = 1;
        }
    }
    for (elapsed3 = 0; elapsed3 < waitFrames3; ) {
        _threadSwitch();
        elapsed3 += GSgetFrameStep();
    }

    h0cea1006 = std112_modelGetResourceID(PC_PTR(0x0CEA1006));
    h0cea1007 = std112_modelGetResourceID(PC_PTR(0x0CEA1007));
    h0cea1001 = std112_modelGetResourceID(PC_PTR(0x0CEA1001));
    h0cea1002 = std112_modelGetResourceID(PC_PTR(0x0CEA1002));
    h0cea1003 = std112_modelGetResourceID(PC_PTR(0x0CEA1003));
    h0cea1004 = std112_modelGetResourceID(PC_PTR(0x0CEA1004));
    h0cea1005 = std112_modelGetResourceID(PC_PTR(0x0CEA1005));

    std070_characterAttachToCharacter(ctx, h06bc0400, ctx, h0cea1000, 0);
    std070_characterAttachToCharacter(ctx, h0d290400a, ctx, h0cea1006, 0);
    std070_characterAttachToCharacter(ctx, h06be0400a, ctx, h0cea1007, 0);
    std070_characterAttachToCharacter(ctx, h0d240400a, ctx, h0cea1001, 0);
    std070_characterAttachToCharacter(ctx, h06be0400b, ctx, h0cea1002, 0);
    std070_characterAttachToCharacter(ctx, h0d240400b, ctx, h0cea1003, 0);
    std070_characterAttachToCharacter(ctx, h0d290400b, ctx, h0cea1004, 0);
    std070_characterAttachToCharacter(ctx, h0d240400c, ctx, h0cea1005, 0);

    std115_mapObjectPlayAnimation(h06bc0400, 3, 0, 1);
    std115_mapObjectPlayAnimation(h0d290400a, 4, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400a, 3, 0, 1);
    std115_mapObjectPlayAnimation(h0d240400a, 4, 0, 1);
    std115_mapObjectPlayAnimation(h06be0400b, 3, 0, 1);
    std115_mapObjectPlayAnimation(h0d240400b, 5, 0, 1);
    std115_mapObjectPlayAnimation(h0d290400b, 4, 0, 1);
    std115_mapObjectPlayAnimation(h0d240400c, 4, 0, 1);

    cameraWaitSyncAnime(1);
    floorRequestChange(ROOM_S2_OUT_3);
    floorSetFadeScript(0, 0);
}
#endif
