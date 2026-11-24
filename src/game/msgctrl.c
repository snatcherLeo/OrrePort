/**
 * @file msgctrl.c
 * @brief Decompiled functions.
 *
 * Address range: 0x80131588 - 0x80132C6C
 *
 * Split out of the former game/effect/effect_util.c CodeCandidate
 * bucket (0x8013151C - 0x80137114); see effect_util_types.h for
 * shared cross-TU declarations.
 */

#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"
#include "pc/disc_types.h" /* DISC_U32: message text is disc data */

/*
 * 0x80131588 - 0x80131690: the unit game/msgctrl.c links these four
 * functions. The TU was built at -O4,p with the peephole pass off (one
 * unit-wide flag in configure.py; see msgctrl_exact_80132A38.c). The rest of
 * the TU is linked through its carves (msgctrl_exact_*.c,
 * msgctrl_candidate_80131714.c, msgctrl_r49_80131A34_suffix.c); the
 * candidate chunks still compile it whole by defining MSGCTRL_WHOLE_TU.
 */

/* 0x80131588 | 0x64 */
u32 msgctrlSndWait(EffectUtilCommandObj* obj) {
    if (obj->activeFlag == 0) {
        u16 handle = _SndID;
        if (handle == 0) {
            return 0;
        }
        if (GSsndGetStatus(handle) == 2) {
            P32_SET(obj->stream, P32(obj->stream) - 3);
        }
    }
    return 1;
}

/* 0x801315EC | 0x44 */
u32 msgctrlSndPlay(EffectUtilCommandObj* obj) {
    if (obj->activeFlag == 0) {
        u16 handle = _SndID;
        if (handle != 0) {
            std104_soundSetBackgroundMusic(handle, 0, 0xFF);
        }
    }
    return 0;
}

/* 0x80131630 | 0x30 -- read a signed byte from the stream into the baseline bias if flag set */
u32 msgctrlBaseLineBias(EffectUtilCommandObj* obj) {
    u8* stream;
    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->field_43 = (s8)*stream;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}

/* 0x80131660 | 0x30 -- read a signed byte from the stream into the line spacing if flag set */
u32 msgctrlLineSpace(EffectUtilCommandObj* obj) {
    u8* stream;
    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->field_42 = (s8)*stream;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}

#if defined(MSGCTRL_WHOLE_TU)
/* 0x80131690 | 16 bytes | set_field_return */
u32 msgctrlIndentOff(EffectUtilCommandObj* obj) {
    obj->field_41 = 1;
    return 0;
}


/* 0x801316A0 | 0x8 | sda_getter */
u16 msgctrlNpc(void) { return _Npc; }


/* 0x801316A8 | 0x28 -- calls fightTrainerKindDataBiosGetPtr with _Tribe then fightTrainerKindDataBiosGetPrefixName */
/* renamed symbols referenced by asm incs (symbolmap port) */
/* Forward declarations for self-referencing asm blocks */
/* _msgctrlSideName__FP15FightOutPokemonUc already declared at top with typed params */





#if 0
asm void msgctrlTribe(void) {
#include "src/game/effect/effect_util_fn_801316A8.inc"
}
#else
u32 msgctrlTribe(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    fightTrainerKindDataBiosGetPtr(_Tribe);
    return fightTrainerKindDataBiosGetPrefixName();
}
#endif


/* 0x801316D0 | 0x8 | sda_getter */
u32 msgctrlString2(void) { return _String2; }


/* 0x801316D8 | 0x8 | sda_getter */
u32 msgctrlMenuMsgID2(void) { return _MenuMsgID2; }


/* 0x801316E0 | 0x8 | sda_getter */
u32 msgctrlMenuMsgID(void) { return _MenuMsgID; }


/* 0x801316E8 | 0x2C -- read byte from stream, store to obj+0x03 if flag clear */
u32 msgctrlTalkSE(EffectUtilCommandObj* obj) {
    u8* stream;
    if (obj->activeFlag == 0) {
        stream = P32(obj->stream);
        obj->field_03 = *stream;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}

#if 0
asm void msgctrlAlign(void) {
#include "src/game/effect/effect_util_fn_80131714.inc"
}
#else
s32 msgctrlAlign(EffectUtilCommandObj* obj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmsgAdjustAlign(void*);
#endif
    u8* stream;
    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->alignMode = stream[0];
        GSmsgAdjustAlign(obj);
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}
#endif


/* 0x80131768 | 0x2C -- read byte from stream, store to obj+0x02 if flag set */
u32 msgctrlShadow(EffectUtilCommandObj* obj) {
    u8* stream;
    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->field_02 = *stream;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}


/* 0x80131794 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80426FF0, 0x10, lbl_8047AE94, 4) */
#if 0
asm void msgctrlMenuMoney(void) {
#include "src/game/effect/effect_util_fn_80131794.inc"
}
#else
u32 msgctrlMenuMoney(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80426FF0, 0x10, lbl_8047AE94, 4));
}
#endif


/* 0x801317C8 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427010, 0x10, lbl_8047AE68, 5) */
#if 0
asm void msgctrlMenuFullDigit(void) {
#include "src/game/effect/effect_util_fn_801317C8.inc"
}
#else
u32 msgctrlMenuFullDigit(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427010, 0x10, lbl_8047AE68, 5));
}
#endif


/* 0x801317FC | 0x28 -- calls pokemonDataBiosGetPtr(_PokemonID) then pokemonDataBiosGetName */
#if 0
asm void msgctrlPokemonID(void) {
#include "src/game/effect/effect_util_fn_801317FC.inc"
}
#else
u32 msgctrlPokemonID(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    pokemonDataBiosGetPtr(_PokemonID);
    return pokemonDataBiosGetName();
}
#endif


/* 0x80131824 | 0x8 | sda_getter */
u32 msgctrlString(void) { return _String; }


/* 0x8013182C | 0x208 */
#if 0
asm void msgctrlTime(void) {
#include "src/game/effect/effect_util_fn_8013182C.inc"
}
#else
u32 msgctrlTime(void) { /* returns its text buffer (the text engine inserts it; addi r3 at the end, assembly) */
    u32 seconds = lbl_8047AE84 / 3600;
    u32 minutes = (lbl_8047AE84 - seconds * 3600) / 60;
    u32 outIndex = 0;
    u16* glyph;

    if (seconds >= 100) {
        glyph = (u16*)_msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, seconds / 100, 0);
        lbl_8047AEA8 = (u32)glyph;
        ((u16*)lbl_80427050)[outIndex++] = glyph[0];
        seconds -= (seconds / 100) * 100;
    }

    glyph = (u16*)_msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, seconds / 10, 0);
    lbl_8047AEA8 = (u32)glyph;
    ((u16*)lbl_80427050)[outIndex++] = glyph[0];

    glyph = (u16*)_msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, seconds - (seconds / 10) * 10, 0);
    lbl_8047AEA8 = (u32)glyph;
    ((u16*)lbl_80427050)[outIndex++] = glyph[0];

#ifdef TARGET_PC
    ((u16*)lbl_80427050)[outIndex++] = (u16)__builtin_bswap16(0x3A); /* PC: message text is read big-endian (A16) */
#else
    ((u16*)lbl_80427050)[outIndex++] = 0x3A;
#endif

    glyph = (u16*)_msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, minutes / 10, 0);
    lbl_8047AEA8 = (u32)glyph;
    ((u16*)lbl_80427050)[outIndex++] = glyph[0];

    glyph = (u16*)_msgctrlMakeDigit__FPUslUll(lbl_80427030, 0x10, minutes - (minutes / 10) * 10, 0);
    lbl_8047AEA8 = (u32)glyph;
    ((u16*)lbl_80427050)[outIndex++] = glyph[0];
    ((u16*)lbl_80427050)[outIndex] = 0;
    return PC_PTR32(lbl_80427050);
}
#endif


/* 0x80131A34 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427070, 0x10, lbl_8047AE80, 4) */
#if 0
asm void msgctrlMoney(void) {
#include "src/game/effect/effect_util_fn_80131A34.inc"
}
#else
u32 msgctrlMoney(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427070, 0x10, lbl_8047AE80, 4));
}
#endif


/* 0x80131A68 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427090, 0x10, lbl_8047AE6C, 2) */
#if 0
asm void msgctrlMenuZDigit2(void) {
#include "src/game/effect/effect_util_fn_80131A68.inc"
}
#else
u32 msgctrlMenuZDigit2(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427090, 0x10, lbl_8047AE6C, 2));
}
#endif


/* 0x80131A9C | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_804270B0, 0x10, lbl_8047AE68, 2) */
#if 0
asm void msgctrlMenuZDigit(void) {
#include "src/game/effect/effect_util_fn_80131A9C.inc"
}
#else
u32 msgctrlMenuZDigit(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804270B0, 0x10, lbl_8047AE68, 2));
}
#endif


/* 0x80131AD0 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_804270D0, 0x10, lbl_8047AE68, 3) */
#if 0
asm void msgctrlMenuHex2(void) {
#include "src/game/effect/effect_util_fn_80131AD0.inc"
}
#else
u32 msgctrlMenuHex2(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804270D0, 0x10, lbl_8047AE68, 3));
}
#endif


/* 0x80131B04 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_804270F0, 0x10, lbl_8047AE68, 3) */
#if 0
asm void msgctrlMenuHex(void) {
#include "src/game/effect/effect_util_fn_80131B04.inc"
}
#else
u32 msgctrlMenuHex(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804270F0, 0x10, lbl_8047AE68, 3));
}
#endif


/* 0x80131B38 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427110, 0x10, lbl_8047AE6C, 1) */
#if 0
asm void msgctrlMenuUDigit2(void) {
#include "src/game/effect/effect_util_fn_80131B38.inc"
}
#else
u32 msgctrlMenuUDigit2(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427110, 0x10, lbl_8047AE6C, 1));
}
#endif


/* 0x80131B6C | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427130, 0x10, lbl_8047AE68, 1) */
#if 0
asm void msgctrlMenuUDigit(void) {
#include "src/game/effect/effect_util_fn_80131B6C.inc"
}
#else
u32 msgctrlMenuUDigit(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427130, 0x10, lbl_8047AE68, 1));
}
#endif


/* 0x80131BA0 | 0x8 | return_const */
u32 msgctrlSeOn(void) { return 0; }


/* 0x80131BA8 | 0x8 | return_const */
u32 msgctrlSeOff(void) { return 0; }


/* 0x80131BB0 | 0x8 | sda_getter */
u32 msgctrlPasoName(void) { return _PASO_NAME; }


/* 0x80131BB8 | 0x8 | sda_getter */
u32 msgctrlItemName(void) { return _ITEM_NAME; }


/* 0x80131BC0 | 0x8 | sda_getter */
u32 msgctrlWazaName(void) { return _WAZA_NAME; }


/* 0x80131BC8 | 0x8 | sda_getter */
u32 msgctrlTrainerClientno(void) { return _TRAINER_CLIENTNO; }


/* 0x80131BD0 | 0x8 | sda_getter */
u32 msgctrlTrainerEnename2(void) { return _TRAINER_ENENAME2; }


/* 0x80131BD8 | 0x8 | sda_getter */
u32 msgctrlTrainerEnename(void) { return _TRAINER_ENENAME; }


/* 0x80131BE0 | 0x8 | sda_getter */
u32 msgctrlTrainerLose(void) { return _TRAINER_LOSE; }


/* 0x80131BE8 | 0x8 | sda_getter */
u32 msgctrlTrainerName(void) { return _TRAINER_NAME; }


/* 0x80131BF0 | 0x8 | sda_getter */
u32 msgctrlTrainerType(void) { return _TRAINER_TYPE; }


/* 0x80131BF8 | 0x28 -- _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMENO, 2) */
#if 0
asm void msgctrlSideDefenceNameno(void) {
#include "src/game/effect/effect_util_fn_80131BF8.inc"
}
#else
u32 msgctrlSideDefenceNameno(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMENO, 2);
}
#endif


/* 0x80131C20 | 0x28 -- _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMEWO, 1) */
#if 0
asm void msgctrlSideDefenceNamewo(void) {
#include "src/game/effect/effect_util_fn_80131C20.inc"
}
#else
u32 msgctrlSideDefenceNamewo(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMEWO, 1);
}
#endif


/* 0x80131C48 | 0x28 -- _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMEHA, 0) */
#if 0
asm void msgctrlSideDefenceNameha(void) {
#include "src/game/effect/effect_util_fn_80131C48.inc"
}
#else
u32 msgctrlSideDefenceNameha(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_DEFENCE_NAMEHA, 0);
}
#endif


/* 0x80131C70 | 0x28 -- _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMENO, 2) */
#if 0
asm void msgctrlSideAttackNameno(void) {
#include "src/game/effect/effect_util_fn_80131C70.inc"
}
#else
u32 msgctrlSideAttackNameno(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMENO, 2);
}
#endif


/* 0x80131C98 | 0x28 -- _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMEWO, 1) */
#if 0
asm void msgctrlSideAttackNamewo(void) {
#include "src/game/effect/effect_util_fn_80131C98.inc"
}
#else
u32 msgctrlSideAttackNamewo(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMEWO, 1);
}
#endif


/* 0x80131CC0 | 0x28 -- _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMEHA, 0) */
#if 0
asm void msgctrlSideAttackNameha(void) {
#include "src/game/effect/effect_util_fn_80131CC0.inc"
}
#else
u32 msgctrlSideAttackNameha(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return _msgctrlSideName__FP15FightOutPokemonUc(_SIDE_ATTACK_NAMEHA, 0);
}
#endif


/* 0x80131CE8 | 0x21C
 * Put the side's trainer name(s) into message values 0x4D/0x57, then fetch
 * the side-name string for this message kind (0, 1, other).
 */
void _msgctrlSideName__FP15FightOutPokemonUc(u32 pokemon, u32 kind) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmsgGetGSchar(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(u32 id, u32 value);
#endif
    extern u8 fightTargetIsHostSide(u32 pokemon, u16 status);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTargetGetPtrAsNowFightType(u32 target, u32 pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F18DC(u32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32 arg, u32 pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32 a, u32 b, u32 id, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightSideGetValidFightTrainerPtr(void* side, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr(void* trainer);
#endif
    extern u8 fightTrainerCheckDoFight(void* trainer);
    u16 status;
    void* side;
    void* ownTrainer;
    void* trainer;
    u16 count;
    u16 i;

    status = fightFloorGetStatus(0, 0, 0x14, 0);
    side = fightTargetGetPtrAsNowFightType(2, pokemon);
    ownTrainer = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, pokemon);
    count = 0;
    for (i = 0; i < 2; i++) {
        trainer = fightSideGetValidFightTrainerPtr(side, i);
        if (trainer != NULL && fightTrainerCheckDoFight(trainer) != 0) {
            if (count == 0) {
                msgctrlSetValue(0x4D, fightTrainerGetNamePtr(trainer));
            } else if (count == 1) {
                msgctrlSetValue(0x57, fightTrainerGetNamePtr(trainer));
            }
            count++;
        }
    }

    if (fn_801F18DC(0) == 1 && ownTrainer != NULL) {
        if (count <= 1) {
            msgctrlSetValue(0x4D, fightTrainerGetNamePtr(ownTrainer));
            if ((u8)kind == 0) {
                GSmsgGetGSchar(0x7722);
                return;
            } else if ((u8)kind == 1) {
                GSmsgGetGSchar(0x7725);
                return;
            } else {
                GSmsgGetGSchar(0x7727);
                return;
            }
        }
        if ((u8)kind == 0) {
            GSmsgGetGSchar(0x7724);
            return;
        } else if ((u8)kind == 1) {
            GSmsgGetGSchar(0x7726);
            return;
        } else {
            GSmsgGetGSchar(0x7728);
            return;
        }
    }

    if (fightTargetIsHostSide(pokemon, status) == 1) {
        if ((u8)kind == 0) {
            GSmsgGetGSchar(0x768A);
            return;
        } else if ((u8)kind == 1) {
            GSmsgGetGSchar(0x768C);
            return;
        } else {
            GSmsgGetGSchar(0x7688);
            return;
        }
    }
    if ((u8)kind == 0) {
        GSmsgGetGSchar(0x7689);
        return;
    } else if ((u8)kind == 1) {
        GSmsgGetGSchar(0x768B);
        return;
    } else {
        GSmsgGetGSchar(0x7687);
        return;
    }
}


/* 0x80131F04 | 0x98 */
#if 0
asm void msgctrlClientnowork(void) {
#include "src/game/effect/effect_util_fn_80131F04.inc"
}
#else
u32 msgctrlClientnowork(void) { /* the text engine uses the result (msgctrlcode mode 1): each branch ends in the call whose r3 is returned (assembly) */
    extern u32 _CLIENTNOWORK;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F18DC(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32);
#endif
    int new_var;
    u32 result;
    u32 val = _CLIENTNOWORK;
    result = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, val);
    new_var = 0;
    if ((((u8) ((new_var, fn_801F18DC(0)))) == 1) && (result != 0)) {
        msgctrlSetValue(0x4D, fightTrainerGetNamePtr(result));
        msgctrlSetValue(0x57, fightOutPokemonGetNicknamePtr(val));
        return PC_PTR32(GSmsgGetGSchar(0x7721));
    } else {
        return fightOutPokemonGetNicknamePtr(val);
    }
}
#endif


/* 0x80131F9C | 0x8 | sda_getter */
u32 msgctrlSpeabiNamet(void) { return _SPEABI_NAMET; }


/* 0x80131FA4 | 0x8 | sda_getter */
u32 msgctrlSpeabiNamec(void) { return lbl_8047AE08; }


/* 0x80131FAC | 0x8 | sda_getter */
u32 msgctrlSpeabiNamed(void) { return lbl_8047AE04; }


/* 0x80131FB4 | 0x8 | sda_getter */
u32 msgctrlSpeabiNamea(void) { return lbl_8047AE00; }


/* 0x80131FBC | 0x8 | sda_getter */
u32 msgctrlEnemyTmons2(void) { return lbl_8047ADFC; }


/* 0x80131FC4 | 0x8 | sda_getter */
u32 msgctrlEnemyTmons(void) { return lbl_8047ADF8; }


/* 0x80131FCC | 0x8 | sda_getter */
u32 msgctrlEnemyMons2(void) { return lbl_8047ADF4; }


/* 0x80131FD4 | 0x8 | sda_getter */
u32 msgctrlEnemyMons(void) { return lbl_8047ADF0; }


/* 0x80131FDC | 0x8 | sda_getter */
u32 msgctrlMyMons2(void) { return lbl_8047ADEC; }


/* 0x80131FE4 | 0x8 | sda_getter */
u32 msgctrlMyMons(void) { return lbl_8047ADE8; }


/* 0x80131FEC | 0x8 | sda_getter */
u32 msgctrlMyName(void) { return lbl_8047ADE4; }


/* 0x80131FF4 | 0x98 */
#if 0
asm void msgctrlTsuikaMons(void) {
#include "src/game/effect/effect_util_fn_80131FF4.inc"
}
#else
u32 msgctrlTsuikaMons(void) { /* the text engine uses the result (msgctrlcode mode 1): each branch ends in the call whose r3 is returned (assembly) */
    extern u32 _TSUIKA_MONS;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32, u32);
#endif
    u32 result;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F18DC(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr(u32);
#endif
    u32 new_var;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32);
#endif
    u32 val = _TSUIKA_MONS;
    result = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, val);
    new_var = val;
    if ((((u8) fn_801F18DC(0)) == 1) && (result != 0)) {
        msgctrlSetValue(0x4D, fightTrainerGetNamePtr(result));
        msgctrlSetValue(0x57, fightOutPokemonGetNicknamePtr(new_var));
        do { return PC_PTR32(GSmsgGetGSchar(0x7721)); } while (0);
    } else {
        return fightOutPokemonGetNicknamePtr(val);
    }
}
#endif


/* 0x8013208C | 0x98 */
#if 0
asm void msgctrlClientMos(void) {
#include "src/game/effect/effect_util_fn_8013208C.inc"
}
#else
u32 msgctrlClientMos(void) { /* the text engine uses the result (msgctrlcode mode 1): each branch ends in the call whose r3 is returned (assembly) */
    extern u32 _CLIENT_MONS;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F18DC(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32);
#endif
    u32 result;
    u32 val = _CLIENT_MONS;
    u32 new_var;
    result = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, val);
    new_var = val;
    if ((((u8) fn_801F18DC(0)) == 1) && (result != 0)) {
        msgctrlSetValue(0x4D, fightTrainerGetNamePtr(result));
        msgctrlSetValue(0x57, fightOutPokemonGetNicknamePtr(new_var));
        return PC_PTR32(GSmsgGetGSchar(0x7721));
    } else {
        return fightOutPokemonGetNicknamePtr(val);
    }
}
#endif


/* 0x80132124 | 0x98 */
#if 0
asm void msgctrlDeffenceMons(void) {
#include "src/game/effect/effect_util_fn_80132124.inc"
}
#else
u32 msgctrlDeffenceMons(void) { /* the text engine uses the result (msgctrlcode mode 1): each branch ends in the call whose r3 is returned (assembly) */
    extern u32 _DEFENCE_MONS;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F18DC(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32);
#endif
    u32 result;
    u32 val = _DEFENCE_MONS;
    u32 new_var;
    result = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, val);
    new_var = val;
    if ((((u8) fn_801F18DC(0)) == 1) && (result != 0)) {
        msgctrlSetValue(0x4D, fightTrainerGetNamePtr(result));
        msgctrlSetValue(0x57, fightOutPokemonGetNicknamePtr(new_var));
        return PC_PTR32(GSmsgGetGSchar(0x7721));
    } else {
        return fightOutPokemonGetNicknamePtr(val);
    }
}
#endif


/* 0x801321BC | 0x98 */
#if 0
asm void msgctrlAttackMons(void) {
#include "src/game/effect/effect_util_fn_801321BC.inc"
}
#else
u32 msgctrlAttackMons(void) { /* the text engine uses the result (msgctrlcode mode 1): each branch ends in the call whose r3 is returned (assembly) */
    extern u32 _ATTACK_MONS;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_801F18DC(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetNamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32);
#endif
    u32 result;
    u32 new_var;
    u32 val = _ATTACK_MONS;
    result = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, val);
    new_var = val;
    if ((((u8) fn_801F18DC(0)) == 1) && (result != 0)) {
        msgctrlSetValue(0x4D, fightTrainerGetNamePtr(result));
        msgctrlSetValue(0x57, fightOutPokemonGetNicknamePtr(new_var));
        return PC_PTR32(GSmsgGetGSchar(0x7721));
    } else {
        return fightOutPokemonGetNicknamePtr(new_var);
    }
}
#endif


/* 0x80132254 | 0x8 | sda_getter */
u32 msgctrlEvStrBuf2(void) { return lbl_8047ADD0; }


/* 0x8013225C | 0x8 | sda_getter */
u32 msgctrlEvStrBuf1(void) { return lbl_8047ADCC; }


/* 0x80132264 | 0x8 | sda_getter */
u32 msgctrlEvStrBuf0(void) { return lbl_8047ADC8; }


/* 0x8013226C | 0x28 -- calls wazaDataBiosGetPtr(_Waza) then wazaDataBiosGetName */
#if 0
asm void msgctrlWaza(void) {
#include "src/game/effect/effect_util_fn_8013226C.inc"
}
#else
u32 msgctrlWaza(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    wazaDataBiosGetPtr(_Waza);
    return wazaDataBiosGetName();
}
#endif


/* 0x80132294 | 0x8 | sda_getter */
u32 msgctrlMenuMsg2(void) { return _MenuMsg2; }


/* 0x8013229C | 0x8 | sda_getter */
u32 msgctrlMenuMsg(void) { return _MenuMsg; }


/* 0x801322A4 | 0x8 | sda_getter */
u32 msgctrlMenuPokemon(void) { return _MenuPokemon; }


/* 0x801322AC | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427150, 0x10, lbl_8047AE6C, 0) */
#if 0
asm void msgctrlMenuDigit2(void) {
#include "src/game/effect/effect_util_fn_801322AC.inc"
}
#else
u32 msgctrlMenuDigit2(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427150, 0x10, lbl_8047AE6C, 0));
}
#endif


/* 0x801322E0 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427170, 0x10, lbl_8047AE68, 0) */
#if 0
asm void msgctrlMenuDigit(void) {
#include "src/game/effect/effect_util_fn_801322E0.inc"
}
#else
u32 msgctrlMenuDigit(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427170, 0x10, lbl_8047AE68, 0));
}
#endif


/* 0x80132314 | 0x8 | sda_getter */
u32 msgctrlPokemon2(void) { return _Pokemon2; }


/* 0x8013231C | 0x8 | sda_getter */
u32 msgctrlPokemon(void) { return _Pokemon; }


/* 0x80132324 | 0x8 | sda_getter */
u32 msgctrlMsgID(void) { return lbl_8047AE5C; }


/* 0x8013232C | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_80427190, 0x10, lbl_8047AE58, 0) */
#if 0
asm void msgctrlDigit2(void) {
#include "src/game/effect/effect_util_fn_8013232C.inc"
}
#else
u32 msgctrlDigit2(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427190, 0x10, lbl_8047AE58, 0));
}
#endif


/* 0x80132360 | 0x34 -- _msgctrlMakeDigit__FPUslUll(lbl_804271B0, 0x10, lbl_8047AE54, 0) */
#if 0
asm void msgctrlDigit(void) {
#include "src/game/effect/effect_util_fn_80132360.inc"
}
#else
u32 msgctrlDigit(void) { /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804271B0, 0x10, lbl_8047AE54, 0));
}
#endif


/* 0x80132394 | 0x34 -- calls itemDataBiosGetPtr(_Item2) then itemDataBiosGetName, default to 0x2B6E */
u32 msgctrlItem2(void) {
    u32 result;
    result = itemDataBiosGetName(itemDataBiosGetPtr(_Item2));
    if (result == 0) { result = 0x2B6E; }
    return result;
}


/* 0x801323C8 | 0x34 -- calls itemDataBiosGetPtr(_Item) then itemDataBiosGetName, default to 0x2B6E */
u32 msgctrlItem(void) {
    u32 result;
    result = itemDataBiosGetName(itemDataBiosGetPtr(_Item));
    if (result == 0) { result = 0x2B6E; }
    return result;
}


/* 0x801323FC | 0x2C -- savedataGetStatus(0, 2) then heroBiosGetHizukiNamePtr */
u32 msgctrlHizuki(void) {
    savedataGetStatus(0, 2);
    return heroBiosGetHizukiNamePtr();
}


/* 0x80132428 | 0x2C -- savedataGetStatus(0, 2) then heroBiosGetNamePtr */
u32 msgctrlHero(void) {
    savedataGetStatus(0, 2);
    return heroBiosGetNamePtr();
}

#if 0
asm void msgctrlWait(void) {
#include "src/game/effect/effect_util_fn_80132454.inc"
}
#else
s32 msgctrlWait(EffectUtilCommandObj* obj) {
    u8* stream;
    s32 counter;
    if (obj->activeFlag != 0) {
        goto doneIncrement;
    }
    if (obj->waitCounter == 0) {
        stream = P32(obj->stream);
        obj->waitCounter = (s16)((s16)stream[0] + 1);
    }
    counter = obj->waitCounter;
    if ((obj->waitCounter = --counter) <= 0) {
        obj->waitCounter = 0;
        goto doneIncrement;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream - 3);
    return 1;
doneIncrement:
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}
#endif


/* 0x801324CC | 0xA4 -- read color index from stream, look up RGBA, apply */
void msgctrlPalette(EffectUtilCommandObj* obj) {
    extern u32* lbl_80478E88;
    extern u32 lbl_80478E8C;
    extern void GSmsgSetColor(void*);
    u8* stream;
    u8 idx;
    u8* colorPtr;
    u32 color;
    u32 maxIdx;

    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        idx = *stream;
        maxIdx = *lbl_80478E88;
        if (idx >= maxIdx) {
            idx = 0;
        }
        colorPtr = (u8*)(lbl_80478E8C + (u32)idx * 4);
        color = (u32)colorPtr[0] << 24;
        color |= (u32)colorPtr[1] << 16;
        color |= (u32)colorPtr[2] << 8;
        color |= colorPtr[3];
        obj->colorRgba = color;
        GSmsgSetColor(obj);
    }
    /* Advance stream pointer */
    P32_SET(obj->stream, P32(obj->stream) + 1);
}

#if 0
asm void msgctrlColor(void) {
#include "src/game/effect/effect_util_fn_80132570.inc"
}
#else
s32 msgctrlColor(EffectUtilCommandObj* obj) {
    extern void GSmsgSetColor(void*);
    u8* stream;
    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->colorRgba = DISC_U32(stream); /* the RGBA bytes follow the code in the big-endian text */
        GSmsgSetColor(obj);
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 4);
    return 0;
}
#endif


/* 0x801325C4 | 0x68 -- read byte command into u16 field and apply */
s32 msgctrlFont(EffectUtilCommandObj* obj) {
    u8* stream;

    if (obj->activeFlag == 0) {
        stream = P32(obj->stream);
        obj->commandValue = stream[0];
        GSmsgSetFontInfo(obj);
    } else {
        stream = P32(obj->stream);
        obj->commandValue = stream[0];
        GSmsgSetFontInfo(obj);
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}


/* 0x8013262C | 16 bytes | set_field_return */
u32 msgctrlRubyEnd(EffectUtilCommandObj* obj) {
    obj->field_4B = 0;
    return 0;
}


/* 0x8013263C | 16 bytes | set_field_return */
u32 msgctrlRubyTop(EffectUtilCommandObj* obj) {
    obj->field_4B = 2;
    return 0;
}

#if 0
asm void msgctrlRubyStart(void) {
#include "src/game/effect/effect_util_fn_8013264C.inc"
}
#else
s32 msgctrlRubyStart(EffectUtilCommandObj* obj) {
    extern void GSmsgInitRuby(void*);
    if (obj->activeFlag != 0) {
        GSmsgInitRuby(obj);
    }
    obj->field_4B = 1;
    return 0;
}
#endif


/* 0x80132690 | 0xCC -- effect tick with flag-based logic */
u32 msgctrlKeyWait(EffectUtilCommandObj* obj) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE(u32);
#endif
    u8* stream;
    u8 done;

    if (obj->flags & 0x02) {
        obj->pendingFlag = 1;
    }
    if ((u32)(menuIsCheck(0x0A) & 0xFF) != 0) {
        obj->pendingFlag = 0;
    }

    if (obj->activeFlag == 0) {
        if (obj->pendingFlag != 0) {
            obj->pendingFlag = 0;
            P32_SET(obj->savedStream, P32(obj->stream));
            if ((obj->flags & 0x02) == 0) {
                soundStartSE(0x24);
            }
        } else {
            stream = P32(obj->stream);
            done = 1;
            P32_SET(obj->stream, stream - 3);
            obj->doneFlag = done;
        }
    } else {
        obj->field_0C = obj->field_04;
        obj->field_10 = obj->field_08;
    }
    return 1;
}


/* 0x8013275C | 0x84 */
#if 0
asm void msgctrlKeyEnd(void) {
#include "src/game/effect/effect_util_fn_8013275C.inc"
}
#else
u32 msgctrlKeyEnd(EffectUtilCommandObj* obj) {
    if (obj->flags & 0x02) {
        obj->pendingFlag = 1;
    }
    if ((u32)(menuIsCheck(0x0A) & 0xFF) != 0) {
        obj->pendingFlag = 0;
    }
    if (obj->pendingFlag != 0) {
        obj->pendingFlag = 0;
    } else {
        P32_SET(obj->stream, P32(obj->stream) - 3);
    }
    return 1;
}
#endif

#if 0
asm void msgctrlCR(void) {
#include "src/game/effect/effect_util_fn_801327E0.inc"
}
#else
u32 msgctrlCR(void* obj) {
    u8* p = (u8*)obj;
    f32 diff;
    *(f32*)(p + 0x0C) = *(f32*)(p + 0x04);
    diff = (f32)((s32)(u8)p[0x23] + (s32)(s8)p[0x42]);
    *(f32*)(p + 0x10) += *(f32*)(p + 0x64) * diff;
    return 0;
}
#endif


/* 0x80132834 - 0x80132A38: shared with the standalone matching owner. */
#include "src/game/msgctrl_candidate_80132834.c"


/* 0x80132A38 - 0x80132C6C: msgctrlSetValue and msgctrlInitValue are in
 * msgctrl_exact_80132A38.c. */
#endif /* MSGCTRL_WHOLE_TU */
