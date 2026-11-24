#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/os/OSMemory.h); docs/port/stage2-plan.md. */
#include_next <dolphin/os/OSMemory.h>
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_OS_OSMEMORY_H
#define DOLPHIN_OS_OSMEMORY_H

#include "dolphin/types.h"

void __OSInitMemoryProtection(void);
void __OSModuleInit(void);

#endif /* DOLPHIN_OS_OSMEMORY_H */
#endif /* TARGET_PC */
