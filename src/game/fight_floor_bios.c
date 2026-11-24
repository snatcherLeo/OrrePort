/**
 * @file fight_floor_bios.c
 * @brief Fight-floor low-level field accessors (BIOS layer).
 *
 * Split out of the former game/pokemon.c CodeCandidate bucket
 * (0x801F000C-0x801F7F80), which was mislabeled "pokemon" but is
 * entirely the XD-era fight-engine cluster. Address range covered by
 * this translation unit: 0x801F640C-0x801F6B54 (72 functions), per
 * config/GC6E01/splits.txt.
 */

#include "game/pokemon_fight_types.h"
#include "pc/disc_types.h" /* DISC_U16 / DISC_STORE16: common_rel tables are big-endian on the PC */

/* Address: 0x801F640C | Size: 0x8 | Pattern: sda_setter */
void fightFloorBiosSetEncountFloorId(u32 val) {
    encount_floor_id = val;
}

/* Address: 0x801F6414 | Size: 0x8 | Pattern: sda_getter */
u32 fightFloorBiosGetEncountFloorId(void) {
    return encount_floor_id;
}

/* Address: 0x801F641C | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetItemPokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA490) = val;
}

/* Address: 0x801F6430 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetItemPokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA490);
}

/* Address: 0x801F644C | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetTokuseiPokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA48C) = val;
}

/* Address: 0x801F6460 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetTokuseiPokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA48C);
}

/* Address: 0x801F647C | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetTuikakoukaPokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA488) = val;
}

/* Address: 0x801F6490 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetTuikakoukaPokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA488);
}

/* Address: 0x801F64AC | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetKizetuPokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA484) = val;
}

/* Address: 0x801F64C0 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetKizetuPokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA484);
}

/* Address: 0x801F64DC | Size: 0x14 | Pattern: nullcheck_addis_setter */
#ifdef TARGET_PC
/* PC test log for the battle_compare.py (golden slice; ORRE_BATTLE_LOG=1, off by default): at each new turn random,
 * "[battle] turn <n> rng=<hex> random=<u16>", then "[battle] mon <species> hp=<hp>/<max>" for each Pokémon on the
 * field. Observes only: no RNG draw, no state change. */
#include <stdlib.h>
#include "game/constants/pokemon_status.h"
static u32 pc_battle_log_mon(void* work, u32 slot, void* arg)
{
    /* PokemonBios (pokemon_data.h): species u16 +0x00, hp u16 +0x8A, maxHp u16 +0x8C (no pointers before them, so the
     * PC offsets are the GameCube's) */
    u8* p = (u8*)fightOutPokemonGetPokemonPtr(work);
    extern void OSReport(const char*, ...);
    (void)slot;
    (void)arg;
    if (p != NULL) {
        /* the battle types (2026-10-19: the type bug; the getter the type check uses) and the record's address */
        extern u32 fightOutPokemonBiosGetZokuseiDataId(u8* ptr, u8 idx);
        /* and its ability and held item in battle (getters, read only; the event log, 2026-11-10: abilities that act
         * without a line of text, and held items, are known for every Pokémon on the field) */
        extern u16 fightOutPokemonGetTokuseiDataId(void* context);
        extern u32 fightOutPokemonGetSoubiItemDataId(void* ctx);
        OSReport("[battle] mon %u hp=%u/%u types %u/%u ability %u item %u at %p\n", (unsigned)*(u16*)p,
                 (unsigned)*(u16*)(p + 0x8A), (unsigned)*(u16*)(p + 0x8C),
                 (unsigned)fightOutPokemonBiosGetZokuseiDataId((u8*)work, 0),
                 (unsigned)fightOutPokemonBiosGetZokuseiDataId((u8*)work, 1),
                 (unsigned)fightOutPokemonGetTokuseiDataId(work), (unsigned)fightOutPokemonGetSoubiItemDataId(work),
                 work);
    }
    return 1;
}
/* the event log, 2026-11-10 (snatcherLeo: every Pokémon loaded into the battle, not only those sent out): at the battle's
 * first turn, one line per trainer with the whole team as the battle holds it: species, level, ability (the species'
 * ability in the record's own slot), held item and the four moves. The same walk as
 * _fightFloorGetFightTrainerFightPokemonPtrArySub (fight_floor.c), read only. */
static u32 s_team_order;
static u32 pc_battle_log_team(u32 p, u32 slot, u32 d)
{
    extern void OSReport(const char*, ...);
    extern u8 fightTrainerCheckValid(void*);
    void* trainer = (void*)PC_PTR(p);
    u16 count = (u16)fightFloorGetStatus(0, 0, 0x17, 0);
    u16 i;
    (void)d;
    if (trainer == NULL || fightTrainerCheckValid(trainer) == 0) {
        return 1;
    }
    /* the trainers in the game's own walk order: the player's first (verified on the reference playthrough P02 / P03), then the opponents
     * (the callback's `slot` is not an index: it read 1 for both sides) */
    OSReport("[battle] team %u:", (unsigned)s_team_order++);
    for (i = 0; i < count; i++) {
        void* fp = fightTrainerGetValidFightPokemonPtr(trainer, i);
        u8* mon = fp != NULL ? (u8*)fightPokemonGetPokemonPtr(fp) : NULL;
        u16 species;
        if (mon == NULL) {
            continue;
        }
        species = *(u16*)mon;
        OSReport(" | %u lv %u ability %u item %u moves %u %u %u %u", (unsigned)species,
                 (unsigned)pokemonGetStatus(mon, 0, POKEMON_STATUS_LEVEL, 0),
                 (unsigned)pokemonGetStatus(NULL, species, POKEMON_STATUS_SPECIES_ABILITY,
                                            pokemonGetStatus(mon, 0, POKEMON_STATUS_ABILITY_SLOT, 0)),
                 (unsigned)pokemonGetStatus(mon, 0, POKEMON_STATUS_HELD_ITEM, 0),
                 (unsigned)pokemonGetStatus(mon, 0, POKEMON_STATUS_MOVE, 0),
                 (unsigned)pokemonGetStatus(mon, 0, POKEMON_STATUS_MOVE, 1),
                 (unsigned)pokemonGetStatus(mon, 0, POKEMON_STATUS_MOVE, 2),
                 (unsigned)pokemonGetStatus(mon, 0, POKEMON_STATUS_MOVE, 3));
    }
    OSReport("\n");
    return 1;
}
static void pc_battle_log_turn(u16 random)
{
    static int enabled = -1;
    extern u32 lbl_80478C94; /* HSD_RandSeedPtr: a pointer to the RNG state (rng.md) */
    extern void OSReport(const char*, ...);
    if (enabled < 0) {
        extern int pc_battle_log_on(void); /* pc_event_log.c: on with the event log, or ORRE_BATTLE_LOG */
        enabled = pc_battle_log_on();
    }
    if (!enabled) {
        return;
    }
    OSReport("[battle] turn %u rng=%08X random=%u\n", (unsigned)(u8)fightFloorGetStatus(0, 0, 0x0C, 0),
             lbl_80478C94 ? (unsigned)*(u32*)PC_PTR(lbl_80478C94) : 0u, (unsigned)random);
    if ((u8)fightFloorGetStatus(0, 0, 0x0C, 0) == 1) { /* turn 0 comes before the teams are set up (P03: no trainers yet) */
        s_team_order = 0;
        fightFloorLoopValidFightTrainer(0, (void (*)(u32, u32, u32))pc_battle_log_team, 0, 0);
    }
    fightFloorLoopValidFightOutPokemon(0, pc_battle_log_mon, 0, 0);
    {
        /* the floor's slot array too (status 0x59, eight cells, 2026-10-26, P12's battle 207: the console's
         * held Gengar first while its side-trainer-index walk held Noctowl first) */
        u32 i;
        OSReport("[battle] slots:");
        for (i = 0; i < 8; i++) {
            void* w = (void*)PC_PTR(fightFloorGetStatus(0, 0, 0x59, i));
            u8* p = w != NULL ? (u8*)fightOutPokemonGetPokemonPtr(w) : NULL;
            OSReport(" %u", p != NULL ? (unsigned)*(u16*)p : 0u);
        }
        OSReport("\n");
    }
}
#endif
void fightFloorBiosSetFirstAttackRnd(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)((u8*)ptr + 0xA4E4) = val;
#ifdef TARGET_PC
    pc_battle_log_turn(val);
#endif
}

/* Address: 0x801F64F0 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetFirstAttackRnd(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)((u8*)ptr + 0xA4E4);
}

/* 0x801F650C | size: 0x38 | small */
void fightFloorBiosSetFightOutPokemonPtrAryPtr(u32 *param_1, u32 *param_2) {
    u32 i;

    if (param_1 == NULL) return;
    for (i = 0; (i & 0xFFFF) < 8; i = i + 1) {
        *(u32*)((u8*)param_1 + ((u16)i << 2) + 0xA4C4) = *(u32*)((u8*)param_2 + ((u16)i << 2));
    }
}

/* 0x801F6544 | size: 0x1C */
u8* fightFloorBiosGetFightOutPokemonPtrAryPtr(u8* ptr) {
    if (ptr == NULL) { return NULL; }
    return ptr + 0xA4C4;
}

/* 0x801F6560 | size: 0x28 */
void fightFloorBiosSetFightOutPokemonPtrAry(u8* ptr, u16 idx, u32 val) {
    u32* base;
    if (ptr == NULL) { return; }
    if (idx >= 8) { return; }
    base = (u32*)(ptr + 0xA4C4);
    base[idx] = val;
}

/* 0x801F6588 | size: 0x38 */
u32 fightFloorBiosGetFightOutPokemonPtrAry(u8* ptr, u16 idx) {
    u32* base;
    if (ptr == NULL) { return 0; }
    if (idx >= 8) { return 0; }
    base = (u32*)(ptr + 0xA4C4);
    return base[idx];
}

/* Address: 0x801F65C0 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetFightPokemonEntryCnt(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)((u8*)ptr + 0xA4C0) = val;
}

/* Address: 0x801F65D4 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetFightPokemonEntryCnt(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(s16*)((u8*)ptr + 0xA4C0);
}

/* Address: 0x801F65F0 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorDataBiosSetName(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    DISC_STORE32(u32, &ptr[0x4], val);
}

/* Address: 0x801F6600 | Size: 0x18 | Pattern: nullcheck_getter */
u32 fightFloorDataBiosGetName(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return DISC_U32(&ptr[0x4]);
}

/* Address: 0x801F6618 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorDataBiosSetEnvSndId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    DISC_STORE32(u32, &ptr[0x10], val);
}

/* Address: 0x801F6628 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorDataBiosSetBgmSndId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    DISC_STORE32(u32, &ptr[0xC], val);
}

/* Address: 0x801F6638 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorDataBiosSetSyoukaiWzxDataId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    DISC_STORE32(u32, &ptr[0x8], val);
}

/* Address: 0x801F6648 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorDataBiosSetTikeiDataId(u8* ptr, u8 val) {
    if (ptr == NULL) { return; }
    *(u8*)(&ptr[0x0]) = val;
}

/* 0x801F6658 | size: 0x24 */
void fightFloorDataBiosSetFightSideDataId(u8* ptr, u8 idx, u16 val) {
    if (ptr == NULL) { return; }
    if (idx >= 2) { return; }
    ptr += idx * 2;
    DISC_STORE16(u16, ptr + 0x14, val);
}

/* Address: 0x801F667C | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorDataBiosSetFloorDataId(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    DISC_STORE16(u16, &ptr[0x2], val);
}

/* Address: 0x801F668C | Size: 0x18 | Pattern: nullcheck_getter */
u32 fightFloorDataBiosGetEnvSndId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return DISC_U32(&ptr[0x10]);
}

/* Address: 0x801F66A4 | Size: 0x18 | Pattern: nullcheck_getter */
u32 fightFloorDataBiosGetBgmSndId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return DISC_U32(&ptr[0xC]);
}

/* Address: 0x801F66BC | Size: 0x18 | Pattern: nullcheck_getter */
u32 fightFloorDataBiosGetSyoukaiWzxDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return DISC_U32(&ptr[0x8]);
}

/* Address: 0x801F66D4 | Size: 0x18 | Pattern: nullcheck_getter */
u8 fightFloorDataBiosGetTikeiDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u8*)(&ptr[0x0]);
}

/* 0x801F66EC | size: 0x34 */
u32 fightFloorDataBiosGetFightSideDataId(u8* ptr, u8 idx) {
    if (ptr == NULL) { return 0; }
    if (idx >= 2) { return 0; }
    ptr += idx * 2;
    return DISC_U16(ptr + 0x14);
}

/* Address: 0x801F6720 | Size: 0x18 | Pattern: nullcheck_getter */
u16 fightFloorDataBiosGetFloorDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return DISC_U16(&ptr[0x2]);
}

/* Address: 0x801F6738 | Size: 0x2C */
FightFloorData* fightFloorDataBiosGetPtr(u16 idx) {
    if (idx >= *fight_floor_data_number) { return NULL; }
    return &fight_floor_data[idx];
}

/* Address: 0x801F6764 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointTokuseiDataId(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)((u8*)ptr + 0xA4BE) = val;
}

/* Address: 0x801F6778 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointItemDataId(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)((u8*)ptr + 0xA4BC) = val;
}

/* Address: 0x801F678C | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointWazaDataId(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)((u8*)ptr + 0xA4BA) = val;
}

/* Address: 0x801F67A0 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointPokemonDataId(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)((u8*)ptr + 0xA4B8) = val;
}

/* Address: 0x801F67B4 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetWazakoukaMsgId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA4B0) = val;
}

/* Address: 0x801F67C8 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetCriticalMsgId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA4AC) = val;
}

/* Address: 0x801F67DC | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAttackMsgId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA4A8) = val;
}

/* Address: 0x801F67F0 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointMsgId(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA4B4) = val;
}

/* Address: 0x801F6804 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointItemPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA4A4) = val;
}

/* Address: 0x801F6818 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointSidePtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA4A0) = val;
}

/* Address: 0x801F682C | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointWazaPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA49C) = val;
}

/* Address: 0x801F6840 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointTrainerPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA498) = val;
}

/* Address: 0x801F6854 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAppointPokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA494) = val;
}

/* Address: 0x801F6868 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetIrekaePokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA480) = val;
}

/* Address: 0x801F687C | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetEscapePokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA47C) = val;
}

/* Address: 0x801F6890 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetDefensePokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA478) = val;
}

/* Address: 0x801F68A4 | Size: 0x14 | Pattern: nullcheck_addis_setter */
void fightFloorBiosSetAttackPokemonPtr(u8* ptr, u32 val) {
    if (ptr == NULL) { return; }
    *(u32*)((u8*)ptr + 0xA474) = val;
}

/* Address: 0x801F68B8 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorBiosSetEncountDataId(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)(&ptr[0x12]) = val;
}

/* Address: 0x801F68C8 | Size: 0x10 | Pattern: nullcheck_setter */
void fightFloorBiosSetTurnCount(u8* ptr, u16 val) {
    if (ptr == NULL) { return; }
    *(u16*)(&ptr[0x10]) = val;
}

/* Address: 0x801F68D8 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointTokuseiDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)((u8*)ptr + 0xA4BE);
}

/* Address: 0x801F68F4 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointItemDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)((u8*)ptr + 0xA4BC);
}

/* Address: 0x801F6910 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointWazaDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)((u8*)ptr + 0xA4BA);
}

/* Address: 0x801F692C | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointPokemonDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)((u8*)ptr + 0xA4B8);
}

/* Address: 0x801F6948 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetWazakoukaMsgId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA4B0);
}

/* Address: 0x801F6964 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetCriticalMsgId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA4AC);
}

/* Address: 0x801F6980 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAttackMsgId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA4A8);
}

/* Address: 0x801F699C | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointMsgId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA4B4);
}

/* Address: 0x801F69B8 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointItemPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA4A4);
}

/* Address: 0x801F69D4 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointSidePtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA4A0);
}

/* Address: 0x801F69F0 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointWazaPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA49C);
}

/* Address: 0x801F6A0C | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointTrainerPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA498);
}

/* Address: 0x801F6A28 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAppointPokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA494);
}

/* Address: 0x801F6A44 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetIrekaePokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA480);
}

/* Address: 0x801F6A60 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetEscapePokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA47C);
}

/* Address: 0x801F6A7C | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetDefensePokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA478);
}

/* Address: 0x801F6A98 | Size: 0x1C | Pattern: nullcheck_addis_getter_bne */
u32 fightFloorBiosGetAttackPokemonPtr(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u32*)((u8*)ptr + 0xA474);
}

/* 0x801F6AB4 | size: 0x34 */
u8* fightFloorBiosGetFightSidePtr(u8* ptr, u16 idx) {
    if (ptr == NULL) { return NULL; }
    if (idx >= 2) { return NULL; }
    return ptr + idx * 0x5230 + 0x14;
}

/* Address: 0x801F6AE8 | Size: 0x18 | Pattern: nullcheck_getter */
u16 fightFloorBiosGetEncountDataId(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)(&ptr[0x12]);
}

/* Address: 0x801F6B00 | Size: 0x18 | Pattern: nullcheck_getter */
u16 fightFloorBiosGetTurnCount(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u16*)(&ptr[0x10]);
}

/* 0x801F6B18 | size: 0x30 */
u8* fightFloorBiosGetJoutaiPtr(u8* ptr, u16 idx) {
    if (ptr == NULL) { return NULL; }
    if (idx >= 1) { return NULL; }
    return ptr + idx * 0x10;
}

/* Address: 0x801F6B48 | Size: 0xC */
u8* fightFloorBiosGetFightFloorPtr(void) {
    return lbl_8046DD90;
}
