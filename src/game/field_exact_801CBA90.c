#include "dolphin/types.h"
#include "game/constants/rooms.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorRequestChange(u32);
#endif

s32 std229_mapLoad395(void)
{
    floorRequestChange(ROOM_COLOSSEUMBATTLE_MENU);
    return 0;
}
