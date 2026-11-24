/*
 * pc_gcopy.h: "Make a GameCube copy…" (the Saves page, 0.1; snatcherLeo, 2026-11-13, through C: "see if we can start building
 * the foundation for doing this"; "We keep inventory/a strict checklist of all kinds of things we keep adding to, to
 * ensure the saves can be converted"). pc_gcopy.c says how and why.
 *
 * The foundation is ONE list of rules (the project's draft): each rule looks at a save and reports what can't go into a GameCube
 * copy ("stays in OrrePort") or what the copy changes (a 12-letter nickname cut to the GameCube's 10). Making the copy
 * runs every rule, shows the list first, then writes a plain .gci the player puts wherever they like. New features
 * that store something a GameCube can't hold add a rule (the living checklist, docs/port/saves.md); the copy's code
 * never changes for them.
 *
 * Platforms: plain C over SDL's file calls; the same on Windows, Linux and macOS.
 */
#ifndef PC_GCOPY_H
#define PC_GCOPY_H

#include <stddef.h>
#include <stdint.h>

#include "pc_grapheme.h" /* PC_NAME_MAX_UTF8 (a whole 12-letter name) */

#ifdef __cplusplus
extern "C" {
#endif

enum { PC_GCOPY_MAX_NOTES = 64 };

/* What a note is (snatcherLeo, 2026-11-14, mockup 104b: "this architecture is great, we'll have this going forward and add
 * to it"): each kind is one section of the copy window, in this order; empty sections don't show. A new feature adds
 * a rule whose notes pick a kind; the window never changes for it. */
typedef enum PcGcopyKind {
    PC_GCOPY_CHANGES = 0, /* "Changes in the copy": the copy has a GameCube version (a name cut, a stack capped) */
    PC_GCOPY_STAYS = 1,   /* "Stays in OrrePort": not in the copy; waits here, back if the copy returns */
    PC_GCOPY_OFF = 2,     /* "Turned off in the copy": a gameplay mod */
    PC_GCOPY_BLOCKS = 3   /* "Can't be copied": stops the copy, with the reason */
} PcGcopyKind;

typedef struct PcGcopyNote {
    const char* rule;     /* the rule's id ("save", "names", ...) */
    int blocking;         /* 1: no copy can be made (e.g. no readable save); kind is then PC_GCOPY_BLOCKS */
    int kind;             /* PcGcopyKind: which section it shows in */
    int editable;         /* 1: an Edit link (names: the GameCube name); what it edits is the rule's (edit_id) */
    char edit_id[48];     /* for the rule's editor: which name ("hero", "partner", "box1", "nick.<key>") */
    char text_key[64];    /* a ui/lang key saying what happens ("gcopy.note.name_cut") */
    char what[160];       /* the thing it is about ("Nuzlocke's Espeon") */
    /* what stays in OrrePort ("DESTROYERRRR"). *Changed 2026-11-16 (visible letters, pc_grapheme.h):* sized for the
     * longest full name (was 96 bytes: twelve emoji families, 300 bytes, were cut mid-character) */
    char kept[PC_NAME_MAX_UTF8];
    char copy[96];        /* what the GameCube copy has instead ("DESTROYERR") */
} PcGcopyNote;

/* Which game the copy is for (3.5 step 9; snatcherLeo's calls of 2026-11-19, the decision log): the US, European (PAL) or Japanese
 * version, and for a European copy its language (the European game continues a story only in the language it was saved
 * in, so the copy is stamped with one). pc_gcopy_region.h says what changes in the bytes. */
typedef struct PcGcopyTarget {
    char region;  /* 'E' US, 'P' Europe (PAL), 'J' Japan */
    int language; /* for 'P': 2 English, 3 German, 4 French, 5 Italian, 6 Spanish (the game's numbers); else ignored */
} PcGcopyTarget;

/* The copy window's defaults for a slot: the save's own region (the one it was imported from, else its stamp's; a save
 * started in OrrePort is a US save); a European copy's language is the play language when that is a European one, else
 * English (snatcherLeo's call 2). */
void pc_gcopy_default_target(int slot, PcGcopyTarget* out);

/* Every rule on the slot's save for that target (NULL: the defaults): the notes (what stays behind, what changes),
 * their count; 0 notes = a clean copy */
int pc_gcopy_check(int slot, const PcGcopyTarget* target, PcGcopyNote* notes, int max);

/* The copy's file name: "<cleaned name> - Colosseum.gci", with the region for a European or Japanese copy
 * ("<name> - Colosseum (PAL).gci"), so copies for two regions don't take each other's name */
void pc_gcopy_file_name(int slot, const PcGcopyTarget* target, char* out, size_t size);

/* Writes the GameCube copy of the slot for that target (NULL: the defaults) into folder dest_dir under
 * pc_gcopy_file_name, never over an existing file (" (2)" and so on). The slot's own save is never changed. The written
 * path in out. 1 on success. */
int pc_gcopy_make(int slot, const PcGcopyTarget* target, const char* dest_dir, char* out, size_t out_size);

/* The same, to the exact file chosen in a Save As window (".gci" added when missing); 1 on success */
int pc_gcopy_make_at(int slot, const PcGcopyTarget* target, const char* file_path, char* out, size_t out_size);

/* The language disc reader (pc_region_text.cpp) hands over a European ('P') or Japanese ('J') disc's main.dol as it
 * reads it, so a copy for that region gets that game's own card header (snatcherLeo's call 1); dol NULL forgets it (the discs
 * changed). Only the header's pieces are kept (a few KB). Safe to call from the reader's thread. */
void pc_gcopy_note_dol(char region, const uint8_t* dol, size_t size);

#ifdef __cplusplus
}
#endif

#endif
