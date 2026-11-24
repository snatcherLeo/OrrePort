#include "game/fight_action.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightFloorSetStatus(u32 side, u16 index, u32 status,
                              u16 subIndex, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void* context, void* buffData, u8 mode);
#endif

u32 fightActionFlowWazaKiaipantiPre(void* context)
{
    FightAction* action = context;

    fightFloorSetStatus(0, 0, 0x36, 0,
                        PC_PTR32(fightActionBiosGetActorFightTargetPtr(action)));
    wsRunScript(context, fightActionBiosGetBuffDataPtr(action), 0);
    return 1;
}
