#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 fmod(f64 x, f64 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 __cvt_fp2unsigned(f32 x);
#endif
extern f32 lbl_8047CB20;
extern f32 lbl_8047CB1C;
extern f32 lbl_8047CB24;
extern f64 lbl_8047CB28;
extern f32 lbl_8047CB18;
extern f32 lbl_8047CB34;
extern f32 lbl_8047CB30;
extern f32 lbl_804011B8[];

f32 fn_800E0CA0(f32 x)
{
    f32 scale;
    f32 reduced;

    scale = lbl_8047CB1C;
    if (x > lbl_8047CB20) {
        scale = lbl_8047CB24;
        reduced = (f32)fmod(x, lbl_8047CB28);
        x = reduced;
    }
    if (x > lbl_8047CB18) {
        x = lbl_8047CB20 - x;
    }
    x = lbl_8047CB34 * x + lbl_8047CB30;
    return scale * lbl_804011B8[__cvt_fp2unsigned(x)];
}
