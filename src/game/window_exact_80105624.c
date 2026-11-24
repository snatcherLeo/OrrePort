#include "dolphin/types.h"

typedef struct WindowSystemWork {
    u8 pad_00[0x10];
    u8 keyInfo;
} WindowSystemWork;

extern WindowSystemWork winSysWork;

void* windowGetKeyInfo(void)
{
    return &winSysWork.keyInfo;
}
