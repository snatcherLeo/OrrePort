/**
 * @file menuPokemon.c
 * @brief menuPokemon -- Party Pokemon menu (PC box / party list) UI.
 *
 * Address range: 0x800181C4 - 0x8001D7E4 (42 functions)
 *
 * Split from game/gs_pcbox.c (originally mislabeled -- this range is
 * actually the party Pokemon menu module, not the PC box module).
 * Corresponds to XD game/menuPokemon.cpp.
 *
 * NOTE: fn_800181C4 (0x3D0 bytes) is declared extern and called from
 * bagGiveItemCheck but has no recovered C body anywhere in the tree yet; it
 * remains an unimplemented hole within this address range.
 */

#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC */
#include "game/constants/pokemon_status.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* items never run out (menuPokemonUseItem) */
#endif

/* One-function carves include this file with one of these defined. */
#if defined(MENU_POKEMON_8001D378_ONLY) || defined(MENU_POKEMON_80019B48_ONLY) || defined(MENU_POKEMON_8001BEBC_ONLY) || \
    defined(MENU_POKEMON_8001C064_ONLY) || defined(MENU_POKEMON_80019D5C_ONLY) || \
    defined(MENU_POKEMON_80019F6C_ONLY) || \
    defined(MENU_POKEMON_ISLAND_80018F30_ONLY) || \
    defined(MENU_POKEMON_ISLAND_80019938_ONLY) || defined(MENU_POKEMON_ISLAND_800195E0_ONLY) || \
    defined(MENU_POKEMON_ISLAND_8001BAC4_ONLY) || defined(MENU_POKEMON_ISLAND_80018594_ONLY) || \
    defined(MENU_POKEMON_ISLAND_800194D0_ONLY) || defined(MENU_POKEMON_SUB_ONLY) || \
    defined(MENU_POKEMON_8001C7B8_ONLY) || defined(MENU_POKEMON_80018A68_ONLY) || \
    defined(MENU_POKEMON_800181C4_ONLY) || defined(MENU_POKEMON_800194E4_ONLY) || \
    defined(MENU_POKEMON_80019754_ONLY) || defined(MENU_POKEMON_800192A8_ONLY)
#define MENU_POKEMON_CARVE_ONLY
#endif

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_80019F6C_ONLY) || \
    defined(MENU_POKEMON_8001C7B8_ONLY) || defined(MENU_POKEMON_80018A68_ONLY)

/* =========================================================================
 * External declarations (shared)
 * ========================================================================= */

/* Pokemon data */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  heroItemGetItemKindToItemAryPtr(void* pokeData, u8 fieldId, u16* outCount,
                          s32 p4, s32 p5, s32 p6);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  heroHizukiItemGetItemAryPtr(void* pokeData, u16* outCount, s32 p3, s32 p4, s32 p5);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fn_801429E8(void* fieldData);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   itemBiosGetNum(void* fieldData);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   itemDataBiosGetPtr(u16 speciesId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   itemDataBiosGetPrice(void);
#endif

/* Text formatting */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_8002A0B8(void* outBuf, void* fmt, s32 p3, s32 p4,
                          u16 p5, s32 p6, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   heroGetStatus(void* partyData, s32 slot, s32 p3);
#endif

/* Dialog/rendering */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winMsgOpenWithSE(s32 p1, void* text, s32 p3, s32 p4, u8 p5);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winMsgClose(s32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winSeqSetMenu(void* ctx, s32 state);
#endif
extern void  menuDataBiosGetXY(s16 npcId, u16* outX, u16* outY);
extern void  menuDataBiosSetXY(s16 x, s16 y, s16 z);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuDataBiosGetPtr(void* data);
#endif


#endif /* !MENU_POKEMON_CARVE_ONLY || MENU_POKEMON_80019F6C_ONLY */

typedef struct MenuPokemonStatus {
    u16 species;
    u8 unk2[0x18];
    u16 unk1A;
    u8 unk1C[0x8];
    u16 iconId;
    u8 unk26[0xA];
} MenuPokemonStatus;

static inline void* menuPokemonGetHero(s32 mode, void* trainer) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* std082_flagGetValue(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataGetStatus(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fn_8006AEEC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetGcHeroFightTrainerPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetStatus(void*, s32, s32, s32);
#endif

    void* hero;

    switch (mode) {
    case 0:
        if (std082_flagGetValue(0x8AE) == 0) {
            hero = PC_PTR(savedataGetStatus(0, 2));
        } else {
            hero = fn_8006AEEC();
        }
        break;
    case 1:
        if (trainer == 0) {
            trainer = PC_PTR(fightFloorGetGcHeroFightTrainerPtr(0));
        }
        if (trainer == 0) {
            return 0;
        }
        hero = PC_PTR(fightTrainerGetStatus(trainer, 0, 0x44, 0));
        break;
    default:
        hero = 0;
        break;
    }
    return hero;
}

static inline void* menuPokemonGetPokemon(s32 mode, u16 index, void* trainer) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroBiosGetPokemonPtr(void* hero, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetGcHeroFightTrainerPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetValidFightPokemonPtr(void* trainer, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus(void*, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(void* pokemon);
#endif
    void* pokemon = 0;
    void* hero;
    void* fightPokemon;

    switch (mode) {
    case 0:
        if (index >= 6) {
            return 0;
        }
        hero = menuPokemonGetHero(mode, trainer);
        if (hero == 0) {
            return 0;
        }
        pokemon = heroBiosGetPokemonPtr(hero, index);
        break;
    case 1:
        if (index >= 6) {
            return 0;
        }
        if (trainer == 0) {
            trainer = PC_PTR(fightFloorGetGcHeroFightTrainerPtr(0));
        }
        if (trainer == 0) {
            return 0;
        }
        fightPokemon = fightTrainerGetValidFightPokemonPtr(trainer, index);
        if (fightPokemon == 0) {
            return 0;
        }
        pokemon = PC_PTR(pokemonGetStatus(fightPokemon, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        break;
    case 2:
        if (index >= 30) {
            return 0;
        }
        break;
    }
    if (pokemonCheckValid(pokemon) == 0) {
        pokemon = 0;
    }
    return pokemon;
}

/* RULE-EXCEPTION(user-approved): inline copy of the real menuPokemonGetStatusSprite — see docs/RULE_EXCEPTIONS.md */
static inline u16 menuPokemonGetStatusIcon(void* pokemon) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonGetStatus(void* pokemon, u32 a, u32 id, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonGetJoutaiMenuSpriteId(void* pokemon);
#endif
    extern u16 lbl_802E4EB8[8];
    u16 kind;

    if (pokemonGetStatus(pokemon, 0, POKEMON_STATUS_IS_EMPTY, 0) == 1) {
        kind = 1;
    } else {
        switch (pokemonGetJoutaiMenuSpriteId(pokemon)) {
        case 0x3A: kind = 2; break;
        case 0x3B: kind = 3; break;
        case 0x3C: kind = 4; break;
        case 0x3D: kind = 5; break;
        case 0x3E: kind = 6; break;
        default: kind = 0; break;
        }
    }
    return lbl_802E4EB8[kind];
}

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_8001C064_ONLY) || \
    defined(MENU_POKEMON_8001C7B8_ONLY)
/* 0x8001C064 | 0x754 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std082_flagGetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_8006AEEC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetGcHeroFightTrainerPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroBiosGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetValidFightPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonCheckValid();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonGetSoubiItemDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroItemCheckAddItemDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroItemAddItemDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonDoItemSoubi(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void heroItemDecItemDataId(void);
#endif
extern void pokemonToMenuPokemonStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetJoutaiMenuSpriteId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 n);
#endif
extern u8 lbl_803A1D40[];
extern u8 lbl_803A1C20[];
#ifndef TARGET_PC /* PC: one type per label in a file (GCC); the code below sees its own type through a cast */
extern u8 lbl_802E4EB8[];
#else
extern u16 lbl_802E4EB8[8];
#define lbl_802E4EB8 ((u8*)lbl_802E4EB8)
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s8 fn_8001E074(u8, s16, s16, u32);
#endif
#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_8001C064_ONLY)
#if 0
asm void fn_8001C064(void) {
#include "src/game/gs_pcbox_fn_8001C064.inc"
}
#else
#if defined(MENU_POKEMON_8001C064_ONLY)
/*
 * RULE-EXCEPTION(user-approved): the one-function carve keeps a static copy
 * of menuPokemonRefresh so its call is inlined as retail's is; the linked
 * menuPokemonRefresh stays in its own unit — see docs/RULE_EXCEPTIONS.md
 */
#define MENU_POKEMON_FILL_STATUS_LINKAGE static inline
#else
#define MENU_POKEMON_FILL_STATUS_LINKAGE
#endif
MENU_POKEMON_FILL_STATUS_LINKAGE void menuPokemonRefresh(void);

s32 fn_8001C064(s8 slot, u8 bagSlot, u16 itemId, u16* outItem) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonGetSoubiItemDataId(void* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 heroItemCheckAddItemDataId(void* hero, u16 item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroItemAddItemDataId(void* hero, u16 item, s32 count, s32 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 heroItemDecItemDataId(void* hero, u16 item, s32 count, u8 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonDoItemSoubi(void* pokemon, u16 item, s32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpen(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
    void* pokemon;
    void* hero;
    u16 removed;
    s8 answer;
    u16 held;
    /* RULE-EXCEPTION(user-approved): mode read into a named local to fix retail's r5/r4 order — see docs/RULE_EXCEPTIONS.md */
    s32 mode;

    mode = *(s32*)(lbl_803A1D40 + 0x8);
    pokemon = menuPokemonGetPokemon(mode, slot, PTR32_AT_T(void, lbl_803A1D40, 0xC));
    msgctrlSetValue(0x32, PC_PTR32_IMPLICIT(&((MenuPokemonStatus*)lbl_803A1C20)[(s8)lbl_803A1D40[6]]));
    held = pokemonGetSoubiItemDataId(PC_PTR32_IMPLICIT(pokemon));
    hero = menuPokemonGetHero(*(s32*)(lbl_803A1D40 + 0x8), PTR32_AT_T(void, lbl_803A1D40, 0xC));
    if (hero == 0) {
        return -1;
    }

    if (itemId == 0) {
        if (held != 0) {
            if (heroItemCheckAddItemDataId(hero, held) <= 0) {
                winMsgOpen(2, 0x2B6B, 1, 1);
            } else {
                heroItemAddItemDataId(hero, held, 1, -1);
                removed = pokemonDoItemSoubi(pokemon, 0, 0);
                if (outItem != 0) {
                    *outItem = removed;
                }
                msgctrlSetValue(0x2D, removed);
                winMsgOpen(2, 0x2B69, 1, 1);
            }
        } else {
            winMsgOpen(2, 0x2B6A, 1, 1);
        }
    } else if (held != 0) {
        msgctrlSetValue(0x2D, held);
        winMsgOpen(2, 0x2B66, 1, 0);
        answer = menuSubOpenYesNo(0, -1, -1, 0);
        winMsgClose(1);
        if (answer != 0) {
            return -1;
        }
#ifdef TARGET_PC
        /* items never run out (pc/orre_hooks.h): an item given to hold stays in the bag too (snatcherLeo, 2026-11-13: "Giving
         * items for pokemon to hold shouldn't allow them to be gone from bag"); 0 = "taken", as the game expects */
        if ((ORRE_ITEM_USE_KEEPS(itemId) ? 0 : heroItemDecItemDataId(hero, itemId, 1, bagSlot)) != 0) {
#else
        if (heroItemDecItemDataId(hero, itemId, 1, bagSlot) != 0) {
#endif
            winMsgOpen(2, 0x2B6B, 1, 1);
        } else if (heroItemCheckAddItemDataId(hero, held) <= 0) {
            heroItemAddItemDataId(hero, itemId, 1, lbl_803A1D40[0x11]);
            winMsgOpen(2, 0x2B6B, 1, 1);
        } else {
            heroItemAddItemDataId(hero, held, 1, -1);
            removed = pokemonDoItemSoubi(pokemon, 0, 0);
            if (outItem != 0) {
                *outItem = removed;
            }
            pokemonDoItemSoubi(pokemon, itemId, 1);
            msgctrlSetValue(0x2D, removed);
            msgctrlSetValue(0x2E, itemId);
            winMsgOpen(2, 0x2B67, 1, 0);
        }
#ifdef TARGET_PC
    } else if ((ORRE_ITEM_USE_KEEPS(itemId) ? 0 : heroItemDecItemDataId(hero, itemId, 1, bagSlot)) == 0) {
#else
    } else if (heroItemDecItemDataId(hero, itemId, 1, bagSlot) == 0) {
#endif
        pokemonDoItemSoubi(pokemon, itemId, 1);
        msgctrlSetValue(0x2D, itemId);
        winMsgOpen(2, 0x2B68, 1, 0);
    }
    winMsgClose(1);
    menuPokemonRefresh();
    return -1;
}
#endif
#endif /* !MENU_POKEMON_CARVE_ONLY || MENU_POKEMON_8001C064_ONLY */
#if defined(MENU_POKEMON_8001C064_ONLY)
/*
 * RULE-EXCEPTION(user-approved): the deferred-inline build generates
 * fn_8001C064 with the peephole state in force at the end of the file, which
 * in the full file is off; the carve sets it the same way — see
 * docs/RULE_EXCEPTIONS.md
 */
#pragma peephole off
#endif

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_8001C7B8_ONLY)
/* 0x8001C7B8 | 0xBC0 */
#if defined(MENU_POKEMON_8001C7B8_ONLY)
/*
 * RULE-EXCEPTION(user-approved): the one-function carve keeps a static copy
 * of menuPokemonRefresh so its call is inlined (the unit builds with -inline
 * auto,deferred) as retail's is; the linked menuPokemonRefresh stays in its own
 * unit — see docs/RULE_EXCEPTIONS.md
 */
static inline void menuPokemonRefresh(void);
#else
void menuPokemonRefresh(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpen();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeSet();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80097F08(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpenCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowSearchID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorItemID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqCheckMove(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonReplace(void);
#endif
extern u32 fn_80019064(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 fn_80018F54();
#endif
extern void fn_8001D718(f32 target);
extern f32 lbl_8047B7C0;
extern u8 lbl_802E4E58[];
static inline void menuPokemonSetSlotSeq(s8 slot, s32 right, s32 left) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winSeqSetMenu(s16 id, s32 seq);
#endif
    s16 x;
    s16 y;
    s16 id;

    id = *(s16*)(lbl_802E4E58 + ((s8*)lbl_803A1D40)[4] * 0x30 + slot * 8);
    menuDataBiosGetXY(id, (u16*)&x, (u16*)&y);
    winSeqSetMenu(id, (x > 0xFA) ? right : left);
}

static inline void menuPokemonWaitSlot(s8 slot) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 winSeqCheckMove(s16 id);
#endif
    s16 x;
    s16 y;
    s16 id;

    while (id = *(s16*)(lbl_802E4E58 + slot * 8 + ((s8*)lbl_803A1D40)[4] * 0x30),
           menuDataBiosGetXY(id, (u16*)&x, (u16*)&y), winSeqCheckMove(id) != 0) {
        _threadSwitch();
    }
}

/* RULE-EXCEPTION(user-approved): single-call inline helper (fn_8001C7B8) — see docs/RULE_EXCEPTIONS.md */
static inline s32 menuPokemonItemMenu(s8* work, s8 slot, s32* itemArg) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_8001C064(s8 slot, s32 pocket, u16 item, u16* out);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80018F54(s32 a, s32 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, s32 parent, void* args, s32, s32, s32, ...);
#endif
    s32 action;
    u16 item;
    u32 pocket;
    void* pokemon;
    u16 newItem;

    work[0x14] = 3;
    action = menuOpen(0x6C, 1);
    menuClose(0x6C);
    switch (action) {
    case 0:
        menuClose(0x63);
        item = fn_80018F54(2, 0, 0);
        pocket = fn_80019064();
        *itemArg = work[6];
        menuOpenCustom(0x63, 0, itemArg, 0, 0, 1, lbl_803A1C20);
        pokemon = windowSearchID(0x63);
        if (pokemon != 0) {
            ((u8*)pokemon)[0x98] = 1;
        }
        if ((u16)item == 0) {
            action = -1;
        } else {
            fn_8001C064(slot, pocket & 0xFF, item, &newItem);
            action = 0;
        }
        break;
    case 1:
        fn_8001C064(slot, -1, 0, &newItem);
        action = 0;
        break;
    case -1:
    case 2:
        action = -1;
        break;
    }
    if (action == -1) {
        return 0;
    }
    return -1;
}

#if 0
asm void fn_8001C7B8(void) {
#include "src/game/gs_pcbox_fn_8001C7B8.inc"
}
#else
/* Built with the unit-level -opt nopeephole (configure.py); retail has no
 * record-form compares here. */
s32 fn_8001C7B8(s8 slot) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* memset(void* dst, int val, u32 n);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, s32 parent, void* args, s32, s32, s32, ...);
#endif
    extern void pokemonToMenuPokemonStatus(void* pokemon, MenuPokemonStatus* status);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonGetStatus(void* pokemon, u32 a, u32 id, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonReplace(void* a, void* b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80097F08(void* pokemon, void* callback, s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_80018F54(s32 a, s32 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80019D5C();
#endif
    s32 swapArg;
    s32 summaryArg;
    s32 itemArg;
    s8* work;
    /* RULE-EXCEPTION(user-approved): single-field int carrier keeps menuOpen's result in sel's register (retail mr r23,r3) — see docs/RULE_EXCEPTIONS.md */
    struct { int value; } sel;
    s32 action;
    s32 target;
    s32 picked;
    void* pokemon;
    void* other;
    MenuPokemonStatus* status;
    u16 i;
    u16 item;
    u32 pocket;

    work = (s8*)lbl_803A1D40;
    do {
        work[0x14] = 1;
        if (std082_flagGetValue(0x8AE) == 0) {
#ifdef TARGET_PC
            /* OrreHook_PartySubmenuOpen (step 3.7, PURIFY in the party menu; pc/orre_hooks.h): OrrePort links its
             * entry into the menu's chain, or puts the game's own records back. NULL: as on the GameCube. All
             * platforms */
            if (OrreHook_PartySubmenuOpen != NULL) {
                OrreHook_PartySubmenuOpen(0x6B, slot);
            }
#endif
            sel.value = menuOpen(0x6B, 1);
            menuClose(0x6B);
        } else {
#ifdef TARGET_PC
            if (OrreHook_PartySubmenuOpen != NULL) {
                OrreHook_PartySubmenuOpen(0x10F, slot);
            }
#endif
            sel.value = menuOpen(0x10F, 1);
            menuClose(0x10F);
        }
        switch (sel.value) {
        case 0x3CF:
        case 0x52A:
            menuCloseCustom(0x63, 0, 1);
            pokemon = menuPokemonGetPokemon(*(s32*)(work + 0x8), slot, PTR32_AT_T(void, work, 0xC));
            fadeSet(3, lbl_8047B7C0);
            fadeCheck(1);
            fn_80097F08(PC_PTR32_IMPLICIT(pokemon), PC_PTR32_IMPLICIT(fn_80019D5C), 0);
            summaryArg = work[6];
            menuOpenCustom(0x63, 0, &summaryArg, 0, 0, 1, lbl_803A1C20);
            pokemon = windowSearchID(0x63);
            if (pokemon != 0) {
                ((u8*)pokemon)[0x98] = 1;
            }
            fadeSet(2, lbl_8047B7C0);
            fadeCheck(1);
            slot = work[6];
            sel.value = 0;
            break;
        case 0x3D0:
        case 0x52B:
            work[0x14] = 2;
            work[7] = slot;
            swapArg = work[6];
            picked = (s8)menuOpenCustom(0x63, 0, &swapArg, 0, 1, 1, lbl_803A1C20);
            if (((u8*)work)[1] == 0) {
                picked = -2;
            }
            target = (s8)picked;
            work[7] = -1;
            if (menuGetCursorItemID(0x63) != 0x3B6 && target != -1) {
                pokemon = menuPokemonGetPokemon(*(s32*)(work + 0x8), slot, PTR32_AT_T(void, work, 0xC));
                other = menuPokemonGetPokemon(*(s32*)(work + 0x8), target, PTR32_AT_T(void, work, 0xC));
                if (pokemon != 0 && pokemon != 0) {
                    menuPokemonSetSlotSeq(slot, 0x11A, 0x122);
                    menuPokemonSetSlotSeq(target, 0x11A, 0x122);
                    menuPokemonWaitSlot(slot);
                    pokemonReplace(pokemon, other);
                    menuPokemonRefresh();
                    fn_8001D718(lbl_8047B7C0);
                    menuPokemonSetSlotSeq(slot, 0x116, 0x11E);
                    menuPokemonSetSlotSeq(target, 0x116, 0x11E);
                    menuPokemonWaitSlot(slot);
                }
            }
            sel.value = -1;
            break;
        case 0x3D1:
            sel.value = menuPokemonItemMenu(work, slot, &itemArg);
            break;
        case -1:
        case 0x3D2:
        case 0x52C:
            sel.value = -1;
            break;
#ifdef TARGET_PC
        default:
            /* OrreHook_PartySubmenuChoice (step 3.7, PURIFY in the party menu; pc/orre_hooks.h): an entry OrrePort
             * added. Nonzero closes the party screen (-1 ends menuPokemonSub's loop, as B does). The game's switch has
             * no default, so with nothing registered (or 0) the menu opens again, as on the GameCube. All platforms */
            if (OrreHook_PartySubmenuChoice != NULL && OrreHook_PartySubmenuChoice(sel.value, slot) != 0) {
                return -1;
            }
            break;
#endif
        }
    } while (sel.value != -1);
    return 0;
}
#endif

#endif /* !MENU_POKEMON_CARVE_ONLY || MENU_POKEMON_8001C7B8_ONLY */

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_8001D378_ONLY) || \
    defined(MENU_POKEMON_8001C064_ONLY) || defined(MENU_POKEMON_8001C7B8_ONLY)
/* 0x8001D378 | 0x2AC */
#if !defined(MENU_POKEMON_8001C7B8_ONLY)
/* RULE-EXCEPTION(user-approved): local peephole-control pragma (the MENU_POKEMON_8001C7B8_ONLY carve's unit flag -opt nopeephole covers its inline copy) — see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
#endif
#if defined(MENU_POKEMON_8001C064_ONLY) || defined(MENU_POKEMON_8001C7B8_ONLY)
static inline
#endif
void menuPokemonRefresh(void) {
    extern u8 lbl_803A1C20[];
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* memset(void* dst, int val, u32 n);
#endif
    extern void pokemonToMenuPokemonStatus(void* pokemon, MenuPokemonStatus* status);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonGetStatus(void* pokemon, u32 a, u32 id, u32 b);
#endif
    /* Typed, not void*: the void* result is converted into this local instead
       of being copy-propagated, so the copy of menuPokemonRefresh inlined into
       fn_8001C7B8 keeps the extra coalesced pokemon copy that sets retail's
       r27-r30 order there (no effect on this function's own codegen). */
    struct Pokemon* pokemon;
    /* The O3 carve and O2 inlined caller color these locals in opposite orders. */
#if defined(MENU_POKEMON_8001D378_ONLY)
    u16 i;
    MenuPokemonStatus* status;
#else
    MenuPokemonStatus* status;
    u16 i;
#endif

    memset(lbl_803A1C20, 0, 0x120);
    for (i = 0; i < 6; i++) {
        status = &((MenuPokemonStatus*)lbl_803A1C20)[i];
        pokemon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), i,
                                        PTR32_AT_T(void, lbl_803A1D40, 0xC));
        if (pokemon == 0) {
            status->species = 0;
        } else {
            pokemonToMenuPokemonStatus(pokemon, status);
            if (pokemonGetStatus(pokemon, 0, POKEMON_STATUS_IS_EMPTY, 0) == 1) {
                status->unk1A = 0;
            }
            status->iconId = menuPokemonGetStatusIcon(pokemon);
        }
    }
}
#if !defined(MENU_POKEMON_8001C7B8_ONLY)
#pragma pop
#endif
#endif

#if !defined(MENU_POKEMON_CARVE_ONLY)

/* 0x8001D718 | 0xCC */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
extern f32 lbl_8047B7C8;
extern f64 lbl_8047B7D0;
extern f64 lbl_8047B7D8;
#if 0
asm void fn_8001D718(void) {
#include "src/game/gs_pcbox_fn_8001D718.inc"
}
#else
void fn_8001D718(f32 target) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
    f32 progress = lbl_8047B7C8;

    while (progress < target) {
        _threadSwitch();
        progress += (f32)GSgetFrameStep() / (f32)fn_800D37CC();
    }
}
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_80018594_ONLY) || \
    defined(MENU_POKEMON_800181C4_ONLY)
typedef struct MenuPokemonSummaryPage {
    u8 color[4];
    s32 kind;
    u8 unk8[0x44];
} MenuPokemonSummaryPage;

/* The quantity menu's trailing argument; its column count travels separately
   (as in the event-side copy bagItemToss). */
typedef struct MenuPokemonQuantityArgs {
    u8 color[4];
    s32 menuId;
    s32 maximum;
    s32 minimum;
    s32 unitPrice;
} MenuPokemonQuantityArgs;

/* RULE-EXCEPTION(user-approved): single-function inline helper (fn_800181C4) — see docs/RULE_EXCEPTIONS.md */
static inline u16 menuPokemonGetBagItemNum(u8* items, u16* count, s32 slot) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801429E8(void* item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 itemBiosGetNum(void* item);
#endif
    s32 i;
    s32 validIndex;
    u16 num;

    validIndex = -1;
    for (i = 0; i < *count; i++, items += 4) {
        if (fn_801429E8(PC_PTR32_IMPLICIT(items))) {
            validIndex++;
            if (validIndex >= slot) {
                /* RULE-EXCEPTION(user-approved): scalar carrier - the count is
                 * held in a named local before it is returned, chosen for
                 * register allocation (fn_800181C4) - see docs/RULE_EXCEPTIONS.md */
                num = itemBiosGetNum(items);
                return num;
            }
        }
    }
    return 0;
}

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_800181C4_ONLY)
/* RULE-EXCEPTION(user-approved): single-use inline helper (fn_800181C4) - the
 * quantity menu's open/close/choice, with the parent window id in a named
 * local, as in the event-side copy bagItemToss - see docs/RULE_EXCEPTIONS.md */
static inline s32 menuPokemonSelectQuantity(s32 menuId, s32* columns, MenuPokemonQuantityArgs* args) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, s32 parent, void* args, s32, s32, s32, ...);
#endif
    extern void menuClose(s32 menuId);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32 menuId, s32 wait);
#endif
    extern s32 lbl_8047A2FC;
    s32 menuResult;
    s32 parent;

    parent = windowGetActiveID();
    menuResult = menuOpenCustom(menuId, parent, columns, 0, 1, 1, args);
    menuClose(menuId);
    menuCloseSync(menuId, 1);
    if (menuResult == -1) {
        return -1;
    }
    return lbl_8047A2FC;
}

#if !defined(MENU_POKEMON_800181C4_ONLY)
/* RULE-EXCEPTION(user-approved): local peephole-control pragma (the MENU_POKEMON_800181C4_ONLY carve uses the unit's -opt nopeephole instead) — see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
#endif
s32 fn_800181C4(page, itemId, itemSlot)
    s32 page;
    u16 itemId;
    s32 itemSlot;
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 itemDataBiosGetPrice(void* itemData);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_8002A0B8(u8* color, u32 messageContext, s32 group,
                           s32 category, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpenWithSE(s32, u32, s32, s32, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpen(s32, u32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8012959C(void* hero, u16 item, u16 count, s16 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroItemDecItemDataId(void* hero, u16 item, u16 count,
                                      s16 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartRaw(s32 sound, s32, s32);
#endif
    extern void heroAddPokedoru(void* hero, u32 amount);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* heroItemGetItemKindToItemAryPtr(void* hero, u8 kind, u16* count,
                                               u16* total, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* heroHizukiItemGetItemAryPtr(void* hero, u16* count,
                                           s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, s32 parent, void* args, s32, s32, s32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32 menuId, s32 wait);
#endif
    extern MenuPokemonSummaryPage lbl_80266918[];
    extern u32 lbl_8047A2BC;
    extern u32 lbl_8047A2DC;
#ifndef TARGET_PC /* PC: one type per label in a file (GCC); 8-byte slot on the PC (pcdata.py), so the pointer view is safe */
    extern void* lbl_8047A2F8;
#else
    extern u32 lbl_8047A2F8;
#define lbl_8047A2F8 (*(void**)&lbl_8047A2F8)
#endif
    extern s32 lbl_8047A2FC;
    /* RULE-EXCEPTION(user-approved): declaration order and the named row
     * offset chosen for register allocation - see docs/RULE_EXCEPTIONS.md */
    u8* items;
    s32 selected;
    s32 half;
    u16 total;
    u16 count;
    MenuPokemonQuantityArgs args;
    s32 digits;
    s32 menuId;
    s32 price;
    s32 item;
    s32* kind;
    s32 offset;
    u32 context;
    s32 saleValue;
    s32 quantity;
    s32 columns;
    s8 answer;
    u8 messageColor;

    /* RULE-EXCEPTION(user-approved): scalar carrier - the item id's
     * zero-extended copy for the untyped and variadic calls, chosen for
     * register allocation - see docs/RULE_EXCEPTIONS.md */
    item = itemId;
    context = lbl_8047A2BC;
    price = itemDataBiosGetPrice(itemDataBiosGetPtr(item));
    if (price <= 0) {
        winMsgOpenWithSE(2, fn_8002A0B8(&messageColor, context, 0xB, 0x2D, item, -1),
                         1, 0, messageColor);
        winMsgClose(1);
        return 0;
    }

    lbl_8047A2DC = fn_8002A0B8(&messageColor, context, 0xD, 0x2D, item, -1);
    /* RULE-EXCEPTION(user-approved): page row walked to its kind field with a
     * pointer pre-increment (one register for the row and &kind, as retail)
     * - see docs/RULE_EXCEPTIONS.md */
    offset = page * 0x4C;
    kind = (s32*)((u8*)lbl_80266918 + offset);
    heroItemGetItemKindToItemAryPtr(lbl_8047A2F8, *++kind, 0, &total, 0, 0);
    if ((s32)total > 100) {
        digits = 3;
    } else {
        digits = 2;
    }
    if (*kind >= 0) {
        items = heroItemGetItemKindToItemAryPtr(lbl_8047A2F8, *kind, &count, 0, 0, 0);
    } else {
        items = PC_PTR(heroHizukiItemGetItemAryPtr(lbl_8047A2F8, &count, 0, 0, 0));
    }
    quantity = menuPokemonGetBagItemNum(items, &count, itemSlot);
    if (quantity < 1) {
        selected = 0;
    } else {
        half = price / 2;
        if (half > 0) {
            if (digits == 2) {
                columns = 1;
                menuId = 0x5D;
            } else {
                columns = 2;
                menuId = 0x5E;
            }
        } else {
            if (digits == 2) {
                columns = 1;
                menuId = 0x5B;
            } else {
                columns = 2;
                menuId = 0x5C;
            }
        }
        {
            u8* entry = (u8*)lbl_80266918;
            entry += offset;
            args.color[0] = entry[0];
            args.color[1] = entry[1];
            args.color[2] = entry[2];
        }
        args.menuId = menuId;
        args.minimum = 1;
        args.maximum = quantity;
        args.unitPrice = half;
        lbl_8047A2FC = 1;

        selected = menuPokemonSelectQuantity(menuId, &columns, &args);
    }
    if (selected < 0) {
        return 0;
    }

    saleValue = price / 2 * selected;
    winMsgOpenWithSE(2, fn_8002A0B8(&messageColor, context, 9, 0x4B, saleValue, -1),
                     1, 0, messageColor);
    answer = menuSubOpenYesNo(0, -1, -1, 0);
    if (answer == 1 || answer == -1) {
        winMsgClose(1);
        return 0;
    }

    if (*kind == -1) {
        fn_8012959C(lbl_8047A2F8, itemId, selected, itemSlot);
    } else {
        heroItemDecItemDataId(lbl_8047A2F8, itemId, selected, itemSlot);
    }
    soundStartRaw(0x3CB, 0, 0);
    heroAddPokedoru(lbl_8047A2F8, saleValue);
    winMsgOpen(2, fn_8002A0B8(&messageColor, context, 0xA, 0x2D, item, 0x4B, saleValue, -1),
               1, 0);
    winMsgClose(1);
    return 1;
}
#ifdef TARGET_PC
#undef lbl_8047A2F8
#endif
#if !defined(MENU_POKEMON_800181C4_ONLY)
#pragma pop
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_80018594_ONLY)
/* bagUseItemOnPokemon - 0x80018594 | size: 0x34c */
extern s32 winMsgOpen(s8 type, u32 message, u32 wait, u8 pause);
extern void winMsgClose(u32 wait);
extern void msgctrlSetValue(u32 id, u32 value);
extern s32 fn_800181C4(s32 page, s32 itemId, s32 itemSlot); /* K&R definition: u16 itemId arrives promoted */
extern u32 itemDataBiosGetFieldUseFunc();
extern u32 itemDataBiosGetBattleUseFunc();
extern s32 fn_80017CB8();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8012959C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseSync(); /* referenced by asm incs */
#endif
extern u32 lbl_8047A2E0;
extern u32 lbl_8047A2D8;
extern MenuPokemonSummaryPage lbl_80266918[];
#define sSummaryPageEntries ((u8*)lbl_80266918)
extern u32 lbl_8047A2DC;
extern u32 lbl_8047A2F8;
extern u32 lbl_8047A2B8;
extern u16 fn_80019754(u32 color);
#if 0
asm s32 fn_80018594() {
#include "src/game/gs_pcbox_fn_80018594.inc"
}
#else
typedef struct MenuPokemonSummaryAction {
    u32 label;
#ifdef TARGET_PC
    u32 callback; /* PC: a 4-byte cell; fn_80017CB8 fills these as 0x0C-byte SummaryMenuItem records (2026-10-17) */
#else
    s32 (*callback)();
#endif
    u32 value;
} MenuPokemonSummaryAction;

/* RULE-EXCEPTION(user-approved): single-function inline helper (bagUseItemOnPokemon) — see docs/RULE_EXCEPTIONS.md */
static inline s32 menuPokemonSelect(s32 menuId, void* arg, s32 count) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, s32, s32, s32, s32, s32, void*, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32);
#endif
    s32 ret;

    ret = menuOpenCustom(menuId, windowGetActiveID(), 0, 0, 1, 1, arg);
    menuClose(menuId);
    menuCloseSync(menuId, 1);
    if (ret == -1) {
        return -1;
    }
    if (ret < 0 || ret >= count) {
        return -1;
    }
    return ret;
}

#ifdef TARGET_PC
static int s_orre_bag_use; /* PC: the chosen bag action is Use (bagItemUse), for items never run out */
#endif
static inline void menuPokemonUseItem(u32 page, u16 itemId, s32 count, u32 slot) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8012959C(void* hero, u16 item, u16 count, s16 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroItemDecItemDataId(void* hero, u16 item, u16 count, s16 slot);
#endif

#ifdef TARGET_PC
    /* ORRE_ITEM_USE_KEEPS (items never run out; pc/orre_hooks.h): an item *used* from the bag stays. Only after the
     * bag's Use (s_orre_bag_use, set where the action is chosen): Toss must still remove it (snatcherLeo, 2026-11-13: "users
     * should be able to [toss], ... given the small bag space"), and the pocket move must not copy it */
    if (s_orre_bag_use && ORRE_ITEM_USE_KEEPS(itemId)) {
        return;
    }
#endif
    if (lbl_80266918[page].kind == -1) {
        fn_8012959C((void*)PC_PTR(lbl_8047A2F8), itemId, count, slot);
    } else {
        heroItemDecItemDataId((void*)PC_PTR(lbl_8047A2F8), itemId, count, slot);
    }
}

/* RULE-EXCEPTION(user-approved): local peephole-control pragma — see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 bagUseItemOnPokemon(boxIndex, itemId, slotIndex, outItem)
    u32 boxIndex;
    u16 itemId;
    u32 slotIndex;
    u16* outItem;
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, s32, s32, s32, s32, s32, void*, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8012959C(void* hero, u16 item, u16 count, s16 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroItemDecItemDataId(void* hero, u16 item, u16 count, s16 slot);
#endif
    struct {
        u8 r;
        u8 g;
        u8 b;
        u8 pad;
#ifdef TARGET_PC
        u32 entries; /* PC: a 4-byte cell; menu 0x5A's callbacks (gs_event_exec.c) read the block at GameCube offsets */
#else
        MenuPokemonSummaryAction* entries;
#endif
        s32 count;
    } menuArg;
    MenuPokemonSummaryAction actions[5];
    s32 count;
    s32 choice;
    s32 callbackRet;
    s32 callbackValue;
    s32 (*menuCallback)();
    s32 (*useCallback)();
    u8* page;
    u16 selected;

    *outItem = 0;
    if ((s32)lbl_8047A2E0 == 0 && itemId == 0x21E) {
        lbl_8047A2DC = 0x2B41;
        selected = fn_80019754((lbl_80266918[lbl_8047A2D8].color[0] << 24) |
                               (lbl_80266918[lbl_8047A2D8].color[1] << 16) |
                               (lbl_80266918[lbl_8047A2D8].color[2] << 8) | 0xFF);
        if (selected == 0) {
            return 1;
        }

        *outItem = selected;
        {
            void* item = itemDataBiosGetPtr(selected); /* r3 from the previous call (assembly) */
            if ((s32)lbl_8047A2E0 == 0) {
                useCallback = (s32 (*)())PC_PTR(itemDataBiosGetFieldUseFunc(item));
            } else {
                useCallback = (s32 (*)())PC_PTR(itemDataBiosGetBattleUseFunc(item));
            }
        }
        if (useCallback == 0) {
            winMsgOpen(2, 0x4261, 1, 0);
            winMsgClose(1);
            return 0;
        }

        if (useCallback(selected, &callbackValue) == 2) {
            if (callbackValue > 0) {
                heroItemDecItemDataId((void*)PC_PTR(lbl_8047A2F8), selected, callbackValue, -1);
            }
#ifdef TARGET_PC
            {   /* OrreHook_ScentUsed (step 3.7, the remembered scent cursors; pc/orre_hooks.h): which scent went on which
                 * party slot (menuUseItemCologn's pick, kept by itemUseClosePartyMenu). Observation only. All platforms */
                extern u32 lbl_8047A2EC;
                if (OrreHook_ScentUsed != NULL) {
                    OrreHook_ScentUsed(selected, (s32)lbl_8047A2EC);
                }
            }
#endif
            return 4;
        }
        return 1;
    }

    count = fn_80017CB8(actions, 5, boxIndex, slotIndex);
    lbl_8047A2DC = 0x135;
    page = sSummaryPageEntries + lbl_8047A2D8 * 0x4C;
    menuArg.r = page[0];
    menuArg.g = page[1];
    menuArg.b = page[2];
#ifdef TARGET_PC
    menuArg.entries = PC_PTR32(actions);
#else
    menuArg.entries = actions;
#endif
    menuArg.count = count;

    choice = menuPokemonSelect(0x5A, &menuArg, count);
    if (choice < 0) {
        return 1;
    }

#ifdef TARGET_PC
    menuCallback = (s32 (*)())PC_PTR(actions[choice].callback);
#else
    menuCallback = actions[choice].callback;
#endif
    if (menuCallback == 0) {
        return 1;
    }

#ifdef TARGET_PC
    /* OrreHook_ItemUseAgain (step 3.7, keep using an item; pc/orre_hooks.h): after a Use or a Give in the field the hook
     * may run the same action again on the item's (re-found) row, so the party list comes straight back with the item
     * still chosen. The picked party slot is the use / give functions' own lbl_8047A2EC (itemUseClosePartyMenu,
     * bagItemGive); it is preset to a value they never write, so an action that never opened the list (a refusal, a
     * key item) reads as "no Pokémon picked", and an untouched value is put back as the game left it. A used item's
     * count is taken inside the loop (each use takes its own), so the switch below sees nothing left to take. NULL
     * hook: one call, exactly the GameCube's. Platforms: all.
     * *Changed 2026-11-20 (snatcherLeo's hand check, research 1k):* OrreHook_PartyListHold brackets the loop (BEGIN before,
     * END after), so the use and give functions may leave the party list open between two uses instead of closing it
     * and opening it again (it slid apart); after the loop a list still held is closed here, where the game's last close
     * would have been. The action's result now goes to OrreHook_ItemUseAgain (a taught TM goes back to the bag). */
    {
        extern s32 bagItemUse(s32 entry_idx, s32 target_n, s32* out);
        extern s32 bagItemGive(s32 entry_idx, s32 target_n, s32* out);
        extern void menuPokemonClose(void);
        extern u32 lbl_8047A2EC;
        const u32 orreUnset = 0x7FFF0000u; /* never a party slot (0-5) nor -1 */
        const s32 orreKind = menuCallback == (s32 (*)())bagItemUse ? 1 : (menuCallback == (s32 (*)())bagItemGive ? 2 : 0);
        const s32 orreAsk = OrreHook_ItemUseAgain != NULL && orreKind != 0 && (s32)lbl_8047A2E0 == 0;
        if (orreAsk) {
            (void)ORRE_PARTY_LIST_HOLD(ORRE_HOLD_BEGIN, -1);
        }
        for (;;) {
            const u32 orreBefore = lbl_8047A2EC;
            s32 orreSlot;
            if (orreAsk) {
                lbl_8047A2EC = orreUnset;
            }
            callbackRet = menuCallback(boxIndex, slotIndex, &callbackValue);
            /* items never run out keeps an item only after Use (menuPokemonUseItem); Toss and the pocket move still take it */
            s_orre_bag_use = orreKind == 1;
            if (!orreAsk) {
                break;
            }
            orreSlot = lbl_8047A2EC == orreUnset ? -1 : (s32)lbl_8047A2EC;
            if (lbl_8047A2EC == orreUnset) {
                lbl_8047A2EC = orreBefore;
            }
            if (callbackRet != 0 && callbackRet != 3) {
                break; /* 4: used, leave the menus (scents, the Time Flute, key items): never repeated */
            }
            if (callbackRet == 0 && callbackValue > 0) {
                menuPokemonUseItem(boxIndex, itemId, callbackValue, slotIndex);
                callbackValue = 0;
            }
            if (OrreHook_ItemUseAgain(orreKind, itemId, (s32)boxIndex, orreSlot, callbackRet, &slotIndex) == 0) {
                break;
            }
        }
        if (orreAsk && ORRE_PARTY_LIST_HOLD(ORRE_HOLD_END, -1)) {
            menuPokemonClose(); /* the list the last use left open: closed now, then the bag comes back */
        }
    }
#else
    callbackRet = menuCallback(boxIndex, slotIndex, &callbackValue);
#endif
    switch (callbackRet) {
    case 0:
        if (callbackValue > 0) {
            menuPokemonUseItem(boxIndex, itemId, callbackValue, slotIndex);
        }
        lbl_8047A2B8 = slotIndex;
        return 0;
    case 1:
        return 2;
    case 2:
        return 3;
    case 3:
        return 1;
    case 4:
        if (callbackValue > 0) {
            menuPokemonUseItem(boxIndex, itemId, callbackValue, slotIndex);
        }
        return 4;
    default:
        return 0;
    }
}
#pragma pop
#endif


/* bagGiveItemCheck - 0x800188E0 | size: 0x188 */
extern u32 fn_80143FCC();
extern void bagItemStore();
extern u32 lbl_8047A2B8;
#if 0
asm void fn_800188E0(void) {
#include "src/game/gs_pcbox_fn_800188E0.inc"
}
#else
/* The island unit builds with -opt nopeephole (configure.py), so this
   pragma only applies to whole-file compiles. */
#if !defined(MENU_POKEMON_ISLAND_80018594_ONLY)
#pragma peephole off
#endif
s32 bagGiveItemCheck(s32 mode, u32 ptr, u32 r5, u32 r6, u16* out) {
    u16 tmp;
    s32 ret;

    tmp = 0;
    switch (mode) {
    case 0:
    case 1: {
        ret = bagUseItemOnPokemon(ptr, r5, r6, &tmp);
        if (ret == 2) {
            ret = 2;
        } else if (ret == 3) {
            ret = 3;
        } else if (ret == 4) {
            ret = 1;
        } else if (mode == 0) {
            ret = 0;
        } else if (ret == 0) {
            ret = 1;
        } else {
            ret = 0;
        }
        break;
    }
    case 2: {
        s32 r0;
        {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
            extern void itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC
            itemDataBiosGetPtr(r5);
#endif
        }
#ifdef TARGET_PC
        /* PC: the item record as the argument. On the GameCube itemDataBiosGetPtr's result stays in r3 for the call
         * (bl itemDataBiosGetPtr; bl fn_80143FCC at 0x800189B0 / 0x800189B4); the PC passed nothing, and
         * fn_80143FCC read the "important" byte through a leftover register: a party screen ITEM > GIVE crashed
         * (reading 0x10) in a P07 replay with step 3.7's tweaks on, 2026-11-20. Port correctness fix, not a hook */
        r0 = fn_80143FCC(itemDataBiosGetPtr(r5));
#else
        r0 = fn_80143FCC();
#endif
        if ((u8)r0 != 0) {
            msgctrlSetValue(0x2d, (u16)r5);
            winMsgOpen(2, 0x4262, 1, 0);
            winMsgClose(1);
            r0 = 0;
        } else {
            lbl_8047A2B8 = r6;
            r0 = 1;
        }
        if (r0 != 0) {
            ret = 1;
        } else {
            ret = 0;
        }
        break;
    }
    case 3:
        fn_800181C4(ptr, r5, r6);
        ret = 0;
        break;
    case 4:
        bagItemStore(ptr, r5, r6);
        ret = 0;
        break;
    default:
        break;
    }
    *out = tmp;
    return ret;
}
#if !defined(MENU_POKEMON_ISLAND_80018594_ONLY)
#pragma peephole reset
#endif
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_80018A68_ONLY)
/* bagMenuRun - 0x80018A68 | size: 0x4c8 */
#if defined(MENU_POKEMON_80018A68_ONLY)
typedef struct MenuPokemonSummaryPage {
    u8 color[4];
    s32 kind;
    u8 unk8[0x44];
} MenuPokemonSummaryPage;
extern MenuPokemonSummaryPage lbl_80266918[];
#define sSummaryPageEntries ((u8*)lbl_80266918)
s32 bagGiveItemCheck(s32 mode, u32 ptr, u32 r5, u32 r6, u16* out);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 itemBiosGetItemDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorBattleEndTerminateScripts(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorSetFadeScript();
#endif
extern u32 lbl_80478860;
extern u8 lbl_802E4DB0[];
extern u8 lbl_802EF0A8[];
extern u32 lbl_8047A2E8;
extern u32 lbl_8047A2F8;
extern u32 lbl_8047A2D8;
extern u32 lbl_8047A2E0;
extern u32 lbl_8047A2E4;

typedef struct MenuPokemonCursor {
    s8 base;
    s8 offset;
} MenuPokemonCursor;

typedef struct MenuPokemonPosEntry {
    u32 index;
    s16 x;
    s16 y;
    u8 unk8[4];
} MenuPokemonPosEntry;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MenuPokemonCursor cursorBiosGetPos(u16 id);
#else
/* MWCC returns this 2-byte struct in the top half of r3; the definition returns (u16)pos << 16 (the project notes 13) */
static MenuPokemonCursor menuPokemonCursorFromReg(u32 v) {
    /* the top half is the cursor table's u16 read in native order (cursor_bios.c); its two bytes in memory order are
     * the struct's two fields on either byte order */
    u16 raw = (u16)(v >> 16);
    MenuPokemonCursor c;
    memcpy(&c, &raw, sizeof(c));
    return c;
}
#define cursorBiosGetPos(id) menuPokemonCursorFromReg(cursorBiosGetPos(id))
#endif
extern void cursorBiosSetPos(u16 id, MenuPokemonCursor* pos);

/* The file-level externs declare both getters void; the casts give their real prototypes
 * (item array pointer result, u8 kind, u16* total).
 * RULE-EXCEPTION(user-approved): codegen-only casts; an unprototyped call puts (u8)kind in an extra coalesced temp that shifts bagMenuRun's colouring — see docs/RULE_EXCEPTIONS.md */
static inline u8* menuPokemonGetItemArray(s32 kind, u16* count) {
#ifdef TARGET_PC
    /* PC (port-gaps 70, 2026-11-06): the casts below pass the getters' optional out-pointers (max, flag1, flag2)
     * as s32 0. On Windows x64 the 5th and 6th arguments go on the stack as 8-byte slots, and a 4-byte 0 leaves the
     * upper half as leftover junk, so the getter saw a non-NULL out_flag2 and wrote 1 to it (a P10 replay with every
     * native traced crashed writing 0x7ff900000000 in heroGetHizukiItems from bagMenuRun; untraced runs only got
     * lucky with a zero upper half). The canonical prototypes (pc/prototypes.h) with real NULLs. */
    if (kind >= 0) {
        return (u8*)heroItemGetItemKindToItemAryPtr((struct HeroSaveData*)PC_PTR(lbl_8047A2F8), (u8)kind, count,
                                                    NULL, NULL, NULL);
    }
    /* the Hizuki getter returns the list as a 32-bit game address (PC_PTR32); decode it as gs_event_exec.c does */
    return (u8*)PC_PTR(heroHizukiItemGetItemAryPtr((u8*)PC_PTR(lbl_8047A2F8), count, NULL, NULL, NULL));
#endif
    if (kind >= 0) {
        return ((u8* (*)(void*, u8, u16*, u16*, s32, s32))heroItemGetItemKindToItemAryPtr)((void*)PC_PTR(lbl_8047A2F8), kind, count, 0, 0, 0);
    }
    return ((u8* (*)(void*, u16*, s32, s32, s32))heroHizukiItemGetItemAryPtr)((void*)PC_PTR(lbl_8047A2F8), count, 0, 0, 0);
}

/* Number of usable items in a bag pocket.
 * RULE-EXCEPTION(user-approved): single-function inline helper (bagMenuRun); expanded inside menuPokemonClampCursors, so its count slot is created last (sp+0x8) — see docs/RULE_EXCEPTIONS.md */
static inline s32 menuPokemonCountItems(s32 kind) {
    u16 count;
    u8* items;
    s32 i;
    s32 n;

    items = menuPokemonGetItemArray(kind, &count);
    n = 0;
    for (i = 0; i < count; i++, items += 4) {
        if (fn_801429E8(PC_PTR32_IMPLICIT(items)) != 0) {
            n++;
        }
    }
    return n;
}

/* Item id of the target-th usable item in a bag pocket, or 0.
 * RULE-EXCEPTION(user-approved): single-function inline helper (bagMenuRun); locals declared items, i, n for retail's r25/r24/r23 — see docs/RULE_EXCEPTIONS.md */
static inline u16 menuPokemonFindItem(s32 kind, s32 target) {
    u16 count;
    u8* items;
    s32 i;
    s32 n;

    items = menuPokemonGetItemArray(kind, &count);
    n = -1;
    for (i = 0; i < count; i++, items += 4) {
        if (fn_801429E8(PC_PTR32_IMPLICIT(items)) != 0) {
            n++;
            if (n >= target) {
                return itemBiosGetItemDataId(items);
            }
        }
    }
    return 0;
}

/* Pull each of the six pocket cursors back inside its pocket's usable item count.
 * RULE-EXCEPTION(user-approved): single-function inline helper (bagMenuRun); its slot counter i is an inline local, numbered above CountItems' n, and the indexed entry pointer becomes an induction temp, giving retail's r29/r28/r27 — see docs/RULE_EXCEPTIONS.md */
static inline void menuPokemonClampCursors(void) {
    MenuPokemonCursor pos;
    MenuPokemonCursor newPos;
    s32 i;
    s32 occupied;
    s32 excess;

    for (i = 0; i < 6; i++) {
        pos = cursorBiosGetPos(*(u32*)(sSummaryPageEntries + i * 0x4C + 0x1C));
        if (pos.offset < 0) {
            pos.offset = 0;
        }
        if (pos.base < 0) {
            pos.base = 0;
        }
        occupied = menuPokemonCountItems(*(s32*)(sSummaryPageEntries + i * 0x4C + 4));
        excess = pos.base + pos.offset - occupied;
        if (excess > 0) {
            if ((pos.offset -= (s8)excess) < 0) {
                pos.offset = 0;
            }
            excess = pos.base + pos.offset - occupied;
            if (excess > 0) {
                if ((pos.base -= (s8)excess) < 0) {
                    pos.base = 0;
                }
            }
        }
        newPos = pos;
        cursorBiosSetPos(*(u32*)(sSummaryPageEntries + i * 0x4C + 0x1C), &newPos);
    }
}

/* RULE-EXCEPTION(user-approved): single-function inline helper (bagMenuRun); the named index local gives the selected index an inline-variable number above FindItem's locals (retail r22) — see docs/RULE_EXCEPTIONS.md */
static inline s32 menuPokemonCursorIndex(MenuPokemonCursor* pos) {
    s32 index;

    index = pos->base + pos->offset;
    return index;
}

#if !defined(MENU_POKEMON_80018A68_ONLY)
/* RULE-EXCEPTION(user-approved): local peephole-control pragma (the MENU_POKEMON_80018A68_ONLY carve uses the unit's -opt nopeephole instead) — see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
#endif
u16 bagMenuRun(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, s32 parent, void* args, s32, s32, s32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
    u16 replacementSpecies;
    s32 targetIndex;
    MenuPokemonCursor initPos;
    MenuPokemonCursor selPos;
    s32 filterValue;
    s32 i;
    s32* filterPtr;
    u16 selectedSpecies;
    s32 ret;
    s32 done;
    s32 selectedOffset;
    s32 retrySelection;
    u16 species;

    filterPtr = 0;
    selectedOffset = 0;
    retrySelection = 0;
    done = 0;

    if (*(u8*)&lbl_80478860 != 0) {
        for (i = 0; i < 8; i++) {
            ((MenuPokemonPosEntry*)lbl_802E4DB0)[i].x = DISC_EL_S16(((s16*)(lbl_802EF0A8 + 4)), ((MenuPokemonPosEntry*)lbl_802E4DB0)[i].index * 14);
            ((MenuPokemonPosEntry*)lbl_802E4DB0)[i].y = DISC_EL_S16(((s16*)(lbl_802EF0A8 + 8)), ((MenuPokemonPosEntry*)lbl_802E4DB0)[i].index * 14);
        }
        *(u8*)&lbl_80478860 = 0;
    }

    initPos = cursorBiosGetPos(2);
    if (initPos.offset <= 0) {
        filterValue = 5;
        filterPtr = &filterValue;
    }

    while (done == 0) {
        if (retrySelection != 0) {
            lbl_8047A2E8 = selectedOffset;
        } else {
            lbl_8047A2E8 = -1;
        }

        menuPokemonClampCursors();

        lbl_8047A2D8 = -1;
        lbl_8047A2D8 = menuOpenCustom(0x59, windowGetActiveID(), filterPtr, 0, 1, 0);
        if ((s32)lbl_8047A2D8 == -1) {
            species = 0;
        } else {
            selPos = cursorBiosGetPos(*(u32*)(sSummaryPageEntries + lbl_8047A2D8 * 0x4C + 0x1C));
            targetIndex = menuPokemonCursorIndex(&selPos);
            {
                u16 item;

                /* RULE-EXCEPTION(user-approved): the item id is held in a block local so selectedOffset's copy is emitted before species' (retail mr r25,r22 then mr r5,r3) — see docs/RULE_EXCEPTIONS.md */
                item = menuPokemonFindItem(*(s32*)(sSummaryPageEntries + lbl_8047A2D8 * 0x4C + 4), targetIndex);
                selectedOffset = targetIndex;
                species = item;
            }
        }
        selectedSpecies = species;

        filterPtr = 0;
        retrySelection = 0;
        if (selectedSpecies == 0) {
            break;
        }
        /* RULE-EXCEPTION(user-approved): codegen-only cast; retail passes the u16 species unconverted (bagGiveItemCheck is defined with a u32 parameter) — see docs/RULE_EXCEPTIONS.md */
        ret = ((s32 (*)(s32, u32, u16, u32, u16*))bagGiveItemCheck)(lbl_8047A2E0, lbl_8047A2D8, selectedSpecies, selectedOffset, &replacementSpecies);
        if (replacementSpecies != 0) {
            selectedSpecies = replacementSpecies;
        }
        switch (ret) {
        case 0:
            break;
        case 1:
            done = 1;
            break;
        case 2:
            retrySelection = 1;
            break;
        case 3:
            filterValue = 0;
            filterPtr = &filterValue;
            break;
        }
    }

    menuCloseCustom(0x59, 0, 1);
    if ((s32)lbl_8047A2E4 != 0) {
        floorBattleEndTerminateScripts();
        floorSetFadeScript(0, 0);
    }
    return selectedSpecies;
}
#if !defined(MENU_POKEMON_80018A68_ONLY)
#pragma pop
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_80018F30_ONLY)
#if defined(MENU_POKEMON_ISLAND_80018F30_ONLY)
u16 bagMenuRun(void);
#endif
/* std016_menuUnk_80018a68 - 0x80018F30 | size: 0x24 */
extern void floorSetFadeScript(u32 a, u32 b);
extern void _threadSwitch(void);
extern u32 lbl_8047A2F0;
#if 0
asm void fn_80018F30(void) {
#include "src/game/gs_pcbox_fn_80018F30.inc"
}
#else
void std016_menuUnk_80018a68(void) {
    *(u16*)&lbl_8047A2F0 = bagMenuRun();
}
#endif

/* fn_80018F54 - 0x80018F54 | size: 0x34 */
extern u32 lbl_8047A2E0;
extern u32 lbl_8047A2F8;
extern u32 lbl_8047A2E4;
extern u32 lbl_8047A2BC;
#if 0
asm void fn_80018F54(void) {
#include "src/game/gs_pcbox_fn_80018F54.inc"
}
#else
u16 fn_80018F54(u32 a, u32 b, u32 c) { /* returns bagMenuRun's result (tail call) */
    lbl_8047A2E0 = a;
    lbl_8047A2F8 = c;
    lbl_8047A2E4 = 0;
    lbl_8047A2BC = b;
    return bagMenuRun();
}
#endif

/* fn_80018F88 - 0x80018F88 | size: 0xdc */
extern u32 fightTrainer_GetHeroPtr();
extern void _flagSet();
extern void std098_mapWarpToMenuRoom();
extern u32 lbl_8047A2F4;
extern u32 lbl_8047A2F8;
extern u32 lbl_8047A2EC;
extern u32 lbl_8047A2E0;
extern u32 lbl_8047A2E4;
extern u32 lbl_8047A2F0;
#if 0
asm void fn_80018F88(void) {
#include "src/game/gs_pcbox_fn_80018F88.inc"
}
#else
u32 fn_80018F88(s32 mode, s32* ptr, u32 val) {
    if (mode == 1) {
        lbl_8047A2F4 = val;
        lbl_8047A2F8 = fightTrainer_GetHeroPtr(val);
        {
            s32 v = ptr[0];
            lbl_8047A2EC = v;
            if (v < 0 || v >= 5) {
                lbl_8047A2EC = 0;
            }
        }
    } else {
        lbl_8047A2F4 = 0;
        lbl_8047A2F8 = val;
        lbl_8047A2EC = (u32)-1;
    }
    lbl_8047A2E0 = mode;
    lbl_8047A2E4 = 1;
    _flagSet(1, 0);
    std098_mapWarpToMenuRoom(0x38f);
    floorSetFadeScript(0, 0);
    _threadSwitch();
    if (*(u16*)&lbl_8047A2F0 == 0) {
        return 0;
    }
    if (ptr != NULL) {
        ptr[0] = lbl_8047A2EC;
    }
    return *(u16*)&lbl_8047A2F0;
}
#endif

/* fn_80019064 - 0x80019064 | size: 0xc */
extern u32 lbl_8047A2B8;
#if 0
asm void fn_80019064(void) {
#include "src/game/gs_pcbox_fn_80019064.inc"
}
#else
u32 fn_80019064(void) {
    return (u8)lbl_8047A2B8;
}
#endif

/* fn_80019070 - 0x80019070 | size: 0x68 */
typedef struct MenuPokemonSpeciesCacheEntry {
    u16 species;
    u16 pad;
    u32 data;
} MenuPokemonSpeciesCacheEntry;

typedef struct MenuPokemonSpeciesCache {
    MenuPokemonSpeciesCacheEntry entries[8];
    s32 count;
} MenuPokemonSpeciesCache;

extern u8 lbl_803A1B90[];
#if 0
asm void fn_80019070(void) {
#include "src/game/gs_pcbox_fn_80019070.inc"
}
#else
u32 fn_80019070(u16 species) {
    MenuPokemonSpeciesCache* cache;
    s32 i;
    u32 result;

    cache = (MenuPokemonSpeciesCache*)lbl_803A1B90;
    result = (u32)-1;

    for (i = 0; i < cache->count; i++) {
        if (cache->entries[i].species == species) {
            {
                /* RULE-EXCEPTION(user-approved): block-local entry pointer chosen for register allocation — see docs/RULE_EXCEPTIONS.md */
                MenuPokemonSpeciesCacheEntry* e = (MenuPokemonSpeciesCacheEntry*)lbl_803A1B90;
                e += i;
                result = e->data;
            }
            break;
        }
    }
    cache->count = 0;
    return result;
}
#endif

/* fn_800190D8 - 0x800190D8 | size: 0x40 */
#if 0
asm void fn_800190D8(void) {
#include "src/game/gs_pcbox_fn_800190D8.inc"
}
#else
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma push
#endif
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma optimization_level 4
#endif
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma peephole off
#endif
void std017_menuQueueAdd(u32 species, u32 data) {
    MenuPokemonSpeciesCache* cache;
    s32 count;

    cache = (MenuPokemonSpeciesCache*)lbl_803A1B90;
    count = cache->count;
    if (count >= 8) {
        return;
    }
    species = (u16)species;
    cache->entries[count].species = species;
    cache->entries[cache->count].data = data;
    cache->count = cache->count + 1;
}
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma pop
#endif
#endif

/* fn_80019118 - 0x80019118 | size: 0xec */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp();
#endif
extern u8 lbl_80266C10[];
extern u32 lbl_8047B7A4;
extern u32 lbl_8047A300;
extern u32 lbl_8047B7A0;
#if 0
asm void fn_80019118(void) {
#include "src/game/gs_pcbox_fn_80019118.inc"
}
#else
/* Matching trick: use if/else (not ternary) for cmpw+bne branch pattern;
   declare r30 as u8 to get clrlwi r0,r30,24 + cmplwi (not record form) */
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma peephole off
#endif
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma optimization_level 4
#endif
s32 fn_80019118(u8* a, u8* b) {
    u32 sp[4];
    u8* r4;
    s32 r7;
    u32 r0;
    u32 r30;
    s32 r4_val;

    r4 = PTR32_AT_T(u8, a, 0x60);
    sp[0] = *(u32*)(lbl_80266C10 + 0x0);
    sp[1] = *(u32*)(lbl_80266C10 + 0x4);
    sp[2] = *(u32*)(lbl_80266C10 + 0x8);
    sp[3] = *(u32*)(lbl_80266C10 + 0xC);
    r7 = (s32)(s8)a[0x95] + (s32)(4 - *(u32*)(r4 + 0xC));
    if (r7 < 0 || r7 >= 4) return 0;
    r0 = sp[r7];
    r4_val = (s32)(s16)*(s16*)(b + 0x6);
    if (r4_val == (s32)r0) {
        r30 = 1;
    } else {
        r30 = 0;
    }
    winSpriteSetDisp(b, r30);
    if ((u8)r30 != 0) {
        *(u8*)(b + 0x67) = *(f32*)&lbl_8047B7A0 * (*(f32*)&lbl_8047B7A4 - *(f32*)&lbl_8047A300);
    }
    return 0;
}
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma peephole reset
#endif
#endif

/* fn_80019204 - 0x80019204 | size: 0xa4 */
extern u8 lbl_80266C00[];
#if 0
asm void fn_80019204(void) {
#include "src/game/gs_pcbox_fn_80019204.inc"
}
#else
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma optimization_level 4
#endif
s32 fn_80019204(u8* a, u8* b) {
#if !defined(NO_LOCAL_PRAGMAS_MENUPOKEMON_ISLAND_80018F30)
#pragma peephole off
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winSpriteSetDisp();
#endif
    s32 table[4];
    u8* r5;
    s32 r8;
    s32 r0;
    s32 r6;
    u32 r4;
    r5 = PTR32_AT_T(u8, (u8*)a, 0x60);
    table[0] = *(u32*)(lbl_80266C00 + 0x0);
    table[1] = *(u32*)(lbl_80266C00 + 0x4);
    table[2] = *(u32*)(lbl_80266C00 + 0x8);
    table[3] = *(u32*)(lbl_80266C00 + 0xC);
    r8 = (s32)((s8)*(u8*)(a + 0x95)) + (s32)(4 - *(u32*)(r5 + 0xC));
    if (r8 < 0 || r8 >= 4) return 0;
    r0 = table[r8];
    r6 = (u32)(s32)(s16)*(s16*)(b + 0x6);
    if (r6 == r0) {
        r4 = 1;
    } else {
        r4 = 0;
    }
    winSpriteSetDisp(b, r4);
    return 0;
}
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_800192A8_ONLY)
/* fn_800192A8 - 0x800192A8 | size: 0x228 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemDataBiosGetName(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemDataBiosGetKind(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80142CF4(void);
#endif
extern u8 lbl_80266BF0[];

typedef struct MenuPokemonSlotIds {
    s32 ids[4];
} MenuPokemonSlotIds;

#if 0
asm void fn_800192A8(void) {
#include "src/game/gs_pcbox_fn_800192A8.inc"
}
#else
/* Shows how many of slot `i`'s held item the hero carries: the bag pocket's
 * count, then the summed stack sizes of the matching entries. */
/* RULE-EXCEPTION(user-approved): single-use inline helper; its locals are numbered after the caller's temporaries, which gives retail's register colouring - see docs/RULE_EXCEPTIONS.md */
static inline void menuPokemonShowItemCount(u8* data, s32 i) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr();
#endif
    extern void msgctrlSetValue();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetKind(void* item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801429E8(void* item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 itemBiosGetNum(void* item);
#endif
    u16 total;
    u16 count;
    u32 item;
    s16 width;
    u8* entry;
    s32 n;
    u32 sum;

    heroItemGetItemKindToItemAryPtr(0, itemDataBiosGetKind(itemDataBiosGetPtr(*(u16*)(data + 4 + i * 2))), 0, &count, 0, 0);
    msgctrlSetValue(0x34, count);
    width = (s16)(GSmsgGetRect(0xCA) >> 16);
    width += (s16)(GSmsgGetRect(0x12E) >> 16);
    GSmsgPrintColor(0xC3 - width, 0, -1, 0x12E);

    item = *(u16*)(data + 4 + i * 2);
    sum = 0;
    entry = heroItemGetItemKindToItemAryPtr(0, itemDataBiosGetKind(itemDataBiosGetPtr(item)), &total, 0, 0, 0);
    for (n = 0; n < total; n++, entry += 4) {
        if (fn_801429E8(PC_PTR32_IMPLICIT(entry)) != 0 && itemGetStatus(entry, 0, 0x1B, 0) == item) {
            sum += itemBiosGetNum(entry);
        }
    }
    msgctrlSetValue(0x34, sum);
    GSmsgPrintColor(0xC3 - (s16)(GSmsgGetRect(0xCA) >> 16), 0, -1, 0xCA);
}

#if !defined(MENU_POKEMON_800192A8_ONLY)
#pragma optimization_level 4
#endif
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 fn_800192A8(u8* a, u8* b) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetName(void* item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetKind(void* item);
#endif
    s32 slots[4];
    u16 count;
    u16 total;
    u8* data;
    u32 sum;
    s16 width;
    u32 name;
    u16 item;
    u8* entry;
    s32 n;
    s32 i;

    data = PTR32_AT_T(u8, a, 0x60);
    *(MenuPokemonSlotIds*)slots = *(MenuPokemonSlotIds*)lbl_80266BF0;
    for (i = 0; i < 4; i++) {
        if (*(s16*)(b + 0x6) == slots[i]) {
            break;
        }
    }
    i -= 4 - *(s32*)(data + 0xC);
    if (i < 0 || i >= *(s32*)(data + 0xC)) {
        return 0;
    }
    if (*(u16*)(data + 4 + i * 2) == 0) {
        name = 0x134;
    } else {
        name = itemDataBiosGetName(itemDataBiosGetPtr(*(u16*)(data + 4 + i * 2)));
    }
    GSmsgPrintColor(0, 0, -1, name);
    if (*(u16*)(data + 4 + i * 2) != 0) {
        heroItemGetItemKindToItemAryPtr(0, itemDataBiosGetKind(itemDataBiosGetPtr(*(u16*)(data + 4 + i * 2))), 0, &count, 0, 0);
        msgctrlSetValue(0x34, count);
        width = (s16)(GSmsgGetRect(0xCA) >> 16);
        width += (s16)(GSmsgGetRect(0x12E) >> 16);
        GSmsgPrintColor(0xC3 - width, 0, -1, 0x12E);

        item = *(u16*)(data + 4 + i * 2);
        sum = 0;
        entry = heroItemGetItemKindToItemAryPtr(0, itemDataBiosGetKind(itemDataBiosGetPtr(item)), &total, 0, 0, 0);
        for (n = 0; n < total; n++, entry += 4) {
            if (fn_801429E8(PC_PTR32_IMPLICIT(entry)) != 0 && itemGetStatus(entry, 0, 0x1B, 0) == item) {
                sum += itemBiosGetNum(entry);
            }
        }
        msgctrlSetValue(0x34, sum);
        GSmsgPrintColor(0xC3 - (s16)(GSmsgGetRect(0xCA) >> 16), 0, -1, 0xCA);
    }
    return 0;
}
#else
s32 fn_800192A8(u8* a, u8* b) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetName(void* item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetKind(void* item);
#endif
    s32 slots[4];
    u8* data;
    u32 name;
    s32 i;

    data = PTR32_AT_T(u8, a, 0x60);
    *(MenuPokemonSlotIds*)slots = *(MenuPokemonSlotIds*)lbl_80266BF0;
    for (i = 0; i < 4; i++) {
        if (*(s16*)(b + 0x6) == slots[i]) {
            break;
        }
    }
    i -= 4 - *(s32*)(data + 0xC);
    if (i < 0 || i >= *(s32*)(data + 0xC)) {
        return 0;
    }
    if (*(u16*)(data + 4 + i * 2) == 0) {
        name = 0x134;
    } else {
        name = itemDataBiosGetName(itemDataBiosGetPtr(*(u16*)(data + 4 + i * 2)));
    }
    GSmsgPrintColor(0, 0, -1, name);
    if (*(u16*)(data + 4 + i * 2) != 0) {
        menuPokemonShowItemCount(data, i);
    }
    return 0;
}
#endif
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_800194D0_ONLY)
/* fn_800194D0 - 0x800194D0 | size: 0x14 */
#if 0
asm void fn_800194D0(void) {
#include "src/game/gs_pcbox_fn_800194D0.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level 4) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
s32 fn_800194D0(u8* a, u8* b) {
    u8* r5;
    s32 ret = 0;
    r5 = PTR32_AT_T(u8, a, 0x60);
    *(u32*)(b + 0x64) = *(u32*)r5;
    return ret;
}
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_800194E4_ONLY)
/* fn_800194E4 - 0x800194E4 | size: 0xfc */
extern u8 lbl_802E4E10[];
#if 0
asm void fn_800194E4(void) {
#include "src/game/gs_pcbox_fn_800194E4.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
/* RULE-EXCEPTION(user-approved): local propagation control preserves the
 * consumed loop index as retail's byte offset; see docs/RULE_EXCEPTIONS.md. */
#pragma opt_propagation off
typedef struct MenuPokemonSlotPos {
    s32 id;
    s16 x;
    s16 y;
    s32 shift;
} MenuPokemonSlotPos;

s32 fn_800194E4(u8* a, u8* b) {
    u8* data;
    s32 i;
    s32 pos;
    MenuPokemonSlotPos* slot;
    s32 offset;

    data = PTR32_AT_T(u8, a, 0x60);
    if (*(s16*)(b + 0x6) != 0x26c) {
        *(u32*)(b + 0x64) = *(u32*)(data + 0x0);
    }
    pos = *(s32*)(data + 0xC);
    if (pos < 4) {
        slot = (MenuPokemonSlotPos*)lbl_802E4E10;
        for (i = 0; i < 6; slot++, i++) {
            if (*(s16*)(b + 0x6) == slot->id) {
                break;
            }
        }
        if (i < 6) {
            i *= sizeof(MenuPokemonSlotPos);
            offset = (4 - pos) * 0x1f;
            slot = (MenuPokemonSlotPos*)(lbl_802E4E10 + i);
            *(s16*)(b + 0x52) = offset + slot->x;
            if (slot->shift != 0) {
                *(s16*)(b + 0x56) = slot->y - offset;
            }
        }
    }
    return 0;
}
#pragma opt_propagation reset
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_800195E0_ONLY)
/* fn_800195E0 - 0x800195E0 | size: 0xa0 */
extern void winSeqSetMenu(s32 id, u32 seqId);
#if 0
asm void fn_800195E0(void) {
#include "src/game/gs_pcbox_fn_800195E0.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off; optimization_level 4) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
#pragma optimization_level 4
s32 fn_800195E0(u8* a) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* windowGetKeyInfo();
#endif
    u8* r3;
    u8* r30;
    u32 r31;
    s32 r5;
    s32 r0;
    s32 r4;
    r30 = a;
    r31 = *(u32*)(a + 0x60);
    r3 = windowGetKeyInfo();
    r5 = *(s32*)((u8*)PC_PTR(r31) + 0xC);
    r0 = *(u16*)(r3 + 0x6) & MENU_KEY_DOWN;
    if (r0 != 0) {
        r4 = (s32)(s8)*(u8*)(r30 + 0x95);
        r0 = r4 + 1;
        if (r0 >= r5) r0 = r5 - 1;
        *(s8*)(r30 + 0x95) = (s8)r0;
    }
    r0 = *(u16*)(r3 + 0x6) & MENU_KEY_UP;
    if (r0 != 0) {
        r4 = (s32)(s8)*(u8*)(r30 + 0x95);
        r0 = r4 - 1;
        if (r0 < 0) r0 = 0;
        *(s8*)(r30 + 0x95) = (s8)r0;
    }
    return 0;
}
#pragma peephole reset
#endif

/* fn_80019680 - 0x80019680 | size: 0xd4 */
extern u32 lbl_8047B7A8;
extern u32 lbl_8047A300;
extern u32 lbl_8047B7AC;
extern u32 lbl_8047B7A4;
#if 0
asm void fn_80019680(void) {
#include "src/game/gs_pcbox_fn_80019680.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 fn_80019680(u8* arg) {
    s32 val;
    val = (s8)arg[1];
    switch (val) {
    case 0:
        if ((s8)arg[2] == 0) {
            winSeqSetMenu(PC_PTR32_IMPLICIT((void*)0x5f), 0x66);
            *(f32*)&lbl_8047A300 = *(f32*)&lbl_8047B7A8;
            arg[2] = 1;
        }
        break;
    case 2: {
        f32 f1;
        f1 = *(f32*)&lbl_8047A300 + *(f32*)&lbl_8047B7AC;
        *(f32*)&lbl_8047A300 = f1;
        if (f1 > *(f32*)&lbl_8047B7A4) {
            *(f32*)&lbl_8047A300 = *(f32*)&lbl_8047B7A8;
        }
        break;
    }
    case 3:
        if ((s8)arg[2] == 0) {
            winSeqSetMenu(PC_PTR32_IMPLICIT((void*)0x5f), 0x6a);
            arg[2] = 1;
        }
        break;
    }
    return 0;
}
#else
s32 fn_80019680(u8* arg) {
    s32 val;
    val = (s8)arg[1];
    switch (val) {
    case 0:
        if ((s8)arg[2] == 0) {
            winSeqSetMenu(0x5f, 0x66);
            *(f32*)&lbl_8047A300 = *(f32*)&lbl_8047B7A8;
            arg[2] = 1;
        }
        break;
    case 2: {
        f32 f1;
        f1 = *(f32*)&lbl_8047A300 + *(f32*)&lbl_8047B7AC;
        *(f32*)&lbl_8047A300 = f1;
        if (f1 > *(f32*)&lbl_8047B7A4) {
            *(f32*)&lbl_8047A300 = *(f32*)&lbl_8047B7A8;
        }
        break;
    }
    case 3:
        if ((s8)arg[2] == 0) {
            winSeqSetMenu(0x5f, 0x6a);
            arg[2] = 1;
        }
        break;
    }
    return 0;
}
#endif
#pragma peephole reset
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_80019754_ONLY)
/* fn_80019754 - 0x80019754 | size: 0x1e4 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroItemCheckHaveItemDataId();
#endif
extern u32 lbl_80478868;
extern u32 lbl_80478BD8;
extern u8  lbl_802E4E10[];
extern u8  lbl_802EF0A8[];
#if 0
asm void fn_80019754(void) {
#include "src/game/gs_pcbox_fn_80019754.inc"
}
#else
/* RULE-EXCEPTION(user-approved): single-use inline helper - the owned kind-6 item scan is a static inline, as in retail: inlined, its count = 0 and loop counter start as `li r30,0; mr r31,r30` (the counter copies count's zero) with the counter in r31, count in r30 and the list cursor in r29; written in place the two start as separate `li 0` and the three swap registers (95.58% without) - see docs/RULE_EXCEPTIONS.md */
static inline s32 fn_80019754_collectItems(u16* ids) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr(u16 itemId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 itemDataBiosGetKind(void* itemData);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 heroItemCheckHaveItemDataId(s32 hero, u16 itemId);
#endif
    u16 i;
    s32 count;

    count = 0;
    for (i = 0; i < lbl_80478BD8 && count < 3; i++) {
        if (itemDataBiosGetKind(itemDataBiosGetPtr(i)) == 6) {
            if (heroItemCheckHaveItemDataId(0, i)) {
                ids[count] = i;
                count++;
            }
        }
    }
    return count;
}

u16 fn_80019754(u32 color) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, s32, s32, s32, s32, s32, void*, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32, s32);
#endif
    struct {
        u32 color;
        u16 ids[4];
        s32 count;
    } list;
    u8* e;
    s16* p4;
    s16* p8;
    s32 count;
    s32 sel;
    s32 r4;

    if ((s32)lbl_80478868 != 0) {
        e = lbl_802E4E10;
        p4 = (s16*)(lbl_802EF0A8 + 0x4);
        p8 = (s16*)(lbl_802EF0A8 + 0x8);
        *(s16*)(e + 0x4)  = DISC_EL_S16(p4, *(u32*)(e + 0x0)  * 0xe);
        *(s16*)(e + 0x6)  = DISC_EL_S16(p8, *(u32*)(e + 0x0)  * 0xe);
        *(s16*)(e + 0x10) = DISC_EL_S16(p4, *(u32*)(e + 0xc)  * 0xe);
        *(s16*)(e + 0x12) = DISC_EL_S16(p8, *(u32*)(e + 0xc)  * 0xe);
        *(s16*)(e + 0x1c) = DISC_EL_S16(p4, *(u32*)(e + 0x18) * 0xe);
        *(s16*)(e + 0x1e) = DISC_EL_S16(p8, *(u32*)(e + 0x18) * 0xe);
        *(s16*)(e + 0x28) = DISC_EL_S16(p4, *(u32*)(e + 0x24) * 0xe);
        *(s16*)(e + 0x2a) = DISC_EL_S16(p8, *(u32*)(e + 0x24) * 0xe);
        *(s16*)(e + 0x34) = DISC_EL_S16(p4, *(u32*)(e + 0x30) * 0xe);
        *(s16*)(e + 0x36) = DISC_EL_S16(p8, *(u32*)(e + 0x30) * 0xe);
        *(s16*)(e + 0x40) = DISC_EL_S16(p4, *(u32*)(e + 0x3c) * 0xe);
        *(s16*)(e + 0x42) = DISC_EL_S16(p8, *(u32*)(e + 0x3c) * 0xe);
        lbl_80478868 = 0;
    }

    list.color = color;
    count = fn_80019754_collectItems(list.ids);
    list.ids[count] = 0;
    list.count = count + 1;

    r4 = windowGetActiveID();
#ifdef TARGET_PC
    {   /* OrreHook_ScentListOpen (step 3.7, the scents; pc/orre_hooks.h): OrrePort may put the scents in another order
         * (Vivid first) and start the cursor on the scent used last; the list's CANCEL stays last. NULL: the game's
         * order and no start (the first row), as on the GameCube. All platforms */
        s32 orreStart = OrreHook_ScentListOpen != NULL ? OrreHook_ScentListOpen(list.ids, count) : -1;
        sel = menuOpenCustom(0x5f, r4, orreStart >= 0 && orreStart < count ? &orreStart : 0, 0, 1, 1, &list);
    }
#else
    sel = menuOpenCustom(0x5f, r4, 0, 0, 1, 1, &list);
#endif
    menuClose(0x5f);
    menuCloseSync(0x5f, 1);
    if (sel >= 0 && sel < list.count) {
        /* RULE-EXCEPTION(user-approved): codegen-only index cast (sel is already known >= 0); keeps MWCC from sharing one &list.ids register between the terminator store and this load, as retail recomputes it — see docs/RULE_EXCEPTIONS.md */
        return list.ids[(u32)sel];
    }
    return 0;
}
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_80019938_ONLY)
/* fn_80019938 - 0x80019938 | size: 0xbc */
extern s32 menuGetCursorItemID(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetAllocPtr();
#endif
#if 0
asm void fn_80019938(void) {
#include "src/game/gs_pcbox_fn_80019938.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level 4) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 4
void fn_80019938(u8* a, u8* b) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmsgPrintColor(s32, s32, s32, u32);
#endif
    u8* base;
    s16 r0;
    u32 r4;
    u32 r4v;
    base = windowGetAllocPtr(a);
    r0 = *(s16*)(b + 0x6);
    r4 = 0x0;
    switch (r0) {
    case (s16)0xe93:
        r4 = 0;
        break;
    case (s16)0xe94:
        r4 = 1;
        break;
    case (s16)0xe95:
        r4 = 2;
        break;
    case (s16)0xe96:
        r4 = 3;
        break;
    }
    base = base + r4 * 0xc;
    r4v = *(u32*)(base + 0x4);
    if (r4v == 0) return;
    msgctrlSetValue(0x37, r4v);
    GSmsgPrintColor(0x0, 0x0, (s32)*(u8*)(a + 0x8b) | (s32)(-0x100), 0xe7);
}
#endif

/* fn_800199F4 - 0x800199F4 | size: 0x128 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowAllocMemory();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuItemBiosSetSelectFlag();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif
extern u32 lbl_80478870;
#if 0
asm void fn_800199F4(void) {
#include "src/game/gs_pcbox_fn_800199F4.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 fn_800199F4(u8* arg) {
    u8* entry;
    u16* ids;
    s32 i;
    void* dst;

    if ((s8)arg[1] == 0) {
        dst = windowAllocMemory(arg, 0x48);
        if (dst != NULL) {
            memcpy(dst, PTR32_AT_T(void, arg, 0x60), 0x48); /* window +0x60: a 32-bit slot (pc/ptr32.h) */
        }
    }
    {
        u8* tmp;
        tmp = windowGetAllocPtr(arg);
        i = 0;
        ids = (u16*)&lbl_80478870;
        entry = tmp;
    }
    while (i < 4) {
        if (*(u32*)(entry + 4) != 0) {
            menuItemBiosSetSelectFlag(*ids, 1);
        } else {
            menuItemBiosSetSelectFlag(*ids, 0);
        }
        entry += 0xc;
        ids++;
        i++;
    }
    {
        s32 val;
        val = menuGetCursorItemID(*(u32*)(arg + 4));
        switch (val) {
        case 0xe93:
            *(u32*)(arg + 0x80) = 0;
            break;
        case 0xe94:
            *(u32*)(arg + 0x80) = 1;
            break;
        case 0xe95:
            *(u32*)(arg + 0x80) = 2;
            break;
        case 0xe96:
            *(u32*)(arg + 0x80) = 3;
            break;
        default:
            *(s32*)(arg + 0x80) = -1;
            break;
        }
    }
    return 0;
}
#pragma peephole reset
#endif

/* menuPokemonCloseWazaLight - 0x80019B1C | size: 0x2c */
#if 0
/* NAME-T1 fn_80019B1C */
asm void menuPokemonCloseWazaLight(void) {
#include "src/game/gs_pcbox_fn_80019B1C.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; optimization_level 4; scheduling off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma optimization_level 4
#pragma scheduling off
/* NAME-T1 fn_80019B1C */ /* XD's map, menuPokemon.o: closes the move list (menu 0x42) the PP items open with menuPokemonOpenWazaLight */
void menuPokemonCloseWazaLight(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
    menuCloseCustom(0x42, 0x0, 0x1);
}
#pragma pop
#endif

#endif /* !MENU_POKEMON_CARVE_ONLY */

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_80019B48_ONLY)
/* menuPokemonOpenWazaLight - 0x80019B48 | size: 0x214 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonToMenuWazaStatus(void);
#endif
extern u8 lbl_803A1BD8[];
#if 0
/* NAME-T1 fn_80019B48 */
asm void menuPokemonOpenWazaLight(void) {
#include "src/game/gs_pcbox_fn_80019B48.inc"
}
#else
/* RULE-EXCEPTION(user-approved): local peephole-control pragma — see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma optimization_level 4
#pragma peephole off
/* NAME-T1 fn_80019B48 */ /* XD's map, menuPokemon.o, right after CloseWazaLight; both games' PP items call it, then Close */
s32 menuPokemonOpenWazaLight(s8 slot) {
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, s32, s32, s32, s32, s32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
    void* pokemon;
    s32 result;

    pokemon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), slot,
                                    PTR32_AT_T(void, lbl_803A1D40, 0xC));
    if (pokemon == 0) {
        return -1;
    }
    ((void (*)(void*, u8*))pokemonToMenuWazaStatus)(pokemon, lbl_803A1BD8);
    result = menuOpenCustom(0x42, 0, 0, 0, 1, 1, lbl_803A1BD8);
    menuCloseCustom(0x42, 0, 1);
    return result;
}
#pragma pop
#endif
#endif

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_80019D5C_ONLY)

/* fn_80019D5C - 0x80019D5C | size: 0x210 */
#if 0
asm void fn_80019D5C(void) {
#include "src/game/gs_pcbox_fn_80019D5C.inc"
}
#else
void* fn_80019D5C(void* current, s32 dir) {
    extern u8 lbl_803A1D40[];
    s8* work = (s8*)lbl_803A1D40;
    s8 slot;
    void* pokemon;

    slot = work[6];
    switch (dir) {
    case 1:
        slot--;
        break;
    case 2:
        slot++;
        break;
    default:
        return current;
    }
    if (slot >= 6) {
        slot = 5;
    }
    if (slot < 0) {
        slot = 0;
    }
    if (slot != work[6]) {
        pokemon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), slot,
                                        PTR32_AT_T(void, lbl_803A1D40, 0xC));
        if (pokemon != 0) {
            current = pokemon;
            work[6] = slot;
        }
    }
    return current;
}
#endif
#endif /* !MENU_POKEMON_CARVE_ONLY || MENU_POKEMON_80019D5C_ONLY */

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_80019F6C_ONLY)
extern u16 pokemonBiosGetPokemonDataId(u8* ptr);

#if defined(MENU_POKEMON_80019F6C_ONLY)
/* First declarations the full file has made by this point. */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 itemDataBiosGetPtr(u16 speciesId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue();
#endif
extern u8 lbl_803A1D40[];
extern u8 lbl_803A1C20[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorItemID();
#endif
extern u8 lbl_802E4E58[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemDataBiosGetKind(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(u32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetKeyInfo();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 windowGetParam(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemDataBiosGetWazaMachineNo();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonIsDarkPokemon();
extern u8 pokemonDataBiosGetWazaMcn(u8* ptr, u16 idx);
extern void GSmsgPrintRect();
#endif
extern void fn_8010B9E8();
extern u32 fn_80107E78();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801081F8();
#endif
extern void windowDrawSprite2();
extern void* pokemonDataBiosGetPtr(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8001DACC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetGSchar();
#endif
extern u32 lbl_8047A308;
extern u32 lbl_8047B7B8;
extern u32 lbl_8047B7B0;
extern u32 menuSubCalcColor();
/* The carve builds with -opt nopeephole (configure.py), the pragma state the
   full file has reached at this point. */
#endif

/* menuPokemonDrawItem - 0x80019F6C | size: 0xa18 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 windowGetParam(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemDataBiosGetWazaMachineNo();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonIsDarkPokemon();
extern u8 pokemonDataBiosGetWazaMcn(u8* ptr, u16 idx);
extern void GSmsgPrintRect();
#endif
extern void fn_8010B9E8();
extern u32 fn_80107E78();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801081F8();
#endif
extern void windowDrawSprite2();
extern void* pokemonDataBiosGetPtr(u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8001DACC();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetGSchar();
#endif
extern u32 lbl_8047A308;
extern u32 lbl_8047B7B8;
extern u32 lbl_8047B7B0;
extern u32 menuSubCalcColor();
#if 0
asm void menuPokemonDrawItem(void) {
#include "src/game/gs_pcbox_menuPokemonDrawItem.inc"
}
#else
void menuPokemonDrawItem(u8* ctx, u8* pane) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr(u16 itemId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 itemDataBiosGetKind(void* item);
#endif
    u8* entry;
    void* mon;
    u32 msgId;
    s8 state;
    u32 paneColor;
    s32 visible;
    s32 color;
    u16 index;
    void* item;
    s32 cur;
    s32 max;
    s32 grade;
    s16 width;
    s32 x;
    s32 value;
    s32 msg;

    entry = PC_PTR((u8*)PC_PTR(windowGetParam((s32)PC_PTR32(ctx), 0)));
    msgId = 0;
    state = 0;
    if (entry == NULL) {
        return;
    }
    if (*(u16*)(entry + 0) == 0) {
        state = -1;
    } else if (*(s16*)(entry + 0x1A) == 0) {
        state = 1;
    }

    visible = 1;
    paneColor = -1;
    if (state == -1) {
        switch (*(s16*)(pane + 6)) {
        case 0x3BA: case 0x3BB: case 0x3BC: case 0x3BD:
        case 0x3BE: case 0x3BF: case 0x3C0: case 0x3C1:
        case 0x3C4: case 0x3C9: case 0x3CA: case 0x3CB:
        case 0x545: case 0x546: case 0x547:
        case 0x12A0: case 0x12A1: case 0x12A2:
            visible = 0;
            break;
        }
    } else if (state == 1) {
        switch (*(s16*)(pane + 6)) {
        case 0x3B8: case 0x3B9:
        case 0x3C4: case 0x3C5: case 0x3C6: case 0x3C7: case 0x3C8:
        case 0x129D: case 0x129E:
        case 0x12A0:
            paneColor = 0x808080FF;
            break;
        }
    }

    winSpriteSetDisp(pane, visible);
#ifdef TARGET_PC
    /* PC: the pane's +0x64 is a GXColor, four bytes r, g, b, a (menuSubCalcColor reads it so). A native u32 store put
     * a fainted Pokémon's 0x808080FF (50% grey) in x64's byte order, read back as r 0xFF, g/b 0x80, a 0x80: the party
     * screen tinted the fainted Pokémon's panel half-transparent red instead of darkening it (P11's party screen
     * against Dolphin's, 2026-10-29; visual check 45). The usual 0xFFFFFFFF reads the same either way */
    pane[0x64] = (u8)(paneColor >> 24);
    pane[0x65] = (u8)(paneColor >> 16);
    pane[0x66] = (u8)(paneColor >> 8);
    pane[0x67] = (u8)paneColor;
#else
    *(u32*)(pane + 0x64) = paneColor;
#endif
    if ((u8)visible == 0) {
        return;
    }

    color = (s32)menuSubCalcColor(ctx, pane);
    index = windowGetParam(PC_PTR((s32)PC_PTR32(ctx)), 1);

    if (lbl_803A1D40[0] == 3 || lbl_803A1D40[0] == 4) {
        item = itemDataBiosGetPtr(*(u16*)(lbl_803A1D40 + 0x12));
        if ((u8)itemDataBiosGetKind(item) == 4) {
            switch (*(s16*)(pane + 6)) {
            case 0x3BC:
            case 0x12A1:
                msgId = itemDataBiosGetWazaMachineNo(item);
                mon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), index,
                                            PTR32_AT_T(void, lbl_803A1D40, 0xC));
                if ((u8)pokemonIsDarkPokemon(mon) != 0) {
                    msg = 0x2B65;
                } else if ((u8)pokemonDataBiosGetWazaMcn((u8*)pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(mon)), (u8)msgId) == 0) {
                    msg = 0x2B65;
                } else {
                    msg = 0x2B64;
                }
#ifdef TARGET_PC
                /* OrreHook_TmLabel (step 3.7, the TM design; pc/orre_hooks.h): LEARNED for a Pokémon that already knows
                 * the move (the game's ABLE there). Look only. NULL: the game's label. All platforms */
                msg = ORRE_TM_LABEL(msg, mon, *(u16*)(lbl_803A1D40 + 0x12));
#endif
                GSmsgPrintColor(0, -4, color, msg);
            case 0x3BE:
            case 0x12A2:
                winSpriteSetDisp(pane, 0);
                return;
            }
        }
    }

    switch (*(s16*)(pane + 6)) {
    case 0x3C2:
    case 0x3C5:
    case 0x3C6:
    case 0x129D:
    case 0x129E:
        if (entry[0x29] == 0) {
            winSpriteSetDisp(pane, 1);
        } else {
            winSpriteSetDisp(pane, 0);
        }
        break;
    case 0x3B8:
    case 0x3B9:
    case 0x3C3:
    case 0x3C7:
    case 0x3C8:
        switch (entry[0x29]) {
        case 0:
            winSpriteSetDisp(pane, 0);
            break;
        case 1:
            winSpriteSetDisp(pane, 1);
            break;
        case 2:
            fn_8001DACC(ctx, pane, (f32)*(s16*)&lbl_8047A308 / *(f32*)&lbl_8047B7B0);
            winSpriteSetDisp(pane, 0);
            break;
        }
        break;
    case 0x3BA:
    case 0x3CB:
        if (*(u16*)(entry + 0x24) != 0) {
            windowDrawSprite2(0, 0, *(s16*)(pane + 0x54), *(s16*)(pane + 0x56), color, ctx, *(u16*)(entry + 0x24), 0);
        }
        break;
    case 0x3BB:
    case 0x3CA:
        if (*(u16*)(entry + 0x2A) != 0) {
            winSpriteSetDisp(pane, 1);
        } else {
            winSpriteSetDisp(pane, 0);
        }
        break;
    case 0x3BF:
        GSmsgPrintColor(0x2E, 0, color, 0x2BD4);
        msgctrlSetValue(0x34, *(s16*)(entry + 0x1A));
        GSmsgPrintRect(0, 0, 0x2C, *(s16*)(pane + 0x56), color, 0xDE);
        msgctrlSetValue(0x34, *(s16*)(entry + 0x18));
        GSmsgPrintRect(0, 0, (s16)(*(s16*)(pane + 0x54) - 2), *(s16*)(pane + 0x56), color, 0xDE);
        break;
    case 0x545:
        GSmsgPrintColor(0x1B, 0, color, 0x195);
        msgctrlSetValue(0x34, *(s16*)(entry + 0x1A));
        GSmsgPrintRect(0, 0, 0x1B, *(s16*)(pane + 0x56), color, 0xDF);
        msgctrlSetValue(0x34, *(s16*)(entry + 0x18));
        GSmsgPrintRect(0, 0, (s16)(*(s16*)(pane + 0x54) - 2), *(s16*)(pane + 0x56), color, 0xDF);
        break;
    case 0x3C0:
    case 0x546:
        msgctrlSetValue(0x34, entry[0x17]);
        GSmsgPrintRect(0, 0, *(s16*)(pane + 0x54), *(s16*)(pane + 0x56), color, 0xD3);
        break;
    case 0x3C1:
    case 0x547:
        msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(entry));
        GSmsgPrintColor(0, 0, color, 0xE7);
        x = (s16)(GSmsgGetRect(0xE7) >> 16);
        switch (entry[0x28]) {
        case 0:
            msgId = 0xD67;
            break;
        case 1:
            msgId = 0xD68;
            break;
        case 2:
            msgId = 0;
            break;
        }
        if (msgId != 0) {
            msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(msgId)));
            GSmsgPrintColor(x - 2, 0, color, 0xCF);
        }
        break;
    case 0x3C4:
    case 0x12A0:
        fn_8010B9E8(ctx, pane, *(u16*)(entry + 0x26));
        break;
    case 0x3BE:
    case 0x12A2:
        cur = *(s16*)(entry + 0x1A);
        max = *(s16*)(entry + 0x18);
        if (cur <= 0) {
            grade = 0;
        } else if (cur <= max * 20 / 100) {
            grade = 0x66;
        } else if (cur <= max * 50 / 100) {
            grade = 0x65;
        } else {
            grade = 0x64;
        }
#ifdef TARGET_PC
        /* PC: PowerPC's divw doesn't trap on a zero divisor (the Gekko gives 0, or -1 for a negative dividend, and
         * Dolphin does the same); x64's idiv raises an exception. P11's second pass crashed here when a party menu
         * opened right after the Relic Stone's purification with an entry whose max HP read 0 (2026-10-29) */
        if (max == 0) {
            static int logged;
            if (!logged) {
                logged = 1;
                OSReport("[menupoke] HP bar with max 0 (cur %d): drawn as on the GameCube\n", (int)cur);
            }
            width = (cur * *(s16*)(pane + 0x54) + (max - 1)) < 0 ? -1 : 0;
        } else
#endif
        width = (cur * *(s16*)(pane + 0x54) + (max - 1)) / max;
        if ((u16)grade != 0) {
            windowDrawSprite2(0, 0, width, *(s16*)(pane + 0x56), color, ctx, grade, 0);
        }
        break;
    case 0x543:
    case 0x12A4:
        if ((s8)lbl_803A1D40[7] >= 0 && (s8)lbl_803A1D40[7] < 6) {
            value = *(s16*)(lbl_802E4E58 + (s8)lbl_803A1D40[4] * 0x30 + (s8)lbl_803A1D40[7] * 8);
        } else {
            value = 0;
        }
        if (value == *(s32*)(ctx + 4)) {
            winSpriteSetDisp(pane, 1);
        } else {
            winSpriteSetDisp(pane, 0);
        }
        break;
    case 0x542:
    case 0x12A3:
        if ((s8)lbl_803A1D40[6] >= 0 && (s8)lbl_803A1D40[6] < 6) {
            value = *(s16*)(lbl_802E4E58 + (s8)lbl_803A1D40[4] * 0x30 + (s8)lbl_803A1D40[6] * 8);
        } else {
            value = 0;
        }
        if (value == *(s32*)(ctx + 4)) {
            winSpriteSetDisp(pane, 1);
            switch (lbl_803A1D40[0x14]) {
            case 0:
            case 2:
                if ((u8)fn_80107E78(ctx, (u16)*(s16*)(pane + 6), 0x2D) == 0) {
                    fn_801081F8(ctx, (u16)*(s16*)(pane + 6), 0x2D);
                }
                break;
            case 1:
            case 3:
                if ((u8)fn_80107E78(ctx, (u16)*(s16*)(pane + 6), 0x20D) == 0) {
                    fn_801081F8(ctx, (u16)*(s16*)(pane + 6), 0x20D);
                }
                break;
            }
        } else {
            winSpriteSetDisp(pane, 0);
        }
        break;
    case 0x3B6:
    case 0x3B7:
        switch (lbl_803A1D40[0x14]) {
        case 0:
        case 2:
            winSpriteSetDisp(pane, 1);
            break;
        case 1:
        case 3:
            winSpriteSetDisp(pane, 0);
            break;
        }
        break;
    }
}
#endif

/* menuPokemonDrawHelp - 0x8001A984 | size: 0x114 */
#if 0
asm void menuPokemonDrawHelp(void) {
#include "src/game/gs_pcbox_menuPokemonDrawHelp.inc"
}
#else
#if !defined(MENU_POKEMON_80019F6C_ONLY)
#pragma optimization_level 4
#endif
void menuPokemonDrawHelp(u8* a) {
    extern u8 lbl_803A1D40[];
    extern u8 lbl_803A1C20[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmsgPrintColor();
#endif
    u32 msg;

    msg = 0;
    switch (*(u8*)(lbl_803A1D40 + 0x14)) {
    case 0:
        switch (*(u8*)(lbl_803A1D40 + 0x0)) {
        case 3:
        case 4:
            if ((itemDataBiosGetKind(PC_PTR32_IMPLICIT(itemDataBiosGetPtr(*(u16*)(lbl_803A1D40 + 0x12)))) & 0xFF) == 4) {
                msg = 0x2b63;
            } else {
                msg = 0x2b61;
            }
            break;
        case 5:
            msg = 0x2b62;
            break;
        case 1:
        case 2:
        case 6:
        case 7:
        default:
            msg = 0x2b5d;
            break;
        }
        break;
    case 1:
        msgctrlSetValue(0x32, PC_PTR32_IMPLICIT(lbl_803A1C20 + (s8)*(u8*)(lbl_803A1D40 + 0x6) * 0x30));
        msg = 0x2b5e;
        break;
    case 2:
        msg = 0x2b5f;
        break;
    case 3:
        msg = 0x2b60;
        break;
    }
    if (msg != 0) {
        GSmsgPrintColor(0, 0, *(u8*)(a + 0x8b) | -0x100, msg);
    }
}
#endif

/* menuPokemonButton - 0x8001AA98 | size: 0xd8 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStartSE(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void);
#endif
#if 0
asm void menuPokemonButton(void) {
#include "src/game/gs_pcbox_menuPokemonButton.inc"
}
#else
#if !defined(MENU_POKEMON_80019F6C_ONLY)
#pragma push
#pragma optimization_level 4
#pragma peephole off
#endif
void menuPokemonButton(u8* a) {
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 windowGetKeyInfo();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuGetCursorItemID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuButtonNormal(u8*);
#endif
    s32 r3;
    u32 r31;
    u32 r3b;
    r31 = 0;
    if (*(u8*)(lbl_803A1D40 + 0x1) == 0) {
        *(u8*)(a + 0x98) = 0x1;
        return;
    }
    r3 = PC_PTR32_IMPLICIT(windowGetKeyInfo());
    r3 = *(u16*)((u8*)PC_PTR(r3) + 0x4);
    if ((r3 & 0x10) != 0) {
        r3b = menuGetCursorItemID(*(u32*)(a + 0x4));
        if ((r3b & 0xFFFF) == 0x3b6) r31 = 1;
    } else if ((r3 & 0x20) != 0) {
        r31 = 1;
    }
    if ((u8)r31 != 0) {
        if (*(u8*)(lbl_803A1D40 + 0x15) == 0) {
            soundStartSE(0x26);
            return;
        }
        *(u8*)(a + 0x98) = 0x1;
        *(u8*)(a + 0x99) = 0x1;
        return;
    }
    menuButtonNormal(a);
}
#if !defined(MENU_POKEMON_80019F6C_ONLY)
#pragma pop
#endif
#endif

/* menuPokemonCursor - 0x8001AB70 | size: 0x3d4 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuDataBiosGetType(void*);
#endif
#if 0
asm void fn_8001AB70(void) {
#include "src/game/gs_pcbox_fn_8001AB70.inc"
}
#else
void menuPokemonCursor(u8* ctx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* windowGetKeyInfo();
#endif
    u8 valid[8];
    s32 i;
    s8 cursor;
    s8 step;
    u16 buttons;
    s32 count;

    cursor = *(s8*)(ctx + 0x95);
    step = 0;
    for (i = 0; i < 6; i++) {
        if (menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), i,
                                  PTR32_AT_T(void, lbl_803A1D40, 0xC)) != 0) {
            valid[i] = 1;
        } else {
            valid[i] = 0;
        }
    }
    valid[i] = 1;

    buttons = *(u16*)(windowGetKeyInfo() + 6);
    if ((buttons & MENU_KEY_RIGHT) != 0) {
        step = 1;
        if (((s8*)lbl_803A1D40)[4] == 0) {
            if (cursor == 0) {
                if (((s8*)lbl_803A1D40)[5] == 3) {
                    cursor = ((s8*)lbl_803A1D40)[5];
                } else {
                    cursor = 2;
                }
            }
        } else if (cursor == 0) {
            if (((s8*)lbl_803A1D40)[5] == 3) {
                cursor = ((s8*)lbl_803A1D40)[5];
            } else {
                cursor = 2;
            }
        } else if (cursor == 1) {
            if (((s8*)lbl_803A1D40)[5] == 5) {
                cursor = ((s8*)lbl_803A1D40)[5];
            } else {
                cursor = 4;
            }
        }
    } else if ((buttons & MENU_KEY_LEFT) != 0) {
        step = -1;
        if (((s8*)lbl_803A1D40)[4] == 0) {
            switch (cursor) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
                cursor = 0;
                break;
            }
        } else if (((s8*)lbl_803A1D40)[4] == 1) {
            switch (cursor) {
            case 2:
            case 3:
                cursor = 0;
                break;
            case 4:
            case 5:
                cursor = 1;
                break;
            }
        }
    }

    if ((buttons & MENU_KEY_UP) != 0) {
        cursor--;
        step = -1;
        if (cursor < 0) {
            cursor = 6;
        }
    } else if ((buttons & MENU_KEY_DOWN) != 0) {
        step = 1;
#ifdef TARGET_PC
        /* the window's menu id is a 32-bit field (read as 8 bytes it glued the next field on: 0x1_00000063) */
        count = menuDataBiosGetType(*(u32*)(ctx + 4));
#else
        count = menuDataBiosGetType(PC_PTR32_IMPLICIT(*(void**)(ctx + 4)));
#endif
        cursor++;
        if (cursor >= count) {
            cursor = 0;
        }
    }

    if (step != 0) {
        while (valid[cursor] == 0) {
            cursor += step;
            if (cursor < 0 || cursor >= 7) {
                cursor = 7;
                break;
            }
        }
    }

    if (*(s8*)(ctx + 0x95) != cursor) {
        ((s8*)lbl_803A1D40)[5] = *(s8*)(ctx + 0x95);
        ((s8*)lbl_803A1D40)[6] = cursor;
        *(s8*)(ctx + 0x95) = cursor;
    }
}
#endif
#endif /* !MENU_POKEMON_CARVE_ONLY || MENU_POKEMON_80019F6C_ONLY */

#if !defined(MENU_POKEMON_CARVE_ONLY)

/* menuPokemonCtrl - 0x8001AF44 | size: 0x240 */
extern void menuItemBiosSetXY(s16 x, s16 y, s16 z);
extern u32 lbl_8047A308;
#if 0
asm void menuPokemonCtrl(void) {
#include "src/game/gs_pcbox_fn_8001AF44.inc"
}
#else
#pragma push
#pragma peephole off
s32 menuPokemonCtrl(s32 ctx) {
    extern u8 lbl_803A1D40[];
    extern u8 lbl_802E4E58[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, s32, s32, s32, s32, s32, void*, ...);
#endif
    u32 result;
    s32 byte_off;
    u8* iter;
    s32 i;

    result = windowGetParam(PC_PTR(ctx), 0);
    if (result == 0) return 0;

    if ((s8)*((u8*)PC_PTR(ctx) + 1) == 0) {
        *((s8*)PC_PTR(ctx) + 0x97) = -1;
        if ((s8)*((u8*)PC_PTR(ctx) + 2) == 0) {
            i = 0;
            byte_off = 0;
            iter = (u8*)PC_PTR(result);
            for (; i < 6; i++) {
                s16 sy, sx;
                s16 npcId;
                u8* slot;
                s32 state;

                slot = lbl_802E4E58 + (s32)(s8)lbl_803A1D40[4] * 0x30 + byte_off;
                menuItemBiosSetXY(*(s16*)(slot + 2), *(s16*)(slot + 4), *(s16*)(slot + 6));
                slot = lbl_802E4E58 + (s32)(s8)lbl_803A1D40[4] * 0x30 + byte_off;
                menuDataBiosSetXY(*(s16*)(slot + 0), *(s16*)(slot + 4), *(s16*)(slot + 6));

                slot = lbl_802E4E58 + (s32)(s8)lbl_803A1D40[4] * 0x30;
                menuOpenCustom((s32)*(s16*)(slot + byte_off), 0x63, 0, 0, 0, 2, (void*)iter, i);

                slot = lbl_802E4E58 + (s32)(s8)lbl_803A1D40[4] * 0x30;
                npcId = *(s16*)(slot + byte_off);
                menuDataBiosGetXY(npcId, (u16*)&sx, (u16*)&sy);
                if (sx > 0xfa) state = 0x116;
                else state = 0x11e;
                winSeqSetMenu(PC_PTR32_IMPLICIT((void*)PC_PTR((s32)npcId)), state);

                byte_off += 8;
                iter += 0x30;
            }
#ifdef TARGET_PC
            winSeqSetMenu(*(s32*)((u8*)PC_PTR(ctx) + 4), 1); /* the window's 32-bit menu id (menuPokemonCtrl_exact.c) */
#else
            winSeqSetMenu(PC_PTR32_IMPLICIT(*(void**)((u8*)PC_PTR(ctx) + 4)), 1);
#endif
            *((s8*)PC_PTR(ctx) + 2) = 1;
        }
    } else if ((s8)*((u8*)PC_PTR(ctx) + 1) == 3) {
        if ((s8)*((u8*)PC_PTR(ctx) + 2) == 0) {
            byte_off = 0;
            for (i = byte_off; byte_off < 6; byte_off++) {
                s16 sy, sx;
                s16 npcId;
                u8* slot;
                s32 state;

                slot = lbl_802E4E58 + (s32)(s8)lbl_803A1D40[4] * 0x30;
                npcId = *(s16*)(slot + i);
                menuDataBiosGetXY(npcId, (u16*)&sx, (u16*)&sy);
                if (sx > 0xfa) state = 0x11a;
                else state = 0x122;
                winSeqSetMenu(PC_PTR32_IMPLICIT((void*)PC_PTR((s32)npcId)), state);

                i += 8;
            }
#ifdef TARGET_PC
            winSeqSetMenu(*(s32*)((u8*)PC_PTR(ctx) + 4), 7); /* the window's 32-bit menu id */
#else
            winSeqSetMenu(PC_PTR32_IMPLICIT(*(void**)((u8*)PC_PTR(ctx) + 4)), 7);
#endif
            *((s8*)PC_PTR(ctx) + 2) = 1;
        }
    }
    *(s16*)&lbl_8047A308 = (s16)(((s32)*(s16*)&lbl_8047A308 + 1) % 1000);
    return 0;
}
#pragma pop
#endif

/* menuPokemonClose - 0x8001B184 | size: 0x68 */
#if 0
asm void menuPokemonClose(void) {
#include "src/game/gs_pcbox_fn_8001B184.inc"
}
#else
#pragma push
#pragma optimization_level 4
#pragma scheduling off
void menuPokemonClose(void) {
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorBattleEndTerminateScripts();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
    menuCloseCustom(0x63, 0x0, 0x1);
    if (*(u8*)(lbl_803A1D40 + 0x2) != 0) {
        floorBattleEndTerminateScripts();
        if (*(u8*)(lbl_803A1D40 + 0x3) != 0x1) floorSetFadeScript(0x0, 0x0);
        _threadSwitch();
    }
}
#pragma pop
#endif

#endif /* !MENU_POKEMON_CARVE_ONLY */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_SUB_ONLY)
/* menuPokemonSub - 0x8001B1EC | size: 0x8d8 */
#if defined(MENU_POKEMON_SUB_ONLY)
extern u8 lbl_803A1D40[];
extern u8 lbl_803A1C20[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuOpen();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeSet();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowSearchID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorItemID();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonIsDarkPokemon();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(s32);
#endif
extern s32 fn_8001C7B8(s8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fn_8001C064(s8, u8, u16, u16*);
#endif
#endif
extern void menuFaceRequestPokemonFace();
extern u32 menuPokemonCheckPokemonChange();
extern void fn_80097E58();
extern f32 pokemonGetDp();
extern u32 fn_8010B560();
extern void* fn_8001BEBC(u16*);
extern u16 lbl_8047A30A;
extern f32 lbl_8047B7C0;
extern u32 lbl_8047B7C4;
#if 0
asm void menuPokemonSub(void) {
#include "src/game/gs_pcbox_fn_8001B1EC.inc"
}
#else
/* RULE-EXCEPTION(user-approved): summary-path inline copy and scalar carriers
 * preserve retail register allocation; see docs/RULE_EXCEPTIONS.md. */
static inline void* menuPokemonGetSummaryPokemon(s32 mode, u16 index, void* trainer) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroBiosGetPokemonPtr(void* hero, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightFloorGetGcHeroFightTrainerPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightTrainerGetValidFightPokemonPtr(void* trainer, u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonGetStatus(void*, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(void* pokemon);
#endif
    struct { void* value; } pokemon;
    void* hero;
    void* fightPokemon;

    pokemon.value = 0;
    switch (mode) {
    case 0:
        if (index >= 6) {
            return 0;
        }
        hero = menuPokemonGetHero(mode, trainer);
        if (hero == 0) {
            return 0;
        }
        pokemon.value = heroBiosGetPokemonPtr(hero, index);
        break;
    case 1:
        if (index >= 6) {
            return 0;
        }
        if (trainer == 0) {
            trainer = PC_PTR(fightFloorGetGcHeroFightTrainerPtr(0));
        }
        if (trainer == 0) {
            return 0;
        }
        fightPokemon = fightTrainerGetValidFightPokemonPtr(trainer, index);
        if (fightPokemon == 0) {
            return 0;
        }
        pokemon.value = PC_PTR(pokemonGetStatus(fightPokemon, 0, POKEMON_STATUS_RECORD_COPY_PTR, 0));
        break;
    case 2:
        if (index >= 30) {
            return 0;
        }
        break;
    }
    if (pokemonCheckValid(pokemon.value) == 0) {
        pokemon.value = 0;
    }
    return pokemon.value;
}

s32 menuPokemonSub() {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* memset(void* dst, int val, u32 n);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 menuId, s32 parent, void* args, s32, s32, s32, ...);
#endif
    extern void pokemonToMenuPokemonStatus(void* pokemon, MenuPokemonStatus* status);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonGetStatus(void* pokemon, u32 a, u32 id, u32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80019D5C();
#endif
    s32 cursorArg;
    s32 summaryArg;
    u16 newItem;
    s8* work;
    s8* cursor;
    MenuPokemonStatus* status;
    void* pokemon;
    struct { u16 value; } slot;
    s8 selection;
    s32 result;

    work = (s8*)lbl_803A1D40;
    /* RULE-EXCEPTION(user-approved): cursor reached by pre-increment so it stays a register pointer as in retail — see docs/RULE_EXCEPTIONS.md */
    cursor = work + 5;
    *++cursor = 0;
    work[5] = 0;
    work[7] = -1;
    if (((u8*)work)[1] == 0) {
        *cursor = -1;
    }

    memset(lbl_803A1C20, 0, 0x120);
    for (slot.value = 0; slot.value < 6; slot.value++) {
        status = &((MenuPokemonStatus*)lbl_803A1C20)[slot.value];
        pokemon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), slot.value,
                                        PTR32_AT_T(void, lbl_803A1D40, 0xC));
        if (pokemon == 0) {
            status->species = 0;
        } else {
            pokemonToMenuPokemonStatus(pokemon, status);
            if (pokemonGetStatus(pokemon, 0, POKEMON_STATUS_IS_EMPTY, 0) == 1) {
                status->unk1A = 0;
            }
            status->iconId = menuPokemonGetStatusIcon(pokemon);
        }
    }
#ifdef TARGET_PC
    /* OrreHook_PartyCursorStart (step 3.7, keep using an item and the scents; pc/orre_hooks.h): the cursor may start on
     * the Pokémon the item was just used on (or a scent last went to) instead of the first. Only for a list that takes
     * a pick (work[1]; Sacred Ash's "show only" list keeps -1), and only onto a slot that holds a Pokémon (its status
     * record, filled just above). NULL: the first Pokémon, as on the GameCube. All platforms */
    if (OrreHook_PartyCursorStart != NULL && ((u8*)work)[1] != 0) {
        const s32 orreSlot = OrreHook_PartyCursorStart((s32)((u8*)work)[0], *(u16*)(lbl_803A1D40 + 0x12));
        if (orreSlot >= 0 && orreSlot < 6 && ((MenuPokemonStatus*)lbl_803A1C20)[orreSlot].species != 0) {
            *cursor = (s8)orreSlot;
        }
    }
#endif

    lbl_8047A30A = 0;
    menuFaceRequestPokemonFace(0, fn_8001BEBC, &lbl_8047A30A);
#ifdef TARGET_PC
    /* OrreHook_PartyPortraitWait (step 3.7, Smooth Party UI; pc/orre_hooks.h): the list may wait a few frames for its
     * portraits' loads to finish, so they are there on its first frame. NULL or 0: the list opens at once, as on the
     * GameCube. All platforms */
    if (OrreHook_PartyPortraitWait != NULL) {
        s32 orreWaited = 0;
        while (OrreHook_PartyPortraitWait(*(s32*)(lbl_803A1D40 + 0x8), orreWaited) != 0) {
            _threadSwitch();
            orreWaited++;
        }
    }
#endif

    while (1) {
        lbl_803A1D40[0x14] = 0;
        cursorArg = *cursor;
        selection = menuOpenCustom(0x63, 0, &cursorArg, 0, 1, 1, lbl_803A1C20);
        if (((u8*)work)[1] == 0) {
            selection = -2;
        }
        *cursor = selection;
        if (selection == -2) {
            break;
        }
        if (menuGetCursorItemID(0x63) == 0x3B6 || selection == -1) {
            *cursor = -1;
            break;
        }

        switch (lbl_803A1D40[0]) {
        case 1:
            result = fn_8001C7B8(selection);
            break;
        case 2: {
            struct { s32 value; } action;
            lbl_803A1D40[0x14] = 1;
            while (1) {
                if (lbl_803A1D40[0x15] != 0) {
                    action.value = menuOpen(0x6D, 1);
                    menuClose(0x6D);
                } else {
                    action.value = menuOpen(0x108, 1);
                    menuClose(0x108);
                }
                switch (action.value) {
                case 0:
                    if ((u8)menuPokemonCheckPokemonChange(*(void**)(lbl_803A1D40 + 0x18),
                                                          PTR32_AT_T(void, lbl_803A1D40, 0xC),
                                                          selection) != 0) {
                        action.value = -1;
                    } else {
                        action.value = 1;
                    }
                    break;
                case 1:
                    menuCloseCustom(0x63, 0, 1);
                    pokemon = menuPokemonGetSummaryPokemon(*(s32*)(lbl_803A1D40 + 0x8), selection,
                                                           PTR32_AT_T(void, lbl_803A1D40, 0xC));
                    fadeSet(3, lbl_8047B7C0);
                    fadeCheck(1);
                    fn_80097E58(PTR32_AT_T(void, lbl_803A1D40, 0xC), pokemon, fn_80019D5C, 0);
                    summaryArg = *cursor;
                    menuOpenCustom(0x63, 0, &summaryArg, 0, 0, 1, lbl_803A1C20);
                    pokemon = windowSearchID(0x63);
                    if (pokemon != 0) {
                        ((u8*)pokemon)[0x98] = 1;
                    }
                    fadeSet(2, lbl_8047B7C0);
                    fadeCheck(1);
                    selection = *cursor;
                    continue;
                case -1:
                case 2:
                    action.value = 1;
                    break;
                }
                break;
            }
            result = action.value;
            break;
        }
        case 3:
        case 4:
            result = -1;
            break;
        case 5:
            result = ((s32 (*)(s8, u8, u16, u16*))fn_8001C064)(selection, lbl_803A1D40[0x11],
                                                              *(u16*)(lbl_803A1D40 + 0x12), &newItem);
            break;
        case 6:
            result = -1;
            break;
        case 7: {
            pokemon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), selection,
                                            PTR32_AT_T(void, lbl_803A1D40, 0xC));
            if (pokemon == 0) {
                continue;
            }
            if ((u8)pokemonIsDarkPokemon(pokemon) == 0) {
                winMsgOpen(2, 0x44DD, 1, 0);
                winMsgClose(1);
                continue;
            }
            if (*(f32*)&lbl_8047B7C4 != pokemonGetDp(pokemon)) {
                winMsgOpen(2, 0x44DE, 1, 0);
                winMsgClose(1);
                continue;
            }
            result = -1;
            break;
        }
        }

        if (result < 0) {
            break;
        }
    }

    while ((u8)fn_8010B560() != 0) {
        _threadSwitch();
    }
    *(s32*)(lbl_803A1D40 + 0x1C) = *cursor;
    return result;
}
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_ISLAND_8001BAC4_ONLY)
/* menuPokemonOpenSub - 0x8001BAC4 | size: 0x228 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCreateOffScreen();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuReleaseOffScreen();
#endif
extern u32 lbl_8047B7B8;
extern u32 lbl_8047B7C4;
#if 0
asm void menuPokemonOpenSub(void) {
#include "src/game/gs_pcbox_fn_8001BAC4.inc"
}
#else
u32 menuPokemonOpenSub(u8 a0, u8 a1, u8 a2, u16 a3, u32 a4, u8 a5) {
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
    extern void _flagSet();
    extern void std098_mapWarpToMenuRoom();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuPokemonSub(u8, u16, u32);
#endif
    s32 v;

    *(u8*)(lbl_803A1D40 + 0x0) = a0;
    *(u8*)(lbl_803A1D40 + 0x10) = a1;
    *(u8*)(lbl_803A1D40 + 0x11) = a2;
    *(u16*)(lbl_803A1D40 + 0x12) = a3;
    *(u32*)(lbl_803A1D40 + 0xC) = a4;
    *(u8*)(lbl_803A1D40 + 0x1) = a5;
    *(u32*)(lbl_803A1D40 + 0x1C) = 0;
    *(u8*)(lbl_803A1D40 + 0x15) = 1;

    switch ((s32)(u8)a0) {
    case 2:
    case 4:
        *(u32*)(lbl_803A1D40 + 0x8) = 1;
        break;
    case 3:
    default:
        *(u32*)(lbl_803A1D40 + 0x8) = 0;
        break;
    }

    v = (s32)*(u8*)(lbl_803A1D40 + 0x0);
    switch (v) {
    case 3:
    case 4:
    case 5:
        *(u8*)(lbl_803A1D40 + 0x2) = 0;
        *(u8*)(lbl_803A1D40 + 0x3) = 0;
        break;
    case 1:
        *(u8*)(lbl_803A1D40 + 0x2) = 1;
        *(u8*)(lbl_803A1D40 + 0x3) = 0;
        break;
    case 2:
        *(u8*)(lbl_803A1D40 + 0x2) = 1;
        *(u8*)(lbl_803A1D40 + 0x3) = 1;
        if ((u16)a3 == 0) {
            *(u8*)(lbl_803A1D40 + 0x15) = 0;
        }
    case 6:
    case 7:
    default:
        menuCreateOffScreen(*(f32*)&lbl_8047B7C4 / (f32)fn_800D37CC());
        *(u8*)(lbl_803A1D40 + 0x2) = 1;
        *(u8*)(lbl_803A1D40 + 0x3) = 2;
        break;
    }
    if (*(u8*)(lbl_803A1D40 + 0x2) == 1) {
        _flagSet(1, 1);
        std098_mapWarpToMenuRoom(0x38f);
        if (*(u8*)(lbl_803A1D40 + 0x3) != 1) {
            floorSetFadeScript(0, 0);
        }
        _threadSwitch();
        if (*(u8*)(lbl_803A1D40 + 0x3) == 2) {
            menuReleaseOffScreen(*(f32*)&lbl_8047B7C4 / (f32)fn_800D37CC());
        }
    } else {
        menuPokemonSub(a0, (u16)a3, a4);
    }
    *(u8*)(lbl_803A1D40 + 0x4) = 1;
    return *(u32*)(lbl_803A1D40 + 0x1C);
}
#endif

#endif /* guard split */
#if !defined(MENU_POKEMON_CARVE_ONLY)
/* menuPokemonOpenItemGive - 0x8001BCEC | size: 0x50 */
#if 0
asm void menuPokemonOpenItemGive(void) {
#include "src/game/gs_pcbox_fn_8001BCEC.inc"
}
#else
#pragma push
#pragma optimization_level 4
#pragma peephole off
#ifdef TARGET_PC
/* PC: void on the GameCube, but its callers read menuPokemonOpenSub's result from r3 (the chosen party slot); the PC
 * returns it, as menuPokemonOpen does (2026-10-29; port-gaps 34's lead) */
u32 menuPokemonOpenItemGive(u32 a, u32 b, u32 c, u32 d) {
#else
void menuPokemonOpenItemGive(u32 a, u32 b, u32 c, u32 d) {
#endif
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuPokemonOpenSub(u32, u32, u32, u32, u32, u32);
#endif
    *(u8*)(lbl_803A1D40 + 0x4) = 0x1;
#ifdef TARGET_PC
    return menuPokemonOpenSub(0x5, a, b, c, d, 0x1);
#else
    menuPokemonOpenSub(0x5, a, b, c, d, 0x1);
#endif
}
#pragma pop
#endif

/* menuPokemonOpenItemUse - 0x8001BD3C | size: 0x44 */
#if 0
asm void menuPokemonOpenItemUse(void) {
#include "src/game/gs_pcbox_fn_8001BD3C.inc"
}
#else
#pragma push
#pragma optimization_level 4
#pragma peephole off
#ifdef TARGET_PC
u32 menuPokemonOpenItemUse(u32 a, u32 b, u32 c, u32 d) { /* PC: returns the chosen slot (see menuPokemonOpenItemGive) */
#else
void menuPokemonOpenItemUse(u32 a, u32 b, u32 c, u32 d) {
#endif
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonOpenSub();
#endif
    *(u8*)(lbl_803A1D40 + 0x4) = 0x1;
#ifdef TARGET_PC
    return menuPokemonOpenSub(a, 0x0, 0x0, c, d, b);
#else
    menuPokemonOpenSub(a, 0x0, 0x0, c, d, b);
#endif
}
#pragma pop
#endif

/* menuPokemonOpenFight - 0x8001BD80 | size: 0x74 */
#if 0
asm void menuPokemonOpenFight(void) {
#include "src/game/gs_pcbox_fn_8001BD80.inc"
}
#else
#pragma push
#pragma optimization_level 4
#pragma peephole off
#ifdef TARGET_PC
u32 menuPokemonOpenFight(u8 a, u8 b, u32 c, u32 d) { /* PC: returns the chosen slot (see menuPokemonOpenItemGive) */
#else
void menuPokemonOpenFight(u8 a, u8 b, u32 c, u32 d) {
#endif
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuPokemonOpenSub(u32, u32, u32, u32, u32, u32);
#endif
    if (a == 0x1) {
        *(u8*)(lbl_803A1D40 + 0x4) = 0x0;
    } else {
        *(u8*)(lbl_803A1D40 + 0x4) = 0x1;
    }
    *(u32*)(lbl_803A1D40 + 0x18) = d;
#ifdef TARGET_PC
    return menuPokemonOpenSub(0x2, 0x0, 0x0, b, c, 0x1);
#else
    menuPokemonOpenSub(0x2, 0x0, 0x0, b, c, 0x1);
#endif
}
#pragma pop
#endif

/* menuPokemonOpen - 0x8001BDF4 | size: 0x44 */
#if 0
asm void menuPokemonOpen(void) {
#include "src/game/gs_pcbox_fn_8001BDF4.inc"
}
#else
#pragma push
#pragma optimization_level 4
#pragma peephole off
u32 menuPokemonOpen(u32 a, u32 b, u32 c) { /* returns the sub-call's result */
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuPokemonOpenSub(u32, u32, u32, u32, u32, u32);
#endif
    *(u8*)(lbl_803A1D40 + 0x4) = 0x1;
    return menuPokemonOpenSub(a, 0x0, 0x0, b, c, 0x1);
}
#pragma pop
#endif

/* menuPokemonMain - 0x8001BE38 | size: 0x84 */
#if 0
asm void menuPokemonMain(void) {
#include "src/game/gs_pcbox_fn_8001BE38.inc"
}
#else
#pragma push
#pragma scheduling off
#pragma optimization_level 4
s32 menuPokemonMain(void) {
    extern u8 lbl_803A1D40[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonSub();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorBattleEndTerminateScripts();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
    u8 r3;
    u16 r4;
    u32 r5;
    r3 = *(u8*)(lbl_803A1D40 + 0x0);
    r4 = *(u16*)(lbl_803A1D40 + 0x12);
    r5 = *(u32*)(lbl_803A1D40 + 0xC);
    menuPokemonSub((u32)r3, (u32)r4, r5);
    menuCloseCustom(0x63, 0x0, 0x1);
    if (*(u8*)(lbl_803A1D40 + 0x2) != 0) {
        ((void (*)(void))floorBattleEndTerminateScripts)();
        if (*(u8*)(lbl_803A1D40 + 0x3) != 0x1) floorSetFadeScript(0x0, 0x0);
        ((void (*)(void))_threadSwitch)();
    }
    return 0;
}
#pragma pop
#endif

#endif /* !MENU_POKEMON_CARVE_ONLY */

#if !defined(MENU_POKEMON_CARVE_ONLY) || defined(MENU_POKEMON_8001BEBC_ONLY)
/* fn_8001BEBC - 0x8001BEBC | size: 0x1a8 */
#if 0
asm void fn_8001BEBC(void) {
#include "src/game/gs_pcbox_fn_8001BEBC.inc"
}
#else
void* fn_8001BEBC(u16* index) {
    extern u8 lbl_803A1D40[];
    void* pokemon;

    pokemon = menuPokemonGetPokemon(*(s32*)(lbl_803A1D40 + 0x8), *index,
                                    PTR32_AT_T(void, lbl_803A1D40, 0xC));
    (*index)++;
    return pokemon;
}
#endif
#endif

#if !defined(MENU_POKEMON_CARVE_ONLY)

/* menuPokemonGetStatusSprite - 0x8001D624 | size: 0xf4 */
extern u8 lbl_802E4EC8[];
#if 0
asm void fn_8001D624(void) {
#include "src/game/gs_pcbox_fn_8001D624.inc"
}
#else
#pragma push
#pragma peephole off
#pragma optimization_level 4
u16 menuPokemonGetStatusSprite(void* a, u8 b) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetJoutaiMenuSpriteId();
#endif
    u32 r3 = pokemonGetStatus(a, 0x0, POKEMON_STATUS_IS_EMPTY, 0x0);
    if ((u8)r3 == 0x1) {
        r3 = 0x1;
    } else {
        r3 = pokemonGetJoutaiMenuSpriteId(a);
        switch ((u16)r3) {
        case 0x3a: r3 = 0x2; break;
        case 0x3b: r3 = 0x3; break;
        case 0x3c: r3 = 0x4; break;
        case 0x3d: r3 = 0x5; break;
        case 0x3e: r3 = 0x6; break;
        default: r3 = 0x0; break;
        }
    }
    if ((u8)b == 0) {
        return *(u16*)(lbl_802E4EB8 + (u16)r3 * 2);
    } else {
        return *(u16*)(lbl_802E4EC8 + (u16)r3 * 2);
    }
}
#pragma pop
#endif
#endif /* !MENU_POKEMON_CARVE_ONLY */
