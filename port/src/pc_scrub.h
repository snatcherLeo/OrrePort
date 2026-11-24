/*
 * pc_scrub.h: personal details taken out of a text before it goes into a report zip (2026-11-22; split out of
 * pc_report.cpp so Studio's Report a problem uses the same rule, 4.3 part 3).
 *
 * Why: a report is posted where other people read it (the Discord #reports channel), and its log and settings carry
 * file paths with the player's home folder and user name in them (snatcherLeo, 2026-11-09, the report plan: personal paths
 * and the user name become <user>). One function both programs call, so the rule can't drift between them.
 *
 * Shared with Studio: plain C, no SDL and no OrrePort types (Studio compiles this file as it is).
 * Platforms: Windows reads USERPROFILE and USERNAME through the wide calls (any letters); Linux and macOS read HOME and
 * USER (likely right there: not built there yet).
 */
#ifndef PC_SCRUB_H
#define PC_SCRUB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A copy of `text` (len bytes) with, ignoring ASCII case: the home folder in both slash styles and the user name
 * replaced by <user>, and the temp folder with the first folder under it (TEMP on Windows, TMPDIR or /tmp elsewhere) by
 * <temp> and <scratch> (2026-11-24: that folder names the program that made it, test tools included). Names shorter
 * than 3 letters are left alone (a 2-letter user name would otherwise eat ordinary words). Returns a NUL-terminated
 * copy to free with pc_scrub_free, its length in *out_len (may be NULL), or NULL when out of memory. */
char* pc_scrub(const char* text, size_t len, size_t* out_len);
void pc_scrub_free(char* s);

#ifdef __cplusplus
}
#endif

#endif
