/*
 * orre_adapter_setup.c: "OrrePort Adapter Setup.exe", the one-click GameCube adapter setup (2026-11-13; snatcherLeo:
 * "as long as they don't need Zadig, and it just converts it for them", then "Separate helper (1)").
 *
 * Why it exists: Nintendo's GameCube controller adapter (USB 057E:0337, made for the Wii U) is taken by Windows'
 * standard input driver (HidUsb), which can't read it (Device Manager shows an error). Programs read it through libusb,
 * which needs Microsoft's general-purpose WinUSB driver on the adapter. Dolphin's guide has players do that with
 * Zadig; this program does the same swap with libwdi, the library Zadig is built on, so players never need Zadig.
 *
 * Why a separate program: libwdi is LGPL v3. Kept in its own small program, it never becomes part of OrrePort itself,
 * and the program has to run as administrator anyway (the next point), which OrrePort never should.
 *
 * What it does (all through libwdi; verified from libwdi 1.5.1's source, pki.c and libwdi.c):
 *   1. lists the USB devices and finds the adapter (057E:0337);
 *   2. if it already has WinUSB: nothing to do;
 *   3. writes a small driver description (an .inf that points at Windows' own WinUSB driver, plus Microsoft's two
 *      redistributable helper DLLs) into a temporary folder, makes a catalog for it and signs it with a certificate
 *      it creates on the spot. That certificate is added to this PC's trusted lists (Root and Trusted Publisher), and
 *      its private key is deleted right after signing, so it can never sign anything else;
 *   4. installs that driver for the adapter (Windows' own driver installer, which libwdi runs).
 * Steps 3 and 4 need administrator rights: that is the one Windows question (the manifest asks for them only when
 * installing; --check runs without).
 *
 * Use:
 *   "OrrePort Adapter Setup.exe" [--install] [--log <file>] [--progress <file>]
 *                                                            set the adapter up (the default; OrrePort's Controls row;
 *                                                            --progress: the busy screen's steps, see progress())
 *   "OrrePort Adapter Setup.exe" --check [--log <file>]       report only, change nothing
 * Exit codes (OrrePort's Controls row reads them): 0 ready (set up now, or already), 2 no adapter plugged in, 3 the
 * driver files couldn't be prepared, 4 the install failed, 5 needs setup (--check only), 6 Windows' question was
 * answered No (when this program had to ask for administrator rights itself).
 * Started by hand (double-click, no arguments) it shows its result in a small window, in the player's language when
 * ui\lang\<code>.txt is next to it (the same files OrrePort uses; English built in otherwise).
 *
 * Platforms: Windows only (the whole program). Linux needs no driver swap, only a one-time udev rule so a normal user
 * may open USB 057E:0337 (likely); macOS needs nothing (likely). docs/port/controls.md "What each platform needs".
 * XD: the same adapter and the same program (likely).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libwdi.h"

#ifndef ORRE_LIBWDI_VERSION
#define ORRE_LIBWDI_VERSION "?" /* set by tools/local/port/build_adapter_setup.py */
#endif

enum { EXIT_READY = 0, EXIT_NONE = 2, EXIT_PREPARE = 3, EXIT_INSTALL = 4, EXIT_NEEDS = 5, EXIT_DECLINED = 6 };
enum { ADAPTER_VID = 0x057E, ADAPTER_PID = 0x0337 };

static FILE* s_log = NULL;
static const char* s_progress_path = NULL;

/* The progress file (--progress <file>, OrrePort's busy screen; snatcherLeo, 2026-11-13: "have a progress bar too, as it's
 * installing the driver"): one line, "<percent> <step>", rewritten at each step. libwdi itself reports no progress,
 * so these are the program's own steps; OrrePort moves the bar on smoothly within the long Windows step. */
static void progress(int percent, const char* step) {
    if (s_progress_path == NULL) return;
    FILE* f = fopen(s_progress_path, "w");
    if (f == NULL) return;
    fprintf(f, "%d %s\n", percent, step);
    fclose(f);
}

static void logf_(const char* fmt, ...) {
    va_list ap;
    if (s_log == NULL) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(s_log, "%02d:%02d:%02d ", t.wHour, t.wMinute, t.wSecond);
    va_start(ap, fmt);
    vfprintf(s_log, fmt, ap);
    va_end(ap);
    fputc('\n', s_log);
    fflush(s_log);
}

/* ---- words, when started by hand (OrrePort shows its own) ---------------------------------------------------------
 * Every word translatable (the project rules, snatcherLeo 2026-11-13): the lines are keys in ui/lang/en.txt ("adapter.helper.*"),
 * read from ui\lang\<Windows' language>.txt next to this program, else en.txt, else the English below. */
static char s_dir[MAX_PATH * 3]; /* this program's folder, UTF-8, ending in a backslash */

static int read_key(const char* file, const char* key, char* out, size_t size) {
    FILE* f = fopen(file, "rb");
    char line[2048];
    const size_t klen = strlen(key);
    if (f == NULL) return 0;
    while (fgets(line, sizeof(line), f) != NULL) {
        if (strncmp(line, key, klen) != 0) continue;
        const char* p = line + klen;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '=') continue;
        p++;
        while (*p == ' ') p++;
        snprintf(out, size, "%s", p);
        size_t n = strlen(out);
        while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r')) out[--n] = '\0';
        fclose(f);
        return 1;
    }
    fclose(f);
    return 0;
}

static void word(const char* key, const char* english, char* out, size_t size) {
    wchar_t wname[LOCALE_NAME_MAX_LENGTH];
    char lang[16] = "en", path[MAX_PATH * 3 + 64];
    if (GetUserDefaultLocaleName(wname, LOCALE_NAME_MAX_LENGTH) > 2) {
        lang[0] = (char)wname[0];
        lang[1] = (char)wname[1];
        lang[2] = '\0';
    }
    snprintf(path, sizeof(path), "%sui\\lang\\%s.txt", s_dir, lang);
    if (read_key(path, key, out, size)) return;
    snprintf(path, sizeof(path), "%sui\\lang\\en.txt", s_dir);
    if (read_key(path, key, out, size)) return;
    snprintf(out, size, "%s", english);
}

static void show(const char* key, const char* english, UINT icon) {
    char title[256], text[1024];
    wchar_t wtitle[256], wtext[1024];
    word("adapter.helper.title", "OrrePort: GameCube adapter", title, sizeof(title));
    word(key, english, text, sizeof(text));
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wtitle, 256);
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wtext, 1024);
    MessageBoxW(NULL, wtext, wtitle, MB_OK | icon);
}

/* ---- administrator rights ------------------------------------------------------------------------------------------ */
static int is_admin(void) {
    BOOL admin = FALSE;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    PSID group = NULL;
    if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &group)) {
        CheckTokenMembership(NULL, group, &admin);
        FreeSid(group);
    }
    return admin != FALSE;
}

/* Started without administrator rights (by hand): ask Windows once by starting this program again "as
 * administrator" with the same arguments, wait, and pass its result on. */
static int run_elevated(void) {
    wchar_t self[MAX_PATH];
    const wchar_t* args = GetCommandLineW();
    /* skip the program's own name in the command line (quoted or not) */
    if (*args == L'"') {
        args++;
        while (*args && *args != L'"') args++;
        if (*args) args++;
    } else {
        while (*args && *args != L' ') args++;
    }
    while (*args == L' ') args++;
    GetModuleFileNameW(NULL, self, MAX_PATH);
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = self;
    sei.lpParameters = args;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        logf_("Windows' administrator question was answered No (or failed: %lu)", GetLastError());
        return EXIT_DECLINED;
    }
    DWORD code = EXIT_INSTALL;
    WaitForSingleObject(sei.hProcess, INFINITE);
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    return (int)code;
}

/* ---- the adapter ----------------------------------------------------------------------------------------------------- */
static struct wdi_device_info* find_adapter(struct wdi_device_info* list) {
    for (struct wdi_device_info* d = list; d != NULL; d = d->next) {
        if (d->vid == ADAPTER_VID && d->pid == ADAPTER_PID && !d->is_composite) return d;
    }
    return NULL;
}

static int has_winusb(const struct wdi_device_info* d) {
    return d->driver != NULL && _stricmp(d->driver, "WinUSB") == 0;
}

int main(int argc, char** argv) {
    int check = 0, by_hand = argc <= 1, code = EXIT_READY;
    const char* log_path = NULL;
    {
        wchar_t wself[MAX_PATH];
        GetModuleFileNameW(NULL, wself, MAX_PATH);
        WideCharToMultiByte(CP_UTF8, 0, wself, -1, s_dir, sizeof(s_dir), NULL, NULL);
        char* slash = strrchr(s_dir, '\\');
        if (slash != NULL) slash[1] = '\0';
    }
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--check") == 0) check = 1;
        else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) log_path = argv[++i];
        else if (strcmp(argv[i], "--progress") == 0 && i + 1 < argc) s_progress_path = argv[++i];
    }
    progress(5, "find");
    if (log_path != NULL) s_log = fopen(log_path, "a");
    logf_("OrrePort Adapter Setup (%s; libwdi %s)", check ? "check only" : "set up", ORRE_LIBWDI_VERSION);

    if (!check && !is_admin()) {
        logf_("not running as administrator: asking Windows");
        code = run_elevated();
        logf_("the administrator run ended with %d", code);
        if (by_hand && code == EXIT_DECLINED) show("adapter.helper.declined", "Nothing was changed.", MB_ICONINFORMATION);
        if (s_log != NULL) fclose(s_log);
        return code;
    }

    struct wdi_device_info* list = NULL;
    struct wdi_options_create_list lopt = {TRUE, FALSE, TRUE}; /* every device, not only driverless ones */
    int r = wdi_create_list(&list, &lopt);
    struct wdi_device_info* dev = r == WDI_SUCCESS ? find_adapter(list) : NULL;
    if (dev == NULL) {
        logf_("no GameCube adapter (USB 057E:0337) found (%s)", r == WDI_SUCCESS ? "listed" : wdi_strerror(r));
        code = EXIT_NONE;
        if (by_hand) {
            show("adapter.helper.none", "No GameCube adapter found. Plug in its black USB plug (the grey one only "
                 "powers rumble) and try again.", MB_ICONWARNING);
        }
    } else if (has_winusb(dev)) {
        logf_("the adapter (%s) already has WinUSB: nothing to do", dev->desc != NULL ? dev->desc : "?");
        code = EXIT_READY;
        if (by_hand) show("adapter.helper.already", "The GameCube adapter is already set up. Nothing was changed.",
                          MB_ICONINFORMATION);
    } else if (check) {
        logf_("the adapter (%s) has driver %s: needs setup", dev->desc != NULL ? dev->desc : "?",
              dev->driver != NULL ? dev->driver : "(none)");
        code = EXIT_NEEDS;
    } else {
        char dir[MAX_PATH];
        struct wdi_options_prepare_driver popt;
        struct wdi_options_install_driver iopt;
        memset(&popt, 0, sizeof(popt));
        memset(&iopt, 0, sizeof(iopt));
        popt.driver_type = WDI_WINUSB;
        popt.vendor_name = "Nintendo";
        GetTempPathA(sizeof(dir) - 40, dir);
        strcat(dir, "OrrePort adapter driver");
        logf_("the adapter (%s) has driver %s: preparing WinUSB in %s", dev->desc != NULL ? dev->desc : "?",
              dev->driver != NULL ? dev->driver : "(none)", dir);
        progress(15, "prepare");
        r = wdi_prepare_driver(dev, dir, "gamecube_adapter.inf", &popt);
        if (r != WDI_SUCCESS) {
            logf_("preparing the driver failed: %s", wdi_strerror(r));
            code = EXIT_PREPARE;
        } else {
            progress(40, "install");
            iopt.pending_install_timeout = 120000;
            r = wdi_install_driver(dev, dir, "gamecube_adapter.inf", &iopt);
            logf_("installing the driver: %s", r == WDI_SUCCESS ? "done" : wdi_strerror(r));
            code = r == WDI_SUCCESS ? EXIT_READY : EXIT_INSTALL;
        }
        if (by_hand) {
            if (code == EXIT_READY) {
                show("adapter.helper.done", "The GameCube adapter is set up. If a controller doesn't show, unplug the "
                     "adapter and plug it back in.", MB_ICONINFORMATION);
            } else {
                show("adapter.helper.failed", "The GameCube adapter couldn't be set up.", MB_ICONERROR);
            }
        }
    }
    if (list != NULL) wdi_destroy_list(list);
    progress(100, "done");
    logf_("result %d", code);
    if (s_log != NULL) fclose(s_log);
    return code;
}
