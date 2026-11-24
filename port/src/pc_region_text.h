/*
 * pc_region_text.h: the language discs (region work, 2.2 step 8): the European languages' text read from the player's
 * own European disc (pc_region_text.cpp says how), and the discs in Colosseum\ROM Other Regions\ for the hub's Language
 * discs panel (docs/port/language-discs.md). Platforms: all (nod reads the discs; the same on every OS).
 */
#ifndef PC_REGION_TEXT_H
#define PC_REGION_TEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* a language disc found in ROM Other Regions\: its file, its game ID (GC6P01 European, GC6J01 Japanese) and size */
typedef struct PcLangDisc {
    char path[1100];
    char id[7];
    uint64_t size;
} PcLangDisc;

/* <exe>/Colosseum/ROM Other Regions/ (out returned) */
const char* pc_lang_discs_folder(char* out, size_t size);
/* a disc image's game ID (6 letters) into id[7], read with nod without touching the US disc the game uses; 1 when it
 * is a GameCube disc nod can read */
int pc_lang_disc_identify(const char* path, char* id);
/* the language discs of the game whose IDs start with prefix ("GC6" for Colosseum), one per region (the newest
 * original wins, then the first by name: the shared rule, 2026-11-23), European first; the count */
int pc_lang_discs_list(const char* prefix, PcLangDisc* out, int max);
/* 1 when the European / Japanese disc at path (game ID id) is the original release: its main.dol's SHA-1 (2026-11-23) */
int pc_lang_disc_is_original(const char* path, const char* id);

/* start-up: find the European disc and read its text (wait != 0: wait for it, the game starts in one of its languages) */
void pc_region_text_start(int wait);
/* the program is ending without the game (the hub's window closed): waits for a read still running */
void pc_region_text_finish(void);
/* again, after the hub added or removed a language disc (the text is read anew in the background) */
void pc_region_text_rescan(void);
int pc_region_text_has(const char* code);
const uint8_t* pc_region_text_get(const char* code, uint32_t key, size_t* size);
/* the language disc's file found at start-up (jp 0: the European disc, 1: the Japanese), or "" (piece 5's files,
 * pc_lang_files.cpp, open it with their own nod handle) */
const char* pc_region_disc_path(int jp);
/* the Japanese disc's own glyph for a character the game's font lacks: its record, its bank in *outBank (part D) */
const void* pc_region_font_glyph(uint16_t fontId, uint16_t code, int cellH, void** outBank);
/* the Japanese disc font's own picture of a character: 4-bit pixels (rows of ceil(w / 2) bytes), its font's cell
 * height and size; NULL without it (the kanji tracer's larger-font check, pc_text_atlas.cpp) */
const uint8_t* pc_region_font_picture(uint16_t fontId, uint16_t code, int* cellH, int* w, int* h);
/* whether the Japanese disc's font of that id has the character (any cell height; 0 without the disc) */
int pc_region_font_has(uint16_t fontId, uint16_t code);
/* whether the Japanese game's own text uses a character: 1 / 0, -1 not known yet (the rare-kanji rule, 2026-11-16) */
int pc_region_text_uses(uint16_t code);
/* the US disc's common_rel, unpacked (read once from the open US disc): the GameCube copy's species table and English
 * species names (3.5 step 9, pc_gcopy.c); NULL when it can't be read */
const uint8_t* pc_region_us_common_rel(size_t* size);

#ifdef __cplusplus
}
#endif

#endif
