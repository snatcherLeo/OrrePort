#include "dolphin/types.h"
#include "game/constants/rooms.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s8 std011_dialogDisplayYesNoMenu(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(s32);
#endif
extern s32 menuSaveLoadSaveGame(s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 floorRequestChange(s32);
#endif

s32 std223_askYesNoThenWarp395(void)
{
    s32 done;
    s32 state;
    s32 input;
    s32 result;

    done = state = result = 0;
    while (done == 0) {
        switch (state) {
        case 0:
            winMsgOpen(2, 0x3C46, 1, 1);
            input = std011_dialogDisplayYesNoMenu();
            winMsgClose(1);
            if (input != 0) {
                done = 1;
            } else {
                state = 2;
            }
            break;
        case 2:
            if (menuSaveLoadSaveGame(1) == 0) {
                state = 3;
                result = 1;
            } else {
                state = 4;
            }
            break;
        case 3:
            floorRequestChange(ROOM_COLOSSEUMBATTLE_MENU);
            state = 4;
            break;
        case 4:
            done = 1;
            break;
        }
    }

    return result;
}
