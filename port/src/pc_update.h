/*
 * pc_update.h: the updater both programs share (2026-11-23; docs/port/updater-plan.md).
 *
 * Why: one update covers OrrePort and Studio "no matter what" (snatcherLeo, 2026-11-15), so the check, the download, the
 * signature check, the unpacking and the shared state live in one place both programs compile, like pc_zip.c. Each
 * program only draws its own screens on top (the hub's note and panel, Studio's dot, Home line and About) and asks the
 * launcher to swap the new app\ in (pc_update_restart_code).
 *
 * The rules (snatcherLeo): the check runs by itself on start, unless the player turned checking off ("stay fully offline");
 * nothing downloads without the player pressing Download (2026-11-23: "we should always ask first"), unless they turned
 * on "Download updates automatically" (off by default; the same day: "let's do it for both hub and studio"); nothing is
 * installed until a restart; saves, mods and settings are never touched.
 *
 * Shared with Studio: plain C, no SDL and no app types. Paths are UTF-8. Everything slow runs on the module's own thread;
 * the calls below return at once. The state is shared through files in <OrrePort folder>\data\system\update\, so what
 * one program finds or downloads, the other shows (pc_update_poll).
 *
 * Platforms: Windows: WinHTTP for the requests, a Win32 thread; Linux / macOS: libcurl or the system's own, pthreads
 * (not written yet; likely). The signature: Ed25519 (Monocypher), the same everywhere.
 *
 * STATUS: the interface is fixed (Studio builds on it); the module is being written.
 */
#ifndef PC_UPDATE_H
#define PC_UPDATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcUpdateState {
    PC_UPDATE_IDLE = 0,       /* not checked yet this start */
    PC_UPDATE_OFF,            /* checking is turned off ("stay fully offline"): nothing is ever sent */
    PC_UPDATE_CHECKING,       /* asking GitHub */
    PC_UPDATE_UP_TO_DATE,     /* this is the newest version */
    PC_UPDATE_AVAILABLE,      /* a newer version exists: show the note; Download starts it */
    PC_UPDATE_DOWNLOADING,    /* downloading (done / size); by this program or the other */
    PC_UPDATE_READY,          /* downloaded, checked and unpacked: restart to apply */
    PC_UPDATE_FAILED          /* the last check or download failed (error_key says why); tried again next start */
} PcUpdateState;

typedef struct PcUpdateStatus {
    PcUpdateState state;
    char current[32];      /* this build's version ("0.1.1") */
    char latest[32];       /* the newest version found ("" when none) */
    char notes[4096];      /* what's new, in the player's language when the release has it, else English (UTF-8) */
    uint64_t size;         /* the download's size in bytes */
    uint64_t done;         /* bytes downloaded so far */
    char error_key[64];    /* an update.* language key saying why it failed ("" otherwise) */
    int other_running;     /* 1: the other program from this folder is open, so the swap has to wait for it */
    char previous[32];     /* the version kept for "Go back" ("" when none) */
} PcUpdateStatus;

/* Once at start. root: the OrrePort folder (the one holding app\ and data\); version: this build's (from VERSION);
 * lang: the player's language code ("en", "de" ...), for the notes. Returns 0 when the folder isn't usable (then every
 * call below does nothing and poll reports PC_UPDATE_OFF). */
int pc_update_init(const char* root, const char* version, const char* lang);

/* Checks GitHub in the background, once per start, and not again for a few hours when the other program just did.
 * allowed: 0 when the player turned checking off (OrrePort's settings.ini update_check = 0); then nothing is sent.
 * 2: the player pressed "Check for updates": checks now, even if the other program checked a moment ago. */
void pc_update_check(int allowed);

/* "Download updates automatically" (settings.ini update_auto_download, off by default; snatcherLeo, 2026-11-23): on, a newer
 * version found by pc_update_check downloads by itself, as if Download had been pressed. Call before pc_update_check
 * (and again when the player flips the switch). Checking turned off wins: nothing is sent at all. */
void pc_update_set_auto_download(int on);

/* The player pressed Download: starts it in the background (only one program downloads at a time; the other follows
 * the shared state). Does nothing unless the state is AVAILABLE or FAILED. */
void pc_update_download(void);

/* Slows the download while a game is being played, so it never stutters (OrrePort calls it at PLAY and back in the
 * hub; Studio never needs it). */
void pc_update_set_playing(int playing);

/* The current state, read from the shared files (cheap: re-read only when they changed). Returns 1 when something
 * changed since the last call, so a screen redraws only then. */
int pc_update_poll(PcUpdateStatus* out);

/* "Restart now" (or "Go back"): the program saves what it needs, then exits with this code; its launcher swaps app\
 * when no program from the folder is running, and starts the program again. */
#define PC_UPDATE_EXIT_RESTART 0x55504431 /* "UPD1" */
/* "Go back to <previous>": marks the rollback; the program then exits with PC_UPDATE_EXIT_RESTART. 0 when there is
 * nothing to go back to. */
int pc_update_request_rollback(void);

/* At exit: stops the module's thread (a download in progress keeps its .part and resumes next time). */
void pc_update_shutdown(void);

/* Just before exiting with PC_UPDATE_EXIT_RESTART (2026-11-24, the laptop sitting: after the US disc's Remove the
 * restarted hub came back behind other windows): lets the next program take the foreground. Windows gives the
 * foreground only to the program in front or one it allows; the launcher that starts the new copy is never in front,
 * so the closing program, which is, allows any program (AllowSetForegroundWindow(ASFW_ANY); it lapses at the next
 * input). The new copy then brings its window forward as it uncloaks it (pc_window_icon.cpp). Windows only; Linux and
 * macOS window managers have no such lock (likely: nothing to do). */
void pc_update_pass_foreground(void);

#ifdef __cplusplus
}
#endif

#endif
