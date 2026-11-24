/*
 * pc_rel_test: OSLink / OSUnlink on the PC (port/src/pc_rel.c, docs/port/rel-loading.md) over real REL modules.
 *
 *   pc_rel_test <file.rel>...
 *
 * Each module is loaded below 4 GB, linked and unlinked. main.dol addresses resolve to slots in a test area (one per
 * distinct address) so every store the _prolog / _epilog makes can be traced back to its main.dol variable. Prints
 * one line per store; port/tools/rel_check.py compares the prolog stores with the decompiled _prolog in src/rel/.
 * Exit code 0 = every module linked and unlinked.
 */
#include "pc_crash.h"
#include "pc_lowmem.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int BOOL;
typedef unsigned int u32;
typedef unsigned char u8;
typedef u32 (*PcRelResolve)(u32 gcAddress);
void pc_rel_set_resolver(PcRelResolve resolve);
void pc_rel_set_store_hook(void (*hook)(u32 pcAddress, u32 value));
BOOL OSLink(void* module, void* bss);
BOOL OSUnlink(void* module);

#define SLOTS 4096
static u32 s_gc[SLOTS];
static int s_nslots;
static u8* s_area;

static u32 resolve(u32 gc) {
    int i;
    for (i = 0; i < s_nslots; i++) {
        if (s_gc[i] == gc) {
            return (u32)(uintptr_t)(s_area + 4 * i);
        }
    }
    if (s_nslots == SLOTS) {
        return 0;
    }
    s_gc[s_nslots] = gc;
    return (u32)(uintptr_t)(s_area + 4 * s_nslots++);
}

static u8* s_mod;
static size_t s_mod_size;
static const char* s_phase;

static void store(u32 pc, u32 value) {
    u32 gc = 0;
    size_t slot = (pc - (u32)(uintptr_t)s_area) / 4;
    if (pc >= (u32)(uintptr_t)s_area && slot < (size_t)s_nslots) {
        gc = s_gc[slot];
    }
    if (value >= (u32)(uintptr_t)s_mod && value < (u32)(uintptr_t)s_mod + s_mod_size) {
        printf("%s store 0x%08X <- module+0x%X\n", s_phase, gc, value - (u32)(uintptr_t)s_mod);
    } else {
        printf("%s store 0x%08X <- 0x%08X\n", s_phase, gc, value);
    }
}

int main(int argc, char** argv) {
    int i, ok = 1;
    pc_crash_install();
    if (!pc_lowmem_init()) {
        fprintf(stderr, "no low memory\n");
        return 1;
    }
    s_area = (u8*)pc_lowmem_alloc(4 * SLOTS);
    pc_rel_set_resolver(resolve);
    pc_rel_set_store_hook(store);
    for (i = 1; i < argc; i++) {
        FILE* f = fopen(argv[i], "rb");
        long size;
        u32 bssSize;
        void* bss;
        if (f == NULL) {
            fprintf(stderr, "cannot open %s\n", argv[i]);
            ok = 0;
            continue;
        }
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);
        s_mod = (u8*)pc_lowmem_alloc((size_t)size);
        s_mod_size = (size_t)size;
        if (fread(s_mod, 1, (size_t)size, f) != (size_t)size) {
            ok = 0;
        }
        fclose(f);
        bssSize = (u32)s_mod[0x20] << 24 | (u32)s_mod[0x21] << 16 | (u32)s_mod[0x22] << 8 | s_mod[0x23];
        bss = bssSize ? pc_lowmem_alloc(bssSize) : NULL;
        printf("module %s\n", argv[i]);
        s_phase = "prolog";
        if (!OSLink(s_mod, bss)) {
            printf("OSLink FAILED\n");
            ok = 0;
            continue;
        }
        if (getenv("PC_REL_DUMP_DIR") != NULL) { /* the linked module, for port/tools/rel_check.py --data */
            char out[1024];
            const char* base = strrchr(argv[i], '/');
            const char* base2 = strrchr(argv[i], '\\');
            FILE* d;
            base = base2 > base ? base2 : base;
            snprintf(out, sizeof(out), "%s/%s.linked", getenv("PC_REL_DUMP_DIR"), base ? base + 1 : argv[i]);
            d = fopen(out, "wb");
            if (d != NULL) {
                u32 at = (u32)(uintptr_t)s_mod;
                fwrite(&at, 4, 1, d); /* the address it was linked at (native), then the module bytes */
                fwrite(s_mod, 1, s_mod_size, d);
                fclose(d);
            }
        }
        s_phase = "epilog";
        if (!OSUnlink(s_mod)) {
            printf("OSUnlink FAILED\n");
            ok = 0;
        }
    }
    printf("result: %s\n", ok ? "ALL LINKED" : "SOME FAILED");
    return ok ? 0 : 1;
}
