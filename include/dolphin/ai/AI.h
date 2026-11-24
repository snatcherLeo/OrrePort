#ifdef TARGET_PC
/* OrrePort (PC build): this SDK header hands over to Aurora's (dolphin/ai.h); docs/port/stage2-plan.md. */
#include <dolphin/ai.h>
#else /* GameCube build: unchanged */
#ifndef DOLPHIN_AI_AI_H
#define DOLPHIN_AI_AI_H

#include "dolphin/types.h"

typedef void (*AISCallback)(u32 count);
typedef void (*AIDCallback)(void);

void AIInit(u8* stack);
void AIReset(void);
void AIStartDMA(void);
void AIStopDMA(void);
void AIInitDMA(u32 addr, u32 length);
BOOL AICheckInit(void);
u32  AIGetDMABytesLeft(void);
u32  AIGetDMAStartAddr(void);
u16  AIGetDMALength(void);
u32  AIGetDSPSampleRate(void);
void AISetDSPSampleRate(u32 rate);
AISCallback AIRegisterStreamCallback(AISCallback callback);
AIDCallback AIRegisterDMACallback(AIDCallback callback);

#endif /* DOLPHIN_AI_AI_H */
#endif /* TARGET_PC */
