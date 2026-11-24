/**
 * @file gs_range_800096B4.c
 * @brief gs-engine, 0x800096B4 - 0x8000BE74.
 *
 * Boundary evidence-verified from asm (sdata clusters, callee families,
 * static linkage, call chains) - mixed-block split pass, 2026-07-01.
 * dbgMenuPokemonEdit (0x800096B4, size 0x23E0) is the debug Pokemon editor; the
 * ten small debug-menu callback functions below call it.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

extern void menuDbgItemCreate(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpen(u32 a, u32 b);
#endif
extern void heroAddPokecoupon(u32 a, s32 b);
extern void heroAddPokedoru(u32 a, s32 b);
extern u8 fightIsBattleActive(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInit(u8* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroCatchPokemon(u32 a, u8* b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroGetStatus(u8* a, u32 b, u32 c);
#endif
extern u8 lbl_80478840;
extern u8 lbl_803A1A48[];
extern u32 lbl_80478B80;
extern u32 lbl_80478BD8;
extern u32* pokemon_seikaku_data_number;
extern u32* fight_trainer_pokemon_part_data_number;
extern u32* lbl_80478F68;
extern u32* lbl_80478F90;
extern s32 lbl_8047A290;
extern f32 lbl_8047B6D0;
extern s32 lbl_8047E700;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 size);
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 dbgMenuPokemonEdit(u32 arg0, u8 arg1, u8* arg2, u8* arg3, u8* arg4, u8* arg5) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GSmsgGetGSchar();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightTrainerSetStatus(s32, u32, s32, u16, u32);
#endif
    extern u8 fn_8001E224();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 joutaiDataBiosGetMaxCount();
#endif
    extern void pokemonSetJoutaiKaisuu(u32, u16, s8);
    extern void fn_801217B4(u32, u16, s8);
    extern s8 fn_8012182C();
    extern s8 pokemonGetJoutaiKaisuu();
    extern void fn_8012190C(u32, u16, s16);
    extern s16 pokemonGetJoutaiStack();
    extern void pokemonWriteJoutaiDataId(u32, u16, s32);
    extern u8 pokemonCheckWriteJoutaiDataId(u32, u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitPersistentJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_80142984(u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 gamedataGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, ...);
#endif
    extern void menuSubCloseNumberInput();
    extern s8 pokemonCheckRare();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonCreate();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonCreateRndFit(u32, s8, s8, s8, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonDoItemSoubi(u32, u16, s32);
#endif
    extern u16 pokemonGetJoutaiDataId();
    extern u32 pokemonGetNowLevelToExp();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonGetSoubiItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonGetStatus(u32, u16, s32, u16);
#endif
    extern void pokemonGrowBasisStatus();
    extern void pokemonInitDarkPokemon();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitJoutai();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsDarkPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetCatchStatus();
#endif
    extern void pokemonSetDarkPokemonStatus(u32, u16);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    extern void pokemonSetTokuseiFlag();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCreate(u32, s32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonWazaGetMaxPP();
#endif
    extern void pokemonWazaInit();

    s32 catchSeed = lbl_8047E700;
    const u16 stat_group_1[6] = { 0x93, 0x94, 0x95, 0x96, 0x97, 0x98 };
    const u16 stat_group_2[6] = { 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92 };
    struct SavedPokemon {
        u8 bytes[0x138];
    } saved;
    s32 input;
    s32 selection;
    s32 delta;
    u32 trainerId;
    u8 changed;
    s32 value;
    s32 i;

    if (arg2 != NULL) {
        *arg2 = 0;
    }
    if (arg3 != NULL) {
        *arg3 = 0;
    }
    if (arg4 != NULL) {
        *arg4 = 0;
    }
    if (arg5 != NULL) {
        *arg5 = 0;
    }

    saved = *(struct SavedPokemon*)PC_PTR(arg0);

    if ((u8)pokemonCheckValid(PC_PTR(arg0)) == 0) {
        pokemonCreate(PC_PTR(arg0), 0x115, 1, PC_PTR(gamedataGetStatus(0, 1)));
        pokemonSetCatchStatus(PC_PTR(arg0), 0, 1, 0, 2, 0, PC_PTR32_IMPLICIT(&catchSeed));
    }
    trainerId = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_CONDITION_SLOT, 0) & 0xFFFF;

    for (;;) {
        lbl_8047A290 = 0;
        selection = menuOpenCustom(0xD, 0, 0, 0, 1, 2, arg0, &lbl_8047A290);
        delta = lbl_8047A290;

        if (selection == -1) {
            menuCloseCustom(0xD, 0, 1);
            *(struct SavedPokemon*)PC_PTR(arg0) = saved;
            return -1;
        }

        if (selection == -2) {
            if (menuOpen(0x44, 1) == 0) {
                break;
            }
            menuCloseCustom(0x44, 0, 1);
            continue;
        }

        if (delta == 0) {
            continue;
        }

        changed = 0;
        switch (selection) {
        case 0x0E:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SPECIES, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value >= *lbl_80478F90) {
                value = *lbl_80478F90 - 1;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SPECIES, 0, value);
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_NICKNAME, 0,
                             PC_PTR32_IMPLICIT(GSmsgGetGSchar(pokemonGetStatus(0, value, POKEMON_STATUS_SPECIES_NAME, 0))));
            pokemonSetTokuseiFlag(arg0, 0);
            changed = 1;
            if (arg2 != NULL) {
                *arg2 = 1;
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x10:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_LEVEL, 0);
            value += delta;
            if (value <= 0) {
                value = 1;
            }
            if (value > 0x64) {
                value = 0x64;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_LEVEL, 0, value);
            changed = 1;
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x12:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EXP, 0);
            if (delta == -0xFFFF) {
                value = 1;
            } else if (delta == 0xFFFF) {
                value = 0x1E8480;
            } else if (fn_8001E224(value, &input, 1, 0x32, 0x32, 0) == 0) {
                menuSubCloseNumberInput();
                break;
            } else {
                value = input;
                menuSubCloseNumberInput();
                if (value <= 0) {
                    value = 1;
                }
                if (value > 0x1E8480) {
                    value = 0x1E8480;
                }
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EXP, 0, value);
            changed = 1;
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x14:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_HP, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > (s32)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MAX_HP, 0)) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MAX_HP, 0);
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_HP, 0, value);
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x1E:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MAX_HP, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x3E7) {
                value = 0x3E7;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MAX_HP, 0, value);
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_HP, 0, value);
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x1F:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_ATTACK, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x3E7) {
                value = 0x3E7;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_ATTACK, 0, value);
            break;
        case 0x20:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_DEFENSE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x3E7) {
                value = 0x3E7;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_DEFENSE, 0, value);
            break;
        case 0x21:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SPEED, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x3E7) {
                value = 0x3E7;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SPEED, 0, value);
            break;
        case 0x22:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SP_ATTACK, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x3E7) {
                value = 0x3E7;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SP_ATTACK, 0, value);
            break;
        case 0x23:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SP_DEFENSE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x3E7) {
                value = 0x3E7;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SP_DEFENSE, 0, value);
            break;
        case 0x24:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_HP, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x1F) {
                value = 0x1F;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_HP, 0, value);
            changed = 1;
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x25:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_ATTACK, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x1F) {
                value = 0x1F;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_ATTACK, 0, value);
            changed = 1;
            break;
        case 0x26:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_DEFENSE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x1F) {
                value = 0x1F;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_DEFENSE, 0, value);
            changed = 1;
            break;
        case 0x27:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_SPEED, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x1F) {
                value = 0x1F;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_SPEED, 0, value);
            changed = 1;
            break;
        case 0x28:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_SP_ATTACK, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x1F) {
                value = 0x1F;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_SP_ATTACK, 0, value);
            changed = 1;
            break;
        case 0x29:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_SP_DEFENSE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0x1F) {
                value = 0x1F;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_IV_SP_DEFENSE, 0, value);
            changed = 1;
            break;
        case 0x2A:
            if (delta > 0) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
                if ((u32)value >= 0x1FE) {
                    break;
                }
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0, value);
            changed = 1;
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x2B:
            if (delta > 0) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
                if ((u32)value >= 0x1FE) {
                    break;
                }
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0, value);
            changed = 1;
            break;
        case 0x2C:
            if (delta > 0) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
                if ((u32)value >= 0x1FE) {
                    break;
                }
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0, value);
            changed = 1;
            break;
        case 0x2D:
            if (delta > 0) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
                if ((u32)value >= 0x1FE) {
                    break;
                }
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0, value);
            changed = 1;
            break;
        case 0x2E:
            if (delta > 0) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
                if ((u32)value >= 0x1FE) {
                    break;
                }
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0, value);
            changed = 1;
            break;
        case 0x2F:
            if (delta > 0) {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_HP, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_ATTACK, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
                value += pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SPEED, 0);
                if ((u32)value >= 0x1FE) {
                    break;
                }
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EV_SP_DEFENSE, 0, value);
            changed = 1;
            break;
        case 0x31:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE, 0);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                pokemonWazaInit(arg0, 0);
                break;
            }
            if (value >= 0x163) {
                value = 0x162;
            }
            pokemonWazaCreate(PC_PTR(arg0), 0, (u16)value);
            break;
        case 0x32:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE, 1);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                pokemonWazaInit(arg0, 1);
                break;
            }
            if (value >= 0x163) {
                value = 0x162;
            }
            pokemonWazaCreate(PC_PTR(arg0), 1, (u16)value);
            break;
        case 0x33:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE, 2);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                pokemonWazaInit(arg0, 2);
                break;
            }
            if (value >= 0x163) {
                value = 0x162;
            }
            pokemonWazaCreate(PC_PTR(arg0), 2, (u16)value);
            break;
        case 0x34:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE, 3);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                pokemonWazaInit(arg0, 3);
                break;
            }
            if (value >= 0x163) {
                value = 0x162;
            }
            pokemonWazaCreate(PC_PTR(arg0), 3, (u16)value);
            break;
        case 0x35:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 0) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 0);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > pokemonWazaGetMaxPP(PC_PTR(arg0), 0)) {
                value = pokemonWazaGetMaxPP(PC_PTR(arg0), 0);
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 0, (u8)value);
            break;
        case 0x36:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 1) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 1);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > pokemonWazaGetMaxPP(PC_PTR(arg0), 1)) {
                value = pokemonWazaGetMaxPP(PC_PTR(arg0), 1);
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 1, (u8)value);
            break;
        case 0x37:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 2) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 2);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > pokemonWazaGetMaxPP(PC_PTR(arg0), 2)) {
                value = pokemonWazaGetMaxPP(PC_PTR(arg0), 2);
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 2, (u8)value);
            break;
        case 0x38:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 3) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 3);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > pokemonWazaGetMaxPP(PC_PTR(arg0), 3)) {
                value = pokemonWazaGetMaxPP(PC_PTR(arg0), 3);
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 3, (u8)value);
            break;
        case 0x39:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 0) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 0);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 3) {
                value = 3;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 0, (u8)value);
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 0, pokemonWazaGetMaxPP(PC_PTR(arg0), 0));
            break;
        case 0x3A:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 1) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 1);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 3) {
                value = 3;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 1, (u8)value);
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 1, pokemonWazaGetMaxPP(PC_PTR(arg0), 1));
            break;
        case 0x3B:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 2) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 2);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 3) {
                value = 3;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 2, (u8)value);
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 2, pokemonWazaGetMaxPP(PC_PTR(arg0), 2));
            break;
        case 0x3C:
            if ((u8)pokemonWazaCheckValid(PC_PTR(arg0), 3) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 3);
            if (value == 0x164 || value == 0x165) {
                break;
            }
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 3) {
                value = 3;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, 3, (u8)value);
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP, 3, pokemonWazaGetMaxPP(PC_PTR(arg0), 3));
            break;
        case 0x3E:
            if ((u8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_ABILITY_SLOT, 0) == 1) {
                value = 0;
            } else {
                value = 1;
            }
            if (pokemonGetStatus(0, pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SPECIES, 0), POKEMON_STATUS_SPECIES_ABILITY, 1) == 0) {
                value = 0;
            }
            pokemonSetTokuseiFlag(arg0, value);
            if (arg4 != NULL) {
                *arg4 = 1;
            }
            break;
        case 0x40:
            value = pokemonGetSoubiItemDataId(arg0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value >= lbl_80478BD8) {
                value = lbl_80478BD8 - 1;
            }
            pokemonDoItemSoubi(PC_PTR(arg0), (u16)value, 1);
            if (arg5 != NULL) {
                *arg5 = 1;
            }
            break;
        case 0x42:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_PERSONALITY, 0);
            if (delta == -0xFFFF) {
                value = 0;
            } else if (delta == 0xFFFF) {
                value = -1;
            } else if (fn_8001E224(value, &input, 1, 0x32, 0x32, 0) == 0) {
                menuSubCloseNumberInput();
                break;
            } else {
                value = input;
                menuSubCloseNumberInput();
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_PERSONALITY, 0, value);
            changed = 1;
            if (arg2 != NULL) {
                *arg2 = 1;
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x43: {
            u32 pid;

            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_GENDER, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value >= lbl_80478B80) {
                value = lbl_80478B80 - 1;
            }
            pid = pokemonCreateRndFit(PC_PTR(arg0), (s8)value,
                                                 (s8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_NATURE, 0),
                                                 (s8)pokemonCheckRare(arg0),
                                                 pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_OT_ID, 0));
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_PERSONALITY, 0, pid);
            changed = 1;
            if (arg2 != NULL) {
                *arg2 = 1;
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        }
        case 0x44:
            value = (u8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_NATURE, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value >= *pokemon_seikaku_data_number) {
                value = *pokemon_seikaku_data_number - 1;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_PERSONALITY, 0,
                             pokemonCreateRndFit(PC_PTR(arg0),
                                                 (s8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_GENDER, 0),
                                                 (s8)value,
                                                 (s8)pokemonCheckRare(arg0),
                                                 pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_OT_ID, 0)));
            changed = 1;
            if (arg2 != NULL) {
                *arg2 = 1;
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x45:
            if (delta > 0) {
                value = 1;
            } else {
                value = 0;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_PERSONALITY, 0,
                             pokemonCreateRndFit(PC_PTR(arg0),
                                                 (s8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_GENDER, 0),
                                                 (s8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_NATURE, 0),
                                                 (s8)value,
                                                 pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_OT_ID, 0)));
            changed = 1;
            if (arg2 != NULL) {
                *arg2 = 1;
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x47:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_OT_ID, 0);
            if (delta == -0xFFFF) {
                value = 0;
            } else if (delta == 0xFFFF) {
                value = -1;
            } else if (fn_8001E224(value, &input, 1, 0x32, 0x32, 0) == 0) {
                menuSubCloseNumberInput();
                break;
            } else {
                value = input;
                menuSubCloseNumberInput();
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_OT_ID, 0, value);
            if (arg2 != NULL) {
                *arg2 = 1;
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x49:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_FRIENDSHIP, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_FRIENDSHIP, 0, value);
            break;
        case 0x4B:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_POKERUS, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_POKERUS, 0, value);
            break;
        case 0x4D:
            value = pokemonGetJoutaiDataId(arg0);
            if (value == 0) {
                value = delta + 2;
            } else {
                value += delta;
            }
            if (value < 3) {
                value = 0;
            }
            if (value > 8) {
                value = 8;
            }
            pokemonInitJoutai(PC_PTR(arg0));
            if (value != 0 && pokemonCheckWriteJoutaiDataId(arg0, value) == 2) {
                pokemonWriteJoutaiDataId(arg0, value, 0);
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x4E:
            value = pokemonGetJoutaiDataId(arg0);
            if (value == 0) {
                break;
            }
            i = pokemonGetJoutaiKaisuu(arg0, value);
            if (value != 4) {
                if (i < 0) {
                    break;
                }
                i += delta;
                if (i < 0) {
                    i = 0;
                }
                if (i > 0x10) {
                    i = 0x10;
                }
                pokemonSetJoutaiKaisuu(arg0, value, i);
                if (fn_8012182C(arg0, value) > i) {
                    fn_801217B4(arg0, value, i);
                }
            } else {
                i = pokemonGetJoutaiStack(arg0, value);
                i += delta;
                if (i < 0) {
                    i = 0;
                }
                if (i > joutaiDataBiosGetMaxCount(value)) {
                    i = joutaiDataBiosGetMaxCount(value);
                }
                fn_8012190C(arg0, value, i);
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0x4F: {
            s32 max;

            value = pokemonGetJoutaiDataId(arg0);
            if (value == 0) {
                break;
            }
            max = pokemonGetJoutaiKaisuu(arg0, value);
            if (value == 4 || max < 0) {
                break;
            }
            i = fn_8012182C(arg0, value);
            i += delta;
            if (i < 0) {
                i = 0;
            }
            if (i > pokemonGetJoutaiKaisuu(arg0, value)) {
                i = pokemonGetJoutaiKaisuu(arg0, value);
            }
            fn_801217B4(arg0, value, i);
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        }
        case 0xA7B:
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_ID, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value >= *lbl_80478F68) {
                value = *lbl_80478F68 - 1;
            }
            if (value == 0) {
                pokemonInitDarkPokemon(arg0);
            } else {
                pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_ID, 0, value);
                pokemonSetDarkPokemonStatus(arg0, value);
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0xA7D:
            if ((u16)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_ID, 0) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_HEART_GAUGE, 0);
            if (delta == -0xFFFF) {
                value = -1;
            } else if (delta == 0xFFFF) {
                value = 0x639C;
            } else if (fn_8001E224(value, &input, 0, 0x32, 0x32, 0) == 0) {
                menuSubCloseNumberInput();
                break;
            } else {
                value = input;
                menuSubCloseNumberInput();
                if (value < -1) {
                    value = -1;
                }
                if ((f32)value > lbl_8047B6D0) {
                    value = 0x639C;
                }
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_HEART_GAUGE, 0, value);
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0xA83:
            if ((u8)pokemonIsDarkPokemon(PC_PTR(arg0)) == 0) {
                break;
            }
            if (delta > 0) {
                pokemonWriteJoutaiDataId(arg0, 0x3E, 0);
            } else {
                pokemonInitPersistentJoutai(PC_PTR(arg0), 0x3E);
            }
            if (arg3 != NULL) {
                *arg3 = 1;
            }
            break;
        case 0xA7F:
            if ((u16)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_ID, 0) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_STORED_EXP, 0);
            if (delta == -0xFFFF) {
                value = 0;
            } else if (delta == 0xFFFF) {
                value = 0x1E8480;
            } else if (fn_8001E224(value, &input, 1, 0x32, 0x32, 0) == 0) {
                menuSubCloseNumberInput();
                break;
            } else {
                value = input;
                menuSubCloseNumberInput();
                if (value < 0) {
                    value = 0;
                }
                if (value > 0x1E8480) {
                    value = 0x1E8480;
                }
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_STORED_EXP, 0, value);
            break;
        case 0xA81:
            if ((u16)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_ID, 0) == 0) {
                break;
            }
            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_STORED_FRIENDSHIP, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 0xFF) {
                value = 0xFF;
            }
            pokemonSetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_STORED_FRIENDSHIP, 0, value);
            break;
        case 0x5F8:
            if (trainerId == 0 || arg1 != 1) {
                break;
            }
            value = (u8)fightTrainerGetStatus(0, trainerId, 0x1C, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value > 3) {
                value = 3;
            }
            fightTrainerSetStatus(0, trainerId, 0x1C, 0, (u8)value);
            break;
        case 0x5F9:
            if (trainerId == 0 || arg1 != 1) {
                break;
            }
            value = fightTrainerGetStatus(0, trainerId, 0x1D, 0);
            value += delta;
            if (value < 0) {
                value = 0;
            }
            if (value >= *fight_trainer_pokemon_part_data_number) {
                value = *fight_trainer_pokemon_part_data_number - 1;
            }
            fightTrainerSetStatus(0, trainerId, 0x1D, 0, (u16)value);
            break;
        }

        if (changed == 1) {
            pokemonSetTokuseiFlag(arg0, (u8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_ABILITY_SLOT, 0));
            if (selection == 0x10) {
                value = pokemonGetNowLevelToExp(arg0);
            } else {
                value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_EXP, 0);
            }
            pokemonGrowBasisStatus(arg0, value);
        }
    }

    menuCloseCustom(0x44, 0, 1);
    menuCloseCustom(0xD, 0, 1);

    if ((u8)pokemonCheckValid(PC_PTR(arg0)) == 0) {
        pokemonInit((u8*)PC_PTR(arg0));
    } else {
        if (fn_80142984(pokemonGetSoubiItemDataId(arg0)) == 0) {
            pokemonDoItemSoubi(PC_PTR(arg0), 0, 0);
        }
        if (trainerId != 0 && arg1 == 1) {
            s32 j;

            value = pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SPECIES, 0);
            fightTrainerSetStatus(0, trainerId, 0x15, 0, value);
            fightTrainerSetStatus(0, trainerId, 0xE, 0, pokemonGetStatus(0, value, POKEMON_STATUS_SPECIES_NAME, 0));
            fightTrainerSetStatus(0, trainerId, 0x11, 0, pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_LEVEL, 0));
            for (j = 0; j < 6; j++) {
                fightTrainerSetStatus(0, trainerId, 0xF, j,
                                      pokemonGetStatus(PC_PTR(arg0), 0, stat_group_1[j], 0));
            }
            for (j = 0; j < 6; j++) {
                fightTrainerSetStatus(0, trainerId, 0x10, j,
                                      pokemonGetStatus(PC_PTR(arg0), 0, stat_group_2[j], 0));
            }
            fightTrainerSetStatus(0, trainerId, 0x14, 0, (u8)pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_ABILITY_SLOT, 0));
            fightTrainerSetStatus(0, trainerId, 0x16, 0, pokemonGetSoubiItemDataId(arg0));
            fightTrainerSetStatus(0, trainerId, 0x19, 0, pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_FRIENDSHIP, 0));
            fightTrainerSetStatus(0, trainerId, 0x1A, 0, pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_GENDER, 0));
            fightTrainerSetStatus(0, trainerId, 0x1B, 0, pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_NATURE, 0));
            for (j = 0; j < 4; j++) {
                fightTrainerSetStatus(0, trainerId, 0x17, j,
                                      pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE, j));
                fightTrainerSetStatus(0, trainerId, 0x18, j,
                                      pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_MOVE_PP_UPS, j));
            }
            fightTrainerSetStatus(0, trainerId, 0x13, 0, pokemonGetStatus(PC_PTR(arg0), 0, POKEMON_STATUS_SHADOW_ID, 0));
        }
    }
    return 1;
}

/* dbgMenuHeroCreateItem - 0x8000BA94 | size: 0x24 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (scheduling off; peephole on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma scheduling off
#pragma peephole on
/* NAME-T1 fn_8000BA94 */ /* XD's map, dbgMenuHero.o: debug "hero" menu entries */
s32 dbgMenuHeroCreateItem(void) {
    menuDbgItemCreate();
    return 0;
}
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (scheduling on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma scheduling on

/* dbgMenuHeroIncCoupon - 0x8000BAB8 | size: 0x48 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
/* NAME-T1 fn_8000BAB8 */
s32 dbgMenuHeroIncCoupon(void) {
    s32 val = menuOpen(2, 1);
    if (val == -1) { return 0; }
    heroAddPokecoupon(0, val);
    return 0;
}

/* dbgMenuHeroIncMoney - 0x8000BB00 | size: 0x48 */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
/* NAME-T1 fn_8000BB00 */
s32 dbgMenuHeroIncMoney(void) {
    s32 val = menuOpen(2, 1);
    if (val == -1) { return 0; }
    heroAddPokedoru(0, val);
    return 0;
}
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole on) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole on

/* dbgMenuHeroPokemonAdd - 0x8000BB48 | size: 0xA4 */
s32 dbgMenuHeroPokemonAdd(void) {
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    if (lbl_80478840 != 0) {
        pokemonInit(lbl_803A1A48);
        lbl_80478840 = 0;
    }
    if (dbgMenuPokemonEdit(PC_PTR32(lbl_803A1A48), 0, 0, 0, 0, 0) < 0) { return -1; }
    heroCatchPokemon(0, lbl_803A1A48, 0, 4, 1);
    return -1;
}

/* dbgMenuHeroPokemonSelect5 - 0x8000BBEC | size: 0x6C */
/* NAME-T1 fn_8000BBEC */ /* XD's map, dbgMenuHero.o: edits party slot 5 (was dbgMenuPokemonEditSlot6, snatcherLeo's OK 2026-11-08) */
s32 dbgMenuHeroPokemonSelect5(void) {
    u32 val;
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    val = heroGetStatus(NULL, 3, 5);
    if (val == 0) { return -1; }
    return dbgMenuPokemonEdit(val, 0, 0, 0, 0, 0);
}

/* dbgMenuHeroPokemonSelect4 - 0x8000BC58 | size: 0x6C */
/* NAME-T1 fn_8000BC58 */ /* XD's map, dbgMenuHero.o: SelectN edits party slot N (this one reads slot 4) */
s32 dbgMenuHeroPokemonSelect4(void) {
    u32 val;
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    val = heroGetStatus(NULL, 3, 4);
    if (val == 0) { return -1; }
    return dbgMenuPokemonEdit(val, 0, 0, 0, 0, 0);
}

/* dbgMenuHeroPokemonSelect3 - 0x8000BCC4 | size: 0x6C */
/* NAME-T1 fn_8000BCC4 */
s32 dbgMenuHeroPokemonSelect3(void) {
    u32 val;
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    val = heroGetStatus(NULL, 3, 3);
    if (val == 0) { return -1; }
    return dbgMenuPokemonEdit(val, 0, 0, 0, 0, 0);
}

/* dbgMenuHeroPokemonSelect2 - 0x8000BD30 | size: 0x6C */
/* NAME-T1 fn_8000BD30 */
s32 dbgMenuHeroPokemonSelect2(void) {
    u32 val;
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    val = heroGetStatus(NULL, 3, 2);
    if (val == 0) { return -1; }
    return dbgMenuPokemonEdit(val, 0, 0, 0, 0, 0);
}

/* dbgMenuHeroPokemonSelect1 - 0x8000BD9C | size: 0x6C */
/* NAME-T1 fn_8000BD9C */
s32 dbgMenuHeroPokemonSelect1(void) {
    u32 val;
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    val = heroGetStatus(NULL, 3, 1);
    if (val == 0) { return -1; }
    return dbgMenuPokemonEdit(val, 0, 0, 0, 0, 0);
}

/* dbgMenuHeroPokemonSelect0 - 0x8000BE08 | size: 0x6C */
/* NAME-T1 fn_8000BE08 */
s32 dbgMenuHeroPokemonSelect0(void) {
    u32 val;
    if ((u8)fightIsBattleActive() == 1) { return -1; }
    val = heroGetStatus(NULL, 3, 0);
    if (val == 0) { return -1; }
    return dbgMenuPokemonEdit(val, 0, 0, 0, 0, 0);
}
