/*
 * pc_layout.h: the UI-per-ratio layout table (widescreen groundwork, Phase 3 step 3.6; docs/port/widescreen-plan.md
 * section 6, "The table and its loader").
 *
 * Why: OrrePort 0.1 ships 4:3, 16:9 and 16:10 (21:9 later, with Studio's UI Editor). snatcherLeo (2026-11-19): every piece
 * that depends on the aspect ratio is tied to its 4:3 place; a ratio's file holds only overrides of that base, so a
 * piece with no row keeps its 4:3 place, centred, and a new ratio works at once and is polished piece by piece. 4:3
 * has no file. Studio's UI Editor will write these files; mods will replace sections for one ratio.
 *
 * What: reads port/ui/layout/<ratio>.ini (copied next to the exe as ui/layout/) into a hash map keyed by the piece id
 * (the section name: "window.0013", "menu.0013.item.0911", "code.fade", "effect.sunaarashi_all.0", "art.title_bg", the
 * ids the 2D inventory lists). A lookup that finds no row answers "the 4:3 place, centred" (pc_layout_lookup's
 * default). Nothing consults it yet (no offset hook reads it: nothing moves); the offset hooks of the plan's build
 * order step 5 will.
 *
 * Threads: load on one thread before lookups start (a ratio change is a reset: the plan, section 8); lookups only read.
 * Platforms: plain C over stdio, the same on Windows, Linux and macOS (verified on Windows by port/test/pc_layout_test.c;
 * the others likely). The ui folder's path comes from the caller (SDL_GetBasePath on every OS).
 * XD: the format and the reader carry over unchanged (likely); the ids and rows are Colosseum's.
 */
#ifndef PC_LAYOUT_H
#define PC_LAYOUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* where a piece sits on a screen wider than 4:3 (the 4:3 picture is centred in it) */
typedef enum PcLayoutAnchor {
    PC_LAYOUT_CENTRE = 0, /* the 4:3 place, in the centred 4:3 area (the default: no row) */
    PC_LAYOUT_LEFT,       /* keeps its distance from the left edge of the 4:3 area, measured from the wide screen's */
    PC_LAYOUT_RIGHT,      /* the same from the right edge */
    PC_LAYOUT_STRETCH,    /* spans the full width (a full-screen layer drawn by code) */
    PC_LAYOUT_KEEP43      /* stays exactly 4:3 with a border beside it (art screens, titles) */
} PcLayoutAnchor;

/* how a full-screen effect spans the picture (the sandstorm's row, the plan's section 3) */
typedef enum PcLayoutSpan {
    PC_LAYOUT_SPAN_DEFAULT = 0, /* no span given: the piece's own (4:3) */
    PC_LAYOUT_SPAN_43,          /* "43": kept 4:3 wide */
    PC_LAYOUT_SPAN_FULL         /* "full": widened to the whole picture */
} PcLayoutSpan;

enum { PC_LAYOUT_ID_MAX = 64, PC_LAYOUT_BORDER_MAX = 32, PC_LAYOUT_NOTE_MAX = 160 };

typedef struct PcLayoutRow {
    char id[PC_LAYOUT_ID_MAX];         /* the section name, lower case */
    PcLayoutAnchor anchor;
    float offsetX, offsetY;            /* logical 640 x 480 pixels, added after the anchor */
    PcLayoutSpan span;
    char border[PC_LAYOUT_BORDER_MAX]; /* a kept panel's border ("black", "#rrggbb", "none"); "" = not given */
    char note[PC_LAYOUT_NOTE_MAX];     /* free text for the UI Editor (why the row exists); never shown to the player */
    int fromFile;                      /* 1 for a row read from a file, 0 for lookup's default answer */
} PcLayoutRow;

/* Drop every row (back to the 4:3 base: every piece at its 4:3 place, centred). */
void pc_layout_clear(void);

/* Read rows from text (a whole .ini file's contents). A section seen again replaces the earlier row whole (a mod's
 * file read after the shipped one replaces its sections). Returns the rows read, or -1 on an out-of-memory failure.
 * Problems (an unknown key, a bad value, a bad id) are counted and logged as "[layout] <source>:<line>: ..." on stderr
 * and the rest of the file is still read. */
int pc_layout_load_text(const char* text, size_t length, const char* sourceName);

/* Read one file. Returns the rows read; 0 for a missing file (no file means the 4:3 base, not an error); -1 when it
 * exists but can't be read. */
int pc_layout_load_file(const char* path);

/* The file name for a ratio ("16x9" -> "16x9.ini"); NULL for "4x3" and anything without a file. */
const char* pc_layout_file_for_ratio(const char* ratio);

/* Clear, then read <uiDir>/layout/<ratio>.ini (uiDir ends with a slash or is ""). 4:3 reads nothing. Returns the rows
 * read, as pc_layout_load_file. Logs one "[layout] ..." line saying which file and how many rows. */
int pc_layout_load_ratio(const char* uiDir, const char* ratio);

/* The row for a piece id, or NULL: no row, so the 4:3 place, centred. */
const PcLayoutRow* pc_layout_find(const char* id);

/* The row for a piece id; with no row, a default answer (anchor centre, no offset, span default, fromFile 0). */
PcLayoutRow pc_layout_lookup(const char* id);

int pc_layout_count(void);    /* rows held now */
int pc_layout_problems(void); /* problems counted since the last clear */

/* The words a row's values are written with, for the UI Editor and logs ("centre", "full", ...). */
const char* pc_layout_anchor_name(PcLayoutAnchor anchor);
const char* pc_layout_span_name(PcLayoutSpan span);

#ifdef __cplusplus
}
#endif

#endif
