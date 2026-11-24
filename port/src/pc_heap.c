/*
 * pc_heap.c: statistics for the game's SDK heaps (option 1's heap check, snatcherLeo's requirement: log peak use and failed
 * allocations, 2026-10-17).
 *
 * The game's own OSAlloc (src/dolphin/sdk_range_8009A2D8.c: OSAllocFromHeap / OSFreeToHeap) calls these under
 * TARGET_PC with the cell size it took or gave back. With the arena now the console's (0x80496740-0x81700000), the SDK
 * heap is the retail size ([arenaLo + 14.5 MB, arenaHi), about 3.9 MB; the GS heap below it is fixed), so a PC struct
 * bigger than the GameCube's can run it dry where the console does not.
 *
 *   ORRE_HEAP_LOG=1   log every failed allocation (heap, size, bytes in use, caller) and print each heap's use and
 *                     high-water mark at exit; the debug command `heaps` prints the same at any time.
 *
 * Counting is always on (a few additions per allocation); only the log lines need the switch.
 *
 * With the switch on (2026-11-07, the reference playthrough P12's crash: the HSD heap ran dry 2 h 40 min into a sitting) also:
 *   - every 18,000 game frames (5 minutes), one line per heap: bytes in use and the peak, so a slow leak shows as a
 *     rising "in use" across rooms and battles;
 *   - on a failed allocation, a census of that heap (pc_heap_census, called by the allocator): its allocated cells
 *     grouped by size (the commonest sizes and their bytes), the free cells' total and the largest free cell, so
 *     a full heap and a fragmented one can be told apart, and a leaking object shows as one size piling up.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PC_HEAP_MAX = 16 };
extern volatile unsigned int pc_vi_heartbeat; /* pc_vi.c: the game frame count */

static long s_used[PC_HEAP_MAX], s_peak[PC_HEAP_MAX];
static long s_size[PC_HEAP_MAX]; /* each heap's size when created (pc_heap_note_create) */
static unsigned s_failures[PC_HEAP_MAX], s_allocs[PC_HEAP_MAX];
static int s_log = -1;
static int s_census_due; /* set every 18,000 frames with the log on; taken by pc_heap_census_due */

/* The allocator asks after each allocation whether a periodic census is due (once per 18,000 frames). */
int pc_heap_census_due(void) {
    if (s_census_due) {
        s_census_due = 0;
        return 1;
    }
    return 0;
}

static int heap_log(void) {
    if (s_log < 0) {
        s_log = getenv("ORRE_HEAP_LOG") != NULL;
        if (s_log) {
            void pc_heap_report(void);
            atexit(pc_heap_report);
        }
    }
    return s_log;
}

/* ORRE_HEAP_TRACE=<cell size>[,<cell size>...] (2026-11-07, P12's leak: 1248- and 608-byte cells pile up):
 * the first 40 allocations of each listed cell size print the host call stack (addresses for addr2line), so the
 * code that makes the leaking objects can be named. Observe-only; Windows only (elsewhere one return address). */
#ifdef _WIN32
#include <windows.h>
#endif
static void heap_trace(int heap, unsigned cellSize) {
    static int s_init;
    static unsigned s_sizes[8], s_counts[8];
    static int s_n;
    int i;
    if (!s_init) {
        const char* env = getenv("ORRE_HEAP_TRACE");
        s_init = 1;
        while (env != NULL && *env != '\0' && s_n < 8) {
            char* end;
            const unsigned long v = strtoul(env, &end, 0);
            if (end == env) {
                break;
            }
            s_sizes[s_n++] = (unsigned)v;
            env = (*end == ',') ? end + 1 : end;
        }
    }
    for (i = 0; i < s_n; i++) {
        if (s_sizes[i] == cellSize && s_counts[i] < 40) {
            void* frames[24];
            int n, k;
            s_counts[i]++;
#ifdef _WIN32
            n = (int)CaptureStackBackTrace(1, 24, frames, NULL);
#else
            frames[0] = __builtin_return_address(0);
            n = 1;
#endif
            fprintf(stderr, "[heap] trace heap %d cell %u (#%u) at game frame %u:", heap, cellSize, s_counts[i],
                    (unsigned)pc_vi_heartbeat);
            for (k = 0; k < n; k++) {
                fprintf(stderr, " %p", frames[k]);
            }
            fprintf(stderr, "\n");
        }
    }
}

/* cellSize: the bytes the allocation took (header included), 0 when it failed */
void pc_heap_note_alloc(int heap, unsigned size, unsigned cellSize, const void* caller) {
    if (heap < 0 || heap >= PC_HEAP_MAX) {
        return;
    }
    if (cellSize == 0) {
        s_failures[heap]++;
        if (heap_log()) {
            fprintf(stderr, "[heap] heap %d: allocation of %u bytes FAILED (%ld bytes in use, peak %ld; called from %p)\n",
                    heap, size, s_used[heap], s_peak[heap], caller);
        }
        return;
    }
    s_allocs[heap]++;
    s_used[heap] += cellSize;
    heap_trace(heap, cellSize);
    if (s_used[heap] > s_peak[heap]) {
        s_peak[heap] = s_used[heap];
    }
    if (heap_log()) { /* registers the exit report once the switch is on */
        static unsigned s_last_period = 0;
        const unsigned period = pc_vi_heartbeat / 18000u;
        if (period != s_last_period) {
            int h;
            s_last_period = period;
            s_census_due = 1; /* the allocator runs a census of this heap after this allocation */
            for (h = 0; h < PC_HEAP_MAX; h++) {
                if (s_size[h] != 0) {
                    fprintf(stderr, "[heap] game frame %u: heap %d in use %ld of %ld (peak %ld), %u allocations\n",
                            (unsigned)pc_vi_heartbeat, h, s_used[h], s_size[h], s_peak[h], s_allocs[h]);
                }
            }
        }
    }
}

/* The GS heap (GSmem, gs_mem.c), with the log on: its free space once per 18,000 frames, and any failed allocation.
 *
 * ORRE_HEAP_LOG_GS=<frames> (the project's build helper, 2026-11-18, port-gaps 83: the move sweep's GS allocation above 4 GB)
 * shortens that period, so a long replay shows the GS heap's high point battle by battle instead of every 5 minutes.
 * Each line also carries the lowest free total and the smallest "largest free block" seen so far (the heap's high-water
 * mark of use, sampled), and the free list's address range: the GS heap is fixed inside the console's main memory
 * (its own area, logged at start, is 0x8048E740-0x8130E740), so a free block that starts or ends outside main memory
 * means the list was overwritten, not that the heap grew. Observe-only; needs ORRE_HEAP_LOG too. Same on every platform (plain stderr lines). */
int pc_heap_gsmem_due(void) {
    static unsigned s_last = 0;
    static unsigned s_every = 0;
    if (!heap_log()) {
        return 0;
    }
    if (s_every == 0) {
        const char* env = getenv("ORRE_HEAP_LOG_GS");
        const long n = env != NULL ? strtol(env, NULL, 10) : 0;
        s_every = n >= 60 ? (unsigned)n : 18000u; /* at least a second apart: the walk visits every free block */
    }
    const unsigned period = pc_vi_heartbeat / s_every;
    if (period == s_last) {
        return 0;
    }
    s_last = period;
    return 1;
}

/* port-gaps 83 (2026-11-18): GSmem's top-down search met a free block whose end lies past 4 GB, which only an
 * overwritten size can give. Print the block, its size and 64 bytes before and 32 after its header (the end of
 * whatever was allocated just below it), always (the game stops at the pointer check right after). Every platform. */
void pc_heap_gsmem_bad(const void* block, unsigned size, unsigned allocationSize) {
    const unsigned char* p = (const unsigned char*)block;
    fprintf(stderr, "[heap] GS heap: free block %p claims %u bytes (0x%x) for an allocation of %u; bytes around it:\n",
            block, size, size, allocationSize);
    for (int row = -4; row < 2; row++) {
        fprintf(stderr, "[heap]   %p:", (const void*)(p + row * 16));
        for (int i = 0; i < 16; i++) {
            fprintf(stderr, " %02x", p[row * 16 + i]);
        }
        fprintf(stderr, "\n");
    }
}

/* ORRE_HEAP_GS_WATCH=<address> (port-gaps 83, 2026-11-18): every GS allocation (GSmemShrink, kind 'A') and free
 * (GSmemFree, 'F') whose block covers that console address prints the host call stack, so a use after free can be
 * tied to the code that freed the block and the code that owned it. Observe-only; the stack is Windows-only (on Linux
 * and macOS one return address; backtrace() would do there, likely). */
void pc_heap_gsmem_watch(const void* block, unsigned size, int kind) {
    static int s_init;
    static unsigned long long s_watch;
    if (!s_init) {
        const char* env = getenv("ORRE_HEAP_GS_WATCH");
        s_init = 1;
        s_watch = env != NULL ? strtoull(env, NULL, 0) : 0;
    }
    const unsigned long long at = (unsigned long long)(uintptr_t)block;
    if (s_watch == 0 || s_watch < at || s_watch >= at + size) {
        return;
    }
    void* frames[24];
    int n, k;
#ifdef _WIN32
    n = (int)CaptureStackBackTrace(1, 24, frames, NULL);
#else
    frames[0] = __builtin_return_address(0);
    n = 1;
#endif
    fprintf(stderr, "[heap] GS watch: %s of %p (%u bytes) at game frame %u, stack:", kind == 'F' ? "free" : "allocation",
            block, size, (unsigned)pc_vi_heartbeat);
    for (k = 0; k < n; k++) {
        fprintf(stderr, " %p", frames[k]);
    }
    fprintf(stderr, "\n");
}

/* ORRE_HEAP_GS_CHECK=1 (port-gaps 83): gs_mem.c checks the free list around every operation (GSmemPcCheck) and
 * reports the first bad block here, once, with the host stack. Observe-only. */
static int s_gs_check = -1; /* -1: not read yet; 0: off, or already reported once; 1: on */
static const char* s_gs_lastWhere = "none";  /* the last check that passed, so a report brackets the overwrite */
static unsigned s_gs_lastFrame;
static const char* s_gs_nowWhere = "none";
static unsigned s_gs_nowFrame;
static void* s_gs_lastStack[16];
static void* s_gs_nowStack[16];
static int s_gs_lastN, s_gs_nowN;
int pc_heap_gsmem_check_on(const char* where) {
    if (s_gs_check < 0) {
        s_gs_check = getenv("ORRE_HEAP_GS_CHECK") != NULL;
    }
    if (s_gs_check != 1) {
        return 0;
    }
    s_gs_lastWhere = s_gs_nowWhere; /* the previous check finished without a report: it passed */
    s_gs_lastFrame = s_gs_nowFrame;
    memcpy(s_gs_lastStack, s_gs_nowStack, sizeof s_gs_lastStack);
    s_gs_lastN = s_gs_nowN;
    s_gs_nowWhere = where;
    s_gs_nowFrame = (unsigned)pc_vi_heartbeat;
#ifdef _WIN32
    s_gs_nowN = (int)CaptureStackBackTrace(1, 16, s_gs_nowStack, NULL);
#else
    s_gs_nowStack[0] = __builtin_return_address(0);
    s_gs_nowN = 1;
#endif
    return 1;
}

void pc_heap_gsmem_check_fail(const char* where, const void* block, unsigned size, const void* next, int index) {
    void* frames[24];
    int n, k;
    fprintf(stderr, "[heap] GS check: free list entry %d at %p (size %u, next %p) is bad, %s, game frame %u; stack:",
            index, block, size, next, where, (unsigned)pc_vi_heartbeat);
#ifdef _WIN32
    n = (int)CaptureStackBackTrace(1, 24, frames, NULL);
#else
    frames[0] = __builtin_return_address(0);
    n = 1;
#endif
    for (k = 0; k < n; k++) {
        fprintf(stderr, " %p", frames[k]);
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "[heap] GS check: the last check that passed: %s, game frame %u; stack:", s_gs_lastWhere,
            s_gs_lastFrame);
    for (k = 0; k < s_gs_lastN; k++) {
        fprintf(stderr, " %p", s_gs_lastStack[k]);
    }
    fprintf(stderr, "\n");
    if (block != NULL && size != 0) { /* inside the heap: the bytes around its header */
        pc_heap_gsmem_bad(block, size, 0);
    }
    s_gs_check = 0; /* once: later operations would see the same bad block */
}

/* port-gaps 83: one line per free-list entry from the head up to the bad block (gs_mem.c walks, this prints). */
void pc_heap_gsmem_chain(int index, const void* block, unsigned size, const void* next) {
    fprintf(stderr, "[heap]   free list %d: block %p size %u (0x%x) next %p\n", index, block, size, size, next);
}

void pc_heap_gsmem_report(long freeTotal, long largest, unsigned blocks, unsigned int failedSize, unsigned long long lowest,
                          unsigned long long reachEnd) {
    static long s_minFree = -1, s_minLargest = -1;
    if (!heap_log()) {
        return;
    }
    if (failedSize != 0) {
        fprintf(stderr, "[heap] GS heap: allocation of %u bytes FAILED at game frame %u\n", failedSize,
                (unsigned)pc_vi_heartbeat);
        return;
    }
    if (s_minFree < 0 || freeTotal < s_minFree) {
        s_minFree = freeTotal;
    }
    if (s_minLargest < 0 || largest < s_minLargest) {
        s_minLargest = largest;
    }
    /* The console's main memory (24 MB from 0x80000000; the GS heap itself logs its area at start, 0x8048E740 to
     * 0x8130E740 in the US game): a free block outside it can only be an overwrite. */
    const int outside = blocks != 0 && (lowest < 0x80000000ull || reachEnd > 0x81800000ull);
    fprintf(stderr,
            "[heap] game frame %u: GS heap free %ld bytes in %u blocks, largest %ld (lowest free so far %ld, smallest "
            "largest %ld), free list 0x%llx-0x%llx%s\n",
            (unsigned)pc_vi_heartbeat, freeTotal, blocks, largest, s_minFree, s_minLargest, lowest, reachEnd,
            outside ? " OUTSIDE the arena" : "");
}

/* A failed allocation's census (the allocator walks the heap's cell lists and calls this once per cell, then once
 * with last = 1): the allocated cells grouped by size, the free cells' total and the largest. */
void pc_heap_census(int heap, int isFree, long cellSize, int last) {
    enum { SIZES = 512 };
    static long sz[SIZES];
    static unsigned cnt[SIZES];
    static int nsz;
    static long freeTotal, freeLargest, usedTotal;
    static unsigned freeCells, usedCells, other;
    static unsigned reports;
    int i;
    if (!heap_log() || reports >= 200) {
        return;
    }
    if (!last) {
        if (isFree) {
            freeTotal += cellSize;
            freeCells++;
            if (cellSize > freeLargest) {
                freeLargest = cellSize;
            }
            return;
        }
        usedTotal += cellSize;
        usedCells++;
        for (i = 0; i < nsz; i++) {
            if (sz[i] == cellSize) {
                cnt[i]++;
                return;
            }
        }
        if (nsz < SIZES) {
            sz[nsz] = cellSize;
            cnt[nsz++] = 1;
        } else {
            other++;
        }
        return;
    }
    reports++;
    fprintf(stderr,
            "[heap] census of heap %d at game frame %u: %u allocated cells, %ld bytes; %u free cells, %ld bytes, "
            "largest %ld\n",
            heap, (unsigned)pc_vi_heartbeat, usedCells, usedTotal, freeCells, freeTotal, freeLargest);
    for (i = 0; i < 25; i++) { /* the 25 sizes holding the most bytes */
        int best = -1, j;
        for (j = 0; j < nsz; j++) {
            if (cnt[j] != 0 && (best < 0 || sz[j] * (long)cnt[j] > sz[best] * (long)cnt[best])) {
                best = j;
            }
        }
        if (best < 0) {
            break;
        }
        fprintf(stderr, "[heap]   cell size %ld x %u = %ld bytes\n", sz[best], cnt[best], sz[best] * (long)cnt[best]);
        cnt[best] = 0;
    }
    if (other) {
        fprintf(stderr, "[heap]   (%u cells of further sizes not grouped)\n", other);
    }
    nsz = 0;
    freeTotal = freeLargest = usedTotal = 0;
    freeCells = usedCells = other = 0;
}

void pc_heap_note_free(int heap, unsigned cellSize) {
    if (heap >= 0 && heap < PC_HEAP_MAX) {
        s_used[heap] -= cellSize;
    }
}

/* heapSize: the heap's size when created (OSCreateHeap), for the report; resets its counters */
void pc_heap_note_create(int heap, unsigned heapSize) {
    if (heap >= 0 && heap < PC_HEAP_MAX) {
        s_size[heap] = heapSize;
        s_used[heap] = s_peak[heap] = 0;
        s_failures[heap] = s_allocs[heap] = 0;
    }
}

void pc_heap_report(void) {
    int heap, any = 0;
    for (heap = 0; heap < PC_HEAP_MAX; heap++) {
        if (s_size[heap] == 0 && s_allocs[heap] == 0 && s_failures[heap] == 0) {
            continue;
        }
        any = 1;
        fprintf(stderr, "[heap] heap %d: %ld bytes, %ld in use, peak %ld (%ld%%), %u allocations, %u failed\n", heap,
                s_size[heap], s_used[heap], s_peak[heap], s_size[heap] ? s_peak[heap] * 100 / s_size[heap] : 0,
                s_allocs[heap], s_failures[heap]);
    }
    if (!any) {
        fprintf(stderr, "[heap] no SDK heap activity recorded (the allocator's TARGET_PC calls are not in this build)\n");
    }
}
