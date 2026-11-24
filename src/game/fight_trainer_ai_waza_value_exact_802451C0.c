/**
 * @file fight_trainer_ai_waza_value_exact_802451C0.c
 * @brief Byte-exact fightTrainerAiWazaValue.cpp range, 0x802451C0 - 0x80245FC4.
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

/* Address: 0x802451C0 | Size: 0x258 (600 bytes) */
u32 fightTrainerAiWazaValueAmagoi(void* ctx, u32 param1, u32 param2, u32 param3) {
    u32 pokemon[23];
    u32 handle;
    u8 weather;
    u16 count;
    u16 i;

    handle = 0;
    weather = fightTrainerAiGetNowTenkou(ctx, 0, 0);
    count = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), pokemon, 1, 1);

    if (weather != 2) {
        handle = fn_80239984(0, ctx, 0x14e);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x14e);
    }

    for (i = 0; i < count; i++) {
        if (fightTrainerAiGetPokemonZokusei(ctx, pokemon[i], 0xb) == 1 ||
            fightTrainerAiIsPokemonTokusei(ctx, pokemon[i], 0x21) == 1 ||
            fightTrainerAiIsPokemonTokusei(ctx, pokemon[i], 0x2c) == 1) {
            handle = fn_80239984(handle, ctx, 0x14f);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x14f);
            break;
        }
    }

    for (i = 0; i < count; i++) {
        if (fightTrainerAiGetPokemonDataId2(ctx, pokemon[i]) == 0x181) {
            handle = fn_80239984(handle, ctx, 0x150);
            fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x150);
            break;
        }
    }

    if (weather == 2) {
        handle = fn_80239984(handle, ctx, 0x151);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x151);
    }
    return handle;
}
/* Address: 0x80245418 | Size: 0x160 (352 bytes) */
u32 fightTrainerAiWazaValueNemurare(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightFloorGetFightTrainerFightPokemonPtrAry(u32, void*, u32*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
    extern u8 fn_802384B4(void*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 entries[26];
    u32 setup;
    u32 compareValue;
    u16 count;
    u16 index;

    setup = 0;
    count = fightFloorGetFightTrainerFightPokemonPtrAry(0, PC_PTR32_IMPLICIT(ctx), entries, 0, 1);
    if (fightTrainerAiIsJoutaiNormal(ctx, param3) == 1) {
        setup = fn_80239984(0, ctx, 0x14c);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x14c);
    }

    index = 0;
    while (index < count) {
        compareValue = pokemonGetStatus(PC_PTR(param3), 0, POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR, 0);
        if (compareValue == entries[index]) {
            ;
        } else {
            if (fn_802384B4(ctx, entries[index], 8) == 1) {
                setup = fn_80239984(setup, ctx, 0x14d);
                fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x14d);
                break;
            }
        }
        index++;
    }

    return setup;
}
/* Address: 0x80245578 | Size: 0x1A0 (416 bytes) */
u32 fightTrainerAiWazaValueNemuru(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E630;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsJoutaiNormal(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
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
    u32 state;

    handle = 0;
    state = fightTrainerAiGetSoubiItem(ctx, param1);
    if ((state & 0xffff) == 3 || (state & 0xffff) == 9) {
        handle = fn_80239984(0, ctx, 0x148);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x148);
    }
    if (fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), param1, -1, lbl_8047E630) == 1) {
        handle = fn_80239984(handle, ctx, 0x149);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x149);
    }
    if (fightTrainerAiIsJoutaiNormal(ctx, param1) == 0) {
        handle = fn_80239984(handle, ctx, 0x14a);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x14a);
    }
    handle = fn_80239984(handle, ctx, 0x14b);
    fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x14b);
    return handle;
}
/* Address: 0x80245718 | Size: 0x98 */
u32 fightTrainerAiWazaValueKaihuku2(void* ctx, u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern const f32 lbl_8047E630;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void*, u32, s32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle;

    handle = 0;
    if (fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), param1, -1, lbl_8047E630) == 1) {
        handle = fn_80239984(0, ctx, 0x147);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x147);
    }
    return handle;
}
/* Address: 0x802457B0 | Size: 0x168 (360 bytes) */
u32 fightTrainerAiWazaValueKaihuku1(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
    extern f32 lbl_8047E630;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerAiGetNowTenkou();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiGetHpPercent(void* a, u32 b, s32 c, f32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80239984();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8();
#endif
    s32 handle = 0;
    u32 state;

    state = fightTrainerAiGetNowTenkou(ctx, 0, 1);
    if ((u8)state == 1) {
        handle = fn_80239984(0, ctx, 0x144);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x144);
    }
    if ((u8)fightTrainerAiGetHpPercent(PC_PTR32_IMPLICIT(ctx), param1, -1, lbl_8047E630) == 1) {
        handle = fn_80239984(handle, ctx, 0x145);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x145);
    }
    if ((u8)state == 2 || (u8)state == 4 || (u8)state == 3) {
        handle = fn_80239984(handle, ctx, 0x146);
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x146);
    }
    return handle;
}
/* Address: 0x80245918 | Size: 0x98 */
u32 fightTrainerAiWazaValueKituke(void* ctx, u32 param1, u32 param2, u32 param3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightTrainerAiIsPokemonJoutai(void*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_80239984(u32, void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80239EE8(u32, void*, u32, u32, u32, u32, u32, u32);
#endif
    u32 handle = 0;

    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(5)) == 1) {
        u32 tmp = fn_80239984(0, ctx, 0x143);
        handle = tmp;
        fn_80239EE8(0xEC64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x143);
    }
    return handle;
}
/* Address: 0x802459B0 | Size: 0x44C (1100 bytes) */
u32 fightTrainerAiWazaValueOiuti(void* ctx, u32 param1, u32 param2, u32 param3) {
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
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(3)) == 1) {
        handle = fn_80239984(0, ctx, 0x139);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x139);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(4)) == 1) {
        handle = fn_80239984(handle, ctx, 0x13a);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x13a);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(6)) == 1) {
        handle = fn_80239984(handle, ctx, 0x13b);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x13b);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1c)) == 1) {
        handle = fn_80239984(handle, ctx, 0x13c);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x13c);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(9)) == 1) {
        handle = fn_80239984(handle, ctx, 0x13d);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x13d);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0xa)) == 1) {
        handle = fn_80239984(handle, ctx, 0x13e);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x13e);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x18)) == 1) {
        handle = fn_80239984(handle, ctx, 0x13f);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x13f);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x1e)) == 1) {
        handle = fn_80239984(handle, ctx, 0x140);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x140);
    }
    if ((u8)fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param3), PC_PTR(0x26)) == 1) {
        handle = fn_80239984(handle, ctx, 0x141);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x141);
    }
    if ((u8)status != 0) {
        handle = fn_80239984(handle, ctx, 0x142);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x142);
    }
    return handle;
}
/* Address: 0x80245DFC | Size: 0x14C (332 bytes) */
u32 fightTrainerAiWazaValueUezaabooru(void* ctx, u32 param1, u32 param2, u32 param3) {
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
    u32 handle;
    u32 state;

    handle = 0;
    state = fightTrainerAiGetNowTenkou(ctx, 0, 1);
    if ((u8)state == 1 || (u8)state == 2) {
        handle = fn_80239984(0, ctx, 0x136);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x136);
    }
    if ((u8)state == 4) {
        handle = fn_80239984(handle, ctx, 0x137);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x137);
    }
    if ((u8)state == 3) {
        handle = fn_80239984(handle, ctx, 0x138);
        fn_80239EE8(0xec64, PC_PTR32_IMPLICIT(ctx), PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(param1)), 0, 0, param2, 0, 0x138);
    }
    return handle;
}
/* Address: 0x80245F48 | Size: 0x7C | Pattern: field_accessor */
u32 fightTrainerAiWazaValueSizennotikara(void* ctx, u32 slot, u32 param, u32 extra) {
    extern u16 tikeiDataBiosGetWazaId(u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8023CA9C(void*, u32, u16, u32);
#endif
    u16 value;

    value = tikeiDataBiosGetWazaId((u16)fightFloorGetStatus(0, 0, 0xf, 0));
    if (value != (u16)param) {
        return fn_8023CA9C(ctx, slot, value, extra);
    }
    return 0;
}
