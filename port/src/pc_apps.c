/*
 * pc_apps.c: how OrrePort and OrrePort Studio find, show, start and close each other (pc_apps.h says what for,
 * 2026-11-23, Studio's design approved by snatcherLeo).
 *
 * How (Windows):
 *  - every name carries the OrrePort folder (Studio's request, 2026-11-23): "Local\OrrePort.<what>.<16 hex digits>",
 *    the digits a hash (64-bit FNV-1a) of the folder written one way: lower-case, backslashes, no trailing slash. So a
 *    development build and the player's folder never block or see each other, and both programs in one folder always
 *    build the same names. "Local\" is this Windows session: two users signed in at once each have their own.
 *  - "open" is a named mutex per program (OrrePort.Hub.*, OrrePort.Studio.*), created by pc_apps_claim and held until
 *    the process ends; Windows drops it by itself when the process ends, even in a crash, so a stale "open" can't
 *    outlive the program (why a mutex, not a file).
 *  - the window: each program writes its HWND and process id into a small named shared-memory block
 *    (OrrePort.HubWindow.*, ...) that it keeps open; the other reads it. Why not search windows by class name: the hub's
 *    window is SDL's, whose class "SDL_app" every SDL program shares. The id is checked against the window before
 *    anything is done to it, so a reused HWND can't be hit.
 *  - "the game is running": a third mutex (OrrePort.Game.*), created when the hub hands over to the game. The hub and
 *    the game are one process (orrecolo.exe), so this is the only way to tell them apart from outside.
 *  - show: restore if minimised, then SetForegroundWindow. Windows allows that only from the program in front (the one
 *    just clicked, or just started from a shortcut), which is when this is called. AllowSetForegroundWindow on the
 *    other's id also lets it bring itself forward if it chooses to.
 *  - close: WM_CLOSE to the window, the same message the title bar's X sends, so the program closes its usual way.
 *    There is a moment between "is the game running?" and the close where a game could start; the hub only starts a
 *    game on PLAY, so it only matters if the player presses PLAY in exactly that instant (accepted; guess: very
 *    unlikely).
 * Linux and macOS: stubs (pc_apps.h); likely a lock file per program in the folder and the window manager's activation.
 */
#include "pc_apps.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN /* Studio's build defines it on the command line: no "redefined" warning there */
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>

static const wchar_t* const kOpen[2] = {L"Hub", L"Studio"};
static const wchar_t* const kWindow[2] = {L"HubWindow", L"StudioWindow"};
/* the launchers in the OrrePort folder (port/launcher, built by build_launcher.py) */
static const wchar_t* const kLauncher[2] = {L"OrrePort.exe", L"OrrePort Studio.exe"};

typedef struct WindowSlot {
    unsigned long long hwnd; /* 64 bits whatever the build, so a 32-bit and a 64-bit program could read each other */
    unsigned long pid;
} WindowSlot;

static HANDLE s_claim[2];
static HANDLE s_window_map[2];
static HANDLE s_game;

static int valid(PcApp app) { return app == PC_APP_HUB || app == PC_APP_STUDIO; }

/* "Local\OrrePort.<what>.<hash of the folder>"; the folder written one way first (pc_apps.h: any case, either slash,
 * with or without the trailing one). Bytes outside ASCII are hashed as they are (UTF-8): both programs get the folder
 * from Windows the same way, so they match */
static void make_name(wchar_t* out, size_t n, const wchar_t* what, const char* root) {
    unsigned long long h = 1469598103934665603ull;
    size_t len = root != NULL ? strlen(root) : 0;
    while (len > 0 && (root[len - 1] == '\\' || root[len - 1] == '/')) --len;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)root[i];
        if (c == '/') c = '\\';
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        h = (h ^ c) * 1099511628211ull;
    }
    _snwprintf(out, n, L"Local\\OrrePort.%ls.%016llx", what, h);
    out[n - 1] = 0;
}

static int mutex_exists(const wchar_t* name) {
    HANDLE h = OpenMutexW(SYNCHRONIZE, FALSE, name);
    if (h == NULL) return 0;
    CloseHandle(h);
    return 1;
}

int pc_apps_claim(PcApp self, const char* root) {
    if (!valid(self) || s_claim[self] != NULL) return 1;
    wchar_t name[96];
    make_name(name, 96, kOpen[self], root);
    HANDLE h = CreateMutexW(NULL, FALSE, name);
    if (h == NULL) return 1; /* can't tell: let it run rather than refuse to start */
    const int first = GetLastError() != ERROR_ALREADY_EXISTS;
    /* held even when another copy had it: a copy that steps aside exits at once (dropping it with the process), and
     * OrrePort's Quit to hub runs the new process beside the old one for a moment, whose marker must outlive the old */
    s_claim[self] = h;
    return first;
}

void pc_apps_set_window(PcApp self, const char* root, void* window) {
    if (!valid(self)) return;
    if (s_window_map[self] == NULL) {
        wchar_t name[96];
        make_name(name, 96, kWindow[self], root);
        s_window_map[self] = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(WindowSlot), name);
        if (s_window_map[self] == NULL) return;
    }
    WindowSlot* slot = (WindowSlot*)MapViewOfFile(s_window_map[self], FILE_MAP_WRITE, 0, 0, sizeof(WindowSlot));
    if (slot == NULL) return;
    slot->hwnd = (unsigned long long)(uintptr_t)window;
    slot->pid = GetCurrentProcessId();
    UnmapViewOfFile(slot);
}

/* the other program's window, checked: still a window, and still owned by the process that published it */
static HWND find_window(PcApp app, const char* root, DWORD* pid_out) {
    wchar_t name[96];
    make_name(name, 96, kWindow[app], root);
    HANDLE map = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
    if (map == NULL) return NULL;
    HWND hwnd = NULL;
    const WindowSlot* slot = (const WindowSlot*)MapViewOfFile(map, FILE_MAP_READ, 0, 0, sizeof(WindowSlot));
    if (slot != NULL) {
        HWND h = (HWND)(uintptr_t)slot->hwnd;
        DWORD owner = 0;
        if (h != NULL && IsWindow(h) && GetWindowThreadProcessId(h, &owner) != 0 && owner == slot->pid) {
            hwnd = h;
            if (pid_out != NULL) *pid_out = owner;
        }
        UnmapViewOfFile(slot);
    }
    CloseHandle(map);
    return hwnd;
}

int pc_apps_running(PcApp app, const char* root) {
    if (!valid(app)) return 0;
    wchar_t name[96];
    make_name(name, 96, kOpen[app], root);
    return mutex_exists(name);
}

int pc_apps_show(PcApp app, const char* root) {
    if (!valid(app)) return 0;
    DWORD pid = 0;
    HWND h = find_window(app, root, &pid);
    if (h == NULL) return 0;
    AllowSetForegroundWindow(pid);
    if (IsIconic(h)) ShowWindow(h, SW_RESTORE);
    else ShowWindow(h, SW_SHOW);
    SetForegroundWindow(h);
    return 1;
}

int pc_apps_start(PcApp app, const char* root) {
    if (!valid(app) || root == NULL || root[0] == '\0') return 0;
    wchar_t wroot[1100], path[1300];
    if (MultiByteToWideChar(CP_UTF8, 0, root, -1, wroot, (int)(sizeof(wroot) / sizeof(wroot[0]))) == 0) return 0;
    size_t n = wcslen(wroot);
    if (n > 0 && wroot[n - 1] != L'\\' && wroot[n - 1] != L'/') {
        if (n + 1 >= sizeof(wroot) / sizeof(wroot[0])) return 0;
        wroot[n++] = L'\\';
        wroot[n] = 0;
    }
    _snwprintf(path, sizeof(path) / sizeof(path[0]), L"%ls%ls", wroot, kLauncher[app]);
    path[sizeof(path) / sizeof(path[0]) - 1] = 0;
    if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES) return 0; /* a development build: no launchers beside it */
    /* ShellExecute rather than CreateProcess: started as from Explorer, in its own folder, not tied to this process
     * (it keeps running when this one closes) */
    HINSTANCE r = ShellExecuteW(NULL, L"open", path, NULL, wroot, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

int pc_apps_close(PcApp app, const char* root) {
    if (!valid(app) || !pc_apps_running(app, root)) return PC_APPS_CLOSE_NOT_OPEN;
    if (app == PC_APP_HUB && pc_apps_game_running(root)) return PC_APPS_CLOSE_GAME_RUNS;
    HWND h = find_window(app, root, NULL);
    if (h == NULL) return PC_APPS_CLOSE_NOT_OPEN;
    PostMessageW(h, WM_CLOSE, 0, 0);
    return PC_APPS_CLOSE_ASKED;
}

void pc_apps_game_begin(const char* root) {
    if (s_game != NULL) return;
    wchar_t name[96];
    make_name(name, 96, L"Game", root);
    s_game = CreateMutexW(NULL, FALSE, name);
}

int pc_apps_game_running(const char* root) {
    wchar_t name[96];
    make_name(name, 96, L"Game", root);
    return mutex_exists(name);
}

#else /* Linux, macOS: not yet (pc_apps.h) */

int pc_apps_claim(PcApp self, const char* root) {
    (void)self;
    (void)root;
    return 1;
}
void pc_apps_set_window(PcApp self, const char* root, void* window) {
    (void)self;
    (void)root;
    (void)window;
}
int pc_apps_running(PcApp app, const char* root) {
    (void)app;
    (void)root;
    return 0;
}
int pc_apps_show(PcApp app, const char* root) {
    (void)app;
    (void)root;
    return 0;
}
int pc_apps_start(PcApp app, const char* root) {
    (void)app;
    (void)root;
    return 0;
}
int pc_apps_close(PcApp app, const char* root) {
    (void)app;
    (void)root;
    return PC_APPS_CLOSE_NOT_OPEN;
}
void pc_apps_game_begin(const char* root) { (void)root; }
int pc_apps_game_running(const char* root) {
    (void)root;
    return 0;
}

#endif
