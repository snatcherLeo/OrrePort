/* PC versions of the MWCC / PowerPC compiler intrinsics the game code calls (OrrePort).
 *
 * On the GameCube these are built into the compiler (each becomes one instruction); on the PC they would be implicit
 * declarations returning int. PC only: included by include/pc/prototypes.h. Semantics follow the PowerPC
 * instructions:
 *   __cntlzw(x)   count leading zeros of a 32-bit word; 32 for 0 (cntlzw)
 *   __abs(x)      absolute value of an int
 *   __fabs(x)     absolute value of a double
 *   __frsqrte(x)  reciprocal square-root estimate (frsqrte); the PC computes the exact value, which is within the
 *                 estimate's error bound, so callers that refine it with Newton steps get the same or a closer result
 *   __memcpy      memcpy */
#ifndef PC_MWCC_INTRINSICS_H
#define PC_MWCC_INTRINSICS_H
#ifdef TARGET_PC
/* GCC builtins only: the host <math.h> would clash with the game's own MSL math (crt/math_ppc.h) */
static inline unsigned int __cntlzw(unsigned int x) { return x ? (unsigned int)__builtin_clz(x) : 32u; }
static inline int __abs(int x) { return x < 0 ? -x : x; }
static inline double __fabs(double x) { return __builtin_fabs(x); }
static inline double __frsqrte(double x) { return 1.0 / __builtin_sqrt(x); }
#define __memcpy __builtin_memcpy

#endif
#endif
