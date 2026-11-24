/* ARAM copy completions at the console's time (2026-10-21).
 *
 * The GameCube copies between main memory and ARAM by DMA and signals the end with an interrupt, so a request's
 * completion callback runs a little after the request, never inside it. Aurora's ARQPostRequest copies at once and
 * calls the callback before returning. The FSYS loader keeps archives it has unloaded in ARAM and reloads them from
 * there (fsysLoadEntry: an async copy whose callback, fsysARQCallback, moves the slot to status 162); on the PC the
 * slot went on in the same game frame, one frame (two retraces at 30 fps) ahead of the console. Found by P01's Willie
 * battle: Dolphin's slots sit in status 162 for two retraces at every reload from ARAM, the PC's never, so the move
 * effects loaded earlier and the fifth move camera picked its shot two retraces early (verified with the fsys probe,
 * fsys.csv vs ORRE_FSYS_LOG).
 *
 * With the replay clock (pc_vi.c), a copy that has a completion callback is done at once but handed to the game at the
 * game thread's next interrupt point after the next retrace, like the console's interrupt. Copies without a callback
 * are the loader's synchronous ones (they spin until done) and stay immediate. ORRE_ARQ_LATENCY=N sets the retraces
 * (default 1; 0 = Aurora's immediate callback). Live play keeps the immediate callback for now (the deliver hooks run
 * only with the replay clock). */
#include <dolphin/types.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void ARQPostRequest(void* request, u32 owner, u32 type, u32 priority, uintptr_t source, uintptr_t dest,
                           u32 length, void (*callback)(uintptr_t request));
extern u32 VIGetRetraceCount(void);
extern int pc_vi_virtual_clock(void);                  /* pc_vi.c */
extern void pc_os_set_deliver_hook(void (*hook)(void)); /* pc_os_thread.c: runs at interrupt points (replay clock) */

enum { ARQ_PENDING = 64 };
typedef struct PendingCopy {
    void (*callback)(uintptr_t request);
    uintptr_t request;
    u32 due;
    u32 seq;
} PendingCopy;
static PendingCopy s_pending[ARQ_PENDING];
static u32 s_seq;
static int s_latency = -1;
/* Removed 2026-10-26 (snatcherLeo: PC time; the project notes): ORRE_ARQ_LATENCY=dolphin (Dolphin's ARAM
 * DMA time from the request's point in the field: no change on P01) and =next (the next interrupt point: broke Willie's
 * battle) */

static int copy_due(const PendingCopy* p, u32 now) {
    return (s32)(p->due - now) <= 0;
}

/* at the game thread's interrupt points (interrupts disabled): every copy due by now, in request order */
static void pc_arq_deliver(void) {
    const u32 now = VIGetRetraceCount();
    for (;;) {
        int best = -1;
        for (int i = 0; i < ARQ_PENDING; i++) {
            if (s_pending[i].callback != NULL && copy_due(&s_pending[i], now) &&
                (best < 0 || (s32)(s_pending[i].seq - s_pending[best].seq) < 0)) {
                best = i;
            }
        }
        if (best < 0) {
            return;
        }
        {
            void (*callback)(uintptr_t) = s_pending[best].callback;
            const uintptr_t request = s_pending[best].request;
            s_pending[best].callback = NULL;
            {
                const char* env = getenv("ORRE_ARQ_LOG"); /* (observes only, as in pc_arq_post) */
                if (env != NULL && *env == '1') {
                    extern volatile u32 pc_vi_heartbeat;
                    fprintf(stderr, "[arq] handed over at retrace %u, game frame %u\n", (unsigned)now,
                            (unsigned)pc_vi_heartbeat);
                }
            }
            callback(request);
        }
    }
}

/* Pieces bigger than their room (2026-11-19, the Japanese release look's determinism fix; docs/port/word-pictures.md).
 * Why: the FSYS loader caches archive pieces in ARAM, and its allocator's choices (which pieces fit, where) depend on
 * their sizes. A swapped entry from the Japanese disc is bigger than the US one, so with the look on other pieces
 * fitted or didn't, later loads came from ARAM in one run and the disc in the other, and a room's events moved a frame
 * (P03, 2026-11-19). OrreHook_FsysCacheRoom (pc_lang_files.cpp) makes the cache reserve the US piece's room; then
 * fn_8017F928 calls pc_arq_short_block, and the copy into that room keeps the bytes past it here, and a copy back from
 * the same address hands them back, so the next block of ARAM is never written and the piece comes back whole. A block
 * is "armed" by its reservation and filled by the first copy into it; any other copy that writes over its room ends
 * it (the cache freed it and the room went to something else). Plain C, all platforms. */
enum { SHORT_BLOCKS = 32 };
typedef struct ShortBlock {
    u32 aram;   /* the block's ARAM address */
    u32 room;   /* the room reserved (32-byte aligned) */
    u32 size;   /* the piece's real size, aligned */
    u8* tail;   /* the bytes past the room (size - room), once filled */
    int state;  /* 0 unused, 1 armed (reserved, not filled yet), 2 filled */
} ShortBlock;
static ShortBlock s_short[SHORT_BLOCKS];

static void short_drop(ShortBlock* b) {
    free(b->tail);
    b->tail = NULL;
    b->state = 0;
}

void pc_arq_short_block(u32 aram, u32 room, u32 size) {
    ShortBlock* slot = NULL;
    for (int i = 0; i < SHORT_BLOCKS; i++) {
        if (s_short[i].state != 0 && s_short[i].aram == aram) {
            short_drop(&s_short[i]);
        }
        if (s_short[i].state == 0 && slot == NULL) {
            slot = &s_short[i];
        }
    }
    if (slot == NULL) {
        /* more than 32 at once: the oldest use of the table is dropped (logged; nothing uses that many today, the
         * Japanese look has 7 swapped entries) */
        fprintf(stderr, "[arq] more than %d short cache blocks: one dropped\n", SHORT_BLOCKS);
        slot = &s_short[0];
        short_drop(slot);
    }
    slot->aram = aram;
    slot->room = room;
    slot->size = (size + 0x1F) & ~0x1Fu;
    slot->tail = NULL;
    slot->state = 1;
}

/* before a copy: a copy into an armed block's address keeps its bytes past the room (the length shortened to the
 * room); a copy out of a filled block's address hands them back (the tail copied at once, as Aurora copies at once);
 * a copy that writes over any other block's room ends that block */
static u32 short_adjust(u32 type, uintptr_t source, uintptr_t dest, u32 length) {
    const u32 aram = (u32)(type == 0 ? dest : source);
    for (int i = 0; i < SHORT_BLOCKS; i++) {
        ShortBlock* b = &s_short[i];
        if (b->state == 0) {
            continue;
        }
        if (aram == b->aram && length > b->room) {
            const u32 extra = length - b->room;
            if (type == 0 && b->state == 1) {
                b->tail = (u8*)malloc(extra);
                if (b->tail != NULL) {
                    memcpy(b->tail, (const u8*)source + b->room, extra);
                    b->size = length;
                    b->state = 2;
                    return b->room;
                }
                short_drop(b);
                return length;
            }
            if (type == 1 && b->state == 2) {
                const u32 have = b->size - b->room;
                memcpy((u8*)dest + b->room, b->tail, extra < have ? extra : have);
                return b->room;
            }
        }
        if (type == 0 && (u64)aram < (u64)b->aram + b->room && (u64)aram + length > b->aram &&
            !(aram == b->aram && b->state == 1)) {
            short_drop(b); /* its room written by something else */
        }
    }
    return length;
}

void pc_arq_post(void* request, u32 owner, u32 type, u32 priority, uintptr_t source, uintptr_t dest, u32 length,
                 void (*callback)(uintptr_t request), int has_user_callback) {
    static int s_hooked;
    static int s_log = -1;
    const u32 asked = length;
    length = short_adjust(type, source, dest, length);
    /* ORRE_ARQ_LOG=1 (observes only; 2026-11-19): every ARAM copy as it is posted, so a cache copy's moment shows next to
     * the loader's disc reads (ORRE_FSYS_LOG=2) */
    if (s_log < 0) {
        const char* env = getenv("ORRE_ARQ_LOG");
        s_log = env != NULL && *env == '1';
    }
    if (s_log) {
        extern volatile u32 pc_vi_heartbeat;
        fprintf(stderr, "[arq] post %s aram 0x%X len %u%s at retrace %u, game frame %u%s\n", type == 0 ? "to" : "from",
                (unsigned)(type == 0 ? dest : source), (unsigned)asked, length != asked ? " (room only, rest kept aside)" : "",
                (unsigned)VIGetRetraceCount(), (unsigned)pc_vi_heartbeat, has_user_callback ? "" : " (sync)");
    }
    if (s_latency < 0) {
        const char* env = getenv("ORRE_ARQ_LATENCY");
        s_latency = env != NULL && *env != '\0' ? atoi(env) : 1;
    }
    if (!has_user_callback || s_latency <= 0 || callback == NULL || !pc_vi_virtual_clock()) {
        ARQPostRequest(request, owner, type, priority, source, dest, length, callback);
        return;
    }
    if (!s_hooked) {
        s_hooked = 1;
        pc_os_set_deliver_hook(pc_arq_deliver);
        fprintf(stderr, "[arq] ARAM copy completions are handed over %d retrace(s) after the request (replay clock)\n",
                s_latency);
    }
    ARQPostRequest(request, owner, type, priority, source, dest, length, NULL); /* the copy itself, now */
    for (int i = 0; i < ARQ_PENDING; i++) {
        if (s_pending[i].callback == NULL) {
            s_pending[i].callback = callback;
            s_pending[i].request = (uintptr_t)request;
            s_pending[i].due = VIGetRetraceCount() + (u32)s_latency;
            s_pending[i].seq = ++s_seq;
            return;
        }
    }
    fprintf(stderr, "[arq] more than %d copies in flight: one completes at once\n", ARQ_PENDING);
    callback((uintptr_t)request);
}
