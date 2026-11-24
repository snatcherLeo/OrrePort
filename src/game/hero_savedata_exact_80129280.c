/**
 * @file hero_savedata_exact_80129280.c
 * @brief Exact savedata and hero core range, 0x80129280 - 0x8012A450.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

typedef struct HeroPokemonData {
    u8 bytes[0x138];
} HeroPokemonData;

typedef struct HeroItemSlot {
    u16 id;
    u16 count;
} HeroItemSlot;

typedef struct HeroSaveData {
    u8 name[0x2C];
    u32 rnd;
    HeroPokemonData pokemon[6];
    HeroItemSlot normalItems[0x14];
    HeroItemSlot extraItems[0x2B];
    HeroItemSlot ballItems[0x10];
    HeroItemSlot skillItems[0x40];
    HeroItemSlot seedItems[0x2E];
    HeroItemSlot koronItems[3];
    u8 sexDataId;
    u8 homePlace;
    u8 padA82[2];
    s32 pokedoru;
    s32 pokecoupon;
    s32 pokecouponAll;
    u8 badge01Flag;
    u8 badge02Flag;
    u8 badge03Flag;
    u8 badge04Flag;
    u8 badge05Flag;
    u8 badge06Flag;
    u8 badge07Flag;
    u8 badge08Flag;
    u8 hizukiFlag;
    u8 padA99;
    HeroItemSlot hizukiItems[0xA];
    u8 hizukiName[0x56];
} HeroSaveData;

/* 0x80129280 | 0x104 */
u32 savedataGetStatus(u8* arg1, u16 arg2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* savedataBiosGetNowSavedataPtr(u8* ptr);
#endif
    extern u32 savedataBiosGetGamedataPtr(u8* ptr);
    extern u32 savedataBiosGetHeroPtr(u8* ptr);
    extern u32 savedataBiosGetPcboxPtr(u8* ptr);
    extern u32 savedataBiosGetFlagLevel0Ptr(u8* ptr);
    extern u32 savedataBiosGetFlagLevel1Ptr(u8* ptr);
    extern u32 savedataBiosGetFlagLevel2Ptr(u8* ptr);
    extern u32 savedataBiosGetMailboxPtr(u8* ptr);
    extern u32 savedataBiosGetSodateyaPtr(u8* ptr);
    extern u32 savedataBiosGetMemoDataPtr(u8* ptr);
    extern u32 savedataBiosGetCardEPtr(u8* ptr);
    extern u32 savedataBiosGetE3E8Ptr(u8* ptr);
    extern u32 savedataBiosGetDarkpokemonPtr(u8* ptr);
    extern u32 savedataBiosGetExRibbonPtr(u8* ptr);

    if ((u32)arg2 >= 0x11) { return 0; }
    if (arg1 == NULL) {
        arg1 = PC_PTR(savedataBiosGetNowSavedataPtr(arg1));
        if (arg1 == NULL) { return 0; }
    }
    switch (arg2) {
    case 0:
        return PC_PTR32(arg1);
    case 1:
        return savedataBiosGetGamedataPtr(arg1);
    case 2:
        return savedataBiosGetHeroPtr(arg1);
    case 3:
        return savedataBiosGetPcboxPtr(arg1);
    case 4:
        return savedataBiosGetFlagLevel0Ptr(arg1);
    case 5:
        return savedataBiosGetFlagLevel1Ptr(arg1);
    case 6:
        return savedataBiosGetFlagLevel2Ptr(arg1);
    case 7:
        return 8;
    case 8:
        return 0x20;
    case 9:
        return 0x180;
    case 10:
        return savedataBiosGetMailboxPtr(arg1);
    case 11:
        return savedataBiosGetSodateyaPtr(arg1);
    case 12:
        return savedataBiosGetMemoDataPtr(arg1);
    case 13:
        return savedataBiosGetCardEPtr(arg1);
    case 14:
        return savedataBiosGetE3E8Ptr(arg1);
    case 15:
        return savedataBiosGetDarkpokemonPtr(arg1);
    case 16:
        return savedataBiosGetExRibbonPtr(arg1);
    default:
        return 0;
    }
}

/* 0x80129384 | 0x78 */
void heroDecPokecoupon(u8* ptr, s32 offset) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8* ptr, u32 selector, u32 index);
#endif
    extern void heroSetStatus(u8* ptr, u32 selector, u32 value);

    heroSetStatus(ptr, 0xD, heroGetStatus(ptr, 0xD, 0) - offset);
    if (offset <= 0) {
        heroSetStatus(ptr, 0xE, heroGetStatus(ptr, 0xE, 0) - offset);
    }
}

/* 0x801293FC | 0x78 */
void heroAddPokecoupon(u8* ptr, s32 offset) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8* ptr, u32 selector, u32 index);
#endif
    extern void heroSetStatus(u8* ptr, u32 selector, u32 value);
    u32 value;

    value = heroGetStatus(ptr, 0xD, 0);
    value += offset;
    heroSetStatus(ptr, 0xD, value);
    if (offset >= 0) {
        value = heroGetStatus(ptr, 0xE, 0);
        value += offset;
        heroSetStatus(ptr, 0xE, value);
    }
}

/* 0x80129474 | 0x50 */
void heroDecPokedoru(u8* ptr, u32 offset) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8* ptr, u32 selector, u32 index);
#endif
    extern void heroSetStatus(u8* ptr, u32 selector, u32 value);

#ifdef TARGET_PC
    /* OrreHook_MoneyTaken (infinite money; pc/orre_hooks.h): how much of the amount is really taken. Every place money
     * goes down calls this function (shops, a lost battle, scripts), so one hook covers them all */
    {
        extern u32 (*OrreHook_MoneyTaken)(u32 amount);
        if (OrreHook_MoneyTaken != NULL) {
            offset = OrreHook_MoneyTaken(offset);
        }
    }
#endif
    heroSetStatus(ptr, 0xC, heroGetStatus(ptr, 0xC, 0) - offset);
}

/* 0x801294C4 | 0x50 */
void heroAddPokedoru(u8* ptr, u32 offset) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8* ptr, u32 selector, u32 index);
#endif
    extern void heroSetStatus(u8* ptr, u32 selector, u32 value);
    u32 value;

    value = heroGetStatus(ptr, 0xC, 0);
    value += offset;
    heroSetStatus(ptr, 0xC, value);
}

static inline void* heroGetHizukiItems(
    u8* ptr, u16* out_count, u16* out_max, u8* out_flag1, u8* out_flag2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroGetStatus(u8* ptr, u32 selector, u32 index);
#endif

    if (out_count != NULL) { *out_count = 10; }
    if (out_max != NULL) { *out_max = 1; }
    if (out_flag1 != NULL) { *out_flag1 = 0; }
    if (out_flag2 != NULL) { *out_flag2 = 1; }
    return PC_PTR(heroGetStatus(ptr, 10, 0));
}

/* 0x80129514 | 0x88 */
void fn_80129514(u8* ptr, s32 arg2, s32 arg3) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80140A9C(u8* first, u8* second);
#endif
    u16 count;
    u8* items;

    items = heroGetHizukiItems(ptr, &count, NULL, NULL, NULL);
    if (items == NULL) { return; }
    if ((u16)arg2 >= count) { return; }
    if ((u16)arg3 >= count) { return; }
    fn_80140A9C(items + (u16)arg2 * 4, items + (u16)arg3 * 4);
}

/* 0x8012959C | 0xB4 */
s32 fn_8012959C(u8* ptr, u32 arg2, u32 arg3, u32 arg4) {
    extern s32 fn_80140ACC(
        void* items, u16 count, u32 item, u32 amount, u32 index, u16 max,
        u8 flag);
    u16 count = 0;
    u16 max = 0;
    u8 flag;
    void* items;

    items = heroGetHizukiItems(ptr, &count, &max, &flag, NULL);
    if (items == NULL) { return -1; }
    return fn_80140ACC(items, count, arg2, arg3, arg4, max, flag);
}

/* 0x80129650 | 0xC8 */
s32 fn_80129650(u8* ptr, u32 arg2, u32 arg3, u32 arg4) {
    extern s32 fn_80141308(
        void* items, u16 count, u32 item, u32 amount, u32 index, u16 max,
        u8 flag1, u8 flag2);
    u16 count = 0;
    u16 max = 0;
    u8 flag1;
    u8 flag2;
    void* items;

    items = heroGetHizukiItems(ptr, &count, &max, &flag1, &flag2);
    if (items == NULL) { return -1; }
    return fn_80141308(
        items, count, arg2, arg3, arg4, max, flag1, flag2);
}

/* 0x80129718 | 0xC0 */
u32 fn_80129718(u8* ptr, u32 arg2) {
    extern s32 fn_80142368(
        void* items, u16 count, u32 item, u32 mode, u16 max);
    u16 count = 0;
    u16 max = 0;
    void* items;

    items = heroGetHizukiItems(ptr, &count, &max, NULL, NULL);
    if (items == NULL) { return 0; }
    if ((u32)fn_80142368(items, count, arg2, 1, max) != 0) { return 1; }
    return fn_80142368(items, count, arg2, 2, max) != 0;
}

/* 0x801297D8 | 0x68 */
u32 heroHizukiItemGetItemAryPtr(
    u8* ptr, u16* out_count, u16* out_max, u8* out_flag1, u8* out_flag2) {
    return PC_PTR32(heroGetHizukiItems(
        ptr, out_count, out_max, out_flag1, out_flag2));
}

/* 0x80129840 | 0x78 */
void heroCheckSetMonohiroiAllTemotiPokemon(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8* ptr, u32 selector, u32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(u32 pokemon);
#endif
    extern void pokemonCheckSetMonohiroi(u32 pokemon);
    u32 pokemon;
    u32 i;

    for (i = 0; (u16)i < 6; i++) {
        pokemon = heroGetStatus(ptr, 3, i);
        if (pokemonCheckValid(PC_PTR(pokemon))) {
            pokemonCheckSetMonohiroi(pokemon);
        }
    }
}

/* 0x801298B8 | 0x90 */
s32 heroItemCheckAddItemDataId(u8* ptr, u32 item_id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr(
        u8*, u8, u16*, u16*, u32, u8*);
#endif
    extern s32 fn_80140588(void*, u16, u32, u16, u8);
    u16 count = 0;
    u16 max = 0;
    u8 flag;
    void* items;

    items = heroItemGetItemKindToItemAryPtr(
        ptr, (u8)itemGetStatus(0, item_id, 2, 0), &count, &max, 0, &flag);
    if (items == NULL) { return -1; }
    return fn_80140588(items, count, item_id, max, flag);
}

/* 0x80129948 | 0x80 */
void fn_80129948(
    u8* ptr, u32 kind, u32 first, u32 second, u32 arg5, u32 arg6,
    u32 arg7) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr(
        u8*, u32, u16*, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80140A9C(u8*, u8*);
#endif
    u16 count;
    u8* items;

    items = (u8*)heroItemGetItemKindToItemAryPtr(
        ptr, kind, &count, 0, 0, 0);
    if (items == NULL) { return; }
    if ((u16)first >= count) { return; }
    if ((u16)second >= count) { return; }
    fn_80140A9C(items + (u16)first * 4, items + (u16)second * 4);
}

/* 0x801299C8 | 0xB0 */
s32 heroItemDecItemDataId(
    u8* ptr, u32 item_id, u32 amount, u32 index) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr(
        u8*, u8, u16*, u16*, u8*, u32);
#endif
    extern s32 fn_80140ACC(
        void*, u16, u32, u32, u32, u16, u8);
    u16 count = 0;
    u16 max = 0;
    u8 flag;
    u8 kind;
    void* items;

    kind = (u8)itemGetStatus(0, item_id, 2, 0);
    items = heroItemGetItemKindToItemAryPtr(
        ptr, kind, &count, &max, &flag, 0);
    if (items == NULL) { return -1; }
    return fn_80140ACC(items, count, item_id, amount, index, max, flag);
}

/* 0x80129A78 | 0xB4 */
s32 heroItemAddItemDataId(
    u8* ptr, u32 item_id, u32 amount, u32 index) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr(
        u8*, u8, u16*, u16*, u8*, u8*);
#endif
    extern s32 fn_80141308(
        void*, u16, u32, u32, u32, u16, u8, u8);
    u16 count = 0;
    u16 max = 0;
    u8 flag1;
    u8 flag2;
    u8 kind;
    void* items;

    kind = (u8)itemGetStatus(0, item_id, 2, 0);
    items = heroItemGetItemKindToItemAryPtr(
        ptr, kind, &count, &max, &flag1, &flag2);
    if (items == NULL) { return -1; }
    return fn_80141308(
        items, count, item_id, amount, index, max, flag1, flag2);
}

/* 0x80129B2C | 0x9C */
u32 heroItemCheckHaveItemDataId(u8* ptr, u32 item_id) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroItemGetItemKindToItemAryPtr(
        u8*, u8, u16*, u16*, u32, u32);
#endif
    extern s32 fn_80142368(void*, u16, u32, u32, u16);
    u16 count = 0;
    u16 max = 0;
    void* items;

    items = heroItemGetItemKindToItemAryPtr(
        ptr, (u8)itemGetStatus(0, item_id, 2, 0), &count, &max, 0, 0);
    if (items == NULL) { return 0; }
    return fn_80142368(items, count, item_id, 0, max) != 0;
}

/* 0x80129BC8 | 0x19C */
void* heroItemGetItemKindToItemAryPtr(
    HeroSaveData* hero, u8 kind, u16* out_count, u16* out_max,
    u8* out_flag1, u8* out_flag2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroGetStatus(
        HeroSaveData* hero, u32 selector, u32 index);
#endif
    void* item_array = NULL;
    u16 count = 0;
    u16 max = 0;
    u8 flag1 = 0;
    u8 flag2;

    switch (kind) {
    case 1:
        item_array = PC_PTR(heroGetStatus(hero, 6, 0));
        count = 0x10;
        max = 0x63;
        flag1 = 0;
        flag2 = 1;
        break;
    case 2:
        item_array = PC_PTR(heroGetStatus(hero, 4, 0));
        count = 0x14;
        max = 0x63;
        flag1 = 0;
        flag2 = 1;
        break;
    case 3:
        item_array = PC_PTR(heroGetStatus(hero, 8, 0));
        count = 0x2E;
        max = 0x3E7;
        flag1 = 1;
        flag2 = 1;
        break;
    case 4:
        item_array = PC_PTR(heroGetStatus(hero, 7, 0));
        count = 0x40;
        max = 0x63;
        flag1 = 1;
        flag2 = 0;
        break;
    case 5:
        item_array = PC_PTR(heroGetStatus(hero, 5, 0));
        count = 0x2B;
        max = 0x63;
        flag1 = 0;
        flag2 = 0;
        break;
    case 6:
        item_array = PC_PTR(heroGetStatus(hero, 9, 0));
        count = 3;
        max = 0x63;
        flag1 = 1;
        flag2 = 0;
        break;
    }
    if (out_count != NULL) { *out_count = count; }
    if (out_max != NULL) { *out_max = max; }
    if (out_flag1 != NULL) { *out_flag1 = flag1; }
    if (out_flag2 != NULL) { *out_flag2 = flag2; }
    return item_array;
}

/* 0x80129D64 | 0xBC */
u32 heroIsMinePokemon(u8* ptr, u8* pokemon) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u8*, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GScharCmp(u32, u32);
#endif
    u32 trainer_id;
    u32 trainer_name;
    u32 pokemon_trainer_id;
    u32 pokemon_trainer_name;

    trainer_id = heroGetStatus(ptr, 2, 0);
    trainer_name = heroGetStatus(ptr, 1, 0);
    pokemon_trainer_id = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_OT_ID, 0);
    pokemon_trainer_name = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_OT_NAME, 0);
    if (trainer_id != pokemon_trainer_id) { return 0; }
    return GScharCmp(PC_PTR(trainer_name), PC_PTR(pokemon_trainer_name)) == 0;
}

static inline u8 heroAddPokemonToParty(u8* ptr, void* pokemon) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroGetStatus(u8*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosCopy(void*, void*);
#endif
    void* slot;
    u8 slot_index;

    if (pokemon == NULL) { return 6; }
    for (slot_index = 0; slot_index < 6; slot_index++) {
        slot = PC_PTR(heroGetStatus(ptr, 3, slot_index));
        if ((u8)pokemonCheckValid(slot) != 1) {
            pokemonBiosCopy(slot, pokemon);
            return slot_index;
        }
    }
    return 6;
}

/* 0x80129E20 | 0x100 */
s32 heroGetPokemon(u8* ptr, void* pokemon, u8 allow_pcbox) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosCopy(void*, void*);
#endif
    extern u32 pcboxAddPokemon(u32, void*, s32);
    u8 copy[0x138];
    u8 slot_index;
    s16 result;

    if (pokemon == NULL) { return 6; }
    pokemonBiosCopy(copy, pokemon);
    slot_index = heroAddPokemonToParty(ptr, copy);
    result = (s16)(u8)slot_index;
    if ((u8)slot_index >= 6) {
        if ((u8)allow_pcbox == 0) { return -2; }
        return pcboxAddPokemon(0, copy, -1) == 1 ? -1 : -2;
    }
    return result;
}

/* 0x80129F20 | 0x16C */
s32 heroCatchPokemon(
    u8* ptr, u8* pokemon, u32 arg3, u32 arg4, u8 allow_pcbox) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(u8*, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosCopy(void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonSetCatchStatus(
        u8*, u32, u32, u32, u32, u32, u32);
#endif
    extern u32 pcboxAddPokemon(u32, void*, s32);
    u8 copy[0x138];
    u8 pokemon_field_7a;
    u8 catch_status;
    u8 slot_index;
    u32 trainer_id;
    u32 trainer_name;
    s16 result;

    if (pokemon == NULL) { return 6; }
    pokemon_field_7a = (u8)pokemonGetStatus(pokemon, 0, POKEMON_STATUS_LEVEL, 0);
    catch_status = (u8)heroGetStatus(ptr, 0xB, 0);
    trainer_id = heroGetStatus(ptr, 2, 0);
    trainer_name = heroGetStatus(ptr, 1, 0);
    pokemonBiosCopy(copy, pokemon);
    pokemonSetCatchStatus(
        copy, arg3, pokemon_field_7a, arg4, catch_status, trainer_id,
        trainer_name);

    slot_index = heroAddPokemonToParty(ptr, copy);
    result = (s16)(u8)slot_index;
    if (slot_index >= 6) {
        if ((u8)allow_pcbox == 0) { return -2; }
        return pcboxAddPokemon(0, copy, -1) == 1 ? -1 : -2;
    }
    return result;
}

/* 0x8012A08C | 0xA4 */
u32 heroAddPokemon(u8* ptr, void* pokemon) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonCheckValid(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosCopy(u32, void*);
#endif
    u32 slot;
    u32 i;

    if (pokemon == NULL) { return 6; }
    i = 0;
    while ((u8)i < 6) {
        slot = heroGetStatus(ptr, 3, i & 0xFF);
        if ((u8)pokemonCheckValid(PC_PTR(slot)) != 1) {
            pokemonBiosCopy(PC_PTR(slot), pokemon);
            return i;
        }
        i++;
    }
    return 6;
}

/* 0x8012A130 | 0x74 */
u32 heroCheckValid(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus(u8*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 GScharCmp(u32, u16*);
#endif
    u16 terminator = 0;

    if (GScharCmp(PC_PTR(heroGetStatus(ptr, 1, 0)), &terminator) == 0) { return 0; }
    return heroGetStatus(ptr, 0xB, 0) != 2;
}

/* 0x8012A1A4 | 0xA4 */
void heroCreate(u8* ptr, u32 trainer_id, u8 sex) {
    extern void heroInit(u8*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSrand(void);
#endif
    extern void heroSetStatus(u8*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32);
#endif
    u32 random_low;
    u32 random_high;

    heroInit(ptr);
    random_low = GSrand() & 0xFFFF;
    random_high = GSrand() << 16;
    heroSetStatus(ptr, 2, random_high | random_low);
    heroSetStatus(ptr, 1, trainer_id);
    heroSetStatus(ptr, 0xB, sex);
    heroSetStatus(ptr, 0x17, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0xFA2)));
}

/* 0x8012A248 | 0x208 */
void heroInit(u8* ptr) {
    extern void heroSetStatus(u8*, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* heroGetStatus(u8*, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitAry(u8*, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_80142A88(u8*, u32);
#endif
    u16 empty_name = 0;

    heroSetStatus(ptr, 1, PC_PTR32(&empty_name));
    heroSetStatus(ptr, 2, 0);
    pokemonInitAry(PC_PTR(heroGetStatus(ptr, 3, 0)), 6);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 4, 0)), 0x14);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 5, 0)), 0x2B);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 6, 0)), 0x10);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 7, 0)), 0x40);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 8, 0)), 0x2E);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 9, 0)), 3);
    heroSetStatus(ptr, 0xB, 2);
    heroSetStatus(ptr, 0xC, 0);
    heroSetStatus(ptr, 0xD, 0);
    heroSetStatus(ptr, 0xE, 0);
    heroSetStatus(ptr, 0xF, 1);
    heroSetStatus(ptr, 0x10, 1);
    heroSetStatus(ptr, 0x11, 1);
    heroSetStatus(ptr, 0x12, 1);
    heroSetStatus(ptr, 0x13, 1);
    heroSetStatus(ptr, 0x14, 1);
    heroSetStatus(ptr, 0x15, 1);
    heroSetStatus(ptr, 0x16, 1);
    heroSetStatus(ptr, 0x17, PC_PTR32(&empty_name));
    heroSetStatus(ptr, 0x18, 0);
    fn_80142A88(PC_PTR(heroGetStatus(ptr, 10, 0)), 10);
    heroSetStatus(ptr, 0x19, 0);
}
