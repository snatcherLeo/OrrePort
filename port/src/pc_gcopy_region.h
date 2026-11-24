/* pc_gcopy_region.h: the byte work of a GameCube copy for another region (Phase 3 step 3.5, step 9, 2026-11-19).
 *
 * Why its own file: "Make a GameCube copy..." can now write a copy for the US, European (PAL) or Japanese game (snatcherLeo's
 * four calls of 2026-11-19, the decision log). Every change it makes to the save's bytes lives here, in plain C over memory
 * with no SDL and no game code, so the hub (pc_gcopy.c, which has the files, the slot folders and the discs) and a test
 * program (test/gcopy_region_test.c, which proves it on real saves) run exactly the same code.
 *
 * What a copy for another region changes (docs/engine/regions.md, "What real saves from each region look like" and "The
 * save file's header per region"; docs/port/saves.md "A GameCube copy for another region"):
 * - the game code in the .gci's directory entry (bytes 0-3: GC6E / GC6P / GC6J);
 * - the card header (the 0x6000 bytes after the directory entry): title 0x20 bytes, comment 0x20 bytes, banner 0x1800,
 *   icon 0x2200, then a 32-bit big-endian word sum of those 0x3A40 bytes (0xE90 words), the rest zero. Verified
 *   2026-11-19: built from each region's own main.dol with the layout below, all four real saves' headers come out byte
 *   for byte (US, PAL English, PAL Spanish, Japanese);
 * - in every valid save slot: the save's region stamp (block +0x02 area, +0x03 language) and the two language bytes
 *   +0x2B and +0xB48 (the language for PAL, 0 for the US and Japan; verified on the four saves), then the slot sealed
 *   again (pc_save_cipher.h);
 * - optionally (snatcherLeo's call 3) Pokémon with no nickname: the nickname field holding their species' name in the save's
 *   language gets the name in the target's language.
 * Never touched (decided 2026-11-17 / 11-20): each Pokémon's own origin stamp, its "original nickname" field (+0x44:
 * what a GBA trade sends, encoded by that origin stamp's language, gba_misc.c), names the player typed.
 *
 * Plain C99 on bytes: the same on Windows, Linux and macOS (built and tested on Windows only yet).
 * XD (likely different): XD's save is another size, cipher and layout and its card header its own; the shape (a game
 * code per region, a stamp per save, a header built from the executable's own data) likely carries over.
 */
#ifndef PC_GCOPY_REGION_H
#define PC_GCOPY_REGION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_CARD_TITLE = 0x00,          /* the card header's title, 0x20 bytes (Latin-1 in the US and Europe, Shift-JIS in Japan) */
    PC_CARD_COMMENT = 0x20,        /* its comment, 0x20 bytes */
    PC_CARD_TEXT_SIZE = 0x20,
    PC_CARD_BANNER = 0x40,         /* 96 x 32 RGB5A3 */
    PC_CARD_BANNER_SIZE = 0x1800,
    PC_CARD_ICON = 0x1840,         /* 8 frames of 32 x 32 C8 and one palette */
    PC_CARD_ICON_SIZE = 0x2200,
    PC_CARD_SUM_AT = 0x3A40,       /* the word sum of the 0x3A40 bytes before it */
    PC_CARD_HEADER_SIZE = 0x6000,
    PC_GCOPY_SPECIES_MAX = 0x200   /* species ids the name tables hold (the game's table has 413: 0-412) */
};

typedef struct PcGcopyNames PcGcopyNames; /* one language's species names (below) */

/* One region's card-header pieces, read from that region's main.dol (pc_card_parts_from_dol). comment[n] is the
 * comment for PAL language n (2 English .. 6 Spanish); the US and Japanese games have one comment, kept in comment[0]. */
typedef struct PcCardParts {
    int ok;
    char region; /* 'E', 'P' or 'J' */
    uint8_t title[PC_CARD_TEXT_SIZE];
    uint8_t comment[7][PC_CARD_TEXT_SIZE];
    int has_comment[7];
    uint8_t banner[PC_CARD_BANNER_SIZE];
    uint8_t icon[PC_CARD_ICON_SIZE];
} PcCardParts;

/* Finds the card header's pieces in a region's main.dol (the whole file as on the disc). Returns 1 and fills out when
 * every piece is where that region's executable keeps it and the banner and icon check out (CRC-32), else 0 (another
 * revision or a modified disc: the caller falls back, pc_gcopy.c). Where they are: docs/engine/regions.md. */
int pc_card_parts_from_dol(char region, const uint8_t* dol, size_t size, PcCardParts* out);

/* The word sum the game checks (the 0xE90 big-endian words before PC_CARD_SUM_AT), and writing it. */
uint32_t pc_card_header_sum(const uint8_t* header);
void pc_card_header_seal(uint8_t* header);

/* A whole 0x6000-byte card header from a region's pieces, the comment for PAL language `language` (ignored for the
 * US and Japan), sealed. Returns 0 when the PAL language has no comment in the pieces (header still written, comment
 * left empty). */
int pc_card_header_build(const PcCardParts* parts, int language, uint8_t* header);

/* Puts another comment into a header (zero-filled, at most 0x1F bytes kept) and seals it again. */
void pc_card_header_set_comment(uint8_t* header, const uint8_t* comment);

/* Which card header a copy gets (snatcherLeo's call 1, 2026-11-19): the save's own original header when the target is the
 * region it came from; else the target region's own, built from that region's disc (when the player added it); else the
 * US one (from the US disc, which OrrePort always has); else the original as it is. Then, for a PAL copy, the comment
 * of the chosen language (call 2), and the sum redone.
 *   original           the save's original 0x6000-byte card header: region_header.bin's (after its 0x40-byte directory
 *                      entry) for a save imported from Europe or Japan, else the slot's own .gci header
 *   original_region    'E', 'P' or 'J': the region that header is from
 *   original_language  for a European original: the Options language its comment was written for (the newest slot's
 *                      block +0x2B; regions.md); ignored otherwise
 *   target_parts       the target region's pieces from its disc (NULL or not ok: no such disc)
 *   us_parts           the US disc's pieces (NULL or not ok: none)
 * Writes out (0x6000 bytes). Returns where it came from (PC_GCOPY_HEADER_*). *exact (may be NULL): 1 when the header is
 * the one that region's game itself writes for this language (verified equal for the US and PAL English, so a US header
 * counts for a PAL English copy), 0 when the memory card screen will show another region's picture or words. */
enum { PC_GCOPY_HEADER_KEPT = 0, PC_GCOPY_HEADER_OWN = 1, PC_GCOPY_HEADER_DISC = 2, PC_GCOPY_HEADER_US = 3 };
int pc_gcopy_pick_header(char target, int language, const uint8_t* original, char original_region, int original_language,
                         const PcCardParts* target_parts, const PcCardParts* us_parts, uint8_t* out, int* exact);

/* The species table of the US game's common_rel (the code module in common.fsys, unpacked): keys[id] = species id's
 * name message key, for ids 0..count-1 (0 for the unused ids). Returns count (413 on the US disc), 0 when the module
 * isn't the expected one. Verified 2026-11-19: the 386 non-zero keys are exactly the messages 0x3E9-0x56A, the same keys
 * on the European and Japanese discs (regions.md: every language uses the US message ids). */
int pc_gcopy_species_keys(const uint8_t* rel, size_t size, uint32_t* keys, int max);

/* One language's species names from the message banks in a code module or data blob whose bank header carries the
 * two-letter tag (the US common_rel's banks say "US"): names->name[id] points into blob (keep it alive). Returns how
 * many species got a name. */
int pc_gcopy_blob_names(const uint8_t* blob, size_t size, const char tag[2], const uint32_t* keys, int count,
                        PcGcopyNames* names);

/* The target's stamp: area (1 Japan, 2 US, 3 PAL), language, and the value of +0x2B / +0xB48 (the language for PAL,
 * 0 for the US and Japan). region 'E', 'P', 'J'; language is PAL's 2-6 (anything else counts as English, 2). */
void pc_gcopy_target_stamp(char region, int language, int* area, int* stamp_language, int* language_bytes);

/* The region bytes of one decrypted slot (pc_save_cipher.h) set for the target. Returns 1 when a byte changed. */
int pc_gcopy_slot_set_region(uint8_t* plain, char region, int language);

/* One language's species names: name[id] is species id's name as the game's text (big-endian UTF-16, ended by 0x0000,
 * no control codes), or NULL when unknown. */
struct PcGcopyNames {
    const uint8_t* name[PC_GCOPY_SPECIES_MAX];
};

/* A Pokémon whose name a copy rewrote (for the window's notes and the test's log) */
typedef struct PcGcopyRenamed {
    int species;
    int where;          /* 0-5 party, 100 + box * 30 + n a PC box, 200 the day care */
    uint8_t from[0x16]; /* the name it had (the record's 0x16-byte field) */
    uint8_t to[0x16];   /* the name it gets */
} PcGcopyRenamed;

/* snatcherLeo's call 3 in one decrypted slot: every Pokémon (party, the three PC boxes, the day care) whose nickname field
 * (record +0x2E) equals its own species' name in one of the `from` tables gets that species' name from `to` (cut to
 * the game's 10 letters, zero-filled). A nickname that is anything else is the player's and stays. Returns how many
 * were rewritten; up to max of them are listed in out (may be NULL). */
int pc_gcopy_slot_rename_species(uint8_t* plain, const PcGcopyNames* const* from, int from_count, const PcGcopyNames* to,
                                 PcGcopyRenamed* out, int max);

/* Every name a decrypted slot holds (for the "region_letters" rule: letters the target game's fonts may not have):
 * the trainer's (block +0x70), Rui's (block +0xB32), the three box names, and every Pokémon's nickname (party, PC boxes,
 * day care). text points into plain (big-endian UTF-16, ended by 0 or by units). Returns the count (at most max). */
enum { PC_GCOPY_NAME_TRAINER = 0, PC_GCOPY_NAME_PARTNER = 1, PC_GCOPY_NAME_BOX = 2, PC_GCOPY_NAME_POKEMON = 3 };
typedef struct PcGcopySlotName {
    int kind;    /* PC_GCOPY_NAME_* */
    int where;   /* a box's number (1-3), a Pokémon's place (as PcGcopyRenamed.where), else 0 */
    int species; /* a Pokémon's species id, else 0 */
    const uint8_t* text;
    int units;   /* the field's size in 16-bit units */
} PcGcopySlotName;
int pc_gcopy_slot_names(const uint8_t* plain, PcGcopySlotName* out, int max);

/* Everything a region copy does to a whole .gci (0x40 + 0x6000 + 3 slots), in place */
typedef struct PcGcopyRegionJob {
    char region;                         /* 'E', 'P', 'J' */
    int language;                        /* PAL: 2-6 */
    const uint8_t* card_header;          /* the 0x6000-byte header to put in, or NULL to keep the file's own */
    const PcGcopyNames* const* names_from; /* species names to recognise (NULL: no renaming) */
    int names_from_count;
    const PcGcopyNames* names_to;        /* and the target's (NULL: no renaming) */
} PcGcopyRegionJob;

/* Applies the job: the game code, the card header (when given), then in every valid slot the region bytes and the
 * renaming, each changed slot sealed again (an unchanged slot keeps its bytes exactly). Returns the number of valid
 * slots, -1 when the file isn't a three-slot Colosseum save or none is valid. *renamed (may be NULL): Pokémon renamed in
 * the newest valid slot. Up to max of the newest slot's renames are listed in out (may be NULL). */
int pc_gcopy_region_apply(uint8_t* gci, size_t len, const PcGcopyRegionJob* job, int* renamed, PcGcopyRenamed* out,
                          int max);

#ifdef __cplusplus
}
#endif

#endif /* PC_GCOPY_REGION_H */
