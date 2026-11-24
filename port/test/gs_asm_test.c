/*
 * gs_asm_test: checks the PC versions of the remaining platform routines (docs/port/asm-routines.md):
 * the PPCArch register accessors (pc_cpu.c), GSmathInit, GSscratchInit with the game's real scratch allocator
 * (src/game/gs_scratch_800EE928.c and gs_scratch_alloc_800EEC38.c, compiled unchanged), the script VM's native-call
 * trampoline with the argument orders found on the disc, and HSD_SaveContext. Exit code 0 = all passed.
 *
 *   cmake --build build-pc --target gs_asm_test && build-pc/gs_asm_test.exe
 */
#include <dolphin/types.h>
#include <dolphin/base/PPCArch.h>
#include <dolphin/os/OSContext.h>

#include "gs_asm_host.h"
#include "pc_cpu.h"
#include "pc_mem.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int check(const char* what, int ok) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

/* --- globals the routines use (in the real port they come with the game's data) --- */
typedef struct GSscratchAllocation {
    u8 firstBlock;
    u8 blockCount;
    u8 _pad[2];
    void (*callback)(BOOL valid, void* ptr, u8 blockCount);
} GSscratchAllocation;
GSscratchAllocation _blockTable[32];
u8* _scratchPadBase;
u8 _scratchInvalid;
u32 _blockMap;
u32 lbl_8047ABD8;
u32 _entriesQueued;
u32 (*lbl_8047AC38)(void);
f32* lbl_8047AC3C;
u32* lbl_8047AC40;
OSContext lbl_80465080;

/* Aurora provides these on the real build; the test does not link Aurora */
void LCEnable(void) {}
void LCQueueWait(u32 len) { (void)len; }
u32 LCQueueLength(void) { return 0; }
u32 LCStoreData(void* dest, void* src, u32 len) { memcpy(dest, src, len); return 1; }

static int s_seeded, s_costable;
void fn_800E0C78(void) { s_seeded++; }
void GSmathInitCosTable(void) { s_costable++; }

extern BOOL GSscratchInit(u8 reservedBlocks);
extern void GSmathInit(void);
extern void HSD_SaveContext(void);
extern u32 cms_CallStdTrampoline(void);
extern u8 GSscratchIsPtr(void* ptr);
extern void* GSscratchAlloc(u8 blockCount, void (*callback)(BOOL valid, void* ptr, u8 blockCount));
extern void GSscratchFree(void* ptr);

/* --- natives with the mixed argument orders found in the disc's native table --- */
static u32 n_none(void) { return 7; }
static u32 n_fi(f32 a, u32 b) { return a == 1.5f && b == 10 ? 1 : 0; }
static u32 n_if(u32 a, f32 b) { return a == 10 && b == 1.5f ? 1 : 0; }
static u32 n_iiiiii(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f) {
    return a == 10 && b == 11 && c == 12 && d == 13 && e == 14 && f == 15 ? 1 : 0;
}
static u32 n_ffffff(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f) {
    return a == 1.5f && b == 2.5f && c == 3.5f && d == 4.5f && e == 5.5f && f == 6.5f ? 1 : 0;
}
static u32 n_iffff(u32 a, f32 b, f32 c, f32 d, f32 e) {
    return a == 10 && b == 1.5f && c == 2.5f && d == 3.5f && e == 4.5f ? 1 : 0;
}
static u32 n_iiiifi(u32 a, u32 b, u32 c, u32 d, f32 e, u32 f) {
    return a == 10 && b == 11 && c == 12 && d == 13 && e == 1.5f && f == 14 ? 1 : 0;
}
static u32 n_iifffffi(u32 a, u32 b, f32 c, f32 d, f32 e, f32 f, f32 g, u32 h) {
    return a == 10 && b == 11 && c == 1.5f && d == 2.5f && e == 3.5f && f == 4.5f && g == 5.5f && h == 12 ? 1 : 0;
}
/* a pointer argument: script integers that are MEM1 addresses must arrive as usable pointers */
static u32 n_ptr(u32* p) { return *p; }

static u32 call_native(void* fn, const char* pattern) {
    static u32 ints[8];
    static f32 floats[8];
    static u8 types[8];
    int k, ni = 0, nf = 0;
    memset(ints, 0, sizeof(ints));
    memset(floats, 0, sizeof(floats));
    memset(types, 0, sizeof(types));
    /* fill the arrays the way cms_op37_callStandard does: integers 10, 11, ... and floats 1.5, 2.5, ... in turn */
    for (k = 0; pattern[k] != 0; k++) {
        if (pattern[k] == 'f') {
            types[k] = 2;
            floats[nf] = 1.5f + nf;
            nf++;
        } else {
            types[k] = 1;
            ints[ni] = 10 + ni;
            ni++;
        }
    }
    lbl_8047AC38 = (u32(*)(void))fn;
    lbl_8047AC40 = ints;
    lbl_8047AC3C = floats;
    pc_cms_native_types = types;
    return cms_CallStdTrampoline();
}

int main(void) {
    int ok = 1, i, records_free = 1;
    u8* blk;
    u32* gcword;

    if (!pc_mem_reserve(PC_MEM1_RETAIL_SIZE)) {
        return 2;
    }

    /* PPCArch register accessors */
    PPCMthid2(0x12345678);
    PPCMtwpar(0x0C008000);
    PPCMtfpscr(0x4);
    PPCSync();
    ok &= check("PPC register writes read back (HID2, WPAR, FPSCR)",
                PPCMfhid2() == 0x12345678 && PPCMfwpar() == 0x0C008000 && PPCMffpscr() == 0x4);
    ok &= check("PPCMfmsr has interrupts on (EE), PPCOrMsr returns the old value",
                (PPCMfmsr() & 0x8000) && PPCOrMsr(0x1) == 0x00009032 && PPCMfmsr() == 0x00009033);
    ok &= check("PPCMfpvr reports Gekko", PPCMfpvr() == 0x00083214);

    /* GSmathInit */
    GSmathInit();
    ok &= check("GSmathInit sets GQR2-5 (u8 / u16 / s8 / s16) and still seeds the RNG and builds the cos table",
                pc_gqr[2] == 0x00040004 && pc_gqr[3] == 0x00050005 && pc_gqr[4] == 0x00060006 &&
                    pc_gqr[5] == 0x00070007 && s_seeded == 1 && s_costable == 1);

    /* GSscratchInit + the game's own allocator */
    memset(_blockTable, 0, sizeof(_blockTable));
    ok &= check("GSscratchInit(0) returns TRUE", GSscratchInit(0) == TRUE);
    for (i = 0; i < 32; i++) {
        records_free &= _blockTable[i].firstBlock == 0xFF;
    }
    ok &= check("all 32 records free, nothing reserved, base 0xE0000000",
                records_free && _blockMap == 0 && lbl_8047ABD8 == 0 &&
                    _scratchPadBase == (u8*)(uintptr_t)PC_LOCKED_CACHE_BASE);
    blk = (u8*)GSscratchAlloc(2, NULL);
    ok &= check("game GSscratchAlloc(2 blocks) hands out locked-cache memory",
                blk != NULL && (uintptr_t)blk >= PC_LOCKED_CACHE_BASE &&
                    (uintptr_t)blk + 0x400 <= PC_LOCKED_CACHE_BASE + PC_LOCKED_CACHE_SIZE);
    if (blk != NULL) {
        memset(blk, 0x5A, 0x400);
    }
    gcword = (u32*)pc_mem_from_gc(PC_MEM1_BASE + 0x2000);
    ok &= check("game GSscratchIsPtr: scratch yes, MEM1 no (stacks stay put)",
                blk != NULL && GSscratchIsPtr(blk) && !GSscratchIsPtr(gcword));
    if (blk != NULL) {
        GSscratchFree(blk);
    }
    ok &= check("game GSscratchFree releases the blocks", _blockMap == 0);
    ok &= check("GSscratchInit(3) reserves blocks 0-2", GSscratchInit(3) == TRUE && _blockMap == 0xE0000000u &&
                                                             _blockTable[0].firstBlock == 0 &&
                                                             _blockTable[0].blockCount == 3 &&
                                                             lbl_8047ABD8 == 0x600);
    ok &= check("GSscratchInit(32) reserves all blocks", GSscratchInit(32) == TRUE && _blockMap == 0xFFFFFFFFu);
    GSscratchInit(0);

    /* the script VM's native calls, in orders found on the disc */
    ok &= check("native ()", call_native((void*)n_none, "") == 7);
    ok &= check("native (f, i)", call_native((void*)n_fi, "fi") == 1);
    ok &= check("native (i, f)", call_native((void*)n_if, "if") == 1);
    ok &= check("native (i x6)", call_native((void*)n_iiiiii, "iiiiii") == 1);
    ok &= check("native (f x6)", call_native((void*)n_ffffff, "ffffff") == 1);
    ok &= check("native (i, f x4)", call_native((void*)n_iffff, "iffff") == 1);
    ok &= check("native (i x4, f, i)", call_native((void*)n_iiiifi, "iiiifi") == 1);
    ok &= check("native (i, i, f x5, i), the longest mix", call_native((void*)n_iifffffi, "iifffffi") == 1);
    *gcword = 0xC0105E0;
    {
        static u32 ints[8];
        static f32 floats[8];
        static const u8 types[8] = {1};
        ints[0] = pc_mem_to_gc(gcword);
        lbl_8047AC38 = (u32(*)(void))(void*)n_ptr;
        lbl_8047AC40 = ints;
        lbl_8047AC3C = floats;
        pc_cms_native_types = types;
        ok &= check("native with a MEM1 pointer passed as a script integer", cms_CallStdTrampoline() == 0xC0105E0);
    }

    /* HSD_SaveContext */
    memset(&lbl_80465080, 0xAA, sizeof(lbl_80465080));
    HSD_SaveContext();
    {
        const u8* c = (const u8*)&lbl_80465080;
        int clear = 1;
        for (i = 0; i < (int)sizeof(lbl_80465080); i++) {
            clear &= c[i] == 0;
        }
        ok &= check("HSD_SaveContext clears the panic context", clear);
    }

    printf("result: %s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
