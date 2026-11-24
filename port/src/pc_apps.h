/*
 * pc_apps.h: how OrrePort and OrrePort Studio find, show, start and close each other (step 4.4's last piece,
 * 2026-11-23, from Studio's design that snatcherLeo approved: "Yes, makes sense. Send this to Main to see if it agrees").
 *
 * What it is for:
 *  - one copy of each program: a second start brings the open window forward instead (pc_apps_claim, pc_apps_show);
 *  - the links between them: the hub's "Open Studio" and Studio's "Open OrrePort" start the other program, or show it
 *    when it is already open (pc_apps_running, pc_apps_show, pc_apps_start);
 *  - finishing an update while the other is open: "Close Studio" / "Close OrrePort" asks it to close as if its user had
 *    (pc_apps_close), never while the game runs (pc_apps_game_running).
 *
 * Everything is per OrrePort folder (Studio's request, 2026-11-23): `root` is the OrrePort folder (UTF-8; the one
 * holding "OrrePort.exe" and "OrrePort Studio.exe"; with or without a trailing slash, any case, either slash), so a
 * development build and the player's folder, or two OrrePort folders side by side, never see each other as "open".
 * That is also what the update swap cares about: only programs from the same folder hold its app\ files. Both
 * programs must pass the same folder: the hub passes pc_paths' PC_PLACE_ROOT.
 *
 * Shared, like pc_zip.c: plain C, no SDL and no OrrePort types, so Studio compiles this file as it is.
 * Platforms: Windows has it all (named mutexes and a small named shared-memory block per program and folder, the window
 * found through it). Linux and macOS: every call answers "not running" / does nothing for now, and pc_apps_claim always
 * succeeds; they would use a lock file per program in the folder and the window manager's "activate" request (likely).
 */
#ifndef PC_APPS_H
#define PC_APPS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PcApp {
    PC_APP_HUB = 0,    /* OrrePort (orrecolo.exe: the hub, then the game, in one process) */
    PC_APP_STUDIO = 1, /* OrrePort Studio */
} PcApp;

/* Marks "this program is open from root", held until the process ends; call once, at the start (test runs may skip
 * it: every other call works without it). 1: the first copy; 0: another copy already had it (then call
 * pc_apps_show(self, root) and exit, unless it is a hand-over: OrrePort's Quit to hub keeps its own per-folder rule,
 * pc_single_instance.c, and only uses this as the marker Studio reads). */
int pc_apps_claim(PcApp self, const char* root);

/* Publishes self's main window (an HWND on Windows), so the other program and a second copy can show or close it.
 * Call once the window exists (and again if it is ever recreated). */
void pc_apps_set_window(PcApp self, const char* root, void* window);

/* 1 while app is open from root */
int pc_apps_running(PcApp app, const char* root);

/* Brings app's window forward (restored first when minimised). 1 when it was found. Windows lets a program bring
 * another forward only while it is in front itself, which is true right after the click on the link or a fresh start
 * from a shortcut. */
int pc_apps_show(PcApp app, const char* root);

/* Starts app from root's launcher ("OrrePort.exe" / "OrrePort Studio.exe"). 1 when started; 0 when the launcher isn't
 * there (a development build) or Windows refused. */
int pc_apps_start(PcApp app, const char* root);

/* pc_apps_close's answers */
#define PC_APPS_CLOSE_ASKED 1        /* the close was asked for (the program closes its usual way, saving first) */
#define PC_APPS_CLOSE_NOT_OPEN 0     /* app isn't open from root (or its window isn't published) */
#define PC_APPS_CLOSE_GAME_RUNS (-1) /* refused: app is OrrePort and its game is running (nothing is closed) */

/* Asks app to close as if its user had closed its window (Studio saves its tabs; the hub saves its settings). One of
 * the PC_APPS_CLOSE_* values above. */
int pc_apps_close(PcApp app, const char* root);

/* The hub calls this as it hands over to the game: from then until the process ends, the game is running (the hub and
 * the game share one process, so this is how Studio tells them apart and never closes a game). */
void pc_apps_game_begin(const char* root);

/* 1 while an OrrePort game from root is running */
int pc_apps_game_running(const char* root);

#ifdef __cplusplus
}
#endif

#endif
