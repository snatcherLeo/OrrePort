/**
 * @file gs_colsys_exact_8010D170.c
 * @brief GScolsys2 collision debug-draw initialisation, 0x8010D170 - 0x8010D20C.
 *
 * Carved out of the gs_range_8010CBD0.c candidate range so it can link on
 * its own; it is built with that range's flags (GC/1.3 -O3).
 */
#include "dolphin/types.h"
#include "game/gs_colsys.h"

extern GSColSysState GScolsys2;

extern u8* gfxVtxDescAlloc(void);
#ifdef TARGET_PC
/* PC: the 7th argument (data) is a pointer passed on the stack on x86-64 (gs_gfx_layer_state.c); declared u32, a
 * literal 0 filled only half the 8-byte slot, so the callee could see a garbage non-NULL pointer. */
extern void gfxVtxDescSetAttr(void*, u32, u32, u32, u32, u8, void*, u8);
#else
extern void gfxVtxDescSetAttr(u8*, u32, u32, u32, u32, u8, u32, u8);
#endif

void fn_8010D170(void)
{
    u8* handle;

    GScolsys2.activeLayer = 0;
    P32_SET(GScolsys2.displayList, NULL);
    handle = gfxVtxDescAlloc();
    GScolsys2.gfxRenderHandle = PC_PTR32(handle);
    gfxVtxDescSetAttr(handle, 1, 0, 1, 4, 0, 0, 0);
    gfxVtxDescSetAttr((u8*)PC_PTR(GScolsys2.gfxRenderHandle), 4, 0, 6, 10, 0, 0, 0);
    P32_SET(GScolsys2.displayList, NULL);
}
