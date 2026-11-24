/*
 * pc_hub.cpp: OrrePort's hub, the first screen (the hub plan's step 2; plan "The hub", the project's mockup 01): pick a disc,
 * pick a save, press Play. 2026-11-09.
 *
 * Two moments use the same page, in the game's own window:
 *  - No disc yet (pc_hub_wait_for_disc, from pc_platform_start when ROM\ has no USA disc): a blank disc (snatcherLeo: "It'll
 *    be a blank disc, until the ISO is recognized, and then, it'll show that design i liked") and the three ways to
 *    add one (plan "Adding a disc"): drop the image on the window, Browse..., or put it in the ROM folder (watched).
 *    A dropped or chosen file is checked first, then copied into ROM\ (the plan's default, "Copy into OrrePort"),
 *    then found like any disc there. The page stays up while the platform runs its disc checks.
 *  - Ready (pc_hub_run, from pc_game_main.c after the disc checks, before the game records or loads anything): the
 *    disc carries OrrePort's own label (the design snatcherLeo signed off, docs/design/brand/
 *    orreport-disc-label-colosseum.svg), swaying (sway C); Save 1 with when it was last saved; PLAY with the Lift and
 *    spin into black, then the game starts in the same window; Report a problem.
 * The blank disc is the same label drawn without its printing (the lettering, the marks and the code line), so
 * "recognised" is the printing fading in. Saves and backups, Import Dolphin save, XD, Collection and Create come later.
 *
 * Automated runs (pad scripts, replays) and ORRE_SKIP_HUB skip it, so replays are untouched. Test hooks:
 * ORRE_HUB_AUTOPLAY=<ms> presses Play by itself; ORRE_HUB_TEST_ADD=<image path> adds that image as if dropped;
 * ORRE_HUB_TEST_LABEL=<picture> opens the disc label page and adds that picture.
 *
 * Platform layer only. Built only with AURORA_ENABLE_RMLUI.
 */
#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/input.hpp>
#include <aurora/rmlui.hpp>

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <dolphin/dvd.h>

#include "pc_disc_label.h"
#include "pc_parallel.h" /* the start-up pictures drawn side by side (setup) */
#include "pc_health.h"   /* the hub's frames in the performance log */
#include "pc_text.h"     /* every word in the player's language (ui/lang/<code>.txt) */
#include "pc_hub_saves.h" /* the Saves page (step 2.1) */
#include "pc_hub_langdiscs.h" /* the Language discs link and panel (2.2 step 8, piece 2) */
#include "pc_saves.h"     /* the save slots: the card shows the one Play starts */
#include "pc_autosave.h"  /* the card names the autosave when PLAY will start it (2026-11-21) */
#include "pc_options.h"   /* GameCube-accurate mode: an .orresave is offered a GameCube copy instead of playing */
#include "pc_pad_shape.h" /* the menu shortcut from Controls: it opens Settings in the hub too, as in game */
#include "pc_zip.h"       /* pc_file_touch_utf8: a moved disc counts as the newest (4.4) */
#include "pc_update.h"    /* the shared updater: the note beside the gear and its panel (4.4) */
#include "pc_log.h"       /* pc_log_drain before Restart now's exit */
#include "pc_settings.h"  /* pc_settings_save before Restart now's exit */
#include "pc_paths.h"     /* the UI folder, the saves, the test files' place (4.1) */
#include "pc_apps.h"      /* Open Studio, Close Studio, this window for Studio to find (4.4, 2026-11-23) */

#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
int pc_hub_run(void);
int pc_hub_wait_for_disc(void);
int pc_ui_load_fonts(void);                                        /* pc_settings_overlay.cpp */
void pc_settings_overlay_open_in_hub(void);                        /* pc_settings_overlay.cpp: the gear */
void pc_settings_overlay_open_in_hub_disc_art(void);               /* ... on Appearance, from the disc art page */
void pc_settings_overlay_test_whatoff(void);                       /* ... its "What it turns off" list (test hook) */
int pc_settings_overlay_is_open(void);
void pc_settings_overlay_hub_frame(void);
int pc_ui_theme_is_dark(void);                                     /* Appearance > Theme, live */
int pc_ui_animations_on(void);                                     /* Appearance > Animations, live */
int pc_controls_ui_is_open(void);                                  /* pc_controls_ui.cpp: the controller setup */
void pc_window_icon_game(int on);                                  /* pc_window_icon.cpp: Appearance > Window icon */
int pc_settings_int(const char* key, int fallback);                /* pc_settings.c */
int pc_platform_automated_run(void);                               /* pc_platform.c */
void pc_platform_window_event(const AuroraEvent* event);           /* pc_platform.c: fullscreen keys, window place */
void pc_platform_window_frame(void);
const char* pc_platform_data_path(const char* name, char* out, size_t size);
const char* pc_platform_rom_folder(char* out, size_t size);        /* pc_platform.c: <exe>/Colosseum/ROM/ */
int pc_platform_disc_identify(const char* path, char* why, size_t size);
int pc_platform_disc_scan(char* message, size_t message_size);
int pc_platform_disc_scan_saw_other(void); /* pc_platform.c: the scan found another game's or version's image */
void pc_vi_close_first_frame(void); /* pc_vi.c: the platform's first frame, closed for the hub's own frames */
void pc_vi_begin(void);             /* ... and opened again for the game */
int pc_report_write(const char* note, const char* screenshot, char* out_zip, size_t out_size); /* pc_report.cpp */
void pc_report_show_in_folder(const char* zip);
int pc_clipboard_set_file(const char* utf8_path); /* pc_clip_file.c: the saved report, for Ctrl+V */
void pc_window_cloak(int on); /* pc_window_icon.cpp: the window stays invisible until the hub has drawn itself */
void pc_platform_handover_ready(void); /* pc_platform.c: Quit to hub's ready signal to the previous OrrePort */
void pc_startup_mark(const char* what); /* pc_log.c: "[startup] <ms> ms: <what>" in the log */
extern volatile unsigned int pc_vi_alive; /* pc_vi.c: the crash watchdog's heartbeat */
}

bool pc_svg_render(const std::string& svg, int width, int& height, std::vector<unsigned char>& rgba,
                   const std::vector<std::string>& skip_fills); /* pc_svg.cpp */

namespace {
namespace input = aurora::input;

struct Picture {
    int w = 0, h = 0;
    std::vector<unsigned char> px; /* RGBA8 */
};

Rml::ElementDocument* s_doc = nullptr;
input::LayerId s_layer{};
bool s_no_disc = false; /* the "no disc yet" moment */
bool s_disc_found = false;
bool s_play = false;
bool s_quit = false;
bool s_labels = false; /* the disc label page is open (body.labels; "the disc label page" below) */
int s_ltop = 0;        /* the controller on its top row: 1 Back to Hub, 2 the gear (ltop_event below) */
void ltop_set(int v);
Uint64 s_play_at = 0;
Picture s_label, s_blank, s_smear, s_wordmark, s_wordmark_light;
Picture s_empty; /* the empty disc (no disc added): the blank disc with a grey top (2026-11-21) */
/* the current save card's symbols (a .gci's memory card, an .orresave's file with the O) and Change save's folder:
 * white, tinted by hub.rcss (image-color), so one picture serves both themes and any colour */
Picture s_icon_gci, s_icon_orre, s_icon_folder;
Picture s_icon_ring; /* the disc art shelf's outline, smooth (hub.rcss .edge) */
Picture s_icon_round, s_icon_ring_small; /* the disc's pencil button: its face and outline, smooth (hub.rcss #b-label) */
Picture s_icon_lang_eu, s_icon_lang_jp, s_icon_lang_us; /* + the US disc's (the Discs panel, 2026-11-23) */   /* the Language discs panel's European and Japanese disc symbols */
Picture s_icon_globe;                     /* the Language discs link's small globe */
std::vector<std::byte> s_banner; /* 96 x 32 RGBA8: the player's banner, for the later "Disc label: Banner" choice */

/* adding a disc */
std::mutex s_pick_mutex;
std::string s_picked; /* from the Browse dialog (its callback may run on another thread) */
std::atomic<int> s_copy_state{0}; /* 0 idle, 1 copying, 2 copied, 3 failed */
std::atomic<Uint64> s_copy_done{0}, s_copy_total{0};
std::string s_copy_src, s_copy_dst, s_copy_error;
bool s_copy_move = false;   /* Move into OrrePort: rename when on the same drive, else copy and remove the original */
std::string s_offer_path;   /* a checked disc waiting for Copy / Move / Cancel (body.offer) */
Uint64 s_offer_size = 0;

/* the disc turned by hand (#discdrag): degrees, degrees per second */
bool s_turn_drag = false;
float s_turn_angle = 0.0f, s_turn_speed = 0.0f, s_turn_grab = 0.0f, s_turn_shown = 0.0f;
Uint64 s_turn_last_move = 0;
std::string s_watch_last, s_watch_scanned; /* ROM\'s contents at the last poll and at the last scan */

std::string ui_path(const char* file) { return pc_paths_str(PC_PLACE_UI, file); /* app\UI\ (flat: ui\), 4.1 */ }

std::string rom_folder() {
    char folder[1300];
    return pc_platform_rom_folder(folder, sizeof(folder));
}

bool load_svg(const char* file, int width, Picture& out, const std::vector<std::string>& skip = {}) {
    size_t len = 0;
    void* data = SDL_LoadFile(ui_path(file).c_str(), &len);
    if (data == nullptr) {
        return false;
    }
    const std::string svg(static_cast<const char*>(data), len);
    SDL_free(data);
    return pc_svg_render(svg, width, out.h, out.px, skip) && (out.w = width) > 0;
}

/* an SVG drawn with one fill colour swapped for another everywhere it is used (the shapes keep their place in the
 * drawing order, so whatever the SVG draws over them stays over them), then the skip list as load_svg: the empty
 * disc's grey top (2026-11-21) */
bool load_svg_recoloured(const char* file, const char* colour, const char* to, int width, Picture& out,
                         const std::vector<std::string>& skip) {
    size_t len = 0;
    void* data = SDL_LoadFile(ui_path(file).c_str(), &len);
    if (data == nullptr) {
        return false;
    }
    std::string svg(static_cast<const char*>(data), len);
    SDL_free(data);
    const std::string want = std::string("fill=\"") + colour + "\"", with = std::string("fill=\"") + to + "\"";
    for (size_t at = svg.find(want); at != std::string::npos; at = svg.find(want, at + with.size())) {
        svg.replace(at, want.size(), with);
    }
    return pc_svg_render(svg, width, out.h, out.px, skip) && (out.w = width) > 0;
}

/* the GameCube's own centre hole: 15 mm on the 80 mm disc = 39.5 of the label's 420 units in radius (gamecube_hole) */
constexpr float kHoleRadius = 39.5f;
constexpr float kRimWidth = 4.0f;     /* the smear disc's light rim round the hole */
constexpr float kRingRadius = 49.0f;  /* the raised (stacking) ring round the hole, style B */
constexpr float kClearRadius = 79.0f; /* the clear band's outer edge */

/* The smeared disc of the Play spin (mockup 19's "smear" layer, which style 04 fades in at the end): what a spinning
 * label looks like, its printing dissolved into soft rings. In the mockup it is rings and one radial gradient over the
 * 420-unit disc; drawn here directly from those values, 4 x 4 samples per pixel. */
void make_smear(int size, Picture& out) {
    struct Ring {
        float r_in, r_out;
        unsigned colour; /* 0: the gradient */
        float opacity;
    };
    /* the mockup's rings, with the GameCube-sized hole (kHoleRadius) in place of its 15.5-unit one; its stacking ring
     * (radius 35.5) falls inside that hole, so it is left out */
    static const Ring rings[] = {
        {204.0f, 210.0f, 0xD7D5DF, 1.0f},                                     /* the rim, its rainbow blurred */
        {kHoleRadius, 204.0f, 0, 1.0f},                                       /* the label, smeared into bands */
        {kHoleRadius, 79.0f, 0xD6E2E8, 0.30f},                                /* the clear ring */
        {78.0f, 79.0f, 0xFFFFFF, 0.45f},
        {kHoleRadius, kHoleRadius + kRimWidth, 0xE8F0F4, 0.30f},              /* the raised rim round the hole */
    };
    static const struct {
        float at;
        unsigned colour;
    } stops[] = {{0.0f, 0x151817},  {0.44f, 0x151817}, {0.5f, 0x1C1E1D}, {0.55f, 0x2A2A28}, {0.6f, 0x30292A},
                 {0.65f, 0x2E2423}, {0.69f, 0x232122}, {0.74f, 0x181A19}, {1.0f, 0x151817}};
    auto channel = [](unsigned c, int shift) { return ((c >> shift) & 255) / 255.0f; };
    auto gradient = [&](float d, float rgb[3]) {
        const float f = SDL_min(1.0f, d / 204.0f);
        int i = 0;
        while (i + 2 < (int)SDL_arraysize(stops) && f > stops[i + 1].at) {
            ++i;
        }
        const float k = SDL_clamp((f - stops[i].at) / (stops[i + 1].at - stops[i].at), 0.0f, 1.0f);
        for (int c = 0; c < 3; ++c) {
            const int shift = 16 - 8 * c;
            rgb[c] = channel(stops[i].colour, shift) * (1 - k) + channel(stops[i + 1].colour, shift) * k;
        }
    };
    out.w = out.h = size;
    out.px.assign((size_t)size * size * 4, 0);
    const float unit = 420.0f / size;
    auto smear_row = [&](int py) { /* one row of the picture; writes only that row */
        for (int px = 0; px < size; ++px) {
            const float cx = (px + 0.5f) * unit - 210.0f, cy = (py + 0.5f) * unit - 210.0f;
            const float d_centre = SDL_sqrtf(cx * cx + cy * cy);
            float r = 0, g = 0, b = 0, a = 0; /* premultiplied */
            for (const Ring& ring : rings) {
                int hits = 0;
                for (int sy = 0; sy < 4; ++sy) {
                    for (int sx = 0; sx < 4; ++sx) {
                        const float x = (px + (sx + 0.5f) / 4) * unit - 210.0f, y = (py + (sy + 0.5f) / 4) * unit - 210.0f;
                        const float d = SDL_sqrtf(x * x + y * y);
                        hits += d >= ring.r_in && d <= ring.r_out ? 1 : 0;
                    }
                }
                const float cov = hits / 16.0f * ring.opacity;
                if (cov <= 0) {
                    continue;
                }
                float rgb[3];
                if (ring.colour == 0) {
                    gradient(d_centre, rgb);
                } else {
                    rgb[0] = channel(ring.colour, 16);
                    rgb[1] = channel(ring.colour, 8);
                    rgb[2] = channel(ring.colour, 0);
                }
                r = rgb[0] * cov + r * (1 - cov);
                g = rgb[1] * cov + g * (1 - cov);
                b = rgb[2] * cov + b * (1 - cov);
                a = cov + a * (1 - cov);
            }
            unsigned char* o = &out.px[((size_t)py * size + px) * 4];
            o[0] = (unsigned char)SDL_lroundf(a > 0 ? r / a * 255 : 0);
            o[1] = (unsigned char)SDL_lroundf(a > 0 ? g / a * 255 : 0);
            o[2] = (unsigned char)SDL_lroundf(a > 0 ? b / a * 255 : 0);
            o[3] = (unsigned char)SDL_lroundf(a * 255);
        }
    };
    /* in eight bands of rows side by side (pc_parallel.h): with 16 samples per pixel against five rings it was the
     * slowest of the hub's start-up pictures (2026-11-13). Each band writes only its own rows. */
    std::vector<std::function<void()>> bands;
    for (int band = 0; band < 8; ++band) {
        bands.push_back([&, band] {
            for (int py = size * band / 8; py < size * (band + 1) / 8; ++py) {
                smear_row(py);
            }
        });
    }
    pc_run_together(bands);
}

/* A transparent margin of pad pixels round a picture (2026-11-11). The disc's rim reaches the edges of its
 * picture, so the image box's own edge cut through the rim's smoothing: a hard line that flickered at the top of the
 * disc as it swayed by fractions of a pixel (snatcherLeo: "the Disc has a weird "shimmering" effect at the top"). With a
 * margin the rim's edge is drawn smooth inside the picture; hub.rcss makes the disc's image boxes larger by the same
 * ratio (17/16 for pad = size / 32), so the disc shows at the same size and place. */
void add_margin(Picture& p, int pad) {
    if (p.px.empty() || pad <= 0) {
        return;
    }
    const int w = p.w + 2 * pad, h = p.h + 2 * pad;
    std::vector<unsigned char> px((size_t)w * h * 4, 0);
    for (int y = 0; y < p.h; ++y) {
        SDL_memcpy(&px[(((size_t)y + pad) * w + pad) * 4], &p.px[(size_t)y * p.w * 4], (size_t)p.w * 4);
    }
    p.w = w;
    p.h = h;
    p.px.swap(px);
}

/* The GameCube's own centre hole (snatcherLeo, 2026-11-09, after a side-by-side: "GameCube-sized"): 15 mm on the 80 mm disc,
 * 39.5 of the label's 420 units in radius, where the signed-off label has a 5.9 mm hole. Punched into the drawn label
 * (and the blank disc) with a thin #2C322F edge, as in the preview; the small stacking ring falls inside it.
 * *Updated the same evening:* the plastic round it a touch lighter, with a soft light rim instead of the dark edge.
 * kHoleRadius, kRimWidth and kClearRadius are defined above make_smear. */
void gamecube_hole(Picture& p) {
    if (p.px.empty()) {
        return;
    }
    const float unit = 420.0f / p.w; /* label units per pixel */
    const float c = 210.0f;
    const int lo = (int)((c - kClearRadius - 2) / unit), hi = (int)((c + kClearRadius + 2) / unit);
    for (int py = SDL_max(0, lo); py <= SDL_min(p.h - 1, hi); ++py) {
        for (int px = SDL_max(0, lo); px <= SDL_min(p.w - 1, hi); ++px) {
            /* the clear plastic, just slightly (snatcherLeo, 2026-11-09, after photos of real discs: "Shoujld we change how
             * the ring looks?", "just slightly"): the band a touch lighter, and a soft light raised rim hugging the
             * hole (the stacking ring) instead of a dark edge */
            /* snatcherLeo then chose style B of the side-by-side ("Either B or C. Let's try B"), whose values these are:
             * the band under a cool white (#DEE8EE) at 30%; a raised ring from the hole out to 49 units, white at 85%
             * near the hole fading to 55%; a thin light edge (65%) round the hole */
            int hole = 0;
            float band = 0.0f, ring = 0.0f, edge = 0.0f;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const float x = (px + (sx + 0.5f) / 4) * unit - c, y = (py + (sy + 0.5f) / 4) * unit - c;
                    const float d = SDL_sqrtf(x * x + y * y);
                    if (d < kHoleRadius) {
                        ++hole;
                        continue;
                    }
                    if (d < kClearRadius) {
                        band += 0.30f;
                    }
                    if (d < kRingRadius) { /* B's gradient: 85% at 41.2, 75% at 45.6, 55% at its edge */
                        ring += d < 41.2f ? 0.85f : d < 45.6f ? 0.85f - 0.10f * (d - 41.2f) / 4.4f
                                                               : 0.75f - 0.20f * (d - 45.6f) / (kRingRadius - 45.6f);
                    }
                    if (d < kHoleRadius + 1.0f) {
                        edge += 0.65f;
                    }
                }
            }
            if (hole == 0 && band == 0.0f) {
                continue;
            }
            unsigned char* o = &p.px[((size_t)py * p.w + px) * 4];
            float a = o[3] / 255.0f * (1.0f - hole / 16.0f); /* the hole: see-through */
            float r = o[0] / 255.0f * a, g = o[1] / 255.0f * a, b = o[2] / 255.0f * a; /* premultiplied */
            auto over = [&](unsigned colour, float k) {
                r = ((colour >> 16) & 255) / 255.0f * k + r * (1 - k);
                g = ((colour >> 8) & 255) / 255.0f * k + g * (1 - k);
                b = (colour & 255) / 255.0f * k + b * (1 - k);
                a = k + a * (1 - k);
            };
            over(0xDEE8EE, band / 16.0f);
            over(0xECF2F6, ring / 16.0f);
            over(0xFFFFFF, edge / 16.0f);
            o[0] = (unsigned char)SDL_lroundf(a > 0 ? r / a * 255 : 0);
            o[1] = (unsigned char)SDL_lroundf(a > 0 ? g / a * 255 : 0);
            o[2] = (unsigned char)SDL_lroundf(a > 0 ? b / a * 255 : 0);
            o[3] = (unsigned char)SDL_lroundf(a * 255);
        }
    }
}

std::optional<aurora::rmlui::RuntimeTexture> serve(const Picture& p) {
    if (p.px.empty()) {
        return std::nullopt;
    }
    return aurora::rmlui::RuntimeTexture{(uint32_t)p.w, (uint32_t)p.h,
                                         std::span<const std::byte>(reinterpret_cast<const std::byte*>(p.px.data()),
                                                                    p.px.size()),
                                         false, true};
}

/* The disc's banner (opening.bnr, "BNR1"): a 96 x 32 picture at +0x20 in the GameCube's RGB5A3 format, in 4 x 4
 * tiles, read from the player's own disc and never stored */
bool read_banner() {
    DVDFileInfo fi;
    if (!DVDOpen("opening.bnr", &fi)) {
        return false;
    }
    alignas(32) static unsigned char bnr[0x1960 + 32];
    const s32 got = DVDReadPrio(&fi, bnr, 0x1840, 0, 2);
    DVDClose(&fi);
    if (got < 0x1820 || std::memcmp(bnr, "BNR", 3) != 0) {
        return false;
    }
    s_banner.assign(96 * 32 * 4, std::byte{0});
    const unsigned char* p = bnr + 0x20;
    for (int ty = 0; ty < 32 / 4; ++ty) {
        for (int tx = 0; tx < 96 / 4; ++tx) {
            for (int y = 0; y < 4; ++y) {
                for (int x = 0; x < 4; ++x, p += 2) {
                    const unsigned v = (unsigned)p[0] << 8 | p[1];
                    unsigned r, g, b, a;
                    if (v & 0x8000) { /* RGB555, opaque */
                        r = ((v >> 10) & 31) * 255 / 31;
                        g = ((v >> 5) & 31) * 255 / 31;
                        b = (v & 31) * 255 / 31;
                        a = 255;
                    } else { /* ARGB3444 */
                        a = ((v >> 12) & 7) * 255 / 7;
                        r = ((v >> 8) & 15) * 17;
                        g = ((v >> 4) & 15) * 17;
                        b = (v & 15) * 17;
                    }
                    std::byte* o = &s_banner[((ty * 4 + y) * 96 + tx * 4 + x) * 4];
                    o[0] = std::byte(r);
                    o[1] = std::byte(g);
                    o[2] = std::byte(b);
                    o[3] = std::byte(a);
                }
            }
        }
    }
    return true;
}

void set_text(const char* id, const std::string& rml);
void set_body_class(const char* name, bool on);

std::string rml_escape(const std::string& s); /* below: a file name may hold & or <, which RML would read as markup */

/* The current save's card (mockup 62, snatcherLeo 2026-11-13): the newest save in Saves\Slot 1, a .gci or an .orresave
 * (OrrePort's own save file, once it exists), shown as "colosseum-main.gci · 12 Nov 2026, 02:36" ("last saved" left
 * out so the longer .orresave name keeps the time in view), or "No save yet · a new game". The symbol and the tag
 * follow the format (body.orresave, hub.rcss). Case-insensitive names: Windows' files are, and a .GCI from another
 * tool should still count (on Linux and macOS this matters more, the file systems being case-sensitive). */
void show_save() {
    /* *Updated 2026-11-13 (step 2.1, the Saves page):* the save Play starts (pc_saves.c), shown by its name, its trainer
     * and play time ("WES · 41:22 played · 12 Nov 2026, 02:36"), read from the save without changing it. The older
     * file-name line below stays for a folder without the save slots (not the portable layout). */
    if (pc_saves_ready()) {
        PcSaveInfo info;
        const int slot = pc_saves_current();
        if (pc_saves_get(slot, &info)) {
            set_body_class("orresave", info.is_orresave != 0);
            /* the "Accurate" badge beside GCI (2.12; snatcherLeo, 2026-11-16): this save plays in GameCube-accurate mode */
            set_body_class("accsave", info.accurate != 0);
            std::string line = rml_escape(tx("hub.save.none"));
            if (info.has_save) {
                SDL_DateTime dt;
                std::string date;
                if (SDL_TimeToDateTime((SDL_Time)info.saved_time * 1000000000LL, &dt, true)) {
                    date = tx_date(dt.year, dt.month, dt.day, dt.hour, dt.minute);
                }
                char time[32];
                SDL_snprintf(time, sizeof(time), "%u:%02u", info.play_seconds / 3600, (info.play_seconds / 60) % 60);
                /* the coloured region word leads the line (3.5 step 6, proposal B; UI id hub.card.region): the short
                 * form ("PAL · ES"), the card's line being tight (snatcherLeo, 2026-11-17). Already RML (pc_hub_saves.cpp) */
                line = hub_save_line_rml(info, hub_fit_name(s_doc->GetElementById("cursub"), info.trainer), time, date,
                                         false, "curregion");
            }
            /* *Added 2026-11-21 (snatcherLeo, after "Play the autosave" left the card naming the save: "should we make it clear
             * that, the current save loaded, is an autosave (just in Hub)?"):* while the current save's autosave is
             * armed (pc_autosave_armed: PLAY will start the autosave, not the save), the card says so: an Autosave tag
             * beside the name (UI id hub.card.autosave_tag, body.autosavecur, hub.rcss), the line with the autosave's
             * own place, play time and date (UI id hub.card.autosave_line), and PLAY's target "<name> (autosave)" (UI id
             * hub.play.autosave). The hub only: the game itself shows the save's own Continue screen as always. All
             * platforms (RmlUi text) */
            static PcAutosaveInfo autosave; /* about 2 KB: off the stack */
            const bool fromAuto = info.has_save && pc_autosave_armed() == slot && pc_autosave_info(slot, &autosave) != 0;
            set_body_class("autosavecur", fromAuto);
            std::string playName = rml_escape(info.name);
            if (fromAuto) {
                SDL_DateTime dt;
                std::string date;
                if (SDL_TimeToDateTime((SDL_Time)autosave.unix_time * 1000000000LL, &dt, true)) {
                    date = tx_date(dt.year, dt.month, dt.day, dt.hour, dt.minute);
                }
                char time[32];
                SDL_snprintf(time, sizeof(time), "%u:%02u", autosave.play_seconds / 3600, (autosave.play_seconds / 60) % 60);
                const std::string trainer = hub_fit_name(s_doc->GetElementById("cursub"), info.trainer);
                line = rml_escape(autosave.place[0] != '\0'
                                      ? tx("hub.save.autosave_line",
                                           {{"place", autosave.place}, {"trainer", trainer}, {"time", time}, {"date", date}})
                                      : tx("hub.save.autosave_line_noplace", {{"trainer", trainer}, {"time", time}, {"date", date}}));
                playName = rml_escape(tx("hub.save.play_autosave", {{"name", info.name}}));
            }
            set_text("cursub", line);
            set_text("curslot", rml_escape(info.name));
            set_text("playslot", playName);
            /* *Added 2026-11-21 (snatcherLeo saw an 80-letter name run across PLAY and over its ▶ and word):* long save names
             * are cut with "…" (hub_fit_name_px) on PLAY and on the card, measured once the page is laid out:
             *   - PLAY: the name (or "<name> (autosave)") keeps to the button's right half, left of the A pill, so it
             *     never reaches "▶ PLAY" (which ends at about a third of the button);
             *   - the card: the name's line less its shown badges (GCI / ORRESAVE, GameCube-accurate, Autosave), whose
             *     place never shrinks (hub.rcss #curname).
             * All platforms (RmlUi's layout and measure) */
            s_doc->UpdateDocument();
            if (Rml::Element* play = s_doc->GetElementById("play")) {
                Rml::Element* slotEl = s_doc->GetElementById("playslot");
                Rml::Element* pill = s_doc->GetElementById("playa");
                const float dp = aurora::rmlui::get_context()->GetDensityIndependentPixelRatio();
                const int room = (int)(play->GetClientWidth() * 0.5f - (pill != nullptr ? pill->GetOffsetWidth() : 0.f) -
                                       20.f * dp);
                const bool autoTarget = fromAuto;
                if (slotEl != nullptr && room > 0)
                    slotEl->SetInnerRML(hub_fit_name_px(slotEl, info.name, room, [&](const std::string& n) {
                        return autoTarget ? tx("hub.save.play_autosave", {{"name", n}}) : n;
                    }));
            }
            if (Rml::Element* nameLine = s_doc->GetElementById("curname")) {
                Rml::Element* slotEl = s_doc->GetElementById("curslot");
                float tags = 0.f;
                for (const char* id : {"curtag-gci", "curtag-orre", "curtag-acc", "curtag-auto"}) {
                    Rml::Element* t = s_doc->GetElementById(id);
                    if (t != nullptr && t->GetOffsetWidth() > 0.f) /* a hidden badge has no width */
                        tags += t->GetOffsetWidth() + t->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Left);
                }
                const int room = (int)(nameLine->GetClientWidth() - tags);
                if (slotEl != nullptr && room > 0)
                    slotEl->SetInnerRML(hub_fit_name_px(slotEl, info.name, room, [](const std::string& n) { return n; }));
            }
            std::printf("[hub] current save: slot %d \"%s\" (%s)%s%s\n", slot, info.name, info.has_save ? info.trainer : "not started",
                        info.accurate ? ", GameCube-accurate mode" : "",
                        fromAuto ? ", PLAY starts its autosave" : "");
            return;
        }
    }
    /* *Fixed 2026-11-21 (4.1):* the folder the card really reads, not a hard-coded Slot 1: the current slot's when the
     * save slots are in use (its setup file unreadable above), else Aurora's single card in the settings folder (a flat
     * folder with no Colosseum\), where Slot 1's folder doesn't exist at all. All platforms */
    char slot[1300];
    if (pc_saves_ready()) {
        pc_saves_slot_dir(pc_saves_current(), slot, sizeof(slot));
    } else {
        pc_paths_get(PC_PLACE_SETTINGS, "USA/Card A", slot, sizeof(slot));
    }
    std::string best;
    SDL_Time best_time = 0;
    bool found = false;
    for (const char* pattern : {"*.gci", "*.orresave"}) {
        int count = 0;
        if (char** files = SDL_GlobDirectory(slot, pattern, SDL_GLOB_CASEINSENSITIVE, &count)) {
            for (int i = 0; i < count; ++i) {
                SDL_PathInfo info;
                if (SDL_GetPathInfo((std::string(slot) + "/" + files[i]).c_str(), &info) &&
                    (!found || info.modify_time > best_time)) {
                    found = true;
                    best = files[i];
                    best_time = info.modify_time;
                }
            }
            SDL_free(files);
        }
    }
    const std::string ext = ".orresave";
    bool orre = found && best.size() > ext.size();
    for (size_t i = 0; orre && i < ext.size(); ++i) {
        orre = SDL_tolower((unsigned char)best[best.size() - ext.size() + i]) == ext[i];
    }
    set_body_class("orresave", orre);
    set_body_class("accsave", false); /* no save slots: no per-save accurate mode to show */
    set_body_class("autosavecur", false); /* nor autosaves (they live in the slots) */
    /* the line in the player's language: the file and the date through "hub.save.line" and "date.time", so each
     * language orders them its own way (pc_text.h) */
    std::string line = tx("hub.save.none");
    if (found) {
        /* a long file name is cut in its middle ("01-GC6E-pokemon…um.gci"), keeping its start and its kind, so the time
         * after it stays in view (a Dolphin-exported name is 30 letters; the card's line is cut at its end) */
        std::string shown = best;
        if (shown.size() > 24) {
            size_t head = 15, tail = shown.size() - 7;
            while (head > 0 && (static_cast<unsigned char>(shown[head]) & 0xC0) == 0x80) --head; /* whole UTF-8 letters */
            while (tail < shown.size() && (static_cast<unsigned char>(shown[tail]) & 0xC0) == 0x80) ++tail;
            shown = shown.substr(0, head) + "…" + shown.substr(tail);
        }
        line = shown;
        SDL_DateTime dt;
        if (SDL_TimeToDateTime(best_time, &dt, true)) {
            line = tx("hub.save.line", {{"file", shown}, {"date", tx_date(dt.year, dt.month, dt.day, dt.hour, dt.minute)}});
        }
    }
    set_text("cursub", rml_escape(line));
    const std::string slot_name = rml_escape(tx("hub.save.slot", {{"n", "1"}}));
    set_text("curslot", slot_name);
    set_text("playslot", slot_name);
    std::printf("[hub] current save: %s (%s)\n", found ? best.c_str() : "none yet", orre ? ".orresave" : ".gci");
}

void set_text(const char* id, const std::string& rml) {
    if (Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById(id) : nullptr) {
        e->SetInnerRML(rml);
    }
}

/* a note in plain text (escaped here: a translation or a file name may hold & or <) */
void set_note(const std::string& text) { set_text("note", rml_escape(text)); }

void set_body_class(const char* name, bool on) {
    if (Rml::Element* body = s_doc != nullptr ? s_doc->GetElementById("body") : nullptr) {
        body->SetClass(name, on);
    }
}

/* Play as a held button (snatcherLeo, 2026-11-13: "Pressing the button without letting go only gets it to lift, but to
 * actually get it spinning, you let go of the button"): pressing A (Enter, Space, or the mouse on PLAY) lifts the disc
 * and keeps it lifted; letting go spins it and starts the game. A mouse let go off PLAY puts the disc back down. */
bool s_hitch = false;       /* the disc is lifted, waiting for the press to end */
bool s_hitch_mouse = false; /* ... and the press is the mouse's */

/* the idle sway (hub.rcss #discfloat's float, #disc's tilt) frozen where it is at the press, so the bounce, the snap and
 * the spin all start from the pose the disc had (snatcherLeo, 2026-11-13: "No matter which way it was rotated, when I press
 * the button, it would do the bounce from there"); switching the animation off would snap it back to level */
/* the pose is set again on the next frames: RmlUi clears an animated property when its animation is switched off, on
 * its next update, which wiped the pose set at the press (verified: the log had rotate(-1.143deg), the picture was
 * level) */
Rml::String s_frozen[2];
int s_freeze_frames = 0;

void freeze_sway(bool on) {
    if (s_doc == nullptr) return;
    int k = 0;
    for (const char* id : {"discfloat", "disc"}) {
        Rml::Element* e = s_doc->GetElementById(id);
        if (e == nullptr) {
            ++k;
            continue;
        }
        if (on) {
            const Rml::Property* now = e->GetProperty("transform"); /* the sway's value this frame */
            const Rml::String pose = now != nullptr ? now->ToString() : Rml::String("none");
            s_frozen[k] = pose.empty() ? Rml::String("none") : pose;
            e->SetProperty("animation", "none");
            e->SetProperty("transform", s_frozen[k]);
            std::printf("[hub] the sway held at the press: #%s %s\n", id, s_frozen[k].c_str());
        } else {
            e->RemoveProperty("animation");
            e->RemoveProperty("transform");
        }
        ++k;
    }
    s_freeze_frames = on ? 3 : 0;
}

/* each hub frame: the frozen pose put back for a few frames after the press (see above) */
void freeze_sway_frame() {
    if (s_freeze_frames <= 0 || s_doc == nullptr) return;
    --s_freeze_frames;
    int k = 0;
    for (const char* id : {"discfloat", "disc"}) {
        if (Rml::Element* e = s_doc->GetElementById(id)) e->SetProperty("transform", s_frozen[k]);
        ++k;
    }
}

void hitch(bool mouse) {
    if (s_play || s_no_disc || !s_offer_path.empty() || s_hitch) {
        return;
    }
    s_hitch = true;
    s_hitch_mouse = mouse;
    freeze_sway(true);
    set_body_class("hitched", true); /* the press, held (hub-play.rcss) */
}

void unhitch() {
    if (!s_hitch) return;
    s_hitch = false;
    s_hitch_mouse = false;
    freeze_sway(false); /* the sway carries on */
    set_body_class("hitched", false); /* the disc settles back */
}

void upd_close(const char* why); /* the update panel (below, "updates") */

void play() {
    if (s_play || s_no_disc) {
        return;
    }
    upd_close("PLAY");
    pc_update_set_playing(1); /* a download in progress slows down so the game never stutters (4.4) */
    /* GameCube-accurate mode plays plain .gci saves only (decided 2.4): a save holding long names (an .orresave) is
     * offered a GameCube copy instead (mockup 104 D), never played as it is. *Since 2.12 (2026-11-16)* the mode is the
     * current save's own setting (pc_option_gamecube_accurate reads it until the game starts); OrrePort never sets it
     * on an .orresave, so this is the guard for a slot.ini changed by hand or a test (ORRE_PRESET=accurate) */
    if (pc_option_gamecube_accurate() && pc_saves_ready()) {
        PcSaveInfo cur;
        if (pc_saves_get(pc_saves_current(), &cur) && cur.is_orresave) {
            std::printf("[hub] Play: accurate mode and an .orresave (slot %d): a GameCube copy is offered\n", cur.slot);
            hub_saves_offer_copy(cur.slot);
            return;
        }
    }
    s_play = true;
    s_play_at = SDL_GetTicks();
    set_body_class("prelifted", s_hitch); /* already up: the spin starts from the held lift, not from the ground */
    s_hitch = false;
    s_hitch_mouse = false;
    set_body_class("playing", true); /* the Lift and spin, then black (hub.rcss) */
    std::printf("[hub] Play: Colosseum, slot %d\n", pc_saves_current());
}

/* Report a problem: the game menu's panel (snatcherLeo, 2026-11-10: "it should just launch the usual problem log thing"):
 * what happened (optional), then Save report writes the zip (the save, the log, PC details; no recording or game
 * picture before Play) and shows it in Explorer */
bool s_report = false;
int s_focus_note = 0; /* frames until the text box takes the keyboard (it can't while the panel is still hidden) */

std::string report_note() {
    auto* control = dynamic_cast<Rml::ElementFormControl*>(s_doc != nullptr ? s_doc->GetElementById("rnote") : nullptr);
    return control != nullptr ? control->GetValue() : std::string();
}

void report_placeholder() { set_body_class("hasnote", !report_note().empty()); }

void report() {
    if (s_play || s_report || s_doc == nullptr) {
        return;
    }
    s_report = true;
    if (auto* control = dynamic_cast<Rml::ElementFormControl*>(s_doc->GetElementById("rnote"))) {
        control->SetValue("");
    }
    set_body_class("reporting", true);
    report_placeholder();
    s_focus_note = 2;
    std::printf("[hub] Report a problem opened\n");
}

/* ---- updates (step 4.4, 2026-11-23; port/src/pc_update.c does the work, this only shows it) ----
 * Why here: one update covers OrrePort and Studio, and most players meet it in the hub (snatcherLeo's reasoning when he
 * approved Studio's mockup, "For Hub I'm fine with it"): the note beside the gear, the panel with what's new and
 * Download / Not now, "Update ready", Restart now. The look is the page's (hub.rml / hub.rcss: snatcherLeo may redesign it;
 * only the element ids matter here). Nothing shows during play (the hub isn't drawn then). */
/* the OrrePort folder, for pc_apps (everything there is per folder, so Studio and the hub in one folder see each other
 * and a development build never sees the player's) */
const char* apps_root() {
    static char root[1300];
    if (root[0] == '\0') pc_paths_dir(PC_PLACE_ROOT, root, sizeof root);
    return root;
}

PcUpdateStatus s_upd{};
bool s_upd_panel = false;
bool s_upd_started = false;

std::string upd_size(uint64_t bytes) { /* "48 MB" through the shared update.size ({mb}) */
    char b[32];
    std::snprintf(b, sizeof b, bytes >= 10ull << 20 ? "%.0f" : "%.1f", (double)bytes / (1024.0 * 1024.0));
    return tx("update.size", {{"mb", b}});
}
int upd_percent() { return s_upd.size > 0 ? (int)(s_upd.done * 100 / s_upd.size) : 0; }

/* the panel's contents for the current state */
void upd_panel_fill() {
    const bool ready = s_upd.state == PC_UPDATE_READY;
    const bool downloading = s_upd.state == PC_UPDATE_DOWNLOADING;
    set_text("uh", rml_escape(ready ? tx("update.ready", {{"version", s_upd.latest}})
                                    : tx("update.available", {{"version", s_upd.latest}})));
    std::string notes = ready ? tx("update.hub.ready_text", {{"version", s_upd.latest}})
                              : (s_upd.notes[0] ? std::string(s_upd.notes) : tx("update.hub.no_notes"));
    std::string rml;
    for (const char c : rml_escape(notes)) rml += c == '\n' ? std::string("<br/>") : std::string(1, c);
    set_text("unotes", rml);
    set_text("uinfo", ready ? std::string() : rml_escape(tx("update.both") + "  " + upd_size(s_upd.size)));
    std::string warn;
    if (ready && s_upd.other_running) warn = tx("update.close_other", {{"program", "Studio"}});
    if (s_upd.state == PC_UPDATE_FAILED && s_upd.error_key[0]) warn = tx(s_upd.error_key);
    set_text("uwarn", rml_escape(warn));
    set_text("ulater", rml_escape(tx(ready ? "update.hub.later" : "update.hub.not_now")) + " <span class=\"bb\">B</span>");
    /* ready while Studio is open: the button becomes "Close Studio" (Studio's design, snatcherLeo approved, 2026-11-23), which
     * asks Studio to close its usual way (pc_apps_close); once it has, the button is Restart now again */
    const bool close_studio = ready && s_upd.other_running && pc_apps_running(PC_APP_STUDIO, apps_root());
    set_text("ugo", downloading ? rml_escape(tx("update.downloading", {{"version", s_upd.latest},
                                                                       {"percent", std::to_string(upd_percent())}}))
                    : close_studio ? rml_escape(tx("update.close_button", {{"program", "Studio"}})) + " <span class=\"ab\">A</span>"
                                : rml_escape(tx(ready ? "update.restart"
                                                      : s_upd.state == PC_UPDATE_FAILED ? "update.retry" : "update.download")) +
                                      " <span class=\"ab\">A</span>");
    if (Rml::Element* e = s_doc->GetElementById("ugo"))
        e->SetClass("off", downloading || (ready && s_upd.other_running && !close_studio));
}

/* the note beside the gear; hidden when there is nothing to say */
void upd_note_fill() {
    std::string text;
    switch (s_upd.state) {
    case PC_UPDATE_AVAILABLE: text = tx("update.available", {{"version", s_upd.latest}}); break;
    case PC_UPDATE_DOWNLOADING:
        text = tx("update.downloading", {{"version", s_upd.latest}, {"percent", std::to_string(upd_percent())}});
        break;
    case PC_UPDATE_READY: text = tx("update.note.ready"); break;
    case PC_UPDATE_FAILED: /* only after a newer version was found (a failed check alone shows nothing) */
        if (s_upd.latest[0]) text = tx("update.note.failed");
        break;
    default: break;
    }
    set_text("updnote", rml_escape(text));
    if (Rml::Element* e = s_doc->GetElementById("updnote")) e->SetClass("ready", s_upd.state == PC_UPDATE_READY);
    set_body_class("upd", !text.empty());
    if (s_upd_panel) upd_panel_fill();
}

/* at the hub's start: the check (unless "Check for updates" is off), never in a test run (no network in tests) */
void upd_start() {
    if (s_upd_started) return;
    s_upd_started = true;
    if (pc_platform_automated_run() || SDL_getenv("ORRE_TEST_RUN") != nullptr) return;
    char root[1300];
#ifdef ORRE_VERSION
    const char* version = ORRE_VERSION;
#else
    const char* version = "0.0.0";
#endif
    if (!pc_update_init(pc_paths_dir(PC_PLACE_ROOT, root, sizeof root), version, pc_text_language())) return;
    pc_update_set_auto_download(pc_settings_int("update_auto_download", 0) != 0);
    pc_update_check(pc_settings_int("update_check", 1) != 0);
    std::printf("[hub] updates: version %s, checking %s, automatic download %s\n", version,
                pc_settings_int("update_check", 1) ? "on" : "off (fully offline)",
                pc_settings_int("update_auto_download", 0) ? "on" : "off");
}

void upd_frame() {
    if (s_doc == nullptr) return;
    /* compared with what's shown, not by poll's "changed" flag: Settings › About reads the same state, and whichever
     * reads first would take the flag */
    PcUpdateStatus st;
    pc_update_poll(&st);
    if (st.state != s_upd.state || st.done != s_upd.done || st.other_running != s_upd.other_running ||
        std::strcmp(st.latest, s_upd.latest) != 0 || std::strcmp(st.error_key, s_upd.error_key) != 0) {
        s_upd = st;
        upd_note_fill();
    }
}

/* "Open Studio ↗" or, while Studio is open, "● Studio is open · Show" (Studio's design, snatcherLeo approved 2026-11-23);
 * looked at once a second (a mutex check: microseconds). Windows-only underneath (pc_apps.c); elsewhere always "Open" */
void studio_link_frame() {
    static Uint64 last = 0;
    static int shown = -1;
    if (s_doc == nullptr || SDL_GetTicks() - last < 1000) return;
    last = SDL_GetTicks();
    /* ORRE_HUB_TEST_SETTINGS=<seconds> (a test switch, never a setting; 2026-11-23): opens Settings once, that many
     * seconds after the hub's start, as the gear would (pictures of About and its Go back pop-up) */
    if (static bool opened = false; !opened) {
        if (const char* t = SDL_getenv("ORRE_HUB_TEST_SETTINGS"); t != nullptr && SDL_GetTicks() >= (Uint64)SDL_atoi(t) * 1000) {
            opened = true;
            pc_settings_overlay_open_in_hub();
        }
    }
    /* ORRE_HUB_TEST_STUDIO=<seconds> (a test switch, never a setting; 2026-11-23): presses the link once, that many
     * seconds after the hub's start, as a click would (the pc_apps check without moving the player's mouse) */
    if (static bool pressed = false; !pressed) {
        if (const char* t = SDL_getenv("ORRE_HUB_TEST_STUDIO"); t != nullptr && SDL_GetTicks() >= (Uint64)SDL_atoi(t) * 1000) {
            pressed = true;
            if (Rml::Element* e = s_doc->GetElementById("b-studio")) e->Click();
        }
    }
    const int open = pc_apps_running(PC_APP_STUDIO, apps_root());
    if (open == shown) return;
    shown = open;
    if (Rml::Element* e = s_doc->GetElementById("b-studio")) {
        /* *Changed 2026-11-23 (snatcherLeo: "keep the arow, and just say "Studio is opened""):* the same link with other
         * words, the arrow kept; no dot or "· Show" (a click still brings Studio forward) */
        e->SetInnerRML(rml_escape(tx(open ? "hub.link.studio_open" : "hub.link.studio")) + " <span class=\"sym\">↗</span>");
        e->SetClass("open", open != 0);
    }
}

/* ---- the "closed unexpectedly" bar (5.1, snatcherLeo 2026-11-23: "Yes, as suggested"; Studio's same bar and words) ----
 * At the start, when the previous run's log has no clean-exit line (pc_log.h says how that's told), a bar under the
 * links: "OrrePort closed unexpectedly last time. Make a report?" with Make a report / Not now. While it stands, any
 * report (the bar's button, or the usual Report a problem link: the controller's way there) carries that earlier log
 * too (pc_report.cpp); once a report has, or after Not now, it's forgotten and the bar goes. Never in test runs.
 * Words: report.crash_* (shared with Studio). UI ids: crashbar, cbtext, cb-report, cb-dismiss. All platforms (RmlUi) */
void crash_bar_show(bool on) {
    set_body_class("crashed", on);
    if (on) set_text("cbtext", rml_escape(tx("report.crash_bar", {{"program", "OrrePort"}})));
}
void crash_bar_start() {
    const bool test = pc_platform_automated_run() || SDL_getenv("ORRE_TEST_RUN") != nullptr;
    if (pc_log_previous_unclean() == nullptr || test) return;
    std::printf("[hub] the previous run ended unexpectedly (%s): offering a report\n", pc_log_previous_unclean());
    crash_bar_show(true);
}
/* the bar goes once a report has carried the earlier log (pc_report.cpp forgets it then) */
void crash_bar_frame() {
    static bool was = false;
    const bool now = pc_log_previous_unclean() != nullptr;
    if (was && !now && s_doc != nullptr) crash_bar_show(false);
    was = now;
}

void upd_open() {
    if (s_upd.state != PC_UPDATE_AVAILABLE && s_upd.state != PC_UPDATE_DOWNLOADING && s_upd.state != PC_UPDATE_READY &&
        !(s_upd.state == PC_UPDATE_FAILED && s_upd.latest[0]))
        return;
    s_upd_panel = true;
    upd_panel_fill();
    set_body_class("updating", true);
    std::printf("[hub] update panel opened (%s)\n", s_upd.latest);
}

void upd_close(const char* why) {
    if (!s_upd_panel) return;
    s_upd_panel = false;
    set_body_class("updating", false);
    std::printf("[hub] update panel closed (%s)\n", why);
}

/* A: Download, or Restart now once it's ready (the hub exits with the update code; the launcher swaps app\ and starts
 * OrrePort again). Restart waits while Studio is open (the swap can't replace a running program's files) */
void upd_go() {
    if (s_upd.state == PC_UPDATE_AVAILABLE || s_upd.state == PC_UPDATE_FAILED) {
        std::printf("[hub] update: Download %s\n", s_upd.latest);
        pc_update_download();
    } else if (s_upd.state == PC_UPDATE_READY && s_upd.other_running && pc_apps_running(PC_APP_STUDIO, apps_root())) {
        std::printf("[hub] update: Close Studio (asked: %d)\n", pc_apps_close(PC_APP_STUDIO, apps_root()));
    } else if (s_upd.state == PC_UPDATE_READY && !s_upd.other_running) {
        std::printf("[hub] update: Restart now (%s)\n", s_upd.latest);
        pc_settings_save();
        pc_update_shutdown();
        std::fflush(stdout);
        std::fflush(stderr);
        pc_log_exit_clean("restarting for the update (the hub's Restart now)");
        _Exit(PC_UPDATE_EXIT_RESTART);
    }
}

bool s_report_saved = false; /* the panel shows its saved screen (Studio's flow, 2026-11-23) */
std::string s_report_zip;     /* ...for that zip (Open folder) */

void close_report(const char* why) {
    if (!s_report) {
        return;
    }
    s_report = false;
    s_report_saved = false;
    if (Rml::Element* e = s_doc->GetElementById("rnote")) {
        e->Blur();
    }
    set_body_class("reporting", false);
    set_body_class("reportsaved", false);
    std::printf("[hub] Report a problem closed (%s)\n", why);
}

/* Open folder on the saved screen: Explorer on the reports folder with the zip selected (never in test runs) */
void report_open_folder() {
    if (!s_report_saved || s_report_zip.empty()) {
        return;
    }
    std::printf("[hub] report: Open folder\n");
    if (SDL_getenv("ORRE_TEST_RUN") == nullptr && !pc_platform_automated_run()) {
        pc_report_show_in_folder(s_report_zip.c_str());
    }
}

/* *Changed 2026-11-23 (snatcherLeo: "Studio's report thing is actually far more elegant, saves it to clipboard and lets user
 * know, and then tells them they can open file epxlorer afterwards to locate file ... we can match the UI and
 * process"):* after Save the panel stays open on a saved screen, as Studio's does: what happened (copied or not), the
 * zip's name and size (red past about 10 MB, many share places' limit), Open folder and Done. Explorer no longer opens by
 * itself. Studio's words (report.saved_body etc., the same text as studio.report.*). */
void save_report() {
    if (!s_report || s_report_saved) {
        return;
    }
    char zip[1300];
    const std::string typed = report_note();
    const std::string note = typed + (typed.empty() ? "" : "\n\n") +
                             (s_no_disc ? "(saved from the hub, no disc added yet)" : "(saved from the hub, before the game started)");
    if (!pc_report_write(note.c_str(), nullptr, zip, sizeof(zip))) {
        close_report("saved, failed");
        set_note(tx("hub.note.report_failed"));
        return;
    }
    /* ORRE_TEST_RUN=1 (the project's own scripted test windows) and automated runs: the clipboard left alone */
    const bool test = SDL_getenv("ORRE_TEST_RUN") != nullptr || pc_platform_automated_run();
    /* copied as a file for Ctrl+V (snatcherLeo, 2026-11-22: the same habit as Studio's report; pc_clip_file.h); the screen
     * says "copied" only when it was. No channel named (snatcherLeo, 2026-11-13: "the support thread may change") */
    const bool copied = !test && pc_clipboard_set_file(zip) != 0;
    std::printf("[hub] report %s\n", copied ? "copied to the clipboard as a file" : "not copied (a test run, or no clipboard)");
    s_report_zip = zip;
    s_report_saved = true;
    if (Rml::Element* e = s_doc->GetElementById("rnote")) {
        e->Blur(); /* Enter is Done now, not a new line in the note */
    }
    SDL_PathInfo info;
    const double mb = SDL_GetPathInfo(zip, &info) ? (double)info.size / (1024.0 * 1024.0) : 0.0;
    char size[32];
    std::snprintf(size, sizeof(size), "%.1f MB", mb);
    const bool big = mb > 9.5;
    const size_t slash = s_report_zip.find_last_of("/\\");
    set_text("rsavedmsg", rml_escape(tx(copied ? "report.saved_body" : "report.saved_not_copied")));
    set_text("rfilename", rml_escape(slash == std::string::npos ? s_report_zip : s_report_zip.substr(slash + 1)));
    set_text("rfilesize", rml_escape(tx("report.size", {{"size", size}})));
    set_text("rbig", big ? rml_escape(tx("report.too_big", {{"size", size}})) : std::string());
    if (Rml::Element* e = s_doc->GetElementById("rfilesize")) {
        e->SetClass("big", big);
    }
    set_body_class("reportsaved", true);
    std::printf("[hub] report saved: %s, %s%s\n", size, copied ? "copied" : "not copied", big ? " (over 9.5 MB)" : "");
}

/* ---- adding a disc ---- */

int copy_thread(void*) {
    if (s_copy_move && SDL_RenamePath(s_copy_src.c_str(), s_copy_dst.c_str())) {
        s_copy_done = s_copy_total.load(); /* the same drive: moved at once */
        pc_file_touch_utf8(s_copy_dst.c_str()); /* a move keeps the old time; the shared rule takes the newest (4.4) */
        s_copy_state = 2;
        return 0;
    }
    SDL_IOStream* in = SDL_IOFromFile(s_copy_src.c_str(), "rb");
    const std::string part = s_copy_dst + ".part";
    SDL_IOStream* out = in != nullptr ? SDL_IOFromFile(part.c_str(), "wb") : nullptr;
    bool ok = in != nullptr && out != nullptr;
    if (ok) {
        std::vector<unsigned char> buf(8u << 20);
        for (;;) {
            const size_t n = SDL_ReadIO(in, buf.data(), buf.size());
            if (n == 0) {
                ok = SDL_GetIOStatus(in) == SDL_IO_STATUS_EOF;
                break;
            }
            if (SDL_WriteIO(out, buf.data(), n) != n) {
                ok = false;
                break;
            }
            s_copy_done += n;
        }
    }
    if (out != nullptr && !SDL_CloseIO(out)) {
        ok = false;
    }
    if (in != nullptr) {
        SDL_CloseIO(in);
    }
    if (ok && !SDL_RenamePath(part.c_str(), s_copy_dst.c_str())) {
        ok = false;
    }
    if (ok && s_copy_move) {
        /* Move into OrrePort across drives: the player's original goes only once the whole copy is in place */
        SDL_PathInfo a, b;
        if (SDL_GetPathInfo(s_copy_src.c_str(), &a) && SDL_GetPathInfo(s_copy_dst.c_str(), &b) && a.size == b.size) {
            SDL_RemovePath(s_copy_src.c_str());
        }
    }
    if (!ok) {
        s_copy_error = SDL_GetError();
        SDL_RemovePath(part.c_str()); /* our own unfinished copy */
    }
    s_copy_state = ok ? 2 : 3;
    return 0;
}

std::string file_name(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string gigabytes(Uint64 bytes) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1f", bytes / 1073741824.0); /* as Windows' Explorer counts: the ISO is 1.4 GB */
    return tx("hub.add.size", {{"gb", buf}});
}

/* plain text (escaped here) */
void drop_texts(const std::string& title, const std::string& sub, const std::string& sub2) {
    set_text("droptitle", rml_escape(title));
    set_text("dropsub", rml_escape(sub));
    set_text("dropsub2", rml_escape(sub2));
}

void default_drop_texts() {
    drop_texts(tx("hub.add.drop"), tx("hub.add.drop.which"), tx("hub.add.drop.copy"));
}

/* Copy (the default) or Move the offered disc into ROM\ */
void accept_offer(bool move) {
    if (s_offer_path.empty() || s_copy_state == 1) {
        return;
    }
    s_copy_src = s_offer_path;
    s_copy_dst = rom_folder() + file_name(s_offer_path);
    s_copy_total = s_offer_size;
    s_copy_done = 0;
    s_copy_move = move;
    s_copy_state = 1;
    s_offer_path.clear();
    set_body_class("offer", false);
    set_body_class("copying", true);
    drop_texts(tx(move ? "hub.add.moving" : "hub.add.copying"), file_name(s_copy_src), "");
    std::printf("[hub] %s %s into %s\n", move ? "moving" : "copying", s_copy_src.c_str(), rom_folder().c_str());
    SDL_Thread* t = SDL_CreateThread(copy_thread, "hub disc copy", nullptr);
    if (t == nullptr) {
        s_copy_state = 3;
        s_copy_error = SDL_GetError();
    } else {
        SDL_DetachThread(t);
    }
}

void cancel_offer() {
    if (s_offer_path.empty()) {
        return;
    }
    s_offer_path.clear();
    set_body_class("offer", false);
    default_drop_texts();
    set_note("");
}

/* a dropped or chosen file: checked first; then the player chooses Copy (the default: the original stays where it was)
 * or Move (no extra disk space), with the file's size shown (plan "Adding a disc": "Copy or move") */
void consider(const std::string& path) {
    if (!s_no_disc || s_copy_state == 1 || s_disc_found) {
        return;
    }
    char why[400];
    std::printf("[hub] disc offered: %s\n", path.c_str());
    if (!pc_platform_disc_identify(path.c_str(), why, sizeof(why))) {
        cancel_offer();
        set_note(why);
        std::printf("[hub] not the USA disc: %s\n", why);
        return;
    }
    if (path.rfind(rom_folder(), 0) == 0) {
        s_watch_scanned.clear(); /* already in ROM\: just look again */
        return;
    }
    SDL_PathInfo info;
    s_offer_path = path;
    s_offer_size = SDL_GetPathInfo(path.c_str(), &info) ? info.size : 0;
    set_body_class("offer", true);
    drop_texts(tx("hub.add.found"), tx("hub.add.found.file", {{"file", file_name(path)}, {"size", gigabytes(s_offer_size)}}),
               tx("hub.add.found.choice", {{"size", gigabytes(s_offer_size)}}));
    set_note("");
    /* test hooks: ORRE_HUB_TEST_ADD accepts with Copy; ORRE_HUB_TEST_ADD_MOVE=0 / 1 with Copy / Move, anything else
     * leaves the offer waiting (for a picture of it) */
    const char* test = SDL_getenv("ORRE_HUB_TEST_ADD_MOVE");
    if (test != nullptr) {
        if ((test[0] == '0' || test[0] == '1') && test[1] == '\0') {
            accept_offer(test[0] == '1');
        }
    } else if (SDL_getenv("ORRE_HUB_TEST_ADD") != nullptr) {
        accept_offer(false);
    }
}

void SDLCALL picked(void*, const char* const* files, int) {
    if (files != nullptr && files[0] != nullptr) {
        const std::lock_guard lock(s_pick_mutex);
        s_picked = files[0];
    }
}

void browse() {
    /* the names live as long as the dialog may read them (static: the dialog can outlive this call) */
    static std::string discs_name, all_name;
    discs_name = tx("hub.add.filter.discs");
    all_name = tx("hub.add.filter.all");
    static SDL_DialogFileFilter filters[2];
    filters[0] = {discs_name.c_str(), "iso;gcm;ciso;rvz;gcz;wia"};
    filters[1] = {all_name.c_str(), "*"};
    int n = 0;
    SDL_Window* window = nullptr;
    if (SDL_Window** all = SDL_GetWindows(&n)) {
        window = n > 0 ? all[0] : nullptr;
        SDL_free(all);
    }
    SDL_ShowOpenFileDialog(picked, nullptr, window, filters, 2, nullptr, false);
}

void open_rom_folder() {
    const std::string folder = rom_folder();
    SDL_CreateDirectory(folder.c_str());
    if (!SDL_OpenURL(folder.c_str())) {
        set_note(tx("hub.add.folder_failed", {{"folder", folder}}));
    }
}

/* ROM\'s contents (names and sizes), to notice a disc put there by hand */
std::string rom_signature() {
    const std::string folder = rom_folder();
    std::string sig;
    int count = 0;
    if (char** files = SDL_GlobDirectory(folder.c_str(), "*", 0, &count)) {
        for (int i = 0; i < count; ++i) {
            SDL_PathInfo info;
            if (SDL_GetPathInfo((folder + files[i]).c_str(), &info)) {
                sig += std::string(files[i]) + ":" + std::to_string(info.size) + ";";
            }
        }
        SDL_free(files);
    }
    return sig;
}

/* ---- the folder watch while a disc is open (step 4.4, 2026-11-23; docs/port/disc-sync-review.md, decided: "both apps
 * watch ROM\ once a second while open") ----
 * Why only a note: Windows won't let the open disc be deleted or replaced while OrrePort runs (verified 2026-11-23), so
 * the one change to meet is another US disc added (by Explorer or Studio's Discs). The shared rule (pc_us_disc_pick:
 * the newest original, then by name) says which disc the next start will use; when that isn't the open one, the note
 * says so. The disc isn't switched under the hub: the open one already passed the start-up checks. Every OS the same. */
extern "C" int pc_us_disc_pick(char* out, size_t size); /* pc_region_text.cpp */
extern "C" const char* pc_platform_disc_image(void);    /* pc_platform.c */
std::string rom_signature();                             /* below */

void watch_open_disc() {
    static Uint64 last = 0;
    static std::string seen, settled;
    const Uint64 now = SDL_GetTicks();
    if (s_no_disc || s_play || now - last < 1000) return;
    last = now;
    const std::string sig = rom_signature();
    if (seen.empty()) { seen = settled = sig; return; } /* the folder as the hub found it */
    const bool still = sig == seen; /* acted on once a file's size stops changing (a copy in progress) */
    seen = sig;
    if (!still || sig == settled) return;
    settled = sig;
    char pick[1500];
    pc_us_disc_pick(pick, sizeof pick);
    std::string a = pick, b = pc_platform_disc_image();
    for (char& c : a) c = c == '\\' ? '/' : (char)std::tolower((unsigned char)c);
    for (char& c : b) c = c == '\\' ? '/' : (char)std::tolower((unsigned char)c);
    std::printf("[hub] ROM folder changed: the rule picks %s\n", pick[0] ? pick : "(no US disc)");
    if (!a.empty() && a != b) set_note(tx("hub.disc.newer"));
}

/* once a second while no disc is there: the copy's progress; a new file in ROM\ looked at once its size stops
 * changing (so a copy made in Explorer is not read half-written) */
void watch_disc() {
    static Uint64 last = 0;
    const Uint64 now = SDL_GetTicks();
    {
        std::string picked_path;
        {
            const std::lock_guard lock(s_pick_mutex);
            picked_path.swap(s_picked);
        }
        if (!picked_path.empty()) {
            consider(picked_path);
        }
    }
    if (s_copy_state == 1) {
        const Uint64 total = s_copy_total, done = s_copy_done;
        set_note(tx(s_copy_move ? "hub.add.moving.progress" : "hub.add.copying.progress",
                    {{"percent", std::to_string(total > 0 ? (int)(done * 100 / total) : 0)}}));
        return;
    }
    if (s_copy_state == 3) {
        s_copy_state = 0;
        set_body_class("copying", false);
        default_drop_texts();
        set_note(tx(s_copy_move ? "hub.add.move_failed" : "hub.add.copy_failed", {{"error", s_copy_error}}));
        return;
    }
    if (s_copy_state == 2) {
        s_copy_state = 0;
        set_body_class("copying", false);
        s_watch_scanned.clear(); /* look now */
        s_watch_last = rom_signature();
    } else if (now - last < 1000) {
        return;
    } else {
        last = now;
        const std::string sig = rom_signature();
        const bool settled = sig == s_watch_last;
        s_watch_last = sig;
        if (!settled || sig == s_watch_scanned) {
            return;
        }
    }
    s_watch_scanned = s_watch_last;
    char message[3200];
    if (pc_platform_disc_scan(message, sizeof(message))) {
        s_disc_found = true;
        std::printf("[hub] disc found in the ROM folder\n");
    } else if (pc_platform_disc_scan_saw_other()) { /* was a search for English words in the message */
        set_note(tx("hub.add.not_colosseum"));
    }
}

/* ---- the disc turned by hand (snatcherLeo, 2026-11-09: "dragging the disc just rotates it, and you can make it a fast spin,
 * but it will always go back to its default position") ----
 * Held, the disc follows the hand round its centre, like a record turned by a finger; let go, it keeps its speed and
 * slows with friction; once slow it eases to the next upright position in the direction it was going (a spring with a
 * little overshoot). Until Play. */

float s_turn_target = 0.0f;
bool s_turn_settling = false;

bool disc_centre(float& cx, float& cy, float& radius) {
    Rml::Element* e = s_doc != nullptr ? s_doc->GetElementById("disc") : nullptr;
    if (e == nullptr) {
        return false;
    }
    const Rml::Vector2f off = e->GetAbsoluteOffset(Rml::BoxArea::Border);
    const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
    cx = off.x + size.x * 0.5f;
    cy = off.y + size.y * 0.5f;
    radius = size.x * 0.5f;
    return true;
}

float wrap180(float d) {
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

class Turn : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (s_labels) {
            return; /* the label page covers the disc: its own mouse (LabelMouse) */
        }
        const float x = event.GetParameter<float>("mouse_x", 0.0f), y = event.GetParameter<float>("mouse_y", 0.0f);
        float cx, cy, radius;
        if (!disc_centre(cx, cy, radius)) {
            return;
        }
        const float angle = SDL_atan2f(y - cy, x - cx) * 57.29578f;
        const Uint64 now = SDL_GetTicks();
        switch (event.GetId()) {
        case Rml::EventId::Mousedown:
            /* *Changed 2026-11-21 (snatcherLeo: "can we have it so that the disc is not rotateable/spinable via mouse click? We
             * just show a picture and that's it?"):* not with no disc added (s_no_disc): the placeholder is a still
             * picture (no turn, no pencil, the arrow cursor: hub.rcss body.nodisc). With a disc, as before */
            if (!s_play && !s_report && !s_no_disc && (x - cx) * (x - cx) + (y - cy) * (y - cy) <= radius * radius) {
                s_turn_drag = true;
                s_turn_settling = false;
                s_turn_grab = angle;
                s_turn_speed = 0.0f;
                s_turn_last_move = now;
            }
            break;
        case Rml::EventId::Mousemove:
            if (s_turn_drag) {
                const float d = wrap180(angle - s_turn_grab);
                const float dt = SDL_max(0.004f, (now - s_turn_last_move) / 1000.0f);
                s_turn_grab = angle;
                s_turn_angle += d;
                s_turn_speed = 0.5f * s_turn_speed + 0.5f * (d / dt);
                s_turn_last_move = now;
            }
            break;
        case Rml::EventId::Mouseup:
            if (s_turn_drag) {
                s_turn_drag = false;
                if (now - s_turn_last_move > 80) {
                    s_turn_speed = 0.0f; /* held still before letting go: no flick */
                }
                s_turn_speed = SDL_clamp(s_turn_speed, -2400.0f, 2400.0f);
            }
            break;
        default:
            break;
        }
    }
};
Turn s_turn;

void turn_update() {
    static Uint64 last = SDL_GetTicks();
    const Uint64 now = SDL_GetTicks();
    const float dt = SDL_min(0.05f, (now - last) / 1000.0f);
    last = now;
    if (s_play) {
        s_turn_drag = false;
    }
    if (s_turn_drag) {
        if (now - s_turn_last_move > 60) {
            s_turn_speed *= 0.8f; /* held still: no speed left to throw */
        }
    } else if (!s_turn_settling && SDL_fabsf(s_turn_speed) > 120.0f) {
        s_turn_speed *= SDL_expf(-0.9f * dt); /* friction: a 2400°/s flick turns for about three seconds */
        s_turn_angle += s_turn_speed * dt;
    } else if (s_turn_angle != 0.0f || s_turn_speed != 0.0f) {
        if (!s_turn_settling) {
            s_turn_settling = true; /* home: the next upright position the way it was turning */
            const float turns = s_turn_angle / 360.0f;
            s_turn_target = 360.0f * (s_turn_speed > 30.0f    ? SDL_ceilf(turns)
                                      : s_turn_speed < -30.0f ? SDL_floorf(turns)
                                                              : SDL_roundf(turns));
        }
        const float accel = -40.0f * (s_turn_angle - s_turn_target) - 9.0f * s_turn_speed;
        s_turn_speed += accel * dt;
        s_turn_angle += s_turn_speed * dt;
        if (SDL_fabsf(s_turn_angle - s_turn_target) < 0.02f && SDL_fabsf(s_turn_speed) < 0.5f) {
            s_turn_angle = s_turn_speed = 0.0f;
            s_turn_settling = false;
        }
    }
    if (SDL_fabsf(s_turn_angle - s_turn_shown) > 0.01f) {
        if (Rml::Element* box = s_doc != nullptr ? s_doc->GetElementById("discdrag") : nullptr) {
            char value[48];
            std::snprintf(value, sizeof(value), "rotate(%.3fdeg)", s_turn_angle);
            box->SetProperty("transform", value);
            s_turn_shown = s_turn_angle;
        }
    }
}

/* ---- the disc label page (mockup 47's label shelf; snatcherLeo, 2026-11-13: "Yes, build it") ----
 * The player picks what the hub's disc wears: OrrePort's own label, the game's banner (read from their disc), or any
 * picture of theirs, placed by dragging and zooming. The page covers the hub (body.labels) with the label as it will
 * look (the editor), the shelf under it and the zoom row. Why a shelf: switching between several pictures is one
 * click; snatcherLeo's "what if a user has a bunch?" is answered by showing only OrrePort's, the banner and the 5 most recently
 * worn pictures, then an "All n" tile that opens every picture as a scrolling grid (no cap on how many are kept).
 * The pictures, their places and the choice live in pc_disc_label.cpp; this part is the page and the drawing sizes.
 * The hub's disc changes when the page closes (it is hidden under the page meanwhile), so dragging only redraws the
 * editor's smaller label. Platform-neutral (RmlUi, SDL); the file dialog is SDL's own on every OS. */
namespace dl = disc_label;
constexpr const char* kLabelGame = "colosseum"; /* this disc's game; XD's disc gets its own label later */
/* the project's answer to "what if a user has a bunch?" (the decision log). *Updated 2026-11-13:* 4 (was 5) since the third
 * built-in (Alternate) arrived, so the shelf keeps its 9 discs in one row; the rest are under All */
constexpr int kShelfPictures = 4;
constexpr int kBuiltIns = 3; /* the built-in labels at the shelf's start: OrrePort's, Alternate, the banner */

bool s_labels_all = false; /* ... with its All grid over it (body.labelsall) */
Picture s_label_own;       /* OrrePort's label as drawn at setup (hole and margin included): back when it is worn */
Picture s_smear_own;       /* ... and its Play spin smear (make_smear, the designed one) */
Picture s_blank_own;       /* ... and the blank disc under it (the no-disc picture, and the base the spin fades to) */
Picture s_own_edit, s_own_thumb;                    /* ... at the editor's and the thumbnails' sizes (hole, no margin) */
dl::Rgba s_frame_hub, s_frame_edit, s_frame_thumb; /* the rim and the clear centre alone, at the three sizes */
int s_disc_px = 1024, s_edit_px = 512, s_thumb_px = 128;
std::string s_wear = "orreport"; /* what the disc wears: "orreport", "classic", "banner" or "picture:<file>" */
dl::Rgba s_full, s_work;         /* the worn picture as read (at most 4096 a side), and at the editor's working size */
std::string s_full_file;
std::vector<std::string> s_order; /* the shelf's order, fixed while the page is open: OrrePort's, banner, pictures */
std::map<std::string, dl::Rgba> s_small; /* each picture at most 512 a side (the worker reads them), for thumbnails */
struct Thumb {
    int id = 0, ver = 0;
    bool ready = false;
    Picture pic;
    std::string shown; /* the texture name the page uses now, released when a newer one replaces it */
};
std::map<std::string, Thumb> s_thumbs; /* by key */
int s_next_thumb_id = 1;
Picture s_edit;
int s_edit_ver = 0;
bool s_edit_dirty = false, s_shelf_dirty = false;
bool s_edit_drag = false, s_zoom_drag = false;
float s_drag_mx = 0.0f, s_drag_my = 0.0f;
float s_ax = 0.0f, s_ay = 0.0f, s_arz = 0.0f, s_alt = 0.0f, s_art = 0.0f; /* the controller's sticks and triggers */
Uint64 s_place_changed = 0; /* a place changed by the wheel or the stick: saved once it rests (0: nothing waiting) */
std::string s_label_picked; /* from the picture dialog (its callback may run on another thread; s_pick_mutex) */

/* Saving (snatcherLeo, 2026-11-13, after his first try: "there should be a button to "save" very clearly, so I know it's
 * saved", "after I click save, a title box will open to allow me to name it", "users can rename very easily"). A new
 * picture is a draft (key "draft": shown on the disc and the shelf, no file yet) until Save, which asks for its name
 * and only then copies it into the folder; moving or zooming a saved picture changes s_work_place, written on Save.
 * Leaving or switching with something unsaved asks first (Save / Don't save / Cancel), so nothing is lost by accident.
 * Which label the disc wears still changes at once: that is a choice, not an edit. */
constexpr const char* kDraft = "draft";
std::string s_draft_source, s_draft_name; /* the new picture's file, and the name the box offers (its file's) */
std::string s_before_draft = "orreport";  /* what was worn before the draft, worn again if it is not saved */
dl::Place s_work_place;                   /* the worn picture's place as being edited */
bool s_dirty = false;                     /* s_work_place differs from what is saved */
enum Ask { ASK_NONE, ASK_NAME_NEW, ASK_RENAME, ASK_LEAVE };
Ask s_ask = ASK_NONE;                     /* the name box or the leave question is open */
std::function<void()> s_after_leave;      /* what the leave question was asked for, done once it is answered */
int s_name_focus = 0;                     /* frames until the name box takes the keyboard (as the report's box) */
/* Undo / redo of the worn picture's place (snatcherLeo, 2026-11-13: ""CTRL+Z" can help when a user makes a minor slip up
 * when adjusting the art"): one step is one gesture (a drag, a burst of wheel zooming, a zoom-bar move, a stick push,
 * Reset), recorded when it ends (place_settled). Ctrl+Z steps back, Ctrl+Y or Ctrl+Shift+Z forward. Kept for the worn
 * picture only: wearing another one, a new draft or Don't save start a fresh history. */
std::vector<dl::Place> s_undo, s_redo;
dl::Place s_gesture_start;                /* the place when the current gesture began */
bool s_gesture_open = false;
void forget_history();                    /* below, with undo_place */

/* the thumbnails' pictures are read off the frame: a big PNG takes a tenth of a second or more, and a shelf of dozens
 * would stall the page. One worker reads one file at a time; the frame picks the results up (labels_frame). */
std::mutex s_small_mutex;
std::vector<std::string> s_small_queue;
std::vector<std::pair<std::string, dl::Rgba>> s_small_done;
std::vector<std::string> s_small_asked;
SDL_Thread* s_small_thread = nullptr;
SDL_Semaphore* s_small_wake = nullptr;
std::atomic<bool> s_small_quit{false};

int small_worker(void*) {
    while (!s_small_quit) {
        SDL_WaitSemaphore(s_small_wake);
        for (;;) {
            std::string file;
            {
                const std::lock_guard lock(s_small_mutex);
                if (s_small_queue.empty() || s_small_quit) break;
                file = s_small_queue.front();
                s_small_queue.erase(s_small_queue.begin());
            }
            dl::Rgba full, small;
            std::string why;
            if (dl::decode(dl::folder() + file, full, why)) {
                dl::shrink(full, 512, small);
            } else {
                std::printf("[hub] disc label %s: %s\n", file.c_str(), why.c_str());
            }
            const std::lock_guard lock(s_small_mutex);
            s_small_done.emplace_back(file, std::move(small)); /* empty: unreadable (a grey tile stays) */
        }
    }
    return 0;
}

void request_small(const std::string& file) {
    const std::lock_guard lock(s_small_mutex);
    if (std::find(s_small_asked.begin(), s_small_asked.end(), file) != s_small_asked.end()) return;
    s_small_asked.push_back(file);
    s_small_queue.push_back(file);
    if (s_small_wake == nullptr) s_small_wake = SDL_CreateSemaphore(0);
    if (s_small_thread == nullptr) s_small_thread = SDL_CreateThread(small_worker, "disc labels", nullptr);
    SDL_SignalSemaphore(s_small_wake);
}

void stop_small_worker() {
    if (s_small_thread == nullptr) return;
    s_small_quit = true;
    SDL_SignalSemaphore(s_small_wake);
    SDL_WaitThread(s_small_thread, nullptr);
    s_small_thread = nullptr;
}

bool is_picture_key(const std::string& key) { return key.rfind("picture:", 0) == 0; }
std::string key_file(const std::string& key) { return is_picture_key(key) ? key.substr(8) : std::string(); }

Picture as_picture(dl::Rgba&& r) {
    Picture p;
    p.w = r.w;
    p.h = r.h;
    p.px = std::move(r.px);
    return p;
}

dl::Rgba as_rgba(Picture&& p) {
    dl::Rgba r;
    r.w = p.w;
    r.h = p.h;
    r.px = std::move(p.px);
    return r;
}

/* a label the page can move and zoom: a saved picture or the draft */
bool is_editable(const std::string& key) { return is_picture_key(key) || key == kDraft; }

dl::Place place_of(const std::string& key) {
    if (s_labels && key == s_wear) return s_work_place; /* the worn one as being edited (saved on Save) */
    const dl::Entry* e = dl::find(key_file(key));
    return e != nullptr ? e->place : dl::Place{};
}

/* The "Alternate" label (ui/discs/colosseum-classic.svg, key "classic"; snatcherLeo, 2026-11-13: "if we still have this older
 * design we changed, we could include it as "built-in" options, so we now have 3 built-in disc art options", then
 * "name the disc art we just added from "OrrePort classic" to "Alternate""): the black label
 * signed off on 2026-11-09, before the half-and-half one. Drawn once per size it is needed at (the hub's disc, the
 * editor, the thumbnails), with the GameCube's hole, no margin. Its spin uses the art's own smear (smear_from), as
 * pictures do. */
Picture classic_at(int size) {
    static std::map<int, Picture> s_drawn;
    auto it = s_drawn.find(size);
    if (it != s_drawn.end()) return it->second;
    Picture p;
    load_svg("discs/colosseum-classic.svg", size, p);
    gamecube_hole(p);
    s_drawn[size] = p;
    return p;
}

/* the label for key drawn at frame's size, with the GameCube's hole (no margin); own: OrrePort's at that size */
Picture label_at(const std::string& key, const dl::Rgba& frame, const dl::Rgba* pic, const Picture& own) {
    dl::Rgba out;
    if (key == "classic") {
        Picture p = classic_at(frame.w);
        return p.px.empty() ? own : p; /* a missing file: OrrePort's */
    }
    if (key == "banner") {
        dl::compose_banner(s_banner.empty() ? nullptr : reinterpret_cast<const unsigned char*>(s_banner.data()), frame,
                           out);
    } else if (is_editable(key) && pic != nullptr && !pic->px.empty()) {
        dl::compose_picture(*pic, place_of(key), frame, out);
    } else {
        return own;
    }
    Picture p = as_picture(std::move(out));
    gamecube_hole(p);
    return p;
}

void label_note(const std::string& text) { set_text("lnote", rml_escape(text)); } /* plain text, escaped here */

/* the worn picture read in full (once per picture); false if it can't be read */
bool ensure_full(const std::string& file) {
    if (s_full_file == file && !s_full.px.empty()) return true;
    std::string why;
    dl::Rgba full;
    if (!dl::decode(dl::folder() + file, full, why)) {
        label_note(why);
        return false;
    }
    s_full = std::move(full);
    s_full_file = file;
    dl::shrink(s_full, 2 * s_edit_px, s_work); /* the editor's working copy: a drag redraws in a few milliseconds */
    return true;
}

/* The Play spin's smear for any label but OrrePort's (snatcherLeo, 2026-11-13: "the disc turns a strange blue when it starts
 * spinning ... make sure no color change"). make_smear draws OrrePort's label as it looks spinning, in that label's own
 * indigo and black, so any other art turned OrrePort-blue as the spin began. A spinning disc shows, at each distance
 * from its centre, the average colour round that circle; this draws exactly that from the worn label (no margin yet),
 * so the spin keeps the art's own colours. OrrePort's label keeps its designed smear (style 04, signed off). */
void smear_from(const Picture& label, int size, Picture& out) {
    const int bins = size / 2; /* one ring per output pixel of radius */
    const float centre = label.w * 0.5f;
    std::vector<float> ring((size_t)bins * 4, 0.0f); /* premultiplied colour and alpha per ring */
    for (int b = 0; b < bins; ++b) {
        const float r = (b + 0.5f) / bins * centre;
        const int n = SDL_max(24, (int)(6.2832f * r / 2.0f)); /* a sample about every 2 label pixels round the ring */
        double sum[4] = {0, 0, 0, 0};
        for (int k = 0; k < n; ++k) {
            const float a = 6.2832f * k / n;
            const int x = SDL_clamp((int)(centre + r * SDL_cosf(a)), 0, label.w - 1);
            const int y = SDL_clamp((int)(centre + r * SDL_sinf(a)), 0, label.h - 1);
            const unsigned char* p = &label.px[((size_t)y * label.w + x) * 4];
            const double al = p[3] / 255.0;
            sum[0] += p[0] / 255.0 * al;
            sum[1] += p[1] / 255.0 * al;
            sum[2] += p[2] / 255.0 * al;
            sum[3] += al;
        }
        for (int c = 0; c < 4; ++c) ring[(size_t)b * 4 + c] = (float)(sum[c] / n);
    }
    out.w = out.h = size;
    out.px.assign((size_t)size * size * 4, 0);
    const float half = size * 0.5f;
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            const float d = SDL_sqrtf((px + 0.5f - half) * (px + 0.5f - half) + (py + 0.5f - half) * (py + 0.5f - half));
            const float f = d - 0.5f; /* between rings: blended, so the rings don't show */
            if (f >= bins - 1) continue;
            const int b0 = SDL_max(0, (int)f), b1 = SDL_min(bins - 1, b0 + 1);
            const float t = SDL_clamp(f - b0, 0.0f, 1.0f);
            float v[4];
            for (int c = 0; c < 4; ++c) v[c] = ring[(size_t)b0 * 4 + c] * (1 - t) + ring[(size_t)b1 * 4 + c] * t;
            unsigned char* o = &out.px[((size_t)py * size + px) * 4];
            for (int c = 0; c < 3; ++c) o[c] = (unsigned char)SDL_lroundf(v[3] > 0 ? v[c] / v[3] * 255.0f : 0.0f);
            o[3] = (unsigned char)SDL_lroundf(v[3] * 255.0f);
        }
    }
}

/* the hub's own disc wears the chosen label (at start, and when the page closes) */
void apply_label_to_hub() {
    if (is_picture_key(s_wear) && !ensure_full(key_file(s_wear))) {
        s_wear = "orreport";
    }
    if (s_wear == "orreport") {
        s_label = s_label_own;
        s_smear = s_smear_own;
        s_blank = s_blank_own;
    } else {
        dl::Rgba src;
        if (is_picture_key(s_wear)) {
            /* about two picture pixels per disc pixel at this zoom: sharp, and not more work than that */
            const float need = 2.0f * s_disc_px * SDL_powf(4.0f, place_of(s_wear).zoom / 100.0f);
            dl::shrink(s_full, (int)SDL_min(need, 4096.0f), src);
        }
        Picture p = label_at(s_wear, s_frame_hub, &src, s_label_own);
        /* the two blurs side by side (pc_parallel.h; both only read p, each writes its own picture) */
        pc_run_together({
            [&] {
                smear_from(p, s_disc_px / 2, s_smear); /* the spin in the art's own colours */
                add_margin(s_smear, s_disc_px / 2 / 32);
            },
            /* the layer under the label: Play fades the sharp label out (hub-play.rcss "sharp", 1 -> 0) over the blank
             * disc, which is OrrePort's indigo-and-black blank, so custom art showed OrrePort's indigo through it as it
             * spun (snatcherLeo, 2026-11-13: "it does look a lightish blue"; measured: blue rose against green mid-spin). With
             * custom art the base is the art's own spin blur, at full size, so nothing foreign shows through */
            [&] {
                smear_from(p, s_disc_px, s_blank);
                add_margin(s_blank, s_disc_px / 32);
            },
        });
        add_margin(p, s_disc_px / 32);
        s_label = std::move(p);
    }
    Rml::ReleaseTexture("disc://label"); /* the disc and its trail copies load the new one when next drawn */
    Rml::ReleaseTexture("disc://smear");
    Rml::ReleaseTexture("disc://blank");
    std::printf("[hub] the disc wears %s\n", s_wear.c_str());
}

std::string rml_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        o += c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : c == '"' ? "&quot;" : std::string(1, c);
    }
    return o;
}

/* a picture's name on the shelf: no extension, and long names cut with "…" (snatcherLeo, 2026-11-12, for long profile
 * names: "we just use a 3 dot") */
std::string shelf_name(const std::string& key) {
    /* the built-ins in the player's language ("Alternate": snatcherLeo, 2026-11-13, "name the disc art we just added ... to
     * "Alternate""); a player's own picture keeps the name they gave it */
    if (key == "orreport") return tx("label.name.orreport");
    if (key == "classic") return tx("label.name.classic");
    if (key == "banner") return tx("label.name.banner");
    if (key == kDraft) return tx("label.name.new");
    std::string n = key_file(key);
    const size_t dot = n.find_last_of('.');
    if (dot != std::string::npos && dot > 0) n.resize(dot);
    if (n.size() > 14) { /* by bytes; a cut inside a UTF-8 letter is backed up to the letter's start */
        size_t cut = 13;
        while (cut > 0 && (static_cast<unsigned char>(n[cut]) & 0xC0) == 0x80) --cut;
        n = n.substr(0, cut) + "…";
    }
    return n;
}

Thumb& thumb(const std::string& key) {
    Thumb& t = s_thumbs[key];
    if (t.id == 0) t.id = s_next_thumb_id++;
    if (!t.ready) {
        if (key == "orreport") {
            t.pic = s_own_thumb;
            t.ready = true;
        } else if (key == "banner" || key == "classic") {
            t.pic = label_at(key, s_frame_thumb, nullptr, s_own_thumb);
            t.ready = true;
        } else if (key == kDraft) { /* the draft is always the worn one: its working copy is in memory */
            t.pic = label_at(key, s_frame_thumb, &s_work, s_own_thumb);
            t.ready = true;
        } else if (const auto it = s_small.find(key_file(key)); it != s_small.end()) {
            if (!it->second.px.empty()) {
                t.pic = label_at(key, s_frame_thumb, &it->second, s_own_thumb);
                t.ready = true;
            }
        } else {
            request_small(key_file(key));
        }
        if (t.ready) ++t.ver;
    }
    return t;
}

/* a thumbnail redrawn (its picture moved, or read at last) */
void thumb_stale(const std::string& key) {
    auto it = s_thumbs.find(key);
    if (it != s_thumbs.end()) it->second.ready = false;
    s_shelf_dirty = true;
}

std::vector<std::string> shelf_pictures() {
    std::vector<std::string> pics;
    for (const std::string& k : s_order) {
        if (is_editable(k)) pics.push_back(k);
    }
    return pics;
}

/* what the shelf shows: OrrePort's, the banner, then the first kShelfPictures pictures */
std::vector<std::string> shelf_visible() {
    std::vector<std::string> v;
    int pictures = 0;
    for (const std::string& k : s_order) {
        if (!is_editable(k) || pictures++ < kShelfPictures) v.push_back(k);
    }
    return v;
}

/* the shelf's order when the page opens: the built-in labels (kBuiltIns), then the pictures most recently worn first.
 * *Updated 2026-11-13:* three built-ins, Alternate added (classic_at) */
void order_shelf() {
    s_order = {"orreport", "classic", "banner"};
    std::vector<dl::Entry> pics = dl::entries();
    std::stable_sort(pics.begin(), pics.end(), [](const dl::Entry& a, const dl::Entry& b) { return a.used > b.used; });
    for (const dl::Entry& e : pics) s_order.push_back("picture:" + e.file);
}

/* a picture worn from the All grid comes to the front of the shelf's pictures, so the shelf shows what is worn */
void ensure_visible(const std::string& key) {
    const std::vector<std::string> v = shelf_visible();
    if (!is_picture_key(key) || std::find(v.begin(), v.end(), key) != v.end()) return;
    s_order.erase(std::remove(s_order.begin(), s_order.end(), key), s_order.end());
    s_order.insert(s_order.begin() + kBuiltIns, key);
    s_shelf_dirty = true;
}

/* the zoom bar: its left end a quarter size (zoom -100), its middle mark "just fills the disc" (0), its right end 4x
 * (100) (*Changed 2026-11-15*, snatcherLeo's mockup A: "let users zoom out even further"; it ran from fill to 4x) */
void zoom_knob() {
    const float z = is_editable(s_wear) ? place_of(s_wear).zoom : 0.0f;
    char pct[32];
    std::snprintf(pct, sizeof(pct), "%.2f%%", (z + 100.0f) * 0.5f);
    if (Rml::Element* e = s_doc->GetElementById("lzoomknob")) e->SetProperty("left", pct);
    if (Rml::Element* e = s_doc->GetElementById("lzoomfill")) e->SetProperty("width", pct);
}

/* What the hub's own disc shows: the saved choice (labels.txt), which a draft or unsaved moves don't change. Shown as
 * an "ON HUB" tag on its shelf disc and a line under the big disc (snatcherLeo, 2026-11-13: "How can we make it super clear to
 * the user, which disc will show up in the Hub's front page?"; the project's suggestion, "Yes, build the tag and the line"):
 * the tag moves only when the change is real (a saved disc clicked, a new one saved), so during a draft it stays on the
 * disc the hub still shows. */
std::string hub_key() { return dl::wearing(kLabelGame); }

std::string hub_name() {
    const std::string k = hub_key();
    if (k == "orreport") return tx("label.hub_name.orreport");
    if (k == "classic") return tx("label.hub_name.classic");
    if (k == "banner") return tx("label.hub_name.banner");
    std::string n = key_file(k);
    const size_t dot = n.find_last_of('.');
    if (dot != std::string::npos && dot > 0) n.resize(dot);
    return n;
}

/* the Save button, the Rename button, the hints and the hub line follow what is unsaved (body.lunsaved, body.ldraft) */
void save_state() {
    const bool draft = s_wear == kDraft;
    set_body_class("lunsaved", draft || s_dirty);
    set_body_class("ldraft", draft);
    set_text("l-save", rml_escape(tx(draft || s_dirty ? "common.save" : "label.saved")));
    std::string line;
    if (draft) {
        line = rml_escape(tx("label.status.unsaved", {{"name", hub_name()}}));
    } else if (s_dirty) {
        line = rml_escape(tx("label.status.unsaved_saved"));
    } else {
        line = "<span class=\"lsdot\"></span> " + rml_escape(tx("label.status.shown"));
    }
    set_text("lstatus", line);
}

void wear(const std::string& key) {
    if (is_picture_key(key) && !ensure_full(key_file(key))) {
        return; /* unreadable: the note says why, the disc keeps what it wore */
    }
    s_wear = key;
    s_work_place = dl::Place{};
    forget_history();
    if (dl::Entry* e = dl::find(key_file(key))) {
        e->used = dl::next_used();
        s_work_place = e->place;
    }
    s_dirty = false;
    dl::set_wearing(kLabelGame, key); /* saves labels.txt, the new "used" too */
    set_body_class("lpic", is_editable(key));
    save_state();
    label_note("");
    s_edit_dirty = s_shelf_dirty = true;
}

void edit_refresh() {
    s_edit_dirty = false;
    s_edit = label_at(s_wear, s_frame_edit, &s_work, s_own_edit);
    const std::string old = "label://e/" + std::to_string(s_edit_ver);
    ++s_edit_ver;
    set_text("leditbox", "<img id=\"ledit\" src=\"label://e/" + std::to_string(s_edit_ver) + "\"/>");
    Rml::ReleaseTexture(old); /* each version once: dragging makes many */
    zoom_knob();
}

std::string tile_rml(const std::string& key, size_t index) {
    Thumb& t = thumb(key);
    std::string pic;
    if (t.ready) {
        const std::string src = "label://t/" + std::to_string(t.id) + "/" + std::to_string(t.ver);
        if (!t.shown.empty() && t.shown != src) Rml::ReleaseTexture(t.shown);
        t.shown = src;
        pic = "<img src=\"" + src + "\"/>";
    } else {
        pic = "<div class=\"lph\"></div>"; /* read in the background; drawn when it arrives */
    }
    const std::string tag = key == hub_key() ? "<div class=\"onhub\"><span>" + rml_escape(tx("label.on_hub")) + "</span></div>"
                                             : ""; /* save_state says why */
    return std::string("<div class=\"lt") + (key == s_wear ? " on" : "") + "\" id=\"lk-" + std::to_string(index) +
           /* the outline: a smooth picture (hubicon://ring, hub.rcss .edge says why), not a round border */
           "\"><div class=\"ring\">" + pic + "<img class=\"edge\" src=\"hubicon://ring\"/>" + tag +
           "</div><div class=\"ln\">" +
           rml_escape(shelf_name(key)) + "</div></div>";
}

std::string add_tile() {
    return "<div class=\"lt\" id=\"l-add\"><div class=\"ring\"><div class=\"ltile plus\">+</div></div>"
           "<div class=\"ln\">" + rml_escape(tx("label.tile.add")) + "</div></div>";
}

void build_shelf() {
    s_shelf_dirty = false;
    std::string rml;
    const std::vector<std::string> visible = shelf_visible();
    for (const std::string& k : visible) {
        rml += tile_rml(k, std::find(s_order.begin(), s_order.end(), k) - s_order.begin());
    }
    const std::vector<std::string> pics = shelf_pictures();
    if ((int)pics.size() > kShelfPictures) {
        rml += "<div class=\"lt\" id=\"l-all\"><div class=\"ring\"><div class=\"ltile\">" + rml_escape(tx("label.tile.all_short")) +
               "</div></div><div class=\"ln\">" +
               rml_escape(tx("label.tile.all", {{"n", std::to_string(pics.size())}})) + "</div></div>";
    }
    rml += add_tile();
    set_text("lshelf", rml);
    /* the hint bar's "Y All" shows with the All tile only (body.labelsmany, hub.rcss) */
    set_body_class("labelsmany", (int)pics.size() > kShelfPictures);
    if (s_labels_all) {
        std::string grid = add_tile();
        for (const std::string& k : pics) {
            grid += tile_rml(k, std::find(s_order.begin(), s_order.end(), k) - s_order.begin());
        }
        set_text("lgrid", grid);
        set_text("lcount", rml_escape(tx_count("label.count", (long)pics.size())));
    }
}

/* the disc label page's pieces: the rim and the clear centre alone (the label drawn without any of its printing or its
 * two halves) at the editor's (400 dp) and the thumbnails' (84 dp) sizes, and OrrePort's label at those two sizes.
 * Drawn the first time the page opens, side by side (pc_parallel.h), not at every start (2026-11-13: the hub's start
 * drew them though most starts never open the page). setup() draws the hub-size rim (s_frame_hub) itself. */
const std::vector<std::string> kPagePrinting = {"#3D4F8F", "#0C0C0D", "#FFCB05", "#E3350D", "#141A33",
                                                "#0A0A0B", "#F49AC8", "#E9EDEA", "#C3CAC6"};
void page_pictures() {
    if (!s_frame_edit.px.empty()) {
        return;
    }
    Picture frame_edit, frame_thumb;
    pc_run_together({
        [&] { load_svg("discs/colosseum.svg", s_edit_px, frame_edit, kPagePrinting); },
        [&] { load_svg("discs/colosseum.svg", s_thumb_px, frame_thumb, kPagePrinting); },
        [&] {
            load_svg("discs/colosseum.svg", s_edit_px, s_own_edit);
            gamecube_hole(s_own_edit);
        },
        [&] {
            load_svg("discs/colosseum.svg", s_thumb_px, s_own_thumb);
            gamecube_hole(s_own_thumb);
        },
    });
    s_frame_edit = as_rgba(std::move(frame_edit));
    s_frame_thumb = as_rgba(std::move(frame_thumb));
}

void open_labels() {
    if (s_doc == nullptr || s_no_disc || s_play || s_labels) return; /* already open: keep its draft and edits */
    page_pictures();
    dl::load();
    s_wear = dl::wearing(kLabelGame);
    order_shelf();
    s_labels = true;
    s_labels_all = false;
    s_turn_drag = false;
    ltop_set(0); /* the controller starts on the page, not its top row */
    set_body_class("labels", true);
    set_body_class("labelsall", false);
    set_body_class("lpic", is_picture_key(s_wear));
    if (is_picture_key(s_wear) && !ensure_full(key_file(s_wear))) s_wear = "orreport";
    const dl::Entry* worn = dl::find(key_file(s_wear));
    s_work_place = worn != nullptr ? worn->place : dl::Place{};
    s_dirty = false;
    s_ask = ASK_NONE;
    set_body_class("lnaming", false);
    set_body_class("lleaving", false);
    save_state();
    label_note("");
    set_text("lformats", rml_escape(dl::formats_text())); /* what this build reads */
    s_edit_dirty = s_shelf_dirty = true;
    std::printf("[hub] disc label page opened: %zu pictures, wearing %s\n", dl::entries().size(), s_wear.c_str());
}

void close_labels() {
    if (!s_labels) return;
    s_labels = s_labels_all = false;
    s_edit_drag = s_zoom_drag = false;
    ltop_set(0);
    set_body_class("labels", false);
    set_body_class("labelsall", false);
    dl::save();
    apply_label_to_hub();
}

void show_all(bool on) {
    s_labels_all = on;
    set_body_class("labelsall", on);
    if (!on) ensure_visible(s_wear);
    s_shelf_dirty = true;
}

/* ---- saving: the name box and the leave question (the comment at s_draft_source says why) ---- */

/* the name a saved picture shows in the name box: its file name without ".png" */
std::string name_of(const std::string& key) {
    std::string n = key == kDraft ? s_draft_name : key_file(key);
    const size_t dot = n.find_last_of('.');
    if (key != kDraft && dot != std::string::npos && dot > 0) n.resize(dot);
    return n;
}

void open_name_box(Ask kind) {
    s_ask = kind;
    set_text("lnamet", rml_escape(tx(kind == ASK_RENAME ? "label.rename_box" : "label.name_box")));
    if (auto* in = dynamic_cast<Rml::ElementFormControl*>(s_doc->GetElementById("lnamein"))) {
        in->SetValue(name_of(s_wear));
    }
    set_body_class("lnaming", true);
    s_name_focus = 2; /* the box can take the keyboard once it is shown */
}

void close_ask() {
    s_ask = ASK_NONE;
    set_body_class("lnaming", false);
    set_body_class("lleaving", false);
}

void run_after_leave() {
    std::function<void()> next = std::move(s_after_leave);
    s_after_leave = nullptr;
    if (next) next();
}

/* the worn picture's new place written (Save on a saved picture) */
void commit_place() {
    if (dl::Entry* e = dl::find(key_file(s_wear))) {
        e->place = s_work_place;
        dl::save();
        thumb_stale(s_wear);
        std::printf("[hub] disc art saved: %s\n", e->file.c_str());
    }
    s_dirty = false;
    save_state();
}

/* what isn't saved is let go: the draft disappears (no file was made), a moved picture goes back to its saved place */
void discard_unsaved() {
    if (s_wear == kDraft) {
        s_order.erase(std::remove(s_order.begin(), s_order.end(), std::string(kDraft)), s_order.end());
        if (auto it = s_thumbs.find(kDraft); it != s_thumbs.end()) {
            if (!it->second.shown.empty()) Rml::ReleaseTexture(it->second.shown);
            s_thumbs.erase(it);
        }
        s_draft_source.clear();
        s_full_file.clear();
        const std::string back = s_before_draft;
        s_wear = "orreport";
        wear(back);
        if (s_wear != back) wear("orreport"); /* the earlier picture can't be read any more: OrrePort's */
        std::printf("[hub] new disc art not saved: let go\n");
        return;
    }
    if (const dl::Entry* e = dl::find(key_file(s_wear))) {
        s_work_place = e->place;
    }
    forget_history();
    s_dirty = false;
    s_edit_dirty = true;
    save_state();
}

/* what would be lost asks first (Save / Don't save / Cancel); with nothing unsaved, next happens at once */
void guarded(std::function<void()> next) {
    if (s_ask != ASK_NONE) return;
    if (s_wear != kDraft && !s_dirty) {
        next();
        return;
    }
    s_after_leave = std::move(next);
    s_ask = ASK_LEAVE;
    set_text("lleavet", rml_escape(tx(s_wear == kDraft ? "label.leave.new.title" : "label.leave.changed.title")));
    set_text("lleaves", rml_escape(s_wear == kDraft ? tx("label.leave.new")
                                                    : tx("label.leave.changed", {{"name", name_of(s_wear)}})));
    set_body_class("lleaving", true);
}

/* Save: a new picture asks for its name first (then is saved); a saved one's place is written at once */
void save_pressed() {
    if (s_ask != ASK_NONE && s_ask != ASK_LEAVE) return;
    close_ask();
    if (s_wear == kDraft) {
        open_name_box(ASK_NAME_NEW); /* run_after_leave waits for the name */
        return;
    }
    if (s_dirty) commit_place();
    run_after_leave();
}

void name_confirmed() {
    std::string typed;
    if (auto* in = dynamic_cast<Rml::ElementFormControl*>(s_doc->GetElementById("lnamein"))) typed = in->GetValue();
    if (dl::clean_name(typed).empty()) typed = name_of(s_wear); /* nothing usable typed: the offered name */
    std::string file, why;
    if (s_ask == ASK_NAME_NEW) {
        if (!dl::add(s_draft_source, file, why, typed)) {
            label_note(why);
            return;
        }
        const std::string key = "picture:" + file;
        if (dl::Entry* e = dl::find(file)) e->place = s_work_place;
        std::replace(s_order.begin(), s_order.end(), std::string(kDraft), key);
        if (auto it = s_thumbs.find(kDraft); it != s_thumbs.end()) {
            if (!it->second.shown.empty()) Rml::ReleaseTexture(it->second.shown);
            s_thumbs.erase(it);
        }
        s_draft_source.clear();
        s_full_file = file; /* the same picture: no need to read it again */
        close_ask();
        wear(key); /* saves labels.txt with its place */
        label_note(tx("label.note.saved_as", {{"name", name_of(key)}}));
        std::printf("[hub] disc art added and saved: %s\n", file.c_str());
        run_after_leave();
        return;
    }
    if (s_ask == ASK_RENAME && is_picture_key(s_wear)) {
        const std::string old_key = s_wear, old_file = key_file(s_wear);
        if (!dl::rename(old_file, typed, file, why)) {
            label_note(why);
            return;
        }
        const std::string key = "picture:" + file;
        std::replace(s_order.begin(), s_order.end(), old_key, key);
        if (auto node = s_thumbs.extract(old_key)) {
            node.key() = key;
            s_thumbs.insert(std::move(node));
        }
        if (auto node = s_small.extract(old_file)) {
            node.key() = file;
            s_small.insert(std::move(node));
        }
        if (s_full_file == old_file) s_full_file = file;
        s_wear = key;
        close_ask();
        s_shelf_dirty = true;
        label_note(tx("label.note.renamed", {{"name", name_of(key)}}));
    }
}

void name_cancelled() {
    close_ask();
    s_after_leave = nullptr; /* a Save asked on the way out was cancelled: stay */
}

/* back to Settings › Appearance, where the page is reached from (snatcherLeo, 2026-11-13: "there should be a way to get back
 * to the settings menu", the ‹ Settings arrow and the gear); Back to Hub closes the page alone (close_labels). Both
 * ask first if something isn't saved. */
void back_to_settings() {
    guarded([] {
        close_labels();
        pc_settings_overlay_open_in_hub_disc_art();
    });
}

/* B / Esc on the disc art page: out of the All grid first, then back to the hub (*Changed 2026-11-15*, snatcherLeo's mockup
 * B: "users can go back to the Hub ... that's where they are more likely to go"; it went to Settings before; the gear
 * still opens Settings › Appearance) */
void labels_back() {
    if (s_labels_all) {
        show_all(false);
    } else {
        guarded(close_labels);
    }
}

/* left / right: the next label on the shelf (or, with the All grid open, the next picture), worn at once */
void step_label(int d) {
    const std::vector<std::string> dom = s_labels_all ? shelf_pictures() : shelf_visible();
    if (dom.empty()) return;
    const int n = (int)dom.size();
    const auto at = std::find(dom.begin(), dom.end(), s_wear);
    const int i = at == dom.end() ? (d > 0 ? 0 : n - 1) : ((int)(at - dom.begin()) + d + n) % n;
    const std::string key = dom[i];
    if (key != s_wear) guarded([key] { wear(key); });
}

void SDLCALL label_picked(void*, const char* const* files, int) {
    if (files != nullptr && files[0] != nullptr) {
        const std::lock_guard lock(s_pick_mutex);
        s_label_picked = files[0];
    }
}

void pick_picture() {
    /* every format this build reads (pc_disc_label.cpp: PNG and BMP, plus stb_image's and libwebp's) */
    static std::string pattern;
    if (pattern.empty()) {
        for (const std::string& ext : dl::dialog_extensions()) pattern += (pattern.empty() ? "" : ";") + ext;
    }
    /* the names live as long as the dialog may read them (static: the dialog can outlive this call) */
    static std::string pictures_name, all_name;
    pictures_name = tx("label.filter.pictures");
    all_name = tx("hub.add.filter.all");
    static SDL_DialogFileFilter filters[2];
    filters[0] = {pictures_name.c_str(), pattern.c_str()};
    filters[1] = {all_name.c_str(), "*"};
    int n = 0;
    SDL_Window* window = nullptr;
    if (SDL_Window** all = SDL_GetWindows(&n)) {
        window = n > 0 ? all[0] : nullptr;
        SDL_free(all);
    }
    SDL_ShowOpenFileDialog(label_picked, nullptr, window, filters, 2, nullptr, false);
}

/* a picture picked or dropped: a draft on the disc (nothing written until Save and its name) */
void start_draft(const std::string& path) {
    std::string why;
    dl::Rgba full;
    if (!dl::decode(path, full, why)) {
        label_note(why);
        return;
    }
    if (s_wear != kDraft) s_before_draft = s_wear;
    s_full = std::move(full);
    s_full_file.clear(); /* not in the folder yet */
    dl::shrink(s_full, 2 * s_edit_px, s_work);
    s_draft_source = path;
    const size_t cut = path.find_last_of("/\\");
    s_draft_name = dl::clean_name(path.substr(cut == std::string::npos ? 0 : cut + 1));
    const size_t dot = s_draft_name.find_last_of('.');
    if (dot != std::string::npos && dot > 0) s_draft_name.resize(dot);
    s_order.erase(std::remove(s_order.begin(), s_order.end(), std::string(kDraft)), s_order.end());
    s_order.insert(s_order.begin() + kBuiltIns, kDraft);
    thumb_stale(kDraft);
    if (s_labels_all) show_all(false);
    s_wear = kDraft;
    s_work_place = dl::Place{};
    forget_history();
    s_dirty = false;
    set_body_class("lpic", true);
    save_state();
    label_note(tx("label.note.place_it"));
    s_edit_dirty = s_shelf_dirty = true;
    std::printf("[hub] disc art draft: %s\n", path.c_str());
}

void add_picture(const std::string& path) {
    guarded([path] { start_draft(path); });
}

void remove_worn() {
    if (s_wear == kDraft) { /* the draft has no file: it is just let go */
        discard_unsaved();
        return;
    }
    if (!is_picture_key(s_wear)) return;
    const std::string key = s_wear, file = key_file(key);
    if (!dl::remove(file)) {
        label_note(tx("label.note.remove_failed"));
        return;
    }
    s_order.erase(std::remove(s_order.begin(), s_order.end(), key), s_order.end());
    if (auto it = s_thumbs.find(key); it != s_thumbs.end()) {
        if (!it->second.shown.empty()) Rml::ReleaseTexture(it->second.shown);
        s_thumbs.erase(it);
    }
    s_small.erase(file);
    if (s_full_file == file) s_full_file.clear();
    wear("orreport");
    label_note(tx("label.note.removed"));
}

void get_template() {
    std::string path, why;
    if (!dl::save_template(path, why)) {
        label_note(tx("label.note.template_failed", {{"error", why}}));
        return;
    }
    SDL_OpenURL(dl::folder().c_str()); /* no note: the folder opening says it (snatcherLeo, 2026-11-13: "we don't need to show") */
}

bool same_place(const dl::Place& a, const dl::Place& b) {
    return SDL_fabsf(a.zoom - b.zoom) <= 0.01f && SDL_fabsf(a.x - b.x) <= 0.01f && SDL_fabsf(a.y - b.y) <= 0.01f &&
           SDL_fabsf(a.rot - b.rot) <= 0.01f;
}

/* unsaved = the place differs from the saved one (a draft is always unsaved) */
void update_dirty() {
    if (s_wear != kDraft) {
        const dl::Entry* e = dl::find(key_file(s_wear));
        s_dirty = e == nullptr || !same_place(e->place, s_work_place);
    }
    save_state();
}

/* a place changed: the editor redraws now, the thumbnail once it rests (place_settled); written only on Save */
void place_changed(const dl::Place& p) {
    if (!is_editable(s_wear) || s_ask != ASK_NONE) return;
    if (!s_gesture_open) { /* a new gesture: where it started, for Ctrl+Z */
        s_gesture_start = s_work_place;
        s_gesture_open = true;
    }
    s_work_place = p;
    dl::clamp(s_work, s_work_place);
    s_edit_dirty = true;
    s_place_changed = SDL_GetTicks();
    update_dirty();
}

void place_settled() {
    s_place_changed = 0;
    if (s_gesture_open) {
        s_gesture_open = false;
        if (!same_place(s_gesture_start, s_work_place)) {
            s_undo.push_back(s_gesture_start);
            if (s_undo.size() > 200) s_undo.erase(s_undo.begin()); /* plenty; a long session can't grow it forever */
            s_redo.clear();
        }
    }
    thumb_stale(s_wear);
}

void forget_history() {
    s_undo.clear();
    s_redo.clear();
    s_gesture_open = false;
}

/* Ctrl+Z (back) / Ctrl+Y (forward): one gesture at a time */
void undo_place(bool redo) {
    if (!is_editable(s_wear) || s_ask != ASK_NONE || s_edit_drag || s_zoom_drag) return;
    if (s_gesture_open) place_settled(); /* a wheel burst still resting counts as done */
    std::vector<dl::Place>& from = redo ? s_redo : s_undo;
    std::vector<dl::Place>& to = redo ? s_undo : s_redo;
    if (from.empty()) return;
    to.push_back(s_work_place);
    s_work_place = from.back();
    from.pop_back();
    std::printf("[hub] disc art %s: zoom %.1f, offset %.1f, %.1f\n", redo ? "redo" : "undo", s_work_place.zoom,
                s_work_place.x, s_work_place.y);
    s_edit_dirty = true;
    update_dirty();
    thumb_stale(s_wear);
}

/* the page's mouse: drag on the label to move the picture, the wheel over it to zoom, the zoom bar */
bool inside(Rml::Element* e, const char* id) {
    for (; e != nullptr; e = e->GetParentNode()) {
        if (e->GetId() == id) return true;
    }
    return false;
}

void zoom_from_mouse(float mx) {
    Rml::Element* track = s_doc->GetElementById("lzoom");
    if (track == nullptr || !is_picture_key(s_wear)) return;
    const float left = track->GetAbsoluteOffset(Rml::BoxArea::Border).x;
    const float width = SDL_max(1.0f, track->GetBox().GetSize(Rml::BoxArea::Border).x);
    dl::Place p = place_of(s_wear);
    p.zoom = SDL_clamp((mx - left) / width * 200.0f - 100.0f, -100.0f, 100.0f);
    place_changed(p);
}

/* the picture's zoom stepped by the − / + buttons, turned by a quarter (↺ ↻, LB / RB), or fitted whole inside the disc
 * (snatcherLeo's mockup A, 2026-11-15); each is one undo step */
void zoom_step(float d) {
    if (!is_editable(s_wear)) return;
    dl::Place p = place_of(s_wear);
    p.zoom = SDL_clamp(p.zoom + d, -100.0f, 100.0f);
    place_changed(p);
    place_settled();
}
void turn_by(float degrees) {
    if (!is_editable(s_wear)) return;
    dl::Place p = place_of(s_wear);
    /* quarter turns land on the nearest quarter (a picture turned a little by Shift + wheel first snaps straight) */
    p.rot = SDL_fabsf(degrees) == 90.0f ? SDL_roundf((p.rot + degrees) / 90.0f) * 90.0f : p.rot + degrees;
    if (p.rot > 180.0f) p.rot -= 360.0f;
    if (p.rot <= -180.0f) p.rot += 360.0f;
    place_changed(p);
    place_settled();
}
/* "Fit whole picture": centred, at the zoom where all of it lies inside the disc at any turn (its turn kept) */
void fit_whole() {
    if (!is_editable(s_wear) || s_full.px.empty()) return;
    dl::Place p = place_of(s_wear);
    p.zoom = dl::fit_zoom(s_full);
    p.x = p.y = 0.0f;
    place_changed(p);
    place_settled();
}

class LabelMouse : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (!s_labels || s_ask != ASK_NONE) return; /* the name box or the question is up: only its buttons */
        const float x = event.GetParameter<float>("mouse_x", 0.0f), y = event.GetParameter<float>("mouse_y", 0.0f);
        switch (event.GetId()) {
        case Rml::EventId::Mousedown:
            if (is_editable(s_wear) && !s_labels_all && inside(event.GetTargetElement(), "leditbox")) {
                s_edit_drag = true;
                s_drag_mx = x;
                s_drag_my = y;
            } else if (inside(event.GetTargetElement(), "lzoom")) {
                s_zoom_drag = true;
                set_body_class("ldragging", true);
                zoom_from_mouse(x);
            }
            break;
        case Rml::EventId::Mousemove:
            if (s_edit_drag) {
                Rml::Element* box = s_doc->GetElementById("leditbox");
                const float units = 420.0f / SDL_max(1.0f, box != nullptr ? box->GetBox().GetSize().x : 400.0f);
                dl::Place p = place_of(s_wear);
                p.x += (x - s_drag_mx) * units;
                p.y += (y - s_drag_my) * units;
                s_drag_mx = x;
                s_drag_my = y;
                place_changed(p);
            } else if (s_zoom_drag) {
                zoom_from_mouse(x);
            }
            break;
        case Rml::EventId::Mouseup:
            if (s_edit_drag || s_zoom_drag) {
                s_edit_drag = s_zoom_drag = false;
                set_body_class("ldragging", false);
                place_settled();
            }
            break;
        case Rml::EventId::Mousescroll:
            if (is_editable(s_wear) && !s_labels_all && inside(event.GetTargetElement(), "leditbox")) {
                dl::Place p = place_of(s_wear);
                /* Shift + wheel turns, 5° a notch (down: clockwise); the wheel alone zooms (down: smaller) */
                if (SDL_GetModState() & SDL_KMOD_SHIFT) p.rot += event.GetParameter<float>("wheel_delta_y", 0.0f) * 5.0f;
                else p.zoom -= event.GetParameter<float>("wheel_delta_y", 0.0f) * 5.0f;
                place_changed(p);
                event.StopPropagation();
            }
            break;
        default:
            break;
        }
    }
};
LabelMouse s_label_mouse;

/* a click on the page; true if it was the page's */
bool label_click(Rml::Element* target) {
    for (Rml::Element* e = target; e != nullptr; e = e->GetParentNode()) {
        const std::string id = e->GetId();
        /* the name box and the leave question are on top: only their own buttons answer */
        if (id == "ln-ok") { name_confirmed(); return true; }
        if (id == "ln-cancel") { name_cancelled(); return true; }
        if (id == "lv-save") { save_pressed(); return true; }
        if (id == "lv-discard") {
            close_ask();
            discard_unsaved();
            run_after_leave();
            return true;
        }
        if (id == "lv-cancel") {
            close_ask();
            s_after_leave = nullptr;
            return true;
        }
        if (s_ask != ASK_NONE) continue;
        if (id.rfind("lk-", 0) == 0) {
            const size_t i = (size_t)SDL_atoi(id.c_str() + 3);
            if (i < s_order.size() && s_order[i] != s_wear) {
                const std::string key = s_order[i];
                guarded([key] {
                    wear(key);
                    if (s_labels_all) show_all(false); /* picked from the grid: back to the shelf, the pick on it */
                });
            } else if (s_labels_all) {
                show_all(false);
            }
            return true;
        }
        if (id == "l-save") { save_pressed(); return true; }
        if (id == "l-rename") {
            if (is_picture_key(s_wear)) open_name_box(ASK_RENAME);
            return true;
        }
        if (id == "l-add") { pick_picture(); return true; }
        if (id == "l-all") { show_all(true); return true; }
        if (id == "l-allback") { show_all(false); return true; }
        if (id == "l-reset") {
            if (is_editable(s_wear)) {
                place_changed(dl::Place{});
                place_settled();
            }
            return true;
        }
        if (id == "l-remove") { remove_worn(); return true; }
        if (id == "l-zoomout") { zoom_step(-10.0f); return true; }
        if (id == "l-zoomin") { zoom_step(10.0f); return true; }
        if (id == "l-turnl") { turn_by(-90.0f); return true; }
        if (id == "l-turnr") { turn_by(90.0f); return true; }
        if (id == "l-fit") { fit_whole(); return true; }
        if (id == "l-template") { get_template(); return true; }
        if (id == "l-folder") {
            SDL_CreateDirectory(dl::folder().c_str());
            SDL_OpenURL(dl::folder().c_str());
            return true;
        }
        if (id == "ldone") { guarded(close_labels); return true; }       /* Back to Hub */
        if (id == "gear") { back_to_settings(); return true; } /* the gear: Settings › Appearance, where it came from */
    }
    return false;
}

/* the page's keys and buttons: left / right choose the next label (worn at once), the stick moves the picture, the
 * triggers (or the right stick) zoom, A saves (only when something is unsaved; the D-pad's choice needs no confirming),
 * X adds a picture, Y the All grid, B back (the grid, then Settings). With the name box up: typing goes into it, Enter
 * or A saves the name, Esc or B cancels; with the leave question: A Save, X Don't save, B Cancel. */
/* the disc art page's top row for the controller (snatcherLeo, 2026-11-15: "If a user ... clicks up/d-pad up, they can get to
 * back to [Back to Hub], and escape out. If they go to the right from "back to hub" they can get to the settings gear"):
 * s_ltop 0 = the page itself (the shelf and the editor), 1 = Back to Hub, 2 = the gear; lit with the buttons' own
 * controller look (.btn.kfocus, #gear.kfocus). A presses the lit one as a click would; Down goes back to the page. The
 * left stick keeps moving the picture, so only the d-pad and the arrow keys reach the row */
void ltop_set(int v) {
    s_ltop = v;
    if (Rml::Element* e = s_doc->GetElementById("ldone")) e->SetClass("kfocus", v == 1);
    if (Rml::Element* e = s_doc->GetElementById("gear")) e->SetClass("kfocus", v == 2);
}
/* a d-pad / arrow move or A while the top row has the controller; true: taken */
bool ltop_event(int dx, int dy, bool press) {
    if (s_ltop == 0) {
        if (dy < 0) ltop_set(1);
        return dy < 0;
    }
    if (dy > 0) ltop_set(0);
    else if (dx != 0) ltop_set(dx > 0 ? 2 : 1);
    else if (press) {
        Rml::Element* e = s_doc->GetElementById(s_ltop == 1 ? "ldone" : "gear");
        ltop_set(0);
        if (e != nullptr) e->Click(); /* Back to Hub (it asks first if there is unsaved art), or the gear */
    }
    return true;
}

input::EventResult labels_event(const input::InputEvent& event) {
    using E = input::InputEvent;
    if (s_ask == ASK_NAME_NEW || s_ask == ASK_RENAME) {
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat && (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER)) {
                name_confirmed();
                return input::EventResult::Consume;
            }
            if (key->pressed && !key->repeat && key->keycode == SDLK_ESCAPE) {
                name_cancelled();
                return input::EventResult::Consume;
            }
        } else if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_SOUTH) name_confirmed();
            if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_EAST) name_cancelled();
            return input::EventResult::Consume;
        } else if (event.payload.get_if<E::AxisChanged>() != nullptr) {
            return input::EventResult::Consume;
        }
        aurora::rmlui::process_input(event); /* typing and the mouse */
        return input::EventResult::Consume;
    }
    if (s_ask == ASK_LEAVE) {
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat && (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER)) {
                save_pressed();
            } else if (key->pressed && !key->repeat && key->keycode == SDLK_ESCAPE) {
                close_ask();
                s_after_leave = nullptr;
            }
            return input::EventResult::Consume;
        }
        if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_SOUTH) {
                save_pressed();
            } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_WEST) {
                close_ask();
                discard_unsaved();
                run_after_leave();
            } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_EAST) {
                close_ask();
                s_after_leave = nullptr;
            }
            return input::EventResult::Consume;
        }
        if (event.payload.get_if<E::AxisChanged>() != nullptr) return input::EventResult::Consume;
        aurora::rmlui::process_input(event); /* the mouse on its buttons */
        return input::EventResult::Consume;
    }
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        if (key->pressed && !key->repeat && (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER) &&
            (s_wear == kDraft || s_dirty)) {
            save_pressed();
            return input::EventResult::Consume;
        }
        /* Ctrl+Z undo, Ctrl+Y / Ctrl+Shift+Z redo (held keys repeat, as in any program). Ctrl is Windows' and Linux's
         * key for this; a Mac build would use Cmd (SDL_KMOD_GUI) instead (likely) */
        const bool ctrl = (key->modifiers & SDL_KMOD_CTRL) != 0, shift = (key->modifiers & SDL_KMOD_SHIFT) != 0;
        if (key->pressed && ctrl && key->keycode == SDLK_Z) {
            undo_place(shift);
            return input::EventResult::Consume;
        }
        if (key->pressed && ctrl && key->keycode == SDLK_Y) {
            undo_place(true);
            return input::EventResult::Consume;
        }
        if (key->pressed && !key->repeat) {
            const int kx = key->keycode == SDLK_LEFT ? -1 : key->keycode == SDLK_RIGHT ? 1 : 0;
            const int ky = key->keycode == SDLK_UP ? -1 : key->keycode == SDLK_DOWN ? 1 : 0;
            const bool kpress = s_ltop != 0 && (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER ||
                                                key->keycode == SDLK_SPACE);
            if (key->keycode == SDLK_ESCAPE) labels_back();
            else if ((kx != 0 || ky != 0 || kpress) && ltop_event(kx, ky, kpress)) {
            } else if (key->keycode == SDLK_LEFT) step_label(-1);
            else if (key->keycode == SDLK_RIGHT) step_label(1);
        }
        return input::EventResult::Consume;
    }
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        if (button->pressed) {
            const auto bt = button->button;
            const int bx = bt == SDL_GAMEPAD_BUTTON_DPAD_LEFT ? -1 : bt == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ? 1 : 0;
            const int by = bt == SDL_GAMEPAD_BUTTON_DPAD_UP ? -1 : bt == SDL_GAMEPAD_BUTTON_DPAD_DOWN ? 1 : 0;
            const bool bpress = s_ltop != 0 && bt == SDL_GAMEPAD_BUTTON_SOUTH;
            if ((bx != 0 || by != 0 || bpress) && ltop_event(bx, by, bpress)) return input::EventResult::Consume;
            if (s_ltop != 0 && bt != SDL_GAMEPAD_BUTTON_EAST) return input::EventResult::Consume; /* the row only */
            switch (button->button) {
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT: step_label(-1); break;
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: step_label(1); break;
            case SDL_GAMEPAD_BUTTON_SOUTH: if (s_wear == kDraft || s_dirty) save_pressed(); break;
            case SDL_GAMEPAD_BUTTON_EAST: labels_back(); break;
            case SDL_GAMEPAD_BUTTON_WEST: pick_picture(); break;
            case SDL_GAMEPAD_BUTTON_NORTH: /* All: only once the All tile shows (over kShelfPictures of the player's own
                                              pictures; snatcherLeo, 2026-11-15: "it should be blocked, unless users have more") */
                if (s_labels_all || (int)shelf_pictures().size() > kShelfPictures) show_all(!s_labels_all);
                break;
            /* LB / RB: a quarter turn of the worn picture (snatcherLeo's mockup A, 2026-11-15) */
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: turn_by(-90.0f); break;
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: turn_by(90.0f); break;
            default: break;
            }
        }
        return input::EventResult::Consume;
    }
    if (const auto* axis = event.payload.get_if<E::AxisChanged>()) {
        switch (axis->axis) {
        case SDL_GAMEPAD_AXIS_LEFTX: s_ax = axis->value; break;
        case SDL_GAMEPAD_AXIS_LEFTY: s_ay = axis->value; break;
        case SDL_GAMEPAD_AXIS_RIGHTY: s_arz = axis->value; break;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER: s_alt = axis->value; break;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: s_art = axis->value; break;
        default: break;
        }
        return input::EventResult::Consume;
    }
    aurora::rmlui::process_input(event); /* the mouse */
    return input::EventResult::Consume;
}

/* the page's work each frame: a picked or dropped picture, thumbnails read by the worker, the controller's sticks,
 * then the editor and the shelf redrawn if anything changed */
void labels_frame() {
    std::string picked;
    {
        const std::lock_guard lock(s_pick_mutex);
        picked.swap(s_label_picked);
    }
    if (!picked.empty() && s_labels) add_picture(picked);
    {
        const std::lock_guard lock(s_small_mutex);
        for (auto& [file, small] : s_small_done) {
            s_small[file] = std::move(small);
            thumb_stale("picture:" + file);
        }
        s_small_done.clear();
    }
    if (!s_labels) return;
    static Uint64 last = SDL_GetTicks();
    const Uint64 now = SDL_GetTicks();
    const float dt = SDL_min(0.05f, (now - last) / 1000.0f);
    last = now;
    auto dz = [](float v) { return SDL_fabsf(v) < 0.2f ? 0.0f : v; }; /* the sticks' dead zone */
    const float mx = dz(s_ax), my = dz(s_ay), zoom = dz(s_art) - dz(s_alt) - dz(s_arz);
    if (s_name_focus > 0 && --s_name_focus == 0) {
        if (auto* in = dynamic_cast<Rml::ElementFormControlInput*>(s_doc->GetElementById("lnamein"))) {
            in->Focus();
            in->Select(); /* typing replaces the offered name */
        }
    }
    if (is_editable(s_wear) && !s_labels_all && s_ask == ASK_NONE && (mx != 0.0f || my != 0.0f || zoom != 0.0f)) {
        dl::Place p = place_of(s_wear);
        p.x += mx * 220.0f * dt; /* label units a second at full tilt: across the disc in about two seconds */
        p.y += my * 220.0f * dt;
        p.zoom += zoom * 60.0f * dt;
        place_changed(p);
    }
    if (s_place_changed != 0 && !s_edit_drag && !s_zoom_drag && now - s_place_changed > 400) {
        place_settled();
    }
    if (s_edit_dirty) edit_refresh();
    if (s_shelf_dirty) build_shelf();
}

/* ---- the page ---- */

/* the mouse held on PLAY lifts the disc; let go on PLAY, the click plays it (Clicks); let go elsewhere, it settles */
class Presses : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        if (s_labels) {
            return;
        }
        if (event.GetId() == Rml::EventId::Mousedown && hub_saves_is_open()) {
            hub_saves_name_press(); /* the Rename box: a second click selects all (pc_hub_saves.cpp) */
        }
        if (event.GetId() == Rml::EventId::Mouseup) {
            if (s_hitch && s_hitch_mouse) {
                bool on_play = false;
                for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
                    on_play = on_play || e->GetId() == "play";
                }
                if (!on_play) unhitch();
            }
            return;
        }
        for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) { /* Mousedown */
            if (e->GetId() == "play") {
                hitch(true);
                return;
            }
        }
    }
};
Presses s_presses;

class Clicks : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        /* the gear first, on every page (*Fixed 2026-11-15, snatcherLeo: "in this screen, if I click the gear settings icon, I
         * can't get to settings":* the Saves page and the disc art page took the click before the gear's check) */
        for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
            if (e->GetId() == "gear") {
                /* from the disc art page: Settings › Appearance, the page it opens from (with its unsaved-changes
                 * question); elsewhere Settings' first page */
                if (s_labels) back_to_settings();
                else pc_settings_overlay_open_in_hub();
                return;
            }
        }
        if (s_labels) {
            label_click(event.GetTargetElement());
            return;
        }
        if (hub_saves_is_open()) { /* the Saves page (pc_hub_saves.cpp) */
            hub_saves_click(event.GetTargetElement());
            return;
        }
        if (hub_lang_click(event.GetTargetElement())) { /* the Language discs link and panel */
            return;
        }
        for (Rml::Element* e = event.GetTargetElement(); e != nullptr; e = e->GetParentNode()) {
            const std::string id = e->GetId();
            if (id == "b-label") { /* the pencil on the disc: the disc label page */
                open_labels();
                return;
            }
            if (id == "play") {
                play(); /* the click comes with the mouse's release on it: the held lift (Presses) becomes the spin */
                return;
            }
            if (id == "b-browse") {
                browse();
                return;
            }
            if (id == "b-copy" || id == "b-move") {
                accept_offer(id == "b-move");
                return;
            }
            if (id == "b-cancel") {
                cancel_offer();
                return;
            }
            if (id == "b-folder") {
                open_rom_folder();
                return;
            }
            if (id == "b-report" || id == "b-report2") {
                report();
                return;
            }
            if (id == "cb-report") { /* the "closed unexpectedly" bar: the report, with the earlier run's log (5.1) */
                crash_bar_show(false);
                report();
                return;
            }
            if (id == "cb-dismiss") {
                std::printf("[hub] closed-unexpectedly bar: Not now\n");
                pc_log_previous_unclean_clear();
                crash_bar_show(false);
                return;
            }
            if (id == "rsave") {
                save_report();
                return;
            }
            if (id == "rcancel") {
                close_report("Cancel");
                return;
            }
            if (id == "rdone") {
                close_report("Done");
                return;
            }
            if (id == "updnote") {
                upd_open();
                return;
            }
            if (id == "ugo") {
                upd_go();
                return;
            }
            if (id == "ulater") {
                upd_close("Not now");
                return;
            }
            if (id == "ropen") {
                report_open_folder();
                return;
            }
            if (id == "b-xd") {
                set_note(tx("hub.note.xd"));
                return;
            }
            if (id == "b-studio") {
                /* Open Studio (Studio's design, snatcherLeo approved 2026-11-23; snatcherLeo: "the button ... should actually open
                 * Studio"): brought forward when it's open, else started from the OrrePort folder's own "OrrePort
                 * Studio.exe" launcher (pc_apps.c says why the launcher). A development build has no launcher: the note
                 * says Studio isn't there */
                if (pc_apps_running(PC_APP_STUDIO, apps_root())) {
                    std::printf("[hub] Open Studio: it's open, shown (%d)\n", pc_apps_show(PC_APP_STUDIO, apps_root()));
                } else {
                    char root[1300];
                    std::snprintf(root, sizeof root, "%s", apps_root());
                    const int ok = pc_apps_start(PC_APP_STUDIO, root);
                    std::printf("[hub] Open Studio: %s (%s)\n", ok ? "started" : "not started, no launcher here", root);
                    if (!ok) set_note(tx("hub.note.studio"));
                }
                return;
            }
            if (id == "gear") { /* the same Settings panel as in game, over the hub (snatcherLeo, 2026-11-11, in-window) */
                pc_settings_overlay_open_in_hub();
                return;
            }
            if (id == "cur") { /* the save card, anywhere on it (Y too): the Saves page (step 2.1; snatcherLeo's mockup A) */
                hub_saves_open();
                return;
            }
        }
    }
};
Clicks s_clicks;

/* ---- the controller's place on the hub (snatcherLeo, 2026-11-15: "if a user is using a gamepad, if they are using D-pad or
 * control stick in this screen, we should have the highlight on stuff, for PLAY, CURRENT SAVE - just to make
 * navigation easy. And, the highlight around "PLAY" should be smooth, not choppy") ----
 * Rows, top to bottom: PLAY, the current save's card (A: Change save...), the links (Left / Right among them; A as a
 * click), then the Language discs panel when open (pc_hub_langdiscs.cpp keeps its own place there). The D-pad, the
 * left stick and the arrow keys move; A / Enter acts. The place shows as the ring PLAY had (now on whichever of PLAY and
 * the card has it) or the links' orange underline. The ring is drawn here as a picture at its exact pixel size (a
 * rounded rectangle measured by distance, edge pixels partly covered), because RmlUi draws rounded borders without
 * smoothing (the choppy ring snatcherLeo saw). All OSes the same. */
/* kNavGear: the Settings gear at the top right, Up from PLAY (snatcherLeo, 2026-11-15: "if I scroll up from PLAY, it should get
 * me to highlight "Settings gear icon" so I can click it later, on gamepad"); A opens Settings, Down returns to PLAY */
/* kNavPencil: the disc's pencil (the disc art page's way in), Left from PLAY, the save card, the gear or the first link
 * (snatcherLeo, 2026-11-15: "if users use d-pad/control stick flicked to the left, they can get to pencil icon, and get to disc
 * art area"); A opens the disc art page, Right goes back to where the controller came from (s_navBefore) */
enum { kNavGear = -1, kNavPlay = 0, kNavCard = 1, kNavLinks = 2, kNavPencil = 3 };
int s_nav = kNavPlay;
int s_navBefore = kNavPlay;
int s_link = 0;
/* the links in their row's order (2026-11-15: Saves left the row, the card opens it; Report a problem last) */
const char* const kLinks[] = {"b-studio", "b-lang", "b-report"};
constexpr int kLinkCount = 3;
constexpr int kLangLink = 1; /* the Language discs link: its mark and panel are pc_hub_langdiscs.cpp's */
float s_stick_y = 0.f, s_stick_x = 0.f; /* the left stick's last values: a move on crossing half way */

/* hubring://<w>x<h>x<radius>x<stroke>x<rrggbb>: the ring as a picture (px; radius and stroke in hundredths);
 * hubfill://<w>x<h>x<radius>x0x<rrggbb>: a filled rounded rectangle the same way (PLAY's face; snatcherLeo, same day: "smooth
 * out the black outline on the PLAY menu thing": its rounded fill was RmlUi's unsmoothed one too) */
std::map<std::string, Picture> s_rings;
const Picture* ring_picture(std::string_view source) {
    const std::string key(source);
    if (auto it = s_rings.find(key); it != s_rings.end()) return &it->second;
    int w = 0, h = 0, r100 = 0, s100 = 0;
    unsigned rgb = 0;
    const size_t at = key.find("://");
    if (at == std::string::npos ||
        std::sscanf(key.c_str() + at + 3, "%dx%dx%dx%dx%x", &w, &h, &r100, &s100, &rgb) != 5 || w <= 0 || h <= 0 ||
        w > 8192 || h > 8192)
        return nullptr;
    Picture p;
    p.w = w, p.h = h;
    p.px.assign((size_t)w * h * 4, 0);
    const bool fill = key.compare(0, 7, "hubfill") == 0;
    const float r = r100 / 100.f, s = fill ? 1e9f : s100 / 100.f, bx = w / 2.f, by = h / 2.f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            /* the signed distance to the rounded rectangle's outer edge (negative inside) */
            const float qx = std::fabs(x + 0.5f - bx) - bx + r, qy = std::fabs(y + 0.5f - by) - by + r;
            const float d = std::sqrt(std::max(qx, 0.f) * std::max(qx, 0.f) + std::max(qy, 0.f) * std::max(qy, 0.f)) +
                            std::min(std::max(qx, qy), 0.f) - r;
            const float a = std::clamp(0.5f - d, 0.f, 1.f) - std::clamp(0.5f - (d + s), 0.f, 1.f);
            if (a <= 0.f) continue;
            unsigned char* o = &p.px[((size_t)y * w + x) * 4];
            o[0] = (unsigned char)(rgb >> 16), o[1] = (unsigned char)(rgb >> 8), o[2] = (unsigned char)rgb;
            o[3] = (unsigned char)std::lround(a * 255.f);
        }
    return &s_rings.emplace(key, std::move(p)).first->second;
}

/* hubbox://<w>x<h>x<radius>x<line>x<fill rrggbb>x<line rrggbb>: a rounded box with a thin line round it, smooth (the
 * save card, Change save..., the A / Y pills, the format tags; snatcherLeo: "is there any jaggedness here? Get rid of, in
 * case"): the fill covers the whole shape, the line its outer `line` px, both partly covering edge pixels */
const Picture* box_picture(std::string_view source) {
    const std::string key(source);
    if (auto it = s_rings.find(key); it != s_rings.end()) return &it->second;
    int w = 0, h = 0, r100 = 0, l100 = 0;
    unsigned fill = 0, line = 0;
    const size_t at = key.find("://");
    if (at == std::string::npos ||
        std::sscanf(key.c_str() + at + 3, "%dx%dx%dx%dx%xx%x", &w, &h, &r100, &l100, &fill, &line) != 6 || w <= 0 ||
        h <= 0 || w > 8192 || h > 8192)
        return nullptr;
    Picture p;
    p.w = w, p.h = h;
    p.px.assign((size_t)w * h * 4, 0);
    const float r = r100 / 100.f, lw = l100 / 100.f, bx = w / 2.f, by = h / 2.f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float qx = std::fabs(x + 0.5f - bx) - bx + r, qy = std::fabs(y + 0.5f - by) - by + r;
            const float d = std::sqrt(std::max(qx, 0.f) * std::max(qx, 0.f) + std::max(qy, 0.f) * std::max(qy, 0.f)) +
                            std::min(std::max(qx, qy), 0.f) - r;
            const float all = std::clamp(0.5f - d, 0.f, 1.f);
            if (all <= 0.f) continue;
            const float inner = lw > 0.f ? std::clamp(0.5f - (d + lw), 0.f, 1.f) : all, ring = all - inner;
            unsigned char* o = &p.px[((size_t)y * w + x) * 4];
            for (int c = 0; c < 3; ++c) {
                const int sh = 16 - 8 * c;
                const float v = (((line >> sh) & 0xFF) * ring + ((fill >> sh) & 0xFF) * inner) / all;
                o[c] = (unsigned char)std::lround(std::clamp(v, 0.f, 255.f));
            }
            o[3] = (unsigned char)std::lround(all * 255.f);
        }
    return &s_rings.emplace(key, std::move(p)).first->second;
}

/* one element drawn as a smooth box over its whole border box (its own background and border are see-through in
 * hub.rcss; its border's width still sets its layout) */
void smooth_box(Rml::Element* e, float dp, float radius, float line, unsigned fill, unsigned lineColour) {
    if (e == nullptr) return;
    const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
    const int w = (int)std::lround(size.x), h = (int)std::lround(size.y);
    if (w <= 0 || h <= 0) return;
    char src[200];
    std::snprintf(src, sizeof(src), "image(hubbox://%dx%dx%dx%dx%06xx%06x) border-box", w, h,
                  (int)std::lround(radius * dp * 100.f), (int)std::lround(line * dp * 100.f), fill, lineColour);
    if (e->GetAttribute<Rml::String>("data-ring", "") != src) {
        e->SetAttribute("data-ring", Rml::String(src));
        e->SetProperty("decorator", src);
    }
}

/* the hub's rounded boxes, smooth, in the colours hub.rcss had for them (dark / light, plain / hover / pressed) */
void update_boxes(float dp, bool light) {
    /* the card is the Saves page's button (2026-11-15, snatcherLeo's mockup A): a little lighter under the mouse, a little
     * darker pressed */
    if (Rml::Element* c = s_doc->GetElementById("cur")) {
        const bool down = c->IsPseudoClassSet("active"), over = c->IsPseudoClassSet("hover");
        c->SetClass("navon", s_nav == kNavCard);
        const unsigned fill = light ? (down ? 0xEEF1EDu : over ? 0xF6F8F5u : 0xFFFFFFu)
                                    : (down ? 0x141816u : over ? 0x1B201Du : 0x161A18u);
        const unsigned line = light ? (over || down ? 0xB8C2B6u : 0xD5DCD4u) : (over || down ? 0x3A433Eu : 0x262B28u);
        smooth_box(c, dp, 15.f, 1.f, fill, line);
    }
    smooth_box(s_doc->GetElementById("playa"), dp, 16.5f, 0.f, 0x2E7D3Au, 0x2E7D3Au);
    smooth_box(s_doc->GetElementById("cury"), dp, 16.5f, 0.f, 0xB8912Cu, 0xB8912Cu);
    smooth_box(s_doc->GetElementById("curtag-gci"), dp, 5.f, 0.f, 0x6A5FCBu, 0x6A5FCBu);
    smooth_box(s_doc->GetElementById("curtag-orre"), dp, 5.f, 0.f, 0xC8452Eu, 0xC8452Eu);
    /* the Accurate badge (2.12): deep gold since 2026-11-21, the in-game badge's colour (hub.rcss .ftag.acc says why) */
    smooth_box(s_doc->GetElementById("curtag-acc"), dp, 5.f, 0.f, 0xAD8000u, 0xAD8000u);
    smooth_box(s_doc->GetElementById("curtag-auto"), dp, 5.f, 0.f, 0x23777Au, 0x23777Au); /* the Autosave tag (2026-11-21) */
}

/* each hub frame: the ring drawn round PLAY or the card, at their current size (a new picture when the size changes) */
void update_rings() {
    if (s_doc == nullptr) return;
    const float dp = aurora::rmlui::get_context()->GetDensityIndependentPixelRatio();
    const bool light = s_doc->GetElementById("body") != nullptr && s_doc->GetElementById("body")->IsClassSet("light");
    const unsigned colour = light ? 0x7E9B78u : 0xBACFB5u;
    struct { const char* id; bool on; float radius; } rings[] = {{"playring", s_nav == kNavPlay, 20.f},
                                                                  {"curring", s_nav == kNavCard, 21.3f}};
    for (const auto& rg : rings) {
        Rml::Element* e = s_doc->GetElementById(rg.id);
        if (e == nullptr) continue;
        e->SetClass("on", rg.on);
        const Rml::Vector2f size = e->GetBox().GetSize(Rml::BoxArea::Border);
        const int w = (int)std::lround(size.x), h = (int)std::lround(size.y);
        if (w <= 0 || h <= 0) continue;
        char src[160];
        std::snprintf(src, sizeof(src), "hubring://%dx%dx%dx%dx%06x", w, h, (int)std::lround(rg.radius * dp * 100.f),
                      (int)std::lround(2.5f * dp * 100.f), colour);
        const std::string want = std::string("image(") + src + ")";
        if (e->GetAttribute<Rml::String>("data-ring", "") != want) {
            e->SetAttribute("data-ring", want);
            e->SetProperty("decorator", want);
        }
    }
    update_boxes(dp, light);
    /* PLAY's face: its padding box (inside the 3.8 dp gap, which shows the page), radius 17.5 - 3.8 dp, in the colour
     * of its state (hub.rcss had them as background colours: plain, hover, pressed) */
    if (Rml::Element* play = s_doc->GetElementById("play")) {
        const Rml::Vector2f size = play->GetBox().GetSize(Rml::BoxArea::Padding);
        const int w = (int)std::lround(size.x), h = (int)std::lround(size.y);
        /* pressed: the mouse held on it (:active) or the held A / Enter press and the Play that follows it (body
         * "hitched", hitch(): a key press never sets :active) */
        Rml::Element* body = s_doc->GetElementById("body");
        const bool held = play->IsPseudoClassSet("active") || (body != nullptr && body->IsClassSet("hitched"));
        const unsigned face = held ? 0x9AB494u
                              : play->IsPseudoClassSet("hover") ? 0xB7CFB1u
                                                                : 0xA9C3A3u;
        if (w > 0 && h > 0) {
            char src[160];
            std::snprintf(src, sizeof(src), "image(hubfill://%dx%dx%dx0x%06x)", w, h,
                          (int)std::lround(13.7f * dp * 100.f), face);
            if (play->GetAttribute<Rml::String>("data-ring", "") != src) {
                play->SetAttribute("data-ring", Rml::String(src));
                play->SetProperty("decorator", src);
            }
        }
    }
}

void set_nav(int nav, int link) {
    s_nav = nav;
    s_link = std::clamp(link, 0, kLinkCount - 1);
    for (int i = 0; i < kLinkCount; ++i) /* the Language discs link's mark is that module's own */
        if (i != kLangLink)
            if (Rml::Element* e = s_doc->GetElementById(kLinks[i])) e->SetClass("kfocus", nav == kNavLinks && s_link == i);
    hub_lang_set_focus(nav == kNavLinks && s_link == kLangLink ? 0 : -1);
    if (Rml::Element* g = s_doc->GetElementById("gear")) g->SetClass("kfocus", nav == kNavGear);
    if (Rml::Element* p = s_doc->GetElementById("b-label")) p->SetClass("kfocus", nav == kNavPencil); /* hub.rcss */
}

bool link_shown(int i) {
    Rml::Element* e = s_doc->GetElementById(kLinks[i]);
    return e != nullptr && e->IsVisible(true);
}

/* the controller's moves and A on the card and the links; false: not taken (PLAY's A is on_event's own) */
bool nav_event(const input::InputEvent& event) {
    using E = input::InputEvent;
    int dx = 0, dy = 0, press = 0;
    if (const auto* b = event.payload.get_if<E::ButtonChanged>()) {
        if (!b->pressed) return s_nav != kNavPlay && b->button == SDL_GAMEPAD_BUTTON_SOUTH; /* A's release too */
        if (b->button == SDL_GAMEPAD_BUTTON_DPAD_UP) dy = -1;
        else if (b->button == SDL_GAMEPAD_BUTTON_DPAD_DOWN) dy = 1;
        else if (b->button == SDL_GAMEPAD_BUTTON_DPAD_LEFT) dx = -1;
        else if (b->button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) dx = 1;
        else if (b->button == SDL_GAMEPAD_BUTTON_SOUTH) press = 1;
        else return false;
    } else if (const auto* a = event.payload.get_if<E::AxisChanged>()) {
        if (a->axis != SDL_GAMEPAD_AXIS_LEFTX && a->axis != SDL_GAMEPAD_AXIS_LEFTY) return false;
        float& last = a->axis == SDL_GAMEPAD_AXIS_LEFTY ? s_stick_y : s_stick_x;
        const int was = last > 0.5f ? 1 : last < -0.5f ? -1 : 0, now = a->value > 0.5f ? 1 : a->value < -0.5f ? -1 : 0;
        last = a->value;
        if (now == 0 || now == was) return true;
        (a->axis == SDL_GAMEPAD_AXIS_LEFTY ? dy : dx) = now;
    } else if (const auto* k = event.payload.get_if<E::KeyChanged>()) {
        if (!k->pressed || k->repeat) return false;
        if (k->keycode == SDLK_UP) dy = -1;
        else if (k->keycode == SDLK_DOWN) dy = 1;
        else if (k->keycode == SDLK_LEFT) dx = -1;
        else if (k->keycode == SDLK_RIGHT) dx = 1;
        else if (s_nav != kNavPlay &&
                 (k->keycode == SDLK_RETURN || k->keycode == SDLK_KP_ENTER || k->keycode == SDLK_SPACE)) press = 1;
        else return false;
    } else {
        return false;
    }
    /* inside the Language discs panel: its own places, until Up leaves its first button for the link */
    if (hub_lang_focus() >= 1) {
        if (dy < 0 && hub_lang_focus() == 1) {
            set_nav(kNavLinks, kLangLink);
            return true;
        }
        if (dx != 0) return true;
        if (dy != 0) {
            hub_lang_set_focus(hub_lang_focus() + dy);
            return true;
        }
        return false; /* A and B: the panel's own (hub_lang_event) */
    }
    /* the disc's pencil: Right goes back where the controller came from; Up and Down stay on it; A below */
    if (s_nav == kNavPencil) {
        if (dx > 0) set_nav(s_navBefore, s_link);
        if (dx != 0 || dy != 0) return true;
    } else if (dx < 0 && !s_no_disc && (s_nav != kNavLinks || s_link == 0)) {
        s_navBefore = s_nav;
        set_nav(kNavPencil, s_link);
        return true;
    }
    if (dy < 0) {
        if (s_nav == kNavLinks) set_nav(kNavCard, s_link);
        else if (s_nav == kNavCard) set_nav(kNavPlay, s_link);
        else if (s_nav == kNavPlay) set_nav(kNavGear, s_link);
        return true;
    }
    if (dy > 0) {
        if (s_nav == kNavGear) set_nav(kNavPlay, s_link);
        else if (s_nav == kNavPlay) set_nav(kNavCard, s_link);
        else if (s_nav == kNavCard) set_nav(kNavLinks, link_shown(s_link) ? s_link : 0);
        else if (hub_lang_is_open() && hub_lang_buttons() > 1) { /* the open panel sits under the whole row */
            set_nav(kNavLinks, kLangLink);
            hub_lang_set_focus(1);
        }
        return true;
    }
    if (dx != 0) {
        if (s_nav != kNavLinks) return true;
        int i = s_link;
        do i += dx;
        while (i >= 0 && i < kLinkCount && !link_shown(i));
        if (i >= 0 && i < kLinkCount) set_nav(kNavLinks, i);
        return true;
    }
    if (press) {
        if (s_nav == kNavGear) {
            pc_settings_overlay_open_in_hub();
            return true;
        }
        if (s_nav == kNavPencil) { /* the disc art page, as a click on the pencil */
            open_labels();
            return true;
        }
        if (s_nav == kNavCard) {
            hub_saves_open();
            return true;
        }
        if (s_nav == kNavLinks) {
            if (s_link == kLangLink) return false; /* the Language discs link: hub_lang_event folds / unfolds it */
            if (Rml::Element* e = s_doc->GetElementById(kLinks[s_link])) e->Click();
            return true;
        }
    }
    return false;
}

input::EventResult on_event(const input::InputEvent& event, void*) {
    using E = input::InputEvent;
    if (s_play) {
        return input::EventResult::Consume;
    }
    if (pc_settings_overlay_is_open()) {
        return input::EventResult::Pass; /* the Settings panel over the hub has the input (its own layer, just below) */
    }
    if (s_labels) {
        return labels_event(event);
    }
    if (hub_saves_is_open() && !s_report) {
        return hub_saves_event(event);
    }
    if (hub_lang_look_is_open() && !s_report) { /* the Language discs' "What changes" window (3.5 step 5) */
        return hub_lang_look_event(event);
    }
    if (s_upd_panel) {
        /* the update panel: A is Download / Restart now, B or Esc closes it (Not now / Later); the mouse clicks */
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat && key->keycode == SDLK_ESCAPE) upd_close("Esc");
            else if (key->pressed && !key->repeat && (key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER)) upd_go();
            return input::EventResult::Consume;
        }
        if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_SOUTH) upd_go();
            else if (button->pressed && (button->button == SDL_GAMEPAD_BUTTON_EAST || button->button == SDL_GAMEPAD_BUTTON_BACK))
                upd_close("B");
            return input::EventResult::Consume;
        }
        if (event.payload.get_if<E::AxisChanged>() != nullptr) return input::EventResult::Consume;
        aurora::rmlui::process_input(event);
        return input::EventResult::Consume;
    }
    if (s_report && s_report_saved) {
        /* the saved screen (2026-11-23): A / Enter / B / Esc close it (Done), Y opens the reports folder; the mouse
         * clicks its two buttons */
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat &&
                (key->keycode == SDLK_ESCAPE || key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER)) {
                close_report("Done");
            }
            return input::EventResult::Consume;
        }
        if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed && (button->button == SDL_GAMEPAD_BUTTON_SOUTH || button->button == SDL_GAMEPAD_BUTTON_EAST)) {
                close_report("Done");
            } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_NORTH) {
                report_open_folder();
            }
            return input::EventResult::Consume;
        }
        if (event.payload.get_if<E::AxisChanged>() != nullptr) {
            return input::EventResult::Consume;
        }
        aurora::rmlui::process_input(event);
        return input::EventResult::Consume;
    }
    if (s_report) {
        /* the report panel, as in the game's menu: typing goes into the text box; Esc or B cancels, A saves; the
         * mouse works too */
        if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
            if (key->pressed && !key->repeat && key->keycode == SDLK_ESCAPE) {
                close_report("Esc");
                return input::EventResult::Consume;
            }
        } else if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
            if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_SOUTH) {
                save_report();
            } else if (button->pressed &&
                       (button->button == SDL_GAMEPAD_BUTTON_EAST || button->button == SDL_GAMEPAD_BUTTON_WEST)) {
                close_report("B");
            }
            return input::EventResult::Consume;
        } else if (event.payload.get_if<E::AxisChanged>() != nullptr) {
            return input::EventResult::Consume;
        }
        aurora::rmlui::process_input(event);
        report_placeholder();
        return input::EventResult::Consume;
    }
    /* A / Start / Enter: the main action (Play; Browse with no disc; Copy when a checked disc waits); X: Move; B / Esc:
     * Cancel; Y: Report a problem */
    auto main_action = [] {
        if (!s_offer_path.empty()) {
            accept_offer(false);
        } else if (s_no_disc) {
            browse();
        } else {
            play();
        }
    };
    /* Play as a held button: the press lifts the disc, the release plays (hitch above) */
    const bool play_is_main = s_offer_path.empty() && !s_no_disc;
    /* the controller's place (nav_event: PLAY, the card, the links) and the Language discs panel (A opens the link, B
     * folds it: pc_hub_langdiscs.cpp); while PLAY has the place, A is PLAY */
    if (play_is_main && !s_hitch && (nav_event(event) || hub_lang_event(event))) {
        return input::EventResult::Consume;
    }
    if (const auto* key = event.payload.get_if<E::KeyChanged>()) {
        const bool enter = key->keycode == SDLK_RETURN || key->keycode == SDLK_KP_ENTER || key->keycode == SDLK_SPACE;
        if (enter && play_is_main && s_nav != kNavPlay) {
            /* (the card's and links' Enter is nav_event's; nothing more here) */
        } else if (enter && play_is_main) {
            if (key->pressed && !key->repeat) {
                hitch(false);
            } else if (!key->pressed && s_hitch && !s_hitch_mouse) {
                play();
            }
        } else if (key->pressed && !key->repeat && enter) {
            main_action();
        } else if (key->pressed && !key->repeat && key->keycode == SDLK_ESCAPE) {
            /* Esc opens Settings, as it does in game (snatcherLeo, 2026-11-14: "hitting the "Esc" key ... should also get us
             * to the settings menu in the hub window as well"); while a found disc waits to be copied it still
             * cancels that offer first, as before */
            if (!s_offer_path.empty()) cancel_offer();
            else pc_settings_overlay_open_in_hub();
        } else if (key->pressed && !key->repeat && pc_shortcut_held(1)) {
            pc_settings_overlay_open_in_hub(); /* the keyboard's menu shortcut from Controls, if not Esc */
        } else if (key->pressed && !key->repeat && key->keycode == SDLK_F2) {
            pc_settings_overlay_open_in_hub(); /* F2 opens Settings, as in game */
        } else if (key->pressed && !key->repeat && key->keycode == SDLK_F8) {
            report(); /* F8: Report a problem, as in game (snatcherLeo, 2026-11-13) */
        }
        return input::EventResult::Consume;
    }
    if (const auto* button = event.payload.get_if<E::ButtonChanged>()) {
        if (button->pressed && pc_shortcut_held(0)) {
            /* the controller's menu shortcut, the one that opens Settings in game (chosen in Controls; Z + L by
             * default on a GameCube controller), opens it here too (snatcherLeo, 2026-11-14: "whatever button press would get
             * to the settings menu in-game ... will also open up the settings in hub") */
            pc_settings_overlay_open_in_hub();
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_START) {
            pc_settings_overlay_open_in_hub(); /* the hint bar's "START Settings" (2026-11-11; START played before) */
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_BACK) {
            upd_open(); /* SELECT: the update panel, when the note beside the gear shows (Studio's mockup's button) */
        } else if (button->button == SDL_GAMEPAD_BUTTON_SOUTH && play_is_main && s_nav != kNavPlay) {
            /* A away from PLAY: the card's and links' own (nav_event); never PLAY */
        } else if (button->button == SDL_GAMEPAD_BUTTON_SOUTH && play_is_main) {
            if (button->pressed) {
                hitch(false);
            } else if (s_hitch && !s_hitch_mouse) {
                play();
            }
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_SOUTH) {
            main_action();
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_WEST && !s_offer_path.empty()) {
            accept_offer(true);
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_WEST && play_is_main) {
            open_labels(); /* X: the disc label page (the hint bar's "X Label") */
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_EAST) {
            cancel_offer();
        } else if (button->pressed && button->button == SDL_GAMEPAD_BUTTON_NORTH && play_is_main) {
            /* Y: Change save (mockup 62's hint bar): the Saves page (step 2.1) */
            hub_saves_open();
        }
        return input::EventResult::Consume;
    }
    aurora::rmlui::process_input(event); /* the mouse */
    return input::EventResult::Consume;
}

/* the hub holds every device, except while the Settings panel is open over it: a capturing layer blocks the layers
 * below it in Aurora's router, and the panel's layer is just below the hub's */
bool captures(const input::InputSource&, void*) { return !pc_settings_overlay_is_open(); }

/* the layout is made for 1600 x 960 dp: scale to fit the window, and stop growing at 80% of the display scaling
 * (snatcherLeo, 2026-11-11, option 2 of the fullscreen mockups: at the full scaling a fullscreen hub on his 225% display was
 * "too in your face"), but never below the design's own size (1 px per dp: 16 px body text), so an ordinary 100%
 * display keeps today's size. On a bigger window the content stays in its centred block (hub.rcss #block). */
void fit() {
    Rml::Context* context = aurora::rmlui::get_context();
    static float system_scale = 0.0f;
    if (system_scale <= 0.0f) {
        system_scale = context->GetDensityIndependentPixelRatio();
    }
    const Rml::Vector2i dims = context->GetDimensions();
    const float largest = SDL_max(0.8f * system_scale, 1.0f);
    float scale = SDL_min(largest, SDL_min(dims.x / 1600.0f, dims.y / 960.0f));
    if (scale > 0.0f && SDL_fabsf(scale - context->GetDensityIndependentPixelRatio()) > 0.01f) {
        aurora::rmlui::set_ui_scale(scale);
    }
    /* *Added 2026-11-15 (snatcherLeo: "can we also expand the area you can scroll? For saves. Since we got more room"):* the
     * full-page views are laid out on the 1600 x 960 dp design, centred; a taller window leaves room above and below it
     * (`extra`, dp each side). The Saves page uses that room: it starts 8 dp under the top bar (88 dp) and ends 8 dp
     * over the hint bar (80 dp), wherever those are; the disc art page starts under the top bar on any window (its
     * rows are fixed, so it only moves). Set when the window's size changes; all OSes the same. */
    if (s_doc != nullptr && scale > 0.0f) {
        const float extra = SDL_max(0.0f, (dims.y / scale - 960.0f) / 2.0f);
        static float last = -1.0f;
        if (SDL_fabsf(extra - last) > 0.5f) {
            last = extra;
            if (Rml::Element* e = s_doc->GetElementById("saves")) {
                e->SetProperty("top", std::to_string(96.0f - extra) + "dp");
                e->SetProperty("bottom", std::to_string(88.0f - extra) + "dp");
            }
            if (Rml::Element* e = s_doc->GetElementById("labels")) {
                /* Back to Hub sits 16 dp above the page's top: the page starts low enough that it stays under the
                 * top bar (88 dp) on a window with no spare height */
                e->SetProperty("top", std::to_string(SDL_max(72.0f, 108.0f - extra)) + "dp");
            }
            /* *3.5 step 5 (2026-11-19):* the main page's right column reaches down to the hint bar, wherever it is, so
             * the Language discs panel uses the room under the design (it used to run on into it unclipped) and the
             * column scrolls only once that room is full too (hub.rcss body.langopen #right). Folded, the column's
             * content is unchanged: it flows from the top, so where its box ends shows nowhere */
            if (Rml::Element* e = s_doc->GetElementById("right")) e->SetProperty("bottom", std::to_string(80.0f - extra) + "dp");
        }
    }
}

/* one frame of the hub: events (window closed, a file dropped on the window), then RmlUi drawn alone */
/* ORRE_HUB_TEST_MOUSE=hover|drag (a test switch, never a setting; 2026-11-21, the no-disc checks): the mouse given to
 * the page directly (RmlUi's context, in its own coordinates, so no real mouse moves): 2.5 s after the hub shows it
 * rests on the disc's centre (the pencil shows on hover with a disc); with "drag", at 6.5 s it presses on the disc's
 * right side, turns a quarter round over 10 frames and lets go, as a flick. The log gives the disc's turn angle at the
 * release and 0.5 s later, so a test reads whether the disc turned. All platforms (RmlUi) */
void test_mouse_frame() {
    static const char* mode = SDL_getenv("ORRE_HUB_TEST_MOUSE");
    static Uint64 start = 0;
    static int step = -1;
    static float cx = 0, cy = 0, r = 0;
    if (mode == nullptr || s_doc == nullptr) return;
    Rml::Context* context = aurora::rmlui::get_context();
    if (context == nullptr) return;
    const Uint64 now = SDL_GetTicks();
    if (start == 0) start = now;
    const Uint64 t = now - start;
    if (step < 0 && t > 2500 && disc_centre(cx, cy, r)) {
        context->ProcessMouseMove((int)cx, (int)cy, 0);
        std::printf("[hub] test mouse: resting on the disc's centre (%.0f, %.0f)\n", cx, cy);
        step = 0;
    }
    if (step >= 0 && SDL_strcmp(mode, "drag") == 0 && t > 6500 && step <= 12) {
        if (step == 0) {
            context->ProcessMouseMove((int)(cx + r * 0.8f), (int)cy, 0);
            context->ProcessMouseButtonDown(0, 0);
        } else if (step <= 10) {
            const float a = step * 3.14159265f / 20.0f;
            context->ProcessMouseMove((int)(cx + r * 0.8f * SDL_cosf(a)), (int)(cy - r * 0.8f * SDL_sinf(a)), 0);
        } else if (step == 11) {
            context->ProcessMouseButtonUp(0, 0);
            std::printf("[hub] test mouse: dragged a quarter turn and let go; the disc's turn %.1f degrees, speed %.0f\n",
                        s_turn_angle, s_turn_speed);
        }
        ++step;
    }
    static bool reported = false;
    if (step > 11 && !reported && t > 7300) {
        reported = true;
        std::printf("[hub] test mouse: 0.5 s after letting go, the disc's turn %.1f degrees\n", s_turn_angle);
    }
}

void frame() {
    freeze_sway_frame();
    test_mouse_frame();
    upd_frame(); /* the update note follows the shared state (a download started in Studio shows here too) */
    studio_link_frame(); /* "Open Studio" / "Studio is opened" (4.4) */
    crash_bar_frame();   /* the "closed unexpectedly" bar goes once a report carried the earlier log (5.1) */
    watch_open_disc(); /* another US disc added to ROM\ while the hub shows (4.4) */
    for (const AuroraEvent* ev = aurora_update(); ev != nullptr && ev->type != AURORA_NONE; ++ev) {
        if (ev->type == AURORA_EXIT) {
            s_quit = true;
        }
        if (ev->type == AURORA_SDL_EVENT && ev->sdl.type == SDL_EVENT_DROP_FILE && ev->sdl.drop.data != nullptr) {
            if (s_no_disc) {
                consider(ev->sdl.drop.data);
            } else if (!s_play && !hub_saves_is_open() && !s_labels && hub_lang_drop(ev->sdl.drop.data)) {
                /* a European or Japanese disc dropped on the hub: offered in the Language discs panel (piece 2) */
            } else if (hub_saves_is_save_file(ev->sdl.drop.data) && !s_play) {
                /* a .gci or a memory card (.raw) dropped anywhere on the hub: imported on the Saves page (snatcherLeo,
                 * 2026-11-13: "Same message with either dropping a .gci or whole memory card"); several dropped
                 * files arrive one event each */
                hub_saves_drop(ev->sdl.drop.data);
            } else if (dl::is_picture_name(ev->sdl.drop.data) && !s_play) {
                /* a picture dropped on the hub: onto the disc label page (the approved draft: "drop a picture on
                 * the disc"; the whole window takes it, as a dropped disc image) */
                open_labels();
                add_picture(ev->sdl.drop.data);
            }
        }
        pc_platform_window_event(ev);
    }
    pc_platform_window_frame();
    labels_frame();
    hub_saves_frame(); /* the Saves page: files picked in the file window, the search box */
    if (!s_no_disc) hub_lang_frame(); /* the Language discs: picked files, a copy's progress, the folders' watch */
    if (!pc_controls_ui_is_open()) {
        fit(); /* the controller setup screen sets its own size while it is open (pc_controls_ui.cpp) */
    }
    turn_update();
    pc_settings_overlay_hub_frame(); /* the Settings panel's per-frame work while it is open over the hub */
    static int s_settings_shown = -1; /* the bottom bar's hints follow the Settings page (hub.rcss body.settings) */
    static int s_dark_shown = -1;     /* light or dark, as Appearance > Theme says now (hub.rcss body.light) */
    static int s_anim_shown = -1;     /* Appearance > Animations: off stills the disc (hub.rcss body.noanim) */
    const int settings_open = pc_settings_overlay_is_open();
    const int dark = pc_ui_theme_is_dark();
    const int anim = pc_ui_animations_on();
    if ((settings_open != s_settings_shown || dark != s_dark_shown || anim != s_anim_shown) && s_doc != nullptr) {
        s_settings_shown = settings_open;
        s_dark_shown = dark;
        s_anim_shown = anim;
        if (Rml::Element* body = s_doc->GetElementById("body")) {
            body->SetClass("settings", settings_open != 0);
            body->SetClass("dark", dark != 0);
            body->SetClass("light", dark == 0);
            body->SetClass("noanim", anim == 0);
        }
    }
    if (s_report && s_focus_note > 0 && --s_focus_note == 0) {
        if (Rml::Element* e = s_doc->GetElementById("rnote")) {
            e->Focus(); /* typing goes straight into the box (as in the game's menu) */
        }
    }
    /* back on the hub's main page from the Saves page, Settings or the disc art page: the controller's place is PLAY,
     * so A starts the game at once (snatcherLeo, 2026-11-15: "we should just ensure the cursor is always on PLAY").
     * *Moved 2026-11-15 (snatcherLeo: "I can briefly see the cursor flicker from the saves and go up to "PLAY""):* this ran at
     * the frame's start, before the events that close a page, so one frame was drawn with the ring still on the save
     * card; now it runs after them, just before the ring is drawn, so the first frame back already shows PLAY */
    {
        static bool was_away = false;
        const bool away = s_labels || hub_saves_is_open() || pc_settings_overlay_is_open();
        if (was_away && !away) set_nav(kNavPlay, 0);
        was_away = away;
    }
    update_rings(); /* the controller's ring at its current size */
    ++pc_vi_alive; /* time in the hub is not a stuck program (the hang watch, pc_health.c) */
    pc_health_frame(); /* the hub's frames in the performance log */
    static int s_drawn = 0;
    if (aurora_begin_frame()) {
        aurora_end_frame(); /* nothing of the game yet: the frame is RmlUi's hub */
        /* the window appears only now, with the hub on it at its final scale and with its pictures loaded */
        if (++s_drawn <= 3) {
            static const char* const kDrawn[] = {"hub: first frame drawn", "hub: second frame drawn",
                                                 "hub: third frame drawn, window shown"};
            pc_startup_mark(kDrawn[s_drawn - 1]);
        }
        if (s_drawn == 3) {
            pc_window_cloak(0);
            pc_platform_handover_ready(); /* Quit to hub: the previous OrrePort's closing screen gives way to this hub */
            /* this window, for Studio's "Open OrrePort" / "Close OrrePort" (pc_apps.c); Windows-only underneath */
            if (SDL_Window** all = SDL_GetWindows(nullptr)) {
                if (all[0] != nullptr)
                    pc_apps_set_window(PC_APP_HUB, apps_root(), SDL_GetPointerProperty(SDL_GetWindowProperties(all[0]),
                                                                          SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
                SDL_free(all);
            }
        }
    }
    SDL_Delay(4);
}

bool setup() {
    if (s_doc != nullptr) {
        return true;
    }
    if (!aurora::rmlui::is_initialized()) {
        return false;
    }
    Rml::Context* context = aurora::rmlui::get_context();
    if (context == nullptr) {
        return false;
    }
    pc_startup_mark("hub: setting up");
    pc_ui_load_fonts();
    pc_startup_mark("hub: fonts loaded");
    const Uint64 t0 = SDL_GetTicks();
    /* the blank disc: the label without its printing (the lettering #E9EDEA, the name, underline, moon and sun
     * #E04A3C, the code line #8A938E); the rim, the black label and the clear rings stay */
    /* drawn at the size they are shown at the display's scaling (the hub never scales above it, fit()), so they stay
     * sharp (snatcherLeo, 2026-11-09: "make sure it's high quality still, doesn't look blurry"); smaller windows scale them
     * down through mipmaps. The disc is 540 dp, the wordmark 278 dp wide. */
    const float dpi_scale = SDL_max(1.0f, context->GetDensityIndependentPixelRatio());
    const int disc_px = SDL_clamp(((int)SDL_ceilf(540.0f * dpi_scale) + 63) / 64 * 64, 1024, 2048);
    const int mark_px = SDL_clamp(((int)SDL_ceilf(278.0f * dpi_scale) + 63) / 64 * 64, 512, 1024);
    s_disc_px = disc_px;
    s_edit_px = SDL_clamp(((int)SDL_ceilf(400.0f * dpi_scale) + 3) / 4 * 4, 400, 1024);
    s_thumb_px = SDL_clamp(((int)SDL_ceilf(84.0f * dpi_scale) + 3) / 4 * 4, 96, 256);
    /* *Updated 2026-11-11:* the half-and-half label (snatcherLeo's B2; tools/local/brand/disc_label.py): the blank disc keeps
     * the rim, the indigo and black halves and the clear centre, and leaves out every printed colour: the title's
     * yellow, red and dark outline, the moon's black (#0A0A0B, kept apart from the bottom's #0C0C0D), the sun's pink,
     * "ORREPORT" and the code line. *Updated 2026-11-13:* the code line is #C3CAC6 now (mockup 48, snatcherLeo: "just do 2
     * (lighter)"); this list names it by colour, so it changes with the label script's --code-colour */
    static const std::vector<std::string> kBlankSkips = {"#FFCB05", "#E3350D", "#141A33", "#0A0A0B",
                                                         "#F49AC8", "#E9EDEA", "#C3CAC6"};
    /* all of these at once, one thread each (pc_parallel.h says why: drawn one after another they took about 0.3 s of
     * the start, 2026-11-13). Each job writes only its own picture. The rim reaches each picture's edge: a margin of 1/32
     * of the size each side (add_margin; hub.rcss draws the disc's image boxes 17/16 as large to match), added after
     * the hole and the smear, which are drawn in label units. The disc label page's own pieces wait until the page
     * first opens (page_pictures); the hub's start needs only these and the rim for worn art (frame_hub). */
    bool label = false, wordmark = false;
    Picture frame_hub;
    pc_run_together({
        [&] {
            label = load_svg("discs/colosseum.svg", disc_px, s_label);
            gamecube_hole(s_label);
            add_margin(s_label, disc_px / 32);
        },
        [&] {
            load_svg("discs/colosseum.svg", disc_px, s_blank, kBlankSkips);
            gamecube_hole(s_blank);
            add_margin(s_blank, disc_px / 32);
        },
        [&] {
            /* *Added 2026-11-21 (snatcherLeo: with no disc added, "the disc just have its top be gray. Or, some other shade,
             * doesn't matter (so it looks fine in dark and light mode). It'll only change to our disc art once you point
             * the NTSC rom"; then one shade for both themes):* the empty disc: the blank disc with its indigo top
             * (#3D4F8F, the label's upper half) drawn in kEmptyTop, a neutral mid grey that reads on both themes'
             * backgrounds, in the same place in the drawing order, so the clear centre ring and the hole stay over it
             * as on the ready disc. *Corrected the same night (snatcherLeo's picture: the grey covered the centre):* the first
             * version drew the top as a separate layer over the whole disc. The real label shows as before once the
             * disc is recognised */
            static const char* const kEmptyTop = "#7D8481";
            load_svg_recoloured("discs/colosseum.svg", "#3D4F8F", kEmptyTop, disc_px, s_empty, kBlankSkips);
            gamecube_hole(s_empty);
            add_margin(s_empty, disc_px / 32);
        },
        [&] {
            wordmark = load_svg("icons/orreport-wordmark-rust.svg", mark_px, s_wordmark);
            load_svg("icons/orreport-wordmark-rust-light.svg", mark_px, s_wordmark_light); /* light mode (hub.rcss) */
        },
        [&] {
            make_smear(disc_px / 2, s_smear); /* a blur by nature: half the label's size is plenty */
            add_margin(s_smear, disc_px / 2 / 32);
        },
        [&] { load_svg("discs/colosseum.svg", disc_px, frame_hub, kPagePrinting); },
        [&] { /* the save card's symbols (38 dp wide) and the folder (20 dp), at the display's scaling */
            load_svg("icons/save-gci.svg", SDL_max(38, (int)SDL_ceilf(38.0f * dpi_scale)), s_icon_gci);
            load_svg("icons/save-orresave.svg", SDL_max(38, (int)SDL_ceilf(38.0f * dpi_scale)), s_icon_orre);
            load_svg("icons/folder.svg", SDL_max(20, (int)SDL_ceilf(20.0f * dpi_scale)), s_icon_folder);
            /* the disc art shelf's outline (88 dp: the 84 dp disc and 2 dp clear round it) */
            load_svg("icons/shelf-ring.svg", SDL_max(88, (int)SDL_ceilf(88.0f * dpi_scale)), s_icon_ring);
            /* the disc's pencil: a smooth filled circle under the ring (hub.rcss #b-label; 60 dp, its own size) */
            load_svg("icons/round-fill.svg", SDL_max(60, (int)SDL_ceilf(60.0f * dpi_scale)), s_icon_round);
            load_svg("icons/shelf-ring.svg", SDL_max(60, (int)SDL_ceilf(60.0f * dpi_scale)), s_icon_ring_small);
            /* the Language discs panel's disc symbols (36 dp; their own colours: piece 2, language-discs.md) */
            load_svg("icons/lang-disc-eu.svg", SDL_max(36, (int)SDL_ceilf(36.0f * dpi_scale)), s_icon_lang_eu);
            load_svg("icons/lang-disc-jp.svg", SDL_max(36, (int)SDL_ceilf(36.0f * dpi_scale)), s_icon_lang_jp);
            load_svg("icons/lang-disc-us.svg", SDL_max(36, (int)SDL_ceilf(36.0f * dpi_scale)), s_icon_lang_us);
            /* the Language discs link's globe (20 dp; tinted like the link, hub.rcss .lkdisc) */
            load_svg("icons/globe.svg", SDL_max(20, (int)SDL_ceilf(20.0f * dpi_scale)), s_icon_globe);
        },
    });
    s_frame_hub = as_rgba(std::move(frame_hub));
    s_label_own = s_label;
    s_smear_own = s_smear;
    s_blank_own = s_blank;
    /* *Corrected 2026-11-13:* the time printed as "smear" was all of the pictures above (the disc's SVG at several
     * sizes, the wordmark, the smear), not the smear alone */
    std::printf("[hub] pictures at %.2fx: disc label %s and blank disc (%d px), wordmark %s (%d px), all pictures (%u ms)\n",
                dpi_scale, label ? "ok" : "missing", disc_px, wordmark ? "ok" : "missing", mark_px,
                (unsigned)(SDL_GetTicks() - t0));
    /* disc://label, disc://blank, disc://smear, disc://banner (the player's own banner); brand://wordmark */
    aurora::rmlui::register_texture_provider("disc", [](std::string_view source) {
        if (source.find("empty") != std::string_view::npos) { /* the empty disc, grey top (no disc added; 2026-11-21) */
            return serve(s_empty);
        }
        if (source.find("label") != std::string_view::npos) {
            return serve(s_label);
        }
        if (source.find("blank") != std::string_view::npos) {
            return serve(s_blank);
        }
        if (source.find("smear") != std::string_view::npos) {
            return serve(s_smear);
        }
        if (s_banner.empty()) {
            return std::optional<aurora::rmlui::RuntimeTexture>{};
        }
        return std::optional<aurora::rmlui::RuntimeTexture>{
            aurora::rmlui::RuntimeTexture{96, 32, std::span<const std::byte>(s_banner), false, false}};
    });
    /* label://e/<version>: the label page's editor; label://t/<id>/<version>: a thumbnail. A new version is a new
     * name, so RmlUi loads the new picture; the old name is released (edit_refresh, tile_rml) */
    aurora::rmlui::register_texture_provider("label", [](std::string_view source) {
        if (source.find("://e/") != std::string_view::npos) {
            return serve(s_edit);
        }
        const size_t at = source.find("://t/");
        if (at != std::string_view::npos) {
            int id = 0;
            for (size_t i = at + 5; i < source.size() && source[i] >= '0' && source[i] <= '9'; ++i) {
                id = id * 10 + (source[i] - '0');
            }
            for (const auto& [key, t] : s_thumbs) {
                if (t.id == id && t.ready) return serve(t.pic);
            }
        }
        return std::optional<aurora::rmlui::RuntimeTexture>{};
    });
    aurora::rmlui::register_texture_provider("brand", [](std::string_view source) {
        return serve(source.find("light") != std::string_view::npos ? s_wordmark_light : s_wordmark);
    });
    /* hubring://...: the controller's ring round PLAY or the card; hubfill://...: PLAY's face; both drawn at their
     * exact size (update_rings) */
    for (const char* scheme : {"hubring", "hubfill"}) {
        aurora::rmlui::register_texture_provider(scheme, [](std::string_view source) {
            const Picture* p = ring_picture(source);
            return p != nullptr ? serve(*p) : std::optional<aurora::rmlui::RuntimeTexture>{};
        });
    }
    aurora::rmlui::register_texture_provider("hubbox", [](std::string_view source) {
        const Picture* p = box_picture(source);
        return p != nullptr ? serve(*p) : std::optional<aurora::rmlui::RuntimeTexture>{};
    });
    /* hubicon://gci, hubicon://orresave, hubicon://folder: the save card's pictures */
    aurora::rmlui::register_texture_provider("hubicon", [](std::string_view source) {
        if (source.find("globe") != std::string_view::npos) return serve(s_icon_globe);
        if (source.find("langeu") != std::string_view::npos) return serve(s_icon_lang_eu);
        if (source.find("langjp") != std::string_view::npos) return serve(s_icon_lang_jp);
        if (source.find("langus") != std::string_view::npos) return serve(s_icon_lang_us);
        if (source.find("orresave") != std::string_view::npos) return serve(s_icon_orre);
        if (source.find("folder") != std::string_view::npos) return serve(s_icon_folder);
        if (source.find("smallring") != std::string_view::npos) return serve(s_icon_ring_small);
        if (source.find("ring") != std::string_view::npos) return serve(s_icon_ring);
        if (source.find("round") != std::string_view::npos) return serve(s_icon_round);
        return serve(s_icon_gci);
    });
    pc_startup_mark("hub: pictures drawn");
    s_doc = context->LoadDocument(ui_path("hub.rml"));
    if (s_doc == nullptr) {
        std::printf("[hub] could not load %s\n", ui_path("hub.rml").c_str());
        return false;
    }
    pc_startup_mark("hub: page loaded");
    hub_saves_attach(s_doc, [] { show_save(); }); /* the Saves page; its choices update the card and PLAY */
    hub_lang_attach(s_doc); /* the Language discs link and panel (folded) */
    upd_start();            /* the update check (4.4): in the background, its note when there is something to say */
    crash_bar_start();      /* the previous run ended unexpectedly: offer a report (5.1) */
    set_nav(kNavPlay, 0);   /* the controller's place starts on PLAY */
    /* ORRE_HUB_TEST_NAV=card | links:<0-2> (a test switch, never a setting; 2026-11-15): start with the place
     * there, for pictures of the highlights without a controller */
    if (const char* tn = SDL_getenv("ORRE_HUB_TEST_NAV")) {
        if (SDL_strcmp(tn, "card") == 0) set_nav(kNavCard, 0);
        else if (SDL_strcmp(tn, "gear") == 0) set_nav(kNavGear, 0);
        else if (SDL_strncmp(tn, "links:", 6) == 0) set_nav(kNavLinks, SDL_atoi(tn + 6));
    }
    s_doc->AddEventListener(Rml::EventId::Mousedown, &s_presses);
    s_doc->AddEventListener(Rml::EventId::Mouseup, &s_presses);
    s_doc->AddEventListener(Rml::EventId::Click, &s_clicks);
    s_doc->AddEventListener(Rml::EventId::Mousedown, &s_turn);
    s_doc->AddEventListener(Rml::EventId::Mousemove, &s_turn);
    s_doc->AddEventListener(Rml::EventId::Mouseup, &s_turn);
    s_doc->AddEventListener(Rml::EventId::Mousedown, &s_label_mouse);
    s_doc->AddEventListener(Rml::EventId::Mousemove, &s_label_mouse);
    s_doc->AddEventListener(Rml::EventId::Mouseup, &s_label_mouse);
    s_doc->AddEventListener(Rml::EventId::Mousescroll, &s_label_mouse);
    s_doc->Show();
    s_layer = input::register_layer({
        .label = "OrrePort hub",
        .priority = input::kRmlUiLayerPriority + 1,
        .onEvent = on_event,
        .capturesSource = captures,
    });
    return true;
}

void teardown() {
    if (s_doc == nullptr) {
        return; /* never set up (an automated run) */
    }
    input::unregister_layer(s_layer);
    stop_small_worker(); /* the thumbnails' reader, if the label page started it */
    aurora::rmlui::set_ui_scale(0.0f); /* back to following the window (the overlay sets its own) */
    if (s_doc != nullptr) {
        s_doc->Close();
        s_doc = nullptr;
    }
}

} // namespace

/* pc_platform_start, when ROM\ holds no USA disc: the hub with a blank disc until one is added. 1: a disc is open
 * (the platform goes on to its checks while the page stays up); 0: the window was closed. */
extern "C" int pc_hub_wait_for_disc(void) {
    if (SDL_getenv("ORRE_SKIP_HUB") != nullptr || !setup()) {
        char folder[1300];
        const std::string message = tx("disc.missing.text", {{"folder", pc_platform_rom_folder(folder, sizeof(folder))}});
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, pc_text("disc.missing.title"), message.c_str(), nullptr);
        return 0;
    }
    s_no_disc = true;
    set_body_class("nodisc", true);
    set_text("status", "<span class=\"dot\"></span> " + rml_escape(tx("hub.status.no_disc")));
    set_text("sub", rml_escape(tx("hub.sub.no_disc")));
    s_watch_last = s_watch_scanned = rom_signature(); /* what is there now was just looked at */
    std::printf("[hub] no disc yet: showing the blank disc (drop an image on the window, Browse..., or the ROM folder)\n");
    const char* test_add = SDL_getenv("ORRE_HUB_TEST_ADD");
    const Uint64 shown_at = SDL_GetTicks();
    bool test_added = false;
    while (!s_quit && !s_disc_found) {
        if (test_add != nullptr && !test_added && SDL_GetTicks() - shown_at > 2500) {
            test_added = true;
            consider(test_add);
        }
        /* 2026-11-21 (a test switch for pictures): ORRE_HUB_TEST_SAVES=settings_discart opens Settings on Appearance
         * with "Change disc art" focused here too, where it is greyed (no disc yet) */
        static bool test_discart_done = false;
        if (!test_discart_done && SDL_GetTicks() - shown_at > 2500) {
            test_discart_done = true;
            const char* t = SDL_getenv("ORRE_HUB_TEST_SAVES");
            if (t != nullptr && std::strcmp(t, "settings_discart") == 0) pc_settings_overlay_open_in_hub_disc_art();
        }
        watch_disc();
        frame();
    }
    if (s_quit) {
        teardown();
        return 0;
    }
    set_text("status", "<span class=\"dot\"></span> " + rml_escape(tx("hub.status.checking")));
    set_note(tx("hub.note.checking"));
    frame();
    frame(); /* the page as it stands while the platform checks the disc */
    return 1;
}

/* 1 when the hub is up with a disc, so the disc art page can open (Settings asks first: with no disc it says to add
 * one instead of closing; snatcherLeo, 2026-11-13) */
extern "C" int pc_hub_disc_ready(void) { return s_doc != nullptr && !s_no_disc && !s_play ? 1 : 0; }

/* Settings > Appearance > Disc label, in the hub (pc_settings_overlay.cpp closes itself first): the label page.
 * 1 if it opened (the hub is up with a disc), 0 otherwise (in game, or no disc yet). */
extern "C" int pc_hub_open_labels(void) {
    if (s_doc == nullptr || s_no_disc || s_play) {
        return 0;
    }
    open_labels();
    return 1;
}

/* Settings > General > More languages, in the hub (pc_settings_overlay.cpp closes itself first): the Language discs
 * panel, unfolded. 1 if it opened (the hub is up with a disc), 0 otherwise (in game, or no disc yet) */
extern "C" int pc_hub_open_langdiscs(void) {
    if (s_doc == nullptr || s_no_disc || s_play) {
        return 0;
    }
    if (s_labels) close_labels();
    if (hub_saves_is_open()) hub_saves_close();
    hub_lang_open(true);
    return 1;
}

/* pc_game_main.c, after the disc checks: 1: go on into the game; 0: the window was closed in the hub */
extern "C" int pc_hub_run(void) {
    if (pc_platform_automated_run() || SDL_getenv("ORRE_SKIP_HUB") != nullptr || !setup()) {
        teardown();
        return 1;
    }
    const bool banner = read_banner();
    pc_startup_mark("hub: banner read from the disc");
    /* the disc wears the player's chosen label (the disc label page), drawn now that the banner is read */
    s_wear = dl::wearing(kLabelGame);
    if (s_wear != "orreport") {
        apply_label_to_hub();
        pc_startup_mark("hub: the chosen disc art drawn");
    }
    /* the ROM is open now: a chosen game icon can be read (the window was given the logo before that) */
    pc_window_icon_game(pc_settings_int("ui_window_icon", 0) == 1 ? 1 : 0);
    pc_startup_mark("hub: window icon read from the disc");
    if (s_no_disc) {
        /* the disc was just recognised: its printing fades in over the blank disc (hub.rcss #label) */
        s_no_disc = false;
        set_body_class("nodisc", false);
    }
    /* mockup 62: "DISC READY" and "NTSC-U · GC6E01" (the game's name is the title under it; "Disc checked" went, snatcherLeo
     * on mockup 50: "We also don't need both "DISC READY" and "Disc checked"") */
    set_text("status", "<span class=\"dot\"></span> " + rml_escape(tx("hub.status.ready")));
    set_text("sub", "NTSC-U · GC6E01"); /* the disc's region and game ID, as printed on it: not translated */
    show_save();
    set_note("");
    std::printf("[hub] showing the hub (banner %s): A / Enter / click PLAY starts Colosseum\n",
                banner ? "read from the disc" : "missing");
    /* saves the player put loose into Saves\ were imported at start-up (pc_saves_init): the import question for them
     * (3.5; pc_hub_saves.cpp), over the Saves page, as for any other import */
    hub_saves_ask_loose();
    const char* autoplay_env = SDL_getenv("ORRE_HUB_AUTOPLAY");
    const Uint64 autoplay = autoplay_env != nullptr ? (Uint64)SDL_strtoull(autoplay_env, nullptr, 10) : 0;
    const Uint64 shown_at = SDL_GetTicks();
    /* test hook: ORRE_HUB_TEST_LABEL=<picture> opens the disc label page and adds it, as if dropped; "open" only opens
     * the page. ORRE_HUB_TEST_LABEL_MS: when, in ms after the hub shows (2500 by default) */
    const char* test_label = SDL_getenv("ORRE_HUB_TEST_LABEL");
    const char* test_label_ms = SDL_getenv("ORRE_HUB_TEST_LABEL_MS");
    const Uint64 test_label_at = test_label_ms != nullptr ? (Uint64)SDL_strtoull(test_label_ms, nullptr, 10) : 2500;
    bool test_label_done = false;
    /* test hook: ORRE_HUB_TEST_SAVES=open opens the Saves page 2.5 s after the hub shows ("versions": with the chosen
     * save's go-back window open); a file's path imports it as
     * if dropped (the page opens with it), so scripted test windows can photograph the page. *2.12 (2026-11-16),
     * GameCube-accurate mode per save:* "accurate:<slot>" presses that save's accurate switch, "gcopyacc:<slot>" makes
     * accurate mode's GameCube copy of it (written beside the exe, then added), "whatoff" opens the Saves page's "What it
     * turns off", "settings" and "settings_whatoff" open Settings › General and its list. *3.5:* "importask:<slot>[,..]"
     * opens the import question for those saves (ORRE_HUB_TEST_IMPORT_ANSWER answers it, pc_hub_saves.cpp) */
    const char* test_saves = SDL_getenv("ORRE_HUB_TEST_SAVES");
    bool test_saves_done = false;
    pc_vi_close_first_frame(); /* the hub draws its own frames; the game's first frame opens again below */
    while (!s_quit) {
        if (test_saves != nullptr && !test_saves_done && SDL_GetTicks() - shown_at > 2500) {
            test_saves_done = true;
            if (std::strcmp(test_saves, "open") == 0) {
                hub_saves_open();
            } else if (std::strcmp(test_saves, "versions") == 0) {
                hub_saves_test_versions(); /* the go-back window too (mockup 74's photographs) */
            } else if (std::strncmp(test_saves, "accurate:", 9) == 0) {
                hub_saves_test_accurate(std::atoi(test_saves + 9)); /* 2.12: that save's accurate switch pressed */
            } else if (std::strncmp(test_saves, "gcopyacc:", 9) == 0) {
                /* 2.12: accurate mode's copy, written in the test files' place (no Save As window) and added in accurate
                 * mode; beside the exe in the flat layout, data\system\ in the portable one (app\ is never written), 4.1 */
                const std::string out = pc_paths_str(PC_PLACE_TEST, "gcopy_test.gci");
                hub_saves_test_gcopy(std::atoi(test_saves + 9), out.c_str());
            } else if (std::strncmp(test_saves, "gcopy:", 6) == 0) {
                hub_saves_test_gcopy(std::atoi(test_saves + 6), nullptr); /* 2026-11-21: the copy window, open only */
            } else if (std::strcmp(test_saves, "whatoff") == 0) {
                hub_saves_test_whatoff(); /* 2.12: the "What it turns off" list */
            } else if (std::strncmp(test_saves, "playauto:", 9) == 0) {
                hub_saves_test_play_autosave(std::atoi(test_saves + 9)); /* 2026-11-21: the card names the autosave */
            } else if (std::strncmp(test_saves, "importask:", 10) == 0) {
                hub_saves_test_import_ask(test_saves + 10); /* 3.5: the import question for those saves */
            } else if (std::strcmp(test_saves, "settings") == 0) {
                pc_settings_overlay_open_in_hub(); /* 2.12: Settings › General with accurate mode's row */
            } else if (std::strcmp(test_saves, "settings_discart") == 0) {
                /* 2026-11-21: Settings on Appearance with "Change disc art" focused (greyed with no disc) */
                pc_settings_overlay_open_in_hub_disc_art();
            } else if (std::strcmp(test_saves, "settings_whatoff") == 0) {
                pc_settings_overlay_open_in_hub(); /* the same list from Settings */
                pc_settings_overlay_test_whatoff();
            } else {
                hub_saves_drop(test_saves);
            }
        }
        if (test_label != nullptr && !test_label_done && SDL_GetTicks() - shown_at > test_label_at) {
            test_label_done = true;
            open_labels();
            if (std::strcmp(test_label, "open") != 0) {
                add_picture(test_label);
            }
        }
        /* ORRE_HUB_TEST_LABEL_PLACE=<zoom>,<turn> (a test switch, never a setting; 2026-11-15): 1.5 s after the
         * test picture, place it so (pictures of zooming out and turning without a mouse) */
        static bool test_place_done = false;
        if (test_label != nullptr && test_label_done && !test_place_done &&
            SDL_GetTicks() - shown_at > test_label_at + 1500) {
            test_place_done = true;
            if (const char* tp = SDL_getenv("ORRE_HUB_TEST_LABEL_PLACE")) {
                float z = 0.0f, r = 0.0f;
                if (std::sscanf(tp, "%f,%f", &z, &r) >= 1 && is_editable(s_wear)) {
                    dl::Place p = place_of(s_wear);
                    p.zoom = z;
                    p.rot = r;
                    place_changed(p);
                    place_settled();
                }
            }
        }
        if (autoplay > 0 && !s_play && SDL_GetTicks() - shown_at > autoplay) {
            play();
        }
        if (s_play && SDL_GetTicks() - s_play_at > 1050) {
            break; /* the Lift and spin has run and the screen is black */
        }
        frame();
    }
    teardown();
    std::printf("[hub] %s\n", s_quit ? "window closed in the hub" : "into the game");
    if (!s_quit) {
        pc_vi_begin(); /* the frame the game's first drawing goes into, as pc_platform_start left it */
    }
    return s_quit ? 0 : 1;
}
