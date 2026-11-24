/*
 * pc_ptr32.c: what happens when the game stores a pointer that does not fit in 32 bits (docs/port/low-memory.md).
 *
 * Session main's include/pc/ptr32.h wraps every pointer -> 32-bit conversion in the game code (PC_PTR32,
 * PC_PTR32_IMPLICIT). Unless ORRE_NO_PTR32_CHECK is defined, each one checks that the upper half is zero and calls
 * this on failure. A failure means something game-visible was allocated above 4 GB (snatcherLeo's rule, option (a)), and
 * the truncated value would point somewhere else entirely, so the only safe thing is to stop with the call site.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "pc_crash.h"

void pc_ptr32_fail(uintptr_t value, const char* file, int line) {
    fprintf(stderr,
            "[pc_ptr32] pointer 0x%016llX does not fit in 32 bits at %s:%d\n"
            "           (something the game can see was allocated above 4 GB; see docs/port/low-memory.md)\n",
            (unsigned long long)value, file != NULL ? file : "?", line);
    fflush(stderr);
    pc_crash_print_stack("pointer check failed; the call stack:"); /* who made the bad pointer (2026-10-17) */
    abort();
}
