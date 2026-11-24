/*
 * pc_region_look.h: the Japanese release's look, from one table of regional differences (3.5 steps 2 and 3; the project's
 * helper, 2026-11-19; docs/port/word-pictures.md "The Japanese release look"). pc_region_look.cpp says how.
 *
 * Why a header: the table (ui/regions/jp_look.txt) is read once here and asked by the three places that act on it:
 * the word pictures (pc_word_pictures.cpp: rows swapped through the sprite records), the language files
 * (pc_lang_files.cpp: rows swapped as archive entries) and the language disc read (pc_region_text.cpp: the check that
 * every row's entry is on the player's disc). Later the hub's "What changes" list (3.5 step 5) and Studio read the same
 * rows through pc_region_look_row. Platforms: all the same (plain C / C++).
 */
#ifndef PC_REGION_LOOK_H
#define PC_REGION_LOOK_H

#ifdef __cplusplus
extern "C" {
#endif

/* how a row is swapped (the table's "how" column) */
enum { PC_RL_NONE = 0, PC_RL_SPRITE = 1, PC_RL_ENTRY = 2 };

/* one row of the table, as written (every field trimmed; never NULL) */
typedef struct PcRegionLookRow {
    const char* id;        /* stable name, e.g. "jp.logo_nintendo" */
    const char* kind;      /* "picture" or "model" */
    const char* entry;     /* the archive entry's name, the same on both discs */
    const char* archives;  /* where the US game takes it from, comma-separated ("main.dol" = embedded) */
    const char* region;    /* "JP" */
    const char* condition; /* "lang=ja disc=GC6J01" */
    const char* what;      /* the ui/lang key saying what it shows */
    const char* how;       /* "sprite", "entry" or "none" */
    const char* note;
} PcRegionLookRow;

/* start-up (pc_word_pictures_start): reads the table once and logs what it holds. Safe to call again */
void pc_region_look_start(void);
/* 1 when ORRE_NO_REGION_LOOK=1 switched every swap of the table off (a test switch) */
int pc_region_look_off(void);
/* the rows read (bad rows are left out, logged), and row i (NULL past the end) */
int pc_region_look_count(void);
const PcRegionLookRow* pc_region_look_row(int i);
/* how a picture row of the region ('J') with this entry name is swapped: PC_RL_SPRITE, PC_RL_ENTRY, or PC_RL_NONE (not
 * in the table, a model, or the table switched off) */
int pc_region_look_how(char region, const char* entry);

/* from the language disc read (its loader thread): one archive entry of that region's disc (archive without ".fsys");
 * and that disc is done (the check that every row's entry is where the table says is logged) */
void pc_region_look_note_found(char region, const char* archive, const char* entry);
void pc_region_look_disc_done(char region);
/* the discs are about to be read again (the hub's rescans): forget what the last read found */
void pc_region_look_reset(void);

/* the log line of what applies now (pc_lang_files_apply: at start-up and on every language change). lang is the game's
 * language, haveDisc whether the player's Japanese disc is added, filesOff whether the language files (the "entry"
 * rows' path) are switched off (ORRE_NO_LANGUAGE_FILES) */
void pc_region_look_log_state(const char* lang, int haveDisc, int filesOff);

#ifdef __cplusplus
}
#endif

#endif
