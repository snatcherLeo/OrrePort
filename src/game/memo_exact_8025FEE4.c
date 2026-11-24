#include "dolphin/types.h"

typedef struct MemoData {
    u16 count;
} MemoData;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* savedataGetStatus(s32 side, s32 slotType);
#endif

u16 memoDataGetCount(MemoData* data)
{
    if (data == NULL) {
        data = (MemoData*)PC_PTR(savedataGetStatus(0, 0xC));
    }
    return data->count;
}
