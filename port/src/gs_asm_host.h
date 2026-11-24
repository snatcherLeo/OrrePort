/*
 * gs_asm_host.h: PC-only additions the game code needs to use the PC versions of the GS engine's assembly routines
 * (gs_asm_host.c). The game code declares these under TARGET_PC (see docs/port/asm-routines.md).
 */
#ifndef GS_ASM_HOST_H
#define GS_ASM_HOST_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The 8 argument-type bytes of the script native being called (record + 4 in the native table lbl_802E1CF0:
 * 2 = float, 0 = end, anything else = integer). cms_op37_callStandard sets it before cms_CallStdTrampoline under
 * TARGET_PC. Needed on Windows x64, where integer and float arguments share positions; elsewhere it is unused. */
extern const u8* pc_cms_native_types;

/* The typed native call behind cms_CallStdTrampoline: calls fn with its arguments in the order `types` gives,
 * taking integers from ints[] and floats from floats[] in turn. Returns the native's integer result. */
u32 pc_cms_call_native(void* fn, const u8* types, const u32* ints, const f32* floats);

#ifdef __cplusplus
}
#endif

#endif /* GS_ASM_HOST_H */
