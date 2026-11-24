#include "game/people/people.h"
#include "game/gs_model.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetPosition(struct GSmodel* model,
                               struct GSvec* position);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelGetPosition(struct GSmodel* model,
                               struct GSvec* position);
#endif
extern struct GSvec* GSmodelGetPositionPtr(struct GSmodel* model);

void peopleApplyPosition(PeopleEntry* entry, struct GSvec* position)
{
    GSmodelSetPosition((struct GSmodel*)entry->modelHandle, position);
}

void fn_8018FC98(PeopleEntry* entry, struct GSvec* position)
{
    GSmodelGetPosition((struct GSmodel*)entry->modelHandle, position);
}

struct GSvec* peopleGetPositionPtr(PeopleEntry* entry)
{
    return GSmodelGetPositionPtr((struct GSmodel*)entry->modelHandle);
}

#ifdef TARGET_PC
/* PC: the person's object id (PeopleEntry.scriptRef, GameCube offset +0x30), for code that read it at that raw offset
 * (hero_move.c getObjID, gs_range_801C766C.c); on the PC the field sits further along (8-byte pointers before it). */
u32 pc_people_object_id(void* person)
{
    return PC_PTR32(((PeopleEntry*)person)->scriptRef);
}
#endif
