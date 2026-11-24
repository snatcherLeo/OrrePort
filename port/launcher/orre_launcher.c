/*
 * orre_launcher.c: OrrePort.exe and "OrrePort Studio.exe", the two small programs at the top of the portable folder
 * (Phase 4 step 4.1; docs/port/portable-layout.md part 4; the decided tree, the project notes section 8).
 * Built twice by tools/local/port/build_launcher.py: plain for OrrePort.exe (starts app\orrecolo.exe), with
 * -DORRE_STUDIO for "OrrePort Studio.exe" (starts app\orrestudio.exe).
 *
 * Why a launcher at all: Windows loads a program's DLLs before any of its code runs, from the program's own folder, so
 * the real exe has to sit beside its DLLs in app\; the player's folder stays tidy (app\ for the program, data\ for
 * its settings and caches, and the game's folder for the player's things; layout A, 2026-11-22: was bin\, settings\,
 * system\) only if what they double-click is a separate small program at the
 * top. It is also the piece that will swap in a new app\ after an update (decided: "when the game closes, the small
 * launcher swaps in the new app\"), so it stays tiny, rarely changed, outside app\, and static (no DLLs of its own).
 *
 * What it does:
 * 1. finds its own folder (the OrrePort folder) with a buffer that grows, so a long path works (no MAX_PATH limit;
 *    the manifest declares longPathAware);
 * 2. checks app\<program> exists, else a plain message box naming the missing file;
 * 3. starts it with the same arguments (so --disc and Studio's switches keep working) and the working folder set to
 *    the OrrePort folder, so nothing that writes relative to the working folder (test-only files) lands in app\;
 * 4. puts it in a Job Object and waits until the job has no processes left, then returns the last one's exit code.
 *    Why the job: Quit to hub starts a new orrecolo.exe and ends the old one (pc_platform.c,
 *    pc_platform_restart_to_hub); a child started by a process in the job joins the job by itself, so the launcher
 *    keeps waiting while the restarted hub runs. Waiting on the first child only would let the launcher think the game
 *    had closed while the hub still runs from app\, which would be fatal once the launcher swaps app\ for updates.
 *    The job never kills anything: if the launcher itself is ended, the game keeps running (no KILL_ON_JOB_CLOSE).
 *    The elevated adapter helper (started with "runas") is created by Windows outside the job, and needn't be in it.
 *
 * Platforms: Windows only (Win32 and Job Objects). Linux (likely): a small shell script per program at the top
 * ("OrrePort", "OrrePort Studio") that cd's to its own folder and execs app/orrecolo with "$@"; libraries are found
 * through an $ORIGIN rpath, and Quit to hub's restart is a child of the same process group, so nothing like the job
 * is needed (the script exec's, so there is nothing left to wait). macOS (likely): two .app bundles replace both the
 * launcher and app\; the bundle is what the player opens and what an update replaces. XD (likely): the same program
 * with the XD exe's name compiled in.
 */
#define WIN32_LEAN_AND_MEAN
#ifndef UNICODE /* -municode defines it already */
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <tlhelp32.h> /* the update swap: is a program from app\ still running */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifdef ORRE_STUDIO
#define TARGET L"orrestudio.exe"
#define APP_NAME L"OrrePort Studio"
#else
#define TARGET L"orrecolo.exe"
#define APP_NAME L"OrrePort"
#endif

/* The launcher's own full path in a heap buffer that grows until it fits (GetModuleFileNameW truncates silently) */
static wchar_t* own_path(void) {
    DWORD size = 512;
    for (;;) {
        wchar_t* buf = (wchar_t*)malloc(size * sizeof(wchar_t));
        DWORD n;
        if (buf == NULL) return NULL;
        n = GetModuleFileNameW(NULL, buf, size);
        if (n == 0) {
            free(buf);
            return NULL;
        }
        if (n < size - 1) return buf;
        free(buf);
        size *= 2;
    }
}

/* The arguments after the program name in the launcher's own command line, unchanged (Windows hands a program one
 * string; the first word is the program, quoted or not, per the CommandLineToArgvW rules for argv[0]) */
static const wchar_t* arguments(const wchar_t* line) {
    if (*line == L'"') {
        line++;
        while (*line != L'\0' && *line != L'"') line++;
        if (*line == L'"') line++;
    } else {
        while (*line != L'\0' && *line != L' ' && *line != L'\t') line++;
    }
    while (*line == L' ' || *line == L'\t') line++;
    return line;
}

/* Words the player sees, in the six official languages (the project's decision, 2026-11-21). The rest of OrrePort's words come
 * from app\UI\lang\<code>.txt (docs/port/translation.md), but these boxes show exactly when app\ is missing, so the
 * lines (the "launcher." keys in port/ui/lang/<code>.txt) are compiled in: build_launcher.py writes launcher_text.h from those files
 * (kLauncherText: one row per language, English first). A language without a line falls back to English there. */
#include "launcher_text.h"

/* The player's language: settings.ini's "language" in the OrrePort folder's data\settings\ when it names one of the compiled
 * languages (the setting the hub writes), else Windows' display language, else English. Windows only (the Linux and
 * macOS launchers are scripts / bundles with no box of their own). */
static const LauncherText* pick_text(const wchar_t* folder) {
    const int n = (int)(sizeof(kLauncherText) / sizeof(kLauncherText[0]));
    char code[16] = "";
    size_t len = wcslen(folder) + 32;
    wchar_t* ini = folder[0] != L'\0' ? (wchar_t*)malloc(len * sizeof(wchar_t)) : NULL; /* "": no folder known */
    if (ini != NULL) {
        FILE* f;
        swprintf(ini, len, L"%lsdata\\settings\\settings.ini", folder); /* layout A, 2026-11-22 (pc_paths.c) */
        f = _wfopen(ini, L"rb");
        if (f != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), f) != NULL) {
                char* p = line;
                while (*p == ' ' || *p == '\t') p++;
                if (strncmp(p, "language", 8) != 0) continue;
                p += 8;
                while (*p == ' ' || *p == '\t') p++;
                if (*p != '=') continue;
                p++;
                while (*p == ' ' || *p == '\t') p++;
                int k = 0;
                while (k < (int)sizeof(code) - 1 && p[k] != '\0' && p[k] != '\r' && p[k] != '\n' && p[k] != ' ') {
                    code[k] = p[k];
                    k++;
                }
                code[k] = '\0';
            }
            fclose(f);
        }
        free(ini);
    }
    if (code[0] == '\0' || strcmp(code, "auto") == 0) {
        switch (PRIMARYLANGID(GetUserDefaultUILanguage())) {
        case LANG_GERMAN: strcpy(code, "de"); break;
        case LANG_FRENCH: strcpy(code, "fr"); break;
        case LANG_ITALIAN: strcpy(code, "it"); break;
        case LANG_SPANISH: strcpy(code, "es"); break;
        case LANG_JAPANESE: strcpy(code, "ja"); break;
        default: strcpy(code, "en"); break;
        }
    }
    for (int i = 0; i < n; i++) {
        if (strcmp(kLauncherText[i].code, code) == 0) return &kLauncherText[i];
    }
    return &kLauncherText[0];
}

/* The line with {app}, {file} and {code} filled in (a new heap string) */
static wchar_t* fill(const wchar_t* line, const wchar_t* file, unsigned long code) {
    wchar_t num[16];
    size_t cap = wcslen(line) + (file != NULL ? wcslen(file) : 0) * 2 + 64, n = 0;
    wchar_t* out = (wchar_t*)malloc(cap * sizeof(wchar_t));
    if (out == NULL) return NULL;
    swprintf(num, 16, L"%lu", code);
    for (const wchar_t* p = line; *p != L'\0';) {
        const wchar_t* put = NULL;
        size_t skip = 0;
        if (wcsncmp(p, L"{app}", 5) == 0) put = APP_NAME, skip = 5;
        else if (wcsncmp(p, L"{file}", 6) == 0) put = file != NULL ? file : L"", skip = 6;
        else if (wcsncmp(p, L"{code}", 6) == 0) put = num, skip = 6;
        if (put != NULL) {
            for (; *put != L'\0' && n + 1 < cap; put++) out[n++] = *put;
            p += skip;
        } else {
            if (n + 1 < cap) out[n++] = *p;
            p++;
        }
    }
    out[n] = L'\0';
    return out;
}

static void tell(const wchar_t* text) { MessageBoxW(NULL, text, APP_NAME, MB_OK | MB_ICONERROR); }

/* A visible top-level window of the started program (the busy cursor, below). "Visible" also means not cloaked: the
 * game shows its window cloaked at about 0.4 s and uncloaks it once the hub has drawn (pc_window_icon.cpp,
 * DWMWA_CLOAK), and IsWindowVisible is already true while it is cloaked. DwmGetWindowAttribute is looked up at run time
 * (as the game does), so the launcher links nothing new; without it, a window counts when IsWindowVisible says so. */
typedef HRESULT(WINAPI* DwmGetAttr)(HWND, DWORD, PVOID, DWORD);

typedef struct WindowSearch {
    DWORD pid;
    int found;
    DwmGetAttr dwm_get;
} WindowSearch;

static int cloaked(const WindowSearch* s, HWND w) {
    DWORD value = 0;
    return s->dwm_get != NULL && SUCCEEDED(s->dwm_get(w, 14 /* DWMWA_CLOAKED */, &value, sizeof(value))) && value != 0;
}

static BOOL CALLBACK find_window(HWND w, LPARAM data) {
    WindowSearch* s = (WindowSearch*)data;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == s->pid && IsWindowVisible(w) && GetWindow(w, GW_OWNER) == NULL && !cloaked(s, w)) {
        s->found = 1;
        return FALSE;
    }
    return TRUE;
}

/* Until the started program shows a window, it ends, or `ms` pass (polled every 20 ms) */
static void wait_for_window(HANDLE process, DWORD pid, DWORD ms) {
    const ULONGLONG until = GetTickCount64() + ms;
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    const DwmGetAttr dwm_get = dwm != NULL ? (DwmGetAttr)(void*)GetProcAddress(dwm, "DwmGetWindowAttribute") : NULL;
    for (;;) {
        WindowSearch s = {pid, 0, dwm_get};
        EnumWindows(find_window, (LPARAM)&s);
        if (s.found || GetTickCount64() >= until || WaitForSingleObject(process, 20) == WAIT_OBJECT_0) {
            return;
        }
    }
}

/* ---- the update swap (2026-11-23; docs/port/updater-plan.md, port/src/pc_update.c) ----
 * The shared updater leaves data\system\update\ready (holding the new version) and the unpacked
 * data\system\update\<version>\app\; "Go back" leaves data\system\update\rollback. The launcher acts on them before it
 * starts the program, and again when the program exits with PC_UPDATE_EXIT_RESTART ("Restart now"), and only when no
 * program from app\ is running (the other one may be open: then nothing changes, and it tries at the next start).
 * The swap is three renames, each undone if a later one fails, so app\ is never left half-replaced: app\ goes to
 * update\previous\app\ (kept for "Go back", its version beside it), the new app\ takes its place. A rollback swaps
 * the two back. Saves, mods and settings are never touched: they live outside app\. Windows only, like the launcher;
 * Linux and macOS would do the same renames in their scripts / bundles (likely). */
#define PC_UPDATE_EXIT_RESTART 0x55504431 /* "UPD1", pc_update.h */

/* any running process whose program file is inside <folder>app\ */
static int app_in_use(const wchar_t* folder) {
    wchar_t app[1200];
    HANDLE snap;
    PROCESSENTRY32W pe;
    int found = 0;
    swprintf(app, 1200, L"%lsapp\\", folder);
    snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 1; /* can't tell: don't swap */
    pe.dwSize = sizeof pe;
    for (BOOL more = Process32FirstW(snap, &pe); more && !found; more = Process32NextW(snap, &pe)) {
        wchar_t img[1200];
        DWORD n = 1200;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (h == NULL) continue;
        if (QueryFullProcessImageNameW(h, 0, img, &n) && _wcsnicmp(img, app, wcslen(app)) == 0) found = 1;
        CloseHandle(h);
    }
    CloseHandle(snap);
    return found;
}

/* a folder this launcher's updater made, and everything in it (an older kept version, a finished update's folder) */
static void remove_tree(const wchar_t* dir) {
    wchar_t pat[1300];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    swprintf(pat, 1300, L"%ls\\*", dir);
    h = FindFirstFileW(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t p[1300];
            if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
            swprintf(p, 1300, L"%ls\\%ls", dir, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) remove_tree(p);
            else { SetFileAttributesW(p, FILE_ATTRIBUTE_NORMAL); DeleteFileW(p); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir);
}

/* the first line of a small text file (a version), "" when missing */
static void read_line(const wchar_t* path, wchar_t* out, size_t n) {
    FILE* f = _wfopen(path, L"rb");
    char buf[64] = "";
    size_t got = 0;
    out[0] = 0;
    if (f == NULL) return;
    got = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[got] = 0;
    for (size_t i = 0; buf[i]; i++) if (buf[i] == '\r' || buf[i] == '\n') { buf[i] = 0; break; }
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, out, (int)n);
}

static void write_line(const wchar_t* path, const wchar_t* text) {
    FILE* f = _wfopen(path, L"wb");
    char buf[64];
    if (f == NULL) return;
    WideCharToMultiByte(CP_UTF8, 0, text, -1, buf, sizeof buf, NULL, NULL);
    fputs(buf, f);
    fputc('\n', f);
    fclose(f);
}

static void try_swap(const wchar_t* folder) {
    wchar_t upd[1200], app[1200], prev[1200], prev_app[1200], marker[1200], ver[64], newapp[1300], hold[1200];
    swprintf(upd, 1200, L"%lsdata\\system\\update\\", folder);
    swprintf(app, 1200, L"%lsapp", folder);
    swprintf(prev, 1200, L"%lsprevious", upd);
    swprintf(prev_app, 1200, L"%lsprevious\\app", upd);
    swprintf(marker, 1200, L"%lsrollback", upd);
    if (GetFileAttributesW(marker) != INVALID_FILE_ATTRIBUTES && GetFileAttributesW(prev_app) != INVALID_FILE_ATTRIBUTES) {
        wchar_t cur[64], old[64], pv[1200], av[1200];
        if (app_in_use(folder)) return;
        swprintf(hold, 1200, L"%lsswap", upd);
        swprintf(pv, 1200, L"%lsprevious\\version.txt", upd);
        swprintf(av, 1200, L"%ls\\version.txt", app);
        read_line(av, cur, 64);
        read_line(pv, old, 64);
        /* app -> swap, previous\app -> app, swap -> previous\app (each step undone if the next fails) */
        if (MoveFileW(app, hold)) {
            if (MoveFileW(prev_app, app)) {
                if (MoveFileW(hold, prev_app)) {
                    write_line(pv, cur); /* what "Go back" would now return to: the version just left */
                    DeleteFileW(marker);
                } else {
                    MoveFileW(app, prev_app);
                    MoveFileW(hold, app);
                }
            } else {
                MoveFileW(hold, app);
            }
        }
        (void)old;
        return;
    }
    swprintf(marker, 1200, L"%lsready", upd);
    read_line(marker, ver, 64);
    if (ver[0] == 0) return;
    swprintf(newapp, 1300, L"%ls%ls\\app", upd, ver);
    if (GetFileAttributesW(newapp) == INVALID_FILE_ATTRIBUTES) { DeleteFileW(marker); return; }
    if (app_in_use(folder)) return; /* the other program is open: next start */
    {
        wchar_t cur[64], av[1200], pv[1200], verdir[1300];
        swprintf(av, 1200, L"%ls\\version.txt", app);
        read_line(av, cur, 64);
        remove_tree(prev); /* the version before last: only one is kept for "Go back" */
        CreateDirectoryW(prev, NULL);
        if (!MoveFileW(app, prev_app)) return;
        if (!MoveFileW(newapp, app)) {
            MoveFileW(prev_app, app); /* put the old one back: nothing half-updated */
            return;
        }
        swprintf(pv, 1200, L"%lsprevious\\version.txt", upd);
        write_line(pv, cur[0] ? cur : L"?");
        DeleteFileW(marker);
        swprintf(verdir, 1300, L"%ls%ls", upd, ver);
        remove_tree(verdir);
    }
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cmd, int show) {
    wchar_t *self, *folder_end, *target, *line;
    const wchar_t* args;
    size_t folder_len, target_len, line_len;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    HANDLE job, port;
    JOBOBJECT_ASSOCIATE_COMPLETION_PORT link;
    HANDLE known[64]; /* processes the job has had, to read the last one's exit code */
    int known_count = 0;
    DWORD exit_code = 0;
    (void)inst;
    (void)prev;
    (void)cmd;
    (void)show;

    self = own_path();
    folder_end = self != NULL ? wcsrchr(self, L'\\') : NULL;
    if (folder_end == NULL) {
        /* no folder, so no settings.ini to read the language from: Windows' display language */
        wchar_t* msg = fill(pick_text(L"")->no_folder, NULL, 0);
        tell(msg != NULL ? msg : L"OrrePort");
        return 1;
    }
    folder_end[1] = L'\0'; /* self is now the OrrePort folder, with its trailing backslash */
    folder_len = wcslen(self);
    const LauncherText* text = pick_text(self);
    try_swap(self); /* an update downloaded last time, or "Go back", before the program starts (above) */

    target_len = folder_len + 4 + wcslen(TARGET) + 1;
    target = (wchar_t*)malloc(target_len * sizeof(wchar_t));
    if (target == NULL) return 1;
    swprintf(target, target_len, L"%lsapp\\%ls", self, TARGET);
    if (GetFileAttributesW(target) == INVALID_FILE_ATTRIBUTES) {
        wchar_t* msg = fill(text->missing, target, 0);
        tell(msg != NULL ? msg : target);
        return 1;
    }

    /* the command line: "<target>" plus the launcher's own arguments, unchanged */
    args = arguments(GetCommandLineW());
    line_len = wcslen(target) + wcslen(args) + 4;
    line = (wchar_t*)malloc(line_len * sizeof(wchar_t));
    if (line == NULL) return 1;
    swprintf(line, line_len, *args != L'\0' ? L"\"%ls\" %ls" : L"\"%ls\"", target, args);

start: /* again after "Restart now" for an update (the end of this function) */
    known_count = 0;
    exit_code = 0;
    /* the job and its completion port: the port is told when a process joins, leaves, and when none are left */
    job = CreateJobObjectW(NULL, NULL);
    port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (job != NULL && port != NULL) {
        link.CompletionKey = job;
        link.CompletionPort = port;
        if (!SetInformationJobObject(job, JobObjectAssociateCompletionPortInformation, &link, sizeof(link))) {
            CloseHandle(job);
            job = NULL;
        }
    }

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    /* suspended until it is in the job, so it can't start a child before the job sees it.
     * CREATE_NO_WINDOW (snatcherLeo, 2026-11-21: "why, when I open "OrrePort.exe" does this terminal window open up as
     * well?"): app\orrecolo.exe is a console program (the run log is also copied to a console when there is one, which
     * run_game.ps1's PowerShell shows), so without the flag Windows opened a terminal window beside the game. With it the
     * game gets a console with no window: nothing shows, the log still goes to data\system\logs\ (pc_log.c), and the
     * programs the game starts (Quit to hub's new run, Report a problem) share that hidden console instead of opening
     * their own. Ignored for a windowed program (orrestudio.exe). Windows only, like the whole launcher; Linux and
     * macOS start programs without a terminal anyway (likely), so their launchers need nothing like it. */
    if (!CreateProcessW(target, line, NULL, NULL, FALSE, CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, self, &si, &pi)) {
        const DWORD err = GetLastError();
        wchar_t* msg = fill(text->start_failed, target, err);
        tell(msg != NULL ? msg : target);
        return 1;
    }
    if (job != NULL && !AssignProcessToJobObject(job, pi.hProcess)) {
        CloseHandle(job); /* can't track it in a job (very old Windows, or a job that forbids it): wait on it alone */
        job = NULL;
    }
    known[known_count++] = pi.hProcess;
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);

    /* The busy cursor (snatcherLeo, 2026-11-22, through Studio: the arrow with the small busy circle when opening Studio from
     * the shortcut). Windows shows "app starting" feedback for the program Explorer started, which is the launcher, not
     * the program it starts in turn. The rule (CreateProcess, STARTF_FORCEONFEEDBACK): no GUI call within 2 s ends it
     * at 2 s; after a GUI call Windows waits up to 5 more seconds for a window; the first GetMessage ends it. The first
     * launcher made no GUI call (busy for 2 s); ef21548c9's PeekMessage was a GUI call but never a GetMessage, so the
     * busy cursor ran the full ~6-7 s (measured from Explorer: gone at 6.0 s, Studio's window at 0.5 s; Studio's
     * session found it, starting through explorer.exe; a start from PowerShell shows no feedback at all, which is why
     * the first check passed). Now: one GUI call at once (PeekMessage, which also makes the message queue), so the busy
     * cursor stays while the program loads; then wait until the started program's window is visible (Studio ~0.5 s,
     * the game's hub ~3.5 s; at most 7 s, the feedback's own limit), then post one message to ourselves and GetMessage
     * it, which ends the feedback at once. Measured (explorer.exe, cursor polled every 20 ms): Studio's busy cursor
     * gone about 40-50 ms after its window shows. Windows only, like the whole launcher; Linux and macOS desktops
     * handle start-up feedback their own way (likely: nothing to do). */
    {
        MSG msg;
        PeekMessageW(&msg, NULL, 0, 0, PM_NOREMOVE);
        wait_for_window(pi.hProcess, pi.dwProcessId, 7000);
        PostThreadMessageW(GetCurrentThreadId(), WM_NULL, 0, 0);
        GetMessageW(&msg, NULL, 0, 0); /* returns at once: the message just posted */
    }

    if (job == NULL) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exit_code);
        if (exit_code == PC_UPDATE_EXIT_RESTART) {
            try_swap(self);
            goto start;
        }
        return (int)exit_code;
    }
    for (;;) {
        DWORD msg = 0;
        ULONG_PTR key = 0;
        LPOVERLAPPED data = NULL;
        if (!GetQueuedCompletionStatus(port, &msg, &key, &data, INFINITE)) break;
        if (key != (ULONG_PTR)job) continue;
        if (msg == JOB_OBJECT_MSG_NEW_PROCESS && known_count < 64) {
            /* the process ID comes in place of the overlapped pointer; a handle now keeps its exit code readable. Only
             * the program itself counts (a Quit to hub restart): other programs it starts join the job too, and one
             * of them ending last must not decide the exit code (seen 2026-11-21: a helper ending with 109, "broken
             * pipe", after the game was ended) */
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, (DWORD)(ULONG_PTR)data);
            if (h != NULL) {
                wchar_t image[1024];
                DWORD n = 1024;
                const wchar_t* base = NULL;
                if (QueryFullProcessImageNameW(h, 0, image, &n)) {
                    base = wcsrchr(image, L'\\');
                    base = base != NULL ? base + 1 : image;
                }
                /* by file name: the folder's spelling can differ (short names, a \\?\ prefix) */
                if (base != NULL && lstrcmpiW(base, TARGET) == 0) {
                    known[known_count++] = h;
                } else {
                    CloseHandle(h);
                }
            }
        } else if (msg == JOB_OBJECT_MSG_EXIT_PROCESS || msg == JOB_OBJECT_MSG_ABNORMAL_EXIT_PROCESS) {
            for (int i = 0; i < known_count; i++) {
                if (GetProcessId(known[i]) == (DWORD)(ULONG_PTR)data) {
                    GetExitCodeProcess(known[i], &exit_code); /* the last one to end gives the launcher's exit code */
                }
            }
        } else if (msg == JOB_OBJECT_MSG_ACTIVE_PROCESS_ZERO) {
            break; /* every OrrePort process has ended */
        }
    }
    CloseHandle(port);
    CloseHandle(job);
    if (exit_code == PC_UPDATE_EXIT_RESTART) { /* "Restart now": swap in the update (when nothing else uses app\), start again */
        try_swap(self);
        goto start;
    }
    return (int)exit_code;
}
