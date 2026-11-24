/**
 * @file gs_thread_exact_800F0424.c
 * @brief GSthread public thread API, scheduler and exit trampoline.
 *
 * Address range: 0x800F0424 - 0x800F106C (the tail of the retail GSthread
 * translation unit), in address order:
 *   GSthreadUnblock, GSthreadBlock, GSthreadExecuteAll, GSthreadExecuteGroup,
 *   GSthreadClose, GSthreadIsRunning, GSthreadTerminateGroup,
 *   GSthreadTerminate, GSthreadSetArgs, GSthreadCreate, GSthread,
 *   fn_800F0A74 (dispatcher), fn_800F0F4C (exit trampoline).
 *
 * The TU ends at 0x800F106C: the GSthread strings end at 0x80271062 and the
 * next string (lbl_80271068) starts on a fresh 8-byte boundary, and the
 * .sbss globals past 0x8047AC34 are only used by the GS VM code that follows.
 * The .rodata strings and .sbss globals stay extern.
 *
 * gs_thread.c includes this file so the candidate units built from it keep
 * seeing the same definitions.
 */

#include "dolphin/types.h"
#include "game/gs_thread.h"

extern void GSlogWrite(const void* fmt, ...);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);           /* GSmemFree */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u16 handle);          /* GSmemUnlock */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);          /* GSmemGetPtr */
#endif
extern u16 GSmemAlloc(u32 alignment, u32 size);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
extern void threadSaveGPRRegisters(void);
extern void threadSaveFPRRegisters(void);
extern void threadExecute(void);
extern void fn_800F02F4(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
extern void GSscratchFree(void*);
extern u8 GSscratchIsPtr(void*);
extern void GSscratchStore(void*, void*, s32);
extern void GSscratchWaitForCompletion(void);
extern void DCFlushRange(void*, s32);

extern const char lbl_80271008[]; /* "GSthreadCreate. Warning: 'usesFPU==FALE' OK?\n" */
extern const char lbl_80271038[]; /* "GSthread: Init OK, maximum of %d threads\n" */

extern u8 lbl_804019F0[];  /* scheduler context block */
extern u32 _activeThread;   /* current thread */
extern u32 lbl_8047AC04;   /* next thread */
extern u32 lbl_8047AC08;   /* head of the affinity-sorted thread list */
extern u8 _rescan;    /* reschedule flag */
extern u32 _usesFPURegisters;   /* switch saves/restores FPRs */
extern u32 _destRegStatePtr;   /* context block for the register save/load primitives */
extern u32 _threadRegStatePtr;   /* "run" context */
extern u32 _localRegStatePtr;   /* "next" context */
extern GSThread* _threadList; /* thread array */
extern u16 lbl_8047AC2C;   /* thread array GSmem handle */
extern u32 _maxThreads;   /* maximum thread count */

extern void fn_800F0F4C(u32 result);

/* 0x800F0424 | 0x14 */
void GSthreadUnblock(GSThread* thr) {
    thr->suspended = 0;
    _rescan = 1;
}

/* 0x800F0438 | 0x10 */
void GSthreadBlock(u8* p) {
#ifdef TARGET_PC
    /* PC: the suspended flag (GameCube +0x9) is at another offset in the native GSThread; +0x9 is the second byte of
     * the 8-byte `next` link, so blocking a room's script threads at a room change turned 0x8049EBB0 into 0x804901B0
     * (found with ORRE_WATCH, 2026-10-17; golden-slice M15) */
    ((GSThread*)p)->suspended = 1;
#else
    p[0x9] = 1;
#endif
    _rescan = 1;
}

/* thread wake/sleep dispatch -- declared old-style (no prototype) because
 * the real definition of fn_800F0A74 (further below) is itself an old-style
 * K&R definition, which mwcc treats as an unprototyped "(...)" type; a
 * modern prototype here would conflict with that declaration. */
extern void fn_800F0A74();

#ifdef TARGET_PC
/* PC diagnostic (2026-10-17, temporary; golden-slice M14): before each dispatch, check every slot's prev / next is
 * NULL or a slot of the thread array. The first bad one is reported once with its slot, raw links and entry, so the
 * damage is caught right after the code that wrote it (a GameCube-offset view of a GSThread, likely: B's reading). */
static const char* s_pc_thread_where; /* which thread function ran last before the check (bisect, M15) */
#define PC_THREAD_CHECK(name) (s_pc_thread_where = (name), pc_thread_scan(NULL))
static void pc_thread_scan(const GSThread* ran)
{
    static int reported;
    u32 i;

    if (reported || _threadList == NULL) {
        return;
    }
    for (i = 0; i < _maxThreads; i++) {
        GSThread* t = &_threadList[i];
        GSThread* links[2];
        u32 k;

        links[0] = t->prev;
        links[1] = t->next;
        for (k = 0; k < 2; k++) {
            GSThread* p = links[k];
            if (p != NULL && (p < _threadList || p >= _threadList + _maxThreads ||
                              ((uintptr_t)((u8*)p - (u8*)_threadList) % sizeof(GSThread)) != 0)) {
                extern void OSReport(const char*, ...);
                extern void pc_crash_print_stack(const char* reason);
                reported = 1;
                OSReport("[thread] slot %u has a bad %s link %p (active %u, prio %u, entry %p, handles %u/%u)\n",
                         (unsigned)i, k ? "next" : "prev", (void*)p, t->active, (unsigned)t->priority, t->entryFunc,
                         t->stackHandle, t->ctxHandle);
                if (ran != NULL) {
                    /* checked after every thread's turn: the thread that just ran (or code it called) wrote it */
                    OSReport("[thread] found right after slot %u ran (entry %p, prio %u)\n",
                             (unsigned)(ran - _threadList), ran->entryFunc, (unsigned)ran->priority);
                } else if (s_pc_thread_where != NULL) {
                    OSReport("[thread] found at the end of %s\n", s_pc_thread_where);
                }
                pc_crash_print_stack("[thread] found before this dispatch; the caller:");
                return;
            }
        }
    }
}
#endif

/* 0x800F0448 | 0x28 */
void GSthreadExecuteAll(void) {
#ifdef TARGET_PC
    pc_thread_scan(NULL);
#endif
    fn_800F0A74(0, 0);
}

/* 0x800F0470 | 0x24 */
void GSthreadExecuteGroup(u32 priority) {
    fn_800F0A74(priority, 1);
}

/* 0x800F0494 | 0x28 */
u32 GSthreadClose(GSThread* thr) {
    u32 ret;
    if (thr->autoStart == 1) {
        return 0;
    }
    ret = thr->unused;
    thr->active = 0;
    return ret;
}

/* 0x800F04BC | 0x8 */
#ifdef TARGET_PC
/* PC: GSThread has native pointers, so the running flag (GameCube +0x14, `pad0` in the struct, written by field) is
 * not at +0x14 here: read it by field. With the raw offset every finished script thread looked alive and a room's
 * entry phase never ended (Outskirt Stand stayed black). */
u8 GSthreadIsRunning(u8* p) { return ((GSThread*)p)->pad0; }
#else
u8 GSthreadIsRunning(u8* p) { return p[0x14]; }
#endif

/* 0x800F04C4 | 0xDC */
/*
 * GSthreadTerminateGroup -- destroy every thread whose priority == @p priority.
 *
 * Walks the global thread list (head lbl_8047AC08); for each matching
 * thread it applies the same destroy/unlink logic as GSthreadTerminate
 * (inlined): current/run threads are deferred via the 0x15 pending flag
 * (+ _rescan reschedule for the run thread); all others are
 * unlinked from the list and have their stack/ctx GSmem handles freed.
 */
void GSthreadTerminateGroup(u32 priority) {
    GSThread* thr;
    for (thr = (GSThread*)PC_PTR(lbl_8047AC08); thr != NULL; thr = thr->next) {
        if (thr->priority == priority) {
            if (thr == (GSThread*)PC_PTR(_activeThread) || thr == (GSThread*)PC_PTR(lbl_8047AC04)) {
                thr->pad1 = 1;
                if (thr == (GSThread*)PC_PTR(lbl_8047AC04)) {
                    _rescan = 1;
                }
            } else {
                thr->pad0 = 0;
                thr->active = 0;
                if (thr->prev != NULL) {
                    thr->prev->next = thr->next;
                }
                if (thr->next != NULL) {
                    thr->next->prev = thr->prev;
                }
                if ((GSThread*)PC_PTR(lbl_8047AC08) == thr) {
                    lbl_8047AC08 = PC_PTR32(thr->next);
                }
                GSmemFree(thr->stackHandle);
                GSmemFree(thr->ctxHandle);
            }
        }
    }
#ifdef TARGET_PC
    PC_THREAD_CHECK("GSthreadTerminateGroup");
#endif
}

/* 0x800F05A0 | 0xB4 */
/*
 * GSthreadTerminate -- destroy / unregister a cooperative thread (GSThread).
 *
 * If @p thr is the current thread (_activeThread) or the run thread
 * (lbl_8047AC04) it cannot be torn down in place, so a destroy-pending
 * flag at offset 0x15 is raised instead; when it is the run thread the
 * global reschedule flag (_rescan) is set so the dispatcher re-enters.
 *
 * Otherwise the thread is unlinked from the doubly-linked thread list,
 * the list head (lbl_8047AC08) is fixed up if it pointed at this thread,
 * and both the stack and context GSmem handles are released via
 * GSmemFree (GSmemFree).
 *
 * Parameter kept as u32 (not GSThread*) to match the other GSthreadTerminate
 * extern declarations/call sites (fn_800F716C, fn_800F7274), which pass a
 * raw GSThread* value stored in a u32 slot.
 */
void GSthreadTerminate(u32 ctxArg) {
    GSThread* thr = (GSThread*)PC_PTR(ctxArg);
    if (thr == (GSThread*)PC_PTR(_activeThread) || thr == (GSThread*)PC_PTR(lbl_8047AC04)) {
        thr->pad1 = 1;
        if (thr == (GSThread*)PC_PTR(lbl_8047AC04)) {
            _rescan = 1;
        }
    } else {
        thr->pad0 = 0;
        thr->active = 0;
        if (thr->prev != NULL) {
            thr->prev->next = thr->next;
        }
        if (thr->next != NULL) {
            thr->next->prev = thr->prev;
        }
        if ((GSThread*)PC_PTR(lbl_8047AC08) == thr) {
            lbl_8047AC08 = PC_PTR32(thr->next);
        }
        GSmemFree(thr->stackHandle);
        GSmemFree(thr->ctxHandle);
    }
#ifdef TARGET_PC
    PC_THREAD_CHECK("GSthreadTerminate");
#endif
}

typedef struct GSthreadVaInfo {
    u8 gpr;
    u8 fpr;
    u16 padding;
    u32* overflow_arg_area;
    u32* reg_save_area;
} GSthreadVaInfo;
typedef GSthreadVaInfo GSthreadVaList[1];

#ifdef TARGET_PC
/* the host's variadic list (port-gaps 16); arguments are 32-bit words in OrrePort's low-4 GB layout */
#define GS_THREAD_VA_START(ap, last) __builtin_va_start(ap, last)
#define GS_THREAD_VA_U32(ap) ((u32)__builtin_va_arg(ap, u32))
#else
#define GS_THREAD_VA_START(ap, last) \
    ((void)(last), __builtin_va_info(&(ap)))
#define GS_THREAD_VA_U32(ap) (*__va_arg(ap, 1))
#endif

extern u32* __va_arg(void* ap, u32 type);

/* 0x800F0654 | 0x154 */
void GSthreadSetArgs(void* threadPtr, s32 count, ...)
{
    GSThread* thread = threadPtr;
#ifdef TARGET_PC
    __builtin_va_list args;
#else
    GSthreadVaList args;
#endif
    u32* stack;
    GSThreadCtx* context;
    s32 i;
    s32 directCount;
    u32 reg;
    u32 stackIndex;

    if (thread->pad0 == 1) {
        return;
    }

    stack = GSmemLock(thread->stackHandle);
    context = GSmemLock(thread->ctxHandle);
    GS_THREAD_VA_START(args, count);

    /* The first eight arguments go in the saved r3-r10 slots... */
    reg = 3;
    directCount = 8;
    if (count < 8) {
        directCount = count;
    }
    for (i = 0; i < directCount; i++) {
        context->gpr[reg++] = GS_THREAD_VA_U32(args);
    }

    /* ...the rest are spilled to the parameter area of the new thread's
     * initial stack frame (8 bytes above its saved stack pointer). */
    if (count > 8) {
        context->gpr[1] -= (count - i) * sizeof(u32);
        stackIndex = (context->gpr[1] >> 2) + 2;
        for (i = 8; i < count; i++) {
            stack[stackIndex++] = GS_THREAD_VA_U32(args);
        }
    }

#ifdef TARGET_PC
    __builtin_va_end(args);
#endif
    fn_800E24B0(thread->stackHandle);
    fn_800E24B0(thread->ctxHandle);
}

#undef GS_THREAD_VA_U32
#undef GS_THREAD_VA_START

/* =======================================================================
 *  GSthreadCreate
 *  Address: 0x800F07A8, Size: 0x230
 *
 *  Creates a cooperative thread: claims a free slot, allocates its stack
 *  (GSmemAlloc, 32-byte aligned) and register context block
 *  (_toolentryAlloc__FUl; 0x88 bytes, or 0x188 with FPU state), seeds the
 *  context so the thread starts in fn_800F0F4C, and links it into the
 *  affinity-sorted thread list.
 * ======================================================================= */
/*
 * Return the first unused slot in the thread array, or NULL when every slot
 * is taken. Inlined into GSthreadCreate: the target materialises the NULL
 * result (li r31, 0) on loop exhaustion and then re-tests it in the caller's
 * own NULL check, and the found path branches straight to that re-test.
 * Same search idiom as floorFindDataEntry in gs_floor.c.
 */
static inline GSThread* threadFindFreeSlot(void) {
    GSThread* thread;
    u32 count;

    thread = _threadList;
    for (count = _maxThreads; count != 0; count--) {
        if (thread->active == 0) {
            return thread;
        }
        thread++;
    }
    return NULL;
}

GSThread* GSthreadCreate(u32 affinity, u32 priority, u32 stackSize,
                         u8 usesFPU, u32 autoStart, void* entryFunc) {
    GSThread* thread;
    GSThread* curr;
    GSThread* next;
    GSThreadCtx* ctx;
    s32* stack;

    if (usesFPU == 0) {
        GSlogWrite(lbl_80271008);
    }

    thread = threadFindFreeSlot();
    if (thread == NULL) {
        return NULL;
    }

    thread->stackHandle = GSmemAlloc(stackSize, 0x20);
    if (thread->stackHandle == 0) {
        return NULL;
    }

    thread->ctxHandle = _toolentryAlloc__FUl(usesFPU ? 0x188 : 0x88);
    if (thread->ctxHandle == 0) {
        GSmemFree(thread->stackHandle);
        return NULL;
    }

    thread->active = 1;
    thread->priority = priority;
    thread->stackSize = stackSize;
    thread->pad0 = 0;
    thread->pad1 = 0;
    thread->usesFPU = usesFPU;
    thread->entryFunc = entryFunc;
    thread->suspended = 0;
    thread->sleeping = 0;
    thread->affinity = affinity;
    thread->prev = NULL;
    thread->next = NULL;
    thread->autoStart = autoStart;

    /* Build the initial register context: the new thread starts in the
     * fn_800F0F4C trampoline with entryFunc in the saved LR slot and its
     * stack pointer 8 bytes below the top of its stack. */
    ctx = GSmemLock(thread->ctxHandle);
    stack = GSmemLock(thread->stackHandle);

    _destRegStatePtr = PC_PTR32(ctx);
    threadSaveGPRRegisters();
    if (thread->usesFPU) {
        threadSaveFPRRegisters();
    }

    ctx->gpr[1] = thread->stackSize - 8;
    ctx->lr = PC_PTR32(thread->entryFunc);
    ctx->ctr = PC_PTR32(fn_800F0F4C);
    *stack = -1; /* stack-overflow sentinel */

    fn_800E24B0(thread->ctxHandle);
    fn_800E24B0(thread->stackHandle);

    /* Insert into the thread list, kept sorted by ascending affinity. */
    curr = (GSThread*)PC_PTR(lbl_8047AC08);
    if (curr == NULL) {
        lbl_8047AC08 = PC_PTR32(thread);
    } else {
        while ((next = curr->next) != NULL && curr->affinity < thread->affinity) {
            curr = next;
        }

        if (next == NULL && curr->affinity < thread->affinity) {
            thread->prev = curr;
            thread->next = NULL;
            curr->next = thread;
        } else {
            if (curr->prev != NULL) {
                curr->prev->next = thread;
            }
            thread->prev = curr->prev;
            thread->next = curr;
            curr->prev = thread;
            if ((GSThread*)PC_PTR(lbl_8047AC08) == curr) {
                lbl_8047AC08 = PC_PTR32(thread);
            }
        }
    }

    _rescan = 1;
#ifdef TARGET_PC
    PC_THREAD_CHECK("GSthreadCreate");
#endif
    return thread;
}

/* =======================================================================
 *  GSthread
 *  Address: 0x800F09D8, Size: 0x9C
 *
 *  Allocates the cooperative thread pool from GSmem (maxThreads * 0x24
 *  bytes), clears each slot's active flag, resets the current thread and
 *  the thread list head and logs
 *  "GSthread: Init OK, maximum of %d threads\n".
 * ======================================================================= */
s32 GSthread(u32 maxThreads) {
    u32 handle;
    u32 i;

    _maxThreads = maxThreads;
#ifdef TARGET_PC
    /* 0x24 is the GameCube sizeof(GSThread); the PC record is bigger (8-byte links and entry pointer), so a pool of
     * maxThreads * 0x24 bytes let records 14-20 overlap the next GSmem block (GSgfx's draw-request buffer;
     * 2026-10-17) */
    handle = _toolentryAlloc__FUl(maxThreads * sizeof(GSThread));
#else
    handle = _toolentryAlloc__FUl(maxThreads * 0x24);
#endif
    lbl_8047AC2C = handle;
    if (handle == 0) {
        return 0;
    }
    _threadList = GSmemLock(handle);

    for (i = 0; i < _maxThreads; i++) {
        _threadList[i].active = 0;
    }

    _activeThread = 0;
    lbl_8047AC08 = 0;
    GSlogWrite(lbl_80271038, _maxThreads);
    return 1;
}

/* 0x800F0A74 | 0x4D8 */
/*
 * Return the first runnable thread after @p from (or from the list head when
 * @p from is NULL) -- active, not suspended, not sleeping, not pending
 * destruction and, when @p groupOnly is set, of priority @p priority.
 * Inlined four times into fn_800F0A74: every expansion materialises the NULL
 * result (li rN, 0) on list exhaustion and re-tests it in the caller, and the
 * found path branches straight to that re-test (same idiom as
 * threadFindFreeSlot above).
 */
static inline GSThread* threadFindRunnable(GSThread* from, u32 priority, u8 groupOnly) {
    GSThread* thread;

    if (from == NULL) {
        thread = (GSThread*)PC_PTR(lbl_8047AC08);
    } else {
        thread = from->next;
    }
#ifdef TARGET_PC
    {
        /* PC diagnostic (2026-10-17, temporary): the diner crash (the automated run, reading 0x110 here) walks
         * into a thread whose `next` is not a thread. Report the last good thread once, then fall through to the crash
         * so the stack is kept. */
        GSThread* last = from;
        static int reported;
        for (; thread != NULL; last = thread, thread = thread->next) {
            /* above 4 GB too: everything the game sees is below it, and Windows reports a non-canonical address as
             * 0xFFFFFFFFFFFFFFFF (A, 2026-10-17: the fight-end crash passed the first version of this check) */
            if (((uintptr_t)thread < 0x10000 || ((uintptr_t)thread & 3) != 0 || (uintptr_t)thread >= 0x100000000ull)
                && !reported) {
                extern void OSReport(const char*, ...);
                extern void pc_crash_print_stack(const char* reason);
                reported = 1;
                OSReport("[thread] bad link %p after thread %p (entry %p, active %u, prio %u, stack %u, handles %u/%u)\n",
                         (void*)thread, (void*)last, last ? last->entryFunc : NULL, last ? last->active : 0,
                         last ? (unsigned)last->priority : 0u, last ? (unsigned)last->stackSize : 0u,
                         last ? last->stackHandle : 0, last ? last->ctxHandle : 0);
                pc_crash_print_stack("[thread] the scheduler's caller:");
            }
            if (thread->active != 0 && thread->suspended == 0 && thread->sleeping == 0 && thread->pad1 == 0) {
                if (groupOnly == 0 || thread->priority == priority) {
                    return thread;
                }
            }
        }
        return NULL;
    }
#endif
    while (thread != NULL) {
        if (thread->active != 0 && thread->suspended == 0 && thread->sleeping == 0 && thread->pad1 == 0) {
            if (groupOnly == 0 || thread->priority == priority) {
                return thread;
            }
        }
        thread = thread->next;
    }
    return NULL;
}

/*
 * Thread dispatcher behind GSthreadExecuteAll/GSthreadExecuteGroup: run every
 * runnable thread (only those of priority @p arg0 when @p arg1 is set) once.
 * The next thread is looked up and its stack fetched before the current one
 * runs; if a thread raised the reschedule flag (_rescan) and that
 * prefetched choice is now stale, it is released (destroying it if it was
 * marked for destruction meanwhile) and the search is repeated. nextStack is
 * only assigned when a next thread exists -- the target has no initialiser.
 */
void fn_800F0A74(arg0, arg1)
    u32 arg0;
    u8 arg1;
{
    GSThread *next;
    GSThread *dead;
    GSThreadCtx *ctx;
    u8 *stack;
    u8 *scratchStack;
#ifdef TARGET_PC
    /* PC: the stale-prefetch branch below tests nextStack even when no next thread was ever found (the console reads
     * whatever the register held, never a locked-cache stack there: GSscratchIsPtr is false). On the PC that garbage
     * failed the 32-bit pointer check (P01 replays with ORRE_FRAME_TIMES, game frame 300, 2026-10-24); NULL keeps
     * GSscratchIsPtr false */
    u8 *nextStack = NULL;
#else
    u8 *nextStack;
#endif
    GSThread *thread;

    thread = threadFindRunnable(NULL, arg0, arg1);
    if (thread == NULL) {
        return;
    }

    ctx = (GSThreadCtx *)GSmemLock(thread->ctxHandle);
    stack = (u8 *)GSmemLock(thread->stackHandle);
    scratchStack = stack;

    while (thread != NULL) {
        next = threadFindRunnable(thread, arg0, arg1);
        lbl_8047AC04 = PC_PTR32(next);
        if (next != NULL) {
            nextStack = (u8 *)GSmemLock(next->stackHandle);
        }

        _activeThread = PC_PTR32(thread);
        thread->pad0 = 1;
        ctx->gpr[1] = PC_PTR32((scratchStack + ctx->gpr[1]));
        ((GSThreadCtx *)lbl_804019F0)->ctr = PC_PTR32(fn_800F02F4);
        _localRegStatePtr = PC_PTR32(&lbl_804019F0);
        _threadRegStatePtr = PC_PTR32(ctx);
        _usesFPURegisters = thread->usesFPU;
        threadExecute();
#ifdef TARGET_PC
        pc_thread_scan(thread);
#endif
        ctx->gpr[1] -= PC_PTR32(scratchStack);
        _activeThread = 0;

        if (thread->pad1 != 0) {
            if (GSscratchIsPtr(scratchStack) != 0) {
                GSscratchFree(scratchStack);
            }
            fn_800E24B0(thread->ctxHandle);
            fn_800E24B0(thread->stackHandle);

            if (PC_PTR32(thread) == _activeThread || PC_PTR32(thread) == lbl_8047AC04) {
                thread->pad1 = 1;
                if (PC_PTR32(thread) == lbl_8047AC04) {
                    _rescan = 1;
                }
            } else {
                thread->pad0 = 0;
                thread->active = 0;
                if (thread->prev != NULL) {
                    thread->prev->next = thread->next;
                }
                if (thread->next != NULL) {
                    thread->next->prev = thread->prev;
                }
                if ((GSThread *)PC_PTR(lbl_8047AC08) == thread) {
                    lbl_8047AC08 = PC_PTR32(thread->next);
                }
                GSmemFree(thread->stackHandle);
                GSmemFree(thread->ctxHandle);
            }
        } else {
            if (GSscratchIsPtr(scratchStack) != 0) {
                DCFlushRange(stack, thread->stackSize);
                GSscratchStore(stack, scratchStack, thread->stackSize);
                GSscratchWaitForCompletion();
                GSscratchFree(scratchStack);
            }
            fn_800E24B0(thread->ctxHandle);
            fn_800E24B0(thread->stackHandle);
        }

        if (_rescan == 0 || (GSThread *)PC_PTR(lbl_8047AC04) == threadFindRunnable(thread, arg0, arg1)) {
            next = (GSThread *)PC_PTR(lbl_8047AC04);
            thread = next;
            if (next != NULL) {
                scratchStack = nextStack;
                stack = nextStack;
                ctx = (GSThreadCtx *)GSmemLock(next->ctxHandle);
            }
        } else {
            if (GSscratchIsPtr(nextStack) != 0) {
                GSscratchWaitForCompletion();
                GSscratchFree(nextStack);
            }
            if ((GSThread *)PC_PTR(lbl_8047AC04) != NULL) {
                fn_800E24B0(((GSThread *)PC_PTR(lbl_8047AC04))->stackHandle);
                dead = (GSThread *)PC_PTR(lbl_8047AC04);
                if (dead->pad1 != 0) {
                    lbl_8047AC04 = 0;
                    if (PC_PTR32(dead) == _activeThread || PC_PTR32(dead) == lbl_8047AC04) {
                        dead->pad1 = 1;
                        if (PC_PTR32(dead) == lbl_8047AC04) {
                            _rescan = 1;
                        }
                    } else {
                        dead->pad0 = 0;
                        dead->active = 0;
                        if (dead->prev != NULL) {
                            dead->prev->next = dead->next;
                        }
                        if (dead->next != NULL) {
                            dead->next->prev = dead->prev;
                        }
                        if ((GSThread *)PC_PTR(lbl_8047AC08) == dead) {
                            lbl_8047AC08 = PC_PTR32(dead->next);
                        }
                        GSmemFree(dead->stackHandle);
                        GSmemFree(dead->ctxHandle);
                    }
                }
            }
            thread = threadFindRunnable(thread, arg0, arg1);
            lbl_8047AC04 = PC_PTR32(thread);
            if (thread != NULL) {
                ctx = (GSThreadCtx *)GSmemLock(thread->ctxHandle);
                stack = (u8 *)GSmemLock(thread->stackHandle);
                scratchStack = stack;
            }
        }

        _rescan = 0;
    }
}

/* 0x800F0F4C | 0x120 */
/*
 * Thread exit trampoline: GSthreadCreate seeds ctx->ctr with this function,
 * so a thread's entry function returns here with its result in r3. The
 * result is kept in the thread slot for GSthreadClose. An autoStart thread
 * is then destroyed exactly as GSthreadTerminate would do it; any other
 * thread is only marked finished and unlinked, leaving its slot and GSmem
 * handles for GSthreadClose. Either way control switches away for good.
 */
void fn_800F0F4C(u32 result) {
    GSThread* thread = (GSThread*)PC_PTR(_activeThread);

    thread->unused = result;
    if (thread->autoStart == 1) {
        if (thread == (GSThread*)PC_PTR(_activeThread) || thread == (GSThread*)PC_PTR(lbl_8047AC04)) {
            thread->pad1 = 1;
            if (thread == (GSThread*)PC_PTR(lbl_8047AC04)) {
                _rescan = 1;
            }
        } else {
            thread->pad0 = 0;
            thread->active = 0;
            if (thread->prev != NULL) {
                thread->prev->next = thread->next;
            }
            if (thread->next != NULL) {
                thread->next->prev = thread->prev;
            }
            if ((GSThread*)PC_PTR(lbl_8047AC08) == thread) {
                lbl_8047AC08 = PC_PTR32(thread->next);
            }
            GSmemFree(thread->stackHandle);
            GSmemFree(thread->ctxHandle);
        }
    } else {
        thread->pad1 = 1;
        thread->pad0 = 0;
        if (thread->prev != NULL) {
            thread->prev->next = thread->next;
        }
        if (thread->next != NULL) {
            thread->next->prev = thread->prev;
        }
        if ((GSThread*)PC_PTR(lbl_8047AC08) == thread) {
            lbl_8047AC08 = PC_PTR32(thread->next);
        }
        _rescan = 1;
    }
    _threadSwitch();
}
