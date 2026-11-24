#include "game/ps_types.h"

extern PSAppSRT* freeAppSRT;
extern u16 numPeakPSAppSRT;
extern u16 numActivePSAppSRT;
extern u16 psSizeofAppSRT;
extern void HSD_Free(void* allocation);
extern void* HSD_MemAlloc(s32 size);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* destination, s32 value, u32 size);
#endif

void psRemoveAppSRT(void)
{
    PSAppSRT* appSRT = freeAppSRT;

    while (appSRT != NULL) {
        PSAppSRT* next = appSRT->next;
        HSD_Free(appSRT);
        appSRT = next;
    }

    freeAppSRT = NULL;
}

s32 psInitAppSRT(s32 count, s32 size)
{
    s32 i = 0;

    numActivePSAppSRT = 0;
    numPeakPSAppSRT = 0;
    psSizeofAppSRT = size;
    freeAppSRT = NULL;

    while (i < count) {
        PSAppSRT* appSRT = HSD_MemAlloc(size);

        if (appSRT == NULL) {
            return i;
        }

        memset(appSRT, 0, size);
        appSRT->next = freeAppSRT;
        freeAppSRT = appSRT;
        i++;
    }

    return i;
}
