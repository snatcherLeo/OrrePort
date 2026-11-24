/**
 * @file fight_range_80201764.c
 * @brief fight code, 0x80201764 - 0x80202810 (14 fns).
 *
 * Range unit assigned from the propagated subsystem map
 * (tools/subsystem_propagation.py, >=80% single-label dominance;
 * campaign 2026-07-01). All functions asm-only until matched; the
 * range name stays honest until internal TU structure is proven.
 *
 * Reconciliation (2026-07-02): the 14 definitions below were originally
 * written into src/game/trainer.c (fightOutPokemonSetPokemonJoutai08) and
 * src/game/colosseum_event.c (the other 13) -- the wrong translation
 * units for this address range per splits.txt. Moved here, in address
 * order, with no functional changes.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u32 flags);
#endif

static inline void* getPartyStatus(void* context) {
    return PC_PTR(pokemonGetStatus(context, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
}

static inline void* getPokemonStatus(void* context) {
    return PC_PTR(pokemonGetStatus(context, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
}

/* 0x80201764 | size: 0x12C */
u16 fightOutPokemonSetPokemonJoutai08(void* context, void* item, void* target) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiSetBuff();
#endif
    extern u16 pokemonSetJoutai08();
    void* partyList;
    void* pokemon;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0xCD) {
        partyList = getPartyStatus(context);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0xC8) {
            if (partyList == NULL) {
                pokemon = NULL;
            } else {
                pokemon = PC_PTR(pokemonGetStatus(partyList, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonSetJoutai08(pokemon, item, target);
        } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0xCD) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
            joutaiSetBuff(partyList, PC_PTR32_IMPLICIT(item), PC_PTR32_IMPLICIT(target));
            return 0;
#else
            return joutaiSetBuff(partyList, item, target);
#endif
        }
    } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(item)) == 0xD8) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
        joutaiSetBuff(context, PC_PTR32_IMPLICIT(item), PC_PTR32_IMPLICIT(target));
        return 0;
#else
        return joutaiSetBuff(context, item, target);
#endif
    }
}

/* 0x80201890 | size: 0x12C */
u16 fightOutPokemonGetPokemonJoutai08(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiGetBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonGetJoutai08();
#endif
    void* resolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = getPartyStatus(ctx);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonGetJoutai08(resolved, PC_PTR32_IMPLICIT(typeObj));
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return 0;
        }
        return joutaiGetBuff(resolved, PC_PTR32_IMPLICIT(typeObj));
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return 0;
    }
    return joutaiGetBuff(ctx, PC_PTR32_IMPLICIT(typeObj));
}

/* 0x802019BC | size: 0x170 */
static inline void fightRangeDispatch(void* ctx, void* src, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiCopy();
#endif
    extern void fn_80121484();
    void* srcResolved;
    void* ctxResolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        srcResolved = PC_PTR(pokemonGetStatus(src, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        ctxResolved = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (ctxResolved == NULL) {
                ctxResolved = NULL;
            } else {
                ctxResolved = getPokemonStatus(ctxResolved);
            }
            if (srcResolved == NULL) {
                srcResolved = NULL;
            } else {
                srcResolved = PC_PTR(pokemonGetStatus(srcResolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            fn_80121484(ctxResolved, srcResolved, typeObj);
        } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
            joutaiCopy(ctxResolved, srcResolved, PC_PTR32_IMPLICIT(typeObj));
        }
    } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xD8) {
        joutaiCopy(ctx, src, PC_PTR32_IMPLICIT(typeObj));
    }
}
void fightOutPokemonCopyPokemonJoutai(void* ctx, void* src, void* typeObj) {
    fightRangeDispatch(ctx, src, typeObj);
}

/* 0x80201B2C | size: 0x12C */
u16 fightOutPokemonSetPokemonJoutai0C(void* ctx, void* typeObj, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiSetIdBuff();
#endif
    extern u16 pokemonSetJoutai0C();
    void* resolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = getPartyStatus(ctx);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonSetJoutai0C(resolved, typeObj, param);
        } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
            joutaiSetIdBuff(resolved, PC_PTR32_IMPLICIT(typeObj), param);
            return 0;
#else
            return joutaiSetIdBuff(resolved, typeObj, param);
#endif
        }
    } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xD8) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
        joutaiSetIdBuff(ctx, PC_PTR32_IMPLICIT(typeObj), param);
        return 0;
#else
        return joutaiSetIdBuff(ctx, typeObj, param);
#endif
    }
}

/* 0x80201C58 | size: 0x12C */
u16 fightOutPokemonGetPokemonJoutai0C(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
    extern u16 joutaiGetIdBuff();
    extern u16 pokemonGetJoutai0C();
    void* resolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = getPartyStatus(ctx);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonGetJoutai0C(resolved, typeObj);
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return 0;
        }
        return joutaiGetIdBuff(resolved, typeObj);
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return 0;
    }
    return joutaiGetIdBuff(ctx, typeObj);
}

/* 0x80201D84 | size: 0x12C */
u16 fightOutPokemonGetJoutaiParam(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
    extern u16 joutaiGetUserFightTargetId();
    extern u16 pokemonGetJoutaiParam();
    void* resolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = getPartyStatus(ctx);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonGetJoutaiParam(resolved, typeObj);
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return 0;
        }
        return joutaiGetUserFightTargetId(resolved, typeObj);
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return 0;
    }
    return joutaiGetUserFightTargetId(ctx, typeObj);
}

/* 0x80201EB0 | size: 0x12C */
u16 fightOutPokemonSetPokemonJoutaiKaisuu(void* ctx, void* typeObj, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiSetKaisuuJoutaiDataId();
#endif
    extern u16 pokemonSetJoutaiKaisuu();
    void* resolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = getPartyStatus(ctx);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonSetJoutaiKaisuu(resolved, typeObj, param);
        } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
            joutaiSetKaisuuJoutaiDataId(resolved, PC_PTR32_IMPLICIT(typeObj), param);
            return 0;
#else
            return joutaiSetKaisuuJoutaiDataId(resolved, typeObj, param);
#endif
        }
    } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xD8) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
        joutaiSetKaisuuJoutaiDataId(ctx, PC_PTR32_IMPLICIT(typeObj), param);
        return 0;
#else
        return joutaiSetKaisuuJoutaiDataId(ctx, typeObj, param);
#endif
    }
}

/* 0x80201FDC | size: 0x12C */
u16 fn_80201FDC(void* ctx, void* typeObj, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiSetNowKaisuuJoutaiDataId();
#endif
    extern u16 fn_801217B4();
    void* resolved;
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = getPartyStatus(ctx);
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return fn_801217B4(resolved, typeObj, param);
        } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
            joutaiSetNowKaisuuJoutaiDataId(resolved, PC_PTR32_IMPLICIT(typeObj), param);
            return 0;
#else
            return joutaiSetNowKaisuuJoutaiDataId(resolved, typeObj, param);
#endif
        }
    } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xD8) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
        joutaiSetNowKaisuuJoutaiDataId(ctx, PC_PTR32_IMPLICIT(typeObj), param);
        return 0;
#else
        return joutaiSetNowKaisuuJoutaiDataId(ctx, typeObj, param);
#endif
    }
}

/* 0x80202108 | size: 0x12C */
/* NAME-T1 fn_80202108 */
s16 fightOutPokemonGetNowKaisuuJoutaiDataId(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
    extern s16 joutaiGetNowKaisuuJoutaiDataId();
    extern s16 fn_8012182C();
    void* originalCtx;
    void* resolved;
    resolved = (originalCtx = ctx);
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return fn_8012182C(resolved, typeObj);
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return -1;
        }
        return joutaiGetNowKaisuuJoutaiDataId(resolved, typeObj);
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return -1;
    }
    return joutaiGetNowKaisuuJoutaiDataId(originalCtx, typeObj);
}

/* 0x80202234 | size: 0x12C */
/* NAME-T1 fn_80202234 */
s16 fightOutPokemonGetKaisuuJoutaiDataId(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
    extern s16 joutaiGetKaisuuJoutaiDataId();
    extern s16 pokemonGetJoutaiKaisuu();
    void* originalCtx;
    void* resolved;
    resolved = (originalCtx = ctx);
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonGetJoutaiKaisuu(resolved, typeObj);
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return -1;
        }
        return joutaiGetKaisuuJoutaiDataId(resolved, typeObj);
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return -1;
    }
    return joutaiGetKaisuuJoutaiDataId(originalCtx, typeObj);
}

/* 0x80202360 | size: 0x12C */
s16 fightOutPokemonGetPokemonJoutaiStack(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
    extern s16 joutaiGetCountAsJoutaiDataId();
    extern s16 pokemonGetJoutaiStack();
    void* originalCtx;
    void* resolved;
    resolved = (originalCtx = ctx);
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonGetJoutaiStack(resolved, typeObj);
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return -1;
        }
        return joutaiGetCountAsJoutaiDataId(resolved, typeObj);
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return -1;
    }
    return joutaiGetCountAsJoutaiDataId(originalCtx, typeObj);
}

/* 0x8020248C | size: 0x12C */
s16 fightOutPokemonWritePokemonJoutaiDataId(void* ctx, void* typeObj, void* inputParam) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 joutaiWriteJoutaiDataId();
#endif
    extern s16 pokemonWriteJoutaiDataId();
    void* originalCtx;
    void* resolved;
    void* resolvedParam;
    void* directParam;
    resolved = (originalCtx = ctx);
    directParam = (resolvedParam = inputParam);
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonWriteJoutaiDataId(resolved, typeObj, resolvedParam);
        } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
            joutaiWriteJoutaiDataId(resolved, PC_PTR32_IMPLICIT(typeObj), PC_PTR32_IMPLICIT(resolvedParam));
            return 0;
#else
            return joutaiWriteJoutaiDataId(resolved, typeObj, resolvedParam);
#endif
        }
    } else if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xD8) {
#ifdef TARGET_PC /* the callee is void (r3 at its return differs by path); no caller uses this function's result (all its C callers are statements): unused, likely */
        joutaiWriteJoutaiDataId(originalCtx, PC_PTR32_IMPLICIT(typeObj), PC_PTR32_IMPLICIT(directParam));
        return 0;
#else
        return joutaiWriteJoutaiDataId(originalCtx, typeObj, directParam);
#endif
    }
}

/* 0x802025B8 | size: 0x12C */
s16 fn_802025B8(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 joutaiCheckWriteJoutaiDataId();
#endif
    extern s16 pokemonCheckWriteJoutaiDataId();
    void* originalCtx;
    void* resolved;
    resolved = (originalCtx = ctx);
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonCheckWriteJoutaiDataId(resolved, typeObj);
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return 0;
        }
        return joutaiCheckWriteJoutaiDataId(resolved, PC_PTR32_IMPLICIT(typeObj));
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return 0;
    }
    return joutaiCheckWriteJoutaiDataId(originalCtx, PC_PTR32_IMPLICIT(typeObj));
}

/* 0x802026E4 | size: 0x12C */
s16 fightOutPokemonIsPokemonJoutaiDataId(void* ctx, void* typeObj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 joutaiDataBiosGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 joutaiIsJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s16 pokemonIsJoutaiDataId();
#endif
    void* originalCtx;
    void* resolved;
    resolved = (originalCtx = ctx);
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8 || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xCD) {
        resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_BATTLE_POKEMON_PTR, 0));
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0x7C || joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) == 0xC8) {
            if (resolved == NULL) {
                resolved = NULL;
            } else {
                resolved = PC_PTR(pokemonGetStatus(resolved, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
            }
            return pokemonIsJoutaiDataId(resolved, PC_PTR32_IMPLICIT(typeObj));
        }
        if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xCD) {
            return 0;
        }
        return joutaiIsJoutaiDataId(resolved, PC_PTR32_IMPLICIT(typeObj));
    }
    if (joutaiDataBiosGetStatus(PC_PTR32_IMPLICIT(typeObj)) != 0xD8) {
        return 0;
    }
    return joutaiIsJoutaiDataId(originalCtx, PC_PTR32_IMPLICIT(typeObj));
}
