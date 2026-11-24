#include "game/colosseum.h"
#include "game/fight_action.h"

u32 fightActionFlowNullFunc(void* action) { return 1; }

void fightActionBiosSetFifoBanme(FightAction* action, s32 fifoIndex)
{
    if (action == NULL) { return; }
    action->fifoIndex = fifoIndex;
}

FightActionCallback fightActionKindDataBiosGetDispFuncPtr(FightActionKindData* data)
{
    if (data == NULL) { return NULL; }
    return data->display;
}

FightActionCallback fightActionKindDataBiosGetFlowFuncPtr(FightActionKindData* data)
{
    if (data == NULL) { return NULL; }
    return data->flow;
}

s32 fightActionKindDataBiosGetPri(FightActionKindData* data)
{
    s32 priority;

    if (data == NULL) {
        priority = -128;
    } else {
        priority = (s8)data->rawPriority;
    }
    return priority;
}

FightActionKindData* fightActionKindDataBiosGetPtr(u16 index)
{
    extern u32 fight_action_kind_data_number;
    if (index >= fight_action_kind_data_number) {
        return NULL;
    }
    return &fight_action_kind_data[index];
}

u32 fightActionDataBiosGetBuff(FightActionData* data)
{
    if (data == NULL) { return 0; }
    return data->buff;
}

u16 fightActionDataBiosGetKind(FightActionData* data)
{
    if (data == NULL) { return 0; }
    return data->kind;
}
