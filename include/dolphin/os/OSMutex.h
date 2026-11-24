#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/os/OSMutex.h); docs/port/stage2-plan.md. */
#include_next <dolphin/os/OSMutex.h>
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_OS_OSMUTEX_H
#define DOLPHIN_OS_OSMUTEX_H

#include "dolphin/types.h"
#include "dolphin/os/OSThread.h"

void OSInitMutex(OSMutex* mutex);
void OSLockMutex(OSMutex* mutex);
void OSUnlockMutex(OSMutex* mutex);
BOOL OSTryLockMutex(OSMutex* mutex);
void __OSUnlockAllMutex(OSThread* thread);

#endif /* DOLPHIN_OS_OSMUTEX_H */
#endif /* TARGET_PC */
