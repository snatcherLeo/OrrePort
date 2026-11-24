/*
 * pc_cpu.h: the stored ("shadow") PowerPC register state the PC platform layer keeps (pc_cpu.c).
 */
#ifndef PC_CPU_H
#define PC_CPU_H

#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The paired-single quantisation registers GQR0-GQR7. On the GameCube, psq_l / psq_st convert between floats and
 * u8 / u16 / s8 / s16 values using these (bits 16-18 load type, 24-29 load scale; bits 0-2 store type, 8-13 store
 * scale; types 4 = u8, 5 = u16, 6 = s8, 7 = s16). PC code that reads quantised data looks the format up here
 * instead of trusting a hard-coded one. GSmathInit sets GQR2-GQR5. */
extern u32 pc_gqr[8];

#ifdef __cplusplus
}
#endif

#endif /* PC_CPU_H */
