/**
 * @file pokemon_range_8011F77C.c
 * @brief Pokemon TU block .text 0x8011F77C-0x80120B00 with its .sdata2
 *        literal pool (0x8047CFF0-0x8047D020).
 *
 * pokemonGetDarkPokemonLevel, pokemonAddDpFormPokemonDpFilterId,
 * pokemonSetDp, pokemonGetDp, pokemonIsDarkPokemon,
 * pokemonSetDarkPokemonStatus, pokemonToMenuPokemonStatus,
 * pokemonToMenuPokemonStatusSubBar, pokemonToMenuWazaStatus,
 * pokemonCheckSetMonohiroi and pokemonAllKaihuku. The pool (0.0f, 100.0f,
 * the 80/60/40/20 dark-gauge thresholds, the int-to-float biases, -1.0f) is
 * read only by these functions, so the block links as one object; it
 * replaces the former pokemon_range_8011F77C/8011F910/8011FC14/8011FCA4
 * candidates and the pokemon_range_exact_8011FBCC/8011FC74 carves.
 *
 * Pokemon XD calls out of line what Colosseum expands here: pokemonGetDp
 * from pokemonGetDarkPokemonLevel and pokemonAddDpFormPokemonDpFilterId,
 * pokemonSetDp from pokemonSetDarkPokemonStatus, and
 * pokemonToMenuPokemonStatusSubBar from pokemonToMenuPokemonStatus
 * (GXXE01 pokemon.o: 0x8013EFA0, 0x8013F0F4 calling 0x8013F20C;
 * StarsMmd/Colo-XD-PBR-symbol-maps @ 6b51d3af, trevor403/xd-asm @ b1087f18).
 * Each callee comes after its caller in both binaries, so the callers can
 * only expand them with deferred inlining, and MWCC then emits the
 * functions in reverse source order: they are written bottom-up here and
 * come out in retail's order, with the literal pool in retail's order.
 *
 * RULE-EXCEPTION(title-path): per-unit compiler flag chosen because it
 * matches (-inline auto,deferred) and the reverse source order that follows
 * from it; pokemonCheckSetMonohiroi's table is an extern stand-in -- see
 * docs/RULE_EXCEPTIONS.md.
 */
#include "dolphin/types.h"
#include "game/constants/moves.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonGetStatus(u8* object, u16 data_id, u32 status, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus(u8* object, u32 data_id, u32 status, u32 index, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* pokemonSeikakuDataBiosGetPtr(u8 index);
#endif
extern u8* pokemonDpFilterDataBiosGetPtr(u16 index);
extern s32 pokemonDpFilterDataBiosGetValue(u8* data);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* itemDataBiosGetPtr(u16 item_data_id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 itemDataBiosGetKind(u8* data);
#endif
extern u32 itemDataBiosGetBuff(u8* data);
extern s32 pokemonSeikakuDataBiosGetReliveFightout(u8* data);
extern s32 pokemonSeikakuDataBiosGetReliveWalk(u8* data);
extern s32 pokemonSeikakuDataBiosGetReliveCall(u8* data);
extern s32 pokemonSeikakuDataBiosGetReliveSodateya(u8* data);
extern s32 pokemonSeikakuDataBiosGetReliveNadenade(u8* data);
extern u8* pokemonSeikakuRateDataBiosGetPtr(u8 index);
extern u8 pokemonSeikakuRateDataBiosGetKake(u8* data);
extern u8 pokemonSeikakuRateDataBiosGetWaru(u8* data);
extern u16 darkPokemonSaveGet4A6(u16 dark_id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiInitAry(u8* base, u16 count);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 joutaiDataBiosGetStatus(u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 joutaiIsJoutaiDataId(u8* obj, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiInitJoutaiDataId(u8* obj, u16 id);
#endif
extern u16 pokemonGetFaceSpriteId(u8* pokemon);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuSubGetPokemonSexForFightDisp(u8* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGrowDataBiosGetPtr(u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGrowDataBiosGetExp(void* ptr, u8 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharCpy(u8* dst, u32 src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetGSchar(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_8010C46C(u16 id);
#endif
extern u32 zokuseiDataBiosGetName(u16 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaGetMaxPP(u16 id, u8 up);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus(u32 obj, u16 id, u32 status, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 GSrand(void);
#endif
extern const u16 lbl_8027296C[20];
/* Defined below: with deferred inlining their bodies are expanded into the
 * functions written above them. */
f32 pokemonGetDp(u8* ptr);
void pokemonSetDp(u8* ptr, f32 dp);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonToMenuPokemonStatusSubBar(u8* pokemon, struct MenuPokemonStatus* menu);
#endif

/* Static copies of pokemon-TU helpers linked from other objects (XD keeps
 * them out of line: pokemonGetLevelToExp 0x801405F8, pokemonIsJoutaiDataId
 * 0x8013FDA4, pokemonGetTokuseiDataId 0x801416A4, pokemonWazaCheckValid
 * 0x80141114, pokemonWazaGetMaxPP 0x801411E4); every caller here expands
 * them. */
static inline u32 pokemonGrowGetLevelToExp(u8 growId, u8 level)
{
    void* growData;

    growData = pokemonGrowDataBiosGetPtr(growId);
    if (growData == NULL) {
        return 0;
    }
    return pokemonGrowDataBiosGetExp(growData, level);
}

static inline u32 pokemonGetLevelToExp(u8* pokemon, u8 level)
{
    if (pokemon == NULL) {
        return 0;
    }
    return pokemonGrowGetLevelToExp(
        (u8)pokemonGetStatus(NULL, (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0), POKEMON_STATUS_GROWTH_RATE, 0), level);
}

static inline u8 pokemonGetJoutai(u8* pokemon, u16 id)
{
    if (joutaiDataBiosGetStatus(id) != 0x7C && joutaiDataBiosGetStatus(id) != 0xC8) {
        return 0;
    }
    return joutaiIsJoutaiDataId(pokemon, id);
}

static inline u16 pokemonGetJoutaiIcon(u8* pokemon)
{
    if (pokemon == NULL) {
        return 0;
    }
    if (pokemonGetJoutai(pokemon, 3) == 1) {
        return 0x3A;
    }
    if (pokemonGetJoutai(pokemon, 4) == 1) {
        return 0x3A;
    }
    if (pokemonGetJoutai(pokemon, 5) == 1) {
        return 0x3B;
    }
    if (pokemonGetJoutai(pokemon, 6) == 1) {
        return 0x3C;
    }
    if (pokemonGetJoutai(pokemon, 7) == 1) {
        return 0x3D;
    }
    if (pokemonGetJoutai(pokemon, 8) == 1) {
        return 0x3E;
    }
    return 0;
}

typedef struct MenuPokemonStatus {
    u8 name[0x17];
    u8 level;
    u16 maxHp;
    u16 hp;
    union {
        u32 integer;
        f32 real;
    } progressMax;
    union {
        u32 integer;
        f32 real;
    } progress;
    u16 conditionIcon;
    u16 displayValue;
    u8 sex;
    u8 darkState;
    u16 status82;
} MenuPokemonStatus;

/* Colosseum's own pokemonWazaCheckValid (0x80123CD4, (Pokemon*, u16)) and
 * pokemonWazaGetMaxPP (0x80123E70, which adds 4 to the move slot for the
 * PP-up entries). */
static inline u8 pokemonWazaSlotIsValid(u8* pokemon, u16 slot)
{
    if (pokemon == NULL) {
        return 0;
    }
    if ((s32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, slot) == 0) {
        return 0;
    }
    if ((s32)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, slot) == MOVE_UNUSED_355) {
        return 0;
    }
    return 1;
}

static inline u8 pokemonGetWazaMaxPP(u8* pokemon, u16 waza)
{
    u16 id;

    if (pokemon == NULL) {
        return 0;
    }
    id = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, waza + 4);
    return wazaGetMaxPP(id, (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE_PP_UPS, waza + 4));
}

static inline u16 pokemonGetTokusei(u8* pokemon)
{
    u16 species;

    if (pokemon == NULL) {
        return 0;
    }
    species = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0);
    if (pokemonGetStatus(NULL, species, POKEMON_STATUS_SPECIES_ABILITY, 1) == 0) {
        return (u16)pokemonGetStatus(NULL, species, POKEMON_STATUS_SPECIES_ABILITY, 0);
    }
    return (u16)pokemonGetStatus(NULL, species, POKEMON_STATUS_SPECIES_ABILITY, (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_ABILITY_SLOT, 0));
}

static inline void pokemonClearJoutai(u8* pokemon, u16 id)
{
    if (joutaiDataBiosGetStatus(id) == 0x7C || joutaiDataBiosGetStatus(id) == 0xC8) {
        joutaiInitJoutaiDataId(pokemon, id);
    }
}

void pokemonAllKaihuku(u8* pokemon)
{
    u8 i;

    if (pokemon == NULL) {
        return;
    }
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_HP, 0, (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MAX_HP, 0));
    for (i = 0; i < 4; i++) {
        if (pokemonWazaSlotIsValid(pokemon, i) == 1) {
            pokemonSetStatus(pokemon, 0, POKEMON_STATUS_MOVE_PP, i, pokemonGetWazaMaxPP(pokemon, i));
        }
    }
    pokemonClearJoutai(pokemon, 3);
    pokemonClearJoutai(pokemon, 4);
    pokemonClearJoutai(pokemon, 5);
    pokemonClearJoutai(pokemon, 6);
    pokemonClearJoutai(pokemon, 7);
    pokemonClearJoutai(pokemon, 8);
}

typedef struct MonohiroiTable {
    u16 entry[20];
} MonohiroiTable;

s32 pokemonCheckSetMonohiroi(u8* pokemon)
{
    u16 table[20];
    u16 item;
    u16 i;
    u32 roll;

    /* RULE-EXCEPTION(title-path): extern named stand-in for this TU's
     * .rodata table -- see docs/RULE_EXCEPTIONS.md. The original is a local
     * initializer ({0x16, 30}, {0x17, 40} ... {0xBB, 1}: item, cumulative
     * percent); as compiler-owned data it would start this object's .rodata,
     * which MWCC aligns to 8, at 0x8027296C (4-aligned), after the
     * pokemonGetEffortFromPokemon table 0x80272948 of the same TU. */
    *(MonohiroiTable*)table = *(const MonohiroiTable*)lbl_8027296C;
    if (pokemonGetTokusei(pokemon) == 0x35 && (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_HELD_ITEM, 0) == 0 &&
        GSrand() % 10 == 0) {
        roll = GSrand() % 100;
        item = 0;
        for (i = 0; (s32)i < 20; i += 2) {
            if (table[i + 1] > roll) {
                item = table[i];
                break;
            }
        }
        if (item != 0) {
            if (pokemon != NULL) {
                pokemonSetStatus(pokemon, 0, POKEMON_STATUS_HELD_ITEM, 0, item);
            }
            return 1;
        }
    }
    return 0;
}

void pokemonToMenuWazaStatus(u8* ptr, u8* out)
{
    u8 i;
    u8* slot;

    memset(out, 0, 0x48);
    *(u32*)out = pokemonGetStatus(ptr, 0, POKEMON_STATUS_NICKNAME, 0);
    for (i = 0; i < 4; i++) {
        slot = out + i * 0xC + 4;
        if (pokemonWazaSlotIsValid(ptr, i) == 0) {
            *(u32*)(slot + 0x0) = 0;
            *(u32*)(slot + 0x4) = 0;
            *(u8*)(slot + 0xA) = 0;
            *(u8*)(slot + 0xB) = 0;
        } else {
            u16 id;
            u16 resolved;

            id = (u16)pokemonGetStatus(ptr, 0, POKEMON_STATUS_MOVE, i);
            resolved = (u16)wazaGetStatus(0, id, 3, 0);
            *(u32*)(slot + 0x0) = PC_PTR32_IMPLICIT(GSmsgGetGSchar(wazaGetStatus(0, id, 1, 0)));
            *(u32*)(slot + 0x4) = PC_PTR32_IMPLICIT(GSmsgGetGSchar(zokuseiDataBiosGetName(resolved)));
            *(u16*)(slot + 0x8) = (u16)fn_8010C46C(resolved);
            *(u8*)(slot + 0xA) = pokemonGetWazaMaxPP(ptr, i);
            *(u8*)(slot + 0xB) = (u8)pokemonGetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP, i);
        }
    }
}

void pokemonToMenuPokemonStatusSubBar(u8* pokemon, MenuPokemonStatus* menu)
{
    u8 level;
    u32 baseExp;

    level = (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_LEVEL, 0);
    baseExp = pokemonGetLevelToExp(pokemon, level);
    if ((u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0) == 0) {
        menu->progressMax.integer = pokemonGetLevelToExp(pokemon, level + 1) - baseExp;
        menu->progress.integer = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_EXP, 0) - baseExp;
    } else {
        menu->progressMax.real = darkPokemonSaveGet4A6((u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SHADOW_ID, 0));
        menu->progress.real = pokemonGetDp(pokemon);
    }
}

void pokemonToMenuPokemonStatus(u8* pokemon, MenuPokemonStatus* menu)
{
    pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0);
    GScharCpy(menu->name, PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_NICKNAME, 0)));
#ifdef TARGET_PC
    { /* OrrePort: which Pokémon this copy of its name belongs to (orre_hooks.h, OrreHook_MenuNameCopy; 2.4): the party
       * menu prints the copy, so its full name is found by the copy's address (the battle HP boxes print a later copy
       * of a copy: corrected 2026-11-16, pc_names.cpp finds those through the battle field). NULL: nothing */
        extern void (*OrreHook_MenuNameCopy)(void* copy, const void* record);
        if (OrreHook_MenuNameCopy != NULL) OrreHook_MenuNameCopy(menu->name, pokemon);
    }
#endif
    menu->level = (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_LEVEL, 0);
    menu->maxHp = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MAX_HP, 0);
    menu->hp = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_HP, 0);
#ifdef TARGET_PC
    if (menu->maxHp == 0) {
        /* PC (2026-10-29; observes only): port-gaps 30, a party menu's HP bar with max HP 0 right after a
         * purification. Which Pokémon, and do its other stats read 0 too? */
        OSReport("[menupoke] max HP 0: species %u level %u hp %u atk %d def %d spe %d shadow %u exp %u\n",
                 (unsigned)(u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0), (unsigned)menu->level, (unsigned)menu->hp,
                 (int)(s16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_ATTACK, 0), (int)(s16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_DEFENSE, 0),
                 (int)(s16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SP_DEFENSE, 0), (unsigned)(u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0),
                 (unsigned)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_EXP, 0));
    }
#endif

    pokemonToMenuPokemonStatusSubBar(pokemon, menu);

    menu->conditionIcon = pokemonGetJoutaiIcon(pokemon);
    menu->displayValue = pokemonGetFaceSpriteId(pokemon);
    menu->sex = menuSubGetPokemonSexForFightDisp(pokemon);

    if ((u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0) == 0) {
        menu->darkState = 0;
    } else if (pokemonGetJoutai(pokemon, 0x3E) == 0) {
        menu->darkState = 1;
    } else {
        menu->darkState = 2;
    }
    menu->status82 = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_HELD_ITEM, 0);
}

void pokemonSetDarkPokemonStatus(u8* pokemon, u16 dark_id)
{
    if (pokemon == NULL) {
        return;
    }
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_SHADOW_ID, 0, 0);
    pokemonSetDp(pokemon, -1.0f);
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_STORED_EXP, 0, 0);
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_STORED_FRIENDSHIP, 0, 0);
    joutaiInitAry((u8*)PC_PTR(pokemonGetStatus(pokemon, 0, POKEMON_STATUS_IS_SHADOW, 0)), 1);
    pokemonSetStatus(pokemon, 0, POKEMON_STATUS_SHADOW_ID, 0, dark_id);
    pokemonSetDp(pokemon, darkPokemonSaveGet4A6(dark_id));
}

u8 pokemonIsDarkPokemon(u8* pokemon)
{
    return (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0);
}

f32 pokemonGetDp(u8* ptr)
{
    s32 value;

    if (ptr == NULL) {
        return 0.0f;
    }
    value = pokemonGetStatus(ptr, 0, POKEMON_STATUS_HEART_GAUGE, 0);
    return (f32)value / 100.0f;
}

void pokemonSetDp(u8* ptr, f32 dp)
{
    if (ptr == NULL) {
        return;
    }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_HEART_GAUGE, 0, (u32)(s32)(100.0f * dp));
}

void pokemonAddDpFormPokemonDpFilterId(u8* ptr, u16 item_data_id, u16 filter_id)
{
    u8* seikaku_data;
    u8* rate_data;
    u8* item_data;
    s32 rate_data_id;
    u8 kake;
    u8 waru;
    f32 dp;
    f32 new_dp;

    if ((u8)pokemonGetStatus(ptr, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0) == 0) {
        return;
    }
    seikaku_data = pokemonSeikakuDataBiosGetPtr((u8)pokemonGetStatus(ptr, 0, POKEMON_STATUS_NATURE, 0));
    if (seikaku_data == NULL) {
        return;
    }
    dp = (f32)(s8)pokemonDpFilterDataBiosGetValue(pokemonDpFilterDataBiosGetPtr(filter_id));
    if (filter_id == 4) {
        item_data = itemDataBiosGetPtr(item_data_id);
        if (item_data == NULL) {
            return;
        }
        if (itemDataBiosGetKind(item_data) != 6) {
            return;
        }
        dp *= (f32)itemDataBiosGetBuff(item_data);
    }
    if (filter_id == 5) {
        dp = -1.0f * pokemonGetDp(ptr);
    }
    if (filter_id == 0) {
        rate_data_id = pokemonSeikakuDataBiosGetReliveFightout(seikaku_data);
    } else if (filter_id == 1) {
        rate_data_id = pokemonSeikakuDataBiosGetReliveWalk(seikaku_data);
    } else if (filter_id == 2) {
        rate_data_id = pokemonSeikakuDataBiosGetReliveCall(seikaku_data);
    } else if (filter_id == 3) {
        rate_data_id = pokemonSeikakuDataBiosGetReliveSodateya(seikaku_data);
    } else if (filter_id == 4) {
        rate_data_id = pokemonSeikakuDataBiosGetReliveNadenade(seikaku_data);
    }
    rate_data = pokemonSeikakuRateDataBiosGetPtr((u8)rate_data_id);
    if (rate_data == NULL) {
        return;
    }
    kake = pokemonSeikakuRateDataBiosGetKake(rate_data);
    waru = pokemonSeikakuRateDataBiosGetWaru(rate_data);
    if (waru != 0) {
        dp *= (f32)kake;
        dp /= (f32)waru;
    } else {
        return;
    }
    if (ptr != NULL) {
        new_dp = pokemonGetDp(ptr);
        new_dp += dp;
        if (new_dp < 0.0f) {
            new_dp = 0.0f;
        }
        pokemonSetDp(ptr, new_dp);
    }
}

s32 pokemonGetDarkPokemonLevel(u8* pokemon)
{
    u16 divisor;
    f32 level;

    if (pokemon == NULL) {
        return 7;
    }
    if ((u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_TRAINER_POKEMON_ID, 0) == 1) {
        divisor = (u16)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_HEART_GAUGE_INITIAL, 0);
        if (divisor == 0) {
            divisor = 1;
        }
        level = pokemonGetDp(pokemon);
        if (level < 0.0f) {
            level = 0.0f;
        } else {
            level = (100.0f * level) / (f32)divisor;
        }
        if (level >= 100.0f) {
            return 0;
        }
        if (level >= 80.0f) {
            return 1;
        }
        if (level >= 60.0f) {
            return 2;
        }
        if (level >= 40.0f) {
            return 3;
        }
        if (level >= 20.0f) {
            return 4;
        }
        if (level > 0.0f) {
            return 5;
        }
        return 6;
    }
    return 7;
}
