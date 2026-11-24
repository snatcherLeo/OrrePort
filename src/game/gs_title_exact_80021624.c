#include "dolphin/types.h"

extern void titleMenuMain(void);

/* Retained in the original gs_title translation unit despite no direct callsite. */
#pragma force_active on

/* NAME-T3 fn_80021624 */ /* main callback of room 0x3A1 topmenu */
void titleTopMenuMain(void)
{
    titleMenuMain();
}
