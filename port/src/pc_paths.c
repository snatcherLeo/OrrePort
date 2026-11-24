/*
 * pc_paths.c: the two layouts' tables and the one "is there a game folder" check (pc_paths.h says why; plan
 * docs/port/portable-layout.md part 3a; Phase 4 step 4.1).
 *
 * Platforms: SDL only, so Windows, Linux and macOS share it (Windows verified). The one difference to come is the
 * macOS .app bundle (pc_paths.h): its program folder is inside the bundle, so init would take the OrrePort folder from
 * the bundle's parent instead of "app"'s parent (likely; to settle with the Mac port).
 */
#include "pc_paths.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h> /* getenv: ORRE_ROOT, read before SDL starts */
#include <string.h>

/* Where each place is: relative to the program folder (from_program = 1) or to the OrrePort folder. */
typedef struct PlaceName {
    int from_program;
    const char* rel;
} PlaceName;

/* The portable layout (exe in app\): the decided tree, the project notes section 8 (with the project's six calls of
 * 2026-11-21 for the places the first draft left open: the shader seed in app\, Aurora's controller and window files
 * and the control profiles in data\settings\, test-only and temporary files in data\system\).
 *
 * Capitals (snatcherLeo, 2026-11-22: "Yes, lowercase them"): the OrrePort folder's own folders are lowercase (the program's
 * things, rarely opened) and the player's are capitalized (the game's folder and what's inside it). Linux and macOS
 * (usually) tell capitals apart, so their builds must use these names exactly.
 *
 * Layout A (snatcherLeo, 2026-11-22: "Perhaps "app and data" is the best", then "We will build A"): OrrePort keeps exactly two
 * top folders, app\ (the program, replaced whole by updates, never written; was bin\) and data\ (its settings and its
 * rebuildable files; settings\ and system\ were top folders until then). Why those names: Explorer sorts folders
 * alphabetically and ignores capitals, so names starting with A and D always come before the player's folders (Mods,
 * OrreBank, the games, Translations in progress), however many the plan adds. */
static const PlaceName kPortable[PC_PLACE_COUNT] = {
    [PC_PLACE_PROGRAM] = {1, ""},
    [PC_PLACE_UI] = {1, "UI/"}, /* "UI" exactly: Linux won't find "ui" for it */
    [PC_PLACE_FONTS] = {1, "Fonts/"},
    [PC_PLACE_ROOT] = {0, ""},
    [PC_PLACE_SETTINGS] = {0, "data/settings/"},
    [PC_PLACE_SYSTEM] = {0, "data/system/"},
    [PC_PLACE_CACHE] = {0, "data/system/cache/"},
    [PC_PLACE_LOGS] = {0, "data/system/logs/"},
    [PC_PLACE_CHECKPOINTS] = {0, "data/system/checkpoints/"},
    [PC_PLACE_TEST] = {0, "data/system/"},
    [PC_PLACE_GAME] = {0, "Pokemon Colosseum/"},
    [PC_PLACE_ROM] = {0, "Pokemon Colosseum/ROM/"},
    [PC_PLACE_ROM_OTHER] = {0, "Pokemon Colosseum/ROM Other Regions/"},
    [PC_PLACE_SAVES] = {0, "Pokemon Colosseum/Saves/"},
    /* snatcherLeo, 2026-11-21: "I would prefer if Recordings wasn't in this folder ... shouldn't we just keep that in the same
     * location, where logs are": the player never opens them (Report a problem puts the sitting's recording in its zip),
     * so they sit in data\system\ beside the logs; the game's name leaves room for XD's */
    [PC_PLACE_RECORDINGS] = {0, "data/system/recordings/Pokemon Colosseum/"},
    [PC_PLACE_SCREENSHOTS] = {0, "Pokemon Colosseum/Screenshots/"},
    [PC_PLACE_REPORTS] = {0, "Pokemon Colosseum/Reports/"},
    /* snatcherLeo, 2026-11-21: the game's own folder (revisit for XD); named "Disc Art" (snatcherLeo, same day: "Should "Art" instead
     * be "Disc Art"?"), the hub's word for these pictures, so it isn't taken for the game's art or a texture pack */
    [PC_PLACE_ART] = {0, "Pokemon Colosseum/Disc Art/"},
    [PC_PLACE_TEXTURES] = {0, "Pokemon Colosseum/Textures/"}, /* snatcherLeo, 2026-11-21: "Put the textures subfolder" */
    [PC_PLACE_GAME_CACHE] = {0, "data/system/cache/Pokemon Colosseum/"}, /* snatcherLeo, 2026-11-21: out of the game folder */
};

/* The flat layout: exactly the names before 4.1, so every test tool, scratch folder and the frozen the reference playthrough tooling keep
 * working with a new exe. Do not rename these; they are the old layout by definition. */
static const PlaceName kFlat[PC_PLACE_COUNT] = {
    [PC_PLACE_PROGRAM] = {1, ""},
    [PC_PLACE_UI] = {1, "ui/"}, /* firstlink.py and the test tools copy port/ui as "ui" */
    [PC_PLACE_FONTS] = {1, "Fonts/"},
    [PC_PLACE_ROOT] = {0, ""},
    [PC_PLACE_SETTINGS] = {0, "user/"},
    [PC_PLACE_SYSTEM] = {0, "user/"},
    [PC_PLACE_CACHE] = {0, "cache/"},
    [PC_PLACE_LOGS] = {0, "logs/"},
    [PC_PLACE_CHECKPOINTS] = {0, "checkpoints/"},
    [PC_PLACE_TEST] = {0, ""},
    [PC_PLACE_GAME] = {0, "Colosseum/"},
    [PC_PLACE_ROM] = {0, "Colosseum/ROM/"},
    [PC_PLACE_ROM_OTHER] = {0, "Colosseum/ROM Other Regions/"},
    [PC_PLACE_SAVES] = {0, "Colosseum/Saves/"},
    [PC_PLACE_RECORDINGS] = {0, "Colosseum/Recordings/"},
    [PC_PLACE_SCREENSHOTS] = {0, "screenshots/"},
    [PC_PLACE_REPORTS] = {0, "Reports/"},
    [PC_PLACE_ART] = {0, "Disc labels/"},
    [PC_PLACE_TEXTURES] = {0, "user/textures/GC6E01/"},
    [PC_PLACE_GAME_CACHE] = {0, "Colosseum/"},
};

static int s_ready;
static int s_portable;
static int s_root_override; /* ORRE_ROOT is set (below) */
static char s_program[1024]; /* with a trailing separator */
static char s_root[1024];

/* Worked out once, on the first call. Why lazily: the run log (pc_log.c) opens before the platform starts, and
 * SDL_GetBasePath works before SDL_Init. The first call is on the main thread at start-up (the log), before any other
 * thread exists, so no lock is needed. */
static void init(void) {
    const char* base;
    size_t n;
    if (s_ready) {
        return;
    }
    base = SDL_GetBasePath(); /* SDL's own string, ends in the OS's separator */
    SDL_strlcpy(s_program, base != NULL ? base : "./", sizeof(s_program));
    SDL_strlcpy(s_root, s_program, sizeof(s_root));
    n = strlen(s_root);
    /* the last folder's name, without the trailing separator: "app" (any capitals) means the portable layout. Only "app"
     * since layout A (2026-11-22; "bin" before): no tester folder existed yet and snatcherLeo's was renamed in the same step,
     * so there is no older portable folder to keep working (a folder still named bin\ starts in the flat layout). */
    if (n >= 5) {
        const char* end = s_root + n - 1; /* the trailing separator */
        const char* name = end - 3;
        if ((*end == '/' || *end == '\\') && (name[-1] == '/' || name[-1] == '\\') && SDL_strncasecmp(name, "app", 3) == 0) {
            s_portable = 1;
            s_root[name - s_root] = '\0'; /* keeps the separator before "app" */
        }
    }
    {
        /* ORRE_ROOT=<folder> (a test switch, never a setting, 2026-11-24, snatcherLeo: "When I wake up, we can do this
         * first thing"): the OrrePort folder (saves, settings, logs, the ROM folders, everything not the program's own)
         * is that folder, while the program, its UI and fonts stay where the exe is. Why: every automated run used to
         * copy the ~280 MB exe into its own fresh folder, because the data was found beside the exe. Those copies were
         * most of the 466 GB the 5.5 clean house found, and each new exe location made Windows ask about the firewall
         * when a run used RenderDoc's capture. Now one program (frozen once per sweep) serves every run, each with its
         * own data folder. The layout (portable or flat) still follows where the exe is, so the test tools' flat
         * folders (Colosseum\, user\, logs\) keep their names. getenv, not SDL_getenv: this runs before SDL_Init (the
         * log opens first). Every OS the same (a path is a path); a Windows folder may be given with either slash. */
        const char* over = getenv("ORRE_ROOT");
        if (over != NULL && over[0] != '\0') {
            size_t k;
            SDL_strlcpy(s_root, over, sizeof(s_root) - 1);
            k = strlen(s_root);
            if (s_root[k - 1] != '/' && s_root[k - 1] != '\\') {
                s_root[k] = '/';
                s_root[k + 1] = '\0';
            }
            s_root_override = 1;
        }
    }
    s_ready = 1;
}

const char* pc_paths_get(PcPlace place, const char* name, char* out, size_t size) {
    const PlaceName* p;
    if (out == NULL || size == 0) {
        return "";
    }
    init();
    if ((int)place < 0 || place >= PC_PLACE_COUNT) {
        place = PC_PLACE_ROOT;
    }
    p = s_portable ? &kPortable[place] : &kFlat[place];
    SDL_snprintf(out, size, "%s%s%s", p->from_program ? s_program : s_root, p->rel, name != NULL ? name : "");
    return out;
}

const char* pc_paths_dir(PcPlace place, char* out, size_t size) {
    size_t n;
    pc_paths_get(place, "", out, size);
    n = strlen(out);
    if (n > 1 && (out[n - 1] == '/' || out[n - 1] == '\\')) {
        out[n - 1] = '\0';
    }
    return out;
}

int pc_paths_is_portable(void) {
    init();
    return s_portable;
}

const char* pc_paths_layout_name(void) { return pc_paths_is_portable() ? "portable" : "flat"; }

int pc_paths_game_folder_exists(void) {
    char folder[1200];
    SDL_PathInfo info;
    pc_paths_get(PC_PLACE_GAME, "", folder, sizeof(folder));
    if (s_portable) {
        /* the portable layout always has its game folder (a fresh one is made here), so live play always records and
         * runs on the replay clock: the answer must never depend on whether a folder happened to exist */
        SDL_CreateDirectory(folder);
        return 1;
    }
    return SDL_GetPathInfo(folder, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

void pc_paths_log(void) {
    init();
    printf("[paths] %s layout: program %s, OrrePort folder %s%s\n", pc_paths_layout_name(), s_program, s_root,
           s_root_override ? " (ORRE_ROOT, a test run's own folder)" : "");
}
