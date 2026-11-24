/** Exact tool-entry resource cleanup, 0x8025CD64 - 0x8025CDB8. */
#include "dolphin/types.h"

extern u32 _toolentryTaisen;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmemGetHandleByPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E24B0();
#endif

void toolentryTaisenFreePokemonData(void* ctx, u32 slot, u32 param)
{
    u32 handle;
    u32 result;

    handle = _toolentryTaisen;
    if (handle != 0) {
        result = GSmemGetHandleByPtr(PC_PTR(handle));
        if ((result & 0xFFFF) != 0) {
            fn_800E24B0(result);
            GSmemFree(result);
        }
        _toolentryTaisen = 0;
    }
}
