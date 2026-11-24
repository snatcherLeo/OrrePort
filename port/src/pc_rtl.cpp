/*
 * pc_rtl.cpp: right-to-left names drawn as they are read: Arabic joining, Hebrew precomposed letters and a simple
 * bidi pass (pc_rtl.h says what each is for and who calls it). the project's build helper, 2026-11-16.
 *
 * Why no library: HarfBuzz is not in the build (translation.md, checked in the CMake cache), adding it needs snatcherLeo's
 * approval, and names alone don't need it: Unicode keeps a 16-bit presentation form for every joined shape of the
 * Arabic letters a name uses, and the shipped Noto Sans Arabic and Noto Sans Hebrew draw all 772 Arabic and every
 * Hebrew presentation form (verified from the files' character maps, 2026-11-16).
 *
 * How the joining works (Unicode's ArabicShaping rules, cut down to names):
 *   - Each character has a joining type: dual (joins on both sides: beh, seen, lam...), right (joins only to the letter
 *     before it: alef, dal, reh, waw...), join-causing (tatweel, the zero-width joiner), non-joining (everything
 *     else, the zero-width non-joiner) or transparent (marks: they are skipped when looking for a neighbour, so a
 *     haraka between two letters never breaks the word's joining). A letter's type is read from which presentation
 *     forms it has (initial and medial: dual; only final: right), so the table below is the whole data.
 *   - A letter joins the one before it when that one is dual or join-causing, and the one after it when it is dual
 *     itself and the next joins back (right, dual or join-causing). Joined both ways: medial; only before: final;
 *     only after: initial; neither: isolated.
 *   - Lam followed by alef (plain, madda, hamza above or below) is one ligature, isolated or final; Arabic writing
 *     requires it (a lam and an alef side by side read as wrong).
 *   - The harakat fathatan, dammatan, kasratan, fatha, damma, kasra, shadda and sukun become their spacing forms
 *     (U+FE70-FE7F): where the word joins through the mark, the form drawn on a tatweel (a short joining stroke with the
 *     mark above or below), else the mark alone. Why spacing forms: the game's text draws every character as its own
 *     letter beside the last, so a mark that sits over its letter can't be placed; on a tatweel it keeps the word joined
 *     and the vowel visible.
 * Hebrew: a letter followed by dagesh, a shin or sin dot, rafe, or (for alef, yod, vav) its one precomposed vowel
 * becomes the precomposed letter (U+FB1D-FB4F), so the most common pointed letters draw as one letter. Hebrew letters
 * don't join.
 *
 * How the bidi pass works (the Unicode Bidirectional Algorithm's rules W1-W7, N1-N2, I1-I2 and L1-L4, for one name
 * on its own, as if isolated from the text around it):
 *   - Each character gets a class: strong left-to-right (Latin, kana, kanji...), strong right-to-left (Hebrew;
 *     Arabic is its own class), European digits, Arabic-Indic digits, number separators and terminators (+ - , . : / %
 *     $ ...), marks, spaces and other neutrals (punctuation, symbols, emoji).
 *   - The name's direction is its first strong letter's (none: left to right). So "Ali" reads from the left and "سلام"
 *     from the right, wherever the name is inserted.
 *   - Marks take their letter's class; digits after an Arabic letter are Arabic numbers; a single + - between digits
 *     or , . : / between digits joins the number; % $ and the like next to digits join it; digits after a Latin letter
 *     are Latin; neutrals between two letters of one direction take it, else the name's direction.
 *   - Levels: in a left-to-right name, right-to-left letters are level 1 and numbers among them level 2; in a
 *     right-to-left name, right-to-left letters are 1 and Latin and digits 2. Then every run at level 2, and then every
 *     run at level 1 or more, is reversed: so "سلام 123" draws as "123 مالس" (read from the right: the word, a space,
 *     one hundred and twenty-three) and the digits keep their order. Trailing spaces stay at the name's own end.
 *   - Brackets and the like are mirrored inside right-to-left runs, so "(" reads as an opening bracket there.
 *
 * What it doesn't cover (the full algorithm and shaping do; listed so nobody counts on it):
 *   - Explicit embeddings, overrides and isolates (U+202A-202E, U+2066-2069) are dropped and ignored; LRM / RLM / ALM
 *     count as strong letters of their direction and are then dropped.
 *   - Bracket pairs aren't matched (rule N0): a bracket takes its direction from its neighbours like any neutral.
 *   - One name at a time: the sentence around an inserted name is not reordered (an English or Japanese message stays
 *     left to right with the name as one block inside it). A right-to-left translation of the game's own text needs the
 *     full algorithm on whole lines and line breaking that respects it: HarfBuzz and a bidi library (later, snatcherLeo's
 *     approval).
 *   - Arabic letters with no presentation form in Unicode (most of the Arabic Supplement and Extended blocks, U+0750
 *     up) draw in their standalone shape and don't join (non-joining for their neighbours too); Syriac, N'Ko, Mandaic
 *     and Adlam are reordered but not joined. Marks other than the eight harakat (superscript alef, maddah, Quranic
 *     marks, most Hebrew vowel points) have no spacing form: they draw as their own small letter (pc_font_render.cpp
 *     moves a zero-width mark's ink into its own box) beside their letter.
 *   - No kashida justification, no optional ligatures beyond lam-alef, no Indic / Thai / Khmer / Myanmar shaping.
 *
 * Platforms: pure C++ (no OS calls, no library): Windows, Linux and macOS the same (likely: nothing can differ).
 * XD: port code with no game knowledge; it serves XD's names unchanged (likely).
 */
#include "pc_rtl.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace {

/* ---- the Arabic presentation forms: base letter, isolated, final, initial, medial (0: none). Generated 2026-11-16 from
 * Unicode 15.0's own data (Python's unicodedata: every U+FB50-FDFF / U+FE70-FEFF character whose compatibility
 * decomposition is <isolated>, <final>, <initial> or <medial> of one Arabic letter), sorted by the base letter ---- */
struct Forms {
    uint16_t base, iso, fin, ini, med;
};
const Forms kForms[] = {
    {0x0621, 0xFE80, 0x0000, 0x0000, 0x0000}, /* Hamza */
    {0x0622, 0xFE81, 0xFE82, 0x0000, 0x0000}, /* Alef With Madda Above */
    {0x0623, 0xFE83, 0xFE84, 0x0000, 0x0000}, /* Alef With Hamza Above */
    {0x0624, 0xFE85, 0xFE86, 0x0000, 0x0000}, /* Waw With Hamza Above */
    {0x0625, 0xFE87, 0xFE88, 0x0000, 0x0000}, /* Alef With Hamza Below */
    {0x0626, 0xFE89, 0xFE8A, 0xFE8B, 0xFE8C}, /* Yeh With Hamza Above */
    {0x0627, 0xFE8D, 0xFE8E, 0x0000, 0x0000}, /* Alef */
    {0x0628, 0xFE8F, 0xFE90, 0xFE91, 0xFE92}, /* Beh */
    {0x0629, 0xFE93, 0xFE94, 0x0000, 0x0000}, /* Teh Marbuta */
    {0x062A, 0xFE95, 0xFE96, 0xFE97, 0xFE98}, /* Teh */
    {0x062B, 0xFE99, 0xFE9A, 0xFE9B, 0xFE9C}, /* Theh */
    {0x062C, 0xFE9D, 0xFE9E, 0xFE9F, 0xFEA0}, /* Jeem */
    {0x062D, 0xFEA1, 0xFEA2, 0xFEA3, 0xFEA4}, /* Hah */
    {0x062E, 0xFEA5, 0xFEA6, 0xFEA7, 0xFEA8}, /* Khah */
    {0x062F, 0xFEA9, 0xFEAA, 0x0000, 0x0000}, /* Dal */
    {0x0630, 0xFEAB, 0xFEAC, 0x0000, 0x0000}, /* Thal */
    {0x0631, 0xFEAD, 0xFEAE, 0x0000, 0x0000}, /* Reh */
    {0x0632, 0xFEAF, 0xFEB0, 0x0000, 0x0000}, /* Zain */
    {0x0633, 0xFEB1, 0xFEB2, 0xFEB3, 0xFEB4}, /* Seen */
    {0x0634, 0xFEB5, 0xFEB6, 0xFEB7, 0xFEB8}, /* Sheen */
    {0x0635, 0xFEB9, 0xFEBA, 0xFEBB, 0xFEBC}, /* Sad */
    {0x0636, 0xFEBD, 0xFEBE, 0xFEBF, 0xFEC0}, /* Dad */
    {0x0637, 0xFEC1, 0xFEC2, 0xFEC3, 0xFEC4}, /* Tah */
    {0x0638, 0xFEC5, 0xFEC6, 0xFEC7, 0xFEC8}, /* Zah */
    {0x0639, 0xFEC9, 0xFECA, 0xFECB, 0xFECC}, /* Ain */
    {0x063A, 0xFECD, 0xFECE, 0xFECF, 0xFED0}, /* Ghain */
    {0x0641, 0xFED1, 0xFED2, 0xFED3, 0xFED4}, /* Feh */
    {0x0642, 0xFED5, 0xFED6, 0xFED7, 0xFED8}, /* Qaf */
    {0x0643, 0xFED9, 0xFEDA, 0xFEDB, 0xFEDC}, /* Kaf */
    {0x0644, 0xFEDD, 0xFEDE, 0xFEDF, 0xFEE0}, /* Lam */
    {0x0645, 0xFEE1, 0xFEE2, 0xFEE3, 0xFEE4}, /* Meem */
    {0x0646, 0xFEE5, 0xFEE6, 0xFEE7, 0xFEE8}, /* Noon */
    {0x0647, 0xFEE9, 0xFEEA, 0xFEEB, 0xFEEC}, /* Heh */
    {0x0648, 0xFEED, 0xFEEE, 0x0000, 0x0000}, /* Waw */
    {0x0649, 0xFEEF, 0xFEF0, 0xFBE8, 0xFBE9}, /* Alef Maksura */
    {0x064A, 0xFEF1, 0xFEF2, 0xFEF3, 0xFEF4}, /* Yeh */
    {0x0671, 0xFB50, 0xFB51, 0x0000, 0x0000}, /* Alef Wasla */
    {0x0677, 0xFBDD, 0x0000, 0x0000, 0x0000}, /* U With Hamza Above */
    {0x0679, 0xFB66, 0xFB67, 0xFB68, 0xFB69}, /* Tteh */
    {0x067A, 0xFB5E, 0xFB5F, 0xFB60, 0xFB61}, /* Tteheh */
    {0x067B, 0xFB52, 0xFB53, 0xFB54, 0xFB55}, /* Beeh */
    {0x067E, 0xFB56, 0xFB57, 0xFB58, 0xFB59}, /* Peh */
    {0x067F, 0xFB62, 0xFB63, 0xFB64, 0xFB65}, /* Teheh */
    {0x0680, 0xFB5A, 0xFB5B, 0xFB5C, 0xFB5D}, /* Beheh */
    {0x0683, 0xFB76, 0xFB77, 0xFB78, 0xFB79}, /* Nyeh */
    {0x0684, 0xFB72, 0xFB73, 0xFB74, 0xFB75}, /* Dyeh */
    {0x0686, 0xFB7A, 0xFB7B, 0xFB7C, 0xFB7D}, /* Tcheh */
    {0x0687, 0xFB7E, 0xFB7F, 0xFB80, 0xFB81}, /* Tcheheh */
    {0x0688, 0xFB88, 0xFB89, 0x0000, 0x0000}, /* Ddal */
    {0x068C, 0xFB84, 0xFB85, 0x0000, 0x0000}, /* Dahal */
    {0x068D, 0xFB82, 0xFB83, 0x0000, 0x0000}, /* Ddahal */
    {0x068E, 0xFB86, 0xFB87, 0x0000, 0x0000}, /* Dul */
    {0x0691, 0xFB8C, 0xFB8D, 0x0000, 0x0000}, /* Rreh */
    {0x0698, 0xFB8A, 0xFB8B, 0x0000, 0x0000}, /* Jeh */
    {0x06A4, 0xFB6A, 0xFB6B, 0xFB6C, 0xFB6D}, /* Veh */
    {0x06A6, 0xFB6E, 0xFB6F, 0xFB70, 0xFB71}, /* Peheh */
    {0x06A9, 0xFB8E, 0xFB8F, 0xFB90, 0xFB91}, /* Keheh */
    {0x06AD, 0xFBD3, 0xFBD4, 0xFBD5, 0xFBD6}, /* Ng */
    {0x06AF, 0xFB92, 0xFB93, 0xFB94, 0xFB95}, /* Gaf */
    {0x06B1, 0xFB9A, 0xFB9B, 0xFB9C, 0xFB9D}, /* Ngoeh */
    {0x06B3, 0xFB96, 0xFB97, 0xFB98, 0xFB99}, /* Gueh */
    {0x06BA, 0xFB9E, 0xFB9F, 0x0000, 0x0000}, /* Noon Ghunna */
    {0x06BB, 0xFBA0, 0xFBA1, 0xFBA2, 0xFBA3}, /* Rnoon */
    {0x06BE, 0xFBAA, 0xFBAB, 0xFBAC, 0xFBAD}, /* Heh Doachashmee */
    {0x06C0, 0xFBA4, 0xFBA5, 0x0000, 0x0000}, /* Heh With Yeh Above */
    {0x06C1, 0xFBA6, 0xFBA7, 0xFBA8, 0xFBA9}, /* Heh Goal */
    {0x06C5, 0xFBE0, 0xFBE1, 0x0000, 0x0000}, /* Kirghiz Oe */
    {0x06C6, 0xFBD9, 0xFBDA, 0x0000, 0x0000}, /* Oe */
    {0x06C7, 0xFBD7, 0xFBD8, 0x0000, 0x0000}, /* U */
    {0x06C8, 0xFBDB, 0xFBDC, 0x0000, 0x0000}, /* Yu */
    {0x06C9, 0xFBE2, 0xFBE3, 0x0000, 0x0000}, /* Kirghiz Yu */
    {0x06CB, 0xFBDE, 0xFBDF, 0x0000, 0x0000}, /* Ve */
    {0x06CC, 0xFBFC, 0xFBFD, 0xFBFE, 0xFBFF}, /* Farsi Yeh */
    {0x06D0, 0xFBE4, 0xFBE5, 0xFBE6, 0xFBE7}, /* E */
    {0x06D2, 0xFBAE, 0xFBAF, 0x0000, 0x0000}, /* Yeh Barree */
    {0x06D3, 0xFBB0, 0xFBB1, 0x0000, 0x0000}, /* Yeh Barree With Hamza Above */
};

/* lam + alef: the alef, the ligature's isolated and final forms (the same Unicode data: <isolated> / <final> 0644 +
 * the alef) */
struct LamAlef {
    uint16_t alef, iso, fin;
};
const LamAlef kLamAlef[] = {
    {0x0622, 0xFEF5, 0xFEF6}, /* lam + alef with madda above */
    {0x0623, 0xFEF7, 0xFEF8}, /* lam + alef with hamza above */
    {0x0625, 0xFEF9, 0xFEFA}, /* lam + alef with hamza below */
    {0x0627, 0xFEFB, 0xFEFC}, /* lam + alef */
};

/* the harakat with spacing forms: the mark, its form alone, its form on a tatweel (0: none; Unicode has the tatweel form
 * of six of the eight) */
struct Haraka {
    uint16_t mark, iso, med;
};
const Haraka kHarakat[] = {
    {0x064B, 0xFE70, 0xFE71}, /* fathatan */
    {0x064C, 0xFE72, 0x0000}, /* dammatan */
    {0x064D, 0xFE74, 0x0000}, /* kasratan */
    {0x064E, 0xFE76, 0xFE77}, /* fatha */
    {0x064F, 0xFE78, 0xFE79}, /* damma */
    {0x0650, 0xFE7A, 0xFE7B}, /* kasra */
    {0x0651, 0xFE7C, 0xFE7D}, /* shadda */
    {0x0652, 0xFE7E, 0xFE7F}, /* sukun */
};

/* Hebrew precomposed letters: base, first mark, second mark (0: none), the precomposed form. Generated the same day
 * from Unicode 15.0's canonical decompositions of U+FB1D-FB4F (FB2C / FB2D decomposed fully) */
struct HebComp {
    uint16_t base, m1, m2, out;
};
const HebComp kHebrew[] = {
    {0x05D9, 0x05B4, 0, 0xFB1D}, {0x05F2, 0x05B7, 0, 0xFB1F}, {0x05E9, 0x05C1, 0, 0xFB2A}, {0x05E9, 0x05C2, 0, 0xFB2B},
    {0x05E9, 0x05BC, 0x05C1, 0xFB2C}, {0x05E9, 0x05BC, 0x05C2, 0xFB2D}, {0x05D0, 0x05B7, 0, 0xFB2E},
    {0x05D0, 0x05B8, 0, 0xFB2F}, {0x05D0, 0x05BC, 0, 0xFB30}, {0x05D1, 0x05BC, 0, 0xFB31}, {0x05D2, 0x05BC, 0, 0xFB32},
    {0x05D3, 0x05BC, 0, 0xFB33}, {0x05D4, 0x05BC, 0, 0xFB34}, {0x05D5, 0x05BC, 0, 0xFB35}, {0x05D6, 0x05BC, 0, 0xFB36},
    {0x05D8, 0x05BC, 0, 0xFB38}, {0x05D9, 0x05BC, 0, 0xFB39}, {0x05DA, 0x05BC, 0, 0xFB3A}, {0x05DB, 0x05BC, 0, 0xFB3B},
    {0x05DC, 0x05BC, 0, 0xFB3C}, {0x05DE, 0x05BC, 0, 0xFB3E}, {0x05E0, 0x05BC, 0, 0xFB40}, {0x05E1, 0x05BC, 0, 0xFB41},
    {0x05E3, 0x05BC, 0, 0xFB43}, {0x05E4, 0x05BC, 0, 0xFB44}, {0x05E6, 0x05BC, 0, 0xFB46}, {0x05E7, 0x05BC, 0, 0xFB47},
    {0x05E8, 0x05BC, 0, 0xFB48}, {0x05E9, 0x05BC, 0, 0xFB49}, {0x05EA, 0x05BC, 0, 0xFB4A}, {0x05D5, 0x05B9, 0, 0xFB4B},
    {0x05D1, 0x05BF, 0, 0xFB4C}, {0x05DB, 0x05BF, 0, 0xFB4D}, {0x05E4, 0x05BF, 0, 0xFB4E},
};

/* combining marks (Unicode's Mn / Me): the Hebrew and Arabic blocks' own (generated the same day from Unicode 15.0),
 * plus the general combining blocks a name may carry */
struct Range {
    uint32_t a, b;
};
const Range kMarks[] = {
    {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x05BF, 0x05BF}, {0x05C1, 0x05C2}, {0x05C4, 0x05C5},
    {0x05C7, 0x05C7}, {0x0610, 0x061A}, {0x064B, 0x065F}, {0x0670, 0x0670}, {0x06D6, 0x06DC}, {0x06DF, 0x06E4},
    {0x06E7, 0x06E8}, {0x06EA, 0x06ED}, {0x0711, 0x0711}, {0x0730, 0x074A}, {0x07A6, 0x07B0}, {0x07EB, 0x07F3},
    {0x07FD, 0x07FD}, {0x0816, 0x0819}, {0x081B, 0x0823}, {0x0825, 0x0827}, {0x0829, 0x082D}, {0x0859, 0x085B},
    {0x0898, 0x089F}, {0x08CA, 0x08E1}, {0x08E3, 0x08FF}, {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0x20D0, 0x20F0},
    {0xFB1E, 0xFB1E}, {0xFE20, 0xFE2F}, {0x10D24, 0x10D27}, {0x1E944, 0x1E94A},
};

bool in_ranges(uint32_t c, const Range* r, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (c >= r[i].a && c <= r[i].b) return true;
    return false;
}
bool is_mark(uint32_t c) { return in_ranges(c, kMarks, sizeof(kMarks) / sizeof(kMarks[0])); }

const Forms* forms_of(uint32_t c) {
    if (c < 0x0621 || c > 0x06D3) return nullptr;
    const Forms* end = kForms + sizeof(kForms) / sizeof(kForms[0]);
    const Forms* f = std::lower_bound(kForms, end, c, [](const Forms& a, uint32_t v) { return a.base < v; });
    return (f != end && f->base == c) ? f : nullptr;
}
const LamAlef* lam_alef(uint32_t alef) {
    for (const LamAlef& l : kLamAlef)
        if (l.alef == alef) return &l;
    return nullptr;
}
const Haraka* haraka(uint32_t c) {
    for (const Haraka& h : kHarakat)
        if (h.mark == c) return &h;
    return nullptr;
}

/* invisible format characters: the joiners, the direction marks and controls, the zero-width space and no-break
 * space. They are never drawn (they have no picture, and a name slot would show nothing); the joiners and direction
 * marks still count where they matter (joining, the name's direction) before they are dropped */
bool invisible(uint32_t c) {
    return c == 0x200B || c == 0x200C || c == 0x200D || c == 0x200E || c == 0x200F || c == 0x061C || c == 0xFEFF ||
           (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069);
}

/* ---- joining types ---- */
enum Join { JU, JR, JD, JC, JT };
Join join_type(uint32_t c) {
    if (c == 0x200D || c == 0x0640) return JC;      /* the zero-width joiner and tatweel join on both sides */
    if (c == 0x200C) return JU;                     /* the zero-width non-joiner stops it */
    if (is_mark(c) || invisible(c)) return JT;      /* marks and other format characters are transparent */
    if (const Forms* f = forms_of(c)) return (f->ini && f->med) ? JD : f->fin ? JR : JU;
    return JU;
}

/* ---- bidi classes ---- */
enum Cls { L, R, AL, EN, AN, ES, ET, CS, NSM, WS, ON };

Cls bidi_class(uint32_t c) {
    if (c == 0x200E) return L;  /* LRM */
    if (c == 0x200F) return R;  /* RLM */
    if (c == 0x061C) return AL; /* ALM */
    if (is_mark(c)) return NSM;
    /* digits */
    if ((c >= '0' && c <= '9') || c == 0xB2 || c == 0xB3 || c == 0xB9 || c == 0x2070 || (c >= 0x2074 && c <= 0x2079) ||
        (c >= 0x2080 && c <= 0x2089) || (c >= 0xFF10 && c <= 0xFF19) || (c >= 0x06F0 && c <= 0x06F9) ||
        (c >= 0x1D7CE && c <= 0x1D7FF) || (c >= 0x1F100 && c <= 0x1F10A))
        return EN;
    if ((c >= 0x0600 && c <= 0x0605) || (c >= 0x0660 && c <= 0x0669) || c == 0x066B || c == 0x066C || c == 0x06DD ||
        c == 0x0890 || c == 0x0891 || c == 0x08E2 || (c >= 0x10D30 && c <= 0x10D39) || (c >= 0x10E60 && c <= 0x10E7E))
        return AN;
    /* number separators and terminators */
    if (c == '+' || c == '-' || c == 0x207A || c == 0x207B || c == 0x208A || c == 0x208B || c == 0x2212 || c == 0xFB29 ||
        c == 0xFE62 || c == 0xFE63 || c == 0xFF0B || c == 0xFF0D)
        return ES;
    if (c == '#' || c == '$' || c == '%' || (c >= 0xA2 && c <= 0xA5) || c == 0xB0 || c == 0xB1 || c == 0x0609 ||
        c == 0x060A || c == 0x066A || (c >= 0x20A0 && c <= 0x20CF) || (c >= 0x2030 && c <= 0x2034) || c == 0x212E ||
        c == 0x2213 || c == 0xFE5F || c == 0xFE69 || c == 0xFE6A || (c >= 0xFF03 && c <= 0xFF05) || c == 0xFFE0 ||
        c == 0xFFE1 || c == 0xFFE5 || c == 0xFFE6)
        return ET;
    if (c == ',' || c == '.' || c == '/' || c == ':' || c == 0xA0 || c == 0x060C || c == 0x202F || c == 0x2044 ||
        c == 0xFE50 || c == 0xFE52 || c == 0xFE55 || c == 0xFF0C || c == 0xFF0E || c == 0xFF0F || c == 0xFF1A)
        return CS;
    if (c == ' ' || c == 0x0C || c == 0x3000 || (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x205F) return WS;
    /* right-to-left scripts: Hebrew, Syriac / Thaana are Arabic-class, N'Ko, Samaritan, Mandaic, Arabic and its
     * presentation forms, Hebrew presentation forms, the right-to-left historic and modern scripts past U+FFFF */
    if (c >= 0x0590 && c <= 0x05FF) return R;
    if ((c >= 0x0600 && c <= 0x07BF) || (c >= 0x0860 && c <= 0x08FF) || (c >= 0xFB50 && c <= 0xFD3D) ||
        (c >= 0xFD40 && c <= 0xFDFF) || (c >= 0xFE70 && c <= 0xFEFE) || (c >= 0x10D00 && c <= 0x10D3F) ||
        (c >= 0x10EC0 && c <= 0x10EFF) || (c >= 0x10F30 && c <= 0x10F6F) || (c >= 0x1EC70 && c <= 0x1ECBF) ||
        (c >= 0x1ED00 && c <= 0x1ED4F) || (c >= 0x1EE00 && c <= 0x1EEFF))
        return AL;
    if ((c >= 0x07C0 && c <= 0x085F) || (c >= 0xFB1D && c <= 0xFB4F) || (c >= 0x10800 && c <= 0x10FFF) ||
        (c >= 0x1E800 && c <= 0x1EFFF))
        return R;
    /* other neutrals: ASCII and Latin-1 punctuation and symbols, the general punctuation, symbol, arrow, box and
     * dingbat blocks, CJK punctuation, the compatibility and small forms, full-width punctuation, emoji */
    if (c < 0x80) return (((c | 0x20) >= 'a' && (c | 0x20) <= 'z')) ? L : ON;
    if ((c >= 0xA1 && c <= 0xBF && c != 0xAA && c != 0xB5 && c != 0xBA) || c == 0xD7 || c == 0xF7) return ON;
    if ((c >= 0x2010 && c <= 0x2BFF && c != 0x2071 && c != 0x207F && !(c >= 0x2090 && c <= 0x209C) &&
         !(c >= 0x2100 && c <= 0x214F)) ||
        (c >= 0x2E00 && c <= 0x2E7F) || (c >= 0x3001 && c <= 0x3004) || (c >= 0x3008 && c <= 0x3020) || c == 0x3030 ||
        (c >= 0x303D && c <= 0x303F) || c == 0xFD3E || c == 0xFD3F || (c >= 0xFE10 && c <= 0xFE19) ||
        (c >= 0xFE30 && c <= 0xFE6B) || c == 0xFF01 || c == 0xFF02 || (c >= 0xFF06 && c <= 0xFF0A) ||
        (c >= 0xFF1B && c <= 0xFF20) || (c >= 0xFF3B && c <= 0xFF40) || (c >= 0xFF5B && c <= 0xFF65) ||
        (c >= 0xFFE2 && c <= 0xFFE4) || (c >= 0xFFE8 && c <= 0xFFEE) || (c >= 0x1F000 && c <= 0x1FAFF))
        return ON;
    return L; /* every other letter: Latin, Greek, Cyrillic, kana, kanji, hangul, Indic, Thai... */
}

/* the mirrored form inside a right-to-left run (Unicode's Bidi_Mirroring_Glyph, the pairs a name may use) */
uint32_t mirror(uint32_t c) {
    static const uint32_t kPairs[][2] = {
        {'(', ')'}, {'<', '>'}, {'[', ']'}, {'{', '}'}, {0xAB, 0xBB}, {0x2039, 0x203A}, {0x2045, 0x2046},
        {0x207D, 0x207E}, {0x208D, 0x208E}, {0x2264, 0x2265}, {0x3008, 0x3009}, {0x300A, 0x300B}, {0x300C, 0x300D},
        {0x300E, 0x300F}, {0x3010, 0x3011}, {0x3014, 0x3015}, {0xFF08, 0xFF09}, {0xFF1C, 0xFF1E}, {0xFF3B, 0xFF3D},
        {0xFF5B, 0xFF5D},
    };
    for (const auto& p : kPairs) {
        if (c == p[0]) return p[1];
        if (c == p[1]) return p[0];
    }
    return c;
}

struct Item {
    uint32_t c;
    int log;   /* index in the input */
    Cls orig;  /* its class before the rules (WS for L1, NSM for L3) */
    Cls cls;   /* resolved */
    int level;
};

/* ---- step 1: Arabic joining and Hebrew composition, in typed order ---- */
std::vector<Item> shape(const uint32_t* in, int n) {
    std::vector<Item> out;
    out.reserve((size_t)n);
    auto prev_joins = [&](int i) { /* the nearest non-transparent character before i joins forward */
        for (int j = i - 1; j >= 0; --j) {
            const Join t = join_type(in[j]);
            if (t == JT) continue;
            return t == JD || t == JC;
        }
        return false;
    };
    auto next_index = [&](int i) { /* the nearest non-transparent character after i, or n */
        int j = i + 1;
        while (j < n && join_type(in[j]) == JT) ++j;
        return j;
    };
    auto next_joins = [&](int i) {
        const int j = next_index(i);
        if (j >= n) return false;
        const Join t = join_type(in[j]);
        return t == JR || t == JD || t == JC;
    };
    auto put = [&](uint32_t c, int log) { out.push_back(Item{c, log, ON, ON, 0}); };
    bool joinsOn = false; /* the last letter drawn joins on to the next (for a haraka's tatweel form) */
    for (int i = 0; i < n;) {
        const uint32_t c = in[i];
        /* lam + alef: one ligature */
        if (c == 0x0644) {
            const int j = next_index(i);
            if (j < n) {
                if (const LamAlef* la = lam_alef(in[j])) {
                    put(prev_joins(i) ? la->fin : la->iso, i);
                    joinsOn = false; /* alef never joins on */
                    for (int k = i + 1; k < j; ++k) { /* marks typed between them come after the ligature */
                        if (const Haraka* h = haraka(in[k])) put(h->iso, k);
                        else if (!invisible(in[k])) put(in[k], k);
                    }
                    i = j + 1;
                    continue;
                }
            }
        }
        if (const Forms* f = forms_of(c)) {
            const bool before = prev_joins(i);
            const bool after = join_type(c) == JD && next_joins(i);
            uint32_t form = before ? (after ? f->med : f->fin) : (after ? f->ini : f->iso);
            if (form == 0) form = (before && f->fin) ? f->fin : f->iso;
            put(form != 0 ? form : c, i);
            joinsOn = after;
            ++i;
            continue;
        }
        if (const Haraka* h = haraka(c)) {
            put((joinsOn && h->med != 0) ? h->med : h->iso, i);
            ++i;
            continue;
        }
        /* Hebrew: a letter and the marks after it as one precomposed letter (two marks first, either order) */
        if (c >= 0x05D0 && c <= 0x05F2 && i + 1 < n && is_mark(in[i + 1])) {
            const HebComp* hit = nullptr;
            int used = 0;
            for (const HebComp& hc : kHebrew) {
                if (hc.base != c) continue;
                if (hc.m2 != 0 && i + 2 < n &&
                    ((in[i + 1] == hc.m1 && in[i + 2] == hc.m2) || (in[i + 1] == hc.m2 && in[i + 2] == hc.m1))) {
                    hit = &hc, used = 3;
                    break;
                }
            }
            if (hit == nullptr)
                for (const HebComp& hc : kHebrew)
                    if (hc.base == c && hc.m2 == 0 && in[i + 1] == hc.m1) {
                        hit = &hc, used = 2;
                        break;
                    }
            if (hit != nullptr) {
                put(hit->out, i);
                joinsOn = false;
                i += used;
                continue;
            }
        }
        if (!invisible(c)) put(c, i);
        if (join_type(c) != JT) joinsOn = (c == 0x0640); /* tatweel joins on; anything else that isn't a mark doesn't */
        ++i;
    }
    return out;
}

/* the name's direction: 1 when its first strong letter is right-to-left */
int base_of(const uint32_t* in, int n) {
    for (int i = 0; i < n; ++i) {
        const Cls k = bidi_class(in[i]);
        if (k == L) return 0;
        if (k == R || k == AL) return 1;
    }
    return 0;
}

/* ---- step 2: the bidi pass on the shaped name ---- */
void bidi(std::vector<Item>& v, int base, int flags) {
    const size_t n = v.size();
    if (n == 0) return;
    const Cls sos = base ? R : L;
    for (Item& it : v) it.orig = it.cls = bidi_class(it.c);
    /* W1: a mark takes its letter's class */
    for (size_t i = 0; i < n; ++i)
        if (v[i].cls == NSM) v[i].cls = i == 0 ? sos : v[i - 1].cls;
    /* W2: digits after an Arabic letter are Arabic numbers; W3: Arabic letters are right-to-left */
    {
        Cls strong = sos;
        for (Item& it : v) {
            if (it.cls == L || it.cls == R || it.cls == AL) strong = it.cls;
            else if (it.cls == EN && strong == AL) it.cls = AN;
        }
        for (Item& it : v)
            if (it.cls == AL) it.cls = R;
    }
    /* W4: one separator between two numbers of a kind joins them */
    for (size_t i = 1; i + 1 < n; ++i) {
        const Cls a = v[i - 1].cls, b = v[i + 1].cls;
        if (v[i].cls == ES && a == EN && b == EN) v[i].cls = EN;
        else if (v[i].cls == CS && a == EN && b == EN) v[i].cls = EN;
        else if (v[i].cls == CS && a == AN && b == AN) v[i].cls = AN;
    }
    /* W5: terminators (%, $, ...) next to European digits join them */
    for (size_t i = 0; i < n; ++i) {
        if (v[i].cls != ET) continue;
        size_t j = i;
        while (j < n && v[j].cls == ET) ++j;
        const bool touches = (i > 0 && v[i - 1].cls == EN) || (j < n && v[j].cls == EN);
        if (touches)
            for (size_t k = i; k < j; ++k) v[k].cls = EN;
        i = j;
    }
    /* W6: other separators and terminators are neutral */
    for (Item& it : v)
        if (it.cls == ES || it.cls == ET || it.cls == CS) it.cls = ON;
    /* W7: digits after a Latin letter (or at the start of a left-to-right name) are Latin */
    {
        Cls strong = sos;
        for (Item& it : v) {
            if (it.cls == L || it.cls == R) strong = it.cls;
            else if (it.cls == EN && strong == L) it.cls = L;
        }
    }
    /* N1 / N2: neutrals between two of one direction take it (numbers count as right-to-left here), else the name's */
    auto dir = [](Cls k) { return k == L ? L : R; };
    for (size_t i = 0; i < n; ++i) {
        if (v[i].cls != ON && v[i].cls != WS) continue;
        size_t j = i;
        while (j < n && (v[j].cls == ON || v[j].cls == WS)) ++j;
        const Cls before = i == 0 ? sos : dir(v[i - 1].cls);
        const Cls after = j == n ? sos : dir(v[j].cls);
        const Cls take = before == after ? before : sos;
        for (size_t k = i; k < j; ++k) v[k].cls = take;
        i = j - 1;
    }
    /* I1 / I2: the levels */
    for (Item& it : v) {
        if (base == 0) it.level = it.cls == R ? 1 : (it.cls == AN || it.cls == EN) ? 2 : 0;
        else it.level = it.cls == R ? 1 : 2;
    }
    /* L1: spaces at the name's end go back to the name's own level */
    for (size_t i = n; i-- > 0 && v[i].orig == WS;) v[i].level = base;
    /* L2: from the highest level down to the lowest odd one, reverse every run at that level or above */
    int top = 0, lowOdd = 99;
    for (const Item& it : v) {
        top = std::max(top, it.level);
        if (it.level & 1) lowOdd = std::min(lowOdd, it.level);
    }
    for (int lv = top; lv >= lowOdd && lv > 0; --lv) {
        for (size_t i = 0; i < n; ++i) {
            if (v[i].level < lv) continue;
            size_t j = i;
            while (j < n && v[j].level >= lv) ++j;
            std::reverse(v.begin() + (long)i, v.begin() + (long)j);
            i = j;
        }
    }
    /* L4: brackets mirrored inside right-to-left runs */
    for (Item& it : v)
        if (it.level & 1) it.c = mirror(it.c);
    /* L3 (renderers that draw a mark over the letter before it): a reversed run put each mark before its letter; move
     * the marks back after it, in their typed order */
    if (flags & PC_RTL_MARKS_AFTER_BASE) {
        for (size_t i = 0; i < n; ++i) {
            if (v[i].orig != NSM || !(v[i].level & 1)) continue;
            size_t j = i;
            while (j < n && v[j].orig == NSM && v[j].level == v[i].level) ++j;
            if (j < n && v[j].level == v[i].level) { /* the marks, then their letter: letter first, marks reversed */
                Item letter = v[j];
                std::reverse(v.begin() + (long)i, v.begin() + (long)j);
                for (size_t k = j; k > i; --k) v[k] = v[k - 1];
                v[i] = letter;
            }
            i = j;
        }
    }
}

/* UTF-8 <-> code points (the hub's strings) */
std::vector<uint32_t> decode(const char* s) {
    std::vector<uint32_t> out;
    for (const unsigned char* p = (const unsigned char*)s; p != nullptr && *p != 0;) {
        uint32_t c = *p++;
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c &= extra == 3 ? 0x07 : extra == 2 ? 0x0F : extra == 1 ? 0x1F : 0x7F;
        while (extra-- > 0 && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
        out.push_back(c);
    }
    return out;
}
void encode(uint32_t c, std::vector<char>& out) {
    if (c < 0x80) out.push_back((char)c);
    else if (c < 0x800) out.push_back((char)(0xC0 | c >> 6)), out.push_back((char)(0x80 | (c & 0x3F)));
    else if (c < 0x10000)
        out.push_back((char)(0xE0 | c >> 12)), out.push_back((char)(0x80 | ((c >> 6) & 0x3F))),
            out.push_back((char)(0x80 | (c & 0x3F)));
    else
        out.push_back((char)(0xF0 | c >> 18)), out.push_back((char)(0x80 | ((c >> 12) & 0x3F))),
            out.push_back((char)(0x80 | ((c >> 6) & 0x3F))), out.push_back((char)(0x80 | (c & 0x3F)));
}

} // namespace

extern "C" int pc_rtl_needed(const uint32_t* cps, int n) {
    for (int i = 0; cps != nullptr && i < n; ++i) {
        const uint32_t c = cps[i];
        if (c == 0x200F || c == 0x061C || (c >= 0x0590 && c <= 0x08FF) || (c >= 0xFB1D && c <= 0xFDFF) ||
            (c >= 0xFE70 && c <= 0xFEFE) || (c >= 0x10800 && c <= 0x10FFF) || (c >= 0x1E800 && c <= 0x1EFFF))
            return 1;
    }
    return 0;
}

extern "C" int pc_rtl_base_rtl(const uint32_t* cps, int n) { return cps != nullptr && n > 0 ? base_of(cps, n) : 0; }

extern "C" int pc_rtl_display(const uint32_t* in, int n, uint32_t* out, int* vis_to_log, int flags) {
    if (in == nullptr || out == nullptr || n <= 0) return 0;
    if (!pc_rtl_needed(in, n)) { /* nothing to do: as typed */
        for (int i = 0; i < n; ++i) {
            out[i] = in[i];
            if (vis_to_log != nullptr) vis_to_log[i] = i;
        }
        return n;
    }
    std::vector<Item> v = shape(in, n);
    bidi(v, base_of(in, n), flags);
    const int m = (int)std::min(v.size(), (size_t)n);
    for (int i = 0; i < m; ++i) {
        out[i] = v[(size_t)i].c;
        if (vis_to_log != nullptr) vis_to_log[i] = v[(size_t)i].log;
    }
    return m;
}

extern "C" size_t pc_rtl_display_utf8(const char* in, char* out, size_t out_size, int flags) {
    if (out == nullptr || out_size == 0) return 0;
    out[0] = 0;
    if (in == nullptr) return 0;
    const std::vector<uint32_t> cps = decode(in);
    std::vector<uint32_t> vis(cps.size() + 1);
    const int m = pc_rtl_display(cps.data(), (int)cps.size(), vis.data(), nullptr, flags);
    std::vector<char> bytes;
    for (int i = 0; i < m; ++i) encode(vis[(size_t)i], bytes);
    size_t len = std::min(bytes.size(), out_size - 1);
    while (len > 0 && len < bytes.size() && ((unsigned char)bytes[len] & 0xC0) == 0x80) --len; /* never half a letter */
    if (len > 0) std::memcpy(out, bytes.data(), len);
    out[len] = 0;
    return len;
}

extern "C" int pc_rtl_base_rtl_utf8(const char* in) {
    const std::vector<uint32_t> cps = decode(in);
    return pc_rtl_base_rtl(cps.data(), (int)cps.size());
}
