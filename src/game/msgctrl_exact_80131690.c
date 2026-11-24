/**
 * @file msgctrl_exact_80131690.c
 * @brief Strict message-control helpers, 0x80131690 - 0x80131714.
 */
#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

u32 msgctrlIndentOff(EffectUtilCommandObj* obj)
{
    obj->field_41 = 1;
    return 0;
}

u16 msgctrlNpc(void)
{
    return _Npc;
}

u32 msgctrlTribe(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return fightTrainerKindDataBiosGetPrefixName(fightTrainerKindDataBiosGetPtr(_Tribe)); /* r3 = the kind data pointer (assembly) */
}

u32 msgctrlString2(void)
{
    return _String2;
}

u32 msgctrlMenuMsgID2(void)
{
    return _MenuMsgID2;
}

u32 msgctrlMenuMsgID(void)
{
    return _MenuMsgID;
}

u32 msgctrlTalkSE(EffectUtilCommandObj* obj)
{
    u8* stream;

    if (obj->activeFlag == 0) {
        stream = P32(obj->stream);
        obj->field_03 = *stream;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}
