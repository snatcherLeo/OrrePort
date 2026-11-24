/*
 * pc_adapter_setup.c: finding Nintendo's GameCube adapter on the wrong driver, and starting "OrrePort Adapter
 * Setup.exe" to fix it (2026-11-13; snatcherLeo: "Separate helper (1)"; the program: port/adapter_setup/).
 *
 * Why: Windows gives the adapter (USB 057E:0337) its standard input driver (HidUsb), which can't read it; SDL reads it
 * through libusb only once the adapter has Microsoft's WinUSB driver (docs/port/controls.md "The GameCube adapter",
 * verified on snatcherLeo's PC). OrrePort itself never changes drivers (that needs administrator rights, and the library that
 * does it is LGPL); it only looks, and offers the setup program.
 *
 * Looking: Windows' device list (SetupAPI), present devices whose hardware ID is USB\VID_057E&PID_0337, and the
 * driver service each one has ("WinUSB" = ready; anything else, or none, = needs the setup). A few milliseconds; the
 * Settings panel asks every two seconds while it is open, so plugging the adapter in shows the row.
 *
 * Starting: ShellExecuteEx with "runas", so Windows asks its administrator question once; the program's exit code
 * comes back through pc_adapter_setup_poll. Its log goes to logs\adapter setup.log.
 *
 * Platforms: Windows only. Linux and macOS: nothing here yet; their own way comes later, based on how each system can
 * best read the adapter (snatcherLeo, 2026-11-13: "we'd have to make linux and Mac alternatives, but that'll be later";
 * likely a one-time udev rule on Linux, nothing on macOS), so the stubs below say "nothing to set up" there.
 */
#include "pc_adapter_setup.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <shellapi.h>

#include "pc_paths.h" /* the helper beside the exe (app\), its logs in the logs folder (4.1) */

static int s_needs = 0;
static HANDLE s_proc = NULL;
static char s_progress[1200]; /* the setup program's progress file (--progress; the busy screen's bar) */

void pc_adapter_refresh(void) {
    int needs = 0;
    HDEVINFO set = SetupDiGetClassDevsA(NULL, "USB", NULL, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return;
    SP_DEVINFO_DATA dev;
    dev.cbSize = sizeof(dev);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dev); i++) {
        char ids[1024] = {0}, service[128] = {0};
        if (!SetupDiGetDeviceRegistryPropertyA(set, &dev, SPDRP_HARDWAREID, NULL, (BYTE*)ids, sizeof(ids) - 2, NULL)) {
            continue;
        }
        int match = 0;
        for (const char* p = ids; *p != '\0'; p += strlen(p) + 1) { /* a list of IDs, each ending in \0 */
            if (_strnicmp(p, "USB\\VID_057E&PID_0337", 21) == 0) match = 1;
        }
        if (!match) continue;
        SetupDiGetDeviceRegistryPropertyA(set, &dev, SPDRP_SERVICE, NULL, (BYTE*)service, sizeof(service) - 1, NULL);
        if (_stricmp(service, "WinUSB") != 0) needs = 1;
    }
    SetupDiDestroyDeviceInfoList(set);
    if (needs != s_needs) printf("[adapter] a GameCube adapter %s\n", needs ? "needs the setup (not on WinUSB)" : "is ready or unplugged");
    s_needs = needs;
}

int pc_adapter_needs_setup(void) { return s_needs; }

int pc_adapter_setup_start(void) {
    char exe[1200], log[1200];
    wchar_t wexe[1200], wargs[2800];
    if (s_proc != NULL) return PC_ADAPTER_STARTED;
    pc_paths_get(PC_PLACE_PROGRAM, "OrrePort Adapter Setup.exe", exe, sizeof(exe)); /* a program file: app\ */
    if (GetFileAttributesA(exe) == INVALID_FILE_ATTRIBUTES) {
        printf("[adapter] the setup program is missing: %s\n", exe);
        return PC_ADAPTER_NO_HELPER;
    }
    pc_paths_get(PC_PLACE_LOGS, "adapter setup.log", log, sizeof(log));
    for (char* p = log; *p; p++) if (*p == '/') *p = '\\';
    /* the progress file starts at 0, so an earlier run's "100 done" is never read as this one's */
    pc_paths_get(PC_PLACE_LOGS, "adapter setup progress.txt", s_progress, sizeof(s_progress));
    for (char* p = s_progress; *p; p++) if (*p == '/') *p = '\\';
    {
        FILE* f = fopen(s_progress, "w");
        if (f != NULL) {
            fputs("0 start\n", f);
            fclose(f);
        }
    }
    MultiByteToWideChar(CP_UTF8, 0, exe, -1, wexe, 1200);
    {
        char args[2800];
        snprintf(args, sizeof(args), "--install --log \"%s\" --progress \"%s\"", log, s_progress);
        MultiByteToWideChar(CP_UTF8, 0, args, -1, wargs, 2800);
    }
    SHELLEXECUTEINFOW sei;
    memset(&sei, 0, sizeof(sei));
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas"; /* Windows' administrator question (the driver swap needs it) */
    sei.lpFile = wexe;
    sei.lpParameters = wargs;
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        const DWORD err = GetLastError();
        printf("[adapter] the setup program didn't start (%lu%s)\n", err, err == ERROR_CANCELLED ? ": answered No" : "");
        return err == ERROR_CANCELLED ? PC_ADAPTER_DECLINED : PC_ADAPTER_FAILED;
    }
    s_proc = sei.hProcess;
    printf("[adapter] setup program started\n");
    return PC_ADAPTER_STARTED;
}

int pc_adapter_setup_poll(int* exit_code) {
    if (s_proc == NULL) return -1;
    if (WaitForSingleObject(s_proc, 0) == WAIT_TIMEOUT) return 1;
    DWORD code = 4;
    GetExitCodeProcess(s_proc, &code);
    CloseHandle(s_proc);
    s_proc = NULL;
    if (exit_code != NULL) *exit_code = (int)code;
    printf("[adapter] setup program ended: %lu\n", code);
    pc_adapter_refresh();
    return 0;
}

int pc_adapter_setup_progress(int* percent, char* step, size_t size) {
    char line[128] = {0};
    FILE* f = s_progress[0] != '\0' ? fopen(s_progress, "r") : NULL;
    if (f == NULL) return 0;
    const int ok = fgets(line, sizeof(line), f) != NULL;
    fclose(f);
    if (!ok) return 0; /* caught mid-write: keep the last step */
    int pct = 0;
    char st[32] = {0};
    if (sscanf(line, "%d %31s", &pct, st) < 1) return 0;
    if (percent != NULL) *percent = pct;
    if (step != NULL && size > 0) snprintf(step, size, "%s", st);
    return 1;
}

#else /* Linux, macOS: nothing to set up yet (see above) */
int pc_adapter_setup_progress(int* percent, char* step, size_t size) {
    (void)percent; (void)step; (void)size;
    return 0;
}
void pc_adapter_refresh(void) {}
int pc_adapter_needs_setup(void) { return 0; }
int pc_adapter_setup_start(void) { return PC_ADAPTER_FAILED; }
int pc_adapter_setup_poll(int* exit_code) { (void)exit_code; return -1; }
#endif
