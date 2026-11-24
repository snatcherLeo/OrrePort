/*
 * pc_save_trace.c: which fields of the save block the game reads and writes, and how wide each access is (
 * 2026-10-18; the Dolphin-save path, the project notes "Saves between Dolphin and the PC").
 *
 * Only in the save-trace build (firstlink.py with ORRE_SAVE_TRACE_BUILD=1): the game units are compiled with
 * -fsanitize=kernel-address, which puts a call before every load and store, __asan_load4_noabort(addr) and so on,
 * the number being the access width. This file supplies those calls. An access inside the live save block
 * (savedataGetStatus(NULL, 0), 0x1DFD0 bytes) records its offset and width; everything else is ignored.
 *
 * Why: the save is one raw byte array copied by value, and the PC's native code keeps each multi-byte field in its
 * own byte order, so moving a save between Dolphin and the PC means swapping every field, and only the code knows the
 * widths (96 files read the save). The table this writes (offset, widths; no game data) feeds the converter.
 *
 * ORRE_SAVE_TRACE=<file>: where the table goes (default save_trace.txt in the working folder); written at exit and
 * every few million accesses. Lines: "0x<offset> <widths>", widths a list such as 4 or 1,4 (a field read two ways).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SAVE_SIZE 0x1DFD0

typedef uint32_t u32;

extern u32 savedataGetStatus(unsigned char* data, unsigned short kind); /* prototypes.h: a 32-bit game address */
extern void* pc_resolve_dp(u32 address);            /* ptr32.h's PC_PTR */

static unsigned char s_widths[SAVE_SIZE]; /* bit 0 = 1 byte, 1 = 2, 2 = 4, 3 = 8, 4 = 16 */
static unsigned char* s_base;
static int s_busy;
static int s_dirty;

static void save_trace_write(void) {
    const char* path = getenv("ORRE_SAVE_TRACE");
    FILE* f = fopen(path != NULL && *path != '\0' ? path : "save_trace.txt", "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "# save block access widths (pc_save_trace.c): offset, widths seen\n");
    for (int i = 0; i < SAVE_SIZE; i++) {
        if (s_widths[i] != 0) {
            const char* sep = "";
            fprintf(f, "0x%05X ", i);
            for (int b = 0; b < 5; b++) {
                if (s_widths[i] >> b & 1) {
                    fprintf(f, "%s%d", sep, 1 << b);
                    sep = ",";
                }
            }
            fputc('\n', f);
        }
    }
    fclose(f);
    s_dirty = 0;
}

static void note(const void* address, unsigned width_bit) {
    unsigned char* p = (unsigned char*)address;
    if (s_busy) {
        return;
    }
    if (s_base == NULL) {
        s_busy = 1; /* the getter is instrumented too */
        s_base = (unsigned char*)pc_resolve_dp(savedataGetStatus(NULL, 0));
        s_busy = 0;
        if (s_base == NULL) {
            return;
        }
        atexit(save_trace_write);
        fprintf(stderr, "[save_trace] save block at %p, tracing accesses\n", (void*)s_base);
    }
    if (p >= s_base && p < s_base + SAVE_SIZE) {
        unsigned char* w = &s_widths[p - s_base];
        if (!(*w >> width_bit & 1)) {
            *w |= (unsigned char)(1u << width_bit);
            s_dirty = 1;
        }
    }
    /* a test run ends by being killed (saferun's time limit), which runs no exit handlers: write the table now and
     * then (every 2^22 accesses, a few times a second) when something new was seen */
    static unsigned s_calls;
    if ((++s_calls & 0x3FFFFF) == 0 && s_dirty) {
        save_trace_write();
    }
}

#define WIDTH_FUNCS(n, bit)                                                                                            \
    void __asan_load##n##_noabort(void* a) { note(a, bit); }                                                           \
    void __asan_store##n##_noabort(void* a) { note(a, bit); }                                                          \
    void __asan_load##n(void* a) { note(a, bit); }                                                                     \
    void __asan_store##n(void* a) { note(a, bit); }
WIDTH_FUNCS(1, 0)
WIDTH_FUNCS(2, 1)
WIDTH_FUNCS(4, 2)
WIDTH_FUNCS(8, 3)
WIDTH_FUNCS(16, 4)

/* accesses of other sizes (structure copies the compiler does inline): each byte as width 1 */
void __asan_loadN_noabort(void* a, size_t n) {
    for (size_t i = 0; i < n; i++) {
        note((char*)a + i, 0);
    }
}
void __asan_storeN_noabort(void* a, size_t n) { __asan_loadN_noabort(a, n); }
void __asan_loadN(void* a, size_t n) { __asan_loadN_noabort(a, n); }
void __asan_storeN(void* a, size_t n) { __asan_loadN_noabort(a, n); }
void __asan_handle_no_return(void) {}
