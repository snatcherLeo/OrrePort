/**
 * @file msgctrl_r49_80131A34_suffix.c
 * @brief msgctrl.c carve, 0x80131A34 - 0x80131BA0: the digit wrappers
 *        msgctrlMoney, msgctrlMenuZDigit2/ZDigit, msgctrlMenuHex2/Hex and
 *        msgctrlMenuUDigit2/UDigit.
 *
 * Text only. Built with the TU's -O4,p and the peephole pass off (one
 * unit-wide flag in configure.py, as for msgctrl.c and
 * msgctrl_exact_80132A38.c); with the pass on, each wrapper loses retail's
 * unfolded argument moves. The same bodies are in src/game/msgctrl.c.
 */
#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

u32 msgctrlMoney(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427070, 0x10, lbl_8047AE80, 4));
}

u32 msgctrlMenuZDigit2(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427090, 0x10, lbl_8047AE6C, 2));
}

u32 msgctrlMenuZDigit(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804270B0, 0x10, lbl_8047AE68, 2));
}

u32 msgctrlMenuHex2(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804270D0, 0x10, lbl_8047AE68, 3));
}

u32 msgctrlMenuHex(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804270F0, 0x10, lbl_8047AE68, 3));
}

u32 msgctrlMenuUDigit2(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427110, 0x10, lbl_8047AE6C, 1));
}

u32 msgctrlMenuUDigit(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427130, 0x10, lbl_8047AE68, 1));
}
