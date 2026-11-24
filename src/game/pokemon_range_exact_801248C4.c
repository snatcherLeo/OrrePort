/**
 * @file pokemon_range_exact_801248C4.c
 * @brief Exact pure-C Pokemon initialization island, 0x801248C4 - 0x801254B4.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus(u8* ptr, u32 a, u32 b, u32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void pokemonResetBasisStatus(void* ptr);
#endif

u16 pokemonGetTokuseiDataId(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u8* a, u32 b, u32 c, u32 d);
#endif
    u32 val;
    if (ptr == NULL) { return 0; }
    val = pokemonGetStatus(ptr, 0, POKEMON_STATUS_SPECIES, 0) & 0xFFFF;
    if ((s32)pokemonGetStatus(NULL, val, POKEMON_STATUS_SPECIES_ABILITY, 1) == 0) {
        return (u16)pokemonGetStatus(NULL, val, POKEMON_STATUS_SPECIES_ABILITY, 0);
    }
    return (u16)pokemonGetStatus(NULL, val, POKEMON_STATUS_SPECIES_ABILITY, (u8)pokemonGetStatus(ptr, 0, POKEMON_STATUS_ABILITY_SLOT, 0));
}

void pokemonSetTokuseiFlag(u8* ptr, u32 arg2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(u8* ptr, u32 a, u32 b, u32 c, u8 d);
#endif
    u16 val1;
    if (ptr == NULL) { return; }
    val1 = (u16)pokemonGetStatus(ptr, 0, POKEMON_STATUS_SPECIES, 0);
    if ((s32)pokemonGetStatus(0, val1, POKEMON_STATUS_SPECIES_ABILITY, 1) == 0) { arg2 = 0; }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_ABILITY_SLOT, 0, (u8)arg2);
}

void pokemonInitAry(u8* ptr, u16 count) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInit(u8* ptr);
#endif
    u16 i;
    if (ptr == NULL) { return; }
    for (i = 0; i < count; i++) {
        pokemonInit(ptr + (u16)i * 0x138);
    }
}

void pokemonInit(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(u8* ptr, u32 a, u32 b, u32 c, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u8* ptr, u32 a, u32 b, u32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry(u32 base, u16 count);
#endif
    extern void gamedataAttestInit(u32 val);
    extern void pokemonBiosSetEventGetFlag(u8* ptr, u8 val);
    u32 i;
    u16 local;
    local = 0;
    if (ptr == NULL) { return; }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SHADOW_CONDITION_SLOT, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SHADOW_ID, 0, 0);
    if (ptr != NULL) { pokemonSetStatus(ptr, 0, POKEMON_STATUS_HEART_GAUGE, 0, (u32)-0x64); }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_STORED_EXP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_STORED_FRIENDSHIP, 0, 0);
    joutaiInitAry(PC_PTR(pokemonGetStatus(ptr, 0, POKEMON_STATUS_IS_SHADOW, 0)), 1);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SPECIES, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_PERSONALITY, 0, 0);
    gamedataAttestInit(pokemonGetStatus(ptr, 0, POKEMON_STATUS_ATTEST, 0));
    if (ptr != NULL) {
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_MET_LOCATION, 0, 0);
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_MET_LEVEL, 0, 0);
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_BALL, 0, 0);
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_OT_GENDER, 0, 2);
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_OT_ID, 0, 0);
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_OT_NAME, 0, PC_PTR32(&local));
    }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_NICKNAME, 0, PC_PTR32(&local));
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EXP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_LEVEL, 0, 0);
    joutaiInitAry(PC_PTR(pokemonGetStatus(ptr, 0, POKEMON_STATUS_CONDITION_SLOT, 0)), 1);
    pokemonSetStatus(ptr, 0, 0x7d, 0, 0);
    if (ptr != NULL) {
        for (i = 0; (u16)i < 4; i++) {
            if (ptr != NULL) {
                pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE, (u32)i, 0);
                pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP, (u32)i, 0);
                pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP_UPS, (u32)i, 0);
            }
        }
    }
    if (ptr != NULL) {
        pokemonGetStatus(ptr, 0, POKEMON_STATUS_HELD_ITEM, 0);
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_HELD_ITEM, 0, 0);
    }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_HP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MAX_HP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_ATTACK, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_DEFENSE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SP_ATTACK, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SP_DEFENSE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SPEED, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EV_HP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EV_ATTACK, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EV_DEFENSE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EV_SP_ATTACK, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EV_SP_DEFENSE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EV_SPEED, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IV_HP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IV_ATTACK, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IV_DEFENSE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IV_SP_ATTACK, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IV_SP_DEFENSE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IV_SPEED, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_FRIENDSHIP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CONTEST_COOL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CONTEST_BEAUTY, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CONTEST_CUTE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CONTEST_SMART, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CONTEST_TOUGH, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CONTEST_SHEEN, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_CHAMPION, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_WINNING, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_VICTORY, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_ARTIST, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_EFFORT, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_MARINE, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_LAND, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_SKY, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_COUNTRY, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_NATIONAL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_EARTH, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_RIBBON_WORLD, 0, 0);
    pokemonSetStatus(ptr, 0, 0xaf, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_COOL_MEDAL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_BEAUTY_MEDAL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_CUTE_MEDAL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SMART_MEDAL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_TOUGH_MEDAL, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_POKERUS, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_IS_EGG, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_ABILITY_SLOT, 0, 0);
    pokemonSetStatus(ptr, 0, 0xb8, 0, 0);
    pokemonSetStatus(ptr, 0, 0xb9, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_BOX_MARKINGS, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MAIL_ID, 0, (u32)0xff);
    pokemonSetStatus(ptr, 0, 0xbd, 0, 0);
    pokemonSetStatus(ptr, 0, 0xbe, 0, 0);
    pokemonBiosSetEventGetFlag(ptr, 0);
}

void pokemonInitDarkPokemon(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(u8* a, u32 b, u32 c, u32 d, u32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u8* a, u32 b, u32 c, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry(u8* a, u32 b);
#endif
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_SHADOW_ID, 0, 0);
    if (ptr != NULL) {
        pokemonSetStatus(ptr, 0, POKEMON_STATUS_HEART_GAUGE, 0, (u32)-100);
    }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_STORED_EXP, 0, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_STORED_FRIENDSHIP, 0, 0);
    joutaiInitAry((u8*)PC_PTR(pokemonGetStatus(ptr, 0, POKEMON_STATUS_IS_SHADOW, 0)), 1);
}

void pokemonInitJoutai(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u8* a, u32 b, u32 c, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void joutaiInitAry(u8* a, u32 b);
#endif
    joutaiInitAry((u8*)PC_PTR(pokemonGetStatus(ptr, 0, POKEMON_STATUS_CONDITION_SLOT, 0)), 1);
}

void pokemonWazaInit(u8* ptr, u32 arg2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus(u8* a, u32 b, u32 c, u32 d, u32 e);
#endif
    if (ptr == NULL) { return; }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE, arg2, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP, arg2, 0);
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_MOVE_PP_UPS, arg2, 0);
}

/* Rare (shiny) check: the XOR of the 16-bit halves of status words 0x75
 * and 0x6F is below 8. */
u32 pokemonCheckRare(void* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(void* a, u32 b, u32 c, u32 d);
#endif
    u32 id75;
    u32 id6F;
    if (ctx == NULL) {
        return 0;
    }
    id75 = pokemonGetStatus(ctx, 0, POKEMON_STATUS_OT_ID, 0);
    id6F = pokemonGetStatus(ctx, 0, POKEMON_STATUS_PERSONALITY, 0);
    return ((id75 >> 16) ^ (id75 & 0xFFFF) ^ (id6F >> 16) ^ (id6F & 0xFFFF)) < 8;
}

void pokemonGrowBasisStatus(void* ptr, u32 arg2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetStatus();
#endif
    if (ptr == NULL) { return; }
    pokemonSetStatus(ptr, 0, POKEMON_STATUS_EXP, 0, arg2);
    pokemonResetBasisStatus(ptr);
}

void pokemonResetBasisStatus(void* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
    extern void pokemonSetLevelBasisStatus(u8* ptr, u32 level);
    u8 val;
    if (ptr == NULL) { return; }
    val = pokemonGetStatus(ptr, 0, POKEMON_STATUS_LEVEL_FROM_EXP, 0);
    pokemonSetLevelBasisStatus(ptr, val);
}
