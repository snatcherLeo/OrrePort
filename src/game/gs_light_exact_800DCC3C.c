#include "hsd/hsd_lobj.h"

#include "game/gs_light.h" /* GSlight */

void GSlightSetTarget(GSlight* light, Vec* target)
{
    HSD_LObjSetInterest(light->lobj, target);
}

void GSlightSetPosition(GSlight* light, Vec* position)
{
    HSD_LObjSetPosition(light->lobj, position);
}
