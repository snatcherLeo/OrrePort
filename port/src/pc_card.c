/*
 * pc_card.c: the memory card at port startup (docs/port/saves.md).
 *
 * The game calls the SDK's CARDInit(void) (memcard.c); Aurora's PC CARDInit takes (game, maker) instead, so the
 * game's call would pass leftover registers. Aurora's CARDInit returns before reading its arguments once it has been
 * initialised, so the port calls it first, with the game's code and maker, and the game's later call does nothing
 * (checked in pc_card_test).
 *
 * Saves are Dolphin-style GCI folders (one .gci file per save, e.g. "USA/Card A/01-GC6E-<name>.gci"), so they can be
 * moved to and from Dolphin by copying files, in the port's user folder (Aurora's userPath, else the working
 * folder); not Dolphin's own folder, so the port never writes to a player's Dolphin saves by itself (decision: B,
 * 2026-10-15; importing from Dolphin is a later option).
 */
#include <aurora/card.h>
#include <dolphin/card.h>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h> /* SDL_LoadFile: the opened save's copy (rules R4 / R7) */
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_time.h>

#include <stdio.h>

#include "pc_paths.h" /* the game folder and its Saves\ (the portable and flat layouts, 4.1) */
#include "pc_saves.h" /* the save slots (step 2.1) */

extern void pc_os_post_call(void (*fn)(s32 result, void* block), s32 result, void* block);
extern void (*OrreHook_CardTaskStart)(s32 task_kind); /* orre_hooks.h: before every card task (rules R4 / R7) */
static void keep_opened_copy(const char* slot_dir); /* below, with the rules' code */
static void card_task_start(s32 task_kind);
/* the hook registered before this file's (chained; *added 2026-11-20*, the Battle Mode guard): the autosave
 * (pc_autosave.cpp) starts earlier and needs the hook too, so pc_card_open chains it instead of replacing it */
static void (*s_prev_task_start)(s32 task_kind) = NULL;

/* Card completion callbacks the GameCube way (aurora_card_set_callback_dispatcher): Aurora finishes a CARD...Async
 * request at once, but the game marks the request pending after the call returns and waits for the callback, as on
 * the GameCube where it comes later from the EXI interrupt. So each callback is posted to the game thread and runs
 * at its next interrupt point, after the call has returned (like the DVD callbacks, pc_dvd.c). */
typedef struct {
    AuroraCardCallback callback;
    s32 chan;
    s32 result;
} PcCardCall;

static PcCardCall s_calls[64];
static unsigned s_next;

static void run_card_call(s32 unused, void* block) {
    PcCardCall* call = block;
    (void)unused;
    call->callback(call->chan, call->result);
}

static void dispatch(AuroraCardCallback callback, s32 chan, s32 result) {
    PcCardCall* call = &s_calls[s_next++ % (sizeof(s_calls) / sizeof(s_calls[0]))];
    call->callback = callback;
    call->chan = chan;
    call->result = result;
    pc_os_post_call(run_card_call, 0, call);
}

void pc_card_timing_init(void); /* pc_card_timing.c: console-like card timing in replays and accurate mode */

/* The portable layout's saves (the reference playthrough Part 1 items 2a / 2b; snatcherLeo's folder names, 2026-10-31 / 11-01, 2026-11-01,
 * for B to review). When a "Colosseum" folder sits next to the exe (the portable build: Colosseum\ROM\ holds the disc),
 * the card is Colosseum\Saves\Slot 1\, the .gci files directly in it (snatcherLeo, 2026-11-01: "Colosseum\Saves\Slot 1\[GCI
 * file]", "since we'll eventually allow users to add more slots": each slot its own card, Slot 2 and on beside it;
 * Aurora's CARDSetFolderPath); otherwise it stays in user\USA\Card A\ (snatcherLeo's play build and every replay folder, so no
 * tool changes). The first run of the portable layout copies the .gci files from user\USA\Card A\ once (never moves or
 * deletes them). Every launch copies the slot's .gci files to Colosseum\Saves\Backups\<date_time>\Slot 1\ before the
 * game reads them; backups are never deleted by the program (pruning them is snatcherLeo's call). */
static int copy_gci_files(const char* from, const char* to) {
    char src[1400], dst[1400];
    char** files;
    int count = 0, copied = 0;
    files = SDL_GlobDirectory(from, "*.gci", 0, &count);
    if (files == NULL || count == 0) {
        SDL_free(files);
        return 0;
    }
    SDL_CreateDirectory(to);
    for (int i = 0; i < count; i++) {
        SDL_snprintf(src, sizeof(src), "%s%s", from, files[i]);
        SDL_snprintf(dst, sizeof(dst), "%s%s", to, files[i]);
        if (SDL_CopyFile(src, dst)) {
            copied++;
        } else {
            fprintf(stderr, "[card] could not copy %s to %s: %s\n", src, dst, SDL_GetError());
        }
    }
    SDL_free(files);
    return copied;
}

/* this launch's backup folder (the save this sitting started from, which its recording replays from), for Report a
 * problem (pc_report.cpp); "" when nothing was backed up */
static char s_launch_backup[1300];
const char* pc_card_launch_backup(void) { return s_launch_backup; }

/* *Updated 2026-11-13 (step 2.1, the Saves page; pc_saves.c):* several slots (Saves\Slot N\, the one Play starts
 * chosen in the hub), each with its own previous versions in Slot N\Backups\<date_time>\ (kept when the save changed,
 * no longer one copy per launch in a shared Saves\Backups\). Saves\ is registered first (pc_saves_init: setup files,
 * files put into Saves\ by hand, the old shared backups moved into Slot 1). Called from pc_card_init at start-up, so
 * the hub's Saves page sees every slot. */
static char s_saves_dir[1100];

static void portable_saves(void) {
    char slot1[1200], old[1200];
    int n;

    /* *Updated 2026-11-21 (4.1):* the one game-folder check (pc_paths.h), shared with the recordings and the clock, so
     * the three can never disagree. Flat layout: a Colosseum folder next to the exe, as before; portable layout
     * (exe in app\): always, in Pokemon Colosseum\Saves\. */
    if (!pc_paths_game_folder_exists()) {
        return; /* no game folder: Aurora's default, user\USA\Card A\ */
    }
    pc_paths_get(PC_PLACE_SAVES, "", s_saves_dir, sizeof(s_saves_dir));
    SDL_snprintf(slot1, sizeof(slot1), "%sSlot 1/", s_saves_dir);
    /* the legacy single card (user\USA\Card A\) exists only in the flat layout, so only the flat layout copies it
     * into Slot 1 (the plan, part 3a); the portable layout starts from its own Saves\ (or the migration tool's copy) */
    pc_paths_get(PC_PLACE_SETTINGS, "USA/Card A/", old, sizeof(old));
    if (!pc_paths_is_portable() && !SDL_GetPathInfo(slot1, NULL)) {
        n = copy_gci_files(old, slot1);
        if (n > 0) {
            printf("[card] copied %d save file(s) from %s to %s (the old files stay where they were)\n", n, old, slot1);
        }
    }
    pc_saves_init(s_saves_dir);
}

/* The card itself opens after the hub (pc_game_main.c), because the folder is fixed when it opens (Aurora's
 * CARDSetFolderPath refuses a change after CARDInit) and the hub's Saves page picks which slot Play starts. Automated
 * runs skip the hub, so they open at the same point in the start-up as before (likely: nothing between the old and
 * the new place reads the card). */
void pc_card_open(void) {
    if (s_saves_dir[0] != '\0') {
        const char* slot = pc_saves_open_current(s_launch_backup, sizeof(s_launch_backup));
        printf("[card] saves: %s (slot %d); the save this sitting starts from: %s\n", slot, pc_saves_current(),
               s_launch_backup[0] != '\0' ? s_launch_backup : "(none yet)");
        CARDSetFolderPath(slot, 0); /* the slot folder itself (Aurora, OrrePort addition) */
        keep_opened_copy(slot);     /* rules R4 / R7 */
        /* chained (*changed 2026-11-20*: it replaced the hook, which dropped the autosave's, registered earlier at
         * start-up; pc_autosave.cpp): once only, so a second call can't chain this file to itself */
        if (OrreHook_CardTaskStart != card_task_start) {
            s_prev_task_start = OrreHook_CardTaskStart;
            OrreHook_CardTaskStart = card_task_start;
        }
    }
    CARDInit("GC6E", "01"); /* GC6E01: Pokemon Colosseum (USA) */
}

/* Rules R4 / R7 (snatcherLeo approved, 2026-11-14; docs/port/title-load-save.md): the save file as OrrePort opened it this
 * session (at start-up or a pick in LOAD), kept in memory (about 400 KB), so a save file deleted from the Saves folder
 * while the game runs, its whole slot folder included, can be put back just before the game saves (the game itself
 * would say "Save failed!"). */
static void* s_opened_copy;
static size_t s_opened_len;

static void keep_opened_copy(const char* slot_dir) {
    char** files;
    int count = 0;
    SDL_free(s_opened_copy);
    s_opened_copy = NULL;
    s_opened_len = 0;
    files = SDL_GlobDirectory(slot_dir, "*.gci", SDL_GLOB_CASEINSENSITIVE, &count);
    if (files != NULL && count > 0) {
        char path[1400];
        SDL_snprintf(path, sizeof(path), "%s%s", slot_dir, files[0]);
        s_opened_copy = SDL_LoadFile(path, &s_opened_len);
    }
    SDL_free(files);
}

/* OrreHook_CardTaskStart (memcard.c): before a save (task 4) or the options save (task 7), a missing save file is put
 * back first. Every platform. */
static void card_task_start(s32 task_kind) {
    if (s_prev_task_start != NULL) s_prev_task_start(task_kind); /* the autosave's (its own save image first) */
    if (task_kind != 4 && task_kind != 7) return;
    if (pc_saves_put_back(pc_saves_current(), s_opened_copy, s_opened_len)) {
        aurora_card_remount(0); /* the card sees the file again before the task looks for it */
    }
}

/* The in-game save switch (the main menu's SAVE FILES list, plan step 7a, 2026-11-14): the picked slot becomes
 * the remembered one (settings.ini save_slot, as the hub's pick), its folder is prepared as a start-up prepares it
 * (pc_saves_open_current: the folder made, its Undo ended, a launch version kept when the save changed), and the card
 * re-opens on it (Aurora's aurora_card_switch_folder, an OrrePort addition). Refused (0, nothing changed: the old slot
 * stays remembered) outside the portable layout or while the game has a card file open. The game itself then reads
 * the save (pc_game_ui card_load_title). All platforms. */
int pc_card_switch(int slot) {
    char launch[sizeof(s_launch_backup)];
    const int before = pc_saves_current();
    const char* dir;
    if (s_saves_dir[0] == '\0' || slot <= 0) {
        return 0;
    }
    pc_saves_set_current(slot);
    dir = pc_saves_open_current(launch, sizeof(launch));
    if (!aurora_card_switch_folder(dir, 0)) {
        fprintf(stderr, "[card] could not switch to slot %d (%s): staying on slot %d\n", slot, dir, before);
        pc_saves_set_current(before);
        return 0;
    }
    SDL_snprintf(s_launch_backup, sizeof(s_launch_backup), "%s", launch);
    keep_opened_copy(dir); /* rules R4 / R7 */
    printf("[card] switched to slot %d: %s; the save it starts from: %s\n", slot, dir,
           launch[0] != '\0' ? launch : "(none yet)");
    return 1;
}

/* The card looks at the current slot's folder again (Main Menu on the PC screen, 2026-11-15): before the title's
 * card read re-reads the slot, so the read sees the folder as it is on disk now (the newest .gci, a file put back by
 * rule R4), not the card's view from the last open. The game must have no card file open (the title's main menu
 * doesn't). Returns 1 when the card re-opened. All platforms (Aurora's GCI-folder card). */
int pc_card_remount_current(void) { return aurora_card_remount(0) ? 1 : 0; }

void pc_card_init(void) {
    aurora_card_set_callback_dispatcher(dispatch);
    pc_card_timing_init();
    CARDSetLoadType(CARD_GCIFOLDER);
    portable_saves(); /* registers the slots; the card itself opens after the hub (pc_card_open) */
}
