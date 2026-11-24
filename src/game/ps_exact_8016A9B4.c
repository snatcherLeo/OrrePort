#include "game/ps_types.h"

extern PSAppSRT* freeAppSRT;
extern u16 psSizeofAppSRT;
extern u16 numActivePSAppSRT;
extern u16 numPeakPSAppSRT;
extern const f32 lbl_8047D5C0;
extern const f32 lbl_8047D5C4;
extern void* HSD_MemAlloc(s32 size);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* destination, s32 value, u32 size);
#endif

PSAppSRT* psAddGeneratorAppSRT(PSGeneratorState* generator, u8 type)
{
    PSAppSRT* appSRT;
    u16 familyId = generator->familyId;

    if (freeAppSRT == NULL) {
        freeAppSRT = HSD_MemAlloc(psSizeofAppSRT);
        if (freeAppSRT != NULL) {
            memset(freeAppSRT, 0, psSizeofAppSRT);
        }
    }

    appSRT = freeAppSRT;
    if (appSRT != NULL) {
        u16 count;

        freeAppSRT = appSRT->next;
        appSRT->next = NULL;
        appSRT->refCount = 1;
        appSRT->flags = 0;
        appSRT->type = type;
        appSRT->rotationX = appSRT->rotationY = appSRT->rotationZ = lbl_8047D5C0;
        appSRT->translationX = appSRT->translationY = appSRT->translationZ = lbl_8047D5C0;
        appSRT->scaleX = appSRT->scaleY = appSRT->scaleZ = lbl_8047D5C4;
        appSRT->destroy = NULL;
        appSRT->active = 0;
        appSRT->owner = NULL;
        appSRT->familyId = familyId;

        count = numActivePSAppSRT + 1;
        numActivePSAppSRT = count;
        if (count > numPeakPSAppSRT) {
            numPeakPSAppSRT = count;
        }
    }

    generator->appSRT = appSRT;
    return appSRT;
}
