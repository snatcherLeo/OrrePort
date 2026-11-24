/*
 * pc_rtl.h: right-to-left names (Arabic, Hebrew) drawn the way they are read, without a shaping library (the project's build
 * helper, 2026-11-16; the plan in docs/port/translation.md, "Right-to-left and complex scripts"; decided 2026-11-15:
 * every language typable in a name before the playtest, HarfBuzz not in the build).
 *
 * Why: the game's text, the naming field and OrrePort's own pages (RmlUi) all draw one letter after another, left to
 * right, with no shaping. Arabic then shows every letter in its standalone shape and a right-to-left name reads
 * backwards. This file turns a name, as typed, into what to draw:
 *   1. Arabic joining: each letter becomes its Unicode presentation form (isolated, initial, medial or final) from its
 *      neighbours' joining types (U+FE70-FEFF and U+FB50-FDFF, all within 16 bits, so the game's 16-bit text can carry
 *      them); lam + alef becomes one ligature; the eight common harakat (vowel marks) become their spacing forms,
 *      on a tatweel where the word joins through them, so a mark never breaks a word's joining.
 *   2. Hebrew: a letter with dagesh, shin / sin dot, rafe and the like becomes its precomposed form (U+FB1D-FB4F).
 *   3. A simple bidi pass: right-to-left runs are reversed for display; digits and Latin inside keep their own
 *      left-to-right order; a name whose first strong letter is right-to-left reads from the right.
 * It is a draw-time step only: names are stored as typed (logical order, the letters' own code points) in the extras
 * file, the save and everywhere else; only the copy handed to a drawing call is changed.
 *
 * One place for every user (snatcherLeo: the same letters in the game, the hub and later Studio): the game's text
 * (pc_names.cpp, OrreHook_MessageInsert), the naming keyboard's field (pc_name_keyboard.cpp) and the hub's pages
 * (pc_hub_saves.cpp, pc_hub.cpp) all call this file; Studio will too.
 *
 * What it is not (the full list in pc_rtl.cpp): the Unicode Bidirectional Algorithm in full (no explicit embeddings,
 * overrides or isolates, no bracket pairs, one name at a time, never a whole paragraph of mixed text), nor shaping for
 * other scripts (Indic, Thai, Khmer, Myanmar, Syriac, N'Ko still draw letter by letter: those need HarfBuzz).
 *
 * Platforms: pure C++ with no OS calls and no library: the same on Windows, Linux and macOS (likely: nothing here can
 * differ between them).
 */
#ifndef PC_RTL_H
#define PC_RTL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Flags for pc_rtl_display. */
enum {
    /* A combining mark with no spacing form stays after its base letter in drawing order (the Unicode bidi rule L3).
     * For renderers that draw a mark over the letter before it (RmlUi: a zero-width mark with a negative bearing); the
     * game's text draws every character as its own letter beside the last, so it leaves this off and the mark lands
     * after its base in reading order instead. */
    PC_RTL_MARKS_AFTER_BASE = 1,
};

/* Whether any character needs this file (a right-to-left letter, or an Arabic letter or mark): a fast "nothing to do"
 * for every Latin, kana or emoji name. */
int pc_rtl_needed(const uint32_t* cps, int n);

/* Whether the name reads from the right: its first strong letter is right-to-left (Hebrew, Arabic, Syriac, Thaana,
 * N'Ko, Adlam...). A name with no strong letter (digits, symbols) reads from the left. */
int pc_rtl_base_rtl(const uint32_t* cps, int n);

/* The name as it is drawn, left to right: `in` is the name as typed (code points, logical order), `out` gets at most n
 * code points (joined Arabic forms, a ligature for two letters, invisible joining and direction marks left out), and
 * `vis_to_log` (may be NULL) gets for each drawn position the index in `in` it came from (a lam-alef ligature: the
 * lam's). Returns the count written. Characters that need nothing come out as they went in. */
int pc_rtl_display(const uint32_t* in, int n, uint32_t* out, int* vis_to_log, int flags);

/* The same for UTF-8 text (the hub's pages): writes the drawn form, 0-ended, at most out_size bytes; returns the
 * length written. Text that needs nothing is copied as it is. */
size_t pc_rtl_display_utf8(const char* in, char* out, size_t out_size, int flags);
/* pc_rtl_base_rtl for UTF-8 text */
int pc_rtl_base_rtl_utf8(const char* in);

#ifdef __cplusplus
}
#endif

#endif
