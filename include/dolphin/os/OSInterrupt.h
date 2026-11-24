#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/os/OSInterrupt.h); docs/port/stage2-plan.md. */
#include_next <dolphin/os/OSInterrupt.h>
#include <dolphin/os.h> /* Aurora declares OSDisableInterrupts / OSEnableInterrupts / OSRestoreInterrupts there */
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_OS_OSINTERRUPT_H
#define DOLPHIN_OS_OSINTERRUPT_H

#include "dolphin/types.h"
#include "dolphin/os/OSContext.h"

typedef s16 __OSInterrupt;
typedef void (*__OSInterruptHandler)(__OSInterrupt interrupt, OSContext* context);

BOOL OSDisableInterrupts(void);
BOOL OSEnableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);

__OSInterruptHandler __OSSetInterruptHandler(__OSInterrupt interrupt, __OSInterruptHandler handler);
__OSInterruptHandler __OSGetInterruptHandler(__OSInterrupt interrupt);
void __OSInterruptInit(void);

u32 __OSMaskInterrupts(u32 mask);
u32 __OSUnmaskInterrupts(u32 mask);

#endif /* DOLPHIN_OS_OSINTERRUPT_H */
#endif /* TARGET_PC */
