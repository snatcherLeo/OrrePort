#include "game/gs_thread.h"

extern u32 _activeThread;
extern GSThread* _threadList;
extern u32 _maxThreads;
extern u8 _rescan;

u8 fn_800F036C(GSThread* thread)
{
    return thread->affinity;
}

u32 fn_800F0374(u32 context)
{
    GSThread* thread;

    thread = (GSThread*)PC_PTR(context);
    return thread->priority;
}

u32 GSthreadGetCurrentThread(void)
{
    return _activeThread;
}

void GSthreadUnblockGroup(u32 priority)
{
    u32 index;
    GSThread* thread;

    if (priority == 0) {
        return;
    }

    for (index = 0; index < _maxThreads; index++) {
        thread = &_threadList[index];
        if (thread->priority == priority) {
            thread->sleeping = 0;
        }
    }

    _rescan = 1;
}

void GSthreadBlockGroup(u32 priority)
{
    u32 index;
    GSThread* thread;

    if (priority == 0) {
        return;
    }

    for (index = 0; index < _maxThreads; index++) {
        thread = &_threadList[index];
        if (thread->priority == priority) {
            thread->sleeping = 1;
        }
    }

    _rescan = 1;
}
