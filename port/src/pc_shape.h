/*
 * pc_shape.h: names shaped by HarfBuzz, in the order SheenBidi gives (the project's build helper, 2026-11-16; snatcherLeo approved
 * both libraries the same day, the decision log "HarfBuzz and a bidi library, added now"; docs/port/font-renderer.md
 * "Shaping names").
 *
 * Why: the game's text is one 16-bit code per letter, and the game draws each letter on its own, left to right. Some
 * scripts can't be drawn that way: in Hindi, Bengali and Tamil letters join into conjuncts and vowel signs move and
 * stack; Thai and Khmer marks sit above and below their letter; Arabic letters take joined shapes and read from the
 * right; an emoji with a skin tone or a family is one picture made from several characters. A shaping engine
 * (HarfBuzz) turns a run of characters into the font's glyphs with their places; the bidi algorithm (SheenBidi) says
 * which runs read from the right and in what order they sit.
 *
 * The design (the game never sees a glyph):
 *   1. A name is stored exactly as typed (the extras file, the save's GameCube name, everything else); only the copy
 *      handed to a drawing call changes, as with pc_rtl.h.
 *   2. pc_shape_name splits the typed name into runs: characters of a script that needs a font of its own (Arabic,
 *      Devanagari, Thai...: the Noto chain's per-script fonts, pc_font_files.h) with the marks and joiners that belong
 *      to them, and emoji followed by a modifier (skin tone, zero-width joiner, variation selector). Everything else
 *      (Latin, kana, kanji, digits, a lone emoji) stays one letter per character, drawn as before: by the game's own
 *      font where it has the letter, else by part D.
 *   3. Each run is shaped by HarfBuzz with the same Noto font, weight and width part D draws with. Each resulting
 *      cluster (what HarfBuzz keeps together: an Indic syllable, a Thai letter with its marks, an Arabic letter in its
 *      joined form, a whole emoji sequence) becomes one private stand-in code (pc_font_files.h, the same U+E000 area as
 *      the emoji stand-ins), whose record holds the cluster's glyphs and their places in font units.
 *   4. SheenBidi's levels put the runs in drawn order (right-to-left runs reversed, numbers and Latin inside keeping
 *      their order, brackets mirrored), and the drawn copy is the list of items in that order.
 *   5. Part D (pc_font_render.cpp) draws a cluster stand-in from its record: the glyphs by index from the same font, at
 *      the game font's size, as one picture whose width is the cluster's advance. The game measures, wraps and draws it
 *      as one letter, so nothing in src/game changes.
 * If HarfBuzz can't shape a name (no font file, more glyphs in one cluster than a record holds), pc_shape_name returns
 * -1 and the caller falls back to pc_rtl.cpp's table pass (Arabic and Hebrew only) or letter by letter.
 * ORRE_SHAPING=0 (a test switch, never a setting) turns shaping off for one run: the before / after pictures.
 *
 * Who uses it: the game's text (pc_names.cpp's shown, at OrreHook_MessageInsert) and the naming keyboard's field
 * (pc_name_keyboard.cpp's draw_name). The hub's RmlUi pages shape their text in their own font engine with the same two
 * libraries (hub_text/pc_ui_shaping.cpp); Studio's text boxes will reuse that engine (font-renderer.md).
 *
 * Platforms: plain C++ over HarfBuzz and SheenBidi, both built from source on every OS; font files from Fonts/ next to
 * the exe (pc_font_dir). The same on Windows, Linux and macOS (likely: no OS calls here).
 */
#ifndef PC_SHAPE_H
#define PC_SHAPE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whether shaping is on: the libraries are always built in, so this is 0 only with ORRE_SHAPING=0. */
int pc_shape_enabled(void);

/* Whether a name needs this file: a letter of a per-script font, a right-to-left letter, or an emoji with a modifier.
 * A fast "nothing to do" for Latin, kana and kanji names. */
int pc_shape_needed(const uint32_t* cps, int n);

/* Whether the name reads from the right: SheenBidi's paragraph level for it (its first strong letter, Unicode's rules
 * P2-P3). */
int pc_shape_base_rtl(const uint32_t* cps, int n);

/* One drawn piece of a name. */
typedef struct PcShapedItem {
    uint32_t cp;      /* the character to draw when cluster is 0: as typed, or its mirror (a bracket in a
                         right-to-left run) */
    uint16_t cluster; /* a shaped cluster's stand-in code (pc_font_files.h), else 0 */
    int first, last;  /* the typed letters it came from: logical indexes into the name, inclusive */
} PcShapedItem;

/* The name `cps` (n code points as typed; stand-ins already turned back into their real characters) as drawn, left to
 * right: at most `max` items into `out`. Returns the item count, or -1 when shaping failed or is off (the caller then
 * uses pc_rtl.h). Only from one thread at a time per caller; a lock inside keeps the game and the keyboard apart. */
int pc_shape_name(const uint32_t* cps, int n, PcShapedItem* out, int max);

/* --- for part D: the record behind a cluster stand-in --- */
enum { PC_SHAPE_MAX_GLYPHS = 24 };
typedef struct PcShapedGlyph {
    uint32_t gid; /* the glyph's index in the font */
    int32_t x, y; /* its place from the cluster's pen, font units, y up */
} PcShapedGlyph;
typedef struct PcShapedRun {
    int chain;          /* which font: the Noto chain's entry (pc_noto_font) */
    uint32_t first_cp;  /* the cluster's first character (logs, and the CJK face rule if ever needed) */
    int upem;           /* the font's units per em */
    int32_t advance;    /* the cluster's width, font units */
    int count;
    PcShapedGlyph glyph[PC_SHAPE_MAX_GLYPHS];
} PcShapedRun;

/* Fills `out` for a cluster stand-in code; 0 when the code isn't one. Any thread. */
int pc_shape_cluster(uint16_t code, PcShapedRun* out);

#ifdef __cplusplus
}
#endif

#endif
