/* Standalone carve of _seaStop (_envMapStart), 0x8013C670 - 0x8013C718. */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelStopTexAnimation(void* model);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetVisibility(void* model, u32 visible);
#endif
extern void GXDrawDone(void);
extern void GXInvalidateVtxCache(void);
extern u32 _seaResetMaterial(void* model, void* work);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u32 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u32 handle);
#endif

/* RULE-EXCEPTION(user-approved): local optimizer control -- see docs/RULE_EXCEPTIONS.md. */
#pragma push
#pragma global_optimizer off
u32 _seaStop(void* arg) {
    void* ptr;
    void* inner;

    if (arg != 0) {
        ptr = arg;
#ifdef TARGET_PC
        /* PC: the model is a 32-bit slot of the sea work record (seaAllocMesh stores it so, effect_visual.c) */
        inner = PTR32_AT_T(void, (u8*)ptr, 0);
#else
        inner = *(void**)ptr;
#endif
        GSmodelStopTexAnimation(inner);
        if (inner != 0) {
            GSmodelSetVisibility(inner, 0);
        }
        GXDrawDone();
        GXInvalidateVtxCache();
        _seaResetMaterial(inner, ptr);

        {
            u16 val;
            val = *(u16*)((u8*)ptr + 0x1c);
            if (val != 0) {
                fn_800E24B0(val);
                GSmemFree(val);
            }
        }

        ptr = (void*)PC_PTR((u32)*(u16*)((u8*)ptr + 0x8c));
        if (ptr != 0) {
            fn_800E24B0(PC_PTR32(ptr));
            GSmemFree(PC_PTR32(ptr));
        }
    }
    return 1;
}
#pragma pop
