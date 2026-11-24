/*
 * trail: what the user just did, kept as short lines (2026-11-22).
 *
 * Why: snatcherLeo, 2026-11-22: "if I can't explain to you WHY the app crashed and what I did, you can see (either via
 * tracking) what I was doing within the app"; "Having all the loggers so you can see what's going on in app as I do
 * things will set us up for a better development cycle". Each action (a click, a key, a timeline drag) becomes one
 * line: written out through the sink as it happens (Studio puts it in its log), and the newest 64 kept in memory so a
 * crash report can print the steps that led to it (Studio's pc_crash_report_hook).
 *
 * Shaped for the shared diagnostics library both programs will link before the laptop sitting (Phase 5, step 5.10;
 * it was step 4.4 until 2026-11-22) (2026-11-22: "a small C
 * API with no Studio types in its signatures"): plain C, no allocation after start, only the clock differs by system
 * (trail.c), so it builds on Linux and macOS too. Not thread-safe by design: notes come from the UI thread; trail_last only reads
 * (a crash report may read a line being written: at worst one garbled line).
 */
#ifndef ORRE_TRAIL_H
#define ORRE_TRAIL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Where each note goes as it's made (one finished line, no newline), or NULL for memory only. */
void trail_set_sink(void (*sink)(const char* line));

/* One action: a short category ("click", "key", "drag", "window") and what it was, with the context that matters
 * ("tab Moves | Bulbasaur, Moves, Solarbeam, turn 2"). Longer texts are cut at 200 bytes. */
void trail_note(const char* category, const char* text);

/* The newest n notes, oldest first, one per line ("  12.345 click   tab Moves"), into out (always terminated). Returns
 * the number of notes written. Safe from a crash handler: no allocation, no locks. */
int trail_last(int n, char* out, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* ORRE_TRAIL_H */
