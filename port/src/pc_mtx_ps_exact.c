/*
 * pc_mtx_ps_exact.c: paired-single math routines rewritten to round exactly as the GameCube's (OrrePort, Main
 * 2026-10-19). The SDK's PS* routines are hand-written paired-single assembly (src/dolphin/sdk_range_800A2D38.c): each
 * ps_madd / ps_msub is one fused multiply-add (one rounding) and the terms are summed in the assembly's order.
 * Aurora's C versions (C_MTX*, C_VEC*) sum in another order with a rounding after every multiply, so their results can
 * differ from the console's in the last bit. That is enough to change a grazing collision test: S02's replay showed
 * an NPC sliding along Phenac's fountain wall 4 steps later on the PC than in Dolphin (colsysWallSweep ->
 * PSMTXMultVec). Each function here follows its assembly operation by operation, with fmaf for every fused operation.
 * port/tools/gen_ps_math.py leaves out the names defined here when it writes the plain forwarders (pc_mtx_ps.c).
 *
 * Notation in the comments: a pair register (ps0, ps1). psq_l with W=1 loads (value, 1.0).
 */
#include <math.h>
#include <dolphin/types.h>
#include <dolphin/mtx.h>

#undef PSMTXMultVec
/* psq_l f0 = (x, y); f2 = (m00, m01); f1 = (z, 1); ps_mul f4 = (m00*x, m01*y); f3 = (m02, m03);
 * ps_madd f5 = (m02*z + f4.ps0, m03*1 + f4.ps1); ps_sum0 -> f5.ps0 + f5.ps1. Rows 1 and 2 the same. */
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst) {
    const f32 x = src->x, y = src->y, z = src->z;
    f32 out[3];
    for (int r = 0; r < 3; r++) {
        const f32 ax = m[r][0] * x;
        const f32 by = m[r][1] * y;
        const f32 lo = fmaf(m[r][2], z, ax);
        const f32 hi = m[r][3] + by; /* fused m03 * 1.0 + by: the product is exact, so one rounding of the sum */
        out[r] = lo + hi;
    }
    dst->x = out[0];
    dst->y = out[1];
    dst->z = out[2];
}
