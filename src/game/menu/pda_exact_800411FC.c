/**
 * @file pda_exact_800411FC.c
 * @brief PDA people-screen species panel, 0x800411FC - 0x80041B5C:
 *        fn_800411FC draws the highlighted entry's name, abilities, height,
 *        weight and type. Copied from pda_range_80037158.c.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_S16 / DISC_EL_S16: the menu sprite table lbl_802EF0A8 stays big-endian on the PC */

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
} PdaSceneWork;

typedef struct PdaEvent {
    u8 pad00[0x6];
    s16 messageId;
} PdaEvent;

extern PdaSceneWork lbl_803A6818;
extern u8 lbl_802EF0A8[];
extern u32 lbl_8047A4E0;
extern u16* lbl_8047A4E4;
extern u32 lbl_8047A4F0;
extern u16 lbl_803A67E8[];

#ifndef TARGET_PC /* PC: canonical prototypes in pc/prototypes.h */
extern u32 gamedataGetStatus(s32 a, s32 b);
extern void pokemonCreate(u32 work, u16 species, s32 level, u32 trainer);
extern u32 memoDataGetPokemonRndFromID(s32 a, u32 id);
extern u32 memoDataGetPokemonTrainerRndFromID(s32 a, u32 id);
extern void pokemonBiosSetRnd(u32 work, u32 rnd);
extern void pokemonBiosSetCatchTrainerRnd(u32 work, u32 rnd);
extern u16 pokemonBiosGetPokemonDataId(u32 work);
extern void* pokemonDataBiosGetPtr(u16 id);
extern void* pokemonDataBiosGetName(void* data);
extern u16 pokemonDataBiosGetWeight(void* data);
extern u16 pokemonDataBiosGetHeight(void* data);
extern u8 pokemonDataBiosGetTokuseiDataId(void* data, s32 slot);
extern void* pokemonTokuseiDataBiosGetPtr(u32 id);
extern void* pokemonTokuseiDataBiosGetName(void* data);
extern u32 pokemonDataBiosGetTypeName(void* data);
extern u32 GSmsgGetGSchar(u32 msg);
extern void msgctrlSetValue(s32 id, u32 value);
extern void GSmsgPrintColor(s32 arg0, s32 arg1, s32 arg2, void* data);
#else /* PC: these six have no canonical prototype; declared with their definitions' types */
extern u16 pokemonDataBiosGetWeight(u8* ptr);
extern u16 pokemonDataBiosGetHeight(u8* ptr);
extern u8 pokemonDataBiosGetTokuseiDataId(u8* ptr, u16 idx);
extern void* pokemonTokuseiDataBiosGetPtr(u16 idx);
extern u32 pokemonTokuseiDataBiosGetName(u8* ptr);
extern u32 pokemonDataBiosGetTypeName(u8* ptr);
#endif

/* Write both seeds into the work Pokemon. MWCC evaluates the arguments right to
 * left, so a caller passing both lookups runs the Pokemon seed lookup first. */
static inline void pda_800411FC_Seeds(u32 work, u32 trainerRnd, u32 rnd)
{
    pokemonBiosSetRnd(PC_PTR(work), rnd);
    pokemonBiosSetCatchTrainerRnd(PC_PTR(work), trainerRnd);
}

/* fn_800411FC's pdaLoadPokemon: rebuild the work Pokemon for memo entry `index`. */
static inline u32 pda_800411FC_Load(s32 index)
{
    u32 work = lbl_8047A4E0;
    u32 species;

    if (work != 0) {
        species = lbl_8047A4E4[index];
        if (species >= 0x8000) {
            species = species & 0x3fff;
        }
        pokemonCreate(PC_PTR(work), (u16)species, 10, PC_PTR(gamedataGetStatus(0, 1)));
        pda_800411FC_Seeds(work, memoDataGetPokemonTrainerRndFromID(0, species),
                           memoDataGetPokemonRndFromID(0, species));
        return lbl_8047A4E0;
    }
    return 0;
}

/* Whether the highlighted memo entry has actually been seen. */
static inline u8 pdaEntrySeen(void)
{
    return (lbl_8047A4E4[lbl_803A6818.currentIndex] & 0x8000) ? 0 : 1;
}

/* Species-name message for the highlighted entry, 0 when unavailable. */
static inline u32 pdaCurrentNameMsg(s32 index)
{
    u32 work;

    if (lbl_8047A4E0 != 0) {
        work = pda_800411FC_Load(index);
        if (work != 0) {
            return PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(pokemonDataBiosGetName(
                pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(PC_PTR(work)))))));
        }
        return 0;
    }
    return 0;
}

/* Ability ID in the given slot of the highlighted entry. */
static inline u16 pda_800411FC_Ability(s32 slot)
{
    return pokemonDataBiosGetTokuseiDataId(
        pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(
            PC_PTR(pda_800411FC_Load(lbl_803A6818.currentIndex)))),
        slot);
}

/* Type-name message of the highlighted entry, 0 when unavailable. */
static inline u32 pda_800411FC_TypeName(void)
{
    if (lbl_8047A4E0 != 0) {
        return pokemonDataBiosGetTypeName(pokemonDataBiosGetPtr(
            pokemonBiosGetPokemonDataId(PC_PTR(pda_800411FC_Load(lbl_803A6818.currentIndex)))));
    }
    return 0;
}

/* Height of the highlighted entry, 0 when unavailable. */
static inline u16 pdaCurrentHeight(void)
{
    u32 work;
    void* data;

    if (lbl_8047A4E0 != 0) {
        work = pda_800411FC_Load(lbl_803A6818.currentIndex);
        if (work != 0) {
            data = pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(PC_PTR(work)));
            if (data != NULL) {
                return pokemonDataBiosGetHeight(data);
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

/* Weight of the highlighted entry, 0 when unavailable. */
static inline u16 pdaCurrentWeight(void)
{
    u32 work;
    void* data;

    if (lbl_8047A4E0 != 0) {
        work = pda_800411FC_Load(lbl_803A6818.currentIndex);
        if (work != 0) {
            data = pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(PC_PTR(work)));
            if (data != NULL) {
                return pokemonDataBiosGetWeight(data);
            }
            return 0;
        }
        return 0;
    }
    return 0;
}

/* Species panel: name, abilities, height, weight and type for the entry. */
void fn_800411FC(PdaSprite* alphaSprite, PdaEvent* event)
{
#ifndef TARGET_PC /* PC: canonical prototypes in pc/prototypes.h */
    extern void GSmsgPrintRight(s32 x, s32 y, s16 w, s16 h, void* color, s32 msg);
#endif
#ifndef TARGET_PC /* PC: canonical prototypes in pc/prototypes.h */
    extern void GSmsgPrintRect(s32 x, s32 y, s16 w, s16 h, void* color, s32 msg);
#endif
    PdaSprite* sprite = (PdaSprite*)event;
    u8* layout;
    u32 name;
    u16 tokusei0;
    u16 tokusei1;
    u32 height;
    u32 weight;
    u32 digit;
    s32 i;
    s32 digits;
    u32 msg;
    s16* pos;

    name = pdaCurrentNameMsg(lbl_803A6818.currentIndex);
    if (name == 0) {
        name = PC_PTR32_IMPLICIT(GSmsgGetGSchar(1));
    }
    msgctrlSetValue(0x37, name);
    GSmsgPrintColor(0, 0, alphaSprite->alphaByte | -0x100, PC_PTR32_IMPLICIT((void*)0xce));
    layout = lbl_802EF0A8;

    if (pdaEntrySeen() != 0) {
        tokusei0 = pda_800411FC_Ability(0);
        msg = PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(pokemonTokuseiDataBiosGetName(
            pokemonTokuseiDataBiosGetPtr(tokusei0)))));
        msgctrlSetValue(0x37, msg);
        GSmsgPrintColor(DISC_S16(layout + 0x5a5a) - sprite->field_50 + 4,
                    DISC_S16(layout + 0x5a5c) - sprite->field_52,
                    alphaSprite->alphaByte | -0x100, PC_PTR32_IMPLICIT((void*)0xe7));
    } else {
        GSmsgPrintColor(DISC_S16(layout + 0x5a5a) - sprite->field_50 + 4,
                    DISC_S16(layout + 0x5a5c) - sprite->field_52,
                    alphaSprite->alphaByte | -0x100, PC_PTR32_IMPLICIT((void*)0x3721));
    }

    if (pdaEntrySeen() != 0) {
        tokusei1 = pda_800411FC_Ability(1);
        if (tokusei1 != 0 && tokusei1 != tokusei0) {
            pos = (s16*)(lbl_802EF0A8 + 0x20b42);
            msgctrlSetValue(0x37,
                            PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(pokemonTokuseiDataBiosGetName(
                                pokemonTokuseiDataBiosGetPtr(tokusei1))))));
            GSmsgPrintColor(DISC_EL_S16(pos, 0) - sprite->field_50 + 4,
                        DISC_EL_S16(pos, 1) - sprite->field_52,
                        alphaSprite->alphaByte | -0x100, PC_PTR32_IMPLICIT((void*)0xe7));
        }
    } else {
        GSmsgPrintColor(DISC_S16(layout + 0x5a5a) - sprite->field_50 + 4,
                    DISC_S16(layout + 0x5a5c) - sprite->field_52,
                    alphaSprite->alphaByte | -0x100, PC_PTR32_IMPLICIT((void*)0x3721));
    }

    layout = lbl_802EF0A8;
    if (pdaEntrySeen() != 0) {
        height = pdaCurrentHeight();
        i = 10;
#ifdef TARGET_PC
/* PC (port-gaps 59, the reference playthrough P02 note 5): the text reader takes this buffer as a GS string in the disc's big-endian
 * order, like the names from the disc; stored natively, each character came out byte-swapped and drew as a box */
#define PDA_HEIGHT_PUT(v) DISC_STORE16(u16, &lbl_803A67E8[--i], (v))
#else
#define PDA_HEIGHT_PUT(v) (lbl_803A67E8[--i] = (v))
#endif
        PDA_HEIGHT_PUT(0);
        PDA_HEIGHT_PUT(0x27);
        PDA_HEIGHT_PUT(0x27);
        digits = 3;
        for (;;) {
            digit = height % 10;
            height = height / 10;
            if (digits <= 0) {
                if (digit == 0) {
                    break;
                }
            } else {
                digits--;
            }
            PDA_HEIGHT_PUT(digit + 0x30);
            if (digits == 1) {
                PDA_HEIGHT_PUT(0x27);
            }
        }
#undef PDA_HEIGHT_PUT
        msgctrlSetValue(0x37, PC_PTR32(&lbl_803A67E8[i]));
        GSmsgPrintRight(lbl_8047A4F0 +
                        (DISC_S16(layout + 0x5a76) - sprite->field_50),
                    DISC_S16(layout + 0x5a78) - sprite->field_52,
                    DISC_S16(layout + 0x5a7a),
                    DISC_S16(layout + 0x5a7c),
                    PC_PTR32_IMPLICIT((void*)PC_PTR((alphaSprite->alphaByte | -0x100))), 0xcf);
    } else {
        GSmsgPrintRect(lbl_8047A4F0 +
                        (DISC_S16(layout + 0x5a76) - sprite->field_50),
                    DISC_S16(layout + 0x5a78) - sprite->field_52,
                    DISC_S16(layout + 0x5a7a),
                    DISC_S16(layout + 0x5a7c),
                    PC_PTR32_IMPLICIT((void*)PC_PTR((alphaSprite->alphaByte | -0x100))), 0x371b);
    }

    layout = lbl_802EF0A8;
    if (pdaEntrySeen() != 0) {
        weight = pdaCurrentWeight();
        msgctrlSetValue(0x34, weight / 10);
        msgctrlSetValue(0x35, weight % 10);
        GSmsgPrintRect(lbl_8047A4F0 +
                        (DISC_S16(layout + 0x5a92) - sprite->field_50),
                    DISC_S16(layout + 0x5a94) - sprite->field_52,
                    DISC_S16(layout + 0x5a96),
                    DISC_S16(layout + 0x5a98),
                    PC_PTR32_IMPLICIT((void*)PC_PTR((alphaSprite->alphaByte | -0x100))), 0x189c);
    } else {
        GSmsgPrintRect(lbl_8047A4F0 +
                        (DISC_S16(layout + 0x5a92) - sprite->field_50),
                    DISC_S16(layout + 0x5a94) - sprite->field_52,
                    DISC_S16(layout + 0x5a96),
                    DISC_S16(layout + 0x5a98),
                    PC_PTR32_IMPLICIT((void*)PC_PTR((alphaSprite->alphaByte | -0x100))), 0x371a);
    }

    layout = lbl_802EF0A8;
    if (pdaEntrySeen() != 0) {
        msgctrlSetValue(0x31, pda_800411FC_TypeName());
        GSmsgPrintRect(lbl_8047A4F0 +
                        (DISC_S16(layout + 0x5a22) - sprite->field_50),
                    DISC_S16(layout + 0x5a24) - sprite->field_52,
                    DISC_S16(layout + 0x5a26),
                    DISC_S16(layout + 0x5a28),
                    PC_PTR32_IMPLICIT((void*)PC_PTR((alphaSprite->alphaByte | -0x100))), 0x371e);
    } else {
        msgctrlSetValue(0x31, 0x371c);
        GSmsgPrintRect(lbl_8047A4F0 +
                        (DISC_S16(layout + 0x5a22) - sprite->field_50),
                    DISC_S16(layout + 0x5a24) - sprite->field_52,
                    DISC_S16(layout + 0x5a26),
                    DISC_S16(layout + 0x5a28),
                    PC_PTR32_IMPLICIT((void*)PC_PTR((alphaSprite->alphaByte | -0x100))), 0x371e);
    }
}
