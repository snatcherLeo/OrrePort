/*
 * pc_font_files.h: where OrrePort's shipped fonts are, the Noto last-resort chain, which Noto CJK region face a
 * character takes, and the stand-in codes for characters past U+FFFF (the project's build helper, 2026-11-16; the decisions
 * "Fallback fonts: Google's Noto set shipped on every OS" and "Fonts in the portable layout"; docs/port/font-renderer.md
 * "Start here", layer 3).
 *
 * Why one place: the game's font renderer (part D, pc_font_render.cpp) and OrrePort's own pages (RmlUi, the hub and the
 * Settings overlay, pc_settings_overlay.cpp) must draw the same fallback letters on every OS (snatcherLeo: "the same letters
 * for everyone"), so they read one list of files in one folder and one rule for the CJK region.
 *
 * Platforms: plain C++ over SDL (SDL_GetBasePath, SDL_GetPreferredLocales); the same on Windows, Linux and macOS. The
 * files are the same on every OS (likely: Noto ships one set for all; Linux file names are case-sensitive, so the folder
 * is always written "Fonts" exactly).
 */
#ifndef PC_FONT_FILES_H
#define PC_FONT_FILES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The folder of OrrePort's shipped fonts, "Fonts/" next to the exe (app\Fonts\ in the portable layout; decided
 * 2026-11-15, "Fonts in the portable layout"), with a trailing slash; "" when SDL can't say where the exe is. */
const char* pc_font_dir(char* out, size_t size);

/* One font of the Noto last-resort chain. index -1: the CJK collection, whose face is picked per character
 * (pc_noto_cjk_face). wght: the weight axis for a variable font (0: the font's default, or not variable). bold: FreeType
 * emboldening in 26.6 units at the game's size (0 none). color: a colour bitmap font (Noto Color Emoji). */
typedef struct PcNotoFont {
    const char* file;
    int index;
    float wght;
    int bold;
    int color;
    const char* what;   /* for logs */
    const char* ranges; /* the code points it is asked for ("0980-09FF 1CD0-1CFF", hex), NULL: any. Why: with one font
                           per script (about 80 files) a character is only offered to its own script's font, so a
                           missing letter never opens (reads whole) every file on the way */
} PcNotoFont;

/* The chain in the order it is tried (pc_font_files.cpp says why this order). */
int pc_noto_count(void);
const PcNotoFont* pc_noto_font(int i);
/* whether the chain entry is asked for code point cp (its ranges) */
int pc_noto_covers(const PcNotoFont* f, uint32_t cp);

#define PC_NOTO_CJK_FILE "NotoSansCJK-Bold.ttc"
/* The faces inside NotoSansCJK-Bold.ttc (read from the file: 0 JP, 1 KR, 2 SC, 3 TC, 4 HK; 5-9 the Mono ones) */
enum { PC_NOTO_CJK_JP = 0, PC_NOTO_CJK_KR = 1, PC_NOTO_CJK_SC = 2, PC_NOTO_CJK_TC = 3, PC_NOTO_CJK_HK = 4 };

/* Which region face draws code point cp, for text in language `lang` ("en", "ja", ... or NULL): the rule in
 * pc_font_files.cpp. */
int pc_noto_cjk_face(uint32_t cp, const char* lang);
/* The one face OrrePort's own pages load (RmlUi takes one CJK fallback face): the language's, else the system's. */
int pc_noto_cjk_default(const char* lang);
/* Whether cp is a CJK-script character (kana, kanji, hangul, bopomofo, CJK signs and full-width forms): those skip the
 * Windows-only recipe fonts in part D and go to Noto CJK, so every OS draws them the same. */
int pc_noto_is_cjk(uint32_t cp);

/* --- Stand-in codes for characters past U+FFFF (emoji, rare CJK) ---------------------------------------------------
 * The game's text is one 16-bit code per letter, with no pairs (docs/engine/messages.md). A character past U+FFFF in a
 * name gets a private-use code (U+E000 up, assigned the first time it is seen this run) that stands for it inside the
 * game's text; part D draws the real character for it, and the extras file stores the real character (UTF-8), so the
 * stand-in never outlives the run. The GameCube name drops it (no GameCube font has a private-use letter), and the
 * GameCube copy's checklist says so (pc_gcopy.c, "emoji"). */
uint16_t pc_text_standin(uint32_t cp);   /* cp itself when below U+10000; 0 when the table is full */
uint32_t pc_text_real(uint16_t code);    /* the real character for a stand-in; the code itself otherwise */
int pc_text_is_standin(uint16_t code);   /* 1 when code is an assigned stand-in */
/* *Added 2026-11-16 (shaping, pc_shape.h):* a stand-in for a shaped cluster, from the same private-use area. `key` is the
 * cluster's record (pc_shape.cpp writes and reads it: its font and glyphs), so the same cluster always gets the same
 * code in a run; pc_text_real gives back its first character. Lives only in the drawn copy of a name, never stored.
 * Returns 0 when the table is full. */
uint16_t pc_text_standin_cluster(const void* key, size_t len, uint32_t first_cp);
/* copies the record of a cluster stand-in into out (at most cap bytes); returns its length, 0 for any other code */
size_t pc_text_cluster_key(uint16_t code, void* out, size_t cap);
/* Characters that ride on another one and aren't letters of their own in a name: variation selectors, the zero-width
 * joiner, skin-tone modifiers, tags. Without shaping an emoji sequence keeps only its first emoji; with HarfBuzz
 * (pc_shape_enabled, 2026-11-16) the naming keyboard keeps the whole sequence and it draws as one picture (see
 * pc_font_files.cpp) */
int pc_text_is_modifier(uint32_t cp);

#ifdef __cplusplus
}
#endif

#endif
