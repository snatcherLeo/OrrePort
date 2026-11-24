#include "dolphin/types.h"

#include "game/gs_model_struct.h" /* GSmodel (flags.raw: native on the PC, bit-fields reversed) */

void GSmodelClearShadowFlags(GSmodel* model, u32 flags)
{

    if ((flags & 1U) != 0U) {
        model->flags.raw &= ~0x10000000U;
    }
    if ((flags & 2U) != 0U) {
        model->flags.raw &= ~0x20000000U;
    }
    if ((flags & 4U) != 0U) {
        model->flags.raw &= ~0x40000000U;
    }
}

void GSmodelSetShadowFlags(GSmodel* model, u32 flags)
{

    if ((flags & 1U) != 0U) {
        model->flags.raw |= 0x10000000U;
    }
    if ((flags & 2U) != 0U) {
        model->flags.raw |= 0x20000000U;
    }
    if ((flags & 4U) != 0U) {
        model->flags.raw |= 0x40000000U;
    }
}
