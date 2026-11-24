/* pc_save_region.h: which region a Colosseum save comes from, read from its own bytes, and the one change an import
 * makes to a European or Japanese save (Phase 3 step 3.5, steps 6 and 7, 2026-11-19).
 *
 * Why its own file: the hub, the game's SAVE FILES list and the importer all need "which save slot is current" and "what
 * does its region stamp say", and a test program has to prove both on real saves without SDL or the game. So the byte
 * work lives here, in plain C over memory, and pc_saves.c (which has the files, SDL and the slot folders) calls it.
 *
 * What a save says about its region (docs/engine/regions.md, "What real saves from each region look like"; verified on
 * snatcherLeo's four saves, a US, two European and a Japanese one):
 * - the card file's game code, the first 4 bytes of the 0x40-byte .gci header: "GC6E" (US), "GC6P" (Europe), "GC6J"
 *   (Japan); the maker code at +4 is "01" in every region. Nothing else in the container differs except the 0x6000-byte
 *   card header after it (the comment, banner and icon);
 * - the region stamp in the decrypted save block (block = slot + 0x08): +0x02 the area (1 Japan, 2 US, 3 Europe), +0x03
 *   the language. The language numbers: 1 Japanese, 2 English, 6 Spanish verified from real saves; 3 German, 4 French,
 *   5 Italian likely (the Card-e region check's language map, cardesavedata.c: GBA French 3 -> 4, Italian 4 -> 5,
 *   German 5 -> 3, Spanish 7 -> 6);
 * - read from the newest valid save slot (the highest save counter whose digest checks out), as the game loads it. The
 *   first slot of a save made in a European game is the card file's creation save, still stamped English whatever the
 *   language later chosen (verified on the PAL Spanish save: slot 0 says 3/2, the newer slot 1 says 3/6).
 *
 * Plain C99 on bytes, no platform calls: the same on Windows, Linux and macOS (built and tested on Windows only yet).
 * XD: its saves use another container and cipher (likely different); the idea (a stamp per save, a game code per region)
 * likely carries over. */
#ifndef PC_SAVE_REGION_H
#define PC_SAVE_REGION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PC_GCI_HEADER = 0x40,       /* the .gci header: the memory card's directory entry for the file */
    PC_GCI_CARD_HEADER = 0x6000, /* the card file's own header after it: comment, banner, icon (three slots follow) */
    PC_SAVE_BLOCK_AT = 0x08,    /* the save block inside a decrypted slot */
    PC_SAVE_STAMP_AREA = PC_SAVE_BLOCK_AT + 0x02,
    PC_SAVE_STAMP_LANGUAGE = PC_SAVE_BLOCK_AT + 0x03
};

/* the stamp's area numbers (as the game writes them; GSgetVersionMajor's 0 / 1 / 2 plus one) */
enum { PC_SAVE_AREA_UNKNOWN = 0, PC_SAVE_AREA_JAPAN = 1, PC_SAVE_AREA_US = 2, PC_SAVE_AREA_PAL = 3 };

/* The newest valid slot of a Colosseum .gci (the one the game loads: the highest save counter, +0x04 of each slot,
 * stored unencrypted, whose digest checks out; on a tie the first), decrypted into plain (PC_SAVE_SLOT_SIZE bytes, from
 * pc_save_cipher.h). Returns its index 0-2, or -1 when the file is too short for three slots or none is valid. Only the
 * candidates are decrypted, highest counter first, so a normal save costs one decrypt instead of three. */
int pc_save_gci_newest_slot(const uint8_t* gci, size_t len, uint8_t* plain);

/* The area and language of a decrypted slot's stamp (plain from pc_save_gci_newest_slot). The area is
 * PC_SAVE_AREA_UNKNOWN when the byte isn't 1, 2 or 3; the language is passed through as stored (0 when the area is
 * unknown). */
void pc_save_stamp(const uint8_t* plain, int* area, int* language);

/* The region letter of a .gci's game code when it is Colosseum's ("GC6" + E / P / J, maker "01"): 'E', 'P' or 'J'; 0
 * for anything else (another game, another maker, an unknown region letter). */
char pc_save_gci_region_letter(const uint8_t* gci, size_t len);

/* The import's one change (decided 2026-11-17, snatcherLeo; regions.md "So moving a save between regions needs only the card
 * file's game code changed"): the game code at bytes 0-3 becomes "GC6E", so the US program's memory card (CARDInit
 * "GC6E", "01") opens the file. Everything else stays: the stamp, the language bytes, each Pokémon's origin stamp, the
 * card header (kept as it is; the GameCube copy puts the original back later). The slot digest and the icon checksum
 * don't cover the directory entry, so the save stays valid (likely; the digest is inside each slot). Returns 1 when
 * bytes changed, 0 when it was already a US code. */
int pc_save_gci_make_us(uint8_t* gci, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* PC_SAVE_REGION_H */
