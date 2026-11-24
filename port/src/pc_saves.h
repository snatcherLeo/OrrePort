/*
 * pc_saves.h: OrrePort's save slots (step 2.1, snatcherLeo, 2026-11-13; mockup 72; the plan's "Saves, boxes and the OrreBank",
 * the project's reviews). pc_saves.c says how each part works and why.
 *
 * In short:
 * - Every save lives in its own numbered folder, Colosseum\Saves\Slot N\, with its .gci and a small setup file
 *   (slot.ini: the name the player gave it, where it came from, a format version). The folder numbers never change;
 *   what the player sees is the name.
 * - The save's file is named after the save ("Nuzlocke - Colosseum.gci"), cleaned for every operating system. The
 *   memory card reads any .gci by its header (extern/aurora CardGciFolder.cpp), as Dolphin does.
 * - Previous versions: Slot N\Backups\<date_time>\, one kept before each in-game save and at a launch whose save
 *   changed since the last one, the newest 10 per save; "Restore from previous version" puts one back, keeping the
 *   current save first, and offers Undo until the save is played.
 * - Importing: a .gci or a whole memory card (.raw), checked (game, region, the save's own SHA-1 check), copied into
 *   the next free slot as "Imported save"; the player's file is never changed. *Since 2026-11-19 (3.5 step 7):* US,
 *   European and Japanese saves; the slot's copy of a European or Japanese one gets the US game code (bytes 0-3 of
 *   the .gci) so the card opens it, with the original region and card header kept in the slot folder (slot.ini).
 *
 * Platforms: plain C over SDL's file functions (Windows, Linux, macOS); only "send to the Recycle Bin" is per
 * platform (pc_saves.c: Windows' shell; elsewhere the slot moves to Saves\Deleted\ until a trash call is added).
 */
#ifndef PC_SAVES_H
#define PC_SAVES_H

#include <stddef.h>

#include "pc_grapheme.h" /* PC_NAME_MAX_UTF8 (the trainer's full name) */

#ifdef __cplusplus
extern "C" {
#endif

enum { PC_SAVES_FORMAT_VERSION = 1 }; /* written into every slot.ini (groundwork item 1: later formats upgrade by it) */

typedef struct PcSaveInfo {
    int slot;              /* the folder number (Slot N) */
    char name[160];        /* the name the player gave it (UTF-8, as typed) */
    char file[512];        /* the save file's full path, "" when the slot has none yet */
    int has_save;          /* a readable Colosseum save is there */
    int is_orresave;       /* .orresave: the slot has an extras file (2.4; pc_names.cpp) */
    /* the trainer's name read from the save (UTF-8), or the full name from the extras file. *Changed 2026-11-16
     * (visible letters, pc_grapheme.h):* sized for the longest full name (was 48 bytes, which cut twelve emoji
     * mid-character) */
    char trainer[PC_NAME_MAX_UTF8];
    unsigned play_seconds; /* play time read from the save */
    long long saved_time;  /* when the file was last written (seconds since 1970, local display by the caller) */
    int is_new;            /* imported and not opened on the Saves page yet */
    char came_from[64];    /* "made" / "imported" / "duplicated" / "found": the setup file's came_from */
    int order;             /* the page's own order (duplicates sit right under their original) */
    int damaged;           /* a save file is there but can't be read as a Colosseum (USA) save: its check fails, it's
                            * cut short, or it's another game's (2026-11-14, rule R2: the SAVE FILES list tells it
                            * apart from an empty slot) */
    int accurate;          /* plays in GameCube-accurate mode (slot.ini gamecube_accurate; 2.12, 2026-11-16) */
    /* Where the save comes from (3.5 step 6, 2026-11-19; snatcherLeo: the coloured region word on every save), read from the
     * save's own region stamp in its newest slot (pc_save_region.h), so it stays true after an imported save is played
     * here: region 1 Japan, 2 US, 3 Europe (PAL), 0 unknown or no save; language as stamped: 1 Japanese, 2 English,
     * 6 Spanish (verified), 3 German, 4 French, 5 Italian (likely). A save started in OrrePort is a US save (2 / 2). */
    int region;
    int language;
    /* the original game code of a save imported from another region ("GC6P", "GC6J"; slot.ini region=), "" otherwise:
     * the slot's file itself says GC6E since the import (3.5 step 7) */
    char imported_region[8];
} PcSaveInfo;

typedef struct PcSaveVersion {
    char folder[600];      /* Slot N\Backups\<date_time>\ */
    char when[32];         /* "20261113_025512" (the folder's name) */
    char why[32];          /* "launch" / "before_save" / "before_restore" / "before_import" */
    char trainer[48];
    unsigned play_seconds;
    int same_as_now;       /* identical to the save as it is now (the Restore window hides it: nothing would change) */
    int is_undo;           /* the copy a pending Undo puts back (hidden while Undo is offered; mockup 74) */
} PcSaveVersion;

/* Import results */
enum {
    PC_SAVE_IMPORTED = 0,
    PC_SAVE_ERR_READ = -1,      /* the file couldn't be read */
    PC_SAVE_ERR_NOT_SAVE = -2,  /* not a GameCube save or memory card */
    PC_SAVE_ERR_OTHER_GAME = -3,
    PC_SAVE_ERR_XD = -4,        /* "XD support comes later" */
    PC_SAVE_ERR_REGION = -5,    /* a Colosseum save with an unknown region letter or maker code (*changed 2026-11-19,
                                   3.5 step 7:* European and Japanese saves are imported now) */
    PC_SAVE_ERR_DAMAGED = -6,   /* the save's own check fails */
    PC_SAVE_ERR_NO_COLOSSEUM = -7, /* a memory card with no Colosseum save on it */
    PC_SAVE_ERR_ALREADY = -8,   /* the same save is already in a slot (*already_slot) */
    PC_SAVE_ERR_WRITE = -9,     /* the slot couldn't be written (disk full, read-only) */
    PC_SAVE_ERR_TAMPERED = -10  /* an .orresave whose long names don't belong to its save (import with force = 2:
                                   just the save, as a plain .gci) */
};

/* Start-up, before the card opens: registers every slot (adopts a Slot folder without a setup file, imports a loose
 * .gci / .raw put into Saves\ by moving it in), moves the old shared backups into their slot, keeps a launch version
 * of the chosen save when it changed. saves_dir ends with '/'. */
void pc_saves_init(const char* saves_dir);
int pc_saves_ready(void);                         /* the portable layout's Saves folder is in use */

int pc_saves_current(void);                       /* the slot Play starts (settings.ini save_slot) */
void pc_saves_set_current(int slot);
const char* pc_saves_slot_dir(int slot, char* out, size_t size); /* "...\Saves\Slot N\" */
/* the folder the card opens for the current slot (creating Slot 1 when there's none); the launch version's folder in
 * *launch_version (or "") for Report a problem and the recordings */
const char* pc_saves_open_current(char* launch_version, size_t size);

int pc_saves_list(PcSaveInfo* out, int max);       /* every slot, in the page's order */
int pc_saves_import_loose(void);                   /* .gci / .raw files put straight into Saves\: imported (pc_saves.c) */
/* The import question (3.5; snatcherLeo, 2026-11-19, "Importing a save asks how to play it"): the slots pc_saves_import_loose
 * made since the last call, so the hub can ask how to play them (one question for all). Returns how many (at most max)
 * and forgets them. Only the hub calls it, once it shows; loose files imported by the game's LOAD list (rule R6, the hub
 * long gone) are never asked about and keep the new-saves default. All platforms the same */
int pc_saves_take_loose_imported(int* out, int max);
/* rules R4 / R7: the slot's save file put back (from `bytes`, else its newest version) when it is missing; 1 = put back */
int pc_saves_put_back(int slot, const void* bytes, size_t len);
int pc_saves_get(int slot, PcSaveInfo* out);       /* 1 if the slot exists */
void pc_saves_mark_seen(int slot);                 /* clears "new" */

/* path: a .gci or .raw anywhere; the file is copied (move = 1 only for files the player put into Saves\). force = 1
 * imports a save already in a slot as a new one. *new_slot = the slot made; *already_slot when ALREADY. */
int pc_saves_import(const char* path, int move, int force, int* new_slot, int* already_slot);

int pc_saves_rename(int slot, const char* name);   /* the name and the file's name; 0 on failure */
int pc_saves_duplicate(int slot);                  /* the new slot, right under the original ("Name (1)"); 0 */
int pc_saves_new_empty(const char* name);          /* a new save with no file yet (NEW GAME), at the top; its slot; 0 */
int pc_saves_delete(int slot);                     /* to the Recycle Bin (Windows) / Saves\Deleted\; 0 on failure */
int pc_recycle_file(const char* path);             /* one file to the Recycle Bin (Windows) / a Removed\ folder beside it */

int pc_saves_versions(int slot, PcSaveVersion* out, int max); /* newest first */
int pc_saves_restore(int slot, const char* version_folder);   /* keeps the current save first; 0 on failure */
/* Undo after a restore (mockup 74; offered until the save is played or restored again): 1 while offered, with the
 * version folder name gone back to ("20261113_052000", the notice's date) and the save's play time now */
int pc_saves_undo_info(int slot, char* gone_back_to, size_t size, unsigned* play_seconds);
int pc_saves_undo(int slot);                                  /* puts the save back as it was; 0 on failure */

/* Read a save without changing it (the hub's card, the Saves page, import). 1 = a readable Colosseum save (US; since
 * 3.5 step 7 also a European or Japanese one, whose game code the import changes to GC6E) */
int pc_saves_read_info(const char* path, char* trainer, size_t trainer_size, unsigned* play_seconds);

/* What the main menu's SAVE FILES list shows in its help box for one save (2.2 step 6, plan step 6, 2026-11-14;
 * docs/port/title-load-save.md), read without changing the file and without loading it into the game:
 * - floor_id: the room Continue resumes in (the save header's floor id, gamedatasaveGetStatus kind 5); the caller turns
 *   it into the game's own place name;
 * - card_time: when the game last saved it, from the card's directory entry (.gci +0x28): seconds since
 *   2000-01-01 on the console's own clock (no time zone: shown as stored, as the GameCube's card screen does); -1 when
 *   the file has none (then the caller uses PcSaveInfo.saved_time);
 * - story_flags: the story-flag words (save section 6, flag class 3), in this machine's byte order, as the game holds
 *   them once loaded; the Shadow Pokémon counts are tested against them with the game's own flag positions.
 * Why a separate call (not in PcSaveInfo): it decrypts the whole save, so the list reads it only for a save it shows. */
enum { PC_SAVE_STORY_FLAG_WORDS = 0x180 };
typedef struct PcSaveDetails {
    unsigned floor_id;
    long long card_time;
    unsigned story_flags[PC_SAVE_STORY_FLAG_WORDS];
} PcSaveDetails;
int pc_saves_read_details(const char* path, PcSaveDetails* out); /* 1 = a readable Colosseum (USA) save */

/* the slot as one .orresave file (a zip: manifest, save, long names, settings; 2.4 part 5) at file_path (".orresave"
 * added when missing); only a slot with long names; 1 on success, the written path in out */
int pc_saves_export_orresave(int slot, const char* file_path, char* out, size_t out_size);

/* the slot's save file as a SHA-1 in hex (the extras file's fingerprint, 2.4); 0 when the slot has no save file */
int pc_saves_save_digest(int slot, char hex[41]);

/* the exact spot a PC MENU save was made at: slot.ini's "spot=" line ("<the .gci's SHA-1> <values>"), or "" (0); set
 * "" to remove it (pc_field_menu.cpp; the save itself stays a .gci) */
int pc_saves_spot_get(int slot, char* out, size_t size);
int pc_saves_spot_set(int slot, const char* value);

/* GameCube-accurate mode, per save (2.12; snatcherLeo 2026-11-08 / 11-10 / 11-16; pc_options.c says how a run's mode is
 * decided). Kept in slot.ini as "gamecube_accurate=0/1".
 * - get: the save's setting (1 accurate); a slot with none yet takes the new-saves default (pc_options.c);
 * - set: 1 when saved; 0 when it couldn't be written; PC_SAVES_ACCURATE_NEEDS_COPY when turning it on for an .orresave
 *   (accurate mode plays .gci saves only: the hub offers a GameCube copy instead; nothing changes). Only the hub calls it,
 *   and the hub runs only while the game is closed, so a running game's mode never changes. */
enum { PC_SAVES_ACCURATE_NEEDS_COPY = -1 };
int pc_saves_accurate_get(int slot);
int pc_saves_accurate_set(int slot, int on);

/* A GameCube copy made for accurate play (2.12): the copy's .gci (gci_path, already written by pc_gcopy_make_at) added
 * as a new save right under the slot it was made from, named `name`, with accurate mode set as asked. It is a plain .gci
 * (no extras file is carried, unlike Duplicate or an import of the same bytes), so it can play in accurate mode. Returns
 * the new slot, 0 on failure. */
int pc_saves_add_gcopy(int from_slot, const char* gci_path, const char* name, int accurate);

/* "Nuzlocke - Colosseum.gci" from a typed name: safe on Windows, macOS and Linux (pc_saves.c lists the rules) */
void pc_saves_file_name(const char* display, int slot, char* out, size_t size);

#ifdef __cplusplus
}
#endif

#endif
