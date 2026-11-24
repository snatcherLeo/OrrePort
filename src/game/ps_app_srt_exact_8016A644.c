#include "game/ps_types.h"
#include "game/script/script.h"

extern PSAppSRT* freeAppSRT;
extern u16 numActivePSAppSRT;

s32 psRemoveGeneratorAppSRT(PSGeneratorState* generator)
{
    PSAppSRT* appSRT = generator->appSRT;
    u16 refCount;

    if (appSRT == NULL) {
        return -1;
    }

    if (appSRT->owner == generator) {
        appSRT->owner = NULL;
    }

    appSRT = generator->appSRT;
    refCount = appSRT->refCount - 1;
    appSRT->refCount = refCount;

    if (refCount == 0) {
        appSRT = generator->appSRT;
        if (appSRT->destroy != NULL) {
            appSRT->destroy(appSRT);
        }
        appSRT->next = freeAppSRT;
        freeAppSRT = appSRT;
        numActivePSAppSRT--;
    }

    generator->appSRT = NULL;
    return refCount;
}

s32 psRemoveParticleAppSRT(PSParticle* particle)
{
    PSAppSRT* appSRT = particle->parentObj;
    u16 refCount;

    if (appSRT == NULL) {
        return -1;
    }

    refCount = appSRT->refCount - 1;
    appSRT->refCount = refCount;

    if (refCount == 0) {
        appSRT = particle->parentObj;
        if (appSRT->destroy != NULL) {
            appSRT->destroy(appSRT);
        }
        appSRT->next = freeAppSRT;
        freeAppSRT = appSRT;
        numActivePSAppSRT--;
    }

    particle->parentObj = NULL;
    return refCount;
}
