/**
 * @file sequence.c
 * @brief sequence / modelSequence lineage: Colosseum-era precursor of XD's
 * ModelSequence + NullSequence classes.
 *
 * Split from the former game/battle/battle_waza.c CodeCandidate bucket
 * (0x801D1470-0x801DE698); see config/GC6E01/splits.txt for the exact
 * address range of this translation unit. Shared typedefs and cross-TU
 * forward declarations live in include/game/battle/battle_waza_types.h.
 */

/* NullSequence::GetWaza takes (u16, u16), as its mangled name says. */
#define BATTLE_WAZA_GETWAZA_U16
#include "game/battle/battle_waza_types.h"
#include "pc/disc_types.h" /* PKX files are big-endian disc data */

typedef struct SequenceResourcePair {
    u32 group;    /* 0x0 */
    u32 resource; /* 0x4 */
    u32 extra;    /* 0x8 */
} SequenceResourcePair;

typedef struct SequenceResourceRef {
    u32 group;    /* 0x0 */
    u32 resource; /* 0x4 */
} SequenceResourceRef;

typedef struct SequenceKindEntry {
    u32 key;                      /* 0x00 */
    SequenceResourceRef kinds[3]; /* 0x04 */
} SequenceKindEntry;

typedef struct SequenceVariantEntry {
    u32 group;                    /* 0x00 */
    u32 variant;                  /* 0x04 */
    SequenceResourceRef kinds[3]; /* 0x08 */
} SequenceVariantEntry;

static inline void sequenceWaitResource(u32 group, u32 resource) {
    extern void fn_8017B3E4(u32 group);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
    extern const char lbl_802799C8[];
    s32 state;

    if (fn_8017B2CC(group) < 0) {
        fn_8017B3E4(group);
    } else if (fn_8017B2CC(group) == 0 && resource != 0 &&
               GSresGetResource(group, resource) == NULL) {
        fn_8017B3E4(group);
    }

    for (;;) {
        state = fn_8017B2CC(group);
        if (state < 0) {
            GSlogWrite(lbl_802799C8);
        }
        if (state == 0) {
            break;
        }
        _threadSwitch();
    }
}

#if defined(SEQUENCE_801DCDA8_801DCF00)

/**
 * wazaSequenceOwnerFindEntry - Waza field effect get type.
 * Address: 0x801DCDA8 | Size: 0x24
 */
void* wazaSequenceOwnerFindEntry(void* obj, s32 fieldEffect) {
    void* cur = PTR32_AT_T(void, (u8*)obj, 0x24);

    while (cur != NULL) {
        if (*(s32*)cur == fieldEffect) {
            return cur;
        }
        cur = PTR32_AT_T(void, (u8*)cur, 0xA8);
    }

    return cur;
}

/**
 * wazaSequenceOwnerGetColorSwapActive - Waza field effect set type.
 * Address: 0x801DCDCC | Size: 0x40
 */
s32 wazaSequenceOwnerGetColorSwapActive(void* obj) {
    if (obj == NULL) {
        return 0;
    }

    if (*(u8*)((u8*)obj + 0x77) == 0) {
        return 0;
    }

    if (*(u8*)((u8*)obj + 0x4E) != 0) {
        return *(u8*)((u8*)obj + 0x4F);
    }

    return 0;
}

/**
 * wazaSequenceOwnerApplyShinyColors - Apply the owner's colour swap (+0x38..0x44) and modulation (+0x48) to its
 * model, once (+0x4F marks it applied, +0x4E enables it). wazaSequencePokemonCreate calls it for a shiny Pokemon
 * that has no separate shiny model; wazaSequenceOwnerRelease undoes it.
 * Address: 0x801DCE0C | Size: 0x9C
 */
#ifdef TARGET_PC
/* PC (A's finding, 2026-10-15): the old-style `()` declarations passed the model slot and the colour-channel indices
 * as 32-bit ints to parameters the callee reads as 64-bit pointers. The model comes from its 32-bit slot (WazaEffect
 * +0x24, GameCube layout), the four indices (0..3, checked at load) are widened, and the calls have real prototypes. */
void GSmodelEnableColorSwap(struct GSmodel* model, void* color0, void* color1, void* color2, void* color3);
void GSmodelEnableModulation(struct GSmodel* model, const void* modulation);

/* NAME-T3 fn_801DCE0C */
void wazaSequenceOwnerApplyShinyColors(void* obj) {
    struct GSmodel* model;

    if (obj != NULL && *(u8*)((u8*)obj + 0x4F) == 0 && *(u8*)((u8*)obj + 0x4E) != 0) {
        model = (struct GSmodel*)PTR32_AT(obj, 0x24);
        if (*(u8*)((u8*)obj + 0x4C) != 0) {
            GSmodelEnableColorSwap(model, (void*)(intptr_t)*(s32*)((u8*)obj + 0x38),
                                   (void*)(intptr_t)*(s32*)((u8*)obj + 0x3C),
                                   (void*)(intptr_t)*(s32*)((u8*)obj + 0x40),
                                   (void*)(intptr_t)*(s32*)((u8*)obj + 0x44));
        }
        if (*(u8*)((u8*)obj + 0x4D) != 0) {
            *(u8*)((u8*)obj + 0x4B) = 0xFF;
            GSmodelEnableModulation(model, (u8*)obj + 0x48);
        }
        *(u8*)((u8*)obj + 0x4F) = 1;
    }
}
#else
/* NAME-T3 fn_801DCE0C */
void wazaSequenceOwnerApplyShinyColors(void* obj) {
    extern void GSmodelEnableColorSwap();
    extern void GSmodelEnableModulation();

    s32 handle;

    if (obj != NULL && *(u8*)((u8*)obj + 0x4F) == 0 && *(u8*)((u8*)obj + 0x4E) != 0) {
        handle = *(s32*)((u8*)obj + 0x24);
        if (*(u8*)((u8*)obj + 0x4C) != 0) {
            GSmodelEnableColorSwap(handle, *(s32*)((u8*)obj + 0x38), *(s32*)((u8*)obj + 0x3C),
                        *(s32*)((u8*)obj + 0x40), *(s32*)((u8*)obj + 0x44));
        }
        if (*(u8*)((u8*)obj + 0x4D) != 0) {
            *(u8*)((u8*)obj + 0x4B) = 0xFF;
            GSmodelEnableModulation(handle, (u8*)obj + 0x48);
        }
        *(u8*)((u8*)obj + 0x4F) = 1;
    }
}
#endif

/**
 * wazaSequenceOwnerClearNull - Clear the root-null state (flag bit 2): remove the model's added null and play
 * idle. The pair of wazaSequenceOwnerSetNull.
 * Address: 0x801DCEA8 | Size: 0x58
 */
extern void wazaSequenceOwnerPlayIdle(void* obj, s32 arg1, s32 arg2);
/* NAME-T3 fn_801DCEA8 */
void wazaSequenceOwnerClearNull(void* obj) {
    WazaEffect* effect = obj;
    u8 flags = effect->flags;

    if ((flags & 2) == 2) {
        effect->flags = flags ^ 2;
        GSmodelRemoveNull(P32(effect->model));
        wazaSequenceOwnerPlayIdle(effect, 1, 0);
    }
}

#endif

#if defined(SEQUENCE_801DCF00_801DCF84)

/**
 * wazaSequenceOwnerSetNull - Waza lighting override set.
 * Address: 0x801DCF00 | Size: 0x84
 */
void wazaSequenceOwnerSetNull(u32 color, f32 intensity) {
    extern u8 GSmodelIsRootNullAdded(s32);
    extern void GSmodelGetRootPosition(s32, void*);
    extern void GSmodelAddNull(s32, void*, s32, s32);
    extern void wazaSequenceOwnerPlayIdle(void*, s32, s32);

    void* obj;
    u8 flags;

    obj = (void*)color;
    flags = *(u8*)((u8*)obj + 0x18);
    if ((flags & 2) != 2) {
        *(u8*)((u8*)obj + 0x18) = flags | 2;
        if (GSmodelIsRootNullAdded(*(s32*)((u8*)obj + 0x24)) != 0) {
            GSmodelGetRootPosition(*(s32*)((u8*)obj + 0x24), (u8*)obj + 0x5C);
        } else {
            GSmodelAddNull(*(s32*)((u8*)obj + 0x24), (u8*)obj + 0x5C, 0, 0);
        }
        wazaSequenceOwnerPlayIdle(obj, 1, 0);
    }
}

#endif

#if defined(SEQUENCE_801DCF84_801DD158)

/**
 * wazaSequenceOwnerClearStop - Waza lighting override clear.
 * Address: 0x801DCF84 | Size: 0x54
 */
void wazaSequenceOwnerClearStop(void* obj) {
    u8 flags = *(u8*)((u8*)obj + 0x18);

    if ((flags & 8) == 8) {
        *(u8*)((u8*)obj + 0x18) = flags ^ 8;
        wazaSequenceOwnerPlayIdle(obj, 1, 1);
        wazaSequenceOwnerDeleteStopParticle(obj);
    }
}

/**
 * wazaSequenceOwnerSetStop - Set the stop state (flag bit 8): stop the model's animation and create the stop
 * particle. The pair of wazaSequenceOwnerClearStop.
 * Address: 0x801DCFD8 | Size: 0x50
 */
/* NAME-T3 fn_801DCFD8 */
void wazaSequenceOwnerSetStop(void* obj) {
    WazaEffect* effect = obj;
    u8 flags = effect->flags;

    if ((flags & 8) != 8) {
        effect->flags = flags | 8;
        GSmodelStopAnimation(P32(effect->model));
        wazaSequenceOwnerCreateStopParticle(effect);
    }
}

/**
 * wazaSequenceOwnerClearAura - Waza lighting ambient set.
 * Address: 0x801DD028 | Size: 0x50
 */
extern void wazaSequenceOwnerTexAnimOff(void* obj);
void wazaSequenceOwnerClearAura(void* obj) {
    u8 flags = *(u8*)((u8*)obj + 0x18);

    if ((flags & 4) == 4) {
        wazaSequenceOwnerTexAnimOff(obj);
        *(u8*)((u8*)obj + 0x18) = *(u8*)((u8*)obj + 0x18) ^ 4;
        wazaSequenceOwnerDeleteAuraParticle(obj);
    }
}

/**
 * wazaSequenceOwnerSetAura - Waza lighting ambient get.
 * Address: 0x801DD078 | Size: 0x50
 */
extern void wazaSequenceOwnerTexAnimOn(void* obj);
void wazaSequenceOwnerSetAura(void* obj) {
    u8 flags = *(u8*)((u8*)obj + 0x18);

    if ((flags & 4) != 4) {
        wazaSequenceOwnerTexAnimOn(obj);
        *(u8*)((u8*)obj + 0x18) = *(u8*)((u8*)obj + 0x18) | 4;
        wazaSequenceOwnerCreateAuraParticle(obj);
    }
}


/**
 * GetWaza__12NullSequenceCFUsUs - Waza lighting reset.
 * Address: 0x801DD0C8 | Size: 0x38
 */
void* GetWaza__12NullSequenceCFUsUs(void* obj, u16 search_key1, u16 search_key2)
{
    WazaFxNode* cur = P32(((WazaFxOwner*)obj)->first_child);

    while (cur != NULL) {
        if (cur->field_2C == search_key1 && cur->field_2E == search_key2) {
            return cur;
        }
        cur = P32(cur->next);
    }

    return cur;
}

/**
 * wazaSequenceOwnerSetAnim - 0x801DD100 | Size: 0x58
 * Two-arg (owner, obj) per the caller in wazaSequence.c; the prior
 * (u32 filterColor) signature was a placeholder.
 */
void wazaSequenceOwnerSetAnim(WazaSequenceOwner* owner, WazaSequence* sequence) {
    if (owner == NULL) return;
    if (sequence == NULL) {
        owner->index = 0;
        owner->field_34 = 0;
        P32(owner->table)[0].field_90 = 0;
    } else {
        WazaEffectTblEntry* tblEntry;
        owner->index = (u8)sequence->kind;
        owner->field_34 = 0;
        tblEntry = &P32(owner->table)[sequence->kind];
        tblEntry->field_90 = sequence->field_10;
    }
}

#endif

#if defined(SEQUENCE_801DD158_801DD23C)

/**
 * wazaSequenceOwnerUpdate - Per-frame owner update (from wazaSequenceSysUpdate): apply the visibility flag to
 * the model, run wazaSequenceUpdate on each active sequence, and stop (or stop and free) the finished ones.
 * Address: 0x801DD158 | Size: 0xE4
 */
/* NAME-T3 fn_801DD158 */
void wazaSequenceOwnerUpdate(void* obj) {
    WazaEffect* effect = obj;
    WazaSequence* sequence;

    if (P32(effect->model) != NULL) {
        if ((effect->flags & 1) == 1) {
            GSmodelSetVisibility(P32(effect->model), 1);
        } else {
            GSmodelSetVisibility(P32(effect->model), 0);
        }
    }

    sequence = P32(effect->sequenceList);
    while (sequence != NULL) {
        WazaSequence* next = PTR32_AT_T(WazaSequence, (u8*)sequence, 0x34);

        if (sequence->active != 0) {
            s32 result;

            if ((s8)sequence->stopping == -1) {
                wazaSequenceApplyStop(sequence);
            } else {
                result = wazaSequenceUpdate(sequence);
                if ((s8)result == 0) {
                    if ((s8)sequence->stopping != -1) {
                        wazaSequenceApplyStop(sequence);
                    }
                } else if ((s8)result < 0) {
                    wazaSequenceApplyStop(sequence);
                    wazaSequenceFree(sequence);
                }
            }
        }
        sequence = next;
    }
}

#endif

#if defined(SEQUENCE_801DD23C_801DD45C)

/**
 * wazaSequenceOwnerRelease - Waza color filter transition.
 * Address: 0x801DD23C | Size: 0x1A8
 */
void wazaSequenceOwnerRelease(void* obj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E24B0(u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmemFree(u16);
#endif
    extern void GSmodelDisableColorSwap(u32);
    extern void GSmodelDisableModulation(u32);
    extern void GSmodelSetAnimEndedCallback();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerSetVisibility();
#endif
    extern void gsParticleKillAll(s32);
    extern void fn_800F9210();
    extern void pachiruEffectRelease(void*);
    extern void wazaSequenceSysFreeSequenceResource(void* obj);
    extern void wazaSequenceOwnerDeleteAuraParticle(void* obj);
    extern void wazaSequenceOwnerDeleteStopParticle(void* obj);

    u8* data;
    u16 id;
    u8 enabled;
    u32 handle;

    data = (u8*)obj;
    if (data != NULL) {
        id = *(u16*)(data + 0x30);
        if (id != 0) {
            fn_800E24B0(id);
            GSmemFree(id);
        }

        if (data == NULL) {
            enabled = 0;
        } else if (*(u8*)(data + 0x77) == 0) {
            enabled = 0;
        } else if (*(u8*)(data + 0x4E) == 0) {
            enabled = 0;
        } else {
            enabled = *(u8*)(data + 0x4F);
        }

        if (enabled != 0 && data != NULL && *(u8*)(data + 0x4F) != 0 && *(u8*)(data + 0x4E) != 0) {
            handle = *(u32*)(data + 0x24);
            if (*(u8*)(data + 0x4C) != 0) {
                GSmodelDisableColorSwap(handle);
            }
            if (*(u8*)(data + 0x4D) != 0) {
                GSmodelDisableModulation(handle);
            }
            *(u8*)(data + 0x4F) = 0;
        }

        if (*(u32*)(data + 0x24) != 0) {
            GSmodelSetAnimEndedCallback(*(u32*)(data + 0x24), 0, 0);
        }

        wazaSequenceOwnerSetVisibility(data, 0);

        if (*(u32*)(data + 0x0C) != 0) {
            gsParticleKillAll(*(s32*)(data + 0x28));
            fn_800F9210(*(u32*)data, *(u32*)(data + 0x0C));
        }

        pachiruEffectRelease(data + 0x50);

        if (*(u32*)data != 0) {
            if (*(u32*)(data + 4) != 0) {
                fn_800F9210(*(u32*)data, *(u32*)(data + 4));
            }
            if (*(u32*)(data + 8) != 0) {
                fn_800F9210(*(u32*)data, *(u32*)(data + 8));
            }
            id = *(u16*)(data + 0x7C);
            if (id != 0) {
                fn_800E24B0(id);
                GSmemFree(id);
            }
        }

        wazaSequenceSysFreeSequenceResource(data);
        wazaSequenceOwnerDeleteAuraParticle(data);
        wazaSequenceOwnerDeleteStopParticle(data);
        memset(data, 0, 0x8C);
    }
}

/**
 * wazaSequenceOwnerFreeSequences - Stop and free every sequence of the owner and empty its list (+0x68).
 * Address: 0x801DD3E4 | Size: 0x78
 */
/* NAME-T3 fn_801DD3E4 */
void wazaSequenceOwnerFreeSequences(void* obj) {
    extern void wazaSequenceApplyStop(void* obj);
    extern void wazaSequenceFree(void* obj);

    void* cur;
    void* next;

    if (obj != NULL) {
        cur = PTR32_AT_T(void, (u8*)obj, 0x68);
        while (cur != NULL) {
            next = PTR32_AT_T(void, (u8*)cur, 0x34);
            if (*(u8*)((u8*)cur + 0x14) != 0) {
                wazaSequenceApplyStop(cur);
            }
            wazaSequenceFree(cur);
            cur = next;
        }
        PTR32_AT_SET_T(void, (u8*)obj, 0x68, NULL);
    }
}

#endif

#if defined(SEQUENCE_801DD45C_801DE164)

/**
 * sequenceLoad - Waza scene snapshot.
 * Address: 0x801DD45C | Size: 0x18C
 */
BOOL sequenceLoad(void* effect, void* data) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSequenceOwnerFindAnim(void*);
#endif
    extern void fn_800EB268(void*, u32);
    extern void* gsParticleBankLoad(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkTexAnimToAnim(void*, s32);
#endif
    extern void GSmodelSetAnimEndedCallback(void*, void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void sequenceAnimEndCallback(void);
#endif
    extern void wazaSequenceOwnerPlayIdle(void*, s32, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceOwnerSetVisibility(void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetPosition(void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetRotation(void*, void*);
#endif
    extern void GSmodelSetScale(void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaSequenceSysGetModelShadowLight__Fv(void);
#endif
    extern s32 wazaSequenceSysGetModelShadowCount__Fv(void);
    extern void* wazaSequenceSysGetModelShadowList__Fv(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(void*, s32, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetBoundCheck(void*, s32);
#endif
    extern void fn_800E3B44(void*, s32);
    extern void GSlogWrite(const char*, ...);
    extern u8 lbl_803727B0[];
    extern u8 lbl_803727BC[];
    extern const char lbl_80279998[];
    void* model;
    u8* sequence = effect;

    if (sequence == NULL) {
        return FALSE;
    }
    if (wazaSequenceOwnerLoadResource(sequence, data)) {
#ifdef TARGET_PC /* the model slot is 32-bit (GameCube layout): not an lvalue on the PC */
        model = GSresGetResource(*(u32*)(sequence + 0), *(u32*)(sequence + 4));
        PTR32_AT_SET_T(void, sequence, 0x24, model);
#else
        model = PTR32_AT_T(void, sequence, 0x24) =
            GSresGetResource(*(u32*)(sequence + 0), *(u32*)(sequence + 4));
#endif
        fn_800EB268(model, wazaSequenceOwnerFindAnim(sequence));
        if (*(u32*)(sequence + 0xC) != 0) {
            PTR32_AT_SET_T(void, sequence, 0x28,
                gsParticleBankLoad(GSresGetResource(
                    *(u32*)(sequence + 0), *(u32*)(sequence + 0xC))));
            GSmodelLinkToGSparticleBank(model, PTR32_AT_T(void, sequence, 0x28));
        }
        GSmodelLinkTexAnimToAnim(model, 1);
        GSmodelSetAnimEndedCallback(model, sequenceAnimEndCallback, sequence);
        wazaSequenceOwnerPlayIdle(sequence, 1, 1);
        wazaSequenceOwnerSetVisibility(sequence, 0);
        GSmodelSetPosition(model, lbl_803727B0);
        GSmodelSetRotation(model, lbl_803727B0);
        GSmodelSetScale(model, lbl_803727BC);
        if (wazaSequenceSysGetModelShadowLight__Fv() != 0 &&
            wazaSequenceSysGetModelShadowCount__Fv() != 0) {
            GSmodelSetShadowFlags(model, 1);
            GSmodelSetShadowLight(
                model, wazaSequenceSysGetModelShadowLight__Fv());
            GSmodelSetShadowSurface(
                model, wazaSequenceSysGetModelShadowCount__Fv(),
                wazaSequenceSysGetModelShadowList__Fv());
            GSmodelSetBoundCheck(model, 1);
            fn_800E3B44(model, 1);
        }
        return TRUE;
    }
    GSlogWrite(lbl_80279998);
    return FALSE;
}

typedef struct SequenceEntryBlock {
    u32 words[16]; /* 0x4C-0x8B */
} SequenceEntryBlock;
typedef struct SequenceEntryTrack {
    s32 kind;  /* 0x0 */
    s32 value; /* 0x4 */
} SequenceEntryTrack;
#ifdef TARGET_PC
/* The PKX slot record as it is in the file: big-endian on the PC (nested types need their own disc twins: GCC does not
 * carry the storage order into separately defined nested structs). */
typedef struct DISC_STRUCT SequenceEntryBlockDisc { u32 words[16]; } SequenceEntryBlockDisc;
typedef struct DISC_STRUCT SequenceEntryTrackDisc { s32 kind; s32 value; } SequenceEntryTrackDisc;
typedef struct DISC_STRUCT SequenceEntrySrc {
    s32 countA;                       /* 0x00 */
    s32 countB;                       /* 0x04 */
    u32 field08;                      /* 0x08 */
    s32 values[16];                   /* 0x0C */
    SequenceEntryBlockDisc block;     /* 0x4C */
    SequenceEntryTrackDisc tracks[8]; /* 0x8C */
} SequenceEntrySrc;
#else
typedef struct SequenceEntrySrc {
    s32 countA;                  /* 0x00 */
    s32 countB;                  /* 0x04 */
    u32 field08;                 /* 0x08 */
    s32 values[16];              /* 0x0C */
    SequenceEntryBlock block;    /* 0x4C */
    SequenceEntryTrack tracks[8]; /* 0x8C */
} SequenceEntrySrc;
#endif
typedef struct SequenceEntryDst {
    s32 countA;                  /* 0x00 */
    s32 countB;                  /* 0x04 */
    u32 field08;                 /* 0x08 */
    s32 values[16];              /* 0x0C */
    SequenceEntryBlock block;    /* 0x4C */
    SequenceEntryTrack tracks[9]; /* 0x8C */
} SequenceEntryDst;

static inline s32 sequenceCopyValueCount(SequenceEntryDst* dst,
                                         SequenceEntrySrc* src) {
    s32 n;

    n = src->countA;
    dst->countA = n;
    return n;
}

/**
 * wazaSequenceOwnerLoadResource - Load an owner's model package from memory: the header, then the model,
 * its animations and particles, each under a fresh resource id. Returns FALSE for an empty package.
 * Address: 0x801DD5E8 | Size: 0x564
 */
/* NAME-T3 fn_801DD5E8 */
u8 wazaSequenceOwnerLoadResource(void* effect, u8* resource) {
    typedef struct DISC_STRUCT SequenceLoadResourceHeader { /* the PKX header: big-endian on the PC */
        s32 mainSize;      /* 0x00 */
        s32 auxSize;       /* 0x04 */
        u32 entryCount;    /* 0x08 */
        u32 sequenceKind;  /* 0x0C */
        s32 loadMode;      /* 0x10 */
        u32 flag14;        /* 0x14 */
        u32 value18;       /* 0x18 */
        u32 value1C;       /* 0x1C */
        u32 value20;       /* 0x20 */
    } SequenceLoadResourceHeader;

    extern u16 GSmemAlloc(u32 size, u32 alignment);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmemLock(u16 handle);
#endif
    extern void DCFlushRange(void* addr, s32 len);
    extern int wazaSequenceSysGetResID(void);
    extern void loadParticle(void* resource, u32 size, u32 group, u32 handle);
    extern void loadModel(u32 model, u32 group, u32 resource, u32 handle);
    extern void loadArchivePtr(void* resource, u32 size, u32 group, u32 handle);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
    extern const f32 lbl_8047E3B8;

    u32 animResId;
    u16 count;
    u32 modelResId;
    SequenceLoadResourceHeader* header;
    u16 i;
    SequenceEntrySrc* src;
    SequenceEntryDst* dst;
    s32 n;
    u8* sequence;
    SequenceEntryTrack* track;
    s32 j;
    s32 trim;
    u32 auxResId;
    u8* workBase;
    u16 handle;

    sequence = effect;
    modelResId = wazaSequenceSysGetResID();
    animResId = wazaSequenceSysGetResID();
    header = (SequenceLoadResourceHeader*)resource;
    if (header->mainSize == 0) {
        return FALSE;
    }

    *(s16*)(sequence + 0x1A) = -1;
    *(s16*)(sequence + 0x1C) = -1;
    *(s16*)(sequence + 0x1E) = -1;
    *(u8*)(sequence + 0x4E) = 0;

    switch (header->loadMode) {
    case 1:
    case 2:
        trim = -0x10;
        break;
    case 3:
        trim = -0xC;
        *(u8*)(sequence + 0x4E) = header->flag14 != 0;
        break;
    case 4:
        trim = -0xC;
        *(u8*)(sequence + 0x4E) = header->flag14 >> 31;
        break;
    case 5:
    default:
        trim = 0;
        *(u8*)(sequence + 0x4E) = header->flag14 >> 31;
        *(s16*)(sequence + 0x1A) = header->value18;
        *(s16*)(sequence + 0x1C) = header->value20;
        *(s16*)(sequence + 0x1E) = header->value1C;
        break;
    }

    *(u32*)(sequence + 0x0) = 0x4E20;
    *(u32*)(sequence + 0x10) = header->sequenceKind;
    resource += (sizeof(SequenceLoadResourceHeader) + trim + 0x1F) & ~0x1F;
    *(u16*)(sequence + 0x32) = 0;
    *(u16*)(sequence + 0x34) = 0;
    *(u16*)(sequence + 0x14) = count = header->entryCount;
    *(u16*)(sequence + 0x7C) = 0;

    if ((*(u16*)(sequence + 0x70) == 0x18) ||
        (*(u16*)(sequence + 0x70) == 0x4A) ||
        (*(u16*)(sequence + 0x70) == 0x134)) {
        *(u16*)(sequence + 0x7C) =
            GSmemAlloc((header->mainSize + 0x1F) & ~0x1F, 0x20);
        if (*(u16*)(sequence + 0x7C) == 0) {
            return FALSE;
        }
        workBase = GSmemLock(*(u16*)(sequence + 0x7C));
        memcpy(workBase, resource, (header->mainSize + 0x1F) & ~0x1F);
        DCFlushRange(workBase, (header->mainSize + 0x1F) & ~0x1F);
        loadArchivePtr(workBase, header->mainSize, 0x4E20, modelResId);
    } else {
        loadArchivePtr(resource, header->mainSize, 0x4E20, modelResId);
    }

    if (GSresGetResource(0x4E20, modelResId) == NULL) {
        return FALSE;
    }
    *(u32*)(sequence + 0x8) = modelResId;

    loadModel(PC_PTR32(GSresGetResource(0x4E20, modelResId)), 0x4E20, 0,
                animResId);
    if (GSresGetResource(0x4E20, animResId) == NULL) {
        return FALSE;
    }
    *(u32*)(sequence + 0x4) = animResId;

    resource += (header->mainSize + 0x1F) & ~0x1F;
    if (header->auxSize != 0) {
        auxResId = wazaSequenceSysGetResID();
        loadParticle(resource, header->auxSize, 0x4E20, auxResId);
        if (GSresGetResource(0x4E20, auxResId) != NULL) {
            *(u32*)(sequence + 0xC) = auxResId;
        } else {
            *(u32*)(sequence + 0xC) = 0;
        }
        resource += (header->auxSize + 0x1F) & ~0x1F;
    }

    handle = _toolentryAlloc__FUl(count * 0xD4);
    *(u16*)(sequence + 0x30) = handle;
    if (handle != 0) {
        src = (SequenceEntrySrc*)resource;
#ifdef TARGET_PC
        /* the slot-table pointer is a 32-bit slot (the 8-byte store overwrote the handle at +0x30) */
        dst = (SequenceEntryDst*)GSmemLock(handle);
        PTR32_AT_SET(sequence, 0x2C, dst);
#else
        dst = (SequenceEntryDst*)(PTR32_AT_T(u8, sequence, 0x2C) = GSmemLock(handle));
#endif
        for (i = 0; i < count; i++) {
            n = sequenceCopyValueCount(dst, src);
            for (j = 0; j < n; j++) {
                dst->values[j] = (f32)src->values[j] * (f32)fn_800D37CC() /
                                 lbl_8047E3B8;
            }
            dst->field08 = src->field08;
            dst->tracks[0].kind = 1;
            dst->tracks[0].value = 0;
            n = src->countB;
            dst->countB = n + 1;
            track = &dst->tracks[1];
            for (j = 0; j < n; j++, track++) {
                track->kind = src->tracks[j].kind;
                track->value = src->tracks[j].value;
                if (track->kind == 1) {
                    track->value = 1;
                }
            }
            dst->block.words[0] = src->block.words[0];
            dst->block.words[1] = src->block.words[1];
            dst->block.words[2] = src->block.words[2];
            dst->block.words[3] = src->block.words[3];
            dst->block.words[4] = src->block.words[4];
            dst->block.words[5] = src->block.words[5];
            dst->block.words[6] = src->block.words[6];
            dst->block.words[7] = src->block.words[7];
            dst->block.words[8] = src->block.words[8];
            dst->block.words[9] = src->block.words[9];
            dst->block.words[10] = src->block.words[10];
            dst->block.words[11] = src->block.words[11];
            dst->block.words[12] = src->block.words[12];
            dst->block.words[13] = src->block.words[13];
            dst->block.words[14] = src->block.words[14];
            dst->block.words[15] = src->block.words[15];
            src = (SequenceEntrySrc*)((u8*)src + 0xD0);
            dst = (SequenceEntryDst*)((u8*)dst + 0xD4);
        }

        if (header->loadMode < 4) {
            resource += count * 0xD0;
        } else {
            resource += (count * 0xD0 + 0x1F) & ~0x1F;
        }

        if (*(u8*)(sequence + 0x4E) != 0) {
            *(s32*)(sequence + 0x38) = DISC_S32(resource + 0x0);
            *(s32*)(sequence + 0x3C) = DISC_S32(resource + 0x4);
            *(s32*)(sequence + 0x40) = DISC_S32(resource + 0x8);
            *(s32*)(sequence + 0x44) = DISC_S32(resource + 0xC);
            if ((*(s32*)(sequence + 0x38) != 0) ||
                (*(s32*)(sequence + 0x3C) != 1) ||
                (*(s32*)(sequence + 0x40) != 2) ||
                (*(s32*)(sequence + 0x44) != 3)) {
                *(u8*)(sequence + 0x4C) = 1;
            }
            *(u8*)(sequence + 0x4B) = DISC_S32(resource + 0x10) >> 24;
            *(u8*)(sequence + 0x4A) = DISC_U32(resource + 0x10) >> 16;
            *(u8*)(sequence + 0x49) = DISC_U32(resource + 0x10) >> 8;
            *(u8*)(sequence + 0x48) = DISC_S32(resource + 0x10);
            if ((*(u8*)(sequence + 0x48) != 0x7F) ||
                (*(u8*)(sequence + 0x49) != 0x7F) ||
                (*(u8*)(sequence + 0x4A) != 0x7F) ||
                (*(u8*)(sequence + 0x4B) != 0x7F)) {
                *(u8*)(sequence + 0x4D) = 1;
            }
        }
        return TRUE;
    }
    PTR32_AT_SET(sequence, 0x2C, NULL);
    return FALSE;
}

/**
 * wazaSequenceOwnerAddSequence - Allocate a sequence, load it from a resource already in memory (move id 0) and
 * link it at the head of the owner's sequence list.
 * Address: 0x801DDB4C | Size: 0xC4
 */
/* NAME-T3 fn_801DDB4C */
BOOL wazaSequenceOwnerAddSequence(void* owner, void* resource) {
    u8* sequence;

    if (resource == NULL) {
        return FALSE;
    }
    sequence = (u8*)wazaSequenceAllocNode();
    if (sequence == NULL) {
        return FALSE;
    }
    *(u16*)(sequence + 0x2C) = 0;
    *(u16*)(sequence + 0x2E) = 0;
    *(u16*)(sequence + 0x30) = 0;
    PTR32_AT_SET_T(void, sequence, 0x3C, owner);
    if (!wazaSequenceLoadData(sequence, resource)) {
        wazaSequenceFree(sequence);
        return FALSE;
    }
    sequence[0x14] = 0;
    sequence[0x15] = 0;
    PTR32_AT_SET_T(u8, sequence, 0x34, PTR32_AT_T(u8, (u8*)owner, 0x68));
    if (PTR32_AT_T(u8, (u8*)owner, 0x68) != NULL) {
        PTR32_AT_SET_T(u8, PTR32_AT_T(u8, (u8*)owner, 0x68), 0x38, sequence);
    }
    PTR32_AT_SET_T(void, sequence, 0x38, NULL);
    PTR32_AT_SET_T(u8, (u8*)owner, 0x68, sequence);
    return TRUE;
}

/**
 * wazaSequenceCheckResExists - Waza transition effect helper B.
 * Address: 0x801DDC10 | Size: 0x118
 */
s32 wazaSequenceCheckResExists(u16 index, u16 type) {
    extern u32 lbl_80478CE0;
    extern u32 lbl_80478CC0;
    extern u32 lbl_80478CE8;
    extern SequenceResourcePair lbl_803727C8[];
    extern SequenceKindEntry lbl_8036E150[];
    extern SequenceVariantEntry lbl_80373210[];
    u32 i;
    u32 group;
    u32 resource;
    u32 count = 0;

    if (type == 0) {
        return 0;
    }
    if (type == 4) {
        if (index == 0 || index >= lbl_80478CE0) {
            return 0;
        }
        group = lbl_803727C8[index].group;
        resource = lbl_803727C8[index].resource;
        if (group != 0 && resource != 0) {
            count = 1;
        }
    } else {
        u16 kind = type - 1;
        SequenceVariantEntry* entry = lbl_80373210;
        if (index == 0 || index >= lbl_80478CC0) {
            return 0;
        }
        group = lbl_8036E150[index].kinds[kind].group;
        resource = lbl_8036E150[index].kinds[kind].resource;
        if (group != 0 && resource != 0) {
            count = 1;
        }
        for (i = 0; i < lbl_80478CE8; i++, entry++) {
            if (entry->group == index) {
                group = entry->kinds[kind].group;
                resource = entry->kinds[kind].resource;
                if (group != 0 && resource != 0) {
                    count++;
                }
            }
        }
    }
    return count;
}

/**
 * wazaSequenceOwnerLoadWaza - Load the sequence for (move, kind, variant) onto an owner: look up its resource
 * (wazaSequenceGetWazaResource), wait for it to load, then allocate, load and link a new sequence. TRUE if the
 * owner already has it.
 * Address: 0x801DDD28 | Size: 0x1BC
 */
/* NAME-T3 fn_801DDD28 */
BOOL wazaSequenceOwnerLoadWaza(void* owner, u16 group, u16 index, u8 variant) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
    extern void fn_8017B3E4(u32 group);
    extern const char lbl_802799C8[];
    u32 resourceGroup;
    u32 resourceId;
    u8* sequence;
    void* resource;

    if (owner == NULL) {
        return FALSE;
    }
    if (index == 0) {
        return FALSE;
    }
    wazaSequenceGetWazaResource(owner, group, index, variant, &resourceGroup, &resourceId);
    if (resourceGroup == 0 || resourceId == 0) {
        return FALSE;
    }
    if (GetWaza__12NullSequenceCFUsUs(owner, group, index) != NULL) {
        return TRUE;
    }

    sequenceWaitResource(resourceGroup, resourceId);

    resource = GSresGetResource(resourceGroup, resourceId);
    if (resource == NULL) {
        return FALSE;
    }
    sequence = (u8*)wazaSequenceAllocNode();
    if (sequence == NULL) {
        return FALSE;
    }
    *(u16*)(sequence + 0x2C) = group;
    *(u16*)(sequence + 0x2E) = index;
    *(u16*)(sequence + 0x30) = (u16)resourceGroup;
    PTR32_AT_SET_T(void, sequence, 0x3C, owner);
    if (!wazaSequenceLoadData(sequence, resource)) {
        wazaSequenceFree(sequence);
        return FALSE;
    }
    sequence[0x14] = 0;
    sequence[0x15] = 0;
    PTR32_AT_SET_T(u8, sequence, 0x34, PTR32_AT_T(u8, (u8*)owner, 0x68));
    if (PTR32_AT_T(u8, (u8*)owner, 0x68) != NULL) {
        PTR32_AT_SET_T(u8, PTR32_AT_T(u8, (u8*)owner, 0x68), 0x38, sequence);
    }
    PTR32_AT_SET_T(void, sequence, 0x38, NULL);
    PTR32_AT_SET_T(u8, (u8*)owner, 0x68, sequence);
    return TRUE;
}

/**
 * wazaSequenceGetWazaResource - Return the archive group and resource id of a move's sequence from the sequence
 * tables (the same tables wazaSequenceCheckResExists reads).
 * Address: 0x801DDEE4 | Size: 0x280
 */
/* NAME-T3 fn_801DDEE4 */
void wazaSequenceGetWazaResource(void* owner, u16 group, u16 type, u8 variant,
                 u32* resourceGroup, u32* resourceId) {
    extern u32 lbl_80478CE0;
    extern u32 lbl_80478CC0;
    extern u32 lbl_80478CE8;
    extern SequenceResourcePair lbl_803727C8[];
    extern SequenceKindEntry lbl_8036E150[];
    extern SequenceVariantEntry lbl_80373210[];
    extern u8 lbl_80373750[];
    u32 i;

    *resourceId = 0;
    *resourceGroup = 0;
    if (type == 0 || type > 4) {
        return;
    }

    if (owner != NULL && ((u8*)owner)[0x75] != 0) {
        s32 modelId = *(u16*)((u8*)owner + 0x70);
        u16 first;
        u16 last;

        if (variant != 0 && type != 4 && type != 2 &&
            variant < wazaSequenceCheckResExists(group, type)) {
            first = 0x162;
            last = 0x16E;
        } else {
            switch (type) {
            case 1:
                first = 1;
                last = 0x11C;
                break;
            case 2:
                first = 0x11E;
                last = 0x12A;
                break;
            case 3:
                first = 0x12C;
                last = 0x15E;
                break;
            case 4:
                first = 0x160;
                last = 0x160;
                break;
            }
        }

        for (i = first; i < last; i++) {
            u8* entry = lbl_80373750 + i * 0x10;
            if (*(u16*)(entry + 2) == modelId &&
                *(u16*)(entry + 4) == group && entry[0] == variant) {
                u32 foundGroup = *(u32*)(entry + 8);
                u32 foundId = *(u32*)(entry + 0x0C);
                if (foundGroup != 0 && foundId != 0) {
                    *resourceGroup = foundGroup;
                    *resourceId = foundId;
                    return;
                }
                break;
            }
        }
    }

    if (type > 3) {
        if (group == 0 || group >= lbl_80478CE0) {
            return;
        }
        *resourceGroup = lbl_803727C8[group].group;
        *resourceId = lbl_803727C8[group].resource;
    } else {
        u16 kind = type - 1;
        SequenceVariantEntry* entry;
        if (group == 0 || group >= lbl_80478CC0) {
            return;
        }
        *resourceGroup = lbl_8036E150[group].kinds[kind].group;
        *resourceId = lbl_8036E150[group].kinds[kind].resource;
        if (variant == 0) {
            return;
        }
        entry = lbl_80373210;
        for (i = 0; i < lbl_80478CE8; i++, entry++) {
            if (entry->group == group && entry->variant == variant) {
                u32 foundGroup = entry->kinds[kind].group;
                u32 foundId = entry->kinds[kind].resource;
                if (foundGroup != 0 && foundId != 0) {
                    *resourceGroup = foundGroup;
                    *resourceId = foundId;
                }
                return;
            }
        }
    }
}

#endif

#if defined(SEQUENCE_801DE164_801DE190)

/**
 * wazaSequenceOwnerGet78 - Waza hit flash get active.
 * Address: 0x801DE164 | Size: 0x2C
 */
BOOL wazaSequenceOwnerGet78(s32 slot) {
    void* obj;

    obj = (void*)PC_PTR(slot);
    if (obj == NULL) {
        return FALSE;
    }
    if (*(u8*)((u8*)obj + 0x75) != 0) {
        return *(s32*)((u8*)obj + 0x78);
    }
    return FALSE;
}

#endif

#if defined(SEQUENCE_801DE190_801DE654)

#if !defined(SEQUENCE_CANDIDATE_801DE598_ONLY) && \
    !defined(SEQUENCE_CANDIDATE_801DE418_ONLY)
/**
 * wazaSequencePokemonCreate - Create a Pokemon owner from its species index: pick its model resource (Unown by
 * form, a dedicated shiny model if one exists, otherwise a fallback), load it into a free owner slot, apply the
 * shiny colours if needed and set up Spinda's spots. The Pokemon form of wazaSequenceTrainerCreate.
 * Address: 0x801DE190 | Size: 0x288
 */
/* NAME-T3 fn_801DE190 */
void* wazaSequencePokemonCreate(u16 index, void* model, u8 variant) {
    typedef struct SequenceOverrideEntry {
        u16 index;    /* 0x0 */
        u32 group;    /* 0x4 */
        u32 resource; /* 0x8 */
    } SequenceOverrideEntry;
    extern u32 lbl_80478CD0;
    extern u32 lbl_80478CF0;
    extern u32 lbl_80478CF8;
    extern SequenceResourcePair lbl_80370BD0[];
    extern SequenceResourceRef lbl_80374E40[];
    extern SequenceOverrideEntry lbl_80374F20[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonGetAnnonKatati(void* model);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
    extern void* wazaSequenceSysAllocOwner(void);
    extern void wazaSequenceOwnerFree(void* sequence);
    extern void wazaSequenceOwnerApplyShinyColors(void* sequence);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 sequenceLoad(void* sequence, void* resource);
#endif
    extern void pachiruEffectSetupModel(void* dst, u32 arg1, void* model);
    void* resourcePtr;
    u8* sequence;
    u32 group;
    u32 resource;
    u8 overridden = 0;

    if (index == 0 || index >= lbl_80478CD0) {
        return NULL;
    }

    if (index == 0xC9) {
        u8 form = pokemonGetAnnonKatati(PC_PTR32_IMPLICIT(model)) % lbl_80478CF0;
        group = lbl_80374E40[form].group;
        resource = lbl_80374E40[form].resource;
    } else {
        group = lbl_80370BD0[index].group;
        resource = lbl_80370BD0[index].resource;
    }
    if (group == 0 || resource == 0) {
        group = lbl_80370BD0[0x11A].group;
        resource = lbl_80370BD0[0x11A].resource;
        if (group == 0 || resource == 0) {
            return NULL;
        }
    }

    if (variant != 0) {
        u32 overrideGroup = 0;
        u32 overrideResource = 0;
        u32 i;

        for (i = 0; i < lbl_80478CF8; i++) {
            if (index == lbl_80374F20[i].index) {
                overrideGroup = lbl_80374F20[i].group;
                overrideResource = lbl_80374F20[i].resource;
                break;
            }
        }
        if (overrideGroup != 0 && overrideResource != 0) {
            group = overrideGroup;
            resource = overrideResource;
            overridden = 1;
        }
    }

    sequenceWaitResource(group, resource);

    resourcePtr = GSresGetResource(group, resource);
    if (resourcePtr == NULL) {
        return NULL;
    }
    sequence = wazaSequenceSysAllocOwner();
    if (sequence == NULL) {
        return NULL;
    }
    *(u16*)(sequence + 0x70) = index;
    *(u16*)(sequence + 0x72) = group;
    sequence[0x75] = 1;
    PTR32_AT_SET_T(void, sequence, 0x78, model); /* a 32-bit slot: read back as an s32 (sequence.c 0x78 getter) */
    sequence[0x77] = variant;
    if (sequenceLoad(sequence, resourcePtr) == 0) {
        wazaSequenceOwnerFree(sequence);
        return NULL;
    }
    if (variant != 0 && overridden == 0) {
        wazaSequenceOwnerApplyShinyColors(sequence);
    }
    if (index == 0x134) {
        pachiruEffectSetupModel(sequence + 0x50, *(u32*)(sequence + 0x24), model);
    }
    return sequence;
}

#endif

#if !defined(SEQUENCE_CANDIDATE_801DE598_ONLY) && \
    !defined(SEQUENCE_CANDIDATE_801DE190_ONLY)
/**
 * wazaSequenceTrainerCreate - Waza HP drain effect.
 * Address: 0x801DE418 | Size: 0x180
 */
void* wazaSequenceTrainerCreate(u16 index) {
    extern u32 lbl_80478CC8;
    extern SequenceResourcePair lbl_80370840[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
    extern void* wazaSequenceSysAllocOwner(void);
    extern void wazaSequenceOwnerFree(void* sequence);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 sequenceLoad(void* sequence, void* resource);
#endif
    u8* sequence;
    u32 group;
    u32 resourceId;
    void* resource;

    if (index == 0 || index >= lbl_80478CC8) {
        return NULL;
    }

    group = lbl_80370840[index].group;
    resourceId = lbl_80370840[index].resource;
    if (group == 0 || resourceId == 0) {
        group = lbl_80370840[1].group;
        resourceId = lbl_80370840[1].resource;
        if (group == 0 || resourceId == 0) {
            return NULL;
        }
    }

    sequenceWaitResource(group, resourceId);
    resource = GSresGetResource(group, resourceId);
    if (resource == NULL) {
        return NULL;
    }

    sequence = wazaSequenceSysAllocOwner();
    if (sequence == NULL) {
        return NULL;
    }
    *(u16*)(sequence + 0x70) = index;
    *(u16*)(sequence + 0x72) = group;
    sequence[0x75] = 0;
    if (sequenceLoad(sequence, resource) == 0) {
        wazaSequenceOwnerFree(sequence);
        return NULL;
    }
    return sequence;
}
#endif

#if !defined(SEQUENCE_CANDIDATE_801DE190_ONLY) && \
    !defined(SEQUENCE_CANDIDATE_801DE418_ONLY)
/**
 * wazaSequenceLoadGroupAndWait - Request an archive group if it is not loaded, then wait (switching threads) until
 * it is. The out-of-line copy of the sequenceWaitResource inline above.
 * Address: 0x801DE598 | Size: 0xBC
 */
/* NAME-T3 fn_801DE598 */
void wazaSequenceLoadGroupAndWait(u32 group, u32 resource) {
    extern void fn_8017B3E4(u32 group);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSresGetResource(u32 group, u32 resource);
#endif
    extern const char lbl_802799C8[];
    s32 state;

    state = fn_8017B2CC(group);
    if (state < 0) {
        fn_8017B3E4(group);
    } else if (fn_8017B2CC(group) == 0 && resource != 0 &&
               GSresGetResource(group, resource) == NULL) {
        fn_8017B3E4(group);
    }

    for (;;) {
        state = fn_8017B2CC(group);
        if (state < 0) {
            GSlogWrite(lbl_802799C8);
        }
        if (state == 0) {
            break;
        }
        _threadSwitch();
    }
}
#endif

#endif

#if defined(SEQUENCE_801DE654_801DE698)

/**
 * sequenceAnimEndCallback - Waza HP drain get active.
 * Address: 0x801DE654 | Size: 0x44
 */
void sequenceAnimEndCallback(void* model, void* info) {
    /* GSmodel's animation-ended callback: (model, the event record on the caller's stack) */
    fn_801DE698(PC_PTR32(model), info);
    _eyeTexAnimEnded(model, info);
}

#endif
