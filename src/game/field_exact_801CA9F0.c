#include "dolphin/types.h"

extern s32 _scriptEventColID;

s32 scriptGetEventColID(void)
{
    return _scriptEventColID;
}
