/* sysinfo.c: the machine and its screens as plain lines (sysinfo.h says why). Windows today; other systems write
 * "not read on this system" until their twins exist. */
#include "sysinfo.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* appends a formatted line, never past cap */
static void add(char* out, size_t cap, size_t* at, const char* fmt, ...) {
    if (!out || cap == 0 || *at >= cap - 1) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(out + *at, cap - *at, fmt, ap);
    va_end(ap);
    if (n > 0) *at += (size_t)n < cap - *at ? (size_t)n : cap - *at - 1;
}

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <dxgi.h>
#include <shellscalingapi.h>

/* a registry string as UTF-8 ("" when missing) */
static void reg_text(HKEY root, const wchar_t* key, const wchar_t* name, char* out, size_t cap) {
    wchar_t buf[256];
    DWORD size = sizeof buf;
    out[0] = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, NULL, buf, &size) == ERROR_SUCCESS)
        WideCharToMultiByte(CP_UTF8, 0, buf, -1, out, (int)cap, NULL, NULL);
}

size_t diag_sysinfo(char* out, size_t cap) {
    size_t at = 0;
    if (out && cap) out[0] = 0;
    char cpu[160], release[64];
    reg_text(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString", cpu, sizeof cpu);
    char* c = cpu;
    while (*c == ' ') ++c;  /* (some processors pad their name) */
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    add(out, cap, &at, "cpu: %s, %lu logical processors\n", *c ? c : "unknown", (unsigned long)si.dwNumberOfProcessors);
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms))
        add(out, cap, &at, "memory: %.1f GB, %.1f GB free\n", ms.ullTotalPhys / 1073741824.0, ms.ullAvailPhys / 1073741824.0);
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    OSVERSIONINFOW v;
    memset(&v, 0, sizeof v);
    v.dwOSVersionInfoSize = sizeof v;
    RtlGetVersionFn get = (RtlGetVersionFn)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion");
    reg_text(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion", release, sizeof release);
    if (get && get(&v) == 0)
        add(out, cap, &at, "os: Windows %lu.%lu build %lu%s%s%s\n", (unsigned long)v.dwMajorVersion, (unsigned long)v.dwMinorVersion,
            (unsigned long)v.dwBuildNumber, release[0] ? " (" : "", release, release[0] ? ")" : "");
    /* every adapter DXGI lists (the software one too: a PC that falls back to it is worth knowing), with its memory and
     * the driver's version (CheckInterfaceSupport's user-mode driver version, as dxdiag shows it) */
    IDXGIFactory1* f = NULL;
    if (SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&f))) {
        IDXGIAdapter1* a = NULL;
        for (UINT i = 0; IDXGIFactory1_EnumAdapters1(f, i, &a) == S_OK; ++i) {
            DXGI_ADAPTER_DESC1 d;
            char name[256];
            IDXGIAdapter1_GetDesc1(a, &d);
            WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, sizeof name, NULL, NULL);
            LARGE_INTEGER ver;
            char driver[48] = "unknown";
            if (SUCCEEDED(IDXGIAdapter1_CheckInterfaceSupport(a, &IID_IDXGIDevice, &ver)))
                snprintf(driver, sizeof driver, "%u.%u.%u.%u", (unsigned)HIWORD(ver.HighPart), (unsigned)LOWORD(ver.HighPart),
                         (unsigned)HIWORD(ver.LowPart), (unsigned)LOWORD(ver.LowPart));
            add(out, cap, &at, "gpu %u: %s, %.0f MB own memory, driver %s%s\n", i + 1, name, d.DedicatedVideoMemory / 1048576.0, driver,
                (d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? " (software)" : "");
            IDXGIAdapter1_Release(a);
        }
        IDXGIFactory1_Release(f);
    }
    return at;
}

typedef struct { char* out; size_t cap; size_t* at; int n; } DisplayWalk;
static BOOL CALLBACK one_display(HMONITOR m, HDC dc, LPRECT r, LPARAM p) {
    (void)dc;
    (void)r;
    DisplayWalk* w = (DisplayWalk*)p;
    MONITORINFOEXW mi;
    mi.cbSize = sizeof mi;
    if (!GetMonitorInfoW(m, (MONITORINFO*)&mi)) return TRUE;
    DEVMODEW dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    const BOOL mode = EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm);
    UINT dx = 96, dy = 96;
    GetDpiForMonitor(m, MDT_EFFECTIVE_DPI, &dx, &dy);
    add(w->out, w->cap, w->at, "display %d: %ldx%ld%s%lu Hz, scaling %u%%, at %ld,%ld%s\n", ++w->n, mi.rcMonitor.right - mi.rcMonitor.left,
        mi.rcMonitor.bottom - mi.rcMonitor.top, mode ? " @ " : "", mode ? (unsigned long)dm.dmDisplayFrequency : 0ul, dx * 100 / 96,
        mi.rcMonitor.left, mi.rcMonitor.top, (mi.dwFlags & MONITORINFOF_PRIMARY) ? ", main" : "");
    return TRUE;
}

size_t diag_displays(char* out, size_t cap) {
    size_t at = 0;
    if (out && cap) out[0] = 0;
    DisplayWalk w = {out, cap, &at, 0};
    EnumDisplayMonitors(NULL, NULL, one_display, (LPARAM)&w);
    return at;
}

#else

size_t diag_sysinfo(char* out, size_t cap) {
    size_t at = 0;
    add(out, cap, &at, "system: not read on this system yet\n");
    return at;
}
size_t diag_displays(char* out, size_t cap) {
    size_t at = 0;
    add(out, cap, &at, "displays: not read on this system yet\n");
    return at;
}

#endif
