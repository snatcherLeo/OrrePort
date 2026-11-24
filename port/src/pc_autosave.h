/*
 * pc_autosave.h: OrrePort's autosave (2026-11-20; pc_autosave.cpp says how and why; the design and how it's built:
 * docs/port/autosave.md).
 *
 * In short: one autosave per save slot, in Slot N\Autosave\ (autosave.gci, the game's own save format, and autosave.ini,
 * the spot Continue goes on from), plus the one before it (autosave.prev.*). Written when a field room has settled and
 * nothing in it has changed yet, by a timer in a quiet room, at most once a minute; never in a battle, scene, menu or room
 * change. The player's own save is never touched; Continue still loads it. The autosave is loaded on request: LOAD's
 * SAVE FILES list, the hub's Saves page, the hang watch.
 *
 * Platforms: plain C interface; the same on Windows, Linux and macOS.
 */
#ifndef PC_AUTOSAVE_H
#define PC_AUTOSAVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcAutosaveInfo {
    int exists;            /* a readable autosave is there */
    int is_prev;           /* it is the one before the last (the last one is missing or damaged) */
    char path[1300];       /* its .gci */
    unsigned play_seconds; /* play time read from it */
    long long unix_time;   /* when it was written (seconds since 1970) */
    char when[32];         /* the same in the Backups folders' form, local time ("20261120_031500") */
    char place[160];       /* the place's name as the game named it when written (the game's language then) */
    char spot[200];        /* autosave.ini's spot line ("<sha1> room prev entry x y z facing") */
    unsigned spot_room;    /* the spot's room (0 when the line can't be read) */
} PcAutosaveInfo;

/* the slot's autosave (the newest readable of autosave.gci / autosave.prev.gci); 1 when there is one. Reads files only */
int pc_autosave_info(int slot, PcAutosaveInfo* out);
/* autosave is on in this run (not GameCube-accurate mode; automated runs only with ORRE_AUTOSAVE=1) */
int pc_autosave_on(void);
/* the next title read of that slot's save (memory-card task 1) loads its autosave instead; then Continue goes on from
 * the autosave's spot. The slot's own files are not changed */
void pc_autosave_request_load(int slot);
/* *Added 2026-11-21 (snatcherLeo: "should we make it clear that, the current save loaded, is an autosave (just in Hub)?"):*
 * the slot whose autosave the next title read will load (armed by pc_autosave_request_load and not yet read; 0: none).
 * The hub's save card and PLAY name the autosave while it is armed for the current save, since that is what PLAY then
 * starts. pc_autosave_cancel_request disarms it (the hub's "Play this save" after "Play the autosave") */
int pc_autosave_armed(void);
void pc_autosave_cancel_request(void);
/* the slot whose autosave is in memory, waiting for Continue (0: none) */
int pc_autosave_loaded_now(void);
/* 1 when the game being played came from that slot's autosave written at `when` (PcAutosaveInfo.when), from Continue
 * until the next title read or save to the card: the hang watch then doesn't offer that same autosave back */
int pc_autosave_playing_from(int slot, const char* when);
/* how many autosaves were written this run (test logs) */
int pc_autosave_written(void);
/* start-up (pc_platform.c), after pc_pc_screen_start and before pc_save_windows_start */
void pc_autosave_start(void);

/* *Added 2026-11-20 (the save-check sweep, pc_savecheck.cpp; the project notes "The save-check
 * sweep"):* one save image (block: 0x1DFD0 bytes in this machine's byte order, as pc_game_ui.save_capture copies it)
 * written as a normal .gci plus an autosave-style .ini (the spot line, then `extra`), by the autosave's own background
 * writer and its own code, so the sweep's test saves are byte for byte what an autosave taken at that moment would be.
 * templateGci gives the card header and the other two slots (NULL or "": the current slot's own save file). Not an
 * autosave: no rotation, no autosave count, the slot's own files untouched. 1 queued, 0 not. Game thread; all platforms */
typedef struct PcAutosaveCopySpot {
    unsigned room, prev_room, entry;
    float x, y, z;
    int facing;
} PcAutosaveCopySpot;
int pc_autosave_write_copy(const unsigned char* block, const char* templateGci, const char* outGci, const char* outIni,
                           const PcAutosaveCopySpot* spot, const char* place, const char* trigger, const char* extra);

#ifdef __cplusplus
}
#endif

#endif /* PC_AUTOSAVE_H */
