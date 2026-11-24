/*
 * hsd_debug_host.c: PC version of HSD_SaveContext (HAL sysdolphin, GameCube: src/hsd/debug.c, 0x80196CE0,
 * registered library assembly; docs/port/asm-routines.md).
 *
 * On the GameCube it is a copy of OSSaveContext: it stores every register (GPRs, GQRs, then the FPU state through
 * OSFillFPUContext) into HAL's panic context lbl_80465080, which HSD_Panic then hands to the panic callback or
 * OSPanic for the crash report. A PC has no PowerPC registers to save, and Aurora's OSContext is an opaque block
 * on the PC, so the context is cleared (a report shows zeros, not stale data) and the host return address is
 * printed: that is the call site a PC debugger needs. A real PC crash handler (host stack trace) belongs in the
 * platform layer later (platform-layer plan, "crash handler").
 */
#include <dolphin/types.h>
#include <dolphin/os/OSContext.h>

#include <stdio.h>
#include <string.h>

extern OSContext lbl_80465080; /* HAL's panic context */

void HSD_SaveContext(void) {
    memset(&lbl_80465080, 0, sizeof(lbl_80465080));
    fprintf(stderr, "[hsd_debug_host] HSD_SaveContext (panic context) called from %p\n", __builtin_return_address(0));
    fflush(stderr);
}
