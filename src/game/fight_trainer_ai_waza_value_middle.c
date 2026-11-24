/* Score instrumentation only; not evidence of a retail TU boundary. */
/**
 * @file fight_trainer_ai_waza_value_middle.c
 * @brief Candidate fightTrainerAiWazaValue.cpp range, 0x80243CD8 - 0x802451C0.
 *
 * Physically split from fight_trainer_ai_waza_value.c so this
 * translation unit owns only the functions in the stated range.
 */
#include "game/colosseum.h"
#include "game/trainer.h"
#include "game/pokemon.h"
#include "game/constants/pokemon_status.h"

/* =========================================================================
 * Duplicated declarations (verbatim from the original colosseum_battle.c
 * preamble, present in every split segment so each TU keeps the same
 * external visibility it had before the split)
 * ========================================================================= */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   pokemonSetStatus();
#endif

/* Battle system functions */
extern void battleCameraSelect();

/* Sound functions */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStop();     /* Stop sound */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std104_soundSetBackgroundMusic();     /* Fade out music */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std105_soundPlayEffect();     /* Start BGM */
#endif

/* SDA2 float constants used by asm wrappers */
extern f32 lbl_8047E678;
extern f32 lbl_8047E67C;

/* SDA1 globals used by asm wrappers */
extern u32 lbl_8047B668;
extern u32 lbl_8047B66C;
extern u32 lbl_8047B670;

/* Data labels used by asm wrappers */
extern u8  lbl_8039A6B8[];
extern u8  lbl_8039A6A8[];
extern int lbl_804782BC[];
extern u8  lbl_804782E0[];
extern u8  lbl_804783E0[];

/* Forward declarations for functions used as addresses in asm wrappers */
void ShortCommandProc(int r3);
void ReadProc(int r3);
void WriteProc(int r3);
void __GBASyncCallback(int r3);
u32  __GBASync(int r3);
u32  __GBATransfer(int r3, u32 r4, u32 r5, u32 r6);

/* Forward declarations for asm wrapper bl targets (use () form for compat) */
extern void DSPInit();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void set__5GSvecFfff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  _fadeEffectGetRandom__FUl();
#endif
extern u32  pokemonBiosGetCatchTrainerRnd();
extern u32  pokemonBiosGetRnd();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16  pokemonBiosGetPokemonDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  savedataGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fadeCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern int  fadeSet();
#endif
extern int  wazaSequenceSysRelease();
extern int  wazaSequenceSysInitWithoutParticles();
extern void OSRegisterResetFunction();
extern void OSInitAlarm();
extern void OSInitThreadQueue();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy();
#endif

/* Forward declarations for converted functions */
u32 evolutionWazaLearn();
int fightTrainerAiWazaValueKuroikiri(void* ctx, u32 param1, u32 param2, u32 param3);
void fightTrainerAiWazaValueHimitunotikara(void* ctx, u32 param1, u32 param2, u32 param3);
s32 fightTrainerAiSelectIrekaeDasuFightPokemon(void* ctx, u32 param1, u32 param2, u32 param3);
u32 fightTrainerAiWazaHit045(void* trainerCtx, u32 trainerSlot, u32 resultSlot, u32 resultType);
u32 fightMenuFightTrainerGcHeroOpenMenu(void* ctx, u32 param1, u32 param2);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u16 fightFloorGetFightTrainerFightPokemonPtrAry(u32, void*, void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fn_80237F74(void*, u32, u32);
#endif
u8 fn_802384B4(void*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightTrainerAiGetNowTenkou(void*, u32, u32);
#endif
u16 fightTrainerAiGetPokemonDataId2(void*, u32);
u8 fightTrainerAiGetPokemonZokusei(void*, u32, u32);
u8 fightTrainerAiIsPokemonTokusei(void*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif

#if !defined(FIGHT_AI_VALUE_MIDDLE_EXACT_80244318) && \
    !defined(FIGHT_AI_VALUE_MIDDLE_SUFFIX_8024498C)

/* Address: 0x80243CD8 | Size: 0x640 (1600 bytes) */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
u32 fightTrainerAiWazaValueKoraeru(void* ctx, u32 param1, u32 param2, u32 param3)
{
    extern u32* waza_data_number;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E630;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetLastUseWaza(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetFightPokemonWazaAry(void*, u32, u16*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_802376EC(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void*, u32, u16, u32);
#endif
    extern u16 fightAiCheckAllyFlag2A(void*, u32, u16*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetSoubiItem(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void*, u32);
#endif
    extern u32 fightTrainerAiWazaValueJisin();
    extern u32 fightTrainerAiWazaValueJibaku();
    extern u32 fightTrainerAiWazaValueNull();
    u32 entries[8];
    u16 targetMoves[10];
    u16 moves[2];
    u32 handle;
    u8 type;
    u32 count;
    u16 moveCount;
    u16 ability;
    u16 item;
    u8 found;
    u16 i;
    u16 j;
    u16 targetCount;
    u32 (*func)();
    u32 waza;
    u8 risky;

    handle = 0;
    type = fightTrainerAiGetNowTenkou(ctx, 0, 1);
    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), entries, 1, 1);
    moveCount = fightAiCheckAllyFlag2A(ctx, param3, moves);
    ability = fightAiGetLastUseWaza(ctx, param1);
    item = fightTrainerAiGetSoubiItem(ctx, param1);

    found = 0;
    for (i = 0; i < moveCount; i++) {
        if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param1, moves[i], fightAiGetWazaIryoku(ctx, param2)) == 0x41) {
            found = 1;
            break;
        }
    }
    if (found == 1) {
        handle = fn_80239984(0, ctx, 0x16A);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16A);
    }

    if (fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), param1, -1, lbl_8047E630) == 1) {
        handle = fn_80239984(handle, ctx, 0x16B);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16B);
    }

    for (i = 0; i < (u16)count; i++) {
        if (param1 != entries[i]) {
            targetCount = fightTrainerAiGetFightPokemonWazaAry(ctx, entries[i], targetMoves, 0, 1);
            if (targetCount != 0) {
                for (j = 0; j < targetCount; j++) {
                    func = (u32 (*)())PC_PTR(wazaGetStatus(0, targetMoves[j], 0x1C, 0));
                    if (func == 0) {
                        func = fightTrainerAiWazaValueNull;
                    }
                    if (func == fightTrainerAiWazaValueJisin ||
                        func == fightTrainerAiWazaValueJibaku) {
                        handle = fn_80239984(handle, ctx, 0x16C);
                        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16C);
                        break;
                    }
                }
                if (j < targetCount) {
                    break;
                }
            }
        }
    }

    for (waza = 0; (u16)waza < *waza_data_number; waza++) {
        func = (u32 (*)())PC_PTR(wazaGetStatus(0, waza, 0x1C, 0));
        if (func == 0) {
            func = fightTrainerAiWazaValueNull;
        }
        if ((func == fightTrainerAiWazaValueJisin ||
             func == fightTrainerAiWazaValueJibaku) &&
            fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(waza), 0) == 1) {
            handle = fn_80239984(handle, ctx, 0x16D);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16D);
            break;
        }
    }

    risky = 0;
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(6)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(3)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(4)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(5)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x18)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x1C)) == 1) {
        risky = 1;
    }
    if (risky == 1) {
        handle = fn_80239984(handle, ctx, 0x16E);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16E);
    }

    if (ability == 0xCB && pokemonGetStatus(PC_PTR(param1), 0, POKEMON_STATUS_PROTECT_COUNT, 0) != 0) {
        handle = fn_80239984(handle, ctx, 0x16F);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16F);
    }

    if (item != 0x11 && item != 0xF) {
        handle = fn_80239984(handle, ctx, 0x170);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x170);
    }

    if (fn_802376EC(ctx, param1) == 1) {
        handle = fn_80239984(handle, ctx, 0x171);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x171);
    }

    if (type == 4 || type == 3) {
        handle = fn_80239984(handle, ctx, 0x172);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x172);
    }
    return handle;
}
#else
u32 fightTrainerAiWazaValueKoraeru(void* ctx, u32 param1, u32 param2, u32 param3)
{
    /* RULE-EXCEPTION(user-approved): caller-side prototype returning u32 (fight_floor.c returns u16); retail keeps the raw count (mr r15,r3, no clrlwi) - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonPtrAry(u32 obj, u32 slot, u32* arr, u8 r6, u8 r7);
#endif
    extern u32* waza_data_number;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E630;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 wazaGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetLastUseWaza(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetFightPokemonWazaAry(void*, u32, u16*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_802376EC(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetAisyou(void*, u32, u16, u32);
#endif
    extern u16 fightAiCheckAllyFlag2A(void*, u32, u16*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerAiGetSoubiItem(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaIryoku(void*, u32);
#endif
    extern u32 fightTrainerAiWazaValueJisin();
    extern u32 fightTrainerAiWazaValueJibaku();
    extern u32 fightTrainerAiWazaValueNull();
    u32 entries[8];
    u16 targetMoves[10];
    u16 moves[2];
    u32 handle;
    u8 type;
    u32 count;
    u16 moveCount;
    u16 ability;
    u16 item;
    u8 found;
    u16 i;
    u16 j;
    u16 targetCount;
    u32 (*func)();
    u32 waza;
    u8 risky;

    handle = 0;
    type = fightTrainerAiGetNowTenkou(ctx, 0, 1);
    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32(ctx), entries, 1, 1);
    moveCount = fightAiCheckAllyFlag2A(ctx, param3, moves);
    ability = fightAiGetLastUseWaza(ctx, param1);
    item = fightTrainerAiGetSoubiItem(ctx, param1);

    found = 0;
    for (i = 0; i < moveCount; i++) {
        if (fightTrainerAiGetAisyou(PC_PTR32_IMPLICIT(ctx), param1, moves[i], fightAiGetWazaIryoku(ctx, param2)) == 0x41) {
            found = 1;
            break;
        }
    }
    if (found == 1) {
        handle = fn_80239984(0, ctx, 0x16A);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16A);
    }

    if (fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), param1, -1, lbl_8047E630) == 1) {
        handle = fn_80239984(handle, ctx, 0x16B);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16B);
    }

    for (i = 0; i < (u16)count; i++) {
        if (param1 != entries[i]) {
            targetCount = fightTrainerAiGetFightPokemonWazaAry(ctx, entries[i], targetMoves, 0, 1);
            if (targetCount != 0) {
                for (j = 0; j < targetCount; j++) {
                    func = (u32 (*)())PC_PTR(wazaGetStatus(0, targetMoves[j], 0x1C, 0));
                    if (func == 0) {
                        func = fightTrainerAiWazaValueNull;
                    }
                    if (func == fightTrainerAiWazaValueJisin ||
                        func == fightTrainerAiWazaValueJibaku) {
                        handle = fn_80239984(handle, ctx, 0x16C);
                        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16C);
                        break;
                    }
                }
                if (j < targetCount) {
                    break;
                }
            }
        }
    }

    for (waza = 0; (u16)waza < *waza_data_number; waza++) {
        func = (u32 (*)())PC_PTR(wazaGetStatus(0, waza, 0x1C, 0));
        if (func == 0) {
            func = fightTrainerAiWazaValueNull;
        }
        if ((func == fightTrainerAiWazaValueJisin ||
             func == fightTrainerAiWazaValueJibaku) &&
            fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(waza), 0) == 1) {
            handle = fn_80239984(handle, ctx, 0x16D);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16D);
            break;
        }
    }

    risky = 0;
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(6)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(3)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(4)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(5)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x18)) == 1) {
        risky = 1;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x1C)) == 1) {
        risky = 1;
    }
    if (risky == 1) {
        handle = fn_80239984(handle, ctx, 0x16E);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16E);
    }

    if (ability == 0xCB && pokemonGetStatus(PC_PTR(param1), 0, POKEMON_STATUS_PROTECT_COUNT, 0) != 0) {
        handle = fn_80239984(handle, ctx, 0x16F);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x16F);
    }

    if (item != 0x11 && item != 0xF) {
        handle = fn_80239984(handle, ctx, 0x170);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x170);
    }

    if (fn_802376EC(ctx, param1) == 1) {
        handle = fn_80239984(handle, ctx, 0x171);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x171);
    }

    if (type == 4 || type == 3) {
        handle = fn_80239984(handle, ctx, 0x172);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x172);
    }
    return handle;
}
#endif

#endif

#if defined(FIGHT_AI_VALUE_MIDDLE_EXACT_80244318)

/* Address: 0x80244318 | Size: 0x160 (352 bytes) */
u32 fightTrainerAiWazaValueMiyaburu(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u32 fightTrainerAiGetAbicntAvoid(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetZokusei(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = 0;
    if (fightTrainerAiGetZokusei(PC_PTR32_IMPLICIT(ctx), PC_PTR(param3), 7) == 1) {
        handle = fn_80239984(0, ctx, 0x167);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x167);
    }
    if ((fightTrainerAiGetAbicntAvoid(ctx, param3) & 0xff) >= 8) {
        handle = fn_80239984(handle, ctx, 0x168);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x168);
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x19)) == 1) {
        handle = fn_80239984(handle, ctx, 0x169);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x169);
    }
    return handle;
}
/* Address: 0x80244478 | Size: 0x9C */
u32 fightTrainerAiWazaValueMajikkukooto(void* ctx, u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = 0;
    if ((pokemonGetStatus(PC_PTR(param1), 0, POKEMON_STATUS_TURNS_ON_FIELD, 0) & 0xFFFF) != 0) {
        handle = fn_80239984(0, ctx, 0x166);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x166);
    }
    return handle;
}
static inline u16 lowHalf(u32 value) {
    return (u16)value;
}
/* Address: 0x80244514 | Size: 0x18C (396 bytes) */
u32 fightTrainerAiWazaValueMiraakooto(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 zokuseiDataBiosGetCategory(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonPtrAry(u32, void*, u32*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetLastUseWaza(void*, u32);
#endif
    extern u32 fightTrainerAiGetBasisSpeAtk(void*, u32);
    extern u32 fightTrainerAiGetBasisPhyAtk(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 entries[8];
    u32 rawCount;
    u32 count;
    u32* entriesPtr;
    u32 move;
    u32 index;
    u32 current;
    u32 handle;

    handle = 0;
    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), entries, 0, 1);
    rawCount = count;
    move = fightAiGetLastUseWaza(ctx, param3);
    entriesPtr = entries;
    count = rawCount & 0xffff;
    index = 0;
    while (lowHalf(index) < count) {
        current = fightTrainerAiGetBasisPhyAtk(ctx, entriesPtr[lowHalf(index)]);
        if (lowHalf(current) < lowHalf(fightTrainerAiGetBasisSpeAtk(ctx, entriesPtr[lowHalf(index)]))) {
            handle = fn_80239984(0, ctx, 0x164);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x164);
            break;
        }
        index++;
    }
    if (lowHalf(move) != 0 && lowHalf(move) != 0xffff && lowHalf(move) != 0x165 && lowHalf(move) != 0x163) {
        if ((u8)zokuseiDataBiosGetCategory(fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), move, param3)) == 2) {
            handle = fn_80239984(handle, ctx, 0x165);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x165);
        }
    }
    return handle;
}
/* Address: 0x802446A0 | Size: 0x18C (396 bytes) */
u32 fightTrainerAiWazaValueKauntaa(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 zokuseiDataBiosGetCategory(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonPtrAry(u32, void*, u32*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetLastUseWaza(void*, u32);
#endif
    extern u32 fightTrainerAiGetBasisSpeAtk(void*, u32);
    extern u32 fightTrainerAiGetBasisPhyAtk(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetWazaTenkouValue(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 entries[8];
    u32 rawCount;
    u32 count;
    u32* entriesPtr;
    u32 move;
    u32 index;
    u32 current;
    u32 handle;

    handle = 0;
    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), entries, 0, 1);
    rawCount = count;
    move = fightAiGetLastUseWaza(ctx, param3);
    entriesPtr = entries;
    count = rawCount & 0xffff;
    index = 0;
    while (lowHalf(index) < count) {
        current = fightTrainerAiGetBasisPhyAtk(ctx, entriesPtr[lowHalf(index)]);
        if (lowHalf(current) > lowHalf(fightTrainerAiGetBasisSpeAtk(ctx, entriesPtr[lowHalf(index)]))) {
            handle = fn_80239984(0, ctx, 0x162);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x162);
            break;
        }
        index++;
    }
    if (lowHalf(move) != 0 && lowHalf(move) != 0xffff && lowHalf(move) != 0x165 && lowHalf(move) != 0x163) {
        if ((u8)zokuseiDataBiosGetCategory(fightTrainerAiGetWazaTenkouValue(PC_PTR32_IMPLICIT(ctx), move, param3)) == 1) {
            handle = fn_80239984(handle, ctx, 0x163);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x163);
        }
    }
    return handle;
}
/* Address: 0x8024482C | Size: 0xD4 (212 bytes) */
u32 fightTrainerAiWazaValueAroma(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightPokemonPtrAry(u32, void*, u32*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u8 fightAiIsJoutaiNormal(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 list[23];
    void* battleCtx = ctx;
    u32 trainer = param1;
    u32 sequenceArg = param2;
    u32* listPtr;
    u32 handle = 0;
    u32 i;
    u32 count;

    count = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(battleCtx), list, 1, 1);
    listPtr = list;
    count &= 0xFFFF;
    for (i = 0; (u16)i < count; i++) {
        if (fightAiIsJoutaiNormal(battleCtx, listPtr[(u16)i]) == 0) {
            handle = fn_80239984(0, battleCtx, 0x161);
            fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x161);
            break;
        }
    }
    return handle;
}
/* Address: 0x80244900 | Size: 0x8C */
u32 fightTrainerAiWazaValueRihuressyu(void* ctx, u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void* ctx, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle = 0;

    if (fightTrainerAiIsJoutaiNormal(ctx, param1) == 0) { /* r3 / r4 passed through */
        u32 tmp = fn_80239984(0, ctx, 0x160);
        handle = tmp;
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x160);
    }
    return handle;
}

#endif

#if defined(FIGHT_AI_VALUE_MIDDLE_SUFFIX_8024498C)

/* Address: 0x8024498C | Size: 0x318 (792 bytes) */
u32 fightTrainerAiWazaValueSunaarasi(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    u16 fightFloorGetFightTrainerFightOutPokemonPtrAry(u32, void*, u32*, u32, u32);
#endif
    u16 fightTrainerAiGetPokemonDataId(void*, u32);
    u32 pokemon[23];
    u32 outPokemon[8];
    u32 handle;
    u8 weather;
    u16 pokemonCount;
    u16 outPokemonCount;
    u16 i;

    handle = 0;
    weather = fightTrainerAiGetNowTenkou(ctx, 0, 0);
    pokemonCount = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), pokemon, 1, 1);
    outPokemonCount = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), outPokemon, 0, 1);

    if (weather != 3) {
        handle = fn_80239984(0, ctx, 0x15b);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x15b);
    }

    for (i = 0; i < pokemonCount; i++) {
        if (fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0x5) == 1 ||
            fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0x4) == 1 ||
            fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0x8) == 1 ||
            fightTrainerAiIsPokemonTokusei(ctx, pokemon[i], 0x8) == 1) {
            handle = fn_80239984(handle, ctx, 0x15c);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x15c);
            break;
        }
    }

    for (i = 0; i < outPokemonCount; i++) {
        if (fightTrainerAiGetPokemonDataId(ctx, outPokemon[i]) == 0x12f) {
            handle = fn_80239984(handle, ctx, 0x15d);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x15d);
            break;
        }
    }

    for (i = 0; i < pokemonCount; i++) {
        if (fightTrainerAiGetPokemonDataId2(ctx, pokemon[i]) == 0x181) {
            handle = fn_80239984(handle, ctx, 0x15e);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x15e);
            break;
        }
    }

    if (weather == 3) {
        handle = fn_80239984(handle, ctx, 0x15f);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x15f);
    }
    return handle;
}
/* Address: 0x80244CA4 | Size: 0x2C4 (708 bytes) */
u32 fightTrainerAiWazaValueArare(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    u16 fightFloorGetFightTrainerFightOutPokemonPtrAry(u32, void*, u32*, u32, u32);
#endif
    u16 fightTrainerAiGetPokemonDataId(void*, u32);
    u32 pokemon[23];
    u32 outPokemon[8];
    u32 handle;
    u8 weather;
    u16 pokemonCount;
    u16 outPokemonCount;
    u16 i;

    handle = 0;
    weather = fightTrainerAiGetNowTenkou(ctx, 0, 0);
    pokemonCount = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), pokemon, 1, 1);
    outPokemonCount = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), outPokemon, 0, 1);

    if (weather != 4) {
        handle = fn_80239984(0, ctx, 0x156);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x156);
    }

    for (i = 0; i < pokemonCount; i++) {
        if (fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0xf) == 1) {
            handle = fn_80239984(handle, ctx, 0x157);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x157);
            break;
        }
    }

    for (i = 0; i < outPokemonCount; i++) {
        if (fightTrainerAiGetPokemonDataId(ctx, outPokemon[i]) == 0x12f) {
            handle = fn_80239984(handle, ctx, 0x158);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x158);
            break;
        }
    }

    for (i = 0; i < pokemonCount; i++) {
        if (fightTrainerAiGetPokemonDataId2(ctx, pokemon[i]) == 0x181) {
            handle = fn_80239984(handle, ctx, 0x159);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x159);
            break;
        }
    }

    if (weather == 4) {
        handle = fn_80239984(handle, ctx, 0x15a);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x15a);
    }
    return handle;
}
/* Address: 0x80244F68 | Size: 0x258 (600 bytes) */
u32 fightTrainerAiWazaValueNihonbare(void* ctx, u32 param1, u32 param2, u32 param3) {
    u32 pokemon[23];
    u32 handle;
    u8 weather;
    u16 count;
    u16 i;

    handle = 0;
    weather = fightTrainerAiGetNowTenkou(ctx, 0, 0);
    count = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), pokemon, 1, 1);

    if (weather != 1) {
        handle = fn_80239984(0, ctx, 0x152);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x152);
    }

    for (i = 0; i < count; i++) {
        if (fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0xa) == 1 ||
            fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0xc) == 1 ||
            fightTrainerAiIsPokemonTokusei(ctx, pokemon[i], 0x22) == 1) {
            handle = fn_80239984(handle, ctx, 0x153);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x153);
            break;
        }
    }

    for (i = 0; i < count; i++) {
        if (fightTrainerAiGetPokemonDataId2(ctx, pokemon[i]) == 0x181) {
            handle = fn_80239984(handle, ctx, 0x154);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x154);
            break;
        }
    }

    if (weather == 1) {
        handle = fn_80239984(handle, ctx, 0x155);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x155);
    }
    return handle;
}

#endif
