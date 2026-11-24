#include "game/battle/battle_waza_types.h"

u32 mailDataBiosGet04(s32 idx)
{
    WazaEntry* entry;

    if (idx < 0 || (u32)idx >= *mailData_number) {
        entry = NULL;
    } else {
        entry = &mailData[idx];
    }
    if (entry == NULL) {
        return 0xFFFF;
    }
    return entry->field_04;
}
