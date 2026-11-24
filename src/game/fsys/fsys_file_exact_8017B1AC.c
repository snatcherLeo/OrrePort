#include "dolphin/types.h"

#ifdef TARGET_PC
/* PC: the drive status passes through (the GameCube body is a tail call, so its r3 is GSdvdGetDriveStatus's result;
 * the title, the party-swap screen and the movie player read it, 4 / 5 / 11 pause them). */
extern s32 GSdvdGetDriveStatus(void);

s32 fn_8017B1AC(void)
{
    return GSdvdGetDriveStatus();
}
#else
extern void GSdvdGetDriveStatus(void);

void fn_8017B1AC(void)
{
    GSdvdGetDriveStatus();
}
#endif
