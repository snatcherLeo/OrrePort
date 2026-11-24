/**
 * @file msgctrl_candidate_80131714.c
 * @brief msgctrl.c carve, 0x80131714 - 0x801317FC: msgctrlAlign,
 *        msgctrlShadow, msgctrlMenuMoney and msgctrlMenuFullDigit.
 *
 * Text only. Built with the TU's -O4,p and the peephole pass off (one
 * unit-wide flag in configure.py, as for msgctrl.c and
 * msgctrl_exact_80132A38.c); with the pass on, the digit wrappers lose
 * retail's unfolded argument moves. The same bodies are in
 * src/game/msgctrl.c.
 */
#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

s32 msgctrlAlign(EffectUtilCommandObj* obj)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmsgAdjustAlign(void*);
#endif
    u8* stream;

    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->alignMode = stream[0];
        GSmsgAdjustAlign(obj);
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}

u32 msgctrlShadow(EffectUtilCommandObj* obj)
{
    u8* stream;

    if (obj->activeFlag != 0) {
        stream = P32(obj->stream);
        obj->field_02 = *stream;
    }
    stream = P32(obj->stream);
    P32_SET(obj->stream, stream + 1);
    return 0;
}

u32 msgctrlMenuMoney(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80426FF0, 0x10, lbl_8047AE94, 4));
}

u32 msgctrlMenuFullDigit(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427010, 0x10, lbl_8047AE68, 5));
}
