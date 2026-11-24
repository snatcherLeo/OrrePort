/*
 * pc_os_sync.c: the SDK's mutexes, condition variables and message queues on the SDK thread scheduler
 * (pc_os_thread.c; docs/port/sdk-threads.md). Aurora declares them but does not define them.
 *
 * Users: the GBA link code (skipped for the prototype) is the main one; game code calls them by address name
 * (OSInitMutex fn_8009F77C, OSLockMutex fn_8009F7B4, OSUnlockMutex fn_8009F890, OSInitCond fn_8009F9C8, OSWaitCond
 * fn_8009F9E8, OSSignalCond fn_8009FABC, OSInitMessageQueue fn_8009F1D0: docs/port/sdk-address-names.md).
 *
 * As with threads, the game's OSMutex / OSCond / OSMessageQueue memory is never written: the game sizes it for the
 * GameCube (an OSMessageQueue's message array holds 4-byte messages there, 8-byte ones in Aurora's PC layout). The
 * port keeps its own record per object, keyed by its address; messages are stored in the record.
 *
 * Semantics follow the SDK: a mutex is recursive and owned by a thread; OSWaitCond releases the mutex completely,
 * sleeps, then takes it back with the same lock count; OSSignalCond wakes every waiter; message queues are FIFO,
 * OSJamMessage puts a message at the front, and OS_MESSAGE_BLOCK makes a full send / empty receive sleep.
 */
#include <dolphin/os.h>

#include <stdio.h>
#include <string.h>

extern void pc_os_thread_sleep_on(const void* key);
extern void pc_os_thread_wakeup(const void* key);

#define PC_SYNC_MAX 64
#define PC_MESSAGES_MAX 64

typedef struct PcMutex {
    const void* key;
    OSThread* owner;
    s32 count;
} PcMutex;

typedef struct PcQueue {
    const void* key;
    s32 capacity;
    s32 first;
    s32 used;
    void* messages[PC_MESSAGES_MAX];
} PcQueue;

static PcMutex s_mutexes[PC_SYNC_MAX];
static PcQueue s_queues[PC_SYNC_MAX];

static PcMutex* mutex_record(const void* key, int create) {
    int i, free_slot = -1;
    for (i = 0; i < PC_SYNC_MAX; i++) {
        if (s_mutexes[i].key == key) {
            return &s_mutexes[i];
        }
        if (s_mutexes[i].key == NULL && free_slot < 0) {
            free_slot = i;
        }
    }
    if (!create || free_slot < 0) {
        if (create) {
            fprintf(stderr, "[pc_os_sync] more than %d mutexes\n", PC_SYNC_MAX);
        }
        return NULL;
    }
    memset(&s_mutexes[free_slot], 0, sizeof(s_mutexes[free_slot]));
    s_mutexes[free_slot].key = key;
    return &s_mutexes[free_slot];
}

static PcQueue* queue_record(const void* key) {
    int i;
    for (i = 0; i < PC_SYNC_MAX; i++) {
        if (s_queues[i].key == key) {
            return &s_queues[i];
        }
    }
    return NULL;
}

/* ---- mutexes ---- */

void OSInitMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    PcMutex* m = mutex_record(mutex, 1);
    if (m != NULL) {
        m->owner = NULL;
        m->count = 0;
    }
    OSRestoreInterrupts(enabled);
}

void OSLockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* self = OSGetCurrentThread();
    PcMutex* m = mutex_record(mutex, 1); /* a mutex the game never initialised is treated as fresh */
    if (m != NULL) {
        while (m->owner != NULL && m->owner != self) {
            pc_os_thread_sleep_on(mutex);
        }
        m->owner = self;
        ++m->count;
    }
    OSRestoreInterrupts(enabled);
}

BOOL OSTryLockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    OSThread* self = OSGetCurrentThread();
    PcMutex* m = mutex_record(mutex, 1);
    BOOL ok = FALSE;
    if (m != NULL && (m->owner == NULL || m->owner == self)) {
        m->owner = self;
        ++m->count;
        ok = TRUE;
    }
    OSRestoreInterrupts(enabled);
    return ok;
}

void OSUnlockMutex(OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    PcMutex* m = mutex_record(mutex, 0);
    if (m != NULL && m->owner == OSGetCurrentThread() && --m->count == 0) {
        m->owner = NULL;
        pc_os_thread_wakeup(mutex);
    }
    OSRestoreInterrupts(enabled);
}

/* ---- condition variables ---- */

void OSInitCond(OSCond* cond) { (void)cond; /* waiters are kept by address */ }

void OSWaitCond(OSCond* cond, OSMutex* mutex) {
    BOOL enabled = OSDisableInterrupts();
    PcMutex* m = mutex_record(mutex, 0);
    if (m != NULL && m->owner == OSGetCurrentThread()) {
        s32 count = m->count;
        m->owner = NULL;
        m->count = 0;
        pc_os_thread_wakeup(mutex);
        pc_os_thread_sleep_on(cond);
        while (m->owner != NULL) {
            pc_os_thread_sleep_on(mutex);
        }
        m->owner = OSGetCurrentThread();
        m->count = count;
    }
    OSRestoreInterrupts(enabled);
}

void OSSignalCond(OSCond* cond) { pc_os_thread_wakeup(cond); }

/* ---- message queues ---- */

void OSInitMessageQueue(OSMessageQueue* mq, OSMessage* msgArray, s32 msgCount) {
    BOOL enabled = OSDisableInterrupts();
    PcQueue* q = queue_record(mq);
    int i;
    (void)msgArray;
    if (q == NULL) {
        for (i = 0; i < PC_SYNC_MAX && s_queues[i].key != NULL; i++) {
        }
        q = i < PC_SYNC_MAX ? &s_queues[i] : NULL;
    }
    if (q == NULL || msgCount > PC_MESSAGES_MAX) {
        fprintf(stderr, "[pc_os_sync] message queue %p: no room (%d messages)\n", (void*)mq, (int)msgCount);
    } else {
        memset(q, 0, sizeof(*q));
        q->key = mq;
        q->capacity = msgCount;
    }
    OSRestoreInterrupts(enabled);
}

static BOOL send(OSMessageQueue* mq, OSMessage msg, s32 flags, int front) {
    BOOL enabled = OSDisableInterrupts();
    PcQueue* q = queue_record(mq);
    BOOL ok = FALSE;
    if (q != NULL) {
        while (q->used >= q->capacity && (flags & OS_MESSAGE_BLOCK)) {
            pc_os_thread_sleep_on((const char*)mq + 1); /* senders wait on a key next to the receivers' */
        }
        if (q->used < q->capacity) {
            if (front) {
                q->first = (q->first + q->capacity - 1) % q->capacity;
                q->messages[q->first] = msg;
            } else {
                q->messages[(q->first + q->used) % q->capacity] = msg;
            }
            ++q->used;
            pc_os_thread_wakeup(mq);
            ok = TRUE;
        }
    }
    OSRestoreInterrupts(enabled);
    return ok;
}

BOOL OSSendMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) { return send(mq, msg, flags, 0); }
BOOL OSJamMessage(OSMessageQueue* mq, OSMessage msg, s32 flags) { return send(mq, msg, flags, 1); }

BOOL OSReceiveMessage(OSMessageQueue* mq, OSMessage* msg, s32 flags) {
    BOOL enabled = OSDisableInterrupts();
    PcQueue* q = queue_record(mq);
    BOOL ok = FALSE;
    if (q != NULL) {
        while (q->used == 0 && (flags & OS_MESSAGE_BLOCK)) {
            pc_os_thread_sleep_on(mq);
        }
        if (q->used > 0) {
            if (msg != NULL) {
                *msg = q->messages[q->first];
            }
            q->first = (q->first + 1) % q->capacity;
            --q->used;
            pc_os_thread_wakeup((const char*)mq + 1);
            ok = TRUE;
        }
    }
    OSRestoreInterrupts(enabled);
    return ok;
}
