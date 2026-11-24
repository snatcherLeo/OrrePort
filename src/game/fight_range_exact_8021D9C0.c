/**
 * @file fight_range_exact_8021D9C0.c
 * @brief Exact pure-C fight-sequence island, 0x8021D9C0 - 0x8021FAD4.
 */
#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/conditions.h"
#include "game/constants/pokemon_status.h"
#include "game/constants/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC */

extern u8* lbl_8047B610;
extern u8 lbl_80478D78[1];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaSequenceOwnerLoadWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceSysStartWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetIrekaeFightPokemon();
#endif
extern void* lbl_8047B64C;
extern u8 lbl_80478278[0x10];
extern u32 ServerStatusFlag;
extern u8 lbl_8047B642;
extern u16 lbl_8047B61C;

extern void wazaSequenceOwnerClearState(void* effect, s32 trajType);
extern void fightMenuSubMenuLvupStatus(s16* current, s16* previous, s16* out);
extern void fightMenuOpenLevelUpStatusMenu(u8* dst, u8 value);
extern void fightApplyDamageVariance(u8 r3, u8 r4);
extern void fn_80265598(u32 r3, u32 r4, u32 r5);
extern void fightOutPokemonDamageEffect(void* ctx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightMenuStatusMenuClose(u32 r3, u32 r4, u8 r5);
#endif
extern u32 soundStart(u32 id, u32 arg1, u32 volume, u32 arg3);
extern u32 std130_soundUnk_801653cc(u32 id, void* buffer, u32 size);
extern void fightTrainerBallThrowEffect(void* context, void* trainerCtx, u8 mode);
extern void fightOutPokemonDasuEffect(void* r3, u8 r4);
/* RULE-EXCEPTION(user-approved): caller-side prototype returning s32 (the definitions return u8); retail tests the result with a signed compare (cmpwi r3,0; ble) - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightOutPokemonGetJoutaiMigawariHp(void* trainer);
#endif
extern void fightMenuCloseMsg(void);
/* RULE-EXCEPTION(user-approved): caller-side prototype with the five arguments retail passes (li r5-r7 before the bl); fight_waza.c defines fightWazaWzxTypeFuncMigawari with the two it reads - see docs/RULE_EXCEPTIONS.md */
extern void fightWazaWzxTypeFuncMigawari(u32 wazaId, void* ctx, void* target, u32 arg3, u32 arg4);
extern u8 wazaSequenceSysIsAllEnd(s32 timeType);
extern void _threadSwitch(void);
extern void fightOutPokemonFreeAllSequenceWaza(void* ctx);
extern void fightOutPokemonSetOnZukanFlag(void* ctx, u32 param);
extern void fightOutPokemonSetOnDarkPokemonFlag(void* ctx, u32 param);
extern void fightOutPokemonInit(int r3);
extern void fightOutPokemonBiosCopy(struct FightOutPokemon* dst, struct FightOutPokemon* src);
extern u32 fightOutPokemonCheckFightOut(void* r3);
extern void fightOutPokemonWriteJoutaiDataId(void* ctx, void* typeObj);
extern void fightOutPokemonModosuEffect(void* r3, u8 r4);

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd74(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsGcHeroFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F18DC();
#endif
    extern u32 fightTrainerGetKindPrefixName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetNicknamePtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetNamePtr();
#endif
    extern s32 fightFloorGetEnemyFightSideManyDamageHpRate();
    u32 ctx1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    u32 obj1 = PC_PTR32(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, ctx1));
    u32 slotActive = ((u8)fightOutPokemonIsGcHeroFightOutPokemon(ctx1) == 1);
    u32 canTrigger = ((u8)fn_801F18DC(0) == 1);
    s32 ret8;
    if ((u32)fightTrainerGetKindPrefixName(obj1) == 0 && (u8)slotActive == 0) {
        canTrigger = 1;
    }
    msgctrlSetValue(0x16, PC_PTR((s32)PC_PTR32(fightOutPokemonGetNicknamePtr(ctx1))));
    msgctrlSetValue(0xd, PC_PTR((s32)PC_PTR32(fightOutPokemonGetNicknamePtr(ctx1))));
    msgctrlSetValue(0x22, (s32)fightTrainerGetKindPrefixName(obj1));
    msgctrlSetValue(0x23, PC_PTR((s32)PC_PTR32(fightTrainerGetNamePtr(obj1))));
    msgctrlSetValue(0x25, PC_PTR((s32)PC_PTR32(fightTrainerGetNamePtr(obj1))));
    if ((u8)canTrigger == 1) {
        lbl_80478D78[5] = 5;
    } else if ((u8)slotActive == 1) {
        ret8 = fightFloorGetEnemyFightSideManyDamageHpRate(0, ctx1);
        if (ret8 < 0) {
            lbl_80478D78[5] = 1;
        } else if (ret8 == 0) {
            lbl_80478D78[5] = 0;
        } else if (ret8 < 0x1e) {
            lbl_80478D78[5] = 1;
        } else if (ret8 < 0x46) {
            lbl_80478D78[5] = 2;
        } else {
            lbl_80478D78[5] = 3;
        }
    } else {
        lbl_80478D78[5] = 4;
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset
#pragma optimize_for_size on
void wsCmd73(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsGcHeroFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F18DC();
#endif
    extern u32 fightTrainerGetKindPrefixName();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetNicknamePtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetNamePtr();
#endif
    extern s32 fightFloorGetEnemyFightSideLittleNokoriHpRate();
    u32 ctx1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    u32 obj1 = PC_PTR32(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, ctx1));
    u32 slotActive = ((u8)fightOutPokemonIsGcHeroFightOutPokemon(ctx1) == 1);
    u32 canTrigger = ((u8)fn_801F18DC(0) == 1);
    s32 ret8;
    if ((u32)fightTrainerGetKindPrefixName(obj1) == 0 && (u8)slotActive == 0) {
        canTrigger = 1;
    }
    msgctrlSetValue(0x16, PC_PTR((s32)PC_PTR32(fightOutPokemonGetNicknamePtr(ctx1))));
    msgctrlSetValue(0xd, PC_PTR((s32)PC_PTR32(fightOutPokemonGetNicknamePtr(ctx1))));
    msgctrlSetValue(0x22, (s32)fightTrainerGetKindPrefixName(obj1));
    msgctrlSetValue(0x23, PC_PTR((s32)PC_PTR32(fightTrainerGetNamePtr(obj1))));
    msgctrlSetValue(0x25, PC_PTR((s32)PC_PTR32(fightTrainerGetNamePtr(obj1))));
    if ((u8)canTrigger == 1) {
        lbl_80478D78[5] = 5;
    } else if ((u8)slotActive == 1) {
        ret8 = fightFloorGetEnemyFightSideLittleNokoriHpRate(0, ctx1);
        if (ret8 >= 0x46 || ret8 < 0) {
            lbl_80478D78[5] = 0;
        } else if (ret8 >= 0x28) {
            lbl_80478D78[5] = 1;
        } else if (ret8 >= 0xa) {
            lbl_80478D78[5] = 2;
        } else {
            lbl_80478D78[5] = 3;
        }
    } else {
        lbl_80478D78[5] = 4;
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}

#pragma optimize_for_size reset
u8* wsCmd72(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 5;
    return pc;
}

void wsCmd70(void) {
    lbl_8047B610++;
}

void wsCmd6F(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTargetGetPtrAsNowFightType();
#endif
    extern void fightOutPokemonSetVisibility();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus();
#endif
    void* obj = (void*)fightTargetGetPtrAsNowFightType(lbl_8047B610[1], 0);
    if (obj != ((void*)0)) {
        void* p;
        fightOutPokemonSetVisibility(obj, 1);
        p = PC_PTR(pokemonGetStatus(obj, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
        if (p != ((void*)0)) {
            wazaSequenceOwnerClearState(p, 3);
        }
    }
    lbl_8047B610 += 2;
}

void wsCmd6E(void) {
    lbl_8047B610++;
}

void wsCmd6D(void) {
    lbl_8047B610++;
}

void wsCmd6C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightPokemonToMenuLvupStatus();
#endif
    extern void fightMenuCloseLevelUpStatusMenu();
    u8 buf1[24];
    u8 buf2[16];
    fightPokemonToMenuLvupStatus(lbl_8047B64C, buf1);
    fightMenuSubMenuLvupStatus((s16*)buf1, (s16*)lbl_80478278, (s16*)buf2);
    fightMenuOpenLevelUpStatusMenu(buf2, 1);
    fightMenuOpenLevelUpStatusMenu(buf1, 0);
    fightMenuCloseLevelUpStatusMenu();
    lbl_8047B610 += 1;
}

void wsCmd6B(void) {
    lbl_8047B610++;
}

void wsCmd6A(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonDoItemSoubi();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff(void* trainer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId(void* trainer, u32 eventId, u32 param1,
                                                        u32 param2);
#endif
    void* ctx = (void*)fightTargetGetPtrAsNowFightType(lbl_8047B610[1], 0);
    fightOutPokemonDoItemSoubi(ctx, 0, 0, 1);
    if (fightOutPokemonIsUseHensinBuff(ctx) == 1) {
        fightOutPokemonSetHensinPokemonStatusId(ctx, 0x82, 0, 0);
    }
    lbl_8047B610 += 2;
}

void WS_KORAERU_CHECK(void) {
    fightApplyDamageVariance(1, 0);
    lbl_8047B610 = lbl_8047B610 + 1;
}

void WS_SCA_END_SET(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8021DF3C();
#endif
    fightFloorLoopValidFightOutPokemon(0, fn_8021DF3C, 0, 0);
    lbl_8047B610 = lbl_8047B610 + 1;
    return;
}

s32 fn_8021DF3C(void* arg) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonSetStatus();
#endif
    pokemonSetStatus(arg, 0, 0x112, 0, 1);
    return 1;
}

void wsCmd67(void) {
    lbl_8047B610++;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd66(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    u8 slot = *(u8*)(lbl_8047B610 + 1);
    u16 cand;
    u32 fieldEE;
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(slot, 0));
    u32 operand = DISC_U32(lbl_8047B610 + 3);
    if ((ServerStatusFlag & 0x80) != 0) {
        goto skip;
    }
    if (operand != 7) {
        goto skip;
    }
    cand = fightFloorGetStatus(0, 0, 0x14, 0);
    fieldEE = pokemonGetStatus(PC_PTR(ctx), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
    if (fieldEE == 0) {
        goto skip;
    }
    if (wazaSequenceOwnerLoadWaza(PC_PTR(fieldEE), 0x34, 4, 0) == 0) {
        goto skip;
    }
    wazaSequenceSysStartWaza(PC_PTR(fieldEE), 0x34, 4);
    fn_80265598(ctx, cand, 1);
skip:
    lbl_8047B610 = lbl_8047B610 + 7;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset
#pragma optimize_for_size on
void wsCmd65(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysStartWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 wazaSequenceOwnerLoadWaza();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80265598();
#endif
    extern u32 ServerStatusFlag;
    u16 uVar3;
    int iVar2;
    u8 cVar4;
    int iVar7;
    u16 uVar6;
    u32 iVar5;
    u32 uVar1;
    fightFloorGetStatus(0, 0, 0x14, 0);
    uVar1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    iVar5 = DISC_U32(lbl_8047B610 + 2);
    if ((ServerStatusFlag & 0x80) == 0) {
        if (iVar5 == 9) {
            uVar6 = fightFloorGetStatus(0, 0, 0x14, 0);
            iVar7 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
            if ((iVar7 != 0) && (cVar4 = wazaSequenceOwnerLoadWaza(PC_PTR(iVar7), 0x34, 4, 0), cVar4 != 0)) {
                wazaSequenceSysStartWaza(PC_PTR(iVar7), 0x34, 4);
                fn_80265598(uVar1, uVar6, 1);
            }
        }
        if (iVar5 == 0x18) {
            uVar3 = fightFloorGetStatus(0, 0, 0x14, 0);
            iVar2 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
            if ((iVar2 != 0) && (cVar4 = wazaSequenceOwnerLoadWaza(PC_PTR(iVar2), 0x35, 4, 0), cVar4 != 0)) {
                wazaSequenceSysStartWaza(PC_PTR(iVar2), 0x35, 4);
                fn_80265598(uVar1, uVar3, 1);
            }
        }
        if (iVar5 == 0x17) {
            uVar3 = fightFloorGetStatus(0, 0, 0x14, 0);
            iVar2 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
            if ((iVar2 != 0) && (cVar4 = wazaSequenceOwnerLoadWaza(PC_PTR(iVar2), 0x36, 4, 0), cVar4 != 0)) {
                wazaSequenceSysStartWaza(PC_PTR(iVar2), 0x36, 4);
                fn_80265598(uVar1, uVar3, 1);
            }
        }
        if (iVar5 == 10) {
            uVar3 = fightFloorGetStatus(0, 0, 0x14, 0);
            iVar5 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
            if ((iVar5 != 0) && (cVar4 = wazaSequenceOwnerLoadWaza(PC_PTR(iVar5), 0x37, 4, 0), cVar4 != 0)) {
                wazaSequenceSysStartWaza(PC_PTR(iVar5), 0x37, 4);
                fn_80265598(uVar1, uVar3, 1);
            }
        }
    }
    lbl_8047B610 = lbl_8047B610 + 6;
    return;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset
#pragma optimize_for_size on
void wsCmd64(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32 a, u32 b, u32 c, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus(void* context, u32 slot, u16 tableId, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId(void* ctx, void* typeObj);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaSequenceOwnerLoadWaza(void* ptr, u32 field, u32 size, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSequenceSysStartWaza(void* ptr, u32 field, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80265598();
#endif
    void* ctx;
    u8* pc;
    fightFloorGetStatus(0, 0, 0x14, 0);
    pc = (u8*)lbl_8047B610;
    ctx = (void*)fightTargetGetPtrAsNowFightType(pc[1], 0);
    if ((ServerStatusFlag & 0x80) == 0) {
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, (void*)CONDITION_SLEEP) == 1) {
            u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
            void* poke = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (poke != 0) {
                if (wazaSequenceOwnerLoadWaza(poke, 0x2e, 4, 0) != 0) {
                    wazaSequenceSysStartWaza(poke, 0x2e, 4);
                    fn_80265598(PC_PTR32_IMPLICIT(ctx), val, 1);
                }
            }
        }
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, (void*)CONDITION_PARALYSIS) == 1) {
            u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
            void* poke = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (poke != 0) {
                if (wazaSequenceOwnerLoadWaza(poke, 0x2f, 4, 0) != 0) {
                    wazaSequenceSysStartWaza(poke, 0x2f, 4);
                    fn_80265598(PC_PTR32_IMPLICIT(ctx), val, 1);
                }
            }
        }
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, (void*)CONDITION_FREEZE) == 1) {
            u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
            void* poke = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (poke != 0) {
                if (wazaSequenceOwnerLoadWaza(poke, 0x30, 4, 0) != 0) {
                    wazaSequenceSysStartWaza(poke, 0x30, 4);
                    fn_80265598(PC_PTR32_IMPLICIT(ctx), val, 1);
                }
            }
        }
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, (void*)CONDITION_BURN) == 1) {
            u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
            void* poke = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (poke != 0) {
                if (wazaSequenceOwnerLoadWaza(poke, 0x31, 4, 0) != 0) {
                    wazaSequenceSysStartWaza(poke, 0x31, 4);
                    fn_80265598(PC_PTR32_IMPLICIT(ctx), val, 1);
                }
            }
        }
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, (void*)CONDITION_POISON) == 1) {
            u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
            void* poke = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (poke != 0) {
                if (wazaSequenceOwnerLoadWaza(poke, 0x32, 4, 0) != 0) {
                    wazaSequenceSysStartWaza(poke, 0x32, 4);
                    fn_80265598(PC_PTR32_IMPLICIT(ctx), val, 1);
                }
            }
        }
        if (fightOutPokemonIsPokemonJoutaiDataId(ctx, (void*)CONDITION_BAD_POISON) == 1) {
            u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
            void* poke = PC_PTR(pokemonGetStatus(ctx, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
            if (poke != 0) {
                if (wazaSequenceOwnerLoadWaza(poke, 0x33, 4, 0) != 0) {
                    wazaSequenceSysStartWaza(poke, 0x33, 4);
                    fn_80265598(PC_PTR32_IMPLICIT(ctx), val, 1);
                }
            }
        }
    }
    lbl_8047B610 += 2;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on; opt_propagation off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset
#pragma optimize_for_size on
#pragma opt_propagation off
void wsCmd63(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightWazaSetUseWazaStatus();
#endif
    extern u16 lbl_8047B60C;
    extern u32 lbl_80379BFF[];
    u32 uVar2;
    u32 uVar1;
    u16 uVar3;
    u16 statusValue;
    fightFloorGetStatus(0, 0, 0x14, 0);
    uVar2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    uVar2 = (int)pokemonGetStatus(PC_PTR(uVar2), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    statusValue = lbl_8047B60C;
    uVar1 = statusValue;
    uVar3 = wazaGetStatus(0, uVar1, 9, 0);
    if (*(char*)(lbl_8047B610 + 1) == 0) {
        goto zero_case;
    }
    fightWazaSetUseWazaStatus(uVar2, uVar1);
    goto done;
zero_case:
    wazaSetStatus(PC_PTR(uVar2), 0, 0x27, 0, uVar1 & 0xffff);
    fightWazaSetUseWazaStatus(uVar2, uVar1);
done:
    lbl_8047B610 = (u8*)PC_PTR(DISC_U32(&lbl_80379BFF[((s16)uVar3) & 0xffff]));
    return;
}

#pragma optimize_for_size reset
#pragma opt_propagation reset
u8* wsCmd62(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 2;
    return pc;
}

u8* wsCmd61(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 2;
    return pc;
}

u8* wsCmd60(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 2;
    return pc;
}

void WS_SWITCH_A_D(void) {
    extern void fightFloorReplaceFightOutPokemonAttackToDefense();
    extern u32 ServerStatusFlag;
    u32 uVar1;
    u32 uVar2;
    fightFloorReplaceFightOutPokemonAttackToDefense(0);
    uVar1 = ServerStatusFlag;
    uVar2 = uVar1 | 0x1000;
    if ((uVar1 & 0x1000) != 0) {
        uVar2 = uVar1 & 0xffffefff;
    }
    lbl_8047B610 = lbl_8047B610 + 1;
    ServerStatusFlag = uVar2;
    return;
}

u8* wsCmd5E(void) {
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 2;
    return pc;
}

void wsCmd5D(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetGcHeroFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus(u32, u32, u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* fightTrainerKindDataBiosGetPtr(u16 idx);
#endif
    extern u16 fightTrainerKindDataBiosGetSyoukinBairitu(u8 * ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightPokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 figthPokemonGetLevel();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    extern void scriptAddPremium();
    extern void heroAddPokedoru();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus();
#endif
    u32 ctx1;
    u32 field44;
    u8 field48;
    u32 best;
    u16 a26;
    u32 v1;
    u32 v2;
    u32 amount;
    u32 ctx2;
    s32 i;
    u16 t43;
    u16 fieldVal;
    u32 t;
    u16 mult;
    a26 = (u16)fightFloorGetStatus(0, 0, 0x18, 0);
    if ((u8)fightFloorGetStatus(0, 0, 0x25, 0) == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }
    ctx1 = fightFloorGetGcHeroFightTrainerPtr(0);
    if (ctx1 == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }
    field44 = fightTrainerGetStatus(PC_PTR(ctx1), 0, 0x44, 0);
    if (field44 == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }
    field48 = (u8)fightTrainerGetStatus(PC_PTR(ctx1), 0, 0x48, 0);
    ctx2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(9, ctx1));
    t43 = (u16)fightTrainerGetStatus(PC_PTR(ctx2), 0, 0x43, 0);
    fieldVal = fightTrainerKindDataBiosGetSyoukinBairitu(
        fightTrainerKindDataBiosGetPtr((u16)fightTrainerGetStatus(0, t43, 4, 0)));
    best = 0;
    for (i = 0; i < 6; i++) {
        v1 = fightTrainerGetStatus(PC_PTR(ctx2), 0, 0x45, i);
        if (fightPokemonCheckValid(PC_PTR(v1))) {
            t = figthPokemonGetLevel(v1);
            if ((u8)best < (u8)t)
                best = t;
        }
    }
    for (i = 0; i < 6; i++) {
        v2 = fightTrainerGetStatus(PC_PTR(ctx1), 0, 0x45, i);
        if (fightPokemonCheckValid(PC_PTR(v2)) && (s32)pokemonGetStatus(PC_PTR(v2), 0, 0xcf, 0) == 1) {
            t = figthPokemonGetLevel(v2);
            if ((u8)best < (u8)t)
                best = t;
        }
    }
    if ((u8)fightFloorGetStatus(0, 0, 0x26, 0) == 0) {
        if (a26 <= 1) {
            mult = 1;
        } else {
            mult = 2;
        }
    } else {
        mult = 4;
    }
    amount = mult * (fieldVal * ((best & 0xFF) * field48));
    if ((u8)fightFloorGetStatus(0, 0, 0x26, 0) == 1) {
        scriptAddPremium(amount);
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
        return;
    }
    heroAddPokedoru(field44, amount);
    msgctrlSetValue(0x2f, amount);
    msgctrlSetValue(0x4b, amount);
    msgctrlSetValue(0x13, heroGetStatus(PC_PTR(field44), 1, 0));
    lbl_8047B610 = lbl_8047B610 + 5;
}

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd5C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
    u32 ctxA = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    u32 ctx2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    u32 fieldD9 = pokemonGetStatus(PC_PTR(ctx2), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    if (fightWazaIsHit(PC_PTR(fieldD9)) == 1) {
        if ((ServerStatusFlag & 0x100) != 0) {
            lbl_8047B610 = lbl_8047B610 + 2;
            return;
        } else if ((ServerStatusFlag & 0x80) != 0) {
            fightOutPokemonDamageEffect(PC_PTR(ctxA));
        }
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#else
void wsCmd5C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightWazaIsHit();
#endif
    u32 ctxA = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    u32 ctx2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x11, 0));
    u32 fieldD9 = pokemonGetStatus(PC_PTR(ctx2), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
    if (fightWazaIsHit(PC_PTR(fieldD9)) == 1) {
        if ((ServerStatusFlag & 0x100) != 0) {
            lbl_8047B610 = lbl_8047B610 + 2;
            return;
        } else if ((ServerStatusFlag & 0x80) != 0) {
            fightOutPokemonDamageEffect((void*)ctxA);
        }
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}
#endif

void wsCmd5B(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightMenuYesNo();
#endif
    if ((u8)fightMenuYesNo() == 1) {
        lbl_8047B642++;
        lbl_8047B610 = lbl_8047B610 + 5;
    } else {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 1);
    }
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd5A(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 wazaGetStatus(u32, u16, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCreate(u32, u16, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
    extern s32 fightMenuWazaWasure();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightMenuYesNo();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightMenuCloseMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightMenuOpenMsg();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u32, u32, u32, u16);
#endif
    extern u16 lbl_8047B61C;
    extern void* lbl_8047B64C;
    u32 pokemon;
    s32 choice;
    struct {
        u16 value;
    } move;
    u32 text;
    pokemon = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(lbl_8047B64C));
    if (fightMenuYesNo() == 1) {
        while (1) {
            fightMenuCloseMsg();
            choice = fightMenuWazaWasure(pokemon, lbl_8047B61C);
            if (choice == -1 || choice >= 4) {
                break;
            }
            move.value = pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_MOVE, choice);
            if (wazaGetStatus(0, move.value, 0x19, 0) == 1) {
                if (fightMenuOpenMsg(0x7635) == 1) {
                    fightMenuCloseMsg();
                }
            } else {
                pokemonWazaCreate(PC_PTR(pokemon), choice, lbl_8047B61C);
#ifdef TARGET_PC /* the assembly passes wazaGetStatus's result straight on in r3 (verified by reading it; the GameCube form
                  * below keeps the bytes, since this function's local declaration returns u8) */
                text = PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, move.value, 1, 0)));
#else
                wazaGetStatus(0, move.value, 1, 0);
                text = GSmsgGetGSchar();
#endif
                msgctrlSetValue(0xe, text);
                msgctrlSetValue(0x5d, 0x468);
                lbl_8047B610 = (u8*)PC_PTR(DISC_U32(lbl_8047B610 + 1));
                return;
            }
        }
    }
    lbl_8047B610 += 5;
    return;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset
#pragma optimize_for_size on
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void WS_WAZAOBOE_CHECK(void) {
    extern u32 pokemonGetOboeWazaDataBanme();
    extern s8 pokemonOboeWaza();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetOboeWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 figthPokemonGetLevel(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
    extern u16 lbl_8047B61C;
    extern u8 lbl_8047B642;
    extern void* lbl_8047B64C;
    u32 uVar8;
    u32 uVar7;
    u32 pokemon;
    u32 uVar2;
    u32 uVar1;
    u8 uVar4;
    s8 cVar5;
    u16 uVar3;
    u8* pc;
    pokemon = PC_PTR32(lbl_8047B64C);
    uVar2 = figthPokemonGetLevel(PC_PTR32(lbl_8047B64C));
    uVar1 = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(pokemon));
    pc = lbl_8047B610;
    uVar8 = DISC_U32(pc + 1);
    uVar7 = DISC_U32(pc + 5);
    if (*(char*)(pc + 9) != 0) {
        uVar4 = pokemonGetOboeWazaDataBanme(uVar1, uVar2);
        lbl_8047B642 = uVar4;
    }
    while (1) {
        cVar5 = pokemonOboeWaza(uVar1, uVar2, 0, &lbl_8047B642);
        if (cVar5 == -1) {
            uVar3 = pokemonGetOboeWazaDataId(PC_PTR(uVar1), uVar2, &lbl_8047B642);
            lbl_8047B61C = uVar3;
            lbl_8047B610 += 10;
            return;
        }
        if (cVar5 != -2)
            break;
        lbl_8047B642 = (char)lbl_8047B642 + 1;
    }
    if (cVar5 == -3) {
        lbl_8047B610 = (u8*)PC_PTR(uVar7);
        return;
    }
    lbl_8047B642 = (char)lbl_8047B642 + 1;
    uVar3 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_MOVE, cVar5);
    lbl_8047B61C = uVar3;
    lbl_8047B610 = (u8*)PC_PTR(uVar8);
    return;
}
#else
void WS_WAZAOBOE_CHECK(void) {
    extern u32 pokemonGetStatus(u8* obj, u32 id, u32 selector, u32 d);
    extern u32 pokemonGetOboeWazaDataBanme();
    extern s8 pokemonOboeWaza();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetOboeWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 figthPokemonGetLevel(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
    extern u16 lbl_8047B61C;
    extern u8 lbl_8047B642;
    extern void* lbl_8047B64C;
    u32 uVar8;
    u32 uVar7;
    u32 pokemon;
    u32 uVar2;
    u32 uVar1;
    u8 uVar4;
    s8 cVar5;
    u16 uVar3;
    u8* pc;
    pokemon = PC_PTR32(lbl_8047B64C);
    uVar2 = figthPokemonGetLevel(PC_PTR32(lbl_8047B64C));
    uVar1 = PC_PTR32_IMPLICIT(fightPokemonGetPokemonPtr(pokemon));
    pc = lbl_8047B610;
    uVar8 = DISC_U32(pc + 1);
    uVar7 = DISC_U32(pc + 5);
    if (*(char*)(pc + 9) != 0) {
        uVar4 = pokemonGetOboeWazaDataBanme(uVar1, uVar2);
        lbl_8047B642 = uVar4;
    }
    while (1) {
        cVar5 = pokemonOboeWaza(uVar1, uVar2, 0, &lbl_8047B642);
        if (cVar5 == -1) {
            uVar3 = pokemonGetOboeWazaDataId(PC_PTR(uVar1), uVar2, &lbl_8047B642);
            lbl_8047B61C = uVar3;
            lbl_8047B610 += 10;
            return;
        }
        if (cVar5 != -2)
            break;
        lbl_8047B642 = (char)lbl_8047B642 + 1;
    }
    if (cVar5 == -3) {
        lbl_8047B610 = (u8*)PC_PTR(uVar7);
        return;
    }
    lbl_8047B642 = (char)lbl_8047B642 + 1;
    uVar3 = (int)pokemonGetStatus((u8*)uVar1, 0, POKEMON_STATUS_MOVE, cVar5);
    lbl_8047B61C = uVar3;
    lbl_8047B610 = (u8*)PC_PTR(uVar8);
    return;
}
#endif

#pragma optimize_for_size reset
void wsCmd71(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 wazaGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetNicknamePtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
    u32 val = PC_PTR32(lbl_8047B64C);
    u32 t = PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, lbl_8047B61C, 1, 0)));
    u32 result = fightPokemonGetNicknamePtr(PC_PTR(val));
    msgctrlSetValue(0xd, result);
    msgctrlSetValue(0xe, t);
    lbl_8047B610 = lbl_8047B610 + 1;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd58(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonModosuEffect();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    u32 ctx;
    u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    if (fightOutPokemonCheckFightOut(PC_PTR(ctx)) == 1) {
        fightOutPokemonModosuEffect(PC_PTR(ctx), 0);
        fn_80265598(ctx, val, 1);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 1);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 2);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 3);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 4);
        fightMenuStatusMenuClose(ctx, val, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 2;
}

#pragma optimize_for_size reset
void wsCmd57(void) {
    lbl_8047B610 = lbl_8047B610 + 1;
}

void wsCmd56(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightOutPokemonGetVoiceSndId();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    soundStart((u16)fightOutPokemonGetVoiceSndId(PC_PTR(ctx)), 0, 0xff, 0);
    lbl_8047B610 = lbl_8047B610 + 2;
}

void wsCmd55(void) {
    std130_soundUnk_801653cc(DISC_U16(lbl_8047B610 + 1), 0, 0xff);
    lbl_8047B610 = lbl_8047B610 + 3;
}

void wsCmd54(void) {
    soundStart(DISC_U16(lbl_8047B610 + 1), 0, 0xff, 0);
    lbl_8047B610 = lbl_8047B610 + 3;
}

void wsCmd53(void) {
    lbl_8047B610 = lbl_8047B610 + 2;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd52(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wazaSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
    extern void fightFloorCreateFightPokemonEnemyAryEnemySideAll();
    extern void fightFloorRegistFightTrainerEnemyPokemonFightSideAll();
    extern void fightFloorSetMeetEnemyFightPokemonEnemySideAll();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
    extern short fightSideGetCountAsJoutaiDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightSideIsJoutaiDataId();
#endif
    extern void fightSideSetStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightSideGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonMaxHpWaruValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fightOutPokemonIsZokuseiDataId();
#endif
    extern void fightWazaInitJoutai();
    extern void fightWazaBiosCopy();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wsRunScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8022D084();
#endif
    extern void fightOutPokemonTokuseiCheckAppear();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightCheckRuiShadowSense();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus();
#endif
    extern void* lbl_8047B62C;
    extern u8 lbl_80378DAF[];
    extern u8 lbl_80378D7C[];
    extern u8 lbl_80378DE2[];
    u32 uVar2;
    u32 uVar3;
    u16 uVar7;
    u32 uVar5;
    u32 uVar1;
    u32 sVar6;
    int iVar4;
    u8 cVar8;
    u8 auStack_c8[184];
    uVar2 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    uVar3 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(2, uVar2));
    sVar6 = fightOutPokemonGetTokuseiDataId(PC_PTR(uVar2));
    fightFloorSetMeetEnemyFightPokemonEnemySideAll(0);
    fightFloorRegistFightTrainerEnemyPokemonFightSideAll(0);
    pokemonSetStatus((void*)PC_PTR(uVar2), 0, POKEMON_STATUS_SWITCHED_FLAG, 0, 0);
    pokemonSetStatus((void*)PC_PTR(uVar2), 0, POKEMON_STATUS_SWITCH_TARGET, 0, 0xffffffff);
    iVar4 = fightSideGetStatus(PC_PTR(uVar3), 0, 8, 0);
    if ((((iVar4 == 0) && (cVar8 = fightSideIsJoutaiDataId(uVar3, 0x4a), cVar8 == 1)) &&
         (cVar8 = fightOutPokemonIsZokuseiDataId(PC_PTR(uVar2), TYPE_FLYING), cVar8 == 0)) &&
        ((u16)sVar6 != ABILITY_LEVITATE)) {
        fightSideSetStatus(uVar3, 0, 8, 0, 1);
        uVar7 = fightOutPokemonMaxHpWaruValue(
            PC_PTR(uVar2), (5 - (short)fightSideGetCountAsJoutaiDataId(uVar3, 0x4a)) * 2 & 0xfffe);
        uVar5 = (int)pokemonGetStatus(fightTargetGetPtrAsNowFightType(0x11, 0), 0, POKEMON_STATUS_BATTLE_MOVE_PTR, 0);
        fightWazaBiosCopy(auStack_c8, uVar5);
        wazaSetStatus(PC_PTR(uVar5), 0, 0x2d, 0, uVar7);
        fightWazaInitJoutai(uVar5);
        fightFloorSetStatus(0, 0, 0x4b, 0, uVar2);
        cVar8 = *(char*)(lbl_8047B610 + 1);
        if (cVar8 == 18) {
            uVar1 = PC_PTR32(lbl_80378DAF);
        } else if ((cVar8 == 17) || (cVar8 == 20)) {
            uVar1 = PC_PTR32(lbl_80378D7C);
        } else {
            uVar1 = PC_PTR32(lbl_80378DE2);
        }
        wsRunScript(lbl_8047B62C, PC_PTR(uVar1), 0);
        fightWazaBiosCopy(uVar5, auStack_c8);
        iVar4 = fightSideGetStatus(PC_PTR(uVar3), 0, 8, 0);
        if (iVar4 == 0) {
            lbl_8047B610 = lbl_8047B610 + 2;
            return;
        }
    }
    sVar6 = fightOutPokemonGetTokuseiDataId(PC_PTR(uVar2));
    if ((u16)sVar6 == ABILITY_TRUANT) {
        pokemonSetStatus((void*)PC_PTR(uVar2), 0, POKEMON_STATUS_TRUANT_FLAG, 0, 1);
    }
    fightCheckRuiShadowSense(uVar2, 0);
    fightOutPokemonTokuseiCheckAppear(uVar2);
    fn_8022D084(PC_PTR(uVar2));
    fightCheckRuiShadowSense(uVar2, 1);
    fightFloorCreateFightPokemonEnemyAryEnemySideAll(0);
    fightSideSetStatus(uVar3, 0, 8, 0, 0);
    lbl_8047B610 = lbl_8047B610 + 2;
    return;
}

#pragma optimize_for_size reset
void wsCmd51(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr();
#endif
    extern u32 fightOutPokemonGetNamePtr();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
    void* ctx;
    u8 byte;
    u8 arg1 = *(u8*)(lbl_8047B610 + 1);
    ctx = (void*)fightTargetGetPtrAsNowFightType(arg1, 0);
    byte = *(u8*)(lbl_8047B610 + 2);
    switch (byte) {
        case 0:
        case 1:
            break;
        case 2:
            msgctrlSetValue(0xd, PC_PTR32_IMPLICIT(fightOutPokemonGetNicknamePtr(ctx)));
            msgctrlSetValue(0xe, fightOutPokemonGetNamePtr(ctx));
            break;
    }
    lbl_8047B610 += 3;
}

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd50(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
    extern void fightFloorSetTuusinErrorFightResult();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTrainerGetIrekaeFightPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern short fightTrainerSelectIrekaeFightPokemon();
#endif
    u32 uVar2;
    u32 uVar7;
    u32 uVar1;
    u16 uVar4;
    u8 cVar6;
    u32 iVar3;
    short sVar5;
    u8 targetType;
    u16 local_18[4];
    uVar4 = fightFloorGetStatus(0, 0, 0x14, 0);
    targetType = *(volatile u8*)(lbl_8047B610 + 1);
    uVar7 = *(volatile u32*)(lbl_8047B610 + 2);
    uVar1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(targetType, 0));
    uVar2 = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, uVar1));
    cVar6 = (int)pokemonGetStatus(PC_PTR(uVar1), 0, POKEMON_STATUS_SWITCHED_FLAG, 0);
    if (cVar6 == 1) {
        lbl_8047B610 = lbl_8047B610 + 6;
    } else {
        local_18[0] = 0;
        iVar3 = PC_PTR32_IMPLICIT(fightTrainerGetIrekaeFightPokemon(uVar2, local_18));
        if (iVar3 == 0) {
            lbl_8047B610 = (u8*)PC_PTR(uVar7);
        } else {
            sVar5 = fightTrainerSelectIrekaeFightPokemon(PC_PTR(uVar2), PC_PTR(uVar4), 0, PC_PTR(uVar1));
            if (sVar5 < 0) {
                if (sVar5 == -2) {
                    fightFloorSetTuusinErrorFightResult(0);
                }
                lbl_8047B610 = (u8*)PC_PTR(uVar7);
            } else {
                fightFloorSetStatus(0, 0, 0x45, 0, uVar1);
                pokemonSetStatus((void*)PC_PTR(uVar1), 0, POKEMON_STATUS_SWITCH_TARGET, 0, (int)sVar5);
                pokemonSetStatus((void*)PC_PTR(uVar1), 0, POKEMON_STATUS_SWITCHED_FLAG, 0, 1);
                lbl_8047B610 = lbl_8047B610 + 6;
            }
        }
    }
    return;
}
#else
void wsCmd50(void) {
    extern u32 pokemonGetStatus(u8* obj, u32 id, u32 selector, u32 d);
    extern void pokemonSetStatus(u8* obj, u32 id, u32 selector, u32 subindex, u32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
    extern void fightFloorSetTuusinErrorFightResult();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern int fightTrainerGetIrekaeFightPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern short fightTrainerSelectIrekaeFightPokemon();
#endif
    u32 uVar2;
    u32 uVar7;
    u32 uVar1;
    u16 uVar4;
    u8 cVar6;
    u32 iVar3;
    short sVar5;
    u8 targetType;
    u16 local_18[4];
    uVar4 = fightFloorGetStatus(0, 0, 0x14, 0);
    targetType = *(volatile u8*)(lbl_8047B610 + 1);
    uVar7 = *(volatile u32*)(lbl_8047B610 + 2);
    uVar1 = fightTargetGetPtrAsNowFightType(targetType, 0);
    uVar2 = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, uVar1);
    cVar6 = (int)pokemonGetStatus((u8*)uVar1, 0, POKEMON_STATUS_SWITCHED_FLAG, 0);
    if (cVar6 == 1) {
        lbl_8047B610 = lbl_8047B610 + 6;
    } else {
        local_18[0] = 0;
        iVar3 = PC_PTR32_IMPLICIT(fightTrainerGetIrekaeFightPokemon(uVar2, local_18));
        if (iVar3 == 0) {
            lbl_8047B610 = (u8*)PC_PTR(uVar7);
        } else {
            sVar5 = fightTrainerSelectIrekaeFightPokemon(PC_PTR(uVar2), PC_PTR(uVar4), 0, PC_PTR(uVar1));
            if (sVar5 < 0) {
                if (sVar5 == -2) {
                    fightFloorSetTuusinErrorFightResult(0);
                }
                lbl_8047B610 = (u8*)PC_PTR(uVar7);
            } else {
                fightFloorSetStatus(0, 0, 0x45, 0, uVar1);
                pokemonSetStatus((void*)PC_PTR(uVar1), 0, POKEMON_STATUS_SWITCH_TARGET, 0, (int)sVar5);
                pokemonSetStatus((void*)PC_PTR(uVar1), 0, POKEMON_STATUS_SWITCHED_FLAG, 0, 1);
                lbl_8047B610 = lbl_8047B610 + 6;
            }
        }
    }
    return;
}
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size reset; optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size reset
#pragma optimize_for_size on
void wsCmd4F(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
    extern u8 fightFloorCheckFightActionFightOutPokemonIrekaeSelect();
    u32 ctxA = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    u32 result = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, ctxA));
    u16 buf;
    if (*(u8*)(lbl_8047B610 + 6) == 1 &&
        fightFloorCheckFightActionFightOutPokemonIrekaeSelect(0, ctxA, 0) == 1) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 2);
        return;
    }
    buf = 0;
    if (fightTrainerGetIrekaeFightPokemon(PC_PTR(result), &buf) == 0) {
        lbl_8047B610 = DISC_PTR_AT(u8, lbl_8047B610 + 2);
    } else {
        lbl_8047B610 = lbl_8047B610 + 7;
    }
}

#pragma optimize_for_size reset
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd4E(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx1;
    u32 trainerData;
    u32 result;
    u16 val;
    u8 mode;
    u8 slot;
    val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    ctx1 = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(
        (slot = *(u8*)(lbl_8047B610 + 1), mode = *(u8*)(lbl_8047B610 + 2), slot), 0));
    pokemonGetStatus(PC_PTR(ctx1), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    result = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, ctx1));
    trainerData = fightTrainerGetStatus(PC_PTR(result), 0, 0x47, 0);
    if (mode != 2) {
        fightTrainerBallThrowEffect(PC_PTR(result), PC_PTR(ctx1), 2);
    }
    if ((u8)fightOutPokemonIsGcHeroFightOutPokemon(ctx1) == 0) {
        fn_80265598(ctx1, val, 0);
    } else {
        fn_80265598(ctx1, val, 1);
    }
    if (mode != 2) {
        fightOutPokemonDasuEffect(PC_PTR(ctx1), 1);
        fightOutPokemonDasuEffect(PC_PTR(ctx1), 2);
    }
    fightOutPokemonDasuEffect(PC_PTR(ctx1), 3);
    fightOutPokemonDasuEffect(PC_PTR(ctx1), 4);
    fightMenuStatusMenuClose(ctx1, val, 0);
    if (mode != 2) {
        fightTrainerBallThrowEffect(PC_PTR(result), PC_PTR(ctx1), 3);
    }
    fightOutPokemonDasuEffect(PC_PTR(ctx1), 5);
    if (mode == 1 && (u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx1), PC_PTR(0x14)) == 1 &&
        fightOutPokemonGetJoutaiMigawariHp(PC_PTR(ctx1)) > 0) {
        fightMenuCloseMsg();
        fightWazaWzxTypeFuncMigawari(0xa4, PC_PTR(ctx1), PC_PTR(ctx1), 0, 0);
        for (;;) {
            if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                break;
            }
            _threadSwitch();
        }
        fightOutPokemonFreeAllSequenceWaza(PC_PTR(ctx1));
    }
    if ((u8)fightFloorGetStatus(0, 0, 0x1e, 0) == 1 &&
        (u8)fightOutPokemonIsGcHeroFightOutPokemon(ctx1) == 0) {
        fightOutPokemonSetOnZukanFlag(PC_PTR(ctx1), 0);
        fightOutPokemonSetOnDarkPokemonFlag(PC_PTR(ctx1), 0);
    }
    fightOutPokemonInit(trainerData);
    lbl_8047B610 = lbl_8047B610 + 3;
}
#else
void wsCmd4E(void) {
    extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(void* obj, void* search_val);
    extern u8 fightOutPokemonIsGcHeroFightOutPokemon(u32 slotId);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx1;
    u32 trainerData;
    u32 result;
    u16 val;
    u8 mode;
    u8 slot;
    val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    ctx1 = fightTargetGetPtrAsNowFightType(
        (slot = *(u8*)(lbl_8047B610 + 1), mode = *(u8*)(lbl_8047B610 + 2), slot), 0);
    pokemonGetStatus(ctx1, 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    result = (u32)fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, (void*)ctx1);
    trainerData = fightTrainerGetStatus(result, 0, 0x47, 0);
    if (mode != 2) {
        fightTrainerBallThrowEffect((void*)result, (void*)ctx1, 2);
    }
    if ((u8)fightOutPokemonIsGcHeroFightOutPokemon(ctx1) == 0) {
        fn_80265598(ctx1, val, 0);
    } else {
        fn_80265598(ctx1, val, 1);
    }
    if (mode != 2) {
        fightOutPokemonDasuEffect((void*)ctx1, 1);
        fightOutPokemonDasuEffect((void*)ctx1, 2);
    }
    fightOutPokemonDasuEffect((void*)ctx1, 3);
    fightOutPokemonDasuEffect((void*)ctx1, 4);
    fightMenuStatusMenuClose(ctx1, val, 0);
    if (mode != 2) {
        fightTrainerBallThrowEffect((void*)result, (void*)ctx1, 3);
    }
    fightOutPokemonDasuEffect((void*)ctx1, 5);
    if (mode == 1 && (u8)fightOutPokemonIsPokemonJoutaiDataId(ctx1, 0x14) == 1 &&
        fightOutPokemonGetJoutaiMigawariHp((void*)ctx1) > 0) {
        fightMenuCloseMsg();
        fightWazaWzxTypeFuncMigawari(0xa4, (void*)ctx1, (void*)ctx1, 0, 0);
        for (;;) {
            if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                break;
            }
            _threadSwitch();
        }
        fightOutPokemonFreeAllSequenceWaza((void*)ctx1);
    }
    if ((u8)fightFloorGetStatus(0, 0, 0x1e, 0) == 1 &&
        (u8)fightOutPokemonIsGcHeroFightOutPokemon(ctx1) == 0) {
        fightOutPokemonSetOnZukanFlag((void*)ctx1, 0);
        fightOutPokemonSetOnDarkPokemonFlag((void*)ctx1, 0);
    }
    fightOutPokemonInit(trainerData);
    lbl_8047B610 = lbl_8047B610 + 3;
}
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
void wsCmd4D(void) {
    extern void fightPokemonGetFaceSpriteId();
    extern u32 _fightCheckLockOnSub();
    extern u32 pokemonCreateSequence();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void battleGridUpdate();
#endif
    extern void battleGridAddPokemon();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorLoopValidFightOutPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetEntryIdToFightPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTrainerBallThrowEffect();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    extern void fightOutPokemonCopyAllAbiCnt();
    extern void fightOutPokemonCopyPokemonJoutai();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    extern void fightPokemonWriteJoutaiDataId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightPokemonCheckWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetNicknamePtr();
#endif
    extern void fightPokemonBiosSwap();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightPokemonGetPokemonPtr();
#endif
    extern void fightOutPokemonCreate();
    extern void fightOutPokemonRegWzxLoad();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonDasuEffect();
#endif
    struct AbilityList {
        u16 values[11];
    };
    extern const struct AbilityList lbl_80279FE0;
    u32 context;
    u32 trainerPokemon;
    u32 trainer;
    u8 scriptMode;
    u8 targetId;
    u16 copyMode;
    u8 i;
    s16 entryId;
    u32 ability;
    u32 pokemon;
    u32 sequence;
    u8 hasAbility;
    u32 gridPokemon;
    u32 callbackData[2];
    context = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(
        (targetId = lbl_8047B610[1], scriptMode = lbl_8047B610[2], targetId), 0));
    entryId = pokemonGetStatus(PC_PTR(context), 0, POKEMON_STATUS_SWITCH_TARGET, 0);
    pokemon = pokemonGetStatus(PC_PTR(context), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
    trainer = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, context));
    trainerPokemon = fightTrainerGetStatus(PC_PTR(trainer), 0, 0x47, 0);
    if (fightPokemonCheckWriteJoutaiDataId(PC_PTR(pokemon), PC_PTR(CONDITION_BAD_POISON)) == 1) {
        fightPokemonWriteJoutaiDataId(pokemon, CONDITION_BAD_POISON, 1);
    }
    gridPokemon = PC_PTR32_IMPLICIT(fightTrainerGetEntryIdToFightPokemonPtr(trainer, entryId));
    fightPokemonBiosSwap(gridPokemon, pokemon);
    fightPokemonGetFaceSpriteId(pokemon, 0, 0);

    sequence = pokemonCreateSequence(fightPokemonGetPokemonPtr(PC_PTR(pokemon)));
    fightOutPokemonCreate(context, pokemon, sequence);
    fightOutPokemonRegWzxLoad(context);
    {
        struct AbilityList abilities;
        u32 first;
        u32 second;
        const u32* abilitySource = (const u32*)&lbl_80279FE0 - 1;
        u32* abilityDestination = (u32*)&abilities - 1;
        u32 copyIndex;
        for (copyIndex = 0; copyIndex < 2; copyIndex++) {
            first = abilitySource[2];
            second = abilitySource[1];
            abilitySource += 2;
            abilityDestination += 2;
            abilityDestination[-1] = second;
            abilityDestination[0] = first;
        }
        copyMode = 0;
        *(volatile u32*)(abilityDestination + 1) = *(volatile const u32*)(abilitySource + 1);
        *(volatile u16*)(abilityDestination + 2) = *(volatile const u16*)(abilitySource + 2);
        if (scriptMode == 1) {
            copyMode = 0x7F;
        }
        if (copyMode == 0x7F) {
            fightOutPokemonCopyAllAbiCnt(context, trainerPokemon);
            for (i = 0; i < 11; i++) {
                ability = abilities.values[i];
                hasAbility = fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(trainerPokemon), PC_PTR(ability));
                if (hasAbility == 1) {
                    fightOutPokemonCopyPokemonJoutai(context, trainerPokemon, ability);
                }
            }
        }
    }
    callbackData[0] = context;
    callbackData[1] = copyMode;
    fightFloorLoopValidFightOutPokemon(0, _fightCheckLockOnSub, callbackData, 0);
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(trainerPokemon), PC_PTR(0x34)) == 1) {
        fightOutPokemonCopyPokemonJoutai(context, trainerPokemon, 0x34);
    }
    if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(trainerPokemon), PC_PTR(0x35)) == 1) {
        fightOutPokemonCopyPokemonJoutai(context, trainerPokemon, 0x35);
    }
    msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(fightOutPokemonGetNicknamePtr(context)));
    {
        u32 addedPokemon = fightTrainerGetStatus(PC_PTR(trainer), 0, 0x4C, 0);
        if (addedPokemon != 0) {
            sequence = pokemonGetStatus(PC_PTR(context), 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
            if (sequence != 0) {
                battleGridAddPokemon(addedPokemon, sequence);
            }
        }
    }
    if (scriptMode != 2) {
        fightTrainerBallThrowEffect(PC_PTR(trainer), PC_PTR(context), 0);
    }
    fightOutPokemonDasuEffect(PC_PTR(context), 0);
    battleGridUpdate();
    if (scriptMode != 2) {
        fightTrainerBallThrowEffect(PC_PTR(trainer), PC_PTR(context), 1);
    }
    lbl_8047B610 += 3;
}

#pragma optimize_for_size reset
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd4C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    fightOutPokemonBiosCopy(
        PC_PTR(fightTrainerGetStatus(PC_PTR(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, ctx)), 0, 0x47, 0)),
        PC_PTR(ctx));
    lbl_8047B610 = lbl_8047B610 + 2;
}
#else
void wsCmd4C(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
    u32 ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(*(u8*)(lbl_8047B610 + 1), 0));
    fightOutPokemonBiosCopy(
        (struct FightOutPokemon*)fightTrainerGetStatus(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, ctx), 0, 0x47, 0),
        (struct FightOutPokemon*)ctx);
    lbl_8047B610 = lbl_8047B610 + 2;
}
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimize_for_size on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimize_for_size on
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wsCmd4B(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx;
    u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    ctx = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(0x14, 0));
    if ((u8)fightOutPokemonCheckFightOut(PC_PTR(ctx)) == 1) {
        if ((u8)fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ctx), PC_PTR(0x14)) == 1 && fightOutPokemonGetJoutaiMigawariHp(PC_PTR(ctx)) > 0) {
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ctx), PC_PTR(0x14));
            fightOutPokemonFreeAllSequenceWaza(PC_PTR(ctx));
            fightWazaWzxTypeFuncMigawari(0xa4, PC_PTR(ctx), PC_PTR(ctx), 0, 0);
            for (;;) {
                if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                    break;
                }
                _threadSwitch();
            }
        }
        fightOutPokemonFreeAllSequenceWaza(PC_PTR(ctx));
        fightOutPokemonModosuEffect(PC_PTR(ctx), 0);
        fn_80265598(ctx, val, 1);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 1);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 2);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 3);
        fightOutPokemonModosuEffect(PC_PTR(ctx), 4);
        fightMenuStatusMenuClose(ctx, val, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#else
void wsCmd4B(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtrAsNowFightType();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
    u32 ctx;
    u16 val = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    ctx = fightTargetGetPtrAsNowFightType(0x14, 0);
    if ((u8)fightOutPokemonCheckFightOut((void*)ctx) == 1) {
        if ((u8)fightOutPokemonIsPokemonJoutaiDataId(ctx, 0x14) == 1 && fightOutPokemonGetJoutaiMigawariHp((void*)ctx) > 0) {
            fightOutPokemonWriteJoutaiDataId((void*)ctx, (void*)0x14);
            fightOutPokemonFreeAllSequenceWaza((void*)ctx);
            fightWazaWzxTypeFuncMigawari(0xa4, (void*)ctx, (void*)ctx, 0, 0);
            for (;;) {
                if ((u8)wazaSequenceSysIsAllEnd(6) == 1) {
                    break;
                }
                _threadSwitch();
            }
        }
        fightOutPokemonFreeAllSequenceWaza((void*)ctx);
        fightOutPokemonModosuEffect((void*)ctx, 0);
        fn_80265598(ctx, val, 1);
        fightOutPokemonModosuEffect((void*)ctx, 1);
        fightOutPokemonModosuEffect((void*)ctx, 2);
        fightOutPokemonModosuEffect((void*)ctx, 3);
        fightOutPokemonModosuEffect((void*)ctx, 4);
        fightMenuStatusMenuClose(ctx, val, 0);
    }
    lbl_8047B610 = lbl_8047B610 + 1;
}
#endif

#pragma optimize_for_size reset
