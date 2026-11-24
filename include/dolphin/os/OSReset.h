#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/os/OSReset.h); docs/port/stage2-plan.md. */
#include_next <dolphin/os/OSReset.h>
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_OS_OSRESET_H
#define DOLPHIN_OS_OSRESET_H

#include "dolphin/types.h"
#include "dolphin/os/OSContext.h"

typedef BOOL (*OSResetCallback)(BOOL final);

typedef struct OSResetFunctionInfo {
    OSResetCallback func;
    u32 priority;
    struct OSResetFunctionInfo* next;
    struct OSResetFunctionInfo* prev;
} OSResetFunctionInfo;

void OSRegisterResetFunction(OSResetFunctionInfo* info);
void OSResetSystem(BOOL reset, u32 resetCode, BOOL forceMenu);
u32  OSGetResetCode(void);
void __OSResetSWInterruptHandler(s16 interrupt, OSContext* context);
void __OSReboot(u32 resetCode, u32 bootDol);

#endif /* DOLPHIN_OS_OSRESET_H */
#endif /* TARGET_PC */
