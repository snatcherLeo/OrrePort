/*
 * pc_cpu.c: the SDK's PowerPC register accessors (PPCArch) for OrrePort (docs/port/asm-routines.md, "CPU primitives",
 * approved as platform layer on 2026-10-14).
 *
 * On the GameCube these are one-instruction assembly routines that read or write a special CPU register (MSR, HID0,
 * L2CR, the decrementer, performance counters, the write-gather pipe address WPAR, the DMA registers, FPSCR).
 * Aurora's headers declare them (dolphin/base/PPCArch.h) but leave them undefined. A PC CPU has none of these
 * registers, so each one is a stored value ("shadow"): a write is kept and the next read returns it. Nothing in the
 * PC hardware changes. That is enough for the game and the libraries, which only use them for cache / gather-pipe
 * setup and timing (callers in src/game, src/hsd, src/musyx: PPCSync in MusyX's hardware layer; the rest are
 * SDK-internal).
 *
 * Exceptions with real effects:
 *   PPCSync, PPCEieio  memory barriers on the GameCube: a full memory fence here.
 *   PPCHalt            stops the CPU for good (used after fatal errors): print and abort.
 *   PPCMfpvr           the processor version: Gekko's (0x00083214), so version checks behave as on the GameCube.
 *   PPCMffpscr / PPCMtfpscr, PPCSetFpIEEEMode / PPCSetFpNonIEEEMode
 *                      shadow only: the host's rounding and denormal modes are NOT changed (the game is not known to
 *                      change them; check if a float result ever differs from Dolphin).
 */
#include <dolphin/types.h>
#include <dolphin/base/PPCArch.h>

#include "pc_cpu.h"

#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

static struct {
    u32 msr, hid0, hid1, hid2, l2cr, dec, wpar, dmau, dmal, sia, fpscr;
    u32 mmcr0, mmcr1, pmc[4];
} s_spr = {
    .msr = 0x00009032, /* EE | FP | IR | DR | RI: interrupts and translation on, as after OSInit */
    .hid2 = 0xA0000000, /* WPE | PSE: write-gather pipe and paired singles enabled */
};

u32 pc_gqr[8]; /* GQR0 is 0 (plain floats) on the GameCube as well */

u32 PPCMfmsr(void) { return s_spr.msr; }
void PPCMtmsr(u32 newMSR) { s_spr.msr = newMSR; }
u32 PPCOrMsr(u32 value) { u32 old = s_spr.msr; s_spr.msr |= value; return old; }
u32 PPCAndMsr(u32 value) { u32 old = s_spr.msr; s_spr.msr &= value; return old; }
u32 PPCAndCMsr(u32 value) { u32 old = s_spr.msr; s_spr.msr &= ~value; return old; }

u32 PPCMfhid0(void) { return s_spr.hid0; }
void PPCMthid0(u32 newHID0) { s_spr.hid0 = newHID0; }
u32 PPCMfhid1(void) { return s_spr.hid1; }
u32 PPCMfhid2(void) { return s_spr.hid2; }
void PPCMthid2(u32 newhid2) { s_spr.hid2 = newhid2; }
u32 PPCMfl2cr(void) { return s_spr.l2cr; }
void PPCMtl2cr(u32 newL2cr) { s_spr.l2cr = newL2cr; }
u32 PPCMfdec(void) { return s_spr.dec; }
void PPCMtdec(u32 newDec) { s_spr.dec = newDec; }
u32 PPCMfwpar(void) { return s_spr.wpar; }
void PPCMtwpar(u32 newwpar) { s_spr.wpar = newwpar; }
u32 PPCMfdmaU(void) { return s_spr.dmau; }
void PPCMtdmaU(u32 newdmau) { s_spr.dmau = newdmau; }
u32 PPCMfdmaL(void) { return s_spr.dmal; }
void PPCMtdmaL(u32 newdmal) { s_spr.dmal = newdmal; }
u32 PPCMfsia(void) { return s_spr.sia; }
void PPCMtsia(u32 newSia) { s_spr.sia = newSia; }
u32 PPCMfpvr(void) { return 0x00083214; }

u32 PPCMffpscr(void) { return s_spr.fpscr; }
void PPCMtfpscr(u32 newFPSCR) { s_spr.fpscr = newFPSCR; }
void PPCSetFpIEEEMode(void) { s_spr.fpscr &= ~0x4u; }   /* NI bit off */
void PPCSetFpNonIEEEMode(void) { s_spr.fpscr |= 0x4u; } /* NI bit on */

void PPCEnableSpeculation(void) { s_spr.hid0 &= ~0x200u; }
void PPCDisableSpeculation(void) { s_spr.hid0 |= 0x200u; }

u32 PPCMfmmcr0(void) { return s_spr.mmcr0; }
void PPCMtmmcr0(u32 newMmcr0) { s_spr.mmcr0 = newMmcr0; }
u32 PPCMfmmcr1(void) { return s_spr.mmcr1; }
void PPCMtmmcr1(u32 newMmcr1) { s_spr.mmcr1 = newMmcr1; }
u32 PPCMfpmc1(void) { return s_spr.pmc[0]; }
u32 PPCMfpmc2(void) { return s_spr.pmc[1]; }
u32 PPCMfpmc3(void) { return s_spr.pmc[2]; }
u32 PPCMfpmc4(void) { return s_spr.pmc[3]; }
void PPCMtpmc1(u32 v) { s_spr.pmc[0] = v; }
void PPCMtpmc2(u32 v) { s_spr.pmc[1] = v; }
void PPCMtpmc3(u32 v) { s_spr.pmc[2] = v; }
void PPCMtpmc4(u32 v) { s_spr.pmc[3] = v; }

void PPCSync(void) { atomic_thread_fence(memory_order_seq_cst); }
void PPCEieio(void) { atomic_thread_fence(memory_order_seq_cst); }

/* frsqrte: the CPU's reciprocal square-root estimate, which Metrowerks exposes as a built-in. The game's sqrt helpers
 * (crt/math_ppc.h, MusyX's snd_math.c) refine it with three Newton steps, so an exact 1/sqrt start gives practically
 * the same final value (likely); bit-exactness against the Gekko's estimate table is not verified. */
f64 __frsqrte(f64 x) { return 1.0 / __builtin_sqrt(x); }

void PPCHalt(void) {
    fprintf(stderr, "[pc_cpu] PPCHalt: the game stopped the CPU (fatal error)\n");
    fflush(stderr);
    abort();
}
