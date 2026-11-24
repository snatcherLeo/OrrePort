/**
 * @file wazaSequenceCamera.c
 * @brief wazaSequenceCamera / battleCamera: camera control during move animation
 * playback (front block is battleCamera code -- kept in this file per
 * split spec, not sub-split further).
 *
 * Split from the former game/battle/battle_waza.c CodeCandidate bucket
 * (0x801D1470-0x801DE698); see config/GC6E01/splits.txt for the exact
 * address range of this translation unit. Shared typedefs and cross-TU
 * forward declarations live in include/game/battle/battle_waza_types.h.
 */

#include "game/battle/battle_waza_types.h"
#include "crt/math_ppc.h"
#include "dolphin/mtx.h"
#include "game/battle/waza_camera_stop.h"
#include "game/gs_bound_offsets.h" /* the model box's minimum / maximum / size offsets (larger on the PC) */

#ifdef TARGET_PC
/* ORRE_BCAM_LOG=1's test log (src/game/pc_bcam_log.c; docs/port/setup.md, "The battle-camera log"; 2026-11-19, for
 * OrrePort Studio's camera check, which replays every shot from the same inputs and random state and must match the
 * game frame for frame). Why: the shot's inputs (the owner, its slot, the aim, the shared generator's state, the
 * previous motion) only exist for a moment inside battleCameraStartWaza / battleCameraUpdateWaza, so the log is called
 * from there. Observe-only and not a hook (the project rules "Hook points"): the callee only reads (it never draws a random
 * number, never writes game state) and the pointer is NULL unless the log is on. Weak, as pobj.c's DRAWMTX log, so
 * units that link this file without the game's PC files still link. Stages: 0 = start (inputs), 1 = aim read (after
 * cameraRefreshTargetPos), 2 = procedural shot placed, 3 = authored camera started, 4 = the per-frame aim
 * (battleCameraUpdateWaza). Every OS the same (no platform code). */
extern void (*pc_bcam_hook)(s32 stage, void* owner, void* sequence, s32 value, const f32* a, const f32* b,
                            const f32* c) __attribute__((weak));
#define PC_BCAM(stage, owner, sequence, value, a, b, c)                                                              \
    do {                                                                                                              \
        if (&pc_bcam_hook != 0 && pc_bcam_hook != 0) {                                                                \
            pc_bcam_hook((stage), (owner), (sequence), (value), (const f32*)(a), (const f32*)(b), (const f32*)(c));  \
        }                                                                                                             \
    } while (0)
#endif

/* One field-of-view keyframe (lbl_804673D4 holds two plus a timing word). File scope: two functions used identical
 * local copies, which C treats as two different types (the PC compiler rejects the shared extern). */
typedef struct CameraFovKey {
    f32 start;
    f32 end;
    u32 startFrame;
    u32 endFrame;
} CameraFovKey;

/* camera.c defines cameraUpdate with an unused u32 parameter; it never reads r3 and no caller sets it. */
extern void cameraUpdate(void);
extern void GSvecAdd(void* a, void* b, void* c);

/* Camera parameters computed per move (lbl_804673A0, 0x34 bytes). */
typedef struct WazaSequenceCameraParams {
    s32 motion;         /* 0x00 */
    f32 size;           /* 0x04 */
    f32 height;         /* 0x08 */
    f32 rotationMin;    /* 0x0C */
    f32 rotationMax;    /* 0x10 */
    f32 rotationBase;   /* 0x14 */
    f32 heightMin;      /* 0x18 */
    f32 heightMax;      /* 0x1C */
    f32 distanceMin;    /* 0x20 */
    f32 distanceMax;    /* 0x24 */
    f32 nearLength;     /* 0x28 */
    f32 middleLength;   /* 0x2C */
    f32 farLength;      /* 0x30 */
} WazaSequenceCameraParams;


/**
 * wazaSequenceCameraGetPattern__Fbi - Waza multi-hit advance.
 * Address: 0x801D2B4C | Size: 0x120
 */
#ifndef WAZA_CAMERA_FILE_ONLY
void* wazaSequenceCameraGetPattern__Fbi(u8 shortTable, s32 flags) {
    typedef struct WazaCameraPattern {
        f32 duration;
        u8 data[0x48];
    } WazaCameraPattern;
    extern f32 GSrandf(u8, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fadeEffectGetRandom__FUl(u32);
#endif
    extern WazaCameraPattern lbl_80371F60[];
    extern WazaCameraPattern lbl_803721C0[];
    WazaCameraPattern* table;
    s32 count;
    s32 i;
    f32 frame = GSrandf(shortTable, flags);
    f32 end = 0.0f;

    if (shortTable != 0) {
        table = lbl_80371F60;
        count = 8;
        if (flags & 0x20) {
            return &table[4];
        }
        if (flags & 0x40) {
            return &table[5];
        }
        if (flags & 0x80) {
            return &table[6];
        }
    } else {
        table = lbl_803721C0;
        count = 13;
        if (flags & 0x20) {
            return &table[9];
        }
        if (flags & 0x40) {
            return &table[10];
        }
        if (flags & 0x80) {
            return &table[11];
        }
    }

    for (i = 0; i < count; i++, table++) {
        end += table->duration;
        if (frame < end) {
            return table;
        }
    }

    if (shortTable != 0) {
        return &lbl_80371F60[_fadeEffectGetRandom__FUl(8)];
    }
    return &lbl_803721C0[_fadeEffectGetRandom__FUl(13)];
}
#endif

/**
 * battleCameraWazaGetOwner - Waza get global state from SDA.
 * Address: 0x801D2C6C | Size: 0x8
 */
extern void* lbl_8047B3EC;
extern void* lbl_8047B3F0;
extern u32 lbl_8047B3E8;
extern s32 _resource_id_count;
extern f32 lbl_80478CDC;

extern u8 lbl_80467CC0[];
extern void wazaSequenceOwnerUpdate(void* obj);
extern void wazaSequenceOwnerUpdateBlink(void* obj);
extern void wazaSequenceOwnerFreeSequences(void* obj);
extern void wazaSequenceOwnerRelease(void* obj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
extern s32 fn_8017B2CC(s32 id);
extern void fn_800F915C(s32 id);
extern void GSresReleaseById(s32 id);
#ifndef WAZA_CAMERA_FILE_ONLY
void* battleCameraWazaGetOwner(void) {
    return lbl_8047B3EC;
}
#endif

/* =========================================================================
 * WAZA ANIMATION STATE MACHINES (0x801D2C74 - 0x801D7230)
 *
 * Large state machines that drive multi-step move animations.
 * These contain extensive float math and switch statements.
 * ========================================================================= */

/**
 * battleCameraStartOwner - End the running move camera, then start the battle camera on one Pokemon or trainer
 * with no move (battleCameraStartWaza(owner, NULL)). battleCameraStartRandom calls it for the one it picks.
 * Address: 0x801D2C74 | Size: 0xB4
 */
#ifndef WAZA_CAMERA_FILE_ONLY
/* NAME-T3 fn_801D2C74 */
void battleCameraStartOwner(void* owner) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetMode(s32 arg);
#endif
    extern void cameraStopAnime(void* arg);
    extern void fn_801765F4(s32 arg);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32  GSgetVersionMinor(void);
#endif
    extern u8   lbl_8047B3F4;

    void* obj;

    if (lbl_8047B3F4 != 0) {
        obj = lbl_8047B3F0;
        if (obj == NULL) {
            if (lbl_8047B3EC != NULL) {
                if (obj == NULL) {
                    GSscene_SetMode(8);
                } else {
                    if (*(u32*)((u8*)obj + 0x18) != 0 && *(u32*)((u8*)obj + 0x20) != 0) {
                        cameraStopAnime(obj);
                    }
                    lbl_8047B3F0 = NULL;
                }
                fn_801765F4(0);
                lbl_8047B3EC = NULL;
                if (GSgetVersionMinor() == 2) {
                    GSscene_SetMode(2);
                }
            }
            battleCameraStartWaza(owner, NULL);
        }
    }
}
#endif

/**
 * battleCameraUpdateWaza - Per-frame move camera (from wazaSequenceSysUpdate): aim at the move's target part,
 * step the field-of-view keys, and end the camera when its move has finished.
 * Address: 0x801D2D28 | Size: 0x26C
 */
/* NAME-T3 fn_801D2D28 */
void battleCameraUpdateWaza(void)
{
    typedef struct WazaCameraMove {
        u8 pad00[0x4C];
        u32 targetParts[3];
        u8 pad58[0x7C];
    } WazaCameraMove;
    typedef struct WazaCameraSequence {
        u8 pad00[0x24];
        PTR32_FIELD(void) model; /* the sequence record's 32-bit slots (GameCube layout on the PC too) */
        u8 pad28[4];
        PTR32_FIELD(WazaCameraMove) moves;
        u16 pad30;
        u16 moveIndex;
    } WazaCameraSequence;
    extern CameraFovKey lbl_804673D4[];
    extern u8 lbl_8047B3F4;
    extern u32 lbl_8047B3E8;
    extern f32 lbl_80478CDC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmodelGetPart(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSpartGetTransform(void*, Vec*, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSpartFree(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetPosition(void*, Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraPositionVector(Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecSub(Vec*, Vec*, Vec*);
#endif
    extern void cameraMoveTargetOfs(s32, Vec*, f32);
    extern f32 GSlerpGetLinearInterpolationFloat(f32, f32, f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraSetFov(f32);
#endif
    extern u32 cameraMoveEndCheckSpecial(u8);
    CameraFovKey* key = lbl_804673D4;
    WazaCameraMove* move;
    void* model;
    Vec target;
    Vec origin;
    void* part;
    f32 t;
    u32 frame;
    u32 partId;
    u32 i;

    if (lbl_8047B3F4 != 0 && lbl_8047B3EC != NULL &&
        (lbl_8047B3F0 == NULL || *(u32*)((u8*)lbl_8047B3F0 + 0x18) == 0 ||
         *(u32*)((u8*)lbl_8047B3F0 + 0x20) == 0)) {
        frame = GSgetFrameStep();
        move = &P32(((WazaCameraSequence*)lbl_8047B3EC)->moves)[
            ((WazaCameraSequence*)lbl_8047B3EC)->moveIndex];
        model = P32(((WazaCameraSequence*)lbl_8047B3EC)->model);
        if (lbl_8047B3F0 != NULL) {
            partId = move->targetParts[((u8*)lbl_8047B3F0)[0x17]];
        } else {
            partId = move->targetParts[2];
        }
        part = GSmodelGetPart(model, partId);
        if (part != NULL) {
            GSpartGetTransform(part, &target, NULL, NULL);
            if (lbl_8047B3F0 != NULL && *(u16*)((u8*)lbl_8047B3F0 + 0x2E) == 3 &&
                (*(u16*)((u8*)lbl_8047B3F0 + 0x2C) == 0x154 ||
                 *(u16*)((u8*)lbl_8047B3F0 + 0x2C) == 0x6E)) {
                GSscene_GetCameraPositionVector(&origin);
            } else {
                GSmodelGetPosition(PTR32_AT_T(void, (u8*)lbl_8047B3EC, 0x24), &origin);
            }
#ifdef TARGET_PC
            /* ORRE_BCAM_LOG (above): this frame's aim joint in world space and the origin it is taken from */
            PC_BCAM(4, lbl_8047B3EC, lbl_8047B3F0, (s32)partId, &target, &origin, NULL);
#endif
            GSvecSub(&target, &target, &origin);
            cameraMoveTargetOfs(7, &target, 0.2f);
            GSpartFree(part);
        }

        lbl_8047B3E8 += frame;
        for (i = 0; i < 2; i++, key++) {
            if (lbl_8047B3E8 <= key->startFrame) {
                lbl_80478CDC = key->start;
                break;
            }
            if (lbl_8047B3E8 <= key->endFrame) {
                t = (f32)(lbl_8047B3E8 - key->startFrame) /
                    (f32)(key->endFrame - key->startFrame);
                lbl_80478CDC = GSlerpGetLinearInterpolationFloat(key->start, key->end, t);
                break;
            }
            lbl_80478CDC = key->end;
        }
        cameraSetFov(lbl_80478CDC);

        if (lbl_8047B3F0 == NULL && (u8)cameraMoveEndCheckSpecial(0) == 0) {
            /* RULE-EXCEPTION(title-path): redundant status temporary preserves
             * retail's li/cmpwi/bne; see docs/RULE_EXCEPTIONS.md. */
            s32 moving = 0;
            if (moving == 0) {
                wazaCameraStop();
            }
        }
    }
}

/**
 * battleCameraEndWaza - End whichever move camera is running: stop its camera animation (or reset the scene
 * mode), clear the camera's owner and sequence. The out-of-line copy of the wazaCameraStop inline.
 * Address: 0x801D2F94 | Size: 0x88
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSscene_SetMode(s32 arg);
#endif
extern void cameraStopAnime(void* arg);
extern void fn_801765F4(s32 arg);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32  GSgetVersionMinor(void);
#endif
/* NAME-T3 fn_801D2F94 */
void battleCameraEndWaza(void) {
    void* obj;
    if (lbl_8047B3EC != NULL) {
        obj = lbl_8047B3F0;
        if (obj == NULL) {
            GSscene_SetMode(8);
        } else {
            if (*(u32*)((u8*)obj + 0x18) != 0 && *(u32*)((u8*)obj + 0x20) != 0) {
                cameraStopAnime(obj);
            }
            lbl_8047B3F0 = NULL;
        }
        fn_801765F4(0);
        lbl_8047B3EC = NULL;
        if (GSgetVersionMinor() == 2) {
            GSscene_SetMode(2);
        }
    }
}

/**
 * battleCameraWazaInit - Waza animation reset: set lbl_8047B3F4=1, lbl_8047B3EC=0, lbl_8047B3F0=0.
 * Address: 0x801D301C | Size: 0x18
 */
extern u8  lbl_8047B3F4;
void battleCameraWazaInit(void) {
    lbl_8047B3F4 = 1;
    lbl_8047B3EC = NULL;
    lbl_8047B3F0 = NULL;
}

/**
 * battleCameraEndOwnerWaza - Like battleCameraEndWaza, but only when this owner holds the move camera; called
 * when the owner's sequence stops.
 * Address: 0x801D3034 | Size: 0x88
 */
/* NAME-T3 fn_801D3034 */
void battleCameraEndOwnerWaza(void* state) {
    void* obj;
    if (state == lbl_8047B3EC) {
        obj = lbl_8047B3F0;
        if (obj == NULL) {
            GSscene_SetMode(8);
        } else {
            if (*(u32*)((u8*)obj + 0x18) != 0 && *(u32*)((u8*)obj + 0x20) != 0) {
                cameraStopAnime(obj);
            }
            lbl_8047B3F0 = NULL;
        }
        fn_801765F4(0);
        lbl_8047B3EC = NULL;
        if (GSgetVersionMinor() == 2) {
            GSscene_SetMode(2);
        }
    }
}

/**
 * battleCameraStartWaza - Waza animation state machine dispatcher.
 * Address: 0x801D30BC | Size: 0x3E0
 */
void battleCameraStartWaza(void* owner, void* sequence) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void clear__5GSvecFv(void* vec);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void* dst, void* src,
                                                   void* target, f32 t);
#endif
    extern u8 lbl_804673A0[];
    extern u16 battleGridGetNumPokemonsForTrainer(u32 id);
    extern void cameraSetTargetExt(u32 a, u32 b, u32 c);
    extern void cameraRefreshTargetPos(void);
    extern void cameraMoveStop(void);
    extern void cameraSetOffsetPosition(void* src);
    extern void cameraSetOffsetRotation(void* src);
    extern void cameraSetOffsetScale(void* src);
    extern void cameraPlayOffsetAnime(u32 groupId, u32 animationId,
                                      s32 frame, u8 loop);
    extern void* GSmodelGetBound(void* model);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetPosition(void* model, Vec* out);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetRotation(void* model, Vec* out);
#endif
    extern void fn_800D1070(s32 mode);
    u8* ownerBytes = owner;
    u8* sequenceBytes = sequence;
    Vec offsetPosition;
    Vec offsetRotation;
    Vec offsetScale;
    Vec center;
    Vec rootPosition;
    Vec ownerPosition;
    void* model;
    u8* bound;
    u8 reverse = FALSE;
    s32 paramsFlags = 0;
    s32 shift = 0;
    /* RULE-EXCEPTION(user-approved): separate shift for the offset-anime path, chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
    s32 animShift;
    f32 scaleValue;

    if (lbl_8047B3F4 == 0) {
        return;
    }
#ifdef TARGET_PC
    /* ORRE_BCAM_LOG (top of file): the owner's inputs, the previous motion and the generator's state, before any
     * of them changes */
    PC_BCAM(0, owner, sequence, 0, NULL, NULL, NULL);
#endif

    if (*(s8*)(ownerBytes + 0x76) < 0 &&
        (sequenceBytes == NULL || ((*(s32*)(sequenceBytes + 0x08) & 0x80) != 0x80))) {
        reverse = TRUE;
    }

    lbl_8047B3EC = owner;
    lbl_8047B3F0 = sequence;
    if (sequenceBytes != NULL) {
        battleCameraDisable();
        if (*(u32*)(sequenceBytes + 0x18) != 0 &&
            *(u32*)(sequenceBytes + 0x20) != 0 &&
            GSresGetResource(*(u32*)(sequenceBytes + 0x18),
                             *(u32*)(sequenceBytes + 0x20)) != NULL) {
            model = PTR32_AT_T(void, ownerBytes, 0x24);
            animShift = 0;
            if (reverse) {
                animShift = 4;
            }

            cameraPlayOffsetAnime(
                *(u32*)(sequenceBytes + 0x18), *(u32*)(sequenceBytes + 0x20),
                0, 0);

            if (*(u32*)(sequenceBytes + 0x08) & 0x4) {
                clear__5GSvecFv(&offsetPosition);
                clear__5GSvecFv(&offsetRotation);
                if (*(s8*)(ownerBytes + 0x76) < 0 &&
                    (*(u32*)(sequenceBytes + 0x08) & 0x02000000) == 0x02000000) {
                    offsetRotation.y = 3.1415927f;
                }
                if ((*(u32*)(sequenceBytes + 0x08) & 0x00800000) == 0x00800000) {
                    battleGridGetNormalisedScale((f32*)&offsetScale);
                    if ((*(u32*)(sequenceBytes + 0x08) & 0x01000000) == 0x01000000) {
                        offsetScale.y = 1.0f;
                    }
                } else {
                    set__5GSvecFfff((f32*)&offsetScale, 1.0f,
                                    1.0f, 1.0f);
                }
            } else {
                GSmodelGetPosition(model, &offsetPosition);
                if (*(u32*)(sequenceBytes + 0x08) & 0x00004000) {
                    bound = GSmodelGetBound(model);
                    GSlerpGetLinearInterpolationVector(
                        &center, bound + GS_BOUND_OFF_MIN, bound + GS_BOUND_OFF_MAX, 0.5f);
                    GSvecAdd(&offsetPosition, &offsetPosition, &center);
                }
                if (*(u16*)(sequenceBytes + 0x2E) == 2 &&
                    (ownerBytes[0x18] & 2) == 2 &&
                    (u8)GSmodelIsRootNullAdded(model) != 0) {
                    GSmodelGetRootPosition(model, (GSvec*)&rootPosition);
                    offsetPosition.y += rootPosition.y;
                }
                GSmodelGetRotation(model, &offsetRotation);
                scaleValue = wazaSequenceOwnerGetSizeScale(owner);
                set__5GSvecFfff(
                    (f32*)&offsetScale, scaleValue, scaleValue, scaleValue);
            }

            fn_801765F4(animShift);
            cameraMoveStop();
            cameraSetOffsetPosition(&offsetPosition);
            cameraSetOffsetRotation(&offsetRotation);
            cameraSetOffsetScale(&offsetScale);
            fn_800D1070(0);
            cameraUpdate();
#ifdef TARGET_PC
            /* ORRE_BCAM_LOG (top of file): an authored camera started (no random draws); its offset */
            PC_BCAM(3, owner, sequence, animShift, &offsetPosition, &offsetRotation, &offsetScale);
#endif
            return;
        }

        if (*(u32*)(sequenceBytes + 0x08) & 0x00200000) {
            paramsFlags |= 2;
        } else if (*(u32*)(sequenceBytes + 0x08) & 0x00000200) {
            paramsFlags |= 4;
        } else if (*(u32*)(sequenceBytes + 0x08) & 0x00400000) {
            GSmodelGetPosition(PTR32_AT_T(void, ownerBytes, 0x24), &ownerPosition);
            if (ownerPosition.z < 0.0f) {
                paramsFlags |= 4;
            } else {
                paramsFlags |= 8;
            }
        }

        if (*(u32*)(sequenceBytes + 0x08) & 0x00000400) {
            paramsFlags |= 0x20;
        } else if (*(u32*)(sequenceBytes + 0x08) & 0x00000800) {
            paramsFlags |= 0x40;
        } else if (*(u32*)(sequenceBytes + 0x08) & 0x00001000) {
            paramsFlags |= 0x80;
        }
    } else {
        paramsFlags = 4;
    }

    if (ownerBytes[0x75] == 0) {
        u16 count = battleGridGetNumPokemonsForTrainer(PC_PTR32(owner));

        if (count == 1) {
            paramsFlags &= ~0x1F;
            paramsFlags |= 0x10;
        } else if (count > 1) {
            paramsFlags &= ~0x1F;
            paramsFlags |= 1;
        }
    }

    GSscene_SetMode(7);
    cameraMoveStop();
    {
        u8* activeOwner = lbl_8047B3EC;
        u8* activeSequence = lbl_8047B3F0;
        u8* cameraParams = ((u8*)PTR32_AT(activeOwner, 0x2C)) +
                           *(u16*)(activeOwner + 0x32) * 0xD4;
        u32 targetId;

        if (activeSequence != NULL) {
            targetId = ((u32*)(cameraParams + 0x4C))[activeSequence[0x17]];
        } else {
            targetId = *(u32*)(cameraParams + 0x54);
        }
        cameraSetTargetExt(*(u32*)(ownerBytes + 0x00),
                           *(u32*)(ownerBytes + 0x04), targetId);
        cameraRefreshTargetPos();
#ifdef TARGET_PC
        /* ORRE_BCAM_LOG (top of file): the aim joint as the camera just read it, and the shot's flags */
        PC_BCAM(1, owner, sequence, paramsFlags, NULL, NULL, NULL);
#endif
        cameraSetTargetExt(*(u32*)(ownerBytes + 0x00),
                           *(u32*)(ownerBytes + 0x04), (u32)-1);
    }

    if (ownerBytes[0x75] == 0) {
        shift = 1;
    }

    _wazaSequenceCameraCalculateParams__FP13ModelSequenceiP24wazaSequenceCameraParams(
        owner, paramsFlags, lbl_804673A0);
    _wazaSequenceCameraSelectMotion__FP13ModelSequenceP12WazaSequenceP24wazaSequenceCameraParams(
        owner, sequence, lbl_804673A0);
    _wazaSequenceCameraDoPosition__FP13ModelSequenceP24wazaSequenceCameraParamsfb(
        owner, lbl_804673A0, shift, reverse);
    _wazaSequenceCameraDoFOV__FP13ModelSequenceP24wazaSequenceCameraParamsif(
        owner, lbl_804673A0, paramsFlags, shift);
    cameraUpdate();
#ifdef TARGET_PC
    /* ORRE_BCAM_LOG (top of file): the procedural shot placed: motion, parameters, zoom keys, state after */
    PC_BCAM(2, owner, sequence, (s32)reverse | (shift << 1), NULL, NULL, NULL);
#endif
}

/**
 * _wazaSequenceCameraDoPosition__FP13ModelSequenceP24wazaSequenceCameraParamsfb - Move animation state machine A.
 * Address: 0x801D349C | Size: 0xAE0
 * Massive state machine (~2.8KB) for a class of move animations.
 * Likely handles physical/contact move animations.
 */
void _wazaSequenceCameraDoPosition__FP13ModelSequenceP24wazaSequenceCameraParamsfb(
    void* modelSequence, void* cameraParams, s32 shift, u8 reverse)
{
    typedef struct WazaSequenceCameraPattern {
        u32 duration_index;
        u8 pad_04[0xD4 - 4];
    } WazaSequenceCameraPattern;
    typedef struct WazaSequenceCameraParamsLocal {
        s32 mode;
        u8 pad_04[8];
        f32 rotation_min;
        f32 rotation_max;
        f32 rotation_base;
        f32 height_min;
        f32 height_max;
        f32 distance_min;
        f32 distance_max;
        f32 out_near;
        f32 out_mid;
        f32 out_far;
    } WazaSequenceCameraParamsLocal;
    extern f32 GSrandf(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
    extern void cameraSetDistance(f32);
    extern void cameraSetHeight(f32);
    extern void cameraSetRotY(f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraUpdate(void);
#endif
    extern void cameraMovePosition(s32, Vec*, f32);
#ifdef TARGET_PC
    /* PC: the real prototype (camera.c: x, y, z, duration). The GameCube form below passes a leading int that only
     * reproduces retail's li r3,7; there ints and floats travel in separate registers, so the callee still reads its
     * four floats. On x64 arguments go by position: the callee read the 0.0f as y and rotB as z, and 0 as the
     * duration, so every motion-2 move camera snapped to rotation 0 and stayed there (2026-10-20, verified
     * against Dolphin: Willie's 10th move shot, Espeon's Confusion; snatcherLeo saw the static straight-on / side shots
     * in Bite, Tackle, Growl, Return and Helping Hand) */
    void cameraMoveRotationXYZ(f32 x, f32 y, f32 z, f32 duration);
#define WAZA_CAMERA_MOVE_ROTATION(lead, x, y, z, t) cameraMoveRotationXYZ((x), (y), (z), (t))
#else
    /* RULE-EXCEPTION(user-approved): codegen-only prototype with a leading int, reproducing retail's li r3,7 before the call - see docs/RULE_EXCEPTIONS.md */
    extern void cameraMoveRotationXYZ(s32, f32, f32, f32, f32);
#define WAZA_CAMERA_MOVE_ROTATION(lead, x, y, z, t) cameraMoveRotationXYZ((lead), (x), (y), (z), (t))
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraDirectionVector(Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetCameraDirectionVector(Vec*);
#endif
    u8* sequence = modelSequence;
    WazaSequenceCameraParamsLocal* params = cameraParams;
    WazaSequenceCameraPattern* pattern;
    s32 duration;

    pattern = (WazaSequenceCameraPattern*)
        (((u8*)PTR32_AT(sequence, 0x2C)) + *(u16*)(sequence + 0x32) * 0xD4);

    switch (params->mode) {
    case 0:
        _wazaSequenceCameraDoDollyPosition__FP21TemplateExpFileHeaderP24wazaSequenceCameraParamsfb(
            pattern, params, shift, reverse);
        break;

    case 1: {
        Vec direction;
        /* RULE-EXCEPTION(user-approved): declaration order chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
        f32 distance;
        f32 height;
        f32 offset;
        f32 square;
        u8* durationPtr = (u8*)pattern + (*(u32*)pattern << 2);

        duration = *(s32*)(durationPtr + 4);
        if (duration == 0) {
            duration = *(s32*)(durationPtr + 8);
        }
        duration <<= shift;

        offset = 10.0f * GSrandf() + 25.0f;
        height = 9.0f * GSrandf() + 1.0f;
        distance = 30.0f * GSrandf() + 20.0f;

        cameraSetDistance(distance);
        cameraSetHeight(height);
        cameraSetRotY(0.0f);
        cameraUpdate();

        GSscene_GetCameraDirectionVector(&direction);
        if (reverse) {
            direction.x += 10.0f;
        } else {
            direction.x -= 10.0f;
        }
        GSscene_SetCameraDirectionVector(&direction);
        if (reverse) {
            direction.x += offset;
        } else {
            direction.x -= offset;
        }
        cameraMovePosition(7, &direction, (f32)duration / (f32)fn_800D37CC());

        square = offset * offset + height * height;
        params->out_near = sqrtf(square);
        params->out_far = sqrtf(distance * distance + square);
        params->out_mid = 0.5f * (params->out_near + params->out_far);
        break;
    }

    case 2: {
        f32 distance;
        f32 height;
        f32 rotA;
        f32 rotB;
        f32 length;
        u8* durationPtr = (u8*)pattern + (*(u32*)pattern << 2);

        duration = *(s32*)(durationPtr + 4);
        if (duration == 0) {
            duration = *(s32*)(durationPtr + 8);
        }
        duration <<= shift;

        /* RULE-EXCEPTION(user-approved): distance_min staged in the reused scalar length for retail's f28 - see docs/RULE_EXCEPTIONS.md */
        length = params->distance_min;
        distance = length + (params->distance_max - length) * GSrandf();
        height = params->height_min +
            (params->height_max - params->height_min) * GSrandf();
        if (reverse) {
            /* RULE-EXCEPTION(user-approved): rotation_min staged in rotA (reused scalar) for allocation - see docs/RULE_EXCEPTIONS.md */
            rotA = params->rotation_min;
            rotA = (params->rotation_base - rotA) -
                   (params->rotation_max - rotA) * GSrandf();
            rotB = (params->rotation_base - params->rotation_min) -
                   (params->rotation_max - params->rotation_min) * GSrandf();
        } else {
            /* RULE-EXCEPTION(user-approved): rotation_min staged in rotA (reused scalar) for allocation - see docs/RULE_EXCEPTIONS.md */
            rotA = params->rotation_min;
            rotA = (params->rotation_max - rotA) * GSrandf() +
                   (params->rotation_base + rotA);
            rotB = (params->rotation_max - params->rotation_min) * GSrandf() +
                   (params->rotation_base + params->rotation_min);
        }
        if (rotA > rotB) {
            f32 swap = rotA;
            rotA = rotB;
            rotB = swap;
        }

        cameraSetDistance(distance);
        cameraSetHeight(height);
        cameraSetRotY(rotA);
        cameraUpdate();
        WAZA_CAMERA_MOVE_ROTATION(7, 0.0f, rotB, 0.0f, (f32)duration / (f32)fn_800D37CC());

        length = sqrtf(distance * distance + height * height);
        params->out_far = length;
        params->out_mid = length;
        params->out_near = length;
        break;
    }

    case 3: {
        Vec direction;
        f32 distance;
        f32 rotation;
        f32 length;
        u8* durationPtr = (u8*)pattern + (*(u32*)pattern << 2);

        duration = *(s32*)(durationPtr + 4);
        if (duration == 0) {
            duration = *(s32*)(durationPtr + 8);
        }
        duration <<= shift;

        /* RULE-EXCEPTION(user-approved): distance_min staged in the reused scalar length for retail's f28 - see docs/RULE_EXCEPTIONS.md */
        length = params->distance_min;
        distance = length + (params->distance_max - length) * GSrandf();
        /* RULE-EXCEPTION(user-approved): the height is kept in the reused scalar length for retail's f28 - see docs/RULE_EXCEPTIONS.md */
        length = params->height_min +
            (params->height_max - params->height_min) * GSrandf();
        if (reverse) {
            /* RULE-EXCEPTION(user-approved): rotation_min staged in rotation (reused scalar) for allocation - see docs/RULE_EXCEPTIONS.md */
            rotation = params->rotation_min;
            rotation = (params->rotation_base - rotation) -
                       (params->rotation_max - rotation) * GSrandf();
        } else {
            /* RULE-EXCEPTION(user-approved): rotation_min staged in rotation (reused scalar) for allocation - see docs/RULE_EXCEPTIONS.md */
            rotation = params->rotation_min;
            rotation = (params->rotation_max - rotation) * GSrandf() +
                       (params->rotation_base + rotation);
        }

        cameraSetDistance(distance);
        cameraSetHeight(length);
        cameraSetRotY(rotation);
        cameraUpdate();
        GSscene_GetCameraDirectionVector(&direction);
        cameraMovePosition(7, &direction, (f32)duration / (f32)fn_800D37CC());

        length = sqrtf(distance * distance + length * length);
        params->out_far = length;
        params->out_mid = length;
        params->out_near = length;
        break;
    }

    case 4: {
        Vec direction;
        f32 rotation;
        f32 length;
        u8* durationPtr = (u8*)pattern + (*(u32*)pattern << 2);

        duration = *(s32*)(durationPtr + 4);
        if (duration == 0) {
            duration = *(s32*)(durationPtr + 8);
        }
        /* Retail does not scale this case's duration by shift. */

        if (reverse) {
            rotation = params->rotation_base - 0.47123894f;
        } else {
            rotation = 0.47123894f + params->rotation_base;
        }

        cameraSetDistance(110.0f);
        cameraSetHeight(25.0f);
        cameraSetRotY(rotation);
        cameraUpdate();
        GSscene_GetCameraDirectionVector(&direction);
        cameraMovePosition(7, &direction, (f32)duration / (f32)fn_800D37CC());

        length = sqrtf(110.0f * 110.0f + 25.0f * 25.0f);
        params->out_far = length;
        params->out_mid = length;
        params->out_near = length;
        break;
    }

    case 5: {
        Vec direction;
        f32 scale;
        f32 distance;
        f32 height;
        f32 rotation;
        f32 length;
        u8* durationPtr = (u8*)pattern + (*(u32*)pattern << 2);
        /* RULE-EXCEPTION(user-approved): mode declared after durationPtr for register allocation - see docs/RULE_EXCEPTIONS.md */
        s32 mode = *(s32*)(sequence + 0x10);

        duration = *(s32*)(durationPtr + 4);
        if (duration == 0) {
            duration = *(s32*)(durationPtr + 8);
        }
        duration <<= shift;

        switch (mode) {
        case -2:
        case -1:
            scale = 0.875f;
            break;
        case 1:
            scale = 1.4f;
            break;
        case 2:
            scale = 1.8f;
            break;
        case 3:
            scale = 3.0f;
            break;
        default:
            scale = 1.0f;
            break;
        }

        distance = 50.0f * scale;
        /* RULE-EXCEPTION(user-approved): height_min staged in the reused scalar rotation for allocation - see docs/RULE_EXCEPTIONS.md */
        rotation = params->height_min;
        height = rotation + (params->height_max - rotation) * GSrandf();
        if (reverse) {
            rotation = params->rotation_base - 0.7853982f;
        } else {
            rotation = 0.7853982f + params->rotation_base;
        }

        cameraSetDistance(distance);
        cameraSetHeight(height);
        cameraSetRotY(rotation);
        cameraUpdate();
        GSscene_GetCameraDirectionVector(&direction);
        cameraMovePosition(7, &direction, (f32)duration / (f32)fn_800D37CC());

        length = sqrtf(distance * distance + height * height);
        params->out_far = length;
        params->out_mid = length;
        params->out_near = length;
        break;
    }
    }
}

/**
 * _wazaSequenceCameraDoDollyPosition__FP21TemplateExpFileHeaderP24wazaSequenceCameraParamsfb - Move animation state machine B.
 * Address: 0x801D3F7C | Size: 0x548
 * State machine for beam/projectile move animations.
 */
void _wazaSequenceCameraDoDollyPosition__FP21TemplateExpFileHeaderP24wazaSequenceCameraParamsfb(
    void* header, void* params, s32 shift, u8 reverse)
{
    typedef struct WazaCameraParams {
        u8 pad_00[0x0C];
        f32 rotationBase;
        f32 rotationRange;
        f32 rotationOffset;
        f32 heightMin;
        f32 heightMax;
        f32 distanceMin;
        f32 distanceMax;
        f32 nearDistance;
        f32 middleDistance;
        f32 farDistance;
        f32 nearLength;
        f32 middleLength;
        f32 farLength;
    } WazaCameraParams;
    extern f32 GSrandf();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 _fadeEffectGetRandom__FUl(u32);
#endif
    extern void cameraSetDistance(f32);
    extern void cameraSetHeight(f32);
    extern void cameraSetRotY(f32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraUpdate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraDirectionVector(Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_GetCameraPositionVector(Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecSub(Vec*, Vec*, Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void vecScale(Vec*, Vec*, f32);
    extern s32 fn_800D37CC(void);
#endif
    extern void cameraMovePosition(s32, Vec*, f32);
    u8* file = header;
    WazaCameraParams* camera = params;
    Vec direction;
    Vec position;
    s32 randomDelay;
    s32 duration;
    s32 delay;
    f32 threshold0 = 0.5f;
    f32 threshold1 = 0.75f;
    f32 distance0;
    f32 distance1;
    f32 nearDistance;
    f32 farDistance;
    f32 height;
    f32 rotation;
    f32 chance;
    u8 alternate;

    duration = *(s32*)(file + (*(s32*)file << 2) + 4);
    if (duration == 0) {
        duration = *(s32*)(file + (*(s32*)file << 2) + 8);
    }
    duration <<= shift;

    if (GSrandf() <= 0.7f) {
        threshold0 = 0.3f;
        threshold1 = 0.65f;
        alternate = TRUE;
    } else {
        alternate = FALSE;
    }

    chance = GSrandf();
    if (chance < threshold0) {
        randomDelay = 0;
    } else if (chance < threshold1) {
        s32 fullDuration;

        randomDelay = 0;
        fullDuration = duration;
        duration = _fadeEffectGetRandom__FUl(duration);
        if (duration < (fullDuration >> 1)) {
            duration = fullDuration >> 1;
        }
    } else {
        randomDelay = _fadeEffectGetRandom__FUl(duration);
        if (randomDelay > (duration >> 1)) {
            randomDelay = duration >> 1;
        }
    }

    distance0 = camera->distanceMin +
        (camera->distanceMax - camera->distanceMin) * GSrandf();
    nearDistance = distance0;
    distance1 = camera->distanceMin +
        (camera->distanceMax - camera->distanceMin) * GSrandf();
    farDistance = distance1;
    /* RULE-EXCEPTION(user-approved): the height is staged in distance1 (reused scalar) as retail's f0 temporary - see docs/RULE_EXCEPTIONS.md */
    if (alternate) {
        if (distance0 < distance1) {
            farDistance = distance0;
            nearDistance = distance1;
        }
        distance1 = camera->heightMin +
            (camera->heightMax - camera->heightMin) * GSrandf();
        height = distance1;
    } else {
        if (distance0 > distance1) {
            farDistance = distance0;
            nearDistance = distance1;
        }
        distance1 = camera->heightMin +
            (camera->heightMax - camera->heightMin) * GSrandf();
        height = distance1;
    }

    if (reverse) {
        rotation = (camera->rotationOffset - camera->rotationBase) -
                   (camera->rotationRange - camera->rotationBase) * GSrandf();
    } else {
        rotation = (camera->rotationRange - camera->rotationBase) *
                       GSrandf() +
                   (camera->rotationOffset + camera->rotationBase);
    }

    cameraSetDistance(nearDistance);
    cameraSetHeight(height);
    cameraSetRotY(rotation);
    cameraUpdate();
    GSscene_GetCameraDirectionVector(&direction);
    GSscene_GetCameraPositionVector(&position);
    GSvecSub(&direction, &direction, &position);
    direction.y = 0.0f;
    vecScale(&direction, &direction, farDistance / nearDistance);
    direction.y = height;
    GSvecAdd(&direction, &direction, &position);
    delay = duration - randomDelay;
    cameraMovePosition(7, &direction, (f32)delay / (f32)fn_800D37CC());

    camera->nearDistance =
        sqrtf(nearDistance * nearDistance + height * height);
    distance0 = 0.5f * (nearDistance + farDistance);
    camera->middleDistance =
        sqrtf(distance0 * distance0 + height * height);
    camera->farDistance =
        sqrtf(farDistance * farDistance + height * height);
}

typedef struct WazaSequenceCameraParamsFov {
    s32 mode;
    f32 scaleMin;
    f32 scaleMax;
    u8 pad_0C[0x1C];
    f32 ranges[3];
} WazaSequenceCameraParamsFov;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 atan2(f64, f64);
#endif

/* RULE-EXCEPTION(user-approved): inline helper reconstructed for codegen; each fov is staged in its own output before the degree scaling - see docs/RULE_EXCEPTIONS.md */
static inline void _wazaSequenceCameraFovRange(
    WazaSequenceCameraParamsFov* params, f32 radius, f32* lowFov, f32* highFov)
{
    f32 span = 0.75f * params->scaleMin;
    if (params->scaleMax > span) {
        span = params->scaleMax;
    }
    *lowFov = (f32)atan2(0.5f * span, radius);
    *lowFov = 57.29578f * (2.0f * *lowFov);
    *highFov = (f32)atan2(2.0f * span, radius);
    *highFov = 57.29578f * (2.0f * *highFov);
}

/* RULE-EXCEPTION(user-approved): inline helper reconstructed for codegen; hasSecond is copied from hasFirst to keep retail's mr - see docs/RULE_EXCEPTIONS.md */
static inline void _wazaSequenceCameraFovMix(s32 choice, f32* mixStart, f32* mixEnd)
{
    u8 hasFirst;
    u8 hasSecond;

    hasFirst = FALSE;
    hasSecond = hasFirst;
    if ((choice & 1) == 1) {
        *mixStart = 0.0f;
        hasFirst = TRUE;
        *mixEnd = 0.2f;
    }
    if ((choice & 2) == 2) {
        if (!hasFirst) {
            *mixStart = 0.35f;
        }
        *mixEnd = 0.6f;
        hasSecond = TRUE;
    }
    if ((choice & 4) == 4) {
        if (!hasSecond) {
            *mixStart = 0.75f;
        }
        *mixEnd = 1.0f;
    }
}

/**
 * _wazaSequenceCameraDoFOV__FP13ModelSequenceP24wazaSequenceCameraParamsif - Move animation state machine C.
 * Address: 0x801D44C4 | Size: 0x514
 * State machine for status/field effect move animations.
 */
void _wazaSequenceCameraDoFOV__FP13ModelSequenceP24wazaSequenceCameraParamsif(
    void* modelSequence, void* cameraParams, s32 flags, s32 shift)
{
    typedef struct WazaSequenceCameraFovTiming {
        s32 count;
        u8 pad_04[8];
        s32 frames[3];
    } WazaSequenceCameraFovTiming;
    typedef struct WazaSequenceCameraFovPattern {
        s32 mode;
        s32 durationMode;
        f32 thresholds[4];
        u32 initialFlags;
        u32 flags;
        s32 timingMode;
    } WazaSequenceCameraFovPattern;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f64 atan2(f64, f64);
#endif
    extern f32 GSrandf(void);
    extern void battleCameraDisable(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraSetFov(f32);
#endif
    extern CameraFovKey lbl_804673D4[];
    extern f32 lbl_80478CDC;
    extern u32 lbl_8047B3E8;
    u8* sequence = modelSequence;
    /* RULE-EXCEPTION(user-approved): declaration order chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
    WazaSequenceCameraFovTiming* timing;
    s32 count;
    s32 prevFrame;
    s32 nextFrame;
    s32 i;
    WazaSequenceCameraFovPattern* pattern;
    s32 duration;
    s32 startFrame;
    s32 endFrame;
    s32 sequenceId;
    s32 choice;
    f32 mixStart;
    f32 mixEnd;
    f32 currentFov;
    f32 lowFov;
    f32 highFov;
    f32 fov;

    sequenceId = *(u16*)(sequence + 0x32);
    timing = (WazaSequenceCameraFovTiming*)
        (((u8*)PTR32_AT(sequence, 0x2C)) + sequenceId * 0xD4);
    count = timing->count;
    mixStart = 0.0f;
    mixEnd = 1.0f;
    _wazaSequenceCameraFovRange(cameraParams, ((WazaSequenceCameraParamsFov*)cameraParams)->ranges[0], &lowFov, &highFov);
    if (lowFov < 15.0f) {
        lowFov = 15.0f;
    }
    if (highFov < 15.0f) {
        highFov = 15.0f;
    }
    if (lowFov > 85.0f) {
        lowFov = 85.0f;
    }
    if (highFov > 85.0f) {
        highFov = 85.0f;
    }

    switch (((WazaSequenceCameraParamsFov*)cameraParams)->mode) {
    case 0:
    case 4:
    case 5:
        if (flags & 0x20) {
            choice = 1;
        } else if (flags & 0x80) {
            choice = 4;
        } else {
            choice = 2;
        }
        _wazaSequenceCameraFovMix(choice, &mixStart, &mixEnd);
        fov = mixStart + (mixEnd - mixStart) * GSrandf();
        fov = fov * (highFov - lowFov) + lowFov;
        lbl_804673D4[1].end = fov;
        lbl_804673D4[1].start = fov;
        lbl_804673D4[0].end = fov;
        lbl_804673D4[0].start = fov;
        lbl_80478CDC = fov;
        lbl_804673D4[1].endFrame = timing->frames[0] << shift;
        lbl_804673D4[1].startFrame = timing->frames[0] << shift;
        lbl_804673D4[0].endFrame = timing->frames[0] << shift;
        lbl_804673D4[0].startFrame = timing->frames[0] << shift;
        lbl_8047B3E8 = timing->frames[0] << shift;
        cameraSetFov(fov);
        return;
    }

    pattern = (WazaSequenceCameraFovPattern*)((u8*)wazaSequenceCameraGetPattern__Fbi(
        sequenceId != 8 && sequenceId != 9, flags) + 4);

    _wazaSequenceCameraFovMix(pattern->initialFlags, &mixStart, &mixEnd);
    fov = mixStart + (mixEnd - mixStart) * GSrandf();
    fov = fov * (highFov - lowFov) + lowFov;
    lbl_804673D4[1].end = fov;
    lbl_804673D4[1].start = fov;
    lbl_804673D4[0].end = fov;
    lbl_804673D4[0].start = fov;
    lbl_80478CDC = fov;
    lbl_804673D4[1].endFrame = timing->frames[0] << shift;
    lbl_804673D4[1].startFrame = timing->frames[0] << shift;
    lbl_804673D4[0].endFrame = timing->frames[0] << shift;
    lbl_804673D4[0].startFrame = timing->frames[0] << shift;
    lbl_8047B3E8 = timing->frames[0] << shift;
    cameraSetFov(fov);

    if (count <= 2) {
        return;
    }

    nextFrame = timing->frames[0];
    currentFov = fov;
    for (i = 0; i < 2; i++, pattern++) {
        prevFrame = nextFrame;
        nextFrame = timing->frames[i + 1];
        if (prevFrame == nextFrame) {
            lbl_804673D4[i].startFrame = prevFrame << shift;
            lbl_804673D4[i].endFrame = prevFrame << shift;
            lbl_804673D4[i].start = currentFov;
            lbl_804673D4[i].end = currentFov;
        } else switch (pattern->mode) {
        case 3:
        case 4: {
            duration = _wazaSequenceCameraSelectDuration__FUcPff(
                pattern->durationMode, pattern->thresholds, nextFrame - prevFrame);
            _wazaSequenceCameraFovRange(cameraParams,
                ((WazaSequenceCameraParamsFov*)cameraParams)->ranges[i + 1], &lowFov, &highFov);
            if (lowFov < 15.0f) {
                lowFov = 15.0f;
            }
            if (highFov < 15.0f) {
                highFov = 15.0f;
            }
            if (lowFov > 85.0f) {
                lowFov = 85.0f;
            }
            if (highFov > 85.0f) {
                highFov = 85.0f;
            }
            _wazaSequenceCameraFovMix(pattern->flags, &mixStart, &mixEnd);
            fov = mixStart + (mixEnd - mixStart) * GSrandf();
            fov = fov * (highFov - lowFov) + lowFov;
            if (pattern->timingMode == 2) {
                endFrame = nextFrame;
                startFrame = nextFrame - duration;
            } else {
                startFrame = prevFrame;
                endFrame = prevFrame + duration;
            }
            lbl_804673D4[i].startFrame = startFrame << shift;
            lbl_804673D4[i].endFrame = endFrame << shift;
            lbl_804673D4[i].start = currentFov;
            currentFov = fov;
            lbl_804673D4[i].end = fov;
            break;
        }
        /* RULE-EXCEPTION(user-approved): explicit labels for modes 0-2 shape retail's compare tree - see docs/RULE_EXCEPTIONS.md */
        case 0:
        case 1:
        case 2:
        default:
            lbl_804673D4[i].startFrame = prevFrame << shift;
            lbl_804673D4[i].endFrame = nextFrame << shift;
            lbl_804673D4[i].start = currentFov;
            lbl_804673D4[i].end = currentFov;
            break;
        }
    }
}

/**
 * _wazaSequenceCameraSelectDuration__FUcPff - Move animation state machine D.
 * Address: 0x801D49D8 | Size: 0x3C8
 * State machine for spread/multi-target move animations.
 */
s32 _wazaSequenceCameraSelectDuration__FUcPff(
    s32 mode, f32* thresholds, s32 duration)
{
    extern f32 GSrandf();
    f32 random;

    if (mode == 1) {
        return duration < 12 ? duration : 12;
    }
    if (mode == 2) {
        return duration < 20 ? duration : 20;
    }
    if (mode == 4) {
        return duration < 45 ? duration : 45;
    }
    if (mode == 8) {
        return duration;
    }
    if (mode == 3) {
        random = GSrandf();
        return random < thresholds[0]
                   ? (duration < 12 ? duration : 12)
                   : (duration < 20 ? duration : 20);
    }
    if (mode == 5) {
        random = GSrandf();
        return random < thresholds[0]
                   ? (duration < 12 ? duration : 12)
                   : (duration < 45 ? duration : 45);
    }
    if (mode == 9) {
        random = GSrandf();
        return random < thresholds[0]
                   ? (duration < 12 ? duration : 12)
                   : duration;
    }
    if (mode == 6) {
        random = GSrandf();
        return random < thresholds[1]
                   ? (duration < 20 ? duration : 20)
                   : (duration < 45 ? duration : 45);
    }
    if (mode == 10) {
        random = GSrandf();
        return random < thresholds[1]
                   ? (duration < 20 ? duration : 20)
                   : duration;
    }
    if (mode == 12) {
        random = GSrandf();
        return random < thresholds[2]
                   ? (duration < 45 ? duration : 45)
                   : duration;
    }
    if (mode == 7) {
        random = GSrandf();
        if (random < thresholds[0]) {
            return duration < 12 ? duration : 12;
        }
        if (random < thresholds[1]) {
            return duration < 20 ? duration : 20;
        }
        return duration < 45 ? duration : 45;
    }
    if (mode == 11) {
        random = GSrandf();
        if (random < thresholds[0]) {
            return duration < 12 ? duration : 12;
        }
        if (random < thresholds[1]) {
            return duration < 20 ? duration : 20;
        }
        return duration;
    }
    if (mode == 13) {
        random = GSrandf();
        if (random < thresholds[0]) {
            return duration < 12 ? duration : 12;
        }
        if (random < thresholds[2]) {
            return duration < 45 ? duration : 45;
        }
        return duration;
    }
    if (mode == 14) {
        random = GSrandf();
        if (random < thresholds[1]) {
            return duration < 12 ? duration : 12;
        }
        if (random < thresholds[2]) {
            return duration < 20 ? duration : 20;
        }
        return duration;
    }
    if (mode == 15) {
        random = GSrandf();
        if (random < thresholds[0]) {
            return duration < 12 ? duration : 12;
        }
        if (random < thresholds[1]) {
            return duration < 12 ? duration : 12;
        }
        if (random < thresholds[2]) {
            return duration < 20 ? duration : 20;
        }
        return duration;
    }
    return duration;
}

/**
 * _wazaSequenceCameraSelectMotion__FP13ModelSequenceP12WazaSequenceP24wazaSequenceCameraParams - Move animation helper: particle burst.
 * Address: 0x801D4DA0 | Size: 0x218
 */
void _wazaSequenceCameraSelectMotion__FP13ModelSequenceP12WazaSequenceP24wazaSequenceCameraParams(
    void* modelSequence, void* wazaSequence, void* cameraParams)
{
    extern f32 GSrandf(void);
    extern s32 gLastMotionType;
    u32 flags;
    u8 option0;
    u8 option1;
    u8 option2;
    u8 option3;
    s32 count = 0;
    s32* motion;
    f32 interval;

    motion = cameraParams;
    if (*(u16*)((u8*)modelSequence + 0x70) == 0x13A) {
        *motion = 4;
        return;
    }

    if (wazaSequence != NULL) {
        flags = *(u32*)((u8*)wazaSequence + 8);
        if ((flags & 1) != 0) {
            *motion = 5;
            return;
        }
        option0 = (u8)((flags >> 3) & 1);
        if (option0 != 0) {
            count++;
        }
        option1 = (u8)((flags >> 4) & 1);
        if (option1 != 0) {
            count++;
        }
        option2 = (u8)((flags >> 5) & 1);
        if (option2 != 0) {
            count++;
        }
        option3 = (u8)((flags >> 6) & 1);
        if (option3 != 0) {
            count++;
        }
    }

    if (count == 0) {
        count = 4;
        option0 = 1;
        option1 = 1;
        option2 = 1;
        option3 = 1;
    } else if (count == 1) {
        if (option0) {
            *motion = 3;
        }
        if (option1) {
            *motion = 0;
        }
        if (option2) {
            *motion = 1;
        }
        if (option3) {
            *motion = 2;
        }
        gLastMotionType = *motion;
        return;
    }

    interval = 1.0f / (f32)count;
    for (;;) {
        f32 random;
        f32 limit;

        random = GSrandf();
        limit = interval;
        if (option0) {
            if (limit > random) {
                *motion = 3;
                if (gLastMotionType != *motion) {
                    break;
                }
            }
            limit += interval;
        }
        if (option1) {
            if (limit > random) {
                *motion = 0;
                if (gLastMotionType != *motion) {
                    break;
                }
            }
            limit += interval;
        }
        if (option2) {
            if (limit > random) {
                *motion = 1;
                if (gLastMotionType != *motion) {
                    break;
                }
            }
            limit += interval;
        }
        if (option3 && limit > random) {
            *motion = 2;
            if (gLastMotionType != *motion) {
                break;
            }
        }
    }
#ifdef TARGET_PC
    {
        /* ORRE_BATTLE_LOG (observe only, 2026-10-28, port-gaps 27): each procedural shot's motion and the last
         * one it avoided, to compare with the console's gLastMotionType (PLAYREC_PEEK) */
        static int logCam = -1;
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        extern u32 VIGetRetraceCount(void);
        if (logCam < 0) {
            logCam = getenv("ORRE_BATTLE_LOG") != 0;
        }
        if (logCam) {
            OSReport("[battle] camera motion %d (last %d, options %d%d%d%d) at retrace %u\n", (int)*motion,
                     (int)gLastMotionType, option0, option1, option2, option3, (unsigned)VIGetRetraceCount());
        }
    }
#endif
    gLastMotionType = *motion;
}

/**
 * _wazaSequenceCameraCalculateParams__FP13ModelSequenceiP24wazaSequenceCameraParams - Move animation helper: model projectile.
 * Address: 0x801D4FB8 | Size: 0x370
 */
void _wazaSequenceCameraCalculateParams__FP13ModelSequenceiP24wazaSequenceCameraParams(
    void* modelSequence, s32 flags, void* cameraParams)
{
    typedef struct WazaCameraBound { /* a view of GSbound: minimum.y, maximum.y, size (game/gs_bound_offsets.h) */
        u8 pad_00[GS_BOUND_OFF_MIN + 4];
        f32 minY;
        u8 pad_18[GS_BOUND_OFF_MAX + 4 - (GS_BOUND_OFF_MIN + 8)];
        f32 maxY;
        u8 pad_24[GS_BOUND_OFF_SIZE - (GS_BOUND_OFF_MAX + 8)];
        Vec size;
    } WazaCameraBound;
    extern void* GSmodelGetBound(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelGetRotation(void*, Vec*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 vecMag(Vec*);
#endif
    u8* sequence = modelSequence;
    WazaSequenceCameraParams* params = cameraParams;
    void* model = PTR32_AT_T(void, sequence, 0x24);
    WazaCameraBound* bound = GSmodelGetBound(model);
    Vec* extent;
    Vec rotation;
    /* RULE-EXCEPTION(user-approved): declaration order chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
    f32 sizeScale;
    f32 lower;
    f32 upper;
    f32 distanceScale;
    s32 mode;

    GSmodelGetRotation(model, &rotation);
    params->rotationBase = rotation.y;
    mode = *(s32*)(sequence + 0x10);
    switch (mode) {
    case -2:
        sizeScale = 1.1f;
        distanceScale = 0.875f;
        break;
    case -1:
        sizeScale = 1.1f;
        distanceScale = 0.875f;
        break;
    case 1:
        sizeScale = 1.25f;
        distanceScale = 1.4f;
        break;
    case 2:
        sizeScale = 1.3f;
        distanceScale = 1.8f;
        break;
    case 3:
        sizeScale = 1.5f;
        distanceScale = 3.0f;
        break;
    default:
        sizeScale = 1.2f;
        distanceScale = 1.0f;
        break;
    }

    if (flags & 1) {
        params->rotationMin = 0.0f;
        params->rotationMax = 0.5237035f;
    } else if (flags & 2) {
        params->rotationMin = 0.31415927f;
        params->rotationMax = 0.62831855f;
    } else if (flags & 4) {
        if (mode > 0) {
            params->rotationMin = 0.31415927f;
            params->rotationMax = 0.62831855f;
        } else {
            params->rotationMin = 0.62831855f;
            params->rotationMax = 0.9424779f;
        }
    } else if (flags & 8) {
        if (mode > 0) {
            params->rotationMin = 0.62831855f;
            params->rotationMax = 0.9424779f;
        } else {
            params->rotationMin = 0.9424779f;
            params->rotationMax = 1.2566371f;
        }
    } else if (flags & 0x10) {
        params->rotationMin = 1.2566371f;
        params->rotationMax = 1.5707964f;
    } else if (mode > 0) {
        params->rotationMin = 0.31415927f;
        params->rotationMax = 0.9424779f;
    } else {
        params->rotationMin = 0.31415927f;
        params->rotationMax = 1.0995574f;
    }

    if (flags & 0x20) {
        lower = 6.0f;
        upper = 8.0f;
        params->distanceMin = 25.0f;
        params->distanceMax = 35.0f;
    } else if (flags & 0x40) {
        params->distanceMin = 35.0f;
        params->distanceMax = 50.0f;
        lower = 6.0f;
        upper = 11.0f;
    } else if (flags & 0x80) {
        params->distanceMin = 50.0f;
        params->distanceMax = 60.0f;
        lower = 6.0f;
        upper = 20.0f;
    } else {
        /* RULE-EXCEPTION(user-approved): chained assignment so distanceMin is stored from upper's register, as retail - see docs/RULE_EXCEPTIONS.md */
        params->distanceMin = upper = 20.0f;
        params->distanceMax = 60.0f;
        lower = 6.0f;
    }

    extent = &bound->size;
    params->distanceMin *= distanceScale;
    params->distanceMax *= distanceScale;
    /* RULE-EXCEPTION(user-approved): size and height read into block temporaries before the stores, so extent->y loads before the size store as in retail - see docs/RULE_EXCEPTIONS.md */
    {
        f32 size = 0.57735026f * vecMag(extent);
        f32 height = extent->y;
        params->size = size;
        params->height = height;
    }
    params->heightMin = bound->minY;
    params->heightMax = bound->maxY;

    if (params->heightMin < lower) {
        params->heightMin = lower;
        if (params->heightMax < lower) {
            params->heightMax = 1.5f * lower;
        }
    }
    if (params->heightMax > upper) {
        params->heightMax = upper;
        if (params->heightMin > upper) {
            params->heightMin = 0.8f * upper;
        }
    }
    params->size *= sizeScale;
    params->height *= sizeScale;
    if (params->distanceMin < 20.0f) {
        params->distanceMin = 20.0f;
    }
    if (params->distanceMax < 40.0f) {
        params->distanceMax = 40.0f;
    }
    if (params->distanceMin > 48.0f) {
        params->distanceMin = 48.0f;
    }
    if (params->distanceMax > 60.0f) {
        params->distanceMax = 60.0f;
    }
}
