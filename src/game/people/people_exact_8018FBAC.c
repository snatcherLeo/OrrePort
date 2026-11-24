#include "game/people/people.h"
#include "game/gs_model.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecCopy(void* destination, const void* source);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetRotation(struct GSmodel* model,
                               struct GSvec* rotation);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetRotation(struct GSmodel* model, struct GSvec* rotation);
#endif

void peopleClearFlags(PeopleEntry* entry, u32 mask)
{
    entry->flags &= ~mask;
}

void peopleSetFlags(PeopleEntry* entry, u32 mask)
{
    entry->flags |= mask;
}

void peopleWriteFlags(PeopleEntry* entry, u32 flags)
{
    entry->flags = flags;
}

void* peopleGetModel(PeopleEntry* entry)
{
    return entry->modelHandle;
}

void peopleSetTransform(PeopleEntry* entry, void* transform)
{
    GSvecCopy(entry->transform, transform);
}

void* peopleGetTransform(PeopleEntry* entry)
{
    return entry->transform;
}

void peopleApplyRotation(PeopleEntry* entry, struct GSvec* rotation)
{
    GSmodelSetRotation((struct GSmodel*)entry->modelHandle, rotation);
}

void peopleGetRotation(PeopleEntry* entry, struct GSvec* rotation)
{
    GSmodelGetRotation((struct GSmodel*)entry->modelHandle, rotation);
}
