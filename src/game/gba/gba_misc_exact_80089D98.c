#include "dolphin/types.h"

/* Retail stores this six-entry retry table in small uninitialized data. */
extern u16 lbl_8047A684[6] __attribute__((section(".sdata")));

/* NAME-T1 fn_80089D98 */
s32 gbaCommandChangePokemonIsPrepared(s32 index)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80072548(s32);
#endif
    s32 status;

    status = fn_80072548(index - 1);
    if (status < 0) {
        lbl_8047A684[index - 1] = 0;
    } else if (status == 1 || status == 2) {
        if (++lbl_8047A684[index - 1] <= 10) {
            status = -1;
        }
    }

    return status;
}
