/*
 * pc_grapheme.h: visible letters (Unicode's extended grapheme clusters, UAX #29), for counting and editing names
 * (the project's build helper, 2026-11-16; snatcherLeo's decision the same day, the decision log: "the 12-letter limit counts visible
 * letters").
 *
 * Why: a name's letters were counted one per Unicode character (code point), so a family emoji (man, joiner, woman,
 * joiner, girl: five characters) used five of the 12 letters, a thumbs-up with a skin tone two, and a Hindi syllable
 * like "स्ते" four. What a player sees is one letter each. Unicode defines that unit: the extended grapheme cluster
 * (UAX #29, "Unicode Text Segmentation"): a base character with everything that rides on it (combining marks, vowel
 * signs, an emoji's skin tone and the emoji joined after a zero-width joiner, the second half of a flag, a Hangul
 * syllable typed as separate jamo, a conjunct joined by a virama in the Indic scripts Unicode lists).
 *
 * One place for every user (snatcherLeo: the same rule wherever a 12-letter name is entered or counted): the naming
 * keyboard (pc_name_keyboard.cpp: the count, typing, the input method's commit and composition, Backspace, the "Only
 * 12 letters fit" check), the hub's trainer-name cut (pc_hub_saves.cpp hub_fit_name), and later Studio's name boxes.
 *
 * The safeguard (snatcherLeo, same day): at most PC_NAME_MAX_HIDDEN characters ride on one letter's base, so pasted text with
 * marks stacked without end ("Zalgo" text) can't make a name grow without limit. Extra parts are dropped as they are
 * typed or pasted. Real text stays well under it (the longest emoji sequences in use, a kiss with two skin tones, are
 * the base and 9 more; a Hindi conjunct of three consonants with a vowel sign is the base and 5 more).
 *
 * Names are still stored as typed (every character, in typed order): only how they are counted and cut changes. So a
 * name is at most PC_NAME_MAX_LETTERS x (1 + PC_NAME_MAX_HIDDEN) characters; buffers that hold a full name are sized
 * for that (PC_NAME_MAX_CPS, PC_NAME_MAX_UTF8).
 *
 * Platforms: plain C over a generated table (pc_grapheme_table.h), no OS calls and no library: the same on Windows,
 * Linux and macOS (likely: nothing here can differ). XD: port code with no game knowledge; it serves XD's names as it is
 * (likely).
 */
#ifndef PC_GRAPHEME_H
#define PC_GRAPHEME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* OrrePort's name limit, in visible letters (snatcherLeo, 2026-11-10 / 2026-11-14 / 2026-11-16), and the safeguard */
#define PC_NAME_MAX_LETTERS 12
#define PC_NAME_MAX_HIDDEN 10
/* the most characters a full name can hold: 12 letters, each a base and at most 10 hidden parts */
#define PC_NAME_MAX_CPS (PC_NAME_MAX_LETTERS * (1 + PC_NAME_MAX_HIDDEN))
/* the same in UTF-8 (4 bytes a character at most), with the ending 0: the size of a C string holding a full name */
#define PC_NAME_MAX_UTF8 (PC_NAME_MAX_CPS * 4 + 1)

/* Marks where each visible letter starts: starts[i] = 1 when a letter starts at cps[i] (starts[0] = 1 for n > 0).
 * `starts` may be NULL. Returns the letter count. */
int pc_grapheme_starts(const uint32_t* cps, int n, uint8_t* starts);

/* The number of visible letters in cps[0, n). */
int pc_grapheme_count(const uint32_t* cps, int n);

/* Where the last visible letter starts (an index into cps; 0 when n is 0): Backspace removes cps[that, n). */
int pc_grapheme_last(const uint32_t* cps, int n);

/* What adding character c after cps[0, n) would do, with a letter limit and the hidden-part cap. */
enum {
    PC_GRAPHEME_ADDED = 0,  /* it fits: a new letter within the limit, or a part riding on the last letter */
    PC_GRAPHEME_FULL = 1,   /* it would start a letter past max_letters */
    PC_GRAPHEME_CAPPED = 2, /* it would ride on the last letter, which already has max_hidden parts after its base */
};
int pc_grapheme_append(const uint32_t* cps, int n, uint32_t c, int max_letters, int max_hidden);

/* UTF-8 text (the hub's pages): the letter count, and the byte offset where the last letter starts (0 when empty). */
int pc_grapheme_utf8_count(const char* s, size_t len);
size_t pc_grapheme_utf8_last(const char* s, size_t len);

#ifdef __cplusplus
}
#endif

#endif
