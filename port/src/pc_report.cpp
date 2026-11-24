/*
 * pc_report.cpp: Report a problem's zip (the RmlUi trial; plan "Reports and crashes", decided by snatcherLeo 2026-11-09).
 * One zip in <exe folder>/Reports/, then File Explorer opens on it with the zip selected, so a tester can drag it
 * straight into the Discord #reports channel.
 *
 * First version (the project's suggestion, the project's order): the note, about.txt (build stamp, preset, PC specs: CPU, RAM, GPU
 * lines, Windows version, display, controllers), this run's log, settings.ini, the save card's files, this sitting's
 * recording and roll record, and a screenshot of the game picture. The crash dump and frame capture come later.
 * Personal paths and the user name are replaced by <user> in every text file; nothing from the disc goes in.
 * The zip is written plainly (stored, no compression): no extra library, any unzip tool opens it.
 *
 * Platform layer only. Windows-only parts (CPU name, Windows version, Explorer) sit in #ifdef _WIN32 with a portable
 * fallback, so the file builds on Linux and Mac too.
 */
#include <SDL3/SDL.h>

#include "pc_debug.h" /* the game side's room and battle (pc_debug_api) */
#include "pc_health.h" /* the performance log (pc_health_perf_path) */
#include "pc_cheats.h" /* the cheats that are on (about.txt) */
#include "pc_paths.h"  /* Reports\, data\settings\, the saves (4.1) */
#include "pc_saves.h"  /* the slot being played (pc_saves_current) */
#include "pc_scrub.h"  /* personal details out (shared with Studio) */
#include "pc_zip.h"    /* the zip writer and UTF-8 file opening (shared with Studio) */

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" {
const char* pc_debug_build_stamp(void);                                     /* pc_debug.c */
const char* pc_log_path(void);                                              /* pc_log.c */
const char* pc_log_previous_unclean(void);                                  /* pc_log.c: a run that ended unexpectedly */
void pc_log_previous_unclean_clear(void);
const char* pc_pad_record_path(void);                                       /* pc_pad_record.c */
int pc_option_gamecube_accurate(void);                                      /* pc_options.c */
const char* pc_card_launch_backup(void);                                    /* pc_card.c */
extern volatile unsigned int pc_vi_heartbeat;                              /* pc_vi.c: the game frame count */
int pc_report_write(const char* note, const char* screenshot, char* out_zip, size_t out_size);
void pc_report_show_in_folder(const char* zip);
}

namespace {

/* ---- the zip's contents, gathered, then written in one go by the shared writer (pc_zip.c) ----
 * *Changed 2026-11-22 (4.3 part 3):* this file had its own stored-zip writer, a second copy of pc_zip.c's; the
 * report now gathers its files and hands them to pc_zip_write, which Studio's report shares (one writer to keep right).
 * pc_zip_write also writes <name>.new first and then puts it in place, so a failed report leaves no half zip. */
class Zip {
public:
    void add(const std::string& name, const std::string& data) { m_files.push_back({name, data}); }
    bool write(const std::string& path) const {
        std::vector<PcZipFile> list;
        list.reserve(m_files.size());
        for (const auto& f : m_files) list.push_back({f.first.c_str(), f.second.data(), f.second.size()});
        return pc_zip_write(path.c_str(), list.data(), (int)list.size()) != 0;
    }

private:
    std::vector<std::pair<std::string, std::string>> m_files;
};

/* ---- reading and scrubbing ---- */
/* the C runtime's fopen, not SDL_LoadFile: the log is still open for writing, and SDL's reader on Windows doesn't
 * share a file another handle is writing (the first test's zip came out without its log). Through pc_fopen_utf8
 * (*changed 2026-11-22*): plain fopen read a UTF-8 path in Windows' ANSI code page, so a home folder with other
 * letters left its files out of the report */
bool load(const std::string& path, std::string& out) {
    FILE* f = pc_fopen_utf8(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    out.clear();
    char buf[65536];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return true;
}

/* the user's home folder (both slash styles) and user name become <user>, the tools' scratch folder <scratch>
 * (*moved 2026-11-22* into pc_scrub.c, shared with Studio's report; the rule itself is unchanged) */
std::string scrub(const std::string& s) {
    size_t n = 0;
    char* out = pc_scrub(s.data(), s.size(), &n);
    if (out == nullptr) {
        return "(left out: no memory to remove personal details)\n"; /* never the unscrubbed text */
    }
    std::string r(out, n);
    pc_scrub_free(out);
    return r;
}

std::string base_name(const std::string& path) {
    const size_t i = path.find_last_of("/\\");
    return i == std::string::npos ? path : path.substr(i + 1);
}

/* ---- the PC snapshot ---- */
std::string pc_specs(const std::string& log_text) {
    std::string out;
    char line[512];
#ifdef _WIN32
    HKEY key;
    char cpu[256] = "";
    DWORD len = sizeof(cpu);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &key) ==
        ERROR_SUCCESS) {
        RegQueryValueExA(key, "ProcessorNameString", nullptr, nullptr, reinterpret_cast<BYTE*>(cpu), &len);
        RegCloseKey(key);
    }
    std::snprintf(line, sizeof(line), "CPU: %s (%d logical cores)\n", cpu, SDL_GetNumLogicalCPUCores());
    out += line;
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    OSVERSIONINFOW v{};
    v.dwOSVersionInfoSize = sizeof(v);
    if (HMODULE ntdll = GetModuleHandleA("ntdll.dll")) {
        if (auto fn = reinterpret_cast<RtlGetVersionFn>(reinterpret_cast<void*>(GetProcAddress(ntdll, "RtlGetVersion")))) {
            fn(&v);
        }
    }
    std::snprintf(line, sizeof(line), "Windows: %lu.%lu build %lu\n", v.dwMajorVersion, v.dwMinorVersion,
                  v.dwBuildNumber);
    out += line;
#else
    std::snprintf(line, sizeof(line), "CPU: %d logical cores\nSystem: %s\n", SDL_GetNumLogicalCPUCores(),
                  SDL_GetPlatform());
    out += line;
#endif
    std::snprintf(line, sizeof(line), "RAM: %d MB\n", SDL_GetSystemRAM());
    out += line;
    /* the display the game window is on, with Windows' scaling (SDL's content scale: 2.25 at 225%; the mode's
     * pixel_density said 1.00 on snatcherLeo's 225% screen), and the game window itself */
    SDL_Window* window = SDL_GetKeyboardFocus();
    if (window == nullptr) {
        int nw = 0;
        if (SDL_Window** all = SDL_GetWindows(&nw)) {
            window = nw > 0 ? all[0] : nullptr;
            SDL_free(all);
        }
    }
    const SDL_DisplayID display = window != nullptr ? SDL_GetDisplayForWindow(window) : SDL_GetPrimaryDisplay();
    if (const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(display)) {
        std::snprintf(line, sizeof(line), "Display: %d x %d at %.2f Hz, Windows scaling %.0f%%\n", m->w, m->h,
                      m->refresh_rate, SDL_GetDisplayContentScale(display) * 100.0f);
        out += line;
    }
    if (window != nullptr) {
        int w = 0, h = 0, pw = 0, ph = 0;
        SDL_GetWindowSize(window, &w, &h);
        SDL_GetWindowSizeInPixels(window, &pw, &ph);
        std::snprintf(line, sizeof(line), "Game window: %d x %d pixels (%d x %d logical), %s\n", pw, ph, w, h,
                      (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) ? "fullscreen"
                      : (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) ? "maximized"
                                                                             : "windowed");
        out += line;
    }
    int n = 0;
    if (SDL_JoystickID* pads = SDL_GetGamepads(&n)) {
        for (int i = 0; i < n; ++i) {
            const char* name = SDL_GetGamepadNameForID(pads[i]);
            std::snprintf(line, sizeof(line), "Controller %d: %s\n", i + 1, name != nullptr ? name : "(unnamed)");
            out += line;
        }
        SDL_free(pads);
    }
    if (n == 0) {
        out += "Controllers: none connected\n";
    }
    /* the graphics adapter, driver and backend, as Aurora logged them at start-up */
    out += "\nGraphics (from the log):\n";
    /* each "aurora::gpu" entry with the lines that continue it (the adapter's details, driver version included,
     * follow on lines that don't start a new "[" entry) */
    size_t pos = 0;
    while ((pos = log_text.find("aurora::gpu", pos)) != std::string::npos) {
        const size_t a = log_text.rfind('\n', pos);
        size_t b = log_text.find('\n', pos);
        while (b != std::string::npos && b + 1 < log_text.size() && log_text[b + 1] != '[') {
            b = log_text.find('\n', b + 1);
        }
        const size_t from = a == std::string::npos ? 0 : a + 1;
        out += log_text.substr(from, (b == std::string::npos ? log_text.size() : b) - from);
        out += "\n";
        pos = b == std::string::npos ? log_text.size() : b;
    }
    return out;
}

} // namespace

/* Writes the report; returns 1 and the zip's path, or 0 */
extern "C" int pc_report_write(const char* note, const char* screenshot, char* out_zip, size_t out_size) {
    const std::time_t now = std::time(nullptr);
    const std::tm* t = std::localtime(&now);
    char stamp[64];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", t);
    char reports[1300];
    const std::string dir = pc_paths_dir(PC_PLACE_REPORTS, reports, sizeof(reports)); /* flat layout: Reports, 4.1 */
    SDL_CreateDirectory(dir.c_str());
    /* a unique name (snatcherLeo, 2026-11-09: "when multiple users create those zip files, they all be named different
     * things right?"): the date and time, plus a short random code, and never an existing file's name */
    std::string zip_path;
    for (int attempt = 0; attempt < 16; ++attempt) {
        char code[8];
        std::snprintf(code, sizeof(code), "%04x", (unsigned)(SDL_rand_bits() & 0xFFFF));
        zip_path = dir + "/OrrePort-report_" + stamp + "_" + code + ".zip";
        if (!SDL_GetPathInfo(zip_path.c_str(), nullptr)) {
            break; /* no such file yet */
        }
    }
    Zip zip;
    std::string log_text;
    const char* log_path = pc_log_path();
    if (log_path != nullptr) {
        std::fflush(stdout);
        std::fflush(stderr);
        load(log_path, log_text);
    }
    char when[64];
    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", t);
    /* the moment: the game frame, the room and the battle when the report was opened */
    std::string moment = "Moment: game frame " + std::to_string((unsigned)pc_vi_heartbeat);
    /* the game's room and battle only once the game runs: from the hub, before Play, the game's data isn't set up
     * yet, and asking for the room read a null pointer (snatcherLeo, 2026-11-10: clicking Report a problem in the hub
     * crashed the program; std092_getCurrentRoomID via dbg_current_room) */
    const PcDebugApi* api = pc_vi_heartbeat > 0 ? pc_debug_api() : nullptr;
    if (pc_vi_heartbeat == 0) {
        moment += ", in the hub (the game had not started)";
    }
    if (api != nullptr) {
        if (api->current_room != nullptr) {
            const unsigned room = (unsigned)api->current_room();
            char buf[96];
            std::snprintf(buf, sizeof(buf), ", room 0x%X", room);
            moment += buf;
            if (api->room_name != nullptr) {
                if (const char* name = api->room_name(room)) {
                    moment += std::string(" (") + name + ")";
                }
            }
        }
        if (api->battle_number != nullptr) {
            const unsigned battle = (unsigned)api->battle_number();
            moment += battle != 0 ? ", battle " + std::to_string(battle) : ", not in a battle";
        }
    }
    moment += "\n";
    zip.add("note.txt", scrub(std::string("What happened (the tester's note):\n") +
                              (note != nullptr && note[0] != '\0' ? note : "(no note)") + "\n\nSaved: " + when + "\n"));
    /* the cheats on when the report was made (Cheats menu, mockup 63 v2), so a strange result is read knowing them;
     * their names in the player's language, the rest of about.txt is for us and stays English */
    char cheats[512];
    pc_cheats_list(cheats, sizeof(cheats));
    zip.add("about.txt", scrub(moment + "Build: " + pc_debug_build_stamp() + "\nPreset: " +
                               (pc_option_gamecube_accurate() ? "GameCube-accurate" : "Normal") + "\nCheats on: " +
                               (cheats[0] != '\0' ? cheats : "none") + "\n\n" + pc_specs(log_text)));
    if (!log_text.empty()) {
        zip.add("log/" + base_name(log_path), scrub(log_text));
    }
    /* the performance log (pc_health.c: frame times and memory, a row a second), so a report says how the app ran, not
     * only what the game did (snatcherLeo, 2026-11-13: "SEparate loggers for the app performance"). About 50 bytes a second,
     * so a long session is a few hundred KB; past 1 MB only its header and the last 1 MB are kept */
    if (const char* perf_path = pc_health_perf_path()) {
        std::string perf;
        if (load(perf_path, perf)) {
            constexpr size_t kMax = 1024 * 1024;
            if (perf.size() > kMax) {
                const size_t head = perf.find('\n', perf.find('\n') + 1) + 1; /* the two header lines */
                const size_t tail = perf.find('\n', perf.size() - kMax) + 1;
                perf = perf.substr(0, head) + "...\n" + perf.substr(tail);
            }
            zip.add("log/" + base_name(perf_path), perf);
        }
    }
    std::string text;
    if (load(pc_paths_str(PC_PLACE_SETTINGS, "settings.ini"), text)) { /* data\settings\ (flat layout: user\), 4.1 */
        zip.add("settings.ini", scrub(text));
    }
    /* the save as it is now (save/now/) and the one this sitting started from (save/start/: this launch's backup,
     * which the recording replays from) */
    auto add_saves = [&zip](const std::string& folder, const std::string& into) {
        int count = 0;
        if (char** files = SDL_GlobDirectory(folder.c_str(), "*.gci", 0, &count)) {
            for (int i = 0; i < count; ++i) {
                std::string data;
                if (load(folder + "/" + files[i], data)) {
                    zip.add(into + files[i], data);
                }
            }
            SDL_free(files);
        }
    };
    /* *Fixed 2026-11-21 (4.1):* the slot being played, not always Slot 1 (the report sent Slot 1's save whenever the
     * player played another slot). pc_saves_current() is the slot the card has open (the hub's pick, or LOAD's switch,
     * pc_card_switch). Without save slots (a flat test folder with no Colosseum\), Aurora's single card in the settings
     * folder, as before 4.1's paths. All platforms. */
    if (pc_saves_ready()) {
        char slot_dir[1300];
        add_saves(pc_saves_slot_dir(pc_saves_current(), slot_dir, sizeof(slot_dir)), "save/now/");
    } else {
        add_saves(pc_paths_str(PC_PLACE_SETTINGS, "USA/Card A"), "save/now/");
    }
    const std::string start = pc_card_launch_backup();
    if (!start.empty()) {
        add_saves(start, "save/start/");
    }
    const std::string rec = pc_pad_record_path();
    if (!rec.empty()) {
        std::string data;
        if (load(rec, data)) {
            zip.add("recording/" + base_name(rec), data);
        }
        const size_t dot = rec.find_last_of('.');
        if (dot != std::string::npos && load(rec.substr(0, dot) + ".rolls", data)) {
            zip.add("recording/" + base_name(rec.substr(0, dot)) + ".rolls", data);
        }
    }
    if (screenshot != nullptr) {
        std::string data;
        if (load(screenshot, data)) {
            zip.add("screenshot.png", data);
            SDL_RemovePath(screenshot); /* it lives on in the zip */
        }
    }
    /* the run that ended unexpectedly (5.1, 2026-11-23; pc_log.h): its log rides along, scrubbed like this run's, under
     * log/previous/, so the report says what happened then and not only now. Its crash dump (.dmp) stays out: a dump
     * holds folder paths with the player's user name, which can't be scrubbed from a binary file (a later choice) */
    const char* previous = pc_log_previous_unclean();
    if (previous != nullptr) {
        std::string prev_text;
        if (load(previous, prev_text)) zip.add("log/previous/" + base_name(previous), scrub(prev_text));
    }
    if (!zip.write(zip_path)) {
        std::fprintf(stderr, "[report] writing %s failed\n", scrub(zip_path).c_str());
        return 0;
    }
    std::snprintf(out_zip, out_size, "%s", zip_path.c_str());
    std::printf("[report] saved %s%s\n", scrub(zip_path).c_str(),
                previous != nullptr ? " (with the earlier run that ended unexpectedly)" : "");
    if (previous != nullptr) pc_log_previous_unclean_clear(); /* carried once; the hub's bar then goes */
    return 1;
}

/* File Explorer on the zip's folder with the zip selected (Windows); the folder itself elsewhere */
extern "C" void pc_report_show_in_folder(const char* zip) {
#ifdef _WIN32
    std::string p = zip;
    for (char& c : p) c = c == '/' ? '\\' : c;
    /* the wide call (*changed 2026-11-22*): the path is UTF-8, which CreateProcessA would read in the ANSI code page */
    const std::string cmd = "explorer.exe /select,\"" + p + "\"";
    std::wstring wcmd(MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, wcmd.data(), (int)wcmd.size());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!wcmd.empty() && CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
#else
    std::string dir = zip;
    const size_t i = dir.find_last_of('/');
    if (i != std::string::npos) {
        dir.resize(i);
    }
    SDL_OpenURL(("file://" + dir).c_str());
#endif
}
