/*
 * pc_paths.h: where every OrrePort file lives, in one table (Phase 4 step 4.1, the portable layout; plan
 * docs/port/portable-layout.md part 3a, snatcherLeo's six approvals 2026-11-21).
 *
 * Why one place: before 4.1, about 30 call sites each built "<exe folder>/ui/...", "<exe folder>/Colosseum/..." or
 * "<exe folder>/logs/" by hand, and three of them each decided on their own whether the folder was "the portable
 * layout" (a Colosseum folder next to the exe). Moving the files means changing one table here instead of 30 places,
 * and the three decisions become one (pc_paths_game_folder_exists), so live play can't silently fall off the replay
 * clock (pc_vi.c) when a folder is renamed.
 *
 * Two layouts, both tables of names (not two code paths):
 * - the PORTABLE layout, used when the exe sits in a folder named "app" (any capitals; "bin" until 2026-11-22): the
 *   program files stay in app\ (never written), and the player's and the PC's files sit beside it in the OrrePort
 *   folder: data\settings\, data\system\ (logs, caches, checkpoints, recordings, temporary files), Pokemon Colosseum\
 *   (ROM, saves, screenshots, reports, disc art, textures). The decided tree: the project notes section 8.
 *   OrrePort's own folders are lowercase and come first (app, data); the player's are capitalized (snatcherLeo, 2026-11-22,
 *   layout A: pc_paths.c says why).
 * - the FLAT layout, everything else: exactly the names used before 4.1 (user\, cache\, logs\, Colosseum\, ...), so the
 *   test tools' scratch folders, the the reference playthrough replays and the frozen the reference playthrough exe's tooling keep working unchanged.
 *
 * Platforms: plain C over SDL (SDL_GetBasePath); the same on Windows, Linux and macOS (Windows verified; Linux likely:
 * the names below are written with exact capitals because Linux file names are case-sensitive; macOS likely needs
 * one change: in an .app bundle SDL_GetBasePath is Contents/Resources/, so the Mac port sets the program folder from
 * the bundle and the OrrePort folder from the bundle's parent, in pc_paths.c's init).
 */
#ifndef PC_PATHS_H
#define PC_PATHS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The places. Each is a folder, given with a trailing '/'. */
typedef enum PcPlace {
    PC_PLACE_PROGRAM,     /* the exe's own folder (app\ in the portable layout): program files, read-only */
    PC_PLACE_UI,          /* the hub's pages, pictures, game tables and translations: <program>/UI (flat: ui) */
    PC_PLACE_FONTS,       /* every shipped font: <program>/Fonts */
    PC_PLACE_ROOT,        /* the OrrePort folder (the player opens this one): app's parent, or the exe's folder */
    PC_PLACE_SETTINGS,    /* settings.ini, Aurora's controller and window files, control profiles (flat: user) */
    PC_PLACE_SYSTEM,      /* small temporary files: the write probe, Quit to hub's ready file (flat: user) */
    PC_PLACE_CACHE,       /* GPU caches, Aurora's cachePath */
    PC_PLACE_LOGS,        /* run logs, performance logs, the Cheats log, the adapter setup logs */
    PC_PLACE_CHECKPOINTS, /* the debug checkpoints */
    PC_PLACE_TEST,        /* test-only files written by switches (gcopy_test.gci, spots.json; flat: the exe's folder) */
    PC_PLACE_GAME,        /* the game's folder: Pokemon Colosseum (flat: Colosseum) */
    PC_PLACE_ROM,         /* the disc, found there by its game ID */
    PC_PLACE_ROM_OTHER,   /* the language discs (European, Japanese) */
    PC_PLACE_SAVES,       /* the save slots, Saves/Slot N/ */
    PC_PLACE_RECORDINGS,  /* the sittings' input recordings: data/system/recordings/Pokemon Colosseum (since 2026-11-21;
                           * flat: Colosseum/Recordings) */
    PC_PLACE_SCREENSHOTS, /* F12 screenshots and frame dumps */
    PC_PLACE_REPORTS,     /* Report a problem's zips */
    PC_PLACE_ART,         /* the player's disc art (flat: Disc labels) */
    PC_PLACE_TEXTURES,    /* texture packs (flat: user/textures/GC6E01) */
    PC_PLACE_GAME_CACHE,  /* the game's own rebuildable files (the disc check's memory): data/system/cache/Pokemon Colosseum
                           * (flat: Colosseum, where it always was). Why its own place (snatcherLeo, 2026-11-21: "why is
                           * disc-check.txt in here? Should it be?"): the game folder holds only what the player keeps
                           * (saves, recordings, screenshots, disc art); a file the game remakes on its own belongs in
                           * data\system\, and the game's name in the path leaves room for XD's beside it. */
    PC_PLACE_COUNT
} PcPlace;

/* <place>/<name> into out (name may be "" for the folder itself, with its trailing '/'); returns out. Works before
 * anything else starts (the log opens first): the layout is worked out on the first call. */
const char* pc_paths_get(PcPlace place, const char* name, char* out, size_t size);

/* The place's folder without its trailing '/', for older callers that add "/<file>" themselves; returns out */
const char* pc_paths_dir(PcPlace place, char* out, size_t size);

/* 1 when the exe sits in app\ (the portable layout), 0 for the flat layout */
int pc_paths_is_portable(void);

/* "portable" or "flat", for the log */
const char* pc_paths_layout_name(void);

/* Is there a game folder? The one answer for the three behaviours that used to test "a Colosseum folder next to the
 * exe" on their own: save slots instead of Aurora's single card (pc_card.c), sitting recordings (pc_pad_record.c) and
 * live play on the replay clock (pc_vi.c). Flat layout: whether Colosseum/ exists, as before 4.1. Portable layout:
 * always 1, and the folder is made if missing, so live play always records and always runs on the replay clock (as
 * snatcherLeo's play build has since 2026-11-01). */
int pc_paths_game_folder_exists(void);

/* One line naming the layout and its two roots, for the run log */
void pc_paths_log(void);

#ifdef __cplusplus
}
#include <string>
/* C++ callers: the same as pc_paths_get, as a std::string */
inline std::string pc_paths_str(PcPlace place, const char* name = "") {
    char buf[1400];
    return pc_paths_get(place, name, buf, sizeof(buf));
}
#endif

#endif
