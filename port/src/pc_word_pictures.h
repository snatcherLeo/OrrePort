/*
 * pc_word_pictures.h: the game's word pictures in the chosen language (language piece 4, 2026-11-15;
 * docs/port/word-pictures.md). pc_word_pictures.cpp says how.
 *
 * Why a header: the language discs are read in one pass by pc_region_text.cpp (it owns nod and the disc handles), and
 * that pass hands this file what it needs (the texture entries of the approved pictures and the disc's main.dol) as it
 * goes, so each disc is read once. Platforms: all the same (plain C / C++).
 */
#ifndef PC_WORD_PICTURES_H
#define PC_WORD_PICTURES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* which language disc a call is about */
enum { PC_WP_DISC_EU = 0, PC_WP_DISC_JP = 1 };

/* start-up (pc_game_text_start): registers the three hooks (orre_hooks.h); ORRE_NO_WORD_PICTURES=1 leaves them off */
void pc_word_pictures_start(void);
/* the discs are about to be read (again): forget everything from the last read. Only before the game starts (the hub's
 * rescans), when nothing points into it yet */
void pc_word_pictures_reset(void);
/* from the disc read (its loader thread): one texture entry (resource type 9) of an archive. archive is the file name
 * without ".fsys"; the rest is the entry as the archive's table gives it (data offset, stored size, LZSS-packed) */
void pc_word_pictures_note_entry(int disc, const char* archive, uint32_t fileIndex, const char* name, uint32_t id,
                                 uint32_t dataOff, uint32_t stored, int packed);
/* from the disc read: the disc's main.dol (its sprite table is copied out) */
void pc_word_pictures_note_dol(int disc, const uint8_t* dol, size_t size);
/* from the disc read: that disc is done (the index is put together and summarised in the log) */
void pc_word_pictures_disc_done(int disc);

/* For OrrePort's own pictures made from the game's (the main menu's LOAD button and its shorter Quit,
 * pc_title_word.cpp / pc_title_loadsave.cpp; 2026-11-15): a language's copies, read from its disc on the caller's
 * thread with nod's own handle. Both return a malloc'd, LZSS-unpacked GS texture file (std::free it) and its size, or
 * NULL when that language has no disc, its disc isn't read yet, the word pictures are off (ORRE_NO_WORD_PICTURES), or
 * the picture isn't one of the approved ones. lang is OrrePort's code ("de", "fr", "it", "es", "ja").
 *   - _read: an approved picture by its archive entry name (e.g. "menu_070", the Battle Mode word sheet);
 *   - _piece: the picture the language's version of US sprite record k is cut from, and its rectangle (x, y, w, h in
 *     the texture), the pair the game itself draws in that language (never one without the other: the rule in
 *     pc_word_pictures.cpp). NULL also when the language keeps the US piece for that record. */
uint8_t* pc_word_pictures_read(const char* lang, const char* name, size_t* size);
uint8_t* pc_word_pictures_piece(const char* lang, uint32_t k, int16_t rect[4], size_t* size);

#ifdef __cplusplus
}
#endif

#endif
