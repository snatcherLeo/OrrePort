/**
 * @file fight_trainer_ai_waza_value_exact_80247048.c
 * @brief Byte-exact fightTrainerAiWazaValue.cpp range, 0x80247048 - 0x8024A170.
 *
 * Physically split from fight_trainer_ai_waza_value.c so this
 * translation unit owns only the functions in the stated range.
 */
#include "game/colosseum.h"
#include "game/constants/pokemon_status.h"
#include "game/trainer.h"
#include "game/pokemon.h"

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

#ifdef TARGET_PC
/* PC: the GameCube declares fightTrainerGetStatus s32 in this file (signed divw in its AI value terms); restore that
 * type over the canonical 64-bit unsigned StatusValue (port-gaps 38; see fight_trainer_ai_waza_value_residual_8024B474.c) */
#define fightTrainerGetStatus(a, b, c, d) ((s32)(fightTrainerGetStatus)((a), (b), (c), (d)))
#endif

/* Address: 0x80247048 | Size: 0x7C | Pattern: field_accessor */
s32 fightTrainerAiWazaValueItigekihissatu(void* ctx, u32 slot, u32 param) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8();
#endif
    s32 handle = fn_80239984(0, ctx, 0x135);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(slot)), 0, 0, param, 0, 0x135);
    return handle;
}
/* Address: 0x802470C4 | Size: 0xEC (236 bytes) */
u32 fightTrainerAiWazaValueZennouryokuappu(void* ctx, u32 slot, u32 param, u32 unused) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
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

    handle = fn_80239984(0, ctx, 0x133);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(slot)), 0, 0, param, 0, 0x133);
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(slot)) == 1) {
        handle = fn_80239984(handle, ctx, 0x134);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(slot)), 0, 0, param, 0, 0x134);
    }
    return handle;
}
/* Address: 0x802471B0 | Size: 0x128 (296 bytes) */
u32 fightTrainerAiWazaValueTuikaKougekiAppu(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetAbicntPhyAtk(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
    u32 battleParam = param2;
    u32 trainerParam = param1;
    void* battleCtx = ctx;
    s32 quotient;
    u32 setupHandle;
    u32 pokemonPtr;
    u32 statusValue;

    statusValue = fightAiGetWazaAddFightKoukaAvg(battleCtx, battleParam);
    quotient = (s32)statusValue / fightTrainerGetStatus(0, 0x131, 0x3e, 0);
    setupHandle = fightTrainerAiAddValue(0, quotient);
    pokemonPtr = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam));
    fn_80239CCC(0xEC64, PC_PTR32_IMPLICIT(battleCtx), pokemonPtr, 0, 0, battleParam, 0, 0x131, quotient);

    if (fightTrainerAiGetAbicntPhyAtk(PC_PTR32_IMPLICIT(battleCtx), trainerParam) >= 0xcU) {
        statusValue = fightAiGetWazaAddFightKoukaAvg(battleCtx, battleParam);
        quotient = (s32)statusValue / fightTrainerGetStatus(0, 0x132, 0x3e, 0);
        setupHandle = fightTrainerAiAddValue(setupHandle, quotient);
        pokemonPtr = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam));
        fn_80239CCC(0xEC64, PC_PTR32_IMPLICIT(battleCtx), pokemonPtr, 0, 0, battleParam, 0, 0x132, quotient);
    }

    return setupHandle;
}
/* Address: 0x802472D8 | Size: 0xDC (220 bytes) */
u32 fightTrainerAiWazaValueHaganenotubasa(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetAbicntPhyDef(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0x12f);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x12f);
    if (fightTrainerAiGetAbicntPhyDef(PC_PTR32_IMPLICIT(ctx), param1) >= 0xc) {
        handle = fn_80239984(handle, ctx, 0x130);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x130);
    }
    return handle;
}
/* Address: 0x802473B4 | Size: 0x144 (324 bytes) */
u32 fightAiValueSoubiItem17(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetSoubiItem(void* ctx, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    void* battleCtx = ctx;
    u32 trainer = param1;
    u32 sequenceArg = param2;
    u32 handle = 0;

    if ((fightTrainerAiGetSoubiItem(battleCtx, param1) & 0xFFFF) == 0x17) {
        handle = fn_80239984(0, battleCtx, 0x12c);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x12c);
    }
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, battleCtx, 1, 1, PC_PTR(0x10e), PC_PTR(trainer)) == 1) {
        handle = fn_80239984(handle, battleCtx, 0x12d);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x12d);
    }
    handle = fn_80239984(handle, battleCtx, 0x12e);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x12e);
    return handle;
}
/* Address: 0x802474F8 | Size: 0x1A8 (424 bytes) */
u32 fightTrainerAiWazaValueOobaahiito(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetSoubiItem(void* ctx, u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = 0;
    if ((fightTrainerAiGetSoubiItem(ctx, param1) & 0xffff) == 0x17) {
        handle = fn_80239984(0, ctx, 0x128);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x128);
    }
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0x129);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x129);
    }
    handle = fn_80239984(handle, ctx, 0x12a);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x12a);
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(7)) == 1) {
        handle = fn_80239984(handle, ctx, 0x12b);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x12b);
    }
    return handle;
}
/* Address: 0x802476A0 | Size: 0x110 (272 bytes) */
s32 fightTrainerAiWazaValueHatakiotosu(void* ctx, void* param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightAiGetSoubiItemOf();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8();
#endif
    s32 handle = 0;

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x3d)) == 0 && fightAiGetSoubiItemOf(ctx, param3) != 0) {
        handle = (s32)fn_80239984(0, ctx, 0x126);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x126);
    }
    if (fn_80237F74(ctx, param3, 0x3c) == 1) {
        handle = (s32)fn_80239984(handle, ctx, 0x127);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x127);
    }
    return handle;
}
/* Address: 0x802477B0 | Size: 0x158 (344 bytes) */
u32 fightTrainerAiWazaValueDouguUbau(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80142984(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u8 fightAiCanStealItem(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetSoubiItemOf();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    void* battleCtx = ctx;
    u32 trainer = param1;
    u32 sequenceArg = param2;
    u32 target = param3;
    u32 handle = 0;
    u8 canSteal;
    u32 targetItem;
    u32 ownItem;

    ownItem = fightAiGetSoubiItemOf(battleCtx, trainer); /* r4 = param1 at the call (assembly) */
    targetItem = fightAiGetSoubiItemOf(battleCtx, target);
    canSteal = 1;
    if (fightAiCanStealItem(trainer) == 0) {
        canSteal = 0;
    }
    if ((ownItem & 0xFFFF) != 0 || (targetItem & 0xFFFF) == 0xAF || (targetItem & 0xFFFF) == 0 ||
        fn_80142984(targetItem) == 0) {
        canSteal = 0;
    }
    if (canSteal == 1) {
        handle = fn_80239984(0, battleCtx, 0x124);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x124);
    }
    if (fn_80237F74(battleCtx, target, 0x3c) == 1) {
        handle = fn_80239984(handle, battleCtx, 0x125);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x125);
    }
    return handle;
}
/* Address: 0x80247908 | Size: 0x1C0 (448 bytes) */
u32 fightTrainerAiWazaValueSooraabiimu(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetNowTenkou(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 trainer;
    u32 sequenceArg;
    void* battleCtx;
    u32 handle;
    u32 state;

    trainer = param1;
    sequenceArg = param2;
    battleCtx = ctx;
    handle = 0;
    state = fightTrainerAiGetNowTenkou(battleCtx, 0, 1);
    if ((u8)state == 1) {
        handle = fn_80239984(0, battleCtx, 0x120);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x120);
    }
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, battleCtx, 1, 1, PC_PTR(0x10e), PC_PTR(trainer)) == 1) {
        handle = fn_80239984(handle, battleCtx, 0x121);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x121);
    }
    if ((u8)state == 2 || (u8)state == 4 || (u8)state == 3) {
        handle = fn_80239984(handle, battleCtx, 0x122);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x122);
    }
    if ((u8)state == 0) {
        handle = fn_80239984(handle, battleCtx, 0x123);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainer)), 0, 0, sequenceArg, 0, 0x123);
    }
    return handle;
}
/* Address: 0x80247AC8 | Size: 0x194 (404 bytes) */
u32 fightTrainerAiWazaValueToraiatakku(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0x11d);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x11d);
    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0) {
        handle = fn_80239984(handle, ctx, 0x11e);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x11e);
    }
    if (fn_80237F74(ctx, param3, 0x13) == 1 || fn_80237F74(ctx, param3, 7) == 1 ||
        fn_80237F74(ctx, param3, 0x29) == 1 || fn_80237F74(ctx, param3, 0x28) == 1) {
        handle = fn_80239984(handle, ctx, 0x11f);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x11f);
    }
    return handle;
}
/* Address: 0x80247C5C | Size: 0x184 (388 bytes) */
u32 fightTrainerAiWazaValueDokudokunokiba(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0x11a);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x11a);
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0x11b);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x11b);
    }
    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0 || fn_80237F74(ctx, param3, 0x11) == 1 ||
        fn_80237F74(ctx, param3, 0x13) == 1) {
        handle = fn_80239984(handle, ctx, 0x11c);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x11c);
    }
    return handle;
}
/* Address: 0x80247DE0 | Size: 0x1C0 (448 bytes) */
u32 fightTrainerAiWazaValuePoizunteeru(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0x116);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x116);
    handle = fn_80239984(handle, ctx, 0x117);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x117);
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0x118);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x118);
    }
    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0 || fn_80237F74(ctx, param3, 0x11) == 1 ||
        fn_80237F74(ctx, param3, 0x13) == 1) {
        handle = fn_80239984(handle, ctx, 0x119);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x119);
    }
    return handle;
}
/* Address: 0x80247FA0 | Size: 0x1D0 (464 bytes) */
u32 fightTrainerAiWazaValueTuikaDoku(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 battleParam = param2;
    u32 trainerParam = param1;
    void* battleCtx = ctx;
    u32 target = param3;
    u32 handle;
    s32 quotient;
    u32 pokemonPtr;
    u32 statusValue;

    statusValue = fightAiGetWazaAddFightKoukaAvg(battleCtx, battleParam);
    quotient = ((s32)statusValue) / fightTrainerGetStatus(0, 0x113, 0x3e, 0);
    handle = fightTrainerAiAddValue(0, quotient);
    ;
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam)), 0, 0, battleParam, 0, 0x113, quotient);
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, battleCtx, 1, 1, PC_PTR(0x10e), PC_PTR(trainerParam)) == 1) {
        handle = fn_80239984(handle, battleCtx, 0x114);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam)), 0, 0, battleParam, 0, 0x114);
    }
    if (((fightTrainerAiIsJoutaiNormal(battleCtx, target) == 0) || (fn_80237F74(battleCtx, target, 0x11) == 1)) ||
        (fn_80237F74(battleCtx, target, 0x13) == 1)) {
        statusValue = fightAiGetWazaAddFightKoukaAvg(battleCtx, battleParam);
        quotient = ((s32)statusValue) / fightTrainerGetStatus(0, 0x115, 0x3e, 0);
        handle = fightTrainerAiAddValue(handle, quotient);
        pokemonPtr = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam));
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(battleCtx), pokemonPtr, 0, 0, battleParam, 0, 0x115, quotient);
    }
    return handle;
}
/* Address: 0x80248170 | Size: 0x150 (336 bytes) */
u32 fightTrainerAiWazaValueHandou(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle = 0;

    if ((fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) & 0xFF) == 1) {
        {
            u32 nextHandle = fn_80239984(0, ctx, 0x110);
            handle = nextHandle;
        }
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x110);
    }
    if ((fn_80237F74(ctx, param1, 0x36) & 0xFF) == 1) {
        {
            u32 nextHandle = fn_80239984(handle, ctx, 0x111);
            handle = nextHandle;
        }
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x111);
    }
    handle = fn_80239984(handle, ctx, 0x112);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x112);
    return handle;
}
/* Address: 0x802482C0 | Size: 0x1A0 (416 bytes) */
s32 fightTrainerAiWazaValueBakuretuPanti(void* ctx, void* param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8();
#endif
    s32 h = 0;
    u32 r28v;

    r28v = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(PC_PTR32_IMPLICIT(param1), fightFloorGetStatus(0, 0, 0x14, 0));
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1d)) == 1 &&
        (u16)r28v == (u16)fightOutPokemonGetJoutaiParam(PC_PTR(param3), PC_PTR(0x1d))) {
        h = fn_80239984(0, ctx, 0x10a);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x10a);
    }
    r28v = fn_80239984(h, ctx, 0x10b);
    fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x10b);
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(9)) == 1 || fn_80237F74(ctx, param3, 0x14) == 1) {
        r28v = fn_80239984(r28v, ctx, 0x10c);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x10c);
    }
    return r28v;
}
/* Address: 0x80248460 | Size: 0x22C (556 bytes) */
s32 fightTrainerAiWazaValueDenjihou(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTargetGetTragetPtrToRelativeHostSideFightTargetId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetFightTrainerFightOutPokemonPtrAry();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetJoutaiParam();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai();
#endif
    extern s32 fightTrainerAiIsFasterBasis();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8();
#endif
    u32 buf[8];
    s32 handle = 0;
    u16 someVal;
    u16 count;
    u16 i;

    someVal = fightTargetGetTragetPtrToRelativeHostSideFightTargetId(param1, fightFloorGetStatus(0, 0, 0x14, 0));
    count = fightFloorGetFightTrainerFightOutPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), buf, 1, 1);

    for (i = 0; i < count; i++) {
        if (buf[i] != 0 && fightTrainerAiIsFasterBasis(ctx, param3, buf[i]) > 0) {
            break;
        }
    }

    if (i < count) {
        handle = (s32)fn_80239984(0, ctx, 0x107);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x107);
    }

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1D)) == 1 && someVal == fightOutPokemonGetJoutaiParam(PC_PTR(param3), PC_PTR(0x1D))) {
        handle = (s32)fn_80239984(handle, ctx, 0x108);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x108);
    }

    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0 || fn_80237F74(ctx, param3, 7) == 1 ||
        fn_80237F74(ctx, param3, 0x13) == 1) {
        handle = (s32)fn_80239984(handle, ctx, 0x109);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x109);
    }

    return handle;
}
/* Address: 0x8024868C | Size: 0x1D0 (464 bytes) */
u32 fightTrainerAiWazaValueTuikaMahi(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 battleParam = param2;
    u32 trainerParam = param1;
    void* battleCtx = ctx;
    u32 target = param3;
    u32 handle;
    u32 pokemonPtr;
    u32 statusValue;
    s32 quotient;

    statusValue = fightAiGetWazaAddFightKoukaAvg(battleCtx, battleParam);
    quotient = (s32)statusValue / fightTrainerGetStatus(0, 0x104, 0x3e, 0);
    handle = fightTrainerAiAddValue(0, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam)), 0, 0, battleParam, 0, 0x104, quotient);
    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, battleCtx, 1, 1, PC_PTR(0x10e), PC_PTR(trainerParam)) == 1) {
        handle = fn_80239984(handle, battleCtx, 0x105);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(battleCtx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam)), 0, 0, battleParam, 0, 0x105);
    }
    if (fightTrainerAiIsJoutaiNormal(battleCtx, target) == 0 || fn_80237F74(battleCtx, target, 7) == 1 ||
        fn_80237F74(battleCtx, target, 0x13) == 1) {
        statusValue = fightAiGetWazaAddFightKoukaAvg(battleCtx, battleParam);
        quotient = (s32)statusValue / fightTrainerGetStatus(0, 0x106, 0x3e, 0);
        handle = fightTrainerAiAddValue(handle, quotient);
        pokemonPtr = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(trainerParam));
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(battleCtx), pokemonPtr, 0, 0, battleParam, 0, 0x106, quotient);
    }
    return handle;
}
/* Address: 0x8024885C | Size: 0x2C0 (704 bytes) */
u32 fightTrainerAiWazaValueTuikaKoori(void* ctx, u32 param1, u32 param2, u32 param3) {
    typedef union PokemonRef {
        u32 value;
        void* ptr;
    } PokemonRef;
    PokemonRef pokemon[23];
    u32 handle;
    u32 value;
    u16 count;
    u16 i;

    count = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), pokemon, 0, 1);
    value = fightAiGetWazaAddFightKoukaAvg(ctx, param2);
    value = (s32)value / fightTrainerGetStatus(0, 0x100, 0x3e, 0);
    handle = fightTrainerAiAddValue(0, value);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x100, value);

    if (fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0x101);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x101);
    }

    for (i = 0; i < count; i++) {
        if (pokemonGetStatus(PC_PTR(param3), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0) != pokemon[i].ptr && fn_802384B4(ctx, pokemon[i].value, 7) == 1) {
            value = fightAiGetWazaAddFightKoukaAvg(ctx, param2);
            value = (s32)value / fightTrainerGetStatus(0, 0x102, 0x3e, 0);
            handle = fightTrainerAiAddValue(handle, value);
            fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x102, value);
            break;
        }
    }

    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 0 || fn_80237F74(ctx, param3, 0x28) == 1 ||
        fn_80237F74(ctx, param3, 0x13) == 1) {
        value = fightAiGetWazaAddFightKoukaAvg(ctx, param2);
        value = (s32)value / fightTrainerGetStatus(0, 0x103, 0x3e, 0);
        handle = fightTrainerAiAddValue(handle, value);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x103, value);
    }
    return handle;
}
/* Address: 0x80248B1C | Size: 0x220 (544 bytes) */
u32 fightTrainerAiWazaValueBureizukikku(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0xfb);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xfb);
    handle = fn_80239984(handle, ctx, 0xfc);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xfc);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xfd);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xfd);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(7)) == 1) {
        handle = fn_80239984(handle, ctx, 0xfe);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xfe);
    }
    if ((u8)fightTrainerAiIsJoutaiNormal(ctx, param3) == 0 || (u8)fn_80237F74(ctx, param3, 0x29) == 1 ||
        (u8)fn_80237F74(ctx, param3, 0x13) == 1) {
        handle = fn_80239984(handle, ctx, 0xff);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xff);
    }
    return handle;
}
/* Address: 0x80248D3C | Size: 0x288 (648 bytes) */
u32 fightTrainerAiWazaValueJikokaitou(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    s32 denom;
    u8 numerator;
    s32 quotient;
    u32 handle;

    handle = 0;
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(7)) == 1) {
        handle = fn_80239984(0, ctx, 0xf6);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf6);
    }
    quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xf7, 0x3e, 0);
    quotient = quotient / denom;
    handle = fightTrainerAiAddValue(handle, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf7, quotient);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf8);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf8);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(7)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf9);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf9);
    }
    if ((u8)fightTrainerAiIsJoutaiNormal(ctx, param3) != 0) {
        if ((u8)fn_80237F74(ctx, param3, 0x29) != 1) {
            if ((u8)fn_80237F74(ctx, param3, 0x13) != 1) {
                goto done;
            }
        }
    }
    numerator = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xfa, 0x3e, 0);
    quotient = numerator / denom;
    handle = fightTrainerAiAddValue(handle, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xfa, quotient);
done:
    return handle;
}
/* Address: 0x80248FC4 | Size: 0x49C (1180 bytes) */
u32 fightTrainerAiWazaValueHonoonouzu(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightOutPokemonPtrToFightTrainerPtr(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetIrekaeFightPokemon(u32, u16*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u32 fn_80235BE4(void*, u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u16 tmp;
    u32 found;
    u32 handle;
    u32 pokemon;
    u32 status;

    handle = 0;
    status = fn_80235BE4(ctx, 0, param3, 0);
    pokemon = PC_PTR32_IMPLICIT(fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, param3));
    tmp = 0;
    found = PC_PTR32_IMPLICIT(fightTrainerGetIrekaeFightPokemon(pokemon, &tmp));
    if (found == 0) {
        status = 1;
    }
    if ((u8)status == 0) {
        handle = fn_80239984(0, ctx, 0xeb);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xeb);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(3)) == 1) {
        handle = fn_80239984(handle, ctx, 0xec);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xec);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(4)) == 1) {
        handle = fn_80239984(handle, ctx, 0xed);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xed);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(6)) == 1) {
        handle = fn_80239984(handle, ctx, 0xee);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xee);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1c)) == 1) {
        handle = fn_80239984(handle, ctx, 0xef);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xef);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(9)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf0);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf0);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0xa)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf1);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf1);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x18)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf2);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf2);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1e)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf3);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf3);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(7)) == 1) {
        handle = fn_80239984(handle, ctx, 0xf4);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf4);
    }
    if ((u8)status != 0) {
        handle = fn_80239984(handle, ctx, 0xf5);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xf5);
    }
    return handle;
}
/* Address: 0x80249460 | Size: 0x218 (536 bytes) */
u32 fightTrainerAiWazaValueIryokuHonoo(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    s32 denom;
    s32 quotient;
    u32 handle;

    quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xe7, 0x3e, 0);
    quotient = quotient / denom;
    handle = fightTrainerAiAddValue(0, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe7, quotient);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xe8);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe8);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(7)) == 1) {
        handle = fn_80239984(handle, ctx, 0xe9);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe9);
    }
    if ((u8)fightTrainerAiIsJoutaiNormal(ctx, param3) == 0 || (u8)fn_80237F74(ctx, param3, 0x29) == 1) {
        quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
        denom = fightTrainerGetStatus(0, 0xea, 0x3e, 0);
        quotient = quotient / denom;
        handle = fightTrainerAiAddValue(handle, quotient);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xea, quotient);
    }
    return handle;
}
/* Address: 0x80249678 | Size: 0x248 (584 bytes) */
u32 fightTrainerAiWazaValueTuikouKonran(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;
    s32 quotient;
    s32 denom;
    s32 finalQuotient;

    quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xe3, 0x3e, 0);
    quotient = quotient / denom;
    handle = fightTrainerAiAddValue(0, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe3, quotient);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xe4);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe4);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(9)) == 1) {
        quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
        denom = fightTrainerGetStatus(0, 0xe5, 0x3e, 0);
        quotient = quotient / denom;
        handle = fightTrainerAiAddValue(handle, quotient);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe5, quotient);
    }
    if ((u8)fn_80237F74(ctx, param3, 0x13) == 1 || (u8)fn_80237F74(ctx, param3, 0x14) == 1) {
        quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
        denom = fightTrainerGetStatus(0, 0xe6, 0x3e, 0);
        finalQuotient = quotient / denom;
        handle = fightTrainerAiAddValue(handle, finalQuotient);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe6, finalQuotient);
    }
    return handle;
}
/* Address: 0x802498C0 | Size: 0x1F4 (500 bytes) */
u32 fightTrainerAiWazaValueDorokake(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u32 fightTrainerAiGetAbicntAverage(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0xdf);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xdf);
    if ((u8)fightTrainerAiGetAbicntAverage(ctx, param3) == 0) {
        handle = fn_80239984(handle, ctx, 0xe0);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe0);
    }
    if ((u8)fn_80237F74(ctx, param3, 0x1d) == 1 || (u8)fn_80237F74(ctx, param3, 0x13) == 1 ||
        (u8)fn_80237F74(ctx, param3, 0x49) == 1 || (u8)fn_80237F74(ctx, param3, 0x33) == 1) {
        handle = fn_80239984(handle, ctx, 0xe1);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe1);
    }
    if ((u8)fightTrainerAiGetAbicntAverage(ctx, param3) <= 4) {
        handle = fn_80239984(handle, ctx, 0xe2);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xe2);
    }
    return handle;
}
/* Address: 0x80249AB4 | Size: 0x278 (632 bytes) */
u32 fightTrainerAiWazaValueTuikouMeityuuDaun(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u32 fightTrainerAiGetAbicntAverage(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;
    s32 quotient;
    s32 denom;
    s32 finalQuotient;

    quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xdb, 0x3e, 0);
    quotient = quotient / denom;
    handle = fightTrainerAiAddValue(0, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xdb, quotient);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xdc);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xdc);
    }
    if ((u8)fightTrainerAiGetAbicntAverage(ctx, param3) == 0) {
        quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
        denom = fightTrainerGetStatus(0, 0xdd, 0x3e, 0);
        quotient = quotient / denom;
        handle = fightTrainerAiAddValue(handle, quotient);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xdd, quotient);
    }
    if ((u8)fn_80237F74(ctx, param3, 0x1d) == 1 || (u8)fn_80237F74(ctx, param3, 0x13) == 1 ||
        (u8)fn_80237F74(ctx, param3, 0x49) == 1 || (u8)fn_80237F74(ctx, param3, 0x33) == 1) {
        quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
        denom = fightTrainerGetStatus(0, 0xde, 0x3e, 0);
        finalQuotient = quotient / denom;
        handle = fightTrainerAiAddValue(handle, finalQuotient);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xde, finalQuotient);
    }
    return handle;
}
/* Address: 0x80249D2C | Size: 0x25C (604 bytes) */
u32 fightTrainerAiWazaValueTuikouTokubouDaun(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetAbicntSpeDef(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightAiGetWazaAddFightKoukaAvg(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiAddValue(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239CCC(u32, void*, u32, u32, u32, u32, u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    s32 denom;
    u8 numerator;
    s32 quotient;
    s32 finalQuotient;
    u32 handle;

    quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xd7, 0x3e, 0);
    quotient = quotient / denom;
    handle = fightTrainerAiAddValue(0, quotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd7, quotient);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xd8);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd8);
    }
    if ((u8)fightTrainerAiGetAbicntSpeDef(PC_PTR32_IMPLICIT(ctx), param3) == 0) {
        quotient = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
        denom = fightTrainerGetStatus(0, 0xd9, 0x3e, 0);
        quotient = quotient / denom;
        handle = fightTrainerAiAddValue(handle, quotient);
        fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd9, quotient);
    }
    if ((u8)fn_80237F74(ctx, param3, 0x1d) != 1) {
        if ((u8)fn_80237F74(ctx, param3, 0x13) != 1) {
            if ((u8)fn_80237F74(ctx, param3, 0x49) != 1) {
                goto done;
            }
        }
    }
    numerator = (s32)(fightAiGetWazaAddFightKoukaAvg(ctx, param2) & 0xff);
    denom = fightTrainerGetStatus(0, 0xda, 0x3e, 0);
    finalQuotient = numerator / denom;
    handle = fightTrainerAiAddValue(handle, finalQuotient);
    fn_80239CCC(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xda, finalQuotient);
done:
    return handle;
}
/* Address: 0x80249F88 | Size: 0x1E8 (488 bytes) */
u32 fightTrainerAiWazaValueTuikouTokukouDaun(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(u32, void*, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
    extern u32 fightTrainerAiGetAbicntSpeAtk(void*, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80237F74(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = fn_80239984(0, ctx, 0xd3);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd3);
    if ((u8)fightFloorGetFightTrainerFightOutPokemonIsFightActionAttackWazaOut(0, ctx, 1, 1, PC_PTR(0x10e), PC_PTR(param1)) == 1) {
        handle = fn_80239984(handle, ctx, 0xd4);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd4);
    }
    if ((u8)fightTrainerAiGetAbicntSpeAtk(ctx, param3) == 0) {
        handle = fn_80239984(handle, ctx, 0xd5);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd5);
    }
    if ((u8)fn_80237F74(ctx, param3, 0x1d) != 1) {
        if ((u8)fn_80237F74(ctx, param3, 0x13) != 1) {
            if ((u8)fn_80237F74(ctx, param3, 0x49) != 1) {
                goto done;
            }
        }
    }
    handle = fn_80239984(handle, ctx, 0xd6);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0xd6);
done:
    return handle;
}
