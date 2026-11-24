#include "dolphin/types.h"
#include "game/people/people.h"

extern u8 lbl_8047B3C8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* peopleOpen(u32 group, s32 people_id, void* param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* peopleSearchID(void* people);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetBoundCheck(void* model, u8 enable);
#endif

s32 std112_modelGetResourceID(void* param)
{
    s32 people_id;
    u32 raw_id;
    void* people;

    raw_id = lbl_8047B3C8;
    lbl_8047B3C8 = raw_id + 1;
    people_id = (s8)raw_id | 0x7FFE0000;

    people = peopleOpen(std093_getCurrentGroupID(), people_id, PC_PTR32_IMPLICIT(param));
    if (people == NULL) {
        return 0;
    }

    GSmodelSetBoundCheck(peopleGetModel(peopleSearchID(people)), 0);
    return people_id;
}
