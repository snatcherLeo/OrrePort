/*
 * pc_single_instance.c: only one OrrePort at a time per install (rule R8; the project plan's decided edge case 7: "a
 * second launch should bring the first window to the front"; snatcherLeo approved the rules, 2026-11-14).
 *
 * Why: two OrrePorts on one install would share one Saves folder and one settings.ini; both could write the same save
 * (the second's in-game save over the first's), the remembered save would flip between them, and the backups would
 * interleave. So a second launch hands over to the first and exits before it touches anything.
 *
 * How: a lock named after the install folder (*since 2026-11-24* the OrrePort folder, so ORRE_ROOT test runs sharing
 * one exe each have their own; before, the exe's folder), so separate installs (and every automated test folder,
 * each with its own exe) never block each other.
 *   - Windows: a named mutex "Local\OrrePort-<hash of the folder>" (CreateMutexW; the system frees it when the process
 *     ends, even after a crash). When it already exists, the first instance's window (a top-level window of a process
 *     running the same exe file, found with EnumWindows and QueryFullProcessImageNameW) is restored and brought to the
 *     front, and this one exits.
 *   - Linux and macOS (likely, not built or run yet): a lock on a file in the install folder (fcntl F_SETLK; the system
 *     drops it when the process ends); a second launch exits with a log line. Bringing the first window forward there
 *     needs the window system's own call (X11 / Wayland activation, NSRunningApplication activate): to add with those
 *     ports.
 * Platforms: the Windows part is Windows-only (#ifdef _WIN32); the rest is POSIX.
 */
#include <SDL3/SDL.h>

#include "pc_paths.h" /* the OrrePort folder the lock is named after (2026-11-24) */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static wchar_t s_self[MAX_PATH]; /* this exe's full path, to recognise the first instance's process */

static BOOL CALLBACK find_first(HWND hwnd, LPARAM found) {
    DWORD pid = 0;
    if (!IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != NULL) return TRUE; /* top-level windows only */
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) return TRUE;
    HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (proc == NULL) return TRUE;
    wchar_t path[MAX_PATH];
    DWORD len = MAX_PATH;
    const BOOL ok = QueryFullProcessImageNameW(proc, 0, path, &len);
    CloseHandle(proc);
    if (ok && _wcsicmp(path, s_self) == 0) {
        *(HWND*)found = hwnd;
        return FALSE; /* stop: the first OrrePort's window */
    }
    return TRUE;
}
#else
#include <fcntl.h>
#include <unistd.h>

/* a hand-over on Linux / macOS: the lock taken once the old OrrePort has ended (it holds the lock until then) */
static int s_lock_fd = -1;
static int SDLCALL take_lock_later(void* unused) {
    (void)unused;
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    for (int i = 0; i < 600; ++i) { /* up to a minute */
        if (fcntl(s_lock_fd, F_SETLK, &fl) == 0) return 1;
        SDL_Delay(100);
    }
    return 0;
}
#endif

/* A handover (Quit to hub, which starts a new OrrePort and then ends this one; pc_platform_restart_to_hub sets
 * ORRE_HANDOVER=1 for the new one): the new OrrePort never steps aside for the old one. *Fixed 2026-11-14 (snatcherLeo: "the
 * app is not running anymore"):* the new one had seen the old one still open, brought its window forward and exited,
 * leaving nothing running once the old one ended; then it waited (up to 15 s) for the old one's lock. *Changed
 * 2026-11-15:* it goes on at once (the old one now waits for the new one's hub; below). */

static int handover(void) {
    const char* v = SDL_getenv("ORRE_HANDOVER");
    return v != NULL && v[0] == '1';
}

/* 1: this is the only OrrePort on this install (go on); 0: another one runs (it was brought forward; exit) */
int pc_single_instance_claim(void) {
    /* *Changed 2026-11-24 (the ORRE_ROOT test switch, pc_paths.c):* the lock is named after the OrrePort folder (the
     * data: saves, settings), not the exe's folder. For a player the two go together (one install, one app\), so
     * nothing changes; for test runs sharing one program, each run's own data folder is its own lock, so runs side by
     * side don't block each other, while two starts on the same data still do (what the rule protects) */
    char folder[1100];
    pc_paths_dir(PC_PLACE_ROOT, folder, sizeof(folder));
#ifdef _WIN32
    /* FNV-1a of the folder, lower-cased (Windows paths ignore case) */
    unsigned long long h = 1469598103934665603ull;
    for (const char* p = folder; *p != '\0'; ++p) h = (h ^ (unsigned char)SDL_tolower((unsigned char)*p)) * 1099511628211ull;
    wchar_t name[64];
    swprintf(name, 64, L"Local\\OrrePort-%016llx", h);
    HANDLE mutex = CreateMutexW(NULL, FALSE, name); /* kept for the whole run: never closed */
    if (mutex == NULL || GetLastError() != ERROR_ALREADY_EXISTS) return 1;
    /* *Changed 2026-11-15 (snatcherLeo: Quit to hub showed the desktop between the black screen and the hub; "the screen
     * telling us the game is closing and you are going back to the hub, will just change to the hub"):* the old
     * OrrePort now keeps its "Returning to the hub..." screen up until this one's hub is on screen
     * (pc_platform_restart_to_hub: it waits for this one's ready file), so this one must not wait for it to end: it
     * goes on at once, holding a handle to the same lock, which keeps the name taken after the old one ends (a third
     * launch still steps aside). Safe: the old one only shows a black screen by then (its memory card work was
     * finished before Quit to hub went on). */
    if (handover()) {
        printf("[single_instance] handover: going on while the previous OrrePort shows its closing screen\n");
        return 1;
    }
    GetModuleFileNameW(NULL, s_self, MAX_PATH);
    HWND first = NULL;
    EnumWindows(find_first, (LPARAM)&first);
    if (first != NULL) {
        if (IsIconic(first)) ShowWindow(first, SW_RESTORE);
        SetForegroundWindow(first);
    }
    printf("[single_instance] OrrePort is already running from %s: %s; this launch exits\n", folder,
           first != NULL ? "its window brought to the front" : "its window wasn't found");
    return 0;
#else
    char lock[1200];
    SDL_snprintf(lock, sizeof(lock), "%s/.orreport.lock", folder); /* pc_paths_dir gives the folder without its slash */
    const int fd = open(lock, O_RDWR | O_CREAT, 0644); /* kept open for the whole run: the lock lives with it */
    if (fd < 0) return 1; /* a read-only install: no lock, go on */
    struct flock fl;
    memset(&fl, 0, sizeof(fl));
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    if (fcntl(fd, F_SETLK, &fl) == 0) return 1;
    if (handover()) {
        /* the old OrrePort keeps its closing screen up until this one's hub shows (as on Windows, above), so go on now
         * and take its lock in the background once it has ended (likely: Linux / macOS not built or run yet). A
         * launch in that second or so could slip past the rule; harmless, as the old one writes nothing by then */
        s_lock_fd = fd;
        SDL_Thread* t = SDL_CreateThread(take_lock_later, "orre_lock", NULL);
        if (t != NULL) SDL_DetachThread(t);
        return 1;
    }
    printf("[single_instance] OrrePort is already running from %s: this launch exits\n", folder);
    return 0;
#endif
}
