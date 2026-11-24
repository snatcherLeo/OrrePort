/*
 * pc_disc_label.cpp: the player's disc labels, their folder and their drawing (pc_disc_label.h says what and why).
 * 2026-11-13, after snatcherLeo chose mockup 47's label shelf ("Yes, build it").
 *
 * The folder: the game's Disc Art\ (Pokemon Colosseum\Disc Art\ in the portable layout, snatcherLeo 2026-11-21, named
 * "Disc Art" the same day, revisited when XD arrives; <OrrePort>/Disc labels/ in the flat layout: pc_paths.h PC_PLACE_ART), so the portable folder keeps the
 * pictures with it. labels.txt in it is OrrePort's own small list, one line each:
 *     wear <game> <orreport | banner | picture:<file>>
 *     picture <zoom> <x> <y> <used> <file>
 *     picture2 <zoom> <x> <y> <turn> <used> <file>   (a turned picture; 2026-11-15)
 * (the file name last, so names with spaces need no quoting). A picture put in the folder by hand joins the shelf the
 * next time the page opens; one that is gone is forgotten. Nothing here is game data: the banner is read from the
 * player's disc each time and never stored.
 *
 * Platforms: SDL's file functions and PNG / BMP readers work the same on Windows, Linux and macOS (likely; checked on
 * Windows). Folder names are UTF-8, as SDL takes them everywhere.
 */
#include "pc_disc_label.h"
#include "pc_parallel.h" /* compose_picture's bands */
#include "pc_text.h"     /* the problems with a picture, in the player's language */

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>

/* The picture formats beyond SDL's own PNG and BMP (snatcherLeo, 2026-11-13: "how do we accept all kinds of image types? WE
 * just accept as many and convert to PNG?", then "approve stb_image and libwebp"). Both live outside the repo in
 * decomp/_external/ (port/CMakeLists.txt finds them); without them only PNG and BMP are read. Read from memory (the
 * file loaded by SDL), so names in any language work the same on every OS. */
#if defined(ORRE_HAVE_STB_IMAGE)
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_NO_STDIO /* from memory only */
#define STBI_NO_HDR   /* no high-dynamic-range or old Softimage pictures: not label pictures */
#define STBI_NO_PIC
#define STBI_NO_PNM
#include <stb_image.h>
#endif
#if defined(ORRE_HAVE_WEBP)
#include <webp/decode.h>
#endif

#include "pc_paths.h" /* the art folder (4.1) */

namespace disc_label {
namespace {

std::vector<Entry> s_entries;
std::map<std::string, std::string> s_wear; /* game -> what it wears */
uint64_t s_used = 0;
bool s_loaded = false;

std::string base_name(const std::string& path) {
    const size_t cut = path.find_last_of("/\\");
    return cut == std::string::npos ? path : path.substr(cut + 1);
}

std::string lower(std::string s) {
    for (char& c : s) c = (char)SDL_tolower((unsigned char)c);
    return s;
}

bool exists(const std::string& path) {
    SDL_PathInfo info;
    return SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE;
}

/* the first free name: "art.png", then "art (2).png", "art (3).png", ... */
std::string free_name(const std::string& dir, const std::string& name) {
    if (!exists(dir + name)) {
        return name;
    }
    const size_t dot = name.find_last_of('.');
    const std::string stem = dot == std::string::npos ? name : name.substr(0, dot);
    const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
    for (int n = 2;; ++n) {
        const std::string candidate = stem + " (" + std::to_string(n) + ")" + ext;
        if (!exists(dir + candidate)) {
            return candidate;
        }
    }
}

/* one sample of the picture between its pixels (bilinear); straight RGBA out, 0-1. *Changed 2026-11-15 (zoom out and
 * turn):* outside the picture it is see-through (the disc's black shows), with a one-pixel soft edge so the picture's
 * border stays smooth; it used to repeat the edge pixels, which never showed while a picture always covered the disc */
void sample(const Rgba& pic, float fx, float fy, float out[4]) {
    const float ex = SDL_clamp(SDL_min(fx + 0.5f, (pic.w - 0.5f) - fx) + 0.5f, 0.0f, 1.0f);
    const float ey = SDL_clamp(SDL_min(fy + 0.5f, (pic.h - 0.5f) - fy) + 0.5f, 0.0f, 1.0f);
    if (ex <= 0.0f || ey <= 0.0f) {
        out[0] = out[1] = out[2] = out[3] = 0.0f;
        return;
    }
    fx = SDL_clamp(fx, 0.0f, (float)(pic.w - 1));
    fy = SDL_clamp(fy, 0.0f, (float)(pic.h - 1));
    const int x0 = (int)fx, y0 = (int)fy;
    const int x1 = SDL_min(x0 + 1, pic.w - 1), y1 = SDL_min(y0 + 1, pic.h - 1);
    const float tx = fx - x0, ty = fy - y0;
    const unsigned char* p00 = &pic.px[((size_t)y0 * pic.w + x0) * 4];
    const unsigned char* p10 = &pic.px[((size_t)y0 * pic.w + x1) * 4];
    const unsigned char* p01 = &pic.px[((size_t)y1 * pic.w + x0) * 4];
    const unsigned char* p11 = &pic.px[((size_t)y1 * pic.w + x1) * 4];
    const float w00 = (1 - tx) * (1 - ty), w10 = tx * (1 - ty), w01 = (1 - tx) * ty, w11 = tx * ty;
    /* premultiplied while mixing, so a see-through pixel's colour doesn't bleed into its neighbours */
    const float a = (p00[3] * w00 + p10[3] * w10 + p01[3] * w01 + p11[3] * w11) / 255.0f;
    for (int c = 0; c < 3; ++c) {
        const float v = (p00[c] * p00[3] * w00 + p10[c] * p10[3] * w10 + p01[c] * p01[3] * w01 +
                         p11[c] * p11[3] * w11) / (255.0f * 255.0f);
        out[c] = a > 0.0f ? v / a : 0.0f;
    }
    out[3] = a * ex * ey;
}

/* the label's black (disc_label.py's bottom half, #0C0C0D): what shows through a see-through part of a picture */
constexpr float kBlack = 12.0f / 255.0f, kBlackB = 13.0f / 255.0f;

/* out pixel = rgb with coverage cov (0-1), then the frame's pixel over it (straight alpha both ways) */
void put(Rgba& out, const Rgba& frame, size_t i, float r, float g, float b, float cov) {
    const unsigned char* f = &frame.px[i * 4];
    const float fa = f[3] / 255.0f;
    const float a = fa + cov * (1.0f - fa);
    unsigned char* o = &out.px[i * 4];
    if (a <= 0.0f) {
        o[0] = o[1] = o[2] = o[3] = 0;
        return;
    }
    const float k = cov * (1.0f - fa);
    o[0] = (unsigned char)SDL_lroundf(SDL_clamp((f[0] / 255.0f * fa + r * k) / a, 0.0f, 1.0f) * 255.0f);
    o[1] = (unsigned char)SDL_lroundf(SDL_clamp((f[1] / 255.0f * fa + g * k) / a, 0.0f, 1.0f) * 255.0f);
    o[2] = (unsigned char)SDL_lroundf(SDL_clamp((f[2] / 255.0f * fa + b * k) / a, 0.0f, 1.0f) * 255.0f);
    o[3] = (unsigned char)SDL_lroundf(a * 255.0f);
}

/* how much of an output pixel the printed circle covers (smooth at its edge; the rim covers the edge anyway) */
float circle_cover(float d, float unit) { return SDL_clamp((kLabelRadius + 1.0f - d) / unit + 0.5f, 0.0f, 1.0f); }

} // namespace

std::string folder() { return pc_paths_str(PC_PLACE_ART); /* Pokemon Colosseum\Disc Art\ (flat: Disc labels\), 4.1 */ }

std::vector<Entry>& entries() { return s_entries; }

Entry* find(const std::string& file) {
    for (Entry& e : s_entries) {
        if (e.file == file) return &e;
    }
    return nullptr;
}

uint64_t next_used() { return ++s_used; }

bool is_picture_name(const std::string& name) {
    const std::string n = lower(name);
    auto ends = [&](const char* ext) {
        const size_t l = std::strlen(ext);
        return n.size() > l && n.compare(n.size() - l, l, ext) == 0;
    };
    if (ends(".png") || ends(".bmp")) return true;
#if defined(ORRE_HAVE_STB_IMAGE)
    if (ends(".jpg") || ends(".jpeg") || ends(".gif") || ends(".tga") || ends(".psd")) return true;
#endif
#if defined(ORRE_HAVE_WEBP)
    if (ends(".webp")) return true;
#endif
    return false;
}

/* "PNG, JPG, GIF, TGA, PSD, WebP or BMP": the names as files call them; the "or" in the player's language
 * ("label.formats.list", pc_text.h) */
std::string formats_text() {
    std::string t = "PNG";
#if defined(ORRE_HAVE_STB_IMAGE)
    t += ", JPG, GIF, TGA, PSD";
#endif
#if defined(ORRE_HAVE_WEBP)
    t += ", WebP";
#endif
    return tx("label.formats.list", {{"list", t}, {"last", "BMP"}});
}

std::vector<std::string> dialog_extensions() {
    std::vector<std::string> v = {"png", "bmp"};
#if defined(ORRE_HAVE_STB_IMAGE)
    v.insert(v.end(), {"jpg", "jpeg", "gif", "tga", "psd"});
#endif
#if defined(ORRE_HAVE_WEBP)
    v.push_back("webp");
#endif
    return v;
}

void load() {
    s_entries.clear();
    s_wear.clear();
    s_used = 0;
    const std::string dir = folder();
    size_t len = 0;
    if (char* text = static_cast<char*>(SDL_LoadFile((dir + "labels.txt").c_str(), &len))) {
        std::string all(text, len);
        SDL_free(text);
        size_t at = 0;
        while (at < all.size()) {
            size_t end = all.find('\n', at);
            if (end == std::string::npos) end = all.size();
            std::string line = all.substr(at, end - at);
            at = end + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            char game[64] = {0}, kind[16] = {0};
            float zoom = 0, x = 0, y = 0, rot = 0;
            unsigned long long used = 0;
            int name_at = 0;
            if (std::sscanf(line.c_str(), "wear %63s %n", game, &name_at) == 1 && name_at > 0) {
                s_wear[game] = line.substr((size_t)name_at);
            } else if ((std::sscanf(line.c_str(), "%15s %f %f %f %llu %n", kind, &zoom, &x, &y, &used, &name_at) == 5 &&
                        std::strcmp(kind, "picture") == 0 && name_at > 0) ||
                       (std::sscanf(line.c_str(), "%15s %f %f %f %f %llu %n", kind, &zoom, &x, &y, &rot, &used, &name_at) ==
                            6 && std::strcmp(kind, "picture2") == 0 && name_at > 0)) {
                Entry e;
                e.file = line.substr((size_t)name_at);
                e.place = {zoom, x, y, std::strcmp(kind, "picture2") == 0 ? rot : 0.0f};
                e.used = used;
                s_used = SDL_max(s_used, (uint64_t)used);
                if (!e.file.empty() && exists(dir + e.file) && find(e.file) == nullptr) {
                    s_entries.push_back(e); /* a picture that is gone is forgotten */
                }
            }
        }
    }
    /* pictures put in the folder by hand join the shelf (the newest by file time first) */
    int count = 0;
    if (char** names = SDL_GlobDirectory(dir.c_str(), "*", SDL_GLOB_CASEINSENSITIVE, &count)) {
        for (int i = 0; i < count; ++i) {
            const std::string name = names[i];
            /* the template itself (save_template) isn't a label; a picture made from it, saved under a new name, is */
            if (name.find_first_of("/\\") == std::string::npos && is_picture_name(name) && exists(dir + name) &&
                lower(name) != "template.png" && find(name) == nullptr) {
                Entry e;
                e.file = name;
                s_entries.push_back(e);
            }
        }
        SDL_free(names);
    }
    s_loaded = true;
}

void save() {
    const std::string dir = folder();
    SDL_CreateDirectory(dir.c_str());
    std::string text = "# OrrePort's disc labels: which label each game wears, and where each picture sits on the disc.\n"
                       "# Written by OrrePort; pictures put in this folder join the shelf by themselves.\n";
    for (const auto& [game, value] : s_wear) {
        text += "wear " + game + " " + value + "\n";
    }
    char line[160];
    for (const Entry& e : s_entries) {
        /* a turned picture is written as picture2 (with its angle); an unturned one keeps the first form, which an older
         * OrrePort still reads */
        if (e.place.rot != 0.0f)
            std::snprintf(line, sizeof(line), "picture2 %.3f %.3f %.3f %.3f %llu ", e.place.zoom, e.place.x, e.place.y,
                          e.place.rot, (unsigned long long)e.used);
        else
            std::snprintf(line, sizeof(line), "picture %.3f %.3f %.3f %llu ", e.place.zoom, e.place.x, e.place.y,
                          (unsigned long long)e.used);
        text += line + e.file + "\n";
    }
    const std::string tmp = dir + "labels.txt.new";
    if (SDL_SaveFile(tmp.c_str(), text.data(), text.size())) {
        SDL_RenamePath(tmp.c_str(), (dir + "labels.txt").c_str()); /* whole or not at all, as settings.ini */
    }
}

std::string wearing(const char* game) {
    if (!s_loaded) load();
    const auto it = s_wear.find(game);
    if (it == s_wear.end()) return "orreport";
    if (it->second.rfind("picture:", 0) == 0 && find(it->second.substr(8)) == nullptr) return "orreport"; /* gone */
    return it->second;
}

void set_wearing(const char* game, const std::string& value) {
    s_wear[game] = value;
    save();
}

namespace {
/* the size limits: 8 to 16,384 pixels a side (a 16K x 16K picture is 1 GB in memory while it is read) */
bool size_ok(int w, int h, std::string& why) {
    if (w > 16384 || h > 16384 || w < 8 || h < 8) {
        why = tx("label.error.size");
        return false;
    }
    return true;
}

/* SDL's own reader (PNG, BMP), the fallback when stb_image isn't built in */
[[maybe_unused]] bool decode_sdl(const std::string& path, Rgba& full, std::string& why) {
    SDL_Surface* loaded = SDL_LoadSurface(path.c_str());
    if (loaded == nullptr) {
        why = tx("label.error.read_why", {{"error", SDL_GetError()}});
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
    SDL_DestroySurface(loaded);
    if (rgba == nullptr) {
        why = tx("label.error.read");
        return false;
    }
    const bool ok = size_ok(rgba->w, rgba->h, why);
    if (ok) {
        full.w = rgba->w;
        full.h = rgba->h;
        full.px.resize((size_t)full.w * full.h * 4);
        for (int y = 0; y < full.h; ++y) {
            std::memcpy(&full.px[(size_t)y * full.w * 4],
                        static_cast<const unsigned char*>(rgba->pixels) + (size_t)y * rgba->pitch, (size_t)full.w * 4);
        }
    }
    SDL_DestroySurface(rgba);
    return ok;
}
} // namespace

bool decode(const std::string& path, Rgba& out, std::string& why) {
    if (!is_picture_name(path)) {
        why = tx("label.error.format", {{"formats", formats_text()}});
        return false;
    }
    Rgba full;
    size_t len = 0;
    unsigned char* data = static_cast<unsigned char*>(SDL_LoadFile(path.c_str(), &len));
    if (data == nullptr) {
        why = tx("label.error.open", {{"error", SDL_GetError()}});
        return false;
    }
    const bool webp = len >= 12 && std::memcmp(data, "RIFF", 4) == 0 && std::memcmp(data + 8, "WEBP", 4) == 0;
    bool ok = false;
    if (webp) {
#if defined(ORRE_HAVE_WEBP)
        int w = 0, h = 0;
        WebPBitstreamFeatures features;
        if (WebPGetFeatures(data, len, &features) != VP8_STATUS_OK) {
            why = tx("label.error.webp");
        } else if (features.has_animation) {
            why = tx("label.error.webp_moving");
        } else if (size_ok(features.width, features.height, why)) {
            if (uint8_t* rgba = WebPDecodeRGBA(data, len, &w, &h)) {
                full.w = w;
                full.h = h;
                full.px.assign(rgba, rgba + (size_t)w * h * 4);
                WebPFree(rgba);
                ok = true;
            } else {
                why = tx("label.error.webp");
            }
        }
#else
        why = tx("label.error.webp_build", {{"formats", formats_text()}});
#endif
    } else {
#if defined(ORRE_HAVE_STB_IMAGE)
        int w = 0, h = 0, n = 0;
        if (!stbi_info_from_memory(data, (int)SDL_min(len, (size_t)0x7FFFFFFF), &w, &h, &n)) {
            why = tx("label.error.read_why", {{"error", stbi_failure_reason()}});
        } else if (size_ok(w, h, why)) {
            if (stbi_uc* rgba = stbi_load_from_memory(data, (int)SDL_min(len, (size_t)0x7FFFFFFF), &w, &h, &n, 4)) {
                full.w = w;
                full.h = h;
                full.px.assign(rgba, rgba + (size_t)w * h * 4);
                stbi_image_free(rgba);
                ok = true;
            } else {
                why = tx("label.error.read_why", {{"error", stbi_failure_reason()}});
            }
        }
#else
        ok = decode_sdl(path, full, why);
#endif
    }
    SDL_free(data);
    if (!ok) {
        return false;
    }
    /* a very large picture is kept at 4096 pixels on its long side: plenty for the disc at 4K (2048 pixels across at
     * most, zoomed in up to 4 times) and 64 MB at most in memory */
    shrink(full, 4096, out);
    return true;
}

void shrink(const Rgba& in, int max_side, Rgba& out) {
    const int side = SDL_max(in.w, in.h);
    if (side <= max_side) {
        if (&out != &in) out = in;
        return;
    }
    const double f = (double)max_side / side;
    Rgba r;
    r.w = SDL_max(1, (int)std::lround(in.w * f));
    r.h = SDL_max(1, (int)std::lround(in.h * f));
    r.px.assign((size_t)r.w * r.h * 4, 0);
    /* each new pixel the average of the old ones under it (premultiplied, as in sample) */
    for (int y = 0; y < r.h; ++y) {
        const int y0 = (int)((double)y * in.h / r.h), y1 = SDL_max(y0 + 1, (int)((double)(y + 1) * in.h / r.h));
        for (int x = 0; x < r.w; ++x) {
            const int x0 = (int)((double)x * in.w / r.w), x1 = SDL_max(x0 + 1, (int)((double)(x + 1) * in.w / r.w));
            double sum[4] = {0, 0, 0, 0};
            for (int yy = y0; yy < y1 && yy < in.h; ++yy) {
                const unsigned char* p = &in.px[((size_t)yy * in.w + x0) * 4];
                for (int xx = x0; xx < x1 && xx < in.w; ++xx, p += 4) {
                    sum[0] += p[0] * p[3];
                    sum[1] += p[1] * p[3];
                    sum[2] += p[2] * p[3];
                    sum[3] += p[3];
                }
            }
            const double n = (double)(y1 - y0) * (x1 - x0);
            unsigned char* o = &r.px[((size_t)y * r.w + x) * 4];
            for (int c = 0; c < 3; ++c) o[c] = sum[3] > 0 ? (unsigned char)std::lround(sum[c] / sum[3]) : 0;
            o[3] = (unsigned char)std::lround(sum[3] / n);
        }
    }
    out = std::move(r);
}

std::string clean_name(const std::string& typed) {
    std::string n;
    for (char c : typed) {
        if (std::strchr("/\\:*?\"<>|", c) == nullptr && (unsigned char)c >= 0x20) n += c;
    }
    while (!n.empty() && (n.back() == ' ' || n.back() == '.')) n.pop_back();
    size_t start = 0;
    while (start < n.size() && (n[start] == ' ' || n[start] == '.')) ++start;
    n = n.substr(start);
    if (n.size() > 60) { /* by bytes; never cut inside a UTF-8 letter */
        size_t cut = 60;
        while (cut > 0 && ((unsigned char)n[cut] & 0xC0) == 0x80) --cut;
        n.resize(cut);
    }
    return n;
}

bool rename(const std::string& old_file, const std::string& name, std::string& file, std::string& why) {
    const std::string dir = folder();
    const std::string clean = clean_name(name);
    if (clean.empty()) {
        why = tx("label.error.name");
        return false;
    }
    if (lower(clean + ".png") == lower(old_file)) {
        file = old_file;
        return true;
    }
    file = free_name(dir, clean + ".png");
    if (!SDL_RenamePath((dir + old_file).c_str(), (dir + file).c_str())) {
        why = tx("label.error.rename", {{"error", SDL_GetError()}});
        return false;
    }
    if (Entry* e = find(old_file)) e->file = file;
    for (auto& [game, value] : s_wear) {
        if (value == "picture:" + old_file) value = "picture:" + file;
    }
    save();
    return true;
}

bool add(const std::string& source, std::string& file, std::string& why, const std::string& want) {
    if (!s_loaded) load();
    Rgba check;
    if (!decode(source, check, why)) {
        return false;
    }
    const std::string dir = folder();
    SDL_CreateDirectory(dir.c_str());
    const std::string original = base_name(source);
    const bool png = lower(original).size() > 4 && lower(original).compare(lower(original).size() - 4, 4, ".png") == 0;
    const std::string wanted = clean_name(want);
    const std::string name = wanted.empty() ? original : wanted + (png ? ".png" : "");
    /* already in the folder (a picture put there by hand, or picked from it), kept under its name: used where it is */
    if (wanted.empty() && exists(dir + name) && SDL_strcasecmp(source.c_str(), (dir + name).c_str()) == 0) {
        file = name;
    } else if (png) {
        file = free_name(dir, name);
        if (!SDL_CopyFile(source.c_str(), (dir + file).c_str())) {
            why = tx("label.error.copy", {{"error", SDL_GetError()}});
            return false;
        }
    } else {
        /* any other format is kept as a PNG copy (snatcherLeo's "accept as many and convert to PNG"), so the folder holds one
         * kind of file. Saved from what was read (at most 4096 a side, decode's limit): the original stays where it is */
        const size_t dot = original.find_last_of('.');
        const std::string stem = !wanted.empty() ? wanted : dot == std::string::npos ? original : original.substr(0, dot);
        file = free_name(dir, stem + ".png");
        SDL_Surface* s = SDL_CreateSurfaceFrom(check.w, check.h, SDL_PIXELFORMAT_RGBA32, check.px.data(), check.w * 4);
        const bool saved = s != nullptr && SDL_SavePNG(s, (dir + file).c_str());
        if (s != nullptr) SDL_DestroySurface(s);
        if (!saved) {
            why = tx("label.error.save", {{"error", SDL_GetError()}});
            return false;
        }
    }
    if (find(file) == nullptr) {
        Entry e;
        e.file = file;
        s_entries.push_back(e);
    }
    save();
    return true;
}

bool remove(const std::string& file) {
    const std::string dir = folder();
    const std::string removed = dir + "Removed/";
    SDL_CreateDirectory(removed.c_str());
    const std::string target = free_name(removed, file);
    if (!SDL_RenamePath((dir + file).c_str(), (removed + target).c_str())) {
        return false;
    }
    s_entries.erase(std::remove_if(s_entries.begin(), s_entries.end(), [&](const Entry& e) { return e.file == file; }),
                    s_entries.end());
    for (auto& [game, value] : s_wear) {
        if (value == "picture:" + file) value = "orreport";
    }
    save();
    return true;
}

float fill_scale(const Rgba& pic) { return 2.0f * kLabelRadius / (float)SDL_max(1, SDL_min(pic.w, pic.h)); }

float scale(const Rgba& pic, const Place& p) { return fill_scale(pic) * SDL_powf(4.0f, p.zoom / 100.0f); }

void clamp(const Rgba& pic, Place& p) {
    p.zoom = SDL_clamp(p.zoom, -100.0f, 100.0f);
    p.rot = SDL_fmodf(p.rot + 540.0f, 360.0f) - 180.0f; /* -180 .. 180 */
    const float s = scale(pic, p);
    /* the centre may move until the picture's nearer edge is a quarter of the label inside the disc (so it can't be
     * lost off the disc), measured on the turned picture's bounding box */
    const float c = SDL_fabsf(SDL_cosf(p.rot * SDL_PI_F / 180.0f)), sn = SDL_fabsf(SDL_sinf(p.rot * SDL_PI_F / 180.0f));
    const float hw = (pic.w * c + pic.h * sn) * s * 0.5f, hh = (pic.w * sn + pic.h * c) * s * 0.5f;
    const float reach = kLabelRadius * 0.75f;
    p.x = SDL_clamp(p.x, -(hw + reach), hw + reach);
    p.y = SDL_clamp(p.y, -(hh + reach), hh + reach);
}

float fit_zoom(const Rgba& pic) {
    /* the whole picture inside the disc at any turn: its half-diagonal no longer than the label's radius */
    const float half_diag = 0.5f * SDL_sqrtf((float)pic.w * pic.w + (float)pic.h * pic.h);
    const float want = kLabelRadius / SDL_max(1.0f, half_diag);
    return SDL_clamp(100.0f * SDL_logf(want / fill_scale(pic)) / SDL_logf(4.0f), -100.0f, 100.0f);
}

void compose_rows(const Rgba& pic, const Rgba& frame, Rgba& out, float unit, float s, float cx, float cy, float ct,
                  float st, int k, int y0, int y1); /* below: compose_picture's rows y0 .. y1 - 1 */

void compose_picture(const Rgba& pic, const Place& place, const Rgba& frame, Rgba& out) {
    const int n = frame.w;
    out.w = n;
    out.h = frame.h;
    out.px.assign((size_t)out.w * out.h * 4, 0);
    if (pic.px.empty() || n <= 0) {
        return;
    }
    Place p = place;
    clamp(pic, p);
    const float unit = 420.0f / n;          /* label units per output pixel */
    const float s = scale(pic, p);          /* label units per picture pixel */
    const float cx = 210.0f + p.x, cy = 210.0f + p.y;
    /* the turn: each output point is turned back by it to find its place in the picture (y grows downward, so a
     * positive angle turns the picture clockwise on screen) */
    const float ct = SDL_cosf(p.rot * SDL_PI_F / 180.0f), st = SDL_sinf(p.rot * SDL_PI_F / 180.0f);
    /* a picture much finer than the output (a big photo shown small) is averaged over k x k points per pixel, so it
     * doesn't shimmer; up to 4 x 4 */
    const int k = SDL_clamp((int)SDL_ceilf(unit / s - 0.01f), 1, 4);
    /* in eight bands of rows side by side (pc_parallel.h): each output pixel is written once and depends only on the
     * picture and the frame, so the bands never touch each other's pixels. The hub's start drew the worn art here on one
     * core (part of about 0.2 s, 2026-11-13) */
    constexpr int kBands = 8;
    std::vector<std::function<void()>> bands;
    for (int band = 0; band < kBands; ++band) {
        bands.push_back([&, band] { compose_rows(pic, frame, out, unit, s, cx, cy, ct, st, k, out.h * band / kBands,
                                                 out.h * (band + 1) / kBands); });
    }
    pc_run_together(bands);
}

void compose_rows(const Rgba& pic, const Rgba& frame, Rgba& out, float unit, float s, float cx, float cy, float ct,
                  float st, int k, int y0, int y1) {
    for (int py = y0; py < y1; ++py) {
        for (int px = 0; px < out.w; ++px) {
            const float X = (px + 0.5f) * unit - 210.0f, Y = (py + 0.5f) * unit - 210.0f;
            const float d = SDL_sqrtf(X * X + Y * Y);
            const float cov = circle_cover(d, unit);
            const size_t i = (size_t)py * out.w + px;
            if (cov <= 0.0f) {
                put(out, frame, i, 0, 0, 0, 0);
                continue;
            }
            float r = 0, g = 0, b = 0;
            for (int sy = 0; sy < k; ++sy) {
                for (int sx = 0; sx < k; ++sx) {
                    const float lx = (px + (sx + 0.5f) / k) * unit, ly = (py + (sy + 0.5f) / k) * unit;
                    const float vx = lx - cx, vy = ly - cy;
                    const float ux = ct * vx + st * vy, uy = -st * vx + ct * vy; /* turned back by the turn */
                    float c[4];
                    sample(pic, ux / s + pic.w * 0.5f - 0.5f, uy / s + pic.h * 0.5f - 0.5f, c);
                    r += c[0] * c[3] + kBlack * (1 - c[3]);
                    g += c[1] * c[3] + kBlack * (1 - c[3]);
                    b += c[2] * c[3] + kBlackB * (1 - c[3]);
                }
            }
            const float kk = (float)(k * k);
            put(out, frame, i, r / kk, g / kk, b / kk, cov);
        }
    }
}

void compose_banner(const unsigned char* banner, const Rgba& frame, Rgba& out) {
    const int n = frame.w;
    out.w = n;
    out.h = frame.h;
    out.px.assign((size_t)out.w * out.h * 4, 0);
    const float unit = 420.0f / n;
    /* the banner at twice its size in label units (192 x 64), centred across the upper half, clear of the clear
     * centre (which starts 79 units from the middle); its pixels kept sharp, as pixel art */
    const float bx0 = 210.0f - 96.0f, by0 = 95.0f - 32.0f, bs = 2.0f;
    for (int py = 0; py < out.h; ++py) {
        for (int px = 0; px < out.w; ++px) {
            const float lx = (px + 0.5f) * unit, ly = (py + 0.5f) * unit;
            const float X = lx - 210.0f, Y = ly - 210.0f;
            const float cov = circle_cover(SDL_sqrtf(X * X + Y * Y), unit);
            float r = kBlack, g = kBlack, b = kBlackB;
            const int bx = (int)SDL_floorf((lx - bx0) / bs), by = (int)SDL_floorf((ly - by0) / bs);
            if (banner != nullptr && bx >= 0 && bx < 96 && by >= 0 && by < 32) {
                const unsigned char* c = &banner[((size_t)by * 96 + bx) * 4];
                const float a = c[3] / 255.0f;
                r = c[0] / 255.0f * a + r * (1 - a);
                g = c[1] / 255.0f * a + g * (1 - a);
                b = c[2] / 255.0f * a + b * (1 - a);
            }
            put(out, frame, (size_t)py * out.w + px, r, g, b, cov);
        }
    }
}

bool save_template(std::string& path, std::string& why) {
    /* 1024 x 1024: at zoom 0 a square picture's side spans the printed circle's width (2 x 204 units), so the circle
     * touches the template's edges; the clear centre (79 units, shows the picture faintly through the plastic) and the
     * hole (39.5 units, never shown) are marked, the corners (never shown) grey */
    constexpr int kSize = 1024;
    const float k = kSize / (2.0f * kLabelRadius); /* pixels per label unit */
    SDL_Surface* s = SDL_CreateSurface(kSize, kSize, SDL_PIXELFORMAT_RGBA32);
    if (s == nullptr) {
        why = SDL_GetError();
        return false;
    }
    for (int y = 0; y < kSize; ++y) {
        unsigned char* row = static_cast<unsigned char*>(s->pixels) + (size_t)y * s->pitch;
        for (int x = 0; x < kSize; ++x) {
            const float X = (x + 0.5f) / k - kLabelRadius, Y = (y + 0.5f) / k - kLabelRadius;
            const float d = SDL_sqrtf(X * X + Y * Y);
            unsigned v = d > kLabelRadius ? 0xB4BCB7u                           /* outside: not shown */
                         : d < 39.5f      ? 0x5C6660u                           /* the hole: not shown */
                         : d < 79.0f      ? 0xDDE3DCu                           /* the clear centre: faint */
                                          : 0xFFFFFFu;                          /* the label */
            if (SDL_fabsf(d - kLabelRadius) < 1.2f / k * 2 || SDL_fabsf(d - 79.0f) < 1.0f / k * 2 ||
                SDL_fabsf(d - 39.5f) < 1.0f / k * 2) {
                v = 0xE8955Au; /* the guide lines, in OrrePort's orange */
            }
            unsigned char* p = row + x * 4;
            p[0] = (unsigned char)(v >> 16);
            p[1] = (unsigned char)(v >> 8);
            p[2] = (unsigned char)v;
            p[3] = 255;
        }
    }
    const std::string dir = folder();
    SDL_CreateDirectory(dir.c_str());
    path = dir + "Template.png";
    const bool ok = SDL_SavePNG(s, path.c_str());
    if (!ok) why = SDL_GetError();
    SDL_DestroySurface(s);
    return ok;
}

} // namespace disc_label
