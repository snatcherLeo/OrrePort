/*
 * pc_fibre: minimal cooperative fibres (each has its own stack; switching is explicit, never preemptive).
 *
 * Used by gs_thread_host.c to run the engine's GSthreads (platform-layer plan, decision 6). One OS thread only:
 * every fibre runs on the thread that called pc_fibre_init_main().
 *
 * Fibre stacks come from low memory (pc_lowmem.h), so pointers to stack locals survive a round trip through 32 bits.
 *
 * Platform files: pc_fibre_win32.c + pc_fibre_switch_win64.S (our own stack switch; Windows fibers can't place
 * their stack), pc_fibre_posix.c (ucontext).
 */
#ifndef PC_FIBRE_H
#define PC_FIBRE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PcFibre PcFibre;
typedef void (*PcFibreFunc)(void* arg);

/* Turn the calling OS thread into a fibre (once; later calls return the same fibre). */
PcFibre* pc_fibre_init_main(void);

/* Create a suspended fibre that will run fn(arg) the first time it is switched to. fn must never return (a fibre
 * that is finished switches away for good and is destroyed by its owner). Returns NULL on failure. */
PcFibre* pc_fibre_create(size_t stack_size, PcFibreFunc fn, void* arg);

/* Switch to `to`; returns when some other fibre switches back to the caller. The first switch turns the calling
 * thread into the main fibre if pc_fibre_init_main() was not called yet. */
void pc_fibre_switch(PcFibre* to);

/* The running fibre (NULL before pc_fibre_init_main). */
PcFibre* pc_fibre_current(void);

/* Free a fibre that is not running. */
void pc_fibre_destroy(PcFibre* f);

/* Top (initial stack pointer) of a fibre's stack; NULL for the main fibre. Stacks come from pc_lowmem, below 4 GB. */
void* pc_fibre_stack_top(PcFibre* f);

#ifdef __cplusplus
}
#endif

#endif /* PC_FIBRE_H */
