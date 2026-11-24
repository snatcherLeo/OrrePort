/*
 * pc_grapheme.cpp: visible letters, Unicode's extended grapheme clusters (pc_grapheme.h says why and who uses it).
 * the project's build helper, 2026-11-16.
 *
 * Why no library and not HarfBuzz's own functions: HarfBuzz (in the build since the same day) gives a character's
 * general category and script, but not the three properties the rules need (Grapheme_Cluster_Break,
 * Extended_Pictographic, Indic_Conjunct_Break); ICU has them but is a large new library (none is added: snatcherLeo's rule
 * for this work). So, the way pc_rtl.cpp's tables were made, a small range table is generated from Unicode's own data
 * (tools/local/gen_grapheme_table.py: Unicode 17.0 from SheenBidi's copy of the database, Extended_Pictographic from
 * HarfBuzz's emoji table, both already fetched by the build): 833 ranges, about 10 KB, looked up by binary search.
 *
 * The rules (UAX #29 "Unicode Text Segmentation", section 3.1.1, rules GB3 to GB999), applied left to right; a letter
 * boundary falls between two characters unless a rule says "no break":
 *   GB3-5   CR LF stay together; a break before and after any other control character (CR, LF: irrelevant in names).
 *   GB6-8   Hangul: a leading consonant (L) joins L, V, LV, LVT; a vowel or LV syllable (V, LV) joins V, T; a trailing
 *           consonant or LVT syllable (T, LVT) joins T: a syllable typed as separate jamo is one letter.
 *   GB9     no break before Extend (combining marks, Hebrew points, Thai vowel marks, variation selectors, emoji skin
 *           tones, tags, the zero-width non-joiner) or the zero-width joiner.
 *   GB9a    no break before a SpacingMark (most Indic vowel signs that take room of their own).
 *   GB9b    no break after a Prepend character (a few Arabic number signs, the Malayalam dot reph and the like).
 *   GB9c    no break between consonants joined by a virama in the scripts Unicode lists (Indic_Conjunct_Break:
 *           Devanagari, Bengali, Gujarati, Oriya, Telugu, Malayalam, Khmer's coeng, Myanmar and others in Unicode 17):
 *           Consonant [Extend Linker]* Linker [Extend Linker]* x Consonant. So "स्ते" is one letter.
 *   GB11    an emoji, its Extends, a zero-width joiner and another emoji stay together (the family, the kiss...).
 *   GB12-13 regional indicators pair up: two make one flag, a third starts the next.
 *   GB999   a break everywhere else.
 *
 * How sure (labels as the project uses them):
 *   - verified: the Extended_Pictographic ranges equal HarfBuzz's own lookup, checked code point by code point
 *     (a C dump of _hb_emoji_is_Extended_Pictographic against the table, 2026-11-16); the test strings in
 *     naming-screen.md "Visible letters" give the counts the rules give.
 *   - likely: Grapheme_Cluster_Break is derived from Unicode 17.0's general categories and Grapheme_Extend by UAX #29's
 *     Table 2 (GraphemeBreakProperty.txt itself isn't among the build's files), with two short lists typed from the
 *     report (the SpacingMark exceptions, Prepend's Indic part); Hangul LV / LVT are computed from the syllable's
 *     number. Not covered: Unicode 16's Kirat Rai vowel signs as V (a script no OrrePort font draws).
 *
 * Platforms: plain C++ (no OS calls, no library): the same on Windows, Linux and macOS (likely: nothing can differ).
 */
#include "pc_grapheme.h"

namespace {

struct PcGraphemeRange {
    uint32_t first, last;
    uint8_t value;
};
#include "pc_grapheme_table.h"

/* Grapheme_Cluster_Break classes (the table's low 4 bits; tools/local/gen_grapheme_table.py's numbering) */
enum Gcb { OTHER, CR, LF, CONTROL, EXTEND, ZWJ, RI, PREPEND, SPACINGMARK, L, V, T, LV, LVT };
enum { EXTPICT = 0x10 };
enum Incb { INCB_NONE = 0, INCB_LINKER = 1, INCB_CONSONANT = 2, INCB_EXTEND = 3 };

struct Props {
    Gcb gcb;
    bool extpict;
    Incb incb;
};

Props props(uint32_t c) {
    if (c >= 0xAC00 && c <= 0xD7A3) /* Hangul syllables: LV every 28th (no trailing consonant), LVT the rest */
        return {(c - 0xAC00) % 28 == 0 ? LV : LVT, false, INCB_NONE};
    int lo = 0, hi = (int)(sizeof kGraphemeRanges / sizeof kGraphemeRanges[0]) - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        const PcGraphemeRange& r = kGraphemeRanges[mid];
        if (c < r.first) hi = mid - 1;
        else if (c > r.last) lo = mid + 1;
        else return {(Gcb)(r.value & 0x0F), (r.value & EXTPICT) != 0, (Incb)((r.value >> 5) & 3)};
    }
    return {OTHER, false, INCB_NONE};
}

/* What the rules remember while walking left to right */
struct Walk {
    bool started = false;
    Props prev{OTHER, false, INCB_NONE};
    int ri = 0;          /* regional indicators in a row just before (GB12-13) */
    int emoji = 0;       /* GB11: 1 after an emoji and its Extends, 2 after that and a zero-width joiner */
    int conjunct = 0;    /* GB9c: 1 after an InCB consonant (and Extends), 2 once a Linker followed it */

    /* true when a letter starts at a character with properties p (and records p) */
    bool step(const Props& p) {
        bool brk = true;
        if (!started) {
            brk = true;
        } else if (prev.gcb == CR && p.gcb == LF) {
            brk = false; /* GB3 */
        } else if (prev.gcb == CONTROL || prev.gcb == CR || prev.gcb == LF || p.gcb == CONTROL || p.gcb == CR || p.gcb == LF) {
            brk = true; /* GB4-5 */
        } else if (prev.gcb == L && (p.gcb == L || p.gcb == V || p.gcb == LV || p.gcb == LVT)) {
            brk = false; /* GB6 */
        } else if ((prev.gcb == LV || prev.gcb == V) && (p.gcb == V || p.gcb == T)) {
            brk = false; /* GB7 */
        } else if ((prev.gcb == LVT || prev.gcb == T) && p.gcb == T) {
            brk = false; /* GB8 */
        } else if (p.gcb == EXTEND || p.gcb == ZWJ || p.gcb == SPACINGMARK || prev.gcb == PREPEND) {
            brk = false; /* GB9, GB9a, GB9b */
        } else if (p.incb == INCB_CONSONANT && conjunct == 2) {
            brk = false; /* GB9c */
        } else if (p.extpict && prev.gcb == ZWJ && emoji == 2) {
            brk = false; /* GB11 */
        } else if (p.gcb == RI && prev.gcb == RI && (ri % 2) == 1) {
            brk = false; /* GB12-13 */
        }
        /* the remembered states, for the next character */
        if (p.extpict) emoji = 1;
        else if (emoji == 1 && p.gcb == EXTEND) emoji = 1;
        else if (emoji == 1 && p.gcb == ZWJ) emoji = 2;
        else emoji = 0;
        if (p.incb == INCB_CONSONANT) conjunct = 1;
        else if (conjunct != 0 && p.incb == INCB_LINKER) conjunct = 2;
        else if (conjunct != 0 && p.incb == INCB_EXTEND) { /* keeps its state */ }
        else conjunct = 0;
        ri = p.gcb == RI ? ri + 1 : 0;
        prev = p;
        started = true;
        return brk;
    }
};

uint32_t utf8_next(const unsigned char* s, size_t len, size_t* i) {
    const unsigned char b = s[*i];
    uint32_t c;
    int n;
    if (b < 0x80) c = b, n = 1;
    else if (b >= 0xF0) c = b & 0x07, n = 4;
    else if (b >= 0xE0) c = b & 0x0F, n = 3;
    else if (b >= 0xC0) c = b & 0x1F, n = 2;
    else c = 0xFFFD, n = 1; /* a stray continuation byte: its own letter */
    for (int k = 1; k < n && *i + k < len; ++k) c = (c << 6) | (s[*i + k] & 0x3F);
    *i += (size_t)n;
    return c;
}

} // namespace

extern "C" int pc_grapheme_starts(const uint32_t* cps, int n, uint8_t* starts) {
    Walk w;
    int count = 0;
    for (int i = 0; i < n; ++i) {
        const bool s = w.step(props(cps[i]));
        if (starts != nullptr) starts[i] = s ? 1 : 0;
        count += s ? 1 : 0;
    }
    return count;
}

extern "C" int pc_grapheme_count(const uint32_t* cps, int n) { return pc_grapheme_starts(cps, n, nullptr); }

extern "C" int pc_grapheme_last(const uint32_t* cps, int n) {
    Walk w;
    int last = 0;
    for (int i = 0; i < n; ++i)
        if (w.step(props(cps[i]))) last = i;
    return last;
}

extern "C" int pc_grapheme_append(const uint32_t* cps, int n, uint32_t c, int max_letters, int max_hidden) {
    /* Only the boundary before the new character can change (every rule looks back, none forward), so the walk over
     * the name as it is decides it */
    Walk w;
    int count = 0, last = 0;
    for (int i = 0; i < n; ++i)
        if (w.step(props(cps[i]))) count++, last = i;
    if (w.step(props(c))) return count + 1 > max_letters ? PC_GRAPHEME_FULL : PC_GRAPHEME_ADDED;
    /* it rides on the last letter: cps[last] is the base, n - 1 - last parts already ride on it */
    return n - 1 - last >= max_hidden ? PC_GRAPHEME_CAPPED : PC_GRAPHEME_ADDED;
}

extern "C" int pc_grapheme_utf8_count(const char* s, size_t len) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    Walk w;
    int count = 0;
    for (size_t i = 0; i < len && p[i] != 0;) count += w.step(props(utf8_next(p, len, &i))) ? 1 : 0;
    return count;
}

extern "C" size_t pc_grapheme_utf8_last(const char* s, size_t len) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(s);
    Walk w;
    size_t last = 0;
    for (size_t i = 0; i < len && p[i] != 0;) {
        const size_t at = i;
        if (w.step(props(utf8_next(p, len, &i)))) last = at;
    }
    return last;
}
