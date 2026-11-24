/*
 * musyx_crt_stub.c: the one C-runtime constant MusyX uses, for the standalone audio test only. In the full game it
 * comes from the C runtime units (src/crt) like everything else there. `__float_nan` (lbl_80478AC0, .sdata) is a
 * NaN bit pattern, 0x7FFFFFFF on the disc (include/crt/math_ppc.h: `#define NAN (*(f32*)lbl_80478AC0)`).
 */
#include <stdint.h>

const uint32_t lbl_80478AC0[1] = {0x7FFFFFFFu};
