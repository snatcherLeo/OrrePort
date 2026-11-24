/**
 * @file gs_range_8017FA5C_exact_80180320.c
 * @brief ARQ transfer queue: DMA wrappers, busy poll and completion callback
 *        (0x80180320 - 0x8018094C).
 *
 * Part of the 0x8017FA5C - 0x801812C4 retail unit, carved so its exact
 * functions can link; built like the rest of it at `-opt level=0` with
 * `-inline deferred` (deferred inlining emits a unit's functions in reverse
 * source order, so they are written from the highest address down) and no
 * local pragmas.
 *
 * Inlining evidence:
 * - arqRead / arqWrite are the request bodies: their result travels through
 *   a return temporary (r27, "mr r27,r31 / li r27,0") that the wrapper then
 *   copies to its own local, which only an inline expansion produces;
 *   arqAlloc, expanded in both, leaves the same pattern in r26.
 * - The synchronous wrappers are the asynchronous ones plus a busy wait.
 *   fn_80180450 expands fn_801807A8 and fn_80180320 expands fn_80180584
 *   with NULL callbacks (`-inline auto` inlines these same-unit calls), and
 *   both expand fn_801808B4 in the wait loop ("mr r29,rN; lwz r0,0x24(rN)",
 *   the typed view of its handle). fn_801808B4 therefore has to live in the
 *   same object as its callers.
 *
 * GS_ASSERT: every wrapper copies `aram` with a record-form "mr. rN,r4"
 * whose CR0 result nothing reads; the size test that decides the branch is
 * a separate cmplwi. That dead compare is what a conditional with two empty
 * arms leaves at level 0: a release-build assert whose failure handler is
 * compiled out but whose test survives. Without it `aram` also loses the
 * extra reference that ranks its register above `main`'s.
 */
#include "game/gs_range_8017FA5C_shared.h"

extern BOOL OSDisableInterrupts(void);
extern BOOL OSRestoreInterrupts(BOOL level);
#ifdef TARGET_PC
/* PC: Aurora's prototype (dolphin/arq.h): source and dest are uintptr_t and go on the stack (args 5-6); declared
 * u32 the upper halves were garbage, a crash on re-entering a room (B, 2026-10-17). The u32 values passed convert
 * (zero-extended) through this prototype. */
extern void ARQPostRequest(void* request, u32 owner, u32 type, u32 priority,
                           uintptr_t source, uintptr_t dest, u32 length,
                           void (*callback)(uintptr_t request));
#else
extern void ARQPostRequest(void* request, u32 owner, u32 type, u32 priority,
                           u32 source, u32 dest, u32 length,
                           void (*callback)(u32 request));
#endif
#ifdef TARGET_PC
#define ARQ_CB(f) ((void (*)(uintptr_t))(f))
/* port/src/pc_arq.c (2026-10-21): the copy at once, the completion at the console's time when the request has a
 * caller's callback (with the replay clock); the synchronous copies (no callback, they spin) stay immediate. Port
 * timing, not a feature: Aurora calls the callback inside the request, a frame before the console's interrupt */
extern void pc_arq_post(void* request, u32 owner, u32 type, u32 priority, uintptr_t source, uintptr_t dest,
                        u32 length, void (*callback)(uintptr_t request), int has_user_callback);
#define ARQ_POST(req, owner, type, prio, src, dst, len, cb, user) \
    pc_arq_post(req, owner, type, prio, src, dst, len, cb, (user) != NULL)
#else
#define ARQ_POST(req, owner, type, prio, src, dst, len, cb, user) ARQPostRequest(req, owner, type, prio, src, dst, len, cb)
#define ARQ_CB(f) ((void (*)(u32))(f))
#endif

#define GS_ASSERT(cond) ((cond) ? (void)0 : (void)0)

/* Address: 0x801808E4 | size: 0x68
 * ARQ completion callback: mark the entry idle, run the user callback,
 * release the entry and flush the transferred range. */
void fn_801808E4(GsRangeARQEntry* entry)
{
    GsRangeARQEntry* e = entry;

    e->mode = 0;
    if (e->callback) {
        e->callback((void*)PC_PTR(e->flush), e->callbackArg);
    }
    e->state = 0;
    DCFlushRange(e->src, e->size);
}

/* Address: 0x801808B4 | size: 0x30
 * Nonzero while the request is still in flight. */
s32 fn_801808B4(void* handle)
{
    GsRangeARQEntry* entry = handle;

    if (entry->mode != 1) {
        entry->state = 0;
    }
    return entry->state;
}

static inline GsRangeARQEntry* arqAlloc(void)
{
    GsRangeARQEntry* entry;
    u32 i;

    entry = lbl_8047B1D4;
    for (i = 0; i < lbl_8047B1D8; i++) {
        if (entry->state == 0) {
            entry->state = 1;
            return entry;
        }
        entry++;
    }
    return NULL;
}

/* Main memory -> ARAM. */
static inline GsRangeARQEntry* arqWrite(void* main, void* aram, u32 size,
                                        void (*callback)(void*, void*), void* arg)
{
    GsRangeARQEntry* entry;
    BOOL level;
    u32 alignedSize;

    GS_ASSERT(aram);
    if (size != 0) {
        entry = arqAlloc();
        level = OSDisableInterrupts();
        entry->flush = 0;
        entry->mode = 1;
        entry->callback = callback;
        entry->callbackArg = arg;
        alignedSize = (size + 0x1F) & ~0x1F;
        entry->src = main;
        entry->dst = aram;
        entry->size = alignedSize;
        DCFlushRange(main, size);
        ARQ_POST(entry, PC_PTR32(entry), 0, 0, PC_PTR32(main), PC_PTR32(aram),
                 (size + 0x1F) & ~0x1F, ARQ_CB(fn_801808E4), callback);
        OSRestoreInterrupts(level);
        return entry;
    }
    return NULL;
}

/* ARAM -> main memory. */
static inline GsRangeARQEntry* arqRead(void* main, void* aram, u32 size,
                                       void (*callback)(void*, void*), void* arg)
{
    GsRangeARQEntry* entry;
    BOOL level;
    u32 alignedSize;

    GS_ASSERT(aram);
    if (size != 0) {
        entry = arqAlloc();
        level = OSDisableInterrupts();
        entry->flush = 1;
        entry->mode = 1;
        entry->callback = callback;
        entry->callbackArg = arg;
        alignedSize = (size + 0x1F) & ~0x1F;
        entry->src = main;
        entry->dst = aram;
        entry->size = alignedSize;
        DCFlushRange(main, size);
        ARQ_POST(entry, PC_PTR32(entry), 1, 0, PC_PTR32(aram), PC_PTR32(main), alignedSize,
                 ARQ_CB(fn_801808E4), callback);
        OSRestoreInterrupts(level);
        return entry;
    }
    return NULL;
}

/* Address: 0x801807A8 | size: 0x10C */
GsRangeARQEntry* fn_801807A8(void* main, void* aram, u32 size)
{
    GsRangeARQEntry* entry = arqWrite(main, aram, size, NULL, NULL);
    return entry;
}

/* Address: 0x80180694 | size: 0x114 */
GsRangeARQEntry* fn_80180694(void* main, void* aram, u32 size,
                             void (*callback)(void*, void*), void* arg)
{
    GsRangeARQEntry* entry = arqWrite(main, aram, size, callback, arg);
    return entry;
}

/* Address: 0x80180584 | size: 0x110 */
GsRangeARQEntry* fn_80180584(void* main, void* aram, u32 size,
                             void (*callback)(void*, void*), void* arg)
{
    GsRangeARQEntry* entry = arqRead(main, aram, size, callback, arg);
    return entry;
}

/* Address: 0x80180450 | size: 0x134
 * Synchronous main memory -> ARAM copy. */
void fn_80180450(void* main, void* aram, u32 size)
{
    GsRangeARQEntry* entry = fn_801807A8(main, aram, size);

    while (fn_801808B4(entry)) {
    }
}

/* Address: 0x80180320 | size: 0x130
 * Synchronous ARAM -> main memory copy. */
void fn_80180320(void* main, void* aram, u32 size)
{
    GsRangeARQEntry* entry = fn_80180584(main, aram, size, NULL, NULL);

    while (fn_801808B4(entry)) {
    }
}
