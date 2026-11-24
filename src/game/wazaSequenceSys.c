/**
 * @file wazaSequenceSys.c
 * @brief Matching waza motion and effect helpers.
 *
 * Address range: 0x801D9E1C - 0x801DAC90
 * Shared typedefs and cross-TU forward declarations live in
 * include/game/battle/battle_waza_types.h.
 */

#include "game/battle/battle_waza_types.h"
#ifdef TARGET_PC
#include "game/gs_render_util.h" /* GSRenderCamera: the camera's cobj by field (PC layout) */
#endif


/**
 * wazaSequenceOwnerGetFlag77 - Pokemon motion get complete. Returns obj->0x77 or 0 if NULL.
 * Address: 0x801D9E1C | Size: 0x18
 */
u8 wazaSequenceOwnerGetFlag77(void* obj) {
    if (obj != NULL) {
        return *(u8*)((u8*)obj + 0x77);
    }
    return 0;
}

/**
 * wazaSequenceOwnerDeleteAuraParticle - Kill the owner's aura particle.
 * The particle comes from the system's +0xC bank (wazaSequenceOwnerCreateAuraParticle); called when the aura
 * state is cleared (wazaSequenceOwnerClearAura) and when the owner is released.
 * Address: 0x801D9E34 | Size: 0x58
 */
/* NAME-T3 fn_801D9E34 */
void wazaSequenceOwnerDeleteAuraParticle(void* obj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKillOwned(u8* resource, u32 notify);
#endif
    WazaEffect* effect = obj;

    if (*(u32*)(lbl_80467CC0 + 0xC) != 0 && P32(effect->effect_handle) != NULL) {
        gsParticleKillOwned(P32(effect->effect_handle), 1);
        P32_SET(effect->effect_handle, NULL);
    }
}

/* =========================================================================
 * WAZA EFFECT HELPERS (0x801D9E8C - 0x801DAC78)
 * ========================================================================= */


#define WAZA_SDATA2_FLOAT(addr) extern const f32 lbl_##addr
#define WAZA_SDATA2_VALUE(addr) lbl_##addr
WAZA_SDATA2_FLOAT(8047E388);
WAZA_SDATA2_FLOAT(8047E38C);
WAZA_SDATA2_FLOAT(8047E390);
WAZA_SDATA2_FLOAT(8047E394);
WAZA_SDATA2_FLOAT(8047E398);
WAZA_SDATA2_FLOAT(8047E39C);
#undef WAZA_SDATA2_FLOAT


/**
 * wazaSequenceOwnerCreateAuraParticle - Create the owner's aura particle.
 * Creates a particle from the system's +0xC bank, links it to the model part named by the owner's table entry
 * (+0x50) and scales it by the owner's size class. Called when the aura state is set (wazaSequenceOwnerSetAura).
 * Address: 0x801D9E8C | Size: 0x188
 */
/* NAME-T3 fn_801D9E8C */
void wazaSequenceOwnerCreateAuraParticle(void* effect) {
    WazaEffect* fx;
    GSpart* target;
    u8* ctx;
    f32 val;
    s32 scale_selector;
    f32 scale[3];

    if (effect == NULL) return;
    fx = (WazaEffect*)effect;
    ctx = PTR32_AT_T(u8, lbl_80467CC0, 0xC);
    if (ctx == 0) return;
    if (P32(fx->effect_handle) != 0) return;

    P32_SET(fx->effect_handle, gsParticleCreate(ctx, 0, 0));
    if (P32(fx->effect_handle) == 0) return;

    if ((fx->flags & 1) != 1) {
        fn_80118C88(P32(fx->effect_handle), 0);
    }

    {
        s32 val_idx = *(s32*)((u8*)P32(fx->table) +
                              fx->index * sizeof(WazaEffectTblEntry) + 0x50);
        if (val_idx > 0) {
            target = GSmodelGetPart(P32(fx->model), val_idx);
        } else {
            target = NULL;
        }
    }

    if (target == NULL) return;

    if (effect != NULL) {
        scale_selector = fx->scale_selector;
    } else {
        scale_selector = 0;
    }

    switch (scale_selector) {
    case -2: val = WAZA_SDATA2_VALUE(8047E388); break;
    case -1: val = WAZA_SDATA2_VALUE(8047E38C); break;
    case 1:  val = WAZA_SDATA2_VALUE(8047E390); break;
    case 2:  val = WAZA_SDATA2_VALUE(8047E394); break;
    case 3:  val = WAZA_SDATA2_VALUE(8047E398); break;
    default: val = WAZA_SDATA2_VALUE(8047E39C); break;
    }

    set__5GSvecFfff(scale, val, val, val);

    gsParticleLinkToJObj(P32(fx->effect_handle), target, 4, 0, 1, 0);
    fn_80118D18(P32(fx->effect_handle), 1);
    fn_80118DE0(P32(fx->effect_handle), scale, 1, 0);
    GSpartFree(target);
}
#undef WAZA_SDATA2_VALUE

/**
 * wazaSequenceOwnerDeleteStopParticle - Kill the owner's stop-state particle.
 * The particle comes from the system's +0x8 bank (wazaSequenceOwnerCreateStopParticle); called when the stop
 * state is cleared (wazaSequenceOwnerClearStop) and when the owner is released.
 * Address: 0x801DA014 | Size: 0x5C
 */
/* NAME-T3 fn_801DA014 */
void wazaSequenceOwnerDeleteStopParticle(void* effect) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gsParticleKillOwned(u8* resource, u32 notify);
#endif
    WazaEffect* fx = effect;

    if (fx != NULL && *(u32*)(lbl_80467CC0 + 0x8) != 0 && P32(fx->field_80) != NULL) {
        gsParticleKillOwned(P32(fx->field_80), 1);
        P32_SET(fx->field_80, NULL);
    }
}

/**
 * wazaSequenceOwnerCreateStopParticle - Create the owner's stop-state particle.
 * Creates a particle from the system's +0x8 bank on the model's centre part (or the table entry's +0x54 part),
 * scaled by size class. Called when the stop state is set (wazaSequenceOwnerSetStop).
 * Address: 0x801DA070 | Size: 0x1B4
 */
#define WAZA_SDATA2_VALUE(addr) lbl_##addr
/* NAME-T3 fn_801DA070 */
void wazaSequenceOwnerCreateStopParticle(void* effect) {
    WazaEffect* fx;
    GSpart* target;
    void* entry;
    u8* ctx;
    s32 n;
    f32 val;
    s32 scale_selector;
    f32 scale[3];

    fx = (WazaEffect*)effect;
    if (fx == NULL) return;
    ctx = PTR32_AT_T(u8, lbl_80467CC0, 0x8);
    if (ctx == 0) return;
    if (P32(fx->field_80) != 0) return;

    P32_SET(fx->field_80, gsParticleCreate(ctx, 0, 0));
    if (P32(fx->field_80) == 0) return;

    if ((fx->flags & 1) != 1) {
        fn_80118C88(P32(fx->field_80), 0);
    }

    entry = (u8*)P32(fx->table) + fx->index * sizeof(WazaEffectTblEntry);
    if ((u8)GSmodelCenterNull(P32(fx->model)) != 0) {
        n = GSmodelWalkJObjs(P32(fx->model)) - 1;
    } else {
        n = *(s32*)((u8*)entry + 0x54);
    }

    if (n > 0) {
        target = GSmodelGetPart(P32(fx->model), n);
    } else {
        target = NULL;
    }

    if (target == NULL) return;

    if (effect != NULL) {
        scale_selector = fx->scale_selector;
    } else {
        scale_selector = 0;
    }

    switch (scale_selector) {
    case -2: val = WAZA_SDATA2_VALUE(8047E388); break;
    case -1: val = WAZA_SDATA2_VALUE(8047E38C); break;
    case 1:  val = WAZA_SDATA2_VALUE(8047E390); break;
    case 2:  val = WAZA_SDATA2_VALUE(8047E394); break;
    case 3:  val = WAZA_SDATA2_VALUE(8047E398); break;
    default: val = WAZA_SDATA2_VALUE(8047E39C); break;
    }

    set__5GSvecFfff(scale, val, val, val);

    gsParticleLinkToJObj(P32(fx->field_80), target, 4, 0, 1, 0);
    fn_80118D18(P32(fx->field_80), 1);
    fn_80118CAC(P32(fx->field_80), 1);
    fn_80118DE0(P32(fx->field_80), scale, 1, 0);
    GSpartFree(target);
}
#undef WAZA_SDATA2_VALUE

/**
 * wazaSequenceOwnerSetFlags - Apply a state-flags byte to an owner.
 * Bit 4 sets or clears the aura state, bit 8 the stop state, bit 2 the root-null state; the byte is then stored
 * at +0x18 (read back by wazaSequenceOwnerGetFlags).
 * Address: 0x801DA224 | Size: 0xA0
 */
/* NAME-T3 fn_801DA224 */
void wazaSequenceOwnerSetFlags(void* effect, s32 flags) {
    if (effect != NULL) {
        u8 value = flags;

        if ((flags & 4) == 4) {
            wazaSequenceOwnerSetAura(effect);
        } else {
            wazaSequenceOwnerClearAura(effect);
        }

        if ((value & 8) == 8) {
            wazaSequenceOwnerSetStop(effect);
        } else {
            wazaSequenceOwnerClearStop(effect);
        }

        if ((value & 2) == 2) {
            wazaSequenceOwnerSetNull(effect);
        } else {
            wazaSequenceOwnerClearNull(effect);
        }

        *(u8*)((u8*)effect + 0x18) = flags;
    }
}

/**
 * wazaSequenceOwnerResetFlags - Clear the aura, stop and root-null states and show the owner again
 * (its model and both particles).
 * Address: 0x801DA2C4 | Size: 0x90
 */
/* NAME-T3 fn_801DA2C4 */
void wazaSequenceOwnerResetFlags(void* effect) {
    WazaEffect* fx = effect;
    struct GSmodel* model;

    if (fx != NULL) {
        wazaSequenceOwnerClearAura(fx);
        wazaSequenceOwnerClearStop(fx);
        wazaSequenceOwnerClearNull(fx);

        if (fx != NULL && (model = P32(fx->model)) != NULL) {
            fx->flags |= 1;
            GSmodelSetVisibility(model, 1);
            if (P32(fx->field_80) != 0) {
                fn_80118C88(P32(fx->field_80), 1);
            }
            if (P32(fx->effect_handle) != 0) {
                fn_80118C88(P32(fx->effect_handle), 1);
            }
        }
    }
}

/**
 * wazaSequenceOwnerGetFlags - Waza effect get trajectory type. Returns effect->0x18 or 0 if NULL.
 * Address: 0x801DA354 | Size: 0x18
 */
u8 wazaSequenceOwnerGetFlags(void* effect) {
    if (effect != NULL) {
        return *(u8*)((u8*)effect + 0x18);
    }
    return 0;
}

/**
 * wazaSequenceOwnerClearState - Clear one owner state by kind: 1 aura, 2 stop, 3 root null.
 * Address: 0x801DA36C | Size: 0x60
 */
/* NAME-T3 fn_801DA36C */
void wazaSequenceOwnerClearState(void* effect, s32 trajType) {
    if (effect != NULL) {
        switch ((u8)trajType) {
        case 1:
            wazaSequenceOwnerClearAura(effect);
            return;
        case 2:
            wazaSequenceOwnerClearStop(effect);
            return;
        case 3:
            wazaSequenceOwnerClearNull(effect);
            return;
        }
    }
}

/**
 * wazaSequenceOwnerSetState - Set one owner state by kind: 1 aura, 2 stop, 3 root null.
 * Address: 0x801DA3CC | Size: 0x60
 */
/* NAME-T3 fn_801DA3CC */
void wazaSequenceOwnerSetState(void* effect, s32 trajType) {
    if (effect != NULL) {
        switch ((u8)trajType) {
        case 1:
            wazaSequenceOwnerSetAura(effect);
            return;
        case 2:
            wazaSequenceOwnerSetStop(effect);
            return;
        case 3:
            wazaSequenceOwnerSetNull(effect);
            return;
        }
    }
}

/**
 * wazaSequenceOwnerIsVisible - Waza effect get bit 0 of field 0x18.
 * Address: 0x801DA42C | Size: 0x1C
 */
u32 wazaSequenceOwnerIsVisible(void* effect) {
    if (effect != NULL) {
        return *(u8*)((u8*)effect + 0x18) & 1;
    }
    return 0;
}

/**
 * wazaSequenceOwnerSetParticleVisibility - Show or hide the particle banks of every running sequence of an owner
 * (particle entries, kind 3). The menu model draw shows them around wazaSequenceOwnerDraw.
 * Address: 0x801DA448 | Size: 0xA0
 */
/* NAME-T3 fn_801DA448 */
void wazaSequenceOwnerSetParticleVisibility(void* effect, u32 visible) {
    extern void GSmodelSetParticleVisibility(void* bank, u32 visible);
    u8* sequence;
    u8* model;

    if (effect != NULL) {
        sequence = PTR32_AT_T(u8, (u8*)effect, 0x68);
        while (sequence != NULL) {
            if (*(u8*)(sequence + 0x14) != 0) {
                model = PTR32_AT_T(u8, sequence, 0x24);
                while (model != NULL) {
                    if (*(s32*)(model + 4) == 3 && *(s32*)(model + 0x18) == 0 &&
                        PTR32_AT_T(void, model, 0x88) != NULL) {
                        GSmodelSetParticleVisibility(PTR32_AT_T(void, model, 0x88), visible);
                    }
                    model = PTR32_AT_T(u8, model, 0xA8);
                }
            }
            sequence = PTR32_AT_T(u8, sequence, 0x34);
        }
    }
}

/**
 * wazaSequenceOwnerSetVisibility - Set visibility on a waza effect and its attached effects.
 * Address: 0x801DA4E8 | Size: 0xC4
 */
void wazaSequenceOwnerSetVisibility(void* effect, u32 visible) {
    WazaEffect* fx = effect;
    struct GSmodel* model;

    if (fx != NULL && (model = P32(fx->model)) != NULL) {
        if ((u8)visible != 0) {
            fx->flags |= 1;
            GSmodelSetVisibility(model, 1);
            if (P32(fx->field_80) != 0) {
                fn_80118C88(P32(fx->field_80), 1);
            }
            if (P32(fx->effect_handle) != 0) {
                fn_80118C88(P32(fx->effect_handle), 1);
            }
        } else {
            u8 flags = fx->flags;

            if ((flags & 1) != 0) {
                fx->flags = flags ^ 1;
            }
            GSmodelSetVisibility(model, 0);
            if (P32(fx->field_80) != 0) {
                fn_80118C88(P32(fx->field_80), 0);
            }
            if (P32(fx->effect_handle) != 0) {
                fn_80118C88(P32(fx->effect_handle), 0);
            }
        }
    }
}

/**
 * wazaSequenceSetDownFlag - Waza effect set fields 0x16=0, 0x17=r4.
 * Address: 0x801DA5AC | Size: 0x18
 */
void wazaSequenceSetDownFlag(void* effect, u8 val) {
    if (effect == NULL) return;
    *(u8*)((u8*)effect + 0x16) = 0;
    *(u8*)((u8*)effect + 0x17) = val;
}

#ifdef TARGET_PC
/* PC (observes only): owner pool entry i (the pool walked as wazaSequenceSysIsAllEnd does, PC layout), or NULL when i
 * is past the count or the entry is inactive; *count gets the pool's count. For pc_debug_game.c's ORRE_OWNERS_LOG,
 * which reads the blink fields at the offsets wazaSequenceOwnerUpdateBlink uses (2026-10-25). Not weak (a
 * weak definition gets stubbed by firstlink) */
u8* pc_waza_owner(s32 i, s32* count) {
    u8* pool = lbl_80467CC0;
    WazaEffect* effect = PTR32_AT_T(WazaEffect, pool, 0);
    *count = effect != NULL ? *(u16*)(pool + 4) : 0;
    if (effect == NULL || i < 0 || i >= *count || effect[i].active == 0) {
        return NULL;
    }
    return (u8*)&effect[i];
}

/* PC (observes only): the owner's running sequence (+0x6C, as wazaSequenceOwnerIsPlaying and
 * wazaSequenceSysIsAllEnd read it), or NULL */
u8* pc_waza_owner_seq(u8* owner) {
    return owner != NULL ? PTR32_AT_T(u8, owner, 0x6C) : NULL;
}

/* PC (observes only): wazaSequenceSysGetWazaTime's timing record for the sequence (its owner +0x2C table, kind *
 * 0xD4), or NULL */
u8* pc_waza_seq_timing(u8* seq) {
    WazaSequence* waza = (WazaSequence*)seq;
    u8* owner;
    if (seq == NULL) {
        return NULL;
    }
    owner = PTR32_AT_T(u8, seq, 0x3C);
    if (owner == NULL) {
        return NULL;
    }
    owner = PTR32_AT_T(u8, owner, 0x2C);
    return owner != NULL ? owner + waza->kind * 0xD4 : NULL;
}
#endif
#ifdef TARGET_PC
#endif

/**
 * wazaSequenceSysIsAllEnd - Check whether every active effect sequence has finished.
 * Address: 0x801DA5C4 | Size: 0xD4
 */
u8 wazaSequenceSysIsAllEnd(s32 timeType) {
    u8* pool = lbl_80467CC0;
    WazaEffect* effect;
    s32 i = 0;
    s32 count;
    void* waza;

    effect = PTR32_AT_T(WazaEffect, pool, 0);
    count = *(u16*)(pool + 4);
    for (; i < count; i++, effect++) {
        if (effect->active != 0) {
            waza = PTR32_AT_T(void, (u8*)effect, 0x6C);
            if (waza != NULL) {
                if (waza == NULL) {
                    return TRUE;
                }

                if ((u8)timeType == 6) {
                    if (*(u8*)((u8*)waza + 0x14) != 0 && *(s8*)((u8*)waza + 0x15) != -1) {
                        return FALSE;
                    }
                    return TRUE;
                }

                return *(s32*)waza >=
                       wazaSequenceSysGetWazaTime(PTR32_AT_T(void, (u8*)waza, 0x3C), waza, timeType);
            }
        }
    }
    return TRUE;
}

/**
 * wazaSequenceSysIsWazaEnd - Waza effect tick lifetime.
 * Address: 0x801DA698 | Size: 0xB4
 */
u8 wazaSequenceSysIsWazaEnd(void* sequence, s32 moveID, s32 variant, s32 timeType) {
    void* waza;

    if (sequence == NULL) {
        return FALSE;
    }

    waza = GetWaza__12NullSequenceCFUsUs(sequence, moveID, variant);
    if (waza == NULL) {
        return TRUE;
    }

    if ((u8)timeType == 6) {
        if (*(u8*)((u8*)waza + 0x14) != 0 && *(s8*)((u8*)waza + 0x15) != -1) {
            return FALSE;
        }
        return TRUE;
    }

    return *(s32*)waza >= wazaSequenceSysGetWazaTime(PTR32_AT_T(void, (u8*)waza, 0x3C), waza, timeType);
}

/**
 * wazaSequenceSysGetWazaTimeById - Find the owner's sequence for (move, variant) and return its time of the given
 * type (wazaSequenceSysGetWazaTime), or 0 when there is none.
 * Address: 0x801DA74C | Size: 0x60
 */
/* NAME-T3 fn_801DA74C */
s32 wazaSequenceSysGetWazaTimeById(void* sequence, s32 moveID, s32 variant, s32 timeType) {
    void* waza;

    if (sequence == NULL) {
        return 0;
    }

    waza = GetWaza__12NullSequenceCFUsUs(sequence, moveID, variant);
    if (waza != NULL) {
        return wazaSequenceSysGetWazaTime(sequence, waza, timeType);
    }
    return 0;
}

/**
 * wazaSequenceSysFreeAll - Clear child sequences from every active effect.
 * Address: 0x801DA7AC | Size: 0x90
 */
void wazaSequenceSysFreeAll(void) {
    extern void wazaSequenceApplyStop(void* obj);
    extern void wazaSequenceFree(void* obj);
    u8* pool;
    WazaEffect* effect;
    s32 i;
    s32 count;
    void* child;
    void* next;

    pool = lbl_80467CC0;
    effect = PTR32_AT_T(WazaEffect, pool, 0);
    count = *(u16*)(pool + 4);
    for (i = 0; i < count; i++, effect++) {
        if (effect->active != 0) {
            child = PTR32_AT_T(void, (u8*)effect, 0x68);
            while (child != NULL) {
                next = PTR32_AT_T(void, (u8*)child, 0x34);
                if (*(u8*)((u8*)child + 0x14) != 0) {
                    wazaSequenceApplyStop(child);
                }
                wazaSequenceFree(child);
                child = next;
            }
            PTR32_AT_SET_T(void, (u8*)effect, 0x68, NULL);
        }
    }
}

/**
 * wazaSequenceSysFreeOwnerSequences - Stop and free every sequence of one owner, if its slot is in use (+0x74).
 * The one-owner form of wazaSequenceSysFreeAll.
 * Address: 0x801DA83C | Size: 0x88
 */
/* NAME-T3 fn_801DA83C */
void wazaSequenceSysFreeOwnerSequences(void* effect) {
    extern void wazaSequenceApplyStop(void* obj);
    extern void wazaSequenceFree(void* obj);

    void* cur;
    void* next;

    if (effect != NULL) {
        cur = PTR32_AT_T(void, (u8*)effect, 0x68);
        if (*(u8*)((u8*)effect + 0x74) == 0) {
            return;
        }
        while (cur != NULL) {
            next = PTR32_AT_T(void, (u8*)cur, 0x34);
            if (*(u8*)((u8*)cur + 0x14) != 0) {
                wazaSequenceApplyStop(cur);
            }
            wazaSequenceFree(cur);
            cur = next;
        }
        PTR32_AT_SET_T(void, (u8*)effect, 0x68, NULL);
    }
}

/**
 * wazaSequenceSysStopWaza - Waza resolve/update helper.
 * Address: 0x801DA8C4 | Size: 0x50
 */
void wazaSequenceSysStopWaza(void* obj, s32 search_key1, s32 search_key2) {
    void* resolved;
    if (obj != NULL) {
        resolved = GetWaza__12NullSequenceCFUsUs(obj, search_key1, search_key2);
        if (resolved != NULL) {
            if (*(u8*)((u8*)resolved + 0x14) != 0) {
                wazaSequenceApplyStop(resolved);
            }
            wazaSequenceFree(resolved);
        }
    }
}

/**
 * wazaSequenceClearFlag16 - Waza effect pool get used count.
 * Address: 0x801DA914 | Size: 0x38
 */
void wazaSequenceClearFlag16(void* obj, s32 search_key1, s32 search_key2) {
    void* resolved;
    if (obj != NULL) {
        resolved = GetWaza__12NullSequenceCFUsUs(obj, search_key1, search_key2);
        if (resolved != NULL) {
            *(u8*)((u8*)resolved + 0x16) = 0;
        }
    }
}

/**
 * wazaSequenceSysIsPlayingWaza - Waza resolved effect has active non-minus-one byte.
 * Address: 0x801DA94C | Size: 0x68
 */
s32 wazaSequenceSysIsPlayingWaza(void* obj, s32 search_key1, s32 search_key2) {
    void* resolved;
    u8 result;
    if (obj == NULL) {
        return 0;
    }
    resolved = GetWaza__12NullSequenceCFUsUs(obj, search_key1, search_key2);
    if (resolved != NULL) {
        result = 0;
        if (*(u8*)((u8*)resolved + 0x14) != 0) {
            if (*(s8*)((u8*)resolved + 0x15) != -1) {
                result = 1;
            }
        }
        return result;
    }
    return 0;
}

/**
 * wazaSequenceSysApplyStopWaza - Find the owner's sequence for (move, variant) and stop it, keeping it loaded
 * (wazaSequenceSysStopWaza also frees it).
 * Address: 0x801DA9B4 | Size: 0x34
 */
/* NAME-T3 fn_801DA9B4 */
void wazaSequenceSysApplyStopWaza(void* obj, s32 search_key1, s32 search_key2) {
    void* resolved;
    if (obj != NULL) {
        resolved = GetWaza__12NullSequenceCFUsUs(obj, search_key1, search_key2);
        if (resolved != NULL) {
            wazaSequenceApplyStop(resolved);
        }
    }
}

/**
 * wazaSequenceSysStartWaza - Waza sound effect play.
 * Address: 0x801DA9E8 | Size: 0xC4
 */
void wazaSequenceSysStartWaza(void* sequence, s32 moveID, s32 variant) {
    u8* pool = lbl_80467CC0;
    WazaEffect* entry;
    s32 i = 0;
    int count;
    void* waza;

    entry = PTR32_AT_T(WazaEffect, pool, 0);
    count = *(u16*)(pool + 4);
    for (; i < count; i++, entry++) {
        if (entry->active != 0) {
            waza = GetWaza__12NullSequenceCFUsUs(entry, moveID, variant);
            if (waza != NULL && *(u8*)((u8*)waza + 0x14) != 0) {
                wazaSequenceApplyStop(waza);
            }
        }
    }

    if (sequence != NULL) {
        waza = GetWaza__12NullSequenceCFUsUs(sequence, moveID, variant);
        if (sequence != NULL) {
            *(u8*)((u8*)sequence + 0x16) = 0;
            *(u8*)((u8*)sequence + 0x17) = 0;
        }
        if (waza != NULL) {
            wazaSequenceStart(waza);
        }
    }
}

/**
 * wazaSequenceOwnerDraw - Draw an owner by hand: its model and every attached sequence layer in three passes
 * (flags 0x10, 0x1000, 0x2000) for both model groups, then its particles through the active camera. Used by the
 * menu model draw (the Pokemon summary).
 * Address: 0x801DAAAC | Size: 0x100
 */
#ifdef TARGET_PC
/* GScamera's cobj is at +0x10 on the PC (8-byte desc before it); the GameCube offset +0xC glued two halves
 * together and the Pokémon summary's model render crashed (2026-10-17) */
#define WAZA_CAMERA_COBJ(camera) (((GSRenderCamera*)(camera))->cobj)
#else
#define WAZA_CAMERA_COBJ(camera) (*(void**)((u8*)(camera) + 0xC))
#endif
/* NAME-T3 fn_801DAAAC */
void wazaSequenceOwnerDraw(void* effectPtr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GScameraGetActiveCamera(void);
#endif
    extern s32 HSD_CObjSetCurrent(void* cobj);
    extern void gsParticleDraw(u32 drawFlags, u8 modelID);
    extern void HSD_CObjEndCurrent(void);
    WazaEffect* effect = effectPtr;
    u8* effectBytes = effectPtr;
    u32 flags;
    u8* entry;
    u32 modelID;
    s32 modelGroup;
    s32 pass;
    void* camera;

    if (effect != NULL) {
        GSmodelForceAnimTransformUpdate(P32(effect->model));
        for (modelGroup = 0; modelGroup < 2; modelGroup++) {
            modelID = modelGroup != 0;
            for (pass = 0; pass < 3; pass++) {
                if (pass == 0) {
                    flags = 0x10;
                } else if (pass == 1) {
                    flags = 0x1000;
                } else {
                    flags = 0x2000;
                }

                if (modelID == (u8)fn_800E3CBC(P32(effect->model))) {
                    GSmodelDrawModel(P32(effect->model), flags);
                }

                entry = PTR32_AT_T(u8, effectBytes, 0x68);
                while (entry != NULL) {
                    if (*(u8*)(entry + 0x14) != 0) {
                        wazaSequenceOwnerDrawAttached(entry, flags, modelID);
                    }
                    entry = PTR32_AT_T(u8, entry, 0x34);
                }

                camera = GScameraGetActiveCamera();
                if (camera != NULL && HSD_CObjSetCurrent(WAZA_CAMERA_COBJ(camera))) {
                    gsParticleDraw(flags, modelID);
                    HSD_CObjEndCurrent();
                }
            }
        }
    }
}

/**
 * wazaSequenceOwnerGetSizeScale - The scale factor for the owner's size class (+0x10, -2..3; the class
 * wazaSequenceOwnerGetSizeClass returns). The move camera uses it.
 * Address: 0x801DABAC | Size: 0x78
 */
/* NAME-T3 fn_801DABAC */
f32 wazaSequenceOwnerGetSizeScale(void* obj) {
    s32 selector;

    if (obj != NULL) {
        selector = *(s32*)((u8*)obj + 0x10);
    } else {
        selector = 0;
    }

    switch (selector) {
    case -2:
        return lbl_8047E388;
    case -1:
        return lbl_8047E38C;
    case 1:
        return lbl_8047E390;
    case 2:
        return lbl_8047E394;
    case 3:
        return lbl_8047E398;
    default:
        return lbl_8047E39C;
    }
}

/**
 * wazaSequenceOwnerGetSizeClass - Waza get field 0x10.
 * Address: 0x801DAC24 | Size: 0x18
 */
u32 wazaSequenceOwnerGetSizeClass(void* obj) {
    if (obj != NULL) {
        return *(u32*)((u8*)obj + 0x10);
    }
    return 0;
}

/**
 * wazaSequenceOwnerGetModel - Waza get field 0x24.
 * Address: 0x801DAC3C | Size: 0x18
 */
u32 wazaSequenceOwnerGetModel(void* obj) {
    if (obj != NULL) {
        return *(u32*)((u8*)obj + 0x24);
    }
    return 0;
}

/**
 * wazaSequenceOwnerIsPlaying - Waza check field 0x6C nonzero.
 * Address: 0x801DAC54 | Size: 0x24
 */
u32 wazaSequenceOwnerIsPlaying(void* obj) {
    if (obj == NULL) {
        return 0;
    }
    return *(u32*)((u8*)obj + 0x6C) != 0;
}

/**
 * wazaSequenceOwnerGetId - Waza get field 0x70.
 * Address: 0x801DAC78 | Size: 0x18
 */
u16 wazaSequenceOwnerGetId(void* obj) {
    if (obj != NULL) {
        return *(u16*)((u8*)obj + 0x70);
    }
    return 0;
}
