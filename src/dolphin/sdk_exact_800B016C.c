#include "dolphin/types.h"

extern u16 __CARDEncode;

u16 __CARDGetFontEncode(void)
{
    return __CARDEncode;
}
