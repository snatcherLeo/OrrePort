/**
 * @file pda_exact_80044378.c
 * @brief PDA species-name list sprite callback, 0x80044378 - 0x80044630:
 *        fn_80044378 with the row helpers it inlines. Copied from
 *        pda_range_80037158.c.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC */

typedef struct PdaSprite {
    u8 pad00[0x4];
    s8 flags;
    u8 pad05;
    s16 eventId;
    u8 pad08[0x44];
    s32 messageId;
    s16 field_50;
    s16 field_52;
    s16 x;
    s16 y;
    u8 pad58[0xc];
    u8 colorR;
    u8 colorG;
    u8 colorB;
    u8 alpha;
    u8 pad68[0x8];
    f32 value;
    u8 pad74[0x17];
    u8 alphaByte;
    u8 pad8c[9];
    s8 selectedIndex;
} PdaSprite;

extern u8 lbl_802EF0A8[];
extern u8 lbl_803A6818[];
extern u32 lbl_8047A4E0;
extern u16* lbl_8047A4E4;
extern u16 lbl_8047A4E8;
extern f32 lbl_8047BCA0;
#ifndef TARGET_PC /* PC: canonical declaration in pc/prototypes.h */
extern const f32 lbl_8047BCF4;
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor(s32 arg0, s32 arg1, s32 arg2, void* data);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE38C(s32 x1, s32 y1, s32 x2, s32 y2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE35C(void);
#endif
extern void fn_800492CC(u8* context, PdaSprite* sprite);

static inline u32 pdaLoadRowPokemon(s32 offset)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 gamedataGetStatus(s32 a, s32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonCreate(u32 work, u16 species, s32 level, u32 trainer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 memoDataGetPokemonRndFromID(s32 a, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 memoDataGetPokemonTrainerRndFromID(s32 a, u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosSetRnd(u32 work, u32 rnd);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosSetCatchTrainerRnd(u32 work, u32 rnd);
#endif
    u32 work = lbl_8047A4E0;
    u32 rnd;
    u32 species;
    u32 trainerRnd;

    if (work != 0) {
        species = *(u16*)((u8*)lbl_8047A4E4 + offset);
        if (species >= 0x8000) {
            species = species & 0x3fff;
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

static inline u32 pdaRowNameMsg(s32 offset)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetPokemonDataId(u32 work);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonDataBiosGetPtr(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonDataBiosGetName(void* data);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 msg);
#endif
    u32 work = lbl_8047A4E0;

    if (work != 0) {
        work = pdaLoadRowPokemon(offset);
        if (work != 0) {
            return PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(pokemonDataBiosGetName(
                pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(work))))));
        }
        return 0;
    }
    return 0;
}

/* Draw the visible slice of the species-name column. */
static inline void pdaDrawNameRows(u8* context, s32 first, f32 y)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32 id, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 msg);
#endif
    s32 offset;
    s32 i;
    u32 name;

    for (i = first, offset = first; i < lbl_8047A4E8; i++) {
        if (i >= *(s32*)((u8*)&lbl_803A6818 + 8) - 1 &&
            i <= *(s32*)((u8*)&lbl_803A6818 + 0xC) + 1) {
            name = pdaRowNameMsg(offset);
            if (name == 0) {
                name = PC_PTR32_IMPLICIT(GSmsgGetGSchar(1));
            }
            msgctrlSetValue(0x37, name);
            GSmsgPrintColor(0, (s32)y - 2, (u32)context[0x8B] | -0x100LL,
                        PC_PTR32_IMPLICIT((void*)0xE7));
        }
        y += lbl_8047BCF4;
        offset += 2;
    }
}

void fn_80044378(u8* context, PdaSprite* sprite)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32 id, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 memoDataGetCount(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 msg);
#endif
    extern s8 fn_8004BDEC(void);
    extern s8 fn_8004BDFC(void);
    u8* entry;
    s16 messageId;

    /* retail reads the s16 eventId at +6 here, not messageId at +0x4c;
       0x12B2 is an eventId value -- it appears as a case in the switch below */
    switch (sprite->eventId) {
    case 0x12B2:
        context[0x8B] = lbl_8047BCA0 * *(f32*)((u8*)&lbl_803A6818 + 0x54);
        break;
    default:
        context[0x8B] = lbl_8047BCA0 * *(f32*)((u8*)&lbl_803A6818 + 0x4C);
        break;
    }
    messageId = sprite->eventId;
    switch (messageId) {
    case 0xD46:
    case 0x12B2:
    case 0x31D:
    case 0x31E:
        break;
    case 0x119B:
        if (fn_8004BDEC() == 1 && fn_8004BDFC() >= 1) {
            fn_800492CC(context, sprite);
        }
        break;
    case 0x76D:
        msgctrlSetValue(0x34, memoDataGetCount(0));
        break;
    default:
        entry = lbl_802EF0A8 + messageId * 0x1C;
        fn_800FE38C(
            DISC_S16(lbl_802EF0A8 + 0x5712) - DISC_S16(entry + 2),
            DISC_S16(lbl_802EF0A8 + 0x5714) - DISC_S16(entry + 4),
            DISC_S16(lbl_802EF0A8 + 0x5716),
            DISC_S16(lbl_802EF0A8 + 0x5718));
        pdaDrawNameRows(context, 0,
                        *(f32*)((u8*)&lbl_803A6818 + 0x30));
        fn_800FE35C();
        break;
    }
}
