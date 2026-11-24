/**
 * @file gs_vm_exact_800F10E8.c
 * @brief GS script VM "call native" opcode.
 *
 * Address range: 0x800F10E8 - 0x800F13D0 (fn_800F10E8).
 *
 * A .text-only unit carved from the GS VM translation unit (game/gs_vm.c,
 * which holds the same definition). fn_800F106C, the hand-written
 * native-call trampoline that opens the retail unit, stays in its own unit.
 * The VM's data stays extern here (see gs_vm.h).
 */

#include "dolphin/types.h"
#include "game/gs_vm.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dest, int val, u32 n);
#endif
extern u32 cms_CallStdTrampoline(void);

/* 0x800F10E8 | 0x2E8 */
/* Script VM "call native" opcode: builds a call frame from the operand
 * stack, looks the callee up in the native-function table (0xC-byte records:
 * function pointer + 8 argument-type bytes, 2 = float, 0 = end), splits the
 * arguments into the integer/float register images consumed by
 * fn_800F106C, stores the result in the frame's return slot and unwinds. */
s32 cms_op37_callStandard(GSVMCtx* ctx)
{
    u8* record;
    u32 result;
    s32 valueCount;
    s32 intArgCount;
    s32 floatArgCount;
    s32 i;

    result = 0;
    GSvmEnterNative(ctx);

    record = (u8*)lbl_80478B00->natives + GSvmFrameSlot(ctx, 0)->u * 0xC;
#ifdef TARGET_PC /* the native table (lbl_802E1CF0) is raw big-endian: a 4-byte relocated slot, then 8 type bytes */
    lbl_8047AC38 = (u32 (*)(void))PC_PTR(DISC_U32(record));
#else
    lbl_8047AC38 = *(u32 (**)(void))record;
#endif
    if (lbl_8047AC38 != NULL) {
        floatArgCount = 0;
        intArgCount = 0;
        memset(lbl_80401BD8, 0, 0x20);
        memset(lbl_80401BB8, 0, 0x20);
        valueCount = ctx->stackCount - ctx->frame - 3;
        if (valueCount > 8) {
            valueCount = 8;
        }
        for (i = 0; i < valueCount; i++) {
            if (record[i + 4] == 0) {
                break;
            }
            if (record[i + 4] == 2) {
                lbl_80401BB8[floatArgCount++] = GSvmFrameSlot(ctx, i + 1)->f;
            } else {
                lbl_80401BD8[intArgCount++] = GSvmFrameSlot(ctx, i + 1)->u;
            }
        }
        lbl_8047AC3C = lbl_80401BB8;
        lbl_8047AC40 = lbl_80401BD8;
#ifdef TARGET_PC
        {
            /* observe-only: the argument images as they are at the call (a waiting native yields while other threads
             * reuse them; gs_vm.h, pc_trace_native) */
            u32 traceInts[8];
            f32 traceFloats[8];
            memcpy(traceInts, lbl_80401BD8, sizeof(traceInts));
            memcpy(traceFloats, lbl_80401BB8, sizeof(traceFloats));
            result = cms_CallStdTrampoline();
            pc_trace_native(ctx, GSvmFrameSlot(ctx, 0)->u, traceInts, intArgCount, result, traceFloats);
        }
#else
        result = cms_CallStdTrampoline();
#endif
    }

    ctx->stack[ctx->frame - 1].u = result;
    GSvmReturn(ctx);
    return 1;
}
