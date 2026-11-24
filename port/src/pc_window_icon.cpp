/*
 * pc_window_icon.cpp: OrrePort's O as the window's icon (title bar, taskbar, Alt+Tab), the RmlUi trial.
 * snatcherLeo, 2026-11-09 ("I'd go with your recommendations", via C): the small O glyph at 32 px and below, the full icon
 * from 48 px up; its colour follows Settings > Appearance (sage by default; rust, white).
 *
 * The brand SVGs are drawn here directly: each is a 512 x 512 canvas of filled shapes made only of straight lines
 * (path commands M, L, Z with absolute coordinates, fill-rule evenodd) plus, in the full icon, one rounded square
 * (rect with rx). That small subset is all this reads (checked on every icon file, 2026-11-09); anything else in a
 * file is ignored. Drawn with 4 x 4 samples per pixel for smooth edges. No library.
 *
 * Platform layer only. The files are ui/icons/orreport-icon-small-<colour>-glyph.svg and orreport-icon-<colour>.svg.
 */
#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "pc_parallel.h" /* the icon's sizes drawn side by side (apply_icons) */
#include "pc_paths.h"    /* the UI folder (4.1) */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern "C" {
void pc_window_icon_set(const char* colour);
void pc_window_icon_game(int on);
int pc_window_icon_game_available(void);
void pc_window_title_bar_style(int dark);
int pc_disc_dol_try_read(void* dst, uint32_t addr, uint32_t size, uint32_t crc); /* pc_disc_check.c */
}

namespace {
void note_theme(int dark); /* the title bar's O follows light / dark (below) */

struct Shape {
    uint8_t r = 0, g = 0, b = 0;
    bool rect = false;
    float rx = 0, w = 512, h = 512;                    /* rect */
    std::vector<std::vector<std::pair<float, float>>> rings; /* path: closed sub-paths */
};

bool load_text(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        return false;
    }
    char buf[8192];
    size_t n;
    out.clear();
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    std::fclose(f);
    return true;
}

std::string attr(const std::string& tag, const char* name) {
    const std::string key = std::string(" ") + name + "=\"";
    const size_t a = tag.find(key);
    if (a == std::string::npos) {
        return "";
    }
    const size_t b = tag.find('"', a + key.size());
    return b == std::string::npos ? "" : tag.substr(a + key.size(), b - a - key.size());
}

bool colour(const std::string& hex, Shape& s) {
    if (hex.size() != 7 || hex[0] != '#') {
        return false;
    }
    const long v = std::strtol(hex.c_str() + 1, nullptr, 16);
    s.r = (uint8_t)(v >> 16);
    s.g = (uint8_t)(v >> 8);
    s.b = (uint8_t)v;
    return true;
}

std::vector<Shape> parse(const std::string& svg) {
    std::vector<Shape> shapes;
    size_t pos = 0;
    while ((pos = svg.find('<', pos)) != std::string::npos) {
        const size_t end = svg.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        const std::string tag = svg.substr(pos, end - pos + 1);
        pos = end;
        const bool is_rect = tag.rfind("<rect", 0) == 0, is_path = tag.rfind("<path", 0) == 0;
        if (!is_rect && !is_path) {
            continue;
        }
        Shape s;
        if (!colour(attr(tag, "fill"), s)) {
            continue;
        }
        if (is_rect) {
            s.rect = true;
            s.w = std::strtof(attr(tag, "width").c_str(), nullptr);
            s.h = std::strtof(attr(tag, "height").c_str(), nullptr);
            s.rx = std::strtof(attr(tag, "rx").c_str(), nullptr);
        } else {
            const std::string d = attr(tag, "d");
            const char* p = d.c_str();
            while (*p != '\0') {
                while (*p == ' ') ++p;
                const char cmd = *p;
                if (cmd == 'M') {
                    s.rings.emplace_back();
                }
                if (cmd == 'M' || cmd == 'L') {
                    char* q;
                    const float x = std::strtof(p + 1, &q);
                    const float y = std::strtof(q, &q);
                    if (!s.rings.empty()) {
                        s.rings.back().emplace_back(x, y);
                    }
                    p = q;
                } else if (*p != '\0') {
                    ++p; /* Z, or anything this subset doesn't read */
                }
            }
        }
        shapes.push_back(std::move(s));
    }
    return shapes;
}

/* is (x, y) inside the shape (canvas units)? evenodd over every sub-path; the rect with rounded corners */
bool inside(const Shape& s, float x, float y) {
    if (s.rect) {
        if (x < 0 || y < 0 || x > s.w || y > s.h) {
            return false;
        }
        const float cx = x < s.rx ? s.rx : x > s.w - s.rx ? s.w - s.rx : x;
        const float cy = y < s.rx ? s.rx : y > s.h - s.rx ? s.h - s.rx : y;
        const float dx = x - cx, dy = y - cy;
        return dx * dx + dy * dy <= s.rx * s.rx;
    }
    bool in = false;
    for (const auto& ring : s.rings) {
        const size_t n = ring.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const float xi = ring[i].first, yi = ring[i].second, xj = ring[j].first, yj = ring[j].second;
            if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) {
                in = !in;
            }
        }
    }
    return in;
}

/* the shapes painted in order onto a size x size RGBA picture (transparent background) */
void draw_rows(const std::vector<Shape>& shapes, int size, SDL_Surface* surf, int y0, int y1);
SDL_Surface* draw(const std::vector<Shape>& shapes, int size) {
    SDL_Surface* surf = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32);
    if (surf == nullptr) {
        return nullptr;
    }
    /* the big sizes in eight bands of rows side by side (pc_parallel.h): 256 px alone was the slowest of the start's
     * icon pictures (2026-11-13). Each row is written by one band only. */
    const int bands = size >= 96 ? 8 : 1;
    std::vector<std::function<void()>> jobs;
    for (int band = 0; band < bands; ++band) {
        jobs.push_back([&, band] { draw_rows(shapes, size, surf, size * band / bands, size * (band + 1) / bands); });
    }
    pc_run_together(jobs);
    return surf;
}

void draw_rows(const std::vector<Shape>& shapes, int size, SDL_Surface* surf, int y0, int y1) {
    const float scale = 512.0f / (float)size;
    constexpr int SS = 4;
    for (int py = y0; py < y1; ++py) {
        uint8_t* row = static_cast<uint8_t*>(surf->pixels) + py * surf->pitch;
        for (int px = 0; px < size; ++px) {
            float r = 0, g = 0, b = 0, a = 0; /* premultiplied */
            for (const Shape& s : shapes) {
                int hits = 0;
                for (int sy = 0; sy < SS; ++sy) {
                    for (int sx = 0; sx < SS; ++sx) {
                        hits += inside(s, (px + (sx + 0.5f) / SS) * scale, (py + (sy + 0.5f) / SS) * scale) ? 1 : 0;
                    }
                }
                const float cov = hits / (float)(SS * SS);
                r = s.r / 255.0f * cov + r * (1 - cov);
                g = s.g / 255.0f * cov + g * (1 - cov);
                b = s.b / 255.0f * cov + b * (1 - cov);
                a = cov + a * (1 - cov);
            }
            uint8_t* o = row + px * 4;
            o[0] = (uint8_t)std::lround(a > 0 ? r / a * 255 : 0);
            o[1] = (uint8_t)std::lround(a > 0 ? g / a * 255 : 0);
            o[2] = (uint8_t)std::lround(a > 0 ? b / a * 255 : 0);
            o[3] = (uint8_t)std::lround(a * 255);
        }
    }
}

SDL_Window* game_window() {
    int n = 0;
    SDL_Window* w = nullptr;
    if (SDL_Window** all = SDL_GetWindows(&n)) {
        w = n > 0 ? all[0] : nullptr;
        SDL_free(all);
    }
    return w;
}

} // namespace

/* The title bar in the theme's colours (snatcherLeo, 2026-11-09: "Yes, both", after the project's suggestion to keep Windows' own
 * title bar, so snap layouts, dragging and the rest keep working, and colour it as mockup 11): Windows 11's caption,
 * text and border colours (DwmSetWindowAttribute 35 / 36 / 34; ignored before Windows 11 build 22000), Graphite dark
 * #161A18 / #E9EDEA / #2C322F, light #E8ECE7 / #18201B / #D5DCD4. dwmapi.dll is loaded at run time (no link change). */
extern "C" void pc_window_title_bar_style(int dark) {
    note_theme(dark);
#ifdef _WIN32
    SDL_Window* window = game_window();
    if (window == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    static HMODULE dwm = LoadLibraryA("dwmapi.dll");
    typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
    static SetAttr set = dwm != nullptr ? reinterpret_cast<SetAttr>(
                                              reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")))
                                        : nullptr;
    if (hwnd == nullptr || set == nullptr) {
        return;
    }
    auto rgb = [](unsigned hex) -> COLORREF { return RGB((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF); };
    const COLORREF caption = rgb(dark ? 0x161A18 : 0xE8ECE7);
    const COLORREF text = rgb(dark ? 0xE9EDEA : 0x18201B);
    const COLORREF border = rgb(dark ? 0x2C322F : 0xD5DCD4);
    const BOOL immersive_dark = dark ? TRUE : FALSE;
    set(hwnd, 20, &immersive_dark, sizeof(immersive_dark)); /* DWMWA_USE_IMMERSIVE_DARK_MODE: the button glyphs */
    set(hwnd, 35, &caption, sizeof(caption));                /* DWMWA_CAPTION_COLOR */
    set(hwnd, 36, &text, sizeof(text));                      /* DWMWA_TEXT_COLOR */
    set(hwnd, 34, &border, sizeof(border));                  /* DWMWA_BORDER_COLOR */
#else
    (void)dark;
#endif
}

/* Cloaking (Windows 8+, DWMWA_CLOAK): the window is shown and drawn to, but Windows keeps it invisible until it is
 * uncloaked. The trial cloaks the window as it is created and the hub uncloaks it once its first frames are drawn, so
 * the window first appears with the hub already on it: no empty window, black frame or rescale first (snatcherLeo,
 * 2026-11-09: "There's a slight flicker at the very begining of the hub being launched"). Hiding instead does not
 * work: Aurora draws nothing while its window is hidden. pc_game_main.c uncloaks after the hub in every case. */
extern "C" void pc_window_cloak(int on) {
#ifdef _WIN32
    static int s_cloaked = 0;
    if ((on != 0) == (s_cloaked != 0)) {
        return;
    }
    SDL_Window* window = game_window();
    if (window == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    static HMODULE dwm = LoadLibraryA("dwmapi.dll");
    typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
    static SetAttr set = dwm != nullptr ? reinterpret_cast<SetAttr>(
                                              reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")))
                                        : nullptr;
    if (hwnd == nullptr || set == nullptr) {
        return;
    }
    const BOOL cloak = on ? TRUE : FALSE;
    if (SUCCEEDED(set(hwnd, 13 /* DWMWA_CLOAK */, &cloak, sizeof(cloak)))) {
        s_cloaked = on;
        std::printf("[window] %s\n", on ? "cloaked until the hub is drawn" : "shown");
    }
#else
    (void)on;
#endif
}

/* Windows' own window animations (minimize, maximize, restore) off or on (DWMWA_TRANSITIONS_FORCEDISABLED, 3). Why:
 * leaving fullscreen into a maximized window, Windows played its maximize animation from the window's normal size, so
 * for a few frames the window shrank to its normal size in the middle of the screen with the desktop around it, then
 * grew back (snatcherLeo, 2026-11-13: "it is awkard in how the window resizes back to the maximized state"; seen in a 30 fps
 * screen recording, frames 217-223). pc_platform.c turns them off around a fullscreen switch only, so a normal
 * maximize or minimize keeps Windows' animation. Platforms: Windows only; Linux and macOS window managers animate
 * fullscreen their own way (to check when those ports start; likely nothing to do). */
extern "C" void pc_window_transitions(int on) {
#ifdef _WIN32
    SDL_Window* window = game_window();
    if (window == nullptr) {
        return;
    }
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    static HMODULE dwm = LoadLibraryA("dwmapi.dll");
    typedef HRESULT(WINAPI * SetAttr)(HWND, DWORD, LPCVOID, DWORD);
    static SetAttr set = dwm != nullptr ? reinterpret_cast<SetAttr>(
                                              reinterpret_cast<void*>(GetProcAddress(dwm, "DwmSetWindowAttribute")))
                                        : nullptr;
    if (hwnd == nullptr || set == nullptr) {
        return;
    }
    const BOOL disabled = on ? FALSE : TRUE;
    set(hwnd, 3 /* DWMWA_TRANSITIONS_FORCEDISABLED */, &disabled, sizeof(disabled));
#else
    (void)on;
#endif
}

namespace {
/* What the title bar and taskbar show (snatcherLeo, 2026-11-11): the taskbar and desktop are always the O in the chosen colour
 * ("O is the only thing users will have in the taskbar/desktop icon"); the title bar's own small icon follows the
 * theme ("Yes, light o": the O given the light wordmark's treatment, ui/icons/orreport-icon-<colour>-light-o.svg) or,
 * with Appearance > Window icon on "Espeon and Umbreon", shows the save icon instead, in the hub and in game ("in
 * real-time, the top left corner should change"). *Updated 2026-11-12 (snatcherLeo: "allow the taskbar icon to switch,
 * depending on light mode or dark mode"):* the taskbar's O follows the theme too (the light O on a light rounded
 * square); the desktop shortcut's icon is the exe's own and does not change. */
std::string s_colour = "rust";
int s_dark = 1;
bool s_game_icon = false;

/* The save icon's frame 4 (snatcherLeo, 2026-11-11: "The onwe with Umbreon's thing fully lit"; frames 4 and 6 are the same
 * picture), read from the player's own main.dol, never shipped: 8 frames of 32 x 32 in 8-bit palette format (each in
 * 8 x 4 tiles), then one 256-colour palette in RGB5A3, at 0x802770E8 (the same bytes memcard.c writes into saves;
 * docs/engine/save-data.md). True while the disc's bytes are there and match. */
bool save_icon_rgba(uint8_t out[32 * 32 * 4]) {
#ifdef PC_HAVE_AURORA_DVD
    static bool loaded = false;
    static uint8_t rgba[32 * 32 * 4];
    if (!loaded) { /* a miss is not remembered: the disc may be added later */
        static uint8_t raw[0x2200];
        if (pc_disc_dol_try_read(raw, 0x802770E8u, sizeof(raw), 0x6B3B8AB4u) == 0) {
            return false;
        }
        const int frame = 3; /* the fourth frame */
        int i = 0;
        for (int ty = 0; ty < 32; ty += 4) {
            for (int tx = 0; tx < 32; tx += 8) {
                for (int y = 0; y < 4; ++y) {
                    for (int x = 0; x < 8; ++x, ++i) {
                        const uint8_t index = raw[frame * 0x400 + i];
                        const unsigned v = (unsigned)raw[0x2000 + index * 2] << 8 | raw[0x2001 + index * 2];
                        uint8_t* o = rgba + ((ty + y) * 32 + tx + x) * 4;
                        if (v & 0x8000) { /* RGB555, opaque */
                            o[0] = (uint8_t)(((v >> 10) & 31) * 255 / 31);
                            o[1] = (uint8_t)(((v >> 5) & 31) * 255 / 31);
                            o[2] = (uint8_t)((v & 31) * 255 / 31);
                            o[3] = 255;
                        } else { /* RGB444 with 3 bits of alpha */
                            o[0] = (uint8_t)(((v >> 8) & 15) * 17);
                            o[1] = (uint8_t)(((v >> 4) & 15) * 17);
                            o[2] = (uint8_t)((v & 15) * 17);
                            o[3] = (uint8_t)(((v >> 12) & 7) * 255 / 7);
                        }
                    }
                }
            }
        }
        loaded = true;
        std::printf("[window_icon] the save icon's Espeon and Umbreon read from the disc\n");
    }
    std::memcpy(out, rgba, sizeof(rgba));
    return true;
#else
    (void)out;
    return false;
#endif
}

/* the save icon at size x size: pixel art, so each output pixel averages the source pixels it covers when smaller
 * (4 x 4 samples), and repeats them unsmoothed when 32 px or more.
 * *Updated 2026-11-13 (snatcherLeo, comparing the title bar's O with Espeon and Umbreon, normal and maximized: "the Espeon and
 * Umbreon (and the future Eevee logo from XD Gale of Darkness) box thing is sized in a way that is... just like the O
 * but without messing with the aspect ratio"):* drawn inside box (left, top, width, height in pixels of the size x size
 * picture): the same box the O fills (o_box), kept square and centred in it, the rest transparent. Before, it filled
 * the whole icon square, so it looked bigger than the O and touched the corner of the title bar. */
SDL_Surface* draw_save_icon(const uint8_t* rgba, int size, float box_x, float box_y, float box_w, float box_h) {
    SDL_Surface* surf = SDL_CreateSurface(size, size, SDL_PIXELFORMAT_RGBA32);
    if (surf == nullptr) {
        return nullptr;
    }
    SDL_memset(surf->pixels, 0, (size_t)surf->pitch * size);
    const float side = SDL_min(box_w, box_h); /* square: the picture's own shape */
    const float ox = box_x + (box_w - side) * 0.5f, oy = box_y + (box_h - side) * 0.5f;
    constexpr int SS = 4;
    for (int py = 0; py < size; ++py) {
        uint8_t* row = static_cast<uint8_t*>(surf->pixels) + py * surf->pitch;
        for (int px = 0; px < size; ++px) {
            float r = 0, g = 0, b = 0, a = 0;
            const int n = side >= 32 ? 1 : SS;
            for (int sy = 0; sy < n; ++sy) {
                for (int sx = 0; sx < n; ++sx) {
                    const float fx = (n == 1 ? px + 0.5f : px + (sx + 0.5f) / n) - ox;
                    const float fy = (n == 1 ? py + 0.5f : py + (sy + 0.5f) / n) - oy;
                    if (fx < 0 || fy < 0 || fx >= side || fy >= side) {
                        continue; /* outside the box: transparent */
                    }
                    const uint8_t* p = rgba + ((int)(fy * 32 / side) * 32 + (int)(fx * 32 / side)) * 4;
                    const float pa = p[3] / 255.0f;
                    r += p[0] * pa;
                    g += p[1] * pa;
                    b += p[2] * pa;
                    a += pa;
                }
            }
            uint8_t* o = row + px * 4;
            o[0] = (uint8_t)std::lround(a > 0 ? r / a : 0);
            o[1] = (uint8_t)std::lround(a > 0 ? g / a : 0);
            o[2] = (uint8_t)std::lround(a > 0 ? b / a : 0);
            o[3] = (uint8_t)std::lround(a * 255 / (n * n));
        }
    }
    return surf;
}

void apply_icons();

void note_theme(int dark) {
    if (s_dark != (dark != 0 ? 1 : 0)) {
        s_dark = dark != 0 ? 1 : 0;
        apply_icons();
    }
}
} // namespace

/* colour: "sage", "rust" or "white" */
extern "C" void pc_window_icon_set(const char* colour) {
    s_colour = colour;
    apply_icons();
}

/* Appearance > Window icon: 1 "Espeon and Umbreon", 0 "OrrePort logo"; applied at once (also when unchanged: the
 * ROM may have been opened since the last call, so the icon can now be read) */
extern "C" void pc_window_icon_game(int on) {
    s_game_icon = on != 0;
    apply_icons();
}

/* 1 when the player's disc has the save icon (the Settings row is offered only then; snatcherLeo, 2026-11-11: "only shows up
 * in apperance (in settings) AFTER the colosseum ROM has successfully been imported") */
extern "C" int pc_window_icon_game_available(void) {
    uint8_t rgba[32 * 32 * 4];
    return save_icon_rgba(rgba) ? 1 : 0;
}

namespace {
/* The pictures are drawn once and kept (2026-11-11, snatcherLeo: "There is quite some lag when switching Window icon", "a
 * noticeable flicker from the light O to the dark o"): drawing the whole set (the 256 px icon and eleven smaller sizes,
 * 16 samples a pixel) took long enough to see, and every switch redrew all of it, so the title bar changed colour
 * before its O caught up. Now the taskbar's set is drawn once per colour, and each title-bar picture (plain O, light
 * O, Espeon and Umbreon) once per size; a switch only hands Windows a picture it already has. */
struct Shapes {
    std::string colour;
    std::vector<Shape> full, small, light;
};

const Shapes* shapes_for(const std::string& colour) {
    static Shapes s_cache;
    if (s_cache.colour == colour) {
        return &s_cache;
    }
    const std::string dir = pc_paths_str(PC_PLACE_UI, "icons/"); /* app\UI\icons\ (flat layout: ui\icons\), 4.1 */
    std::string full_svg, light_svg;
    if (!load_text(dir + "orreport-icon-" + colour + ".svg", full_svg)) {
        std::printf("[window_icon] no icon files for '%s' in %s\n", colour.c_str(), dir.c_str());
        return nullptr;
    }
    s_cache = Shapes{colour, parse(full_svg), {}, {}};
    /* the title bar shows the full icon's O, without its backing (snatcherLeo, 2026-11-09: "we also don't need a box to put
     * that "O" in", then, showing the full icon's O with its rim, face, notches and cracks: "Make the O be this, but
     * without a background"): the full icon's shapes minus its rounded square */
    for (const Shape& s : s_cache.full) {
        if (!s.rect) {
            s_cache.small.push_back(s);
        }
    }
    s_cache.light = load_text(dir + "orreport-icon-" + colour + "-light-o.svg", light_svg) ? parse(light_svg)
                                                                                           : s_cache.small;
    return &s_cache;
}

/* the box the title bar's O fills in a size x size picture (its shapes' bounding box, from the SVG's 512-unit canvas),
 * so a game's icon (draw_save_icon) sits where the O does and at its size; the whole square if the shapes are empty */
void o_box(const std::vector<Shape>& small, int size, float& x, float& y, float& w, float& h) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const Shape& s : small) {
        for (const auto& ring : s.rings) {
            for (const auto& [px, py] : ring) {
                x0 = SDL_min(x0, px);
                y0 = SDL_min(y0, py);
                x1 = SDL_max(x1, px);
                y1 = SDL_max(y1, py);
            }
        }
    }
    const float k = (float)size / 512.0f;
    if (x1 <= x0 || y1 <= y0) {
        x = y = 0;
        w = h = (float)size;
        return;
    }
    x = x0 * k;
    y = y0 * k;
    w = (x1 - x0) * k;
    h = (y1 - y0) * k;
}

void apply_icons() {
    SDL_Window* window = game_window();
    if (window == nullptr) {
        return;
    }
    const Shapes* shapes = shapes_for(s_colour);
    if (shapes == nullptr) {
        return;
    }
    const char* colour = s_colour.c_str();
    const std::vector<Shape>& full = shapes->full;
    const std::vector<Shape>& small = shapes->small;
    /* the title bar's own icon (below): the light O on a light title bar, or the save icon when chosen */
    uint8_t game_rgba[32 * 32 * 4];
    const bool game = s_game_icon && save_icon_rgba(game_rgba);
    /* Windows picks each place's size from these: the title bar's small icon is 16 px times the display scaling
     * (36 px at 225%), the taskbar's 24-32 px times it. So the plain O up to 40 px (scalings to 250%) and the full
     * icon, on its rounded backing (snatcherLeo: "the app icon in taskbar having circle makes sense"), from 48 px; before,
     * the set jumped from 32 to 48 and a 225% title bar took the full icon, box and all. Drawn and handed to SDL
     * once per colour: it never changes with the theme or the window-icon choice. */
    static std::string s_sdl_colour;
    bool ok = true;
    if (s_sdl_colour != s_colour) {
        /* the thirteen sizes drawn side by side, one thread each, then handed to SDL here on the main thread
         * (pc_parallel.h; 2026-11-13: drawn one after another they were about 0.2 s of every start, measured with the
         * [startup] marks). draw() only reads the shapes and fills its own new surface. */
        static const int kSizes[] = {256, 128, 96, 72, 64, 48, 40, 36, 32, 28, 24, 20, 16};
        constexpr int kCount = (int)(sizeof(kSizes) / sizeof(kSizes[0]));
        SDL_Surface* drawn[kCount] = {};
        std::vector<std::function<void()>> jobs;
        for (int i = 0; i < kCount; ++i) {
            jobs.push_back([&, i] { drawn[i] = draw(kSizes[i] >= 48 ? full : small, kSizes[i]); });
        }
        pc_run_together(jobs);
        SDL_Surface* icon = drawn[0];
        if (icon == nullptr) {
            for (SDL_Surface* s : drawn) SDL_DestroySurface(s);
            return;
        }
        for (int i = 1; i < kCount; ++i) {
            if (drawn[i] != nullptr) {
                SDL_AddSurfaceAlternateImage(icon, drawn[i]);
                SDL_DestroySurface(drawn[i]);
            }
        }
        ok = SDL_SetWindowIcon(window, icon);
        SDL_DestroySurface(icon);
        s_sdl_colour = s_colour;
    }
#ifdef _WIN32
    /* Windows: SDL hands Windows one picture and Windows shrinks it for the title bar, so the title bar showed the full
     * icon, backing and all (seen in screenshots, 2026-11-09). So Windows' two icons are set directly, each drawn at the
     * exact size Windows wants at this display scaling: the small one (title bar) as the plain O, the big one
     * (taskbar, Alt+Tab) as the full icon. */
    HWND hwnd = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    /* Windows 10's DPI-aware calls, loaded at run time (MinGW declares them only for newer target versions, and
     * older Windows lacks them: then the plain system sizes) */
    typedef UINT(WINAPI * GetDpiForWindowFn)(HWND);
    typedef int(WINAPI * GetSystemMetricsForDpiFn)(int, UINT);
    HMODULE user32 = GetModuleHandleA("user32.dll");
    auto dpi_for_window = user32 != nullptr ? reinterpret_cast<GetDpiForWindowFn>(reinterpret_cast<void*>(
                                                  GetProcAddress(user32, "GetDpiForWindow")))
                                            : nullptr;
    auto metrics_for_dpi = user32 != nullptr ? reinterpret_cast<GetSystemMetricsForDpiFn>(reinterpret_cast<void*>(
                                                   GetProcAddress(user32, "GetSystemMetricsForDpi")))
                                             : nullptr;
    auto GetSystemMetricsForDpi = [&](int metric, UINT dpi) {
        return metrics_for_dpi != nullptr ? metrics_for_dpi(metric, dpi) : GetSystemMetrics(metric);
    };
    if (hwnd != nullptr) {
        const UINT dpi = dpi_for_window != nullptr ? dpi_for_window(hwnd) : 96;
        auto make_icon = [](SDL_Surface* s, int size) -> HICON { /* takes the picture and frees it */
            if (s == nullptr) {
                return nullptr;
            }
            BITMAPV5HEADER bi{};
            bi.bV5Size = sizeof(bi);
            bi.bV5Width = size;
            bi.bV5Height = -size; /* top-down */
            bi.bV5Planes = 1;
            bi.bV5BitCount = 32;
            bi.bV5Compression = BI_BITFIELDS;
            bi.bV5RedMask = 0x00FF0000;
            bi.bV5GreenMask = 0x0000FF00;
            bi.bV5BlueMask = 0x000000FF;
            bi.bV5AlphaMask = 0xFF000000;
            void* bits = nullptr;
            HDC dc = GetDC(nullptr);
            HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
            ReleaseDC(nullptr, dc);
            HBITMAP mask = CreateBitmap(size, size, 1, 1, nullptr);
            HICON h = nullptr;
            if (color != nullptr && mask != nullptr && bits != nullptr) {
                auto* out = static_cast<uint8_t*>(bits);
                for (int y = 0; y < size; ++y) {
                    const uint8_t* in = static_cast<const uint8_t*>(s->pixels) + y * s->pitch;
                    for (int x = 0; x < size; ++x) {
                        const uint8_t* p = in + x * 4; /* RGBA */
                        uint8_t* o = out + (y * size + x) * 4; /* BGRA */
                        o[0] = p[2];
                        o[1] = p[1];
                        o[2] = p[0];
                        o[3] = p[3];
                    }
                }
                ICONINFO ii{};
                ii.fIcon = TRUE;
                ii.hbmColor = color;
                ii.hbmMask = mask;
                h = CreateIconIndirect(&ii);
            }
            if (color != nullptr) DeleteObject(color);
            if (mask != nullptr) DeleteObject(mask);
            SDL_DestroySurface(s);
            return h;
        };
        /* kept icons: three title-bar pictures and the taskbar's, each per colour and size; the window uses whichever
         * it was given last, so none is freed (a few KB; the colour and the display scaling rarely change) */
        struct Kept {
            std::string colour;
            int kind, px; /* kind: 0 plain O, 1 light O, 2 Espeon and Umbreon, 3 the taskbar's full icon, 4 its
                           * light-mode version */
            HICON icon;
        };
        static std::vector<Kept> s_kept;
        static HICON s_small_now = nullptr, s_big_now = nullptr;
        auto kept = [&](int kind, int px) -> HICON {
            for (const Kept& k : s_kept) {
                if (k.colour == s_colour && k.kind == kind && k.px == px) {
                    return k.icon;
                }
            }
            /* kind 4, the taskbar's icon in light mode (snatcherLeo, 2026-11-12: "allow the taskbar icon to switch, depending
             * on light mode or dark mode"): the full icon's rounded square in the light theme's colour (#E9EDEA) with
             * the light O on it */
            std::vector<Shape> light_full;
            if (kind == 4) {
                for (const Shape& sh : full) {
                    if (sh.rect) {
                        Shape back = sh;
                        back.r = 0xE9;
                        back.g = 0xED;
                        back.b = 0xEA;
                        light_full.push_back(back);
                    }
                }
                light_full.insert(light_full.end(), shapes->light.begin(), shapes->light.end());
            }
            float bx = 0, by = 0, bw = (float)px, bh = (float)px;
            o_box(small, px, bx, by, bw, bh); /* a game's icon in the O's box (draw_save_icon says why) */
            SDL_Surface* s = kind == 2   ? draw_save_icon(game_rgba, px, bx, by, bw, bh)
                             : kind == 3 ? draw(full, px)
                             : kind == 4 ? draw(light_full, px)
                             : kind == 1 ? draw(shapes->light, px)
                                         : draw(small, px);
            HICON h = make_icon(s, px);
            if (h != nullptr) {
                s_kept.push_back(Kept{s_colour, kind, px, h});
            }
            return h;
        };
        const int small_px = GetSystemMetricsForDpi(SM_CXSMICON, dpi), big_px = GetSystemMetricsForDpi(SM_CXICON, dpi);
        const int kind = game ? 2 : s_dark == 0 ? 1 : 0;
        HICON small_icon = kept(kind, small_px);
        HICON big_icon = kept(s_dark == 0 ? 4 : 3, big_px);
        /* the other title-bar pictures ready too, so the first switch is as quick as the rest */
        kept(s_dark == 0 ? 0 : 1, small_px);
        if (small_icon != nullptr && small_icon != s_small_now) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
            s_small_now = small_icon;
            std::printf("[window_icon] title bar: %s (%d px)\n",
                        kind == 2 ? "Espeon and Umbreon" : kind == 1 ? "light O" : "plain O", small_px);
        }
        if (big_icon != nullptr && big_icon != s_big_now) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
            s_big_now = big_icon;
            std::printf("[window_icon] taskbar: the full icon%s (%d px, at %u dpi)\n", s_dark == 0 ? ", light" : "",
                        big_px, dpi);
        }
    }
#else
    (void)game; /* other systems: one icon for everything, always the O */
#endif
    if (!ok) {
        std::printf("[window_icon] %s O: %s\n", colour, SDL_GetError());
    }
}
} // namespace
