#include "dolphin/types.h"

extern s32 _scriptEventColID;

s32 scriptSetEventColID(s32 id)
{
    s32 old = _scriptEventColID;

    _scriptEventColID = id;
    return old;
}
