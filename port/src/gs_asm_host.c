/*
 * gs_asm_host.c: PC versions of the GS engine's remaining first-party assembly routines (docs/port/asm-routines.md):
 * GSmathInit, GSscratchInit and the script VM's native-call trampoline cms_CallStdTrampoline. The thread-switch
 * routines are in gs_thread_host.c, HSD_SaveContext in hsd_debug_host.c.
 */
#include "dolphin/types.h"

#include "gs_asm_host.h"
#include "pc_cpu.h"
#include "pc_mem.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ===== GSmathInit (GameCube: src/game/gs_math_init.c, 0x800E0790) =====
 * Programs GQR2-GQR5 for quantised loads and stores (u8, u16, s8, s16, scale 0, both directions), seeds the
 * random number generator from the clock (fn_800E0C78) and builds the cosine table. The PC has no GQRs, so the
 * values go into the platform layer's shadow copy (pc_cpu.h), where PC code reading quantised data finds them. */
extern void fn_800E0C78(void);
extern void GSmathInitCosTable(void);

void GSmathInit(void) {
    pc_gqr[2] = 0x00040004;
    pc_gqr[3] = 0x00050005;
    pc_gqr[4] = 0x00060006;
    pc_gqr[5] = 0x00070007;
    fn_800E0C78();
    GSmathInitCosTable();
}

/* ===== GSscratchInit (GameCube: src/game/gs_scratch_exact_800EEDF8.c, 0x800EEDF8) =====
 * Sets up the scratch allocator: 32 allocation records (firstBlock = 0xFF for "free"), a used-block bitmask, and the base of the 16 KB locked cache (32 blocks of 0x200 bytes).
 * The first `reservedBlocks` blocks can be reserved, and on the GameCube the caller's stack is then moved into them.
 *
 * On the PC the locked cache is ordinary memory mapped at its real address, 0xE0000000 (pc_mem.h), so
 * GSscratchIsPtr's "top four address bits" test keeps telling scratch pointers from main memory. The stack move is
 * left out: Colosseum only ever calls GSscratchInit(0) (main.c), which never moves the stack, and moving a host
 * stack is not possible from C. The bookkeeping follows the old C at tag pre-upstream-merge. */
/* The game's record type (src/game/gs_scratch*.c define it locally; keep in sync). It holds a callback pointer, so
 * it is 8 bytes on the GameCube and 16 on a 64-bit PC: the PC code must index records by type, never by 8. */
typedef struct GSscratchAllocation {
    u8 firstBlock; /* 0xFF: record unused */
    u8 blockCount;
    u8 _pad[2];
    void (*callback)(BOOL valid, void* ptr, u8 blockCount);
} GSscratchAllocation;

extern GSscratchAllocation _blockTable[32];
extern u8* _scratchPadBase;   /* scratch base */
extern u8 _scratchInvalid;
extern u32 _blockMap;   /* used-block mask, block 0 = bit 31 */
extern u32 lbl_8047ABD8;   /* reserved bytes */
extern void LCEnable(void);

BOOL GSscratchInit(u8 reservedBlocks) {
    int i;

    _blockMap = 0;
    for (i = 0; i < 32; i++) {
        _blockTable[i].firstBlock = 0xFF;
    }
    if (!pc_mem_map_locked_cache()) {
        return FALSE;
    }
    LCEnable();
    lbl_8047ABD8 = (u32)reservedBlocks << 9;
    _scratchPadBase = (u8*)(uintptr_t)PC_LOCKED_CACHE_BASE;
    if (reservedBlocks != 0) {
        /* the GameCube sets one bit per block from bit 31 down; more than 32 blocks set them all */
        _blockMap = reservedBlocks >= 32 ? 0xFFFFFFFFu : ~(0xFFFFFFFFu >> reservedBlocks);
        _blockTable[0].firstBlock = 0;
        _blockTable[0].blockCount = reservedBlocks;
        fprintf(stderr, "[gs_asm_host] GSscratchInit(%u): blocks reserved, but the stack stays where it is on the PC\n",
                reservedBlocks);
    }
    _scratchInvalid = 0;
    return TRUE;
}

/* ===== cms_CallStdTrampoline (GameCube: src/game/gs_thread_exact_800F106C.c, 0x800F106C) =====
 * The script VM's "call native": cms_op37_callStandard puts the native's address in lbl_8047AC38 and splits the
 * script arguments into an integer array (lbl_8047AC40) and a float array (lbl_8047AC3C), 8 slots each. The
 * GameCube loads them into r3-r10 and f1-f8 and calls the native, which picks integers and floats in order, each
 * from its own register file. Its result comes back in r3.
 *
 * Linux / macOS (x86-64 System V) and ARM64 pass integers and floats in separate registers too, so one call with
 * 8 integers then 8 floats is correct for every native. Windows x64 does not: argument N uses the N-th register of
 * whichever kind it is, so the call must follow the native's own order of integers and floats. That order is the
 * native table's type bytes (pc_cms_native_types, set by the game code under TARGET_PC). The disc's 246 natives
 * use 22 orders, mixed ones included ("fi", "iiiifi", "iifffffi"); docs/port/asm-routines.md. */
extern u32 (*lbl_8047AC38)(void); /* native to call */
extern f32* lbl_8047AC3C;         /* float arguments */
extern u32* lbl_8047AC40;         /* integer arguments */

const u8* pc_cms_native_types;

#if defined(_WIN64)
/* Windows x64: positions 1-4 travel in registers (RCX/XMM0, RDX/XMM1, R8/XMM2, R9/XMM3) and need the right kind,
 * so there is one call shape per integer / float choice for those four (16 shapes). Positions 5-8 travel in 8-byte
 * stack slots where a float is its 4 bytes in the low half, so an integer slot holding the float's bits works. */
typedef union {
    u32 i;
    f32 f;
} PcArg;

#define T_I u64
#define T_F f32
#define V_I(k) ((u64)a[k].i)
#define V_F(k) (a[k].f)
#define SHAPE(mask, p0, p1, p2, p3)                                                                               \
    case mask:                                                                                                    \
        return ((u32(*)(T_##p0, T_##p1, T_##p2, T_##p3, u64, u64, u64, u64))fn)(V_##p0(0), V_##p1(1), V_##p2(2), \
                                                                                 V_##p3(3), V_I(4), V_I(5),     \
                                                                                 V_I(6), V_I(7));

u32 pc_cms_call_native(void* fn, const u8* types, const u32* ints, const f32* floats) {
    PcArg a[8] = {0};
    int k, ni = 0, nf = 0;
    unsigned mask = 0;

    for (k = 0; k < 8; k++) {
        if (types == NULL || types[k] == 0) {
            break;
        }
        if (types[k] == 2) {
            a[k].f = floats[nf++];
            if (k < 4) {
                mask |= 1u << k;
            }
        } else {
            a[k].i = ints[ni++];
        }
    }
    if (types == NULL) {
        /* No type bytes: only integer natives can be called correctly (194 of the 246) */
        for (k = 0; k < 8; k++) {
            a[k].i = ints[k];
        }
        for (k = 0; k < 8; k++) {
            if (floats[k] != 0.0f) {
                fprintf(stderr, "[gs_asm_host] native %p called with float arguments but no type bytes; "
                                "set pc_cms_native_types\n", fn);
                break;
            }
        }
    }
    switch (mask) {
        SHAPE(0x0, I, I, I, I)
        SHAPE(0x1, F, I, I, I)
        SHAPE(0x2, I, F, I, I)
        SHAPE(0x3, F, F, I, I)
        SHAPE(0x4, I, I, F, I)
        SHAPE(0x5, F, I, F, I)
        SHAPE(0x6, I, F, F, I)
        SHAPE(0x7, F, F, F, I)
        SHAPE(0x8, I, I, I, F)
        SHAPE(0x9, F, I, I, F)
        SHAPE(0xA, I, F, I, F)
        SHAPE(0xB, F, F, I, F)
        SHAPE(0xC, I, I, F, F)
        SHAPE(0xD, F, I, F, F)
        SHAPE(0xE, I, F, F, F)
        SHAPE(0xF, F, F, F, F)
    }
    return 0;
}

#undef SHAPE
#undef T_I
#undef T_F
#undef V_I
#undef V_F

#else
/* System V x86-64 and ARM64: integer and float arguments fill separate registers, as on the GameCube */
u32 pc_cms_call_native(void* fn, const u8* types, const u32* ints, const f32* floats) {
    (void)types;
    return ((u32(*)(u32, u32, u32, u32, u32, u32, u32, u32, f32, f32, f32, f32, f32, f32, f32, f32))fn)(
        ints[0], ints[1], ints[2], ints[3], ints[4], ints[5], ints[6], ints[7], floats[0], floats[1], floats[2],
        floats[3], floats[4], floats[5], floats[6], floats[7]);
}
#endif

/* Per-native thunks (the project's tools/local/port/nativethunks.py, generated into build-pc): each passes the native its
 * arguments by its C signature, integer parameter n <- integer slot n, float parameter m <- float slot m. That is
 * what the GameCube's trampoline amounts to (it always loads all 8 r and 8 f slots), including natives that read
 * arguments their table entry never supplies (leftover slots: heroMoveInit, peopleOpen, std200, ...). Registered
 * by the game's PC main; the type-byte path above stays as the fallback. */
typedef struct PcNativeThunk {
    void* native;
    u32 (*thunk)(const u32* i, const f32* f);
} PcNativeThunk;

static PcNativeThunk* s_thunks;
static unsigned s_thunk_count;

static int thunk_order(const void* a, const void* b) {
    const char* x = (const char*)((const PcNativeThunk*)a)->native;
    const char* y = (const char*)((const PcNativeThunk*)b)->native;
    return x < y ? -1 : x > y;
}

void pc_cms_set_thunks(const PcNativeThunk* table, unsigned count) {
    free(s_thunks);
    s_thunks = (PcNativeThunk*)malloc(sizeof(*s_thunks) * count);
    if (s_thunks == NULL) {
        s_thunk_count = 0;
        return;
    }
    memcpy(s_thunks, table, sizeof(*s_thunks) * count);
    qsort(s_thunks, count, sizeof(*s_thunks), thunk_order);
    s_thunk_count = count;
}

u32 cms_CallStdTrampoline(void) {
    void* fn = (void*)lbl_8047AC38;
    unsigned lo = 0, hi = s_thunk_count;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if ((const char*)s_thunks[mid].native < (const char*)fn) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if (lo < s_thunk_count && s_thunks[lo].native == fn) {
        return s_thunks[lo].thunk(lbl_8047AC40, lbl_8047AC3C);
    }
    return pc_cms_call_native(fn, pc_cms_native_types, lbl_8047AC40, lbl_8047AC3C);
}
