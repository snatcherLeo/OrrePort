/**
 * @file pda_exact_8003D8CC.c
 * @brief PDA memo list filter by the active kana-row tab, 0x8003D8CC - 0x8003DC54.
 *        Standalone copy of fn_8003D8CC (and the helpers it inlines) from
 *        pda_range_80037158.c, linked on its own.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the kana-group tables stay big-endian on the PC */

typedef struct PdaNameEntry {
    u8 used;
    u8 pad01;
    u16 text[8];
} PdaNameEntry;

typedef struct PdaKanaGroup {
    u32 words[15];
    s32 count;
} PdaKanaGroup;

typedef struct PdaSceneWork {
    s32 currentIndex;
    u8 pad04[0xC];
    s32 field_10;
    u8 pad14[0x14];
    s32 field_28;
    u8 pad2C[0x14];
    f32 angle;
    u8 pad44[8];
    f32 alphaScale;
    u8 pad50[0x1F8];
} PdaSceneWork; /* lbl_803A6818, size 0x248 */

extern PdaSceneWork lbl_803A6818;
extern u32 lbl_8047A4E0;
extern u16* lbl_8047A4E4;
extern u16 lbl_8047A4E8;
extern PdaKanaGroup* lbl_802E60B0[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gamedataGetStatus(s32 a, s32 b);
extern void pokemonCreate(u32 work, u16 species, s32 level, u32 trainer);
extern u32 memoDataGetPokemonRndFromID(s32 a, u32 id);
extern u32 memoDataGetPokemonTrainerRndFromID(s32 a, u32 id);
extern void pokemonBiosSetRnd(u32 work, u32 rnd);
extern void pokemonBiosSetCatchTrainerRnd(u32 work, u32 rnd);
extern u16 pokemonBiosGetPokemonDataId(u32 work);
extern void* pokemonDataBiosGetPtr(u16 id);
extern void* pokemonDataBiosGetName(void* data);
extern void GScharMakeFromSJIS(u16* dst, u32 sjis);
extern s32 GSmsgGetText(u8* dst, s32 size, u8* src);
#endif

/* As pdaLoadPokemon, but from an entry ID the caller already has in hand. */
static inline u32 pdaLoadPokemonId(u32 id)
{
    u32 work;
    u32 rnd;
    u32 species = id;
    u32 trainerRnd;

    if ((work = lbl_8047A4E0) != 0) {
        if (id >= 0x8000) {
            species = id & 0x3fff;
        }
        pokemonCreate(PC_PTR(work), (u16)species, 10, PC_PTR(gamedataGetStatus(0, 1)));
        rnd = memoDataGetPokemonRndFromID(0, species);
        trainerRnd = memoDataGetPokemonTrainerRndFromID(0, species);
        pokemonBiosSetRnd(PC_PTR(work), rnd);
        pokemonBiosSetCatchTrainerRnd(PC_PTR(work), trainerRnd);
        return lbl_8047A4E0;
    }
    return 0;
}

/* pdaLoadPokemonId for the lookup helpers, which go on to read the work slot. */
static inline u32 pdaLoadPokemonWork(u32 id)
{
    u32 rnd;
    u32 species = id;
    u32 trainerRnd;
    u32 work;

    if ((work = lbl_8047A4E0) != 0) {
        if (id >= 0x8000) {
            species = id & 0x3fff;
        }
        pokemonCreate(PC_PTR(work), (u16)species, 10, PC_PTR(gamedataGetStatus(0, 1)));
        rnd = memoDataGetPokemonRndFromID(0, species);
        trainerRnd = memoDataGetPokemonTrainerRndFromID(0, species);
        pokemonBiosSetRnd(PC_PTR(work), rnd);
        pokemonBiosSetCatchTrainerRnd(PC_PTR(work), trainerRnd);
        return lbl_8047A4E0;
    }
    return 0;
}

/* Name of the memo entry as a wide string, or NULL when unavailable. */
static inline void* pdaGetPokemonName(u16 id)
{
    u32 work = pdaLoadPokemonWork(id);

    if (work != 0) {
        return PC_PTR(pokemonDataBiosGetName(
            pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(PC_PTR(work)))));
    }
    return NULL;
}

/* Mark the row's entries used and convert their first characters. */
/* RULE-EXCEPTION(user-approved): single-use inline helper pda_8003D8CC_Fill (fn_8003D8CC) - see docs/RULE_EXCEPTIONS.md */
static inline void pda_8003D8CC_Fill(PdaNameEntry* tbl, PdaKanaGroup* group)
{
    s32 n;

#ifdef TARGET_PC
    /* 2026-10-25 (the unsure-table audit): the group records (lbl_802E5570 and the four after it) stay
     * big-endian on the PC, their words big-endian pointer slots; read natively, count came out huge and the loop
     * wrote past tbl[15] */
    for (n = 0; n < DISC_S32(&group->count); n++) {
        tbl[n].used = 1;
        GScharMakeFromSJIS(tbl[n].text, PC_PTR(DISC_U32(&group->words[n])));
    }
#else
    for (n = 0; n < group->count; n++) {
        tbl[n].used = 1;
        GScharMakeFromSJIS(tbl[n].text, PC_PTR(group->words[n]));
    }
#endif
}

/* Clear the kana-row table and fill it from tab's row. */
/* RULE-EXCEPTION(user-approved): single-use inline helper pda_8003D8CC_KanaSetup (fn_8003D8CC) - see docs/RULE_EXCEPTIONS.md */
static inline PdaKanaGroup* pda_8003D8CC_KanaSetup(PdaNameEntry* tbl, s32 tab)
{
    PdaKanaGroup* group;
    s32 n;

    for (n = 0; n < 15; n++) {
        tbl[n].used = 0;
    }
    if (tab != 0) {
#ifdef TARGET_PC
        /* lbl_802E60B0's slots are big-endian 32-bit pointers filled at startup, not native 8-byte ones */
        group = &DISC_PTR_AT(PdaKanaGroup, lbl_802E60B0)[tab - 1];
#else
        group = &lbl_802E60B0[0][tab - 1];
#endif
        pda_8003D8CC_Fill(tbl, group);
    }
    return group;
}

/* Filter the memo index table down to the active kana-row tab. */
void fn_8003D8CC(void)
{
    u16 nameBuf[0x80];
    PdaNameEntry tbl[15];
    u16 out[500];
    PdaKanaGroup* group;
    u16 j;
    u16 total;
    u16 i;
    u16 id;
    u16 count;

    if (*(s8*)((u8*)&lbl_803A6818 + 0x158) == 0) {
        return;
    }
    group = pda_8003D8CC_KanaSetup(tbl, *(s8*)((u8*)&lbl_803A6818 + 0x158));
#ifdef TARGET_PC
    count = DISC_S32(&group->count);
#else
    count = group->count;
#endif
    total = 0;
    for (i = 0; i < lbl_8047A4E8; i++) {
        pdaLoadPokemonId(id = lbl_8047A4E4[i]);
        if (PC_PTR32(GSmsgGetText((u8*)nameBuf, 0x100, PC_PTR32_IMPLICIT((u8*)pdaGetPokemonName(id)))) !=
            0) {
            for (j = 0; j < count; j++) {
                if (nameBuf[0] == tbl[j].text[0]) {
                    out[total++] = id;
                    break;
                }
            }
        }
    }
    for (i = 0; i < total; i++) {
        lbl_8047A4E4[i] = out[i];
    }
    lbl_8047A4E8 = total;
}
