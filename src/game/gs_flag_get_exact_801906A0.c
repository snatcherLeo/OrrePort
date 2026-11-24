/**
 * @file gs_flag_get_exact_801906A0.c
 * @brief GS flag getter (XD: GSflagGet), 0x801906A0 - 0x8019075C.
 *
 * Function-boundary carve of the GS flag module (see gs_range_8018FE30.c
 * for its neighbours). No local data: the definition and state tables
 * (.sdata), the bit-mask table (XD: _flagAND) and the error string are
 * extern.
 *
 * fn_801906A0 is Pokemon XD's GSflagGet with its local _flagGet inlined.
 * XD keeps both out of line: GSflagGet (0x801A0364, 0x40) looks up the
 * flag's state buffer from the definition's type bits and calls
 * _flagGet(buffer, definitions, flagId) (0x801A03E8, 0xA8, scope:local),
 * which logs "ERROR[GSflagGet]:Initialization has not finished." for a
 * missing buffer and otherwise extracts the flag's bits. The helper below
 * is named after it and makes the same call with the same argument
 * sources (same-engine sister-title clause, docs/CAMPAIGN_OPERATIONS.md).
 * References: TeamOrre/xd-decomp config/GXXE01/symbols.txt @ 4989794e,
 * trevor403/xd-asm @ b1087f18 (func_FUN_801a0364.s, func_FUN_801a03e8.s).
 * Colosseum's definitions are 8 bytes (XD's are 6).
 *
 * Written in place, the definition-table load and the word-offset temp
 * outrank the type byte, buffer and bit width in MWCC's colouring (97.2%);
 * as _flagGet's parameters and locals they take retail's registers.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT: common_rel tables are big-endian on the PC */

typedef struct FlagStateEntry {
    u32 wordCount;
    PTR32_FIELD(u32) buffer; /* 8-byte entries on the PC too (pc/ptr32.h): the array is allocated in GameCube bytes */
} FlagStateEntry;

typedef struct DISC_STRUCT FlagDefinition { /* the common_rel flag-definition table: big-endian view on the PC */
    u8 typeAndWidth;
    u8 initialValue;
    u8 itemSwap;
    u8 event;
    u16 bitPosition;
    s16 next;
} FlagDefinition;

extern FlagDefinition* lbl_80478F9C; /* flag definitions */
extern FlagStateEntry* lbl_80478EEC; /* per-type state buffers */
extern u32 lbl_8036C568[];           /* low-bit masks (XD: _flagAND) */
extern const char lbl_80274284[];    /* "ERROR[GSflagGet]:Initialization has not finished.\n" */
extern void GSlogWrite(const char* format, ...);

static inline u32 _flagGet(u32* buffer, FlagDefinition* definitions, s32 flagId)
{
    u32 bitOffset;
    u32 wordIndex;
    u32 bitPosition;
    u32 bitWidth;

    if (buffer == NULL) {
        GSlogWrite(lbl_80274284);
        return 0;
    }
    bitOffset = definitions[flagId].bitPosition;
    bitWidth = definitions[flagId].typeAndWidth & 0x3F;
    wordIndex = bitOffset >> 5;
    bitPosition = bitOffset & 0x1F;
    if (bitWidth > 1) {
        u32 low = buffer[wordIndex];
        u32 high = buffer[wordIndex + 1];

        low >>= bitPosition;
#ifdef TARGET_PC
        /* port-gaps 43: a value starting at bit 0 shifts the next word by 32. PowerPC's slw gives 0 there; x86 masks
         * the count to 0 and kept the whole next word, so such values read OR'd with it (P27: a Shadow Pokémon's
         * state flag, 4 on both sides, read 20 on the PC and a mail's "raise to 5" skipped) */
        high = bitPosition != 0 ? high << (32 - bitPosition) : 0;
#else
        high <<= 32 - bitPosition;
#endif
        return (high | low) & lbl_8036C568[bitWidth];
    }
    return (buffer[wordIndex] >> bitPosition) & 1;
}

u32 std082_flagGetValue(s32 flagId)
{
    return _flagGet(P32(lbl_80478EEC[(lbl_80478F9C[flagId].typeAndWidth & 0xC0) >> 6].buffer),
                    lbl_80478F9C, flagId);
}
