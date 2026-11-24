/*
 * pc_shape.cpp: names shaped by HarfBuzz in SheenBidi's order (pc_shape.h says why and the design). the project's build helper,
 * 2026-11-16.
 *
 * Platforms: plain C++ over HarfBuzz and SheenBidi (both static, built from source by port/CMakeLists.txt) and SDL for
 * the environment switch; Windows, Linux and macOS alike (likely). HarfBuzz maps each font file into memory on Windows
 * (hb_blob_create_from_file, CreateFileMapping); on Linux / macOS our build doesn't define HAVE_MMAP, so it reads the
 * file whole instead (likely: the same result, the file's size in memory once per font used for shaping).
 */
#include "pc_shape.h"

#include "pc_font_files.h"

#include <SDL3/SDL.h>
#include <SheenBidi/SheenBidi.h>
#include <hb.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::mutex s_lock; /* the game thread and the keyboard's drawing; HarfBuzz fonts are safe to share, our tables aren't */

/* One shaping font per Noto chain entry (pc_font_files.cpp's kChain), opened the first time a character of its script
 * is shaped. The same file, face, weight and width part D draws with (pc_font_render.cpp's draw_chain_face: a variable
 * font at the entry's wght with wdth 100), so HarfBuzz's glyph ids and places are the ones part D renders. */
struct ShapeFont {
    bool tried = false;
    hb_font_t* font = nullptr;
    int upem = 0;
};
std::vector<ShapeFont> s_fonts;

bool enabled() {
    static int s_on = -1;
    if (s_on < 0) {
        const char* e = SDL_getenv("ORRE_SHAPING"); /* a test switch, never a setting: 0 = off for this run */
        s_on = !(e != nullptr && e[0] == '0');
        if (!s_on) std::printf("[shape] ORRE_SHAPING=0: names drawn without HarfBuzz (pc_rtl.cpp's table pass)\n");
    }
    return s_on != 0;
}

hb_font_t* font_of(int chain, int* upem = nullptr) {
    if (s_fonts.empty()) s_fonts.resize((size_t)pc_noto_count());
    if (chain < 0 || chain >= (int)s_fonts.size()) return nullptr;
    ShapeFont& f = s_fonts[(size_t)chain];
    if (!f.tried) {
        f.tried = true;
        const PcNotoFont* nf = pc_noto_font(chain);
        char dir[1024];
        const std::string path = std::string(pc_font_dir(dir, sizeof dir)) + nf->file;
        if (hb_blob_t* blob = hb_blob_create_from_file_or_fail(path.c_str())) {
            hb_face_t* face = hb_face_create(blob, (unsigned)(nf->index < 0 ? 0 : nf->index));
            hb_blob_destroy(blob);
            f.upem = (int)hb_face_get_upem(face);
            f.font = hb_font_create(face); /* scale = the font's own units: places come out in font units */
            hb_face_destroy(face);
            if (nf->wght > 0) {
                const hb_variation_t v[2] = {{HB_TAG('w', 'g', 'h', 't'), nf->wght}, {HB_TAG('w', 'd', 't', 'h'), 100.f}};
                hb_font_set_variations(f.font, v, 2); /* an axis the font lacks is ignored */
            }
        }
        std::printf("[shape] %s %s\n", nf->what, f.font != nullptr ? "opened for shaping" : "not found (Fonts/ next to the exe)");
    }
    if (upem != nullptr) *upem = f.upem;
    return f.font;
}

bool has_glyph(hb_font_t* f, uint32_t cp) {
    hb_codepoint_t g = 0;
    return f != nullptr && hb_font_get_nominal_glyph(f, cp, &g) && g != 0;
}

/* The per-script fonts: chain entries asked only for their own script's code points (ranges), except Noto Sans Math
 * (symbols, never shaped). Noto Sans, CJK, the two symbol fonts and the emoji font have no ranges or are handled
 * apart: their letters draw one by one, as before. */
bool per_script(const PcNotoFont* f) {
    return f != nullptr && f->ranges != nullptr && !f->color && std::strcmp(f->what, "Noto Sans Math") != 0;
}

int emoji_chain() {
    for (int i = 0; i < pc_noto_count(); ++i)
        if (pc_noto_font(i)->color) return i;
    return -1;
}

/* Noto Sans (Latin, Greek, Cyrillic): the chain entry that shapes a letter of those scripts carrying combining marks
 * (*added 2026-11-16*, visible letters). Why: such a letter is one visible letter of the 12 (pc_grapheme.h), drawn in
 * one slot, so its marks must sit on it; drawn one by one they stood beside it, a pasted "Zalgo" name's marks spilling
 * out of the field. Keyboards type precomposed letters (é is one character), so this is rare in real names. */
int chain_named(const char* what) {
    for (int i = 0; i < pc_noto_count(); ++i)
        if (std::strcmp(pc_noto_font(i)->what, what) == 0) return i;
    return -1;
}
int latin_chain() { return chain_named("Noto Sans"); }

/* Hangul typed as conjoining jamo (*added 2026-11-16*, visible letters): a leading consonant (L) followed by a vowel (V)
 * and maybe a trailing consonant (T) is one syllable, one visible letter (pc_grapheme.h), so it is shaped as one with
 * Noto Sans CJK: HarfBuzz's Hangul shaper turns L V T into the precomposed syllable when the font has it. Drawn one by
 * one, the three jamo stood side by side across the neighbouring slots. Korean input methods commit precomposed
 * syllables (가-힣), so this is rare in real names. */
bool hangul_l(uint32_t cp) { return (cp >= 0x1100 && cp <= 0x115F) || (cp >= 0xA960 && cp <= 0xA97C); }
bool hangul_vt(uint32_t cp) { return (cp >= 0x1160 && cp <= 0x11FF) || (cp >= 0xD7B0 && cp <= 0xD7FB); }

/* the per-script font that draws cp (its ranges cover it and the file has the letter), -1 for none */
/* Kept per character: names are inserted into the game's text often (every message that names someone), and the answer
 * never changes in a run. Latin-1 and below never need a script font, so they skip the table altogether. */
int script_font(uint32_t cp) {
    if (cp < 0x0370) return -1;
    static std::unordered_map<uint32_t, int> s_known;
    if (const auto it = s_known.find(cp); it != s_known.end()) return it->second;
    int found = -1;
    for (int i = 0; i < pc_noto_count() && found < 0; ++i) {
        const PcNotoFont* f = pc_noto_font(i);
        if (!per_script(f) || !pc_noto_covers(f, cp)) continue;
        if (has_glyph(font_of(i), cp)) found = i;
    }
    s_known.emplace(cp, found);
    return found;
}

bool is_mark(uint32_t cp) {
    const hb_unicode_general_category_t c = hb_unicode_general_category(hb_unicode_funcs_get_default(), cp);
    return c == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK || c == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK ||
           c == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK;
}
/* what rides on the character before it inside a run: marks, the two joiners, emoji modifiers, selectors, tags */
bool rides(uint32_t cp) { return cp == 0x200C || cp == 0x200D || pc_text_is_modifier(cp) || is_mark(cp); }
/* a regional indicator (U+1F1E6-1F1FF): two in a row are one flag, one visible letter (pc_grapheme.h) */
bool regional(uint32_t cp) { return cp >= 0x1F1E6 && cp <= 0x1F1FF; }

bool rtl_script(uint32_t cp) {
    return hb_script_get_horizontal_direction(hb_unicode_script(hb_unicode_funcs_get_default(), cp)) == HB_DIRECTION_RTL;
}

/* A shaping segment: [start, end) of the name, shaped with chain font `chain`; chain -1: one plain character. */
struct Seg {
    int start, end, chain;
};

/* Splits the name into segments (pc_shape.h, step 2). Spaces and anything no per-script font has end a run: Arabic
 * joining never crosses a space, and an Indic syllable never holds one. */
std::vector<Seg> segments(const uint32_t* cps, int n) {
    std::vector<Seg> out;
    const int emoji = emoji_chain();
    for (int i = 0; i < n;) {
        int chain = script_font(cps[i]);
        /* an emoji with something riding on it (a skin tone, a selector, a joiner and the next emoji) is one sequence.
         * *Added 2026-11-16 (visible letters):* so is a pair of regional indicators, a flag: it counts as one letter of the
         * 12 (pc_grapheme.h), so it is drawn as one too (Noto Color Emoji's flag, joined by HarfBuzz), not as two letter
         * boxes. A third indicator starts the next pair (UAX #29 rules GB12-13) */
        const bool flag = regional(cps[i]) && i + 1 < n && regional(cps[i + 1]);
        const bool seq = chain < 0 && emoji >= 0 && i + 1 < n && ((rides(cps[i + 1]) && !rides(cps[i])) || flag) &&
                         has_glyph(font_of(emoji), cps[i]);
        /* a letter with no script font of its own (Latin, Greek, Cyrillic) followed by combining marks: shaped with Noto
         * Sans, the marks placed on it (latin_chain says why) */
        if (chain < 0 && !seq && i + 1 < n && is_mark(cps[i + 1]) && !rides(cps[i]) && !regional(cps[i])) {
            const int latin = latin_chain();
            if (latin >= 0 && has_glyph(font_of(latin), cps[i])) chain = latin;
        }
        /* conjoining jamo L V (T): one syllable, shaped with the CJK font (hangul_l says why) */
        bool jamo = false;
        if (chain < 0 && !seq && i + 1 < n && hangul_l(cps[i]) && hangul_vt(cps[i + 1])) {
            const int cjk = chain_named("Noto Sans CJK");
            if (cjk >= 0 && has_glyph(font_of(cjk), cps[i])) chain = cjk, jamo = true;
        }
        if (chain < 0 && !seq) {
            out.push_back({i, i + 1, -1});
            ++i;
            continue;
        }
        if (seq) chain = emoji;
        hb_font_t* f = font_of(chain);
        int j = i + 1;
        while (j < n) {
            const uint32_t d = cps[j];
            if (chain == emoji) {
                if (rides(d) || cps[j - 1] == 0x200D) { /* a modifier, or the emoji after a joiner */
                    ++j;
                    continue;
                }
                if (j == i + 1 && regional(cps[i]) && regional(d)) { /* a flag's second half */
                    ++j;
                    continue;
                }
                break;
            }
            if (script_font(d) == chain || (rides(d) && has_glyph(f, d))) {
                ++j;
                continue;
            }
            if (jamo && hangul_vt(d)) { /* the syllable's vowel and trailing consonant jamo (UAX #29 GB6-8) */
                ++j;
                continue;
            }
            break;
        }
        out.push_back({i, j, chain});
        i = j;
    }
    return out;
}

/* the cluster record kept behind its stand-in (pc_font_files.h): a PcShapedRun's header and its glyphs */
std::string record_of(const PcShapedRun& r) {
    std::string k = "SH1";
    k.append(reinterpret_cast<const char*>(&r), offsetof(PcShapedRun, glyph));
    k.append(reinterpret_cast<const char*>(r.glyph), sizeof(PcShapedGlyph) * (size_t)r.count);
    return k;
}

/* Shapes cps[start, end) with chain font `chain`, direction `rtl`, the whole name as context (Arabic joining looks past
 * the run's ends), and appends one item per cluster in drawn order. false: a glyph the font lacks, a cluster too big
 * for a record, or the stand-in table full. */
bool shape_piece(const uint32_t* cps, int n, int start, int end, int chain, bool rtl, std::vector<PcShapedItem>& items) {
    int upem = 0;
    hb_font_t* f = font_of(chain, &upem);
    if (f == nullptr) return false;
    hb_buffer_t* buf = hb_buffer_create();
    hb_buffer_add_codepoints(buf, cps, n, (unsigned)start, end - start);
    hb_buffer_set_direction(buf, rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(f, buf, nullptr, 0);
    unsigned count = 0;
    const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &count);
    const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, nullptr);
    /* each cluster's typed letters: from its value to the next larger cluster value (or the piece's end) */
    std::vector<int> starts;
    for (unsigned g = 0; g < count; ++g) starts.push_back((int)info[g].cluster);
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());
    /* HarfBuzz's clusters in drawn order, each with its glyphs placed from its own pen */
    struct Raw {
        std::vector<PcShapedGlyph> glyphs;
        int32_t advance = 0;
        int first = 0, last = 0;
    };
    std::vector<Raw> raw;
    bool ok = count > 0;
    for (unsigned g = 0; ok && g < count;) {
        const unsigned c = info[g].cluster;
        Raw r;
        const auto next = std::upper_bound(starts.begin(), starts.end(), (int)c);
        r.first = (int)c;
        r.last = (next != starts.end() ? *next : end) - 1;
        for (; g < count && info[g].cluster == c; ++g) {
            if (info[g].codepoint == 0) { /* the font lacks a letter of the run: let the caller fall back */
                ok = false;
                break;
            }
            r.glyphs.push_back({info[g].codepoint, r.advance + pos[g].x_offset, pos[g].y_offset});
            r.advance += pos[g].x_advance;
        }
        raw.push_back(std::move(r));
    }
    /* A cluster with no width of its own can't be a letter of its own in the game's text: the game would give it a box
     * beside its neighbour, not over it. Seen 2026-11-16 in Noto Sans Arabic's lam-alef, made of two glyphs in two
     * clusters, the alef's with no advance, drawn over the lam: as two letters the alef stood apart. So such a cluster
     * joins the neighbour its ink lies over: the one before it when its ink's middle is left of its pen (a mark
     * HarfBuzz kept apart), else the one after it (it then draws from the same pen). */
    std::vector<Raw> merged;
    for (size_t i = 0; ok && i < raw.size(); ++i) {
        Raw r = std::move(raw[i]);
        if (r.advance == 0 && !r.glyphs.empty()) {
            int32_t lo = INT32_MAX, hi = INT32_MIN;
            for (const PcShapedGlyph& gl : r.glyphs) {
                hb_glyph_extents_t ext{};
                if (!hb_font_get_glyph_extents(f, gl.gid, &ext) || ext.width == 0) continue;
                lo = std::min(lo, gl.x + ext.x_bearing);
                hi = std::max(hi, gl.x + ext.x_bearing + ext.width);
            }
            const bool left = lo <= hi && (int64_t)lo + hi < 0;
            if (left && !merged.empty()) {
                Raw& p = merged.back();
                for (PcShapedGlyph gl : r.glyphs) p.glyphs.push_back({gl.gid, gl.x + p.advance, gl.y});
                p.first = std::min(p.first, r.first), p.last = std::max(p.last, r.last);
                continue;
            }
            if (!left && i + 1 < raw.size()) {
                Raw& n2 = raw[i + 1];
                n2.glyphs.insert(n2.glyphs.begin(), r.glyphs.begin(), r.glyphs.end());
                n2.first = std::min(n2.first, r.first), n2.last = std::max(n2.last, r.last);
                continue;
            }
        }
        merged.push_back(std::move(r));
    }
    /* *Changed the same day (verified on the first Arabic picture):* the whole run is one letter for the game, not one per
     * cluster. Why: joined scripts draw strokes across cluster edges. Noto Sans Arabic's lam-alef is a lam and an alef
     * glyph in two clusters whose strokes cross; as two letters each was cut to its own box and the ligature fell apart
     * (the alef's diagonal stood alone), and a cursive join could open by a rounding pixel between boxes. Drawn as one
     * picture, the run is exactly what HarfBuzz laid out. A run is a word of one script (spaces end runs), so it stays
     * small; one wider than 6 em (132 pixels in the 22-pixel font) is cut at cluster edges, as a record's width is one
     * byte in the game's glyph format. The 12-letter count is untouched: it counts ~~typed letters~~ visible letters
     * (*corrected 2026-11-16*, pc_grapheme.h), from the name as typed, never from these drawn pieces. */
    std::vector<Raw> chunks;
    for (Raw& m : merged) {
        if (!chunks.empty() && chunks.back().advance + m.advance <= 6 * upem &&
            chunks.back().glyphs.size() + m.glyphs.size() <= (size_t)PC_SHAPE_MAX_GLYPHS) {
            Raw& c = chunks.back();
            for (const PcShapedGlyph& gl : m.glyphs) c.glyphs.push_back({gl.gid, gl.x + c.advance, gl.y});
            c.advance += m.advance;
            c.first = std::min(c.first, m.first), c.last = std::max(c.last, m.last);
            continue;
        }
        chunks.push_back(std::move(m));
    }
    for (const Raw& m : chunks) {
        if (!ok) break;
        if (m.glyphs.size() > (size_t)PC_SHAPE_MAX_GLYPHS) {
            ok = false;
            break;
        }
        PcShapedRun r;
        std::memset(&r, 0, sizeof r); /* the record is compared as bytes: no stray padding */
        r.chain = chain;
        r.first_cp = cps[m.first];
        r.upem = upem;
        r.advance = m.advance;
        r.count = (int)m.glyphs.size();
        std::copy(m.glyphs.begin(), m.glyphs.end(), r.glyph);
        const std::string key = record_of(r);
        const uint16_t code = pc_text_standin_cluster(key.data(), key.size(), r.first_cp);
        if (code == 0) {
            ok = false;
            break;
        }
        items.push_back({0, code, m.first, m.last});
    }
    hb_buffer_destroy(buf);
    return ok;
}

} // namespace

extern "C" int pc_shape_enabled(void) { return enabled() ? 1 : 0; }

extern "C" int pc_shape_needed(const uint32_t* cps, int n) {
    if (!enabled() || cps == nullptr) return 0;
    std::lock_guard<std::mutex> lock(s_lock);
    for (int i = 0; i < n; ++i) {
        if (rtl_script(cps[i]) || script_font(cps[i]) >= 0) return 1;
        if (i > 0 && rides(cps[i]) && !rides(cps[i - 1])) return 1; /* an emoji (or letter) with a modifier on it */
        if (i > 0 && regional(cps[i]) && regional(cps[i - 1])) return 1; /* a flag (2026-11-16) */
        if (i > 0 && hangul_vt(cps[i]) && hangul_l(cps[i - 1])) return 1; /* a syllable typed as jamo (2026-11-16) */
    }
    return 0;
}

/* SheenBidi's levels for the name (Unicode's bidi algorithm, rules P2 to L2 for one paragraph: the name's direction from
 * its first strong letter, numbers and neutrals resolved, explicit embeddings and isolates honoured). */
extern "C" int pc_shape_base_rtl(const uint32_t* cps, int n) {
    if (cps == nullptr || n <= 0) return 0;
    const SBCodepointSequence seq = {SBStringEncodingUTF32, cps, (SBUInteger)n};
    SBAlgorithmRef algo = SBAlgorithmCreate(&seq);
    if (algo == nullptr) return 0;
    SBParagraphRef para = SBAlgorithmCreateParagraph(algo, 0, (SBUInteger)n, SBLevelDefaultLTR);
    const int rtl = para != nullptr && (SBParagraphGetBaseLevel(para) & 1) != 0;
    if (para != nullptr) SBParagraphRelease(para);
    SBAlgorithmRelease(algo);
    return rtl;
}

extern "C" int pc_shape_name(const uint32_t* cps, int n, PcShapedItem* out, int max) {
    if (!enabled() || cps == nullptr || n <= 0 || out == nullptr) return -1;
    std::lock_guard<std::mutex> lock(s_lock);
    const std::vector<Seg> segs = segments(cps, n);
    /* the bidi runs in drawn order (SheenBidi's line: rule L2's reordering done for us) */
    const SBCodepointSequence seq = {SBStringEncodingUTF32, cps, (SBUInteger)n};
    SBAlgorithmRef algo = SBAlgorithmCreate(&seq);
    if (algo == nullptr) return -1;
    SBParagraphRef para = SBAlgorithmCreateParagraph(algo, 0, (SBUInteger)n, SBLevelDefaultLTR);
    SBLineRef line = para != nullptr ? SBParagraphCreateLine(para, 0, SBParagraphGetLength(para)) : nullptr;
    std::vector<PcShapedItem> items;
    bool ok = line != nullptr;
    if (ok) {
        const SBRun* runs = SBLineGetRunsPtr(line);
        const SBUInteger runCount = SBLineGetRunCount(line);
        for (SBUInteger r = 0; ok && r < runCount; ++r) {
            const int a = (int)runs[r].offset, b = (int)(runs[r].offset + runs[r].length);
            const bool rtl = (runs[r].level & 1) != 0;
            /* the segments inside this bidi run, clipped to it */
            std::vector<Seg> pieces;
            for (const Seg& s : segs) {
                const int lo = std::max(s.start, a), hi = std::min(s.end, b);
                if (lo < hi) pieces.push_back({lo, hi, s.chain});
            }
            if (rtl) std::reverse(pieces.begin(), pieces.end());
            for (const Seg& p : pieces) {
                if (p.chain >= 0) {
                    ok = shape_piece(cps, n, p.start, p.end, p.chain, rtl, items);
                    if (!ok) break;
                    continue;
                }
                /* a plain character; in a right-to-left run its mirror (rule L4: brackets, < >) */
                uint32_t cp = cps[p.start];
                if (rtl) cp = hb_unicode_mirroring(hb_unicode_funcs_get_default(), cp);
                items.push_back({cp, 0, p.start, p.start});
            }
        }
    }
    if (line != nullptr) SBLineRelease(line);
    if (para != nullptr) SBParagraphRelease(para);
    SBAlgorithmRelease(algo);
    if (!ok || (int)items.size() > max) {
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            std::printf("[shape] a name couldn't be shaped (a letter its font lacks, or too long): pc_rtl.cpp's pass instead\n");
        }
        return -1;
    }
    std::copy(items.begin(), items.end(), out);
    return (int)items.size();
}

extern "C" int pc_shape_cluster(uint16_t code, PcShapedRun* out) {
    char buf[3 + sizeof(PcShapedRun)];
    const size_t len = pc_text_cluster_key(code, buf, sizeof buf);
    if (len < 3 + offsetof(PcShapedRun, glyph) || std::memcmp(buf, "SH1", 3) != 0 || out == nullptr) return 0;
    *out = PcShapedRun{};
    std::memcpy(out, buf + 3, offsetof(PcShapedRun, glyph));
    const size_t glyphs = (len - 3 - offsetof(PcShapedRun, glyph)) / sizeof(PcShapedGlyph);
    if (out->count < 0 || out->count > PC_SHAPE_MAX_GLYPHS || (size_t)out->count != glyphs) return 0;
    std::memcpy(out->glyph, buf + 3 + offsetof(PcShapedRun, glyph), sizeof(PcShapedGlyph) * glyphs);
    return 1;
}
