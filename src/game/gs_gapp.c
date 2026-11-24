/**
 * @file gs_gapp.c
 * @brief GSgapp -- GSAPI "gapp" cooperative task wrapper (block/unblock,
 *        terminate, update, create/init, background and VSync dispatch).
 *
 * Address range: 0x800FE6DC - 0x800FEC34.
 * XD class: game/pxdvs/GSAPI/GSgapp/GSgapp.cpp
 *
 * Tasks live in one array of GSgappTask records (the first numTasks
 * records serve the foreground/VSync states, the remaining numQueues serve
 * the background state) and are threaded onto a doubly-linked list kept
 * sorted by ascending priority. Background tasks created while the list is
 * live are parked on a pending list and merged in by the idle callback.
 *
 * Split out of the former monolithic game/gs_thread_hi.c. The whole range
 * is one unit built at the default GC/1.3 -O4,p flags: compiled that way,
 * this file reproduces all eight functions of 0x800FE6DC-0x800FEC34 byte for
 * byte, with every relocation pairing, and emits no data or helper symbols.
 */
#include "dolphin/types.h"

typedef void (*GSgappCallback)(u32 taskId, void* param);

/* Task states. */
#define GAPP_STATE_FREE       0
#define GAPP_STATE_UPDATE     1 /* run by GSgappUpdate */
#define GAPP_STATE_BACKGROUND 2 /* run by gappBackgroundCallback */
#define GAPP_STATE_VSYNC      3 /* run by gappVSyncCallback */

typedef struct GSgappTask {
    /* 0x00 */ struct GSgappTask* prev;
    /* 0x04 */ struct GSgappTask* next;
    /* 0x08 */ s32 state;
    /* 0x0C */ u8 priority; /* list is sorted by ascending priority */
    /* 0x0D */ u8 blocked;
    /* 0x0E */ u8 _pad[2];
    /* 0x10 */ void* param;
    /* 0x14 */ GSgappCallback callback;
} GSgappTask; /* size 0x18 */

/* ===== External SDK / engine functions ===== */
extern BOOL OSDisableInterrupts(void);
extern BOOL OSRestoreInterrupts(BOOL level);
extern void* OSSetIdleFunction(void (*idleFunction)(void*), void* param,
                               void* stack, u32 stackSize);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _toolentryAlloc__FUl(u32 size); /* GSmemAllocRaw; u16 handle in r3 */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);      /* GSmemGetPtr */
#endif
extern void fn_800D30A0(void (*callback)(u32)); /* GSgfx retrace callback: called with the retrace count */

/* ===== sbss state ===== */
extern u16 lbl_8047AC78;         /* task array memory handle */
extern GSgappTask* _gappStructList; /* task array */
extern u32 lbl_8047AC80;         /* number of foreground task slots */
extern u32 lbl_8047AC84;         /* number of background task slots */
extern u32 lbl_8047AC88;         /* total task slots */
extern u16 lbl_8047AC8C;         /* idle stack memory handle */
extern u8* lbl_8047AC90;         /* idle stack */
extern GSgappTask* lbl_8047AC94; /* task currently being dispatched */
extern GSgappTask* _gappListTop; /* head of the priority-sorted task list */
extern GSgappTask* lbl_8047AC9C; /* background tasks awaiting insertion */

void gappBackgroundCallback(void);
void gappVSyncCallback(u32 retraceCount);

/* Links a task into the priority-sorted list ahead of the first task whose
 * priority is not lower, or at the tail. The target expands this identical
 * sequence twice: in GSgappCreate (0x800FE8F8) and in the pending-list merge
 * of gappBackgroundCallback (0x800FEB00). */
static inline void gappInsertTask(GSgappTask* task) {
    GSgappTask* curr;

    curr = _gappListTop;
    while (curr->next != NULL && curr->priority < task->priority) {
        curr = curr->next;
    }
    if (curr->next == NULL && curr->priority < task->priority) {
        task->prev = curr;
        task->next = NULL;
        curr->next = task;
    } else {
        if (curr->prev != NULL) {
            curr->prev->next = task;
        }
        task->prev = curr->prev;
        task->next = curr;
        curr->prev = task;
        if (_gappListTop == curr) {
            _gappListTop = task;
        }
    }
}

/* Returns the first free record in the slot range serving the given state,
 * or NULL when that range is full. Inline fingerprint in GSgappCreate: the
 * found path branches (0x800FE884) to the caller's NULL test on a pointer
 * already known non-NULL, and the exhausted path materialises NULL
 * (0x800FE890) only for that same test -- the helper's two returns merged
 * into the caller's check. */
static inline GSgappTask* gappFindFreeTask(s32 state) {
    GSgappTask* task;
    u32 count;

    if (state == GAPP_STATE_BACKGROUND) {
        task = &_gappStructList[lbl_8047AC80];
        count = lbl_8047AC84;
    } else {
        task = _gappStructList;
        count = lbl_8047AC80;
    }
    for (; count != 0; count--, task++) {
        if (task->state == GAPP_STATE_FREE) {
            return task;
        }
    }
    return NULL;
}

/* 0x800FE6DC | 0x1C */
void GSgappUnblock(u32 taskId) {
    GSgappTask* task = &_gappStructList[taskId - 1];
    task->blocked = FALSE;
}

/* 0x800FE6F8 | 0x1C */
void GSgappBlock(u32 taskId) {
    GSgappTask* task = &_gappStructList[taskId - 1];
    task->blocked = TRUE;
}

/* 0x800FE714 | 0x8C */
void GSgappTerminate(u32 taskId) {
    GSgappTask* task;
    BOOL level;

    task = &_gappStructList[taskId - 1];
    level = OSDisableInterrupts();
    if (task->prev != NULL) {
        task->prev->next = task->next;
    }
    if (task->next != NULL) {
        task->next->prev = task->prev;
    }
    if (_gappListTop == task) {
        _gappListTop = task->next;
    }
    task->prev = NULL;
    task->next = NULL;
    OSRestoreInterrupts(level);
    task->state = GAPP_STATE_FREE;
}

/* 0x800FE7A0 | 0x94 */
void GSgappUpdate(void) {
    GSgappTask* task;
    GSgappTask* next;

    task = _gappListTop;
    while (task != NULL) {
        next = task->next;
        if (task->state == GAPP_STATE_UPDATE && task->blocked == FALSE) {
            lbl_8047AC94 = task;
            task->callback((PC_PTR32(task) - PC_PTR32(_gappStructList)) / sizeof(GSgappTask) + 1,
                           task->param);
        }
        task = next;
    }
    lbl_8047AC94 = NULL;
}

#ifdef TARGET_PC
/* PC (2026-10-26, for ORRE_RNG_TRACE): the task GSgappUpdate is dispatching right now, as index << 8 | priority
 * (index as the callback receives it), 0 outside a dispatch. P12's battle 207: the battle thread's AI drew before the
 * frame's blink task on the PC and after it on the console */
u32 pc_gapp_current_task(void) {
    GSgappTask* task = lbl_8047AC94;
    if (task == NULL) {
        return 0;
    }
    return (((PC_PTR32(task) - PC_PTR32(_gappStructList)) / sizeof(GSgappTask) + 1) << 8) | task->priority;
}

/* PC (the project's build helper, 2026-11-21, the quicker way back from the PC screen; src/game/pc_game_ui.c fsys_hurry): run
 * one update task by its id once, exactly as GSgappUpdate would dispatch it (an update-state task that isn't blocked),
 * outside the frame's own pass. 1 when it ran. Only called after GSgappUpdate has finished its pass (OrreHook_
 * AfterGappUpdate), so no list walk is in progress when the task terminates itself. All platforms */
s32 pc_gapp_run_task(u32 taskId) {
    GSgappTask* task;
    if (taskId == 0 || taskId > lbl_8047AC88) {
        return 0;
    }
    task = &_gappStructList[taskId - 1];
    if (task->state != GAPP_STATE_UPDATE || task->blocked != FALSE || task->callback == NULL) {
        return 0;
    }
    lbl_8047AC94 = task;
    task->callback(taskId, task->param);
    lbl_8047AC94 = NULL;
    return 1;
}
#endif

/* 0x800FE834 | 0x17C */
u32 GSgappCreate(s32 state, u8 priority, void* param, GSgappCallback callback) {
    GSgappTask* task;
    BOOL level;

    task = gappFindFreeTask(state);
    if (task == NULL) {
        return 0;
    }

    task->prev = NULL;
    task->next = NULL;
    task->state = state;
    task->priority = priority;
    task->blocked = FALSE;
    task->param = param;
    task->callback = callback;

    if (_gappListTop == NULL) {
        _gappListTop = task;
    } else {
        level = OSDisableInterrupts();
        if (task->state == GAPP_STATE_BACKGROUND) {
            task->next = lbl_8047AC9C;
            lbl_8047AC9C = task;
        } else {
            gappInsertTask(task);
        }
        OSRestoreInterrupts(level);
    }
    return (PC_PTR32(task) - PC_PTR32(_gappStructList)) / sizeof(GSgappTask) + 1;
}

/* 0x800FE9B0 | 0xC4 */
void GSgappInit(u32 numTasks, u32 numQueues) {
    u32 total;
    u32 i;
    u16 handle;

    total = numTasks + numQueues;
    lbl_8047AC80 = numTasks;
    lbl_8047AC84 = numQueues;
    lbl_8047AC88 = total;
    lbl_8047AC94 = NULL;
    handle = _toolentryAlloc__FUl(total * sizeof(GSgappTask));
    lbl_8047AC78 = handle;
    if (handle == 0) {
        return;
    }
    _gappStructList = GSmemLock(handle);
    for (i = 0; i < lbl_8047AC88; i++) {
        _gappStructList[i].state = GAPP_STATE_FREE;
    }
    handle = _toolentryAlloc__FUl(0x2000);
    lbl_8047AC8C = handle;
    lbl_8047AC90 = GSmemLock(handle);
    OSSetIdleFunction((void (*)(void*))gappBackgroundCallback, NULL,
                      lbl_8047AC90 + 0x1FFC, 0x1FFC);
    fn_800D30A0(gappVSyncCallback);
}

/* 0x800FEA74 | 0x12C */
void gappBackgroundCallback(void) {
    GSgappTask* task;
    GSgappTask* next;
    BOOL level;

    for (;;) {
        task = _gappListTop;
        while (task != NULL) {
            next = task->next;
            if (task->state == GAPP_STATE_BACKGROUND && task->blocked == FALSE) {
                lbl_8047AC94 = task;
                task->callback((PC_PTR32(task) - PC_PTR32(_gappStructList)) / sizeof(GSgappTask) + 1,
                               task->param);
            }
            task = next;
        }
        lbl_8047AC94 = NULL;

        level = OSDisableInterrupts();
        task = lbl_8047AC9C;
        while (task != NULL) {
            next = task->next;
            gappInsertTask(task);
            task = next;
        }
        lbl_8047AC9C = NULL;
        OSRestoreInterrupts(level);
    }
}

/* 0x800FEBA0 | 0x94 */
void gappVSyncCallback(u32 retraceCount) { /* retraceCount: passed by the GSgfx retrace handler, unused here */
    GSgappTask* task;
    GSgappTask* next;

    task = _gappListTop;
    while (task != NULL) {
        next = task->next;
        if (task->state == GAPP_STATE_VSYNC && task->blocked == FALSE) {
            lbl_8047AC94 = task;
            task->callback((PC_PTR32(task) - PC_PTR32(_gappStructList)) / sizeof(GSgappTask) + 1,
                           task->param);
        }
        task = next;
    }
    lbl_8047AC94 = NULL;
}
