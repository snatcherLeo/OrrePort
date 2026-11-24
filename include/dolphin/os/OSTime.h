#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/os/OSTime.h); docs/port/stage2-plan.md. */
#include_next <dolphin/os/OSTime.h>
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_OS_OSTIME_H
#define DOLPHIN_OS_OSTIME_H

#include "dolphin/types.h"

s64 OSGetTime(void);
u32 OSGetTick(void);
s64 __OSGetSystemTime(void);

#endif /* DOLPHIN_OS_OSTIME_H */
#endif /* TARGET_PC */
