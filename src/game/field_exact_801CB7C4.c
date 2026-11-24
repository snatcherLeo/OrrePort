#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* peopleIsInGroup(u32 group, u32 resource);
#endif
extern void peopleStopAnimation(u32 group, u32 resource);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSresGetResource(u32 group, u32 resource);
#endif
extern void GSmodelStopAnimation(void* model);

void std116_mapObjectStopAnimation(u32 resource)
{
    u32 group = std093_getCurrentGroupID();
    void* model;

    if (peopleIsInGroup(group, resource) != NULL) {
        peopleStopAnimation(group, resource);
    } else {
        model = GSresGetResource(group, resource);
        if (model != NULL) {
            GSmodelStopAnimation(model);
        }
    }
}
