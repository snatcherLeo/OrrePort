// log.cpp: Studio's logs and its memory / disk watch (snatcherLeo's philosophy, section 8, "Watching the program: memory and
// logs", 2026-11-18: "have RAM watch, loggers built for features that would need logging to review and fix. The RAM
// watch and memory usage/data usage is to ensure our program isn't acting out of place.").
//
// Logs: <exe folder>\logs\studio-<date>-<time>.log, plain text, one per start; the newest 10 kept (older Studio logs
// are removed at start: rotation, as the rule asks; nothing else is ever touched). No personal data: no user names
// or full paths of the player's files (the disc shows by its file name only). Detail lines only with --log-detail.
//
// The watch: Studio's own working set, private memory and peak (GetProcessMemoryInfo), and bytes read and written
// (GetProcessIoCounters), checked after each piece of work against budgets set from the first measurements (likely
// generous; to be tightened as editors are added):
//   private memory above 600 MB, or more than 150 MB above the level after the first full load (memory that keeps
//   growing); more than 200 MB read by one piece of work (a feature reading far more than it should: one species'
//   pictures and model are a few MB); more than 5 MB written between two checks (Studio only writes its log now).
// Platforms: Windows-only calls (psapi, the IO counters, FindFirstFile). Linux: /proc/self/status (VmRSS, VmPeak) and
// /proc/self/io; macOS: task_info (TASK_VM_INFO) and proc_pid_rusage (ri_diskio_bytesread / written).
#include "app.hpp"  // windows.h first: psapi.h needs its types

#include "build_stamp.h"  // the build's identity, the log's first line (diag/)
#include <dxgi1_4.h>  // graphics memory for the performance log (perf_tick)
#include <io.h>  // _get_osfhandle: stderr into the log (log_open)
#include <psapi.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace studio {

namespace {
std::FILE* s_log = nullptr;
bool s_detail = false;
std::mutex s_mutex;
DWORD s_start = 0;
Usage s_last, s_base;
bool s_have_base = false;
std::vector<std::string> s_warnings;  // language keys
std::wstring s_log_stem;  // the log's path without ".log" (the performance CSV goes beside it: perf_tick)
int s_warn_count = 0, s_error_count = 0;  // for the closing summary (log_exit)
std::wstring s_prev_unclean;  // the previous run's log when it didn't end cleanly (previous_run_unclean)
// (4.3 part 3, the logging list's items 12 and 16) the window the UI thread is working for (its lines carry "w<n>";
// other threads' lines don't, since they may work for any window), and the run's highs for the closing summary
int s_window = 0;
DWORD s_ui_thread = 0;
int s_most_windows = 0, s_most_tabs = 0;
double s_slowest_ms = 0;
std::string s_slowest;
// the clock as "2026-11-22 23:16:35" (local time)
std::string clock_text(bool date) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    char b[32];
    if (date) std::snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    else std::snprintf(b, sizeof b, "%02d:%02d:%02d", t.wHour, t.wMinute, t.wSecond);
    return b;
}

// Whether an earlier log ended cleanly: its last lines hold the closing line ("[exit] clean", or "[app] closed" from
// builds before 2026-11-22). A log still held open by another running Studio isn't a crash: it's tried with no sharing
// first, and one that can't be opened that way is in use. Windows (CreateFileW's share modes); Linux / macOS: a lock
// file or flock beside the log (likely).
// *Updated 2026-11-23 (Studio's bar back on, snatcherLeo: "Yes, turn it on"; OrrePort's rule, the project's 8ef34d4b8, the same mark
// and format):* a log is judged only when its first 4 KB say "[log] exit line: on" (builds that write the closing line
// say so at the top), so the first start after updating from an older build isn't taken for a crash; then its last 8 KB
// must hold "[exit] clean".
bool log_ended_cleanly(const std::wstring& path) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return true;  // (in use by another Studio, or unreadable: no bar either way)
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    DWORD got = 0;
    std::string head((size_t)std::min<LONGLONG>(size.QuadPart, 4096), '\0');
    ReadFile(f, head.data(), (DWORD)head.size(), &got, nullptr);
    head.resize(got);
    if (head.find("[log] exit line: on") == std::string::npos) { CloseHandle(f); return true; }  // (not judged)
    const LONGLONG tail = std::min<LONGLONG>(size.QuadPart, 8192);
    std::string s((size_t)tail, '\0');
    LARGE_INTEGER at{};
    at.QuadPart = size.QuadPart - tail;
    SetFilePointerEx(f, at, nullptr, FILE_BEGIN);
    got = 0;
    ReadFile(f, s.data(), (DWORD)tail, &got, nullptr);
    CloseHandle(f);
    s.resize(got);
    return s.find("[exit] clean") != std::string::npos;
}
}  // namespace

std::wstring previous_run_unclean() { return s_prev_unclean; }

// The last line of a clean close (4.3 part 3, the logging list's items 9 and 16, first part): how the session went, so
// a log that lacks this line is known to have ended in a crash or a kill (the next start offers a report then)
void log_exit(int windows, size_t tabs) {
    const Usage u = usage_now();
    log_note_open(windows, (int)tabs);
    logf(Level::Info, "exit", "clean at %s: %d window(s), %zu tab(s); %d warning(s), %d error(s); memory peak %.0f MB working set, %.0f MB private; "
         "at most %d window(s) and %d tab(s) open; the slowest step: %s (%.0f ms)",
         clock_text(false).c_str(), windows, tabs, s_warn_count, s_error_count, u.peak_mb, u.private_mb, s_most_windows, s_most_tabs,
         s_slowest.empty() ? "none timed" : s_slowest.c_str(), s_slowest_ms);
}

void log_set_window(int window_no) { s_window = window_no; }
void log_note_open(int windows, int tabs) {
    s_most_windows = std::max(s_most_windows, windows);
    s_most_tabs = std::max(s_most_tabs, tabs);
}
void log_note_step(const char* what, double ms) {
    if (ms <= s_slowest_ms) return;
    s_slowest_ms = ms;
    s_slowest = what;
}

void log_open(const std::wstring& folder, bool detail) {
    s_detail = detail;
    s_start = GetTickCount();
    CreateDirectoryW(folder.c_str(), nullptr);
    // rotation: keep the 9 newest earlier logs (names sort by date and time)
    std::vector<std::wstring> old;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((folder + L"\\studio-*.log").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do old.push_back(fd.cFileName);
        while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(old.begin(), old.end());
    // (a crashed run's dump, studio-<stamp>.dmp beside its log, goes with it: pc_crash_set_dump_path, set in run())
    for (size_t i = 0; i + 9 < old.size(); ++i) {
        DeleteFileW((folder + L"\\" + old[i]).c_str());
        DeleteFileW((folder + L"\\" + old[i].substr(0, old[i].size() - 4) + L".dmp").c_str());
    }
    // the previous run: did it end cleanly? (its log is the newest earlier one; the bar offering a report reads this)
    if (!old.empty() && !log_ended_cleanly(folder + L"\\" + old.back())) s_prev_unclean = folder + L"\\" + old.back();
    // the performance logs beside them (perf_tick), kept the same way: the 9 newest earlier ones
    std::vector<std::wstring> perf;
    h = FindFirstFileW((folder + L"\\studio-*_perf.csv").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do perf.push_back(fd.cFileName);
        while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(perf.begin(), perf.end());
    for (size_t i = 0; i + 9 < perf.size(); ++i) DeleteFileW((folder + L"\\" + perf[i]).c_str());
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf(name, 64, L"\\studio-%04d%02d%02d-%02d%02d%02d.log", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    s_last = usage_now();
    // A crash's report into this same log (2026-11-22, after snatcherLeo's crash left no trace): OrrePort's crash handler
    // (pc_crash_win32.c, linked in) writes the exception and call stack to stderr, which a program started from a
    // shortcut doesn't have. When there is none, the log is opened as stderr itself (one file handle, one position: two
    // handles on the file overwrote its first lines in the test), so the report lands under the last thing Studio did.
    // Started from a console (tests), stderr stays the console and the log is a file of its own. The exe's load address and link
    // time are logged so the stack's addresses can be turned into function names for this exact build
    // (address - load address + 0x140000000, then the exe's symbols). Interim: step 4.2e's shared diagnostics library
    // replaces this with OrrePort's crash file and Report a problem. Windows only; Linux / macOS: pc_crash_posix.c
    // writes to stderr too, and the same redirect (freopen) works there.
    const HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    if (err == nullptr || err == INVALID_HANDLE_VALUE || GetFileType(err) == FILE_TYPE_UNKNOWN) {
        s_log = _wfreopen((folder + name).c_str(), L"w", stderr);
        if (s_log) SetStdHandle(STD_ERROR_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(s_log))));
    }
    if (!s_log) s_log = _wfopen((folder + name).c_str(), L"w");
    s_log_stem = folder + std::wstring(name).substr(0, wcslen(name) - 4);  // (name minus ".log")
    const auto* base = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    s_ui_thread = GetCurrentThreadId();  // (the thread whose lines carry the window tag: log_open runs on the UI thread)
    // which build this is, first of all (4.3 part 3, the logging list's item 1: commit, uncommitted changes, branch,
    // build time, main or test build; diag/build_stamp.h), then the clock (item 13: each line's own time counts seconds
    // from this start; the "[state]" line repeats the clock every 30 s)
    logf(Level::Info, "app", "%s", diag_build_stamp());
    logf(Level::Info, "log", "exit line: on");  // (this build ends every normal run with "[exit] clean": the next start may judge it)
    logf(Level::Info, "app", "started %s (local time)", clock_text(true).c_str());
    logf(Level::Info, "app", "exe loaded at 0x%llx, linked %08lx (for crash reports)", (unsigned long long)(uintptr_t)base,
         (unsigned long)nt->FileHeader.TimeDateStamp);
}

void logf(Level lv, const char* area, const char* fmt, ...) {
    if (lv == Level::Detail && !s_detail) return;
    std::lock_guard<std::mutex> g(s_mutex);
    if (!s_log) return;
    if (lv == Level::Error) ++s_error_count;
    else if (lv == Level::Warn) ++s_warn_count;
    static const char* names[] = {"ERROR", "warn", "info", "detail"};
    // the window tag (item 12): "w1", "w2"... on the UI thread's lines once a window exists, blank otherwise, so the
    // columns stay lined up
    char tag[8] = "";
    if (s_window > 0 && GetCurrentThreadId() == s_ui_thread) std::snprintf(tag, sizeof tag, "w%d", s_window);
    std::fprintf(s_log, "%8.3f %-6s %-3s [%s] ", (GetTickCount() - s_start) / 1000.0, names[(int)lv], tag, area);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(s_log, fmt, ap);
    va_end(ap);
    std::fputc('\n', s_log);
    std::fflush(s_log);
}

Usage usage_now() {
    Usage u;
    PROCESS_MEMORY_COUNTERS_EX pm{};
    pm.cb = sizeof pm;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pm), sizeof pm)) {
        u.working_mb = pm.WorkingSetSize / 1048576.0;
        u.private_mb = pm.PrivateUsage / 1048576.0;
        u.peak_mb = pm.PeakWorkingSetSize / 1048576.0;
    }
    IO_COUNTERS io{};
    if (GetProcessIoCounters(GetCurrentProcess(), &io)) {
        u.read_mb = io.ReadTransferCount / 1048576.0;
        u.written_mb = io.WriteTransferCount / 1048576.0;
    }
    return u;
}

// the memory check after a piece of work ("[watch] after ..."), or, with `state`, the program's state line every 30 s
// ("[state] 23:17:05, 2 windows: w1 4 tabs (in front), w2 1 tab; memory ..."; items 13 and 14, replacing the earlier
// per-window "after 30 s while in front" lines). Both check the same budgets.
void watch_line(const char* after, const char* state);
// Reads and writes up to now are left out of the next check (Report a problem's temporary full-size screenshots,
// about 30 MB at 4K, set off the "more than 5 MB written" warning in the very log the report carried; 2026-11-22)
void watch_forget_io() {
    const Usage u = usage_now();
    s_last.read_mb = u.read_mb;
    s_last.written_mb = u.written_mb;
}
void watch_check(const char* after) { watch_line(after, nullptr); }
void log_state(const char* windows) { watch_line("the last 30 s", windows); }
void watch_line(const char* after, const char* state) {
    const Usage u = usage_now();
    const double read = u.read_mb - s_last.read_mb, written = u.written_mb - s_last.written_mb;
    if (state)
        logf(Level::Info, "state", "%s, %s; memory: private %.1f MB, working set %.1f MB (peak %.1f); read %.1f MB, wrote %.2f MB in the last 30 s",
             clock_text(false).c_str(), state, u.private_mb, u.working_mb, u.peak_mb, read, written);
    else
        logf(Level::Info, "watch", "after %s: private %.1f MB, working set %.1f MB (peak %.1f), read %.1f MB, wrote %.2f MB",
             after, u.private_mb, u.working_mb, u.peak_mb, read, written);
    // the log line stays English (logs are for fixing problems); the developer view shows the translated key
    auto warn = [&](const char* key, const char* what) {
        s_warnings.push_back(key);
        if (s_warnings.size() > 20) s_warnings.erase(s_warnings.begin());
        logf(Level::Warn, "watch", "%s (after %s)", what, after);
    };
    // the two memory warnings once per new high, 50 MB past the last warned level (*changed 2026-11-22*: snatcherLeo's 3-minute
    // session logged the growth warning after nearly every pick, 200 lines burying the rest, while memory sat flat at
    // about 435 MB: the WebGPU / driver pools and the pipeline cache filling once, then reused; likely, not a leak)
    static double warned_private = 0, warned_growth = 0;
    if (u.private_mb > 600 && u.private_mb > warned_private + 50) {
        warned_private = u.private_mb;
        warn("studio.dev.warn.private", "private memory above the 600 MB budget");
    }
    if (s_have_base && u.private_mb > s_base.private_mb + 150 && u.private_mb > warned_growth + 50) {
        warned_growth = u.private_mb;
        warn("studio.dev.warn.growth", "memory has grown 150 MB past its level after loading");
    }
    if (read > 200) warn("studio.dev.warn.read", "one piece of work read more than 200 MB");
    if (written > 5) warn("studio.dev.warn.written", "more than 5 MB written since the last check");
    if (!s_have_base && std::string(after) == "first load") {
        s_base = u;
        s_have_base = true;
    }
    s_last = u;
}

// ---- the performance log (part 4, snatcherLeo 2026-11-22: "In case the app is using too much RAM, system performance is
// too high, etc.") ----
// One row a second in studio-<stamp>_perf.csv, flushed per row (a crash keeps every row before it). The first seven
// columns are OrrePort's (pc_health.c, 2026-11-22: "keep those first seven columns in the same order"), then
// Studio's own: seconds, where, frames, avg_ms, worst_ms, private_mb, ram_mb, cpu_pct, gpu_mb.
//   frames / avg_ms / worst_ms: the 3D views' frames this second and the CPU time to draw them (viewer_frame_stats);
//   private_mb / ram_mb: private memory and the working set (usage_now);
//   cpu_pct: Studio's share of the whole machine's processor time this second (Task Manager's way), GetProcessTimes;
//   gpu_mb: this program's graphics memory on every adapter (DXGI QueryVideoMemoryInfo; -1 if it can't be read).
// Budgets (first guesses, to tune as Studio grows; each warning at most once a minute): a frame over 250 ms (the 3D
// view stalled); an average over 20 ms for 5 seconds running (the view runs slow, below 50 frames a second); the
// processor over 50% of the machine for 10 seconds running; graphics memory over 2048 MB. Memory itself stays with
// watch_check's budgets.
// Platforms: GetProcessTimes and DXGI are Windows'. Linux: /proc/self/stat (utime, stime) and the GPU driver's own
// query (Vulkan's VK_EXT_memory_budget through Dawn, likely); macOS: proc_pid_rusage and Metal's
// currentAllocatedSize (likely).
namespace {
std::FILE* s_perf = nullptr;
int s_perf_rows = 0;
ULONGLONG s_cpu_last = 0, s_wall_last = 0;
int s_slow_run = 0, s_busy_run = 0;
DWORD s_warned[4] = {0, 0, 0, 0};

double cpu_pct_now() {
    FILETIME c, e, k, u;
    if (!GetProcessTimes(GetCurrentProcess(), &c, &e, &k, &u)) return -1;
    auto v = [](const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; };
    const ULONGLONG cpu = v(k) + v(u);  // (100 ns units)
    FILETIME now;
    GetSystemTimeAsFileTime(&now);
    const ULONGLONG wall = v(now);
    double pct = -1;
    if (s_wall_last && wall > s_wall_last) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        pct = 100.0 * (double)(cpu - s_cpu_last) / ((double)(wall - s_wall_last) * std::max<DWORD>(1, si.dwNumberOfProcessors));
    }
    s_cpu_last = cpu;
    s_wall_last = wall;
    return pct;
}

double gpu_mb_now() {
    static IDXGIFactory4* factory = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), reinterpret_cast<void**>(&factory)))) factory = nullptr;
    }
    if (!factory) return -1;
    double total = 0;
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; ++i) {
        IDXGIAdapter3* a3 = nullptr;
        if (SUCCEEDED(a->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&a3)))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO local{}, shared{};
            if (SUCCEEDED(a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local))) total += (double)local.CurrentUsage;
            if (SUCCEEDED(a3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &shared))) total += (double)shared.CurrentUsage;
            a3->Release();
        }
        a->Release();
    }
    return total / 1048576.0;
}

// a warning of kind k, at most once a minute
void perf_warn(int k, const char* fmt, double a, double b) {
    const DWORD now = GetTickCount();
    if (s_warned[k] && now - s_warned[k] < 60000) return;
    s_warned[k] = now ? now : 1;
    char what[200];
    std::snprintf(what, sizeof what, fmt, a, b);
    logf(Level::Warn, "perf", "%s", what);
}
}  // namespace

void perf_tick(const char* where) {
    if (s_log_stem.empty()) return;
    if (!s_perf) {
        s_perf = _wfopen((s_log_stem + L"_perf.csv").c_str(), L"w");
        if (!s_perf) { s_log_stem.clear(); return; }
        std::fprintf(s_perf, "# OrrePort Studio performance, one row a second (studio/win32/log.cpp perf_tick)\n"
                             "seconds,where,frames,avg_ms,worst_ms,private_mb,ram_mb,cpu_pct,gpu_mb,page_frames,page_avg_ms,page_worst_ms\n");
    }
    int frames, pframes;
    double avg, worst, pavg, pworst;
    viewer_frame_stats(frames, avg, worst);
    page_frame_stats(pframes, pavg, pworst);  // (the page's draws: added 2026-11-22, the columns after gpu_mb)
    const Usage u = usage_now();
    const double cpu = cpu_pct_now(), gpu = gpu_mb_now();
    std::fprintf(s_perf, "%.1f,%s,%d,%.2f,%.2f,%.1f,%.1f,%.1f,%.1f,%d,%.2f,%.2f\n", (GetTickCount() - s_start) / 1000.0, where,
                 frames, avg, worst, u.private_mb, u.working_mb, cpu, gpu, pframes, pavg, pworst);
    std::fflush(s_perf);
    ++s_perf_rows;
    if (worst > 250) perf_warn(0, "the 3D view stalled: a frame took %.0f ms (%.0f frames this second)", worst, frames);
    s_slow_run = frames >= 10 && avg > 20 ? s_slow_run + 1 : 0;
    if (s_slow_run >= 5) perf_warn(1, "the 3D view runs slow: %.1f ms a frame for 5 seconds (%.0f frames a second)", avg, frames);
    s_busy_run = cpu > 50 ? s_busy_run + 1 : 0;
    if (s_busy_run >= 10) perf_warn(2, "the processor stayed busy: %.0f%% of the machine for 10 seconds (%.0f)", cpu, 10);
    if (gpu > 2048) perf_warn(3, "graphics memory above the 2048 MB budget: %.0f MB (%.0f)", gpu, 0);
}

// this run's two files, for Report a problem's zip (4.3 part 3): their paths, and everything written so far flushed
std::wstring log_file_path() { return s_log ? s_log_stem + L".log" : std::wstring(); }
std::wstring perf_file_path() { return s_perf ? s_log_stem + L"_perf.csv" : std::wstring(); }
void log_flush() {
    std::lock_guard<std::mutex> g(s_mutex);
    if (s_log) std::fflush(s_log);
    if (s_perf) std::fflush(s_perf);
}

// (the log has its own time column, so the trail's leading time is skipped: "  12.345 click   ..." -> "click   ...")
void log_trail_line(const char* line) {
    const char* p = line;
    while (*p == ' ') ++p;
    while (*p && *p != ' ') ++p;
    while (*p == ' ') ++p;
    logf(Level::Info, "do", "%s", *p ? p : line);
}

std::vector<std::wstring> watch_lines() {
    const Usage u = usage_now();
    auto mb = [](double v) {
        wchar_t b[32];
        swprintf(b, 32, L"%.1f", v);
        return std::wstring(b);
    };
    std::vector<std::wstring> l;
    l.push_back(tx("studio.dev.memory", {{"private", mb(u.private_mb)}, {"working", mb(u.working_mb)}, {"peak", mb(u.peak_mb)}}));
    l.push_back(tx("studio.dev.disk", {{"read", mb(u.read_mb)}, {"written", mb(u.written_mb)}}));
    if (s_have_base) l.push_back(tx("studio.dev.base", {{"private", mb(s_base.private_mb)}}));
    l.push_back(tx(s_warnings.empty() ? "studio.dev.ok" : "studio.dev.out_of_place"));
    for (const auto& w : s_warnings) l.push_back(L"  " + tx(w.c_str()));
    return l;
}

}  // namespace studio
