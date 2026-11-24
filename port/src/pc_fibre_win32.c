/*
 * pc_fibre on Windows x64: our own stack switch (pc_fibre_switch_win64.S) on stacks from low memory
 * (pc_lowmem.h), so GS thread stacks sit below 4 GB (snatcherLeo's 32-bit pointer decision, docs/port/low-memory.md).
 * Windows fibers (CreateFiber) were used before, but they always place the stack themselves. See pc_fibre.h.
 */
#include "pc_fibre.h"
#include "pc_lowmem.h"

#include <intrin.h>
#include <xmmintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct PcFibre {
    void* sp;         /* saved stack pointer while not running */
    void* stack;      /* pc_lowmem stack block (NULL for the main fibre) */
    uintptr_t top;    /* initial stack pointer = TEB StackBase while running */
    PcFibreFunc fn;
    void* arg;
    int is_main;
};

extern void pc_fibre_switch_ctx(void** save_sp, void* load_sp);
extern void pc_fibre_trampoline(void);
void pc_fibre_entry(PcFibre* self);

static PcFibre s_main;
static PcFibre* s_current;

void pc_fibre_entry(PcFibre* self) {
    self->fn(self->arg);
    /* pc_fibre_create's contract: fn never returns */
    fprintf(stderr, "[pc_fibre] a fibre function returned; this is a bug\n");
    abort();
}

PcFibre* pc_fibre_init_main(void) {
    if (!s_main.is_main) {
        s_main.is_main = 1;
        s_current = &s_main;
    }
    return &s_main;
}

PcFibre* pc_fibre_create(size_t stack_size, PcFibreFunc fn, void* arg) {
    PcFibre* f = (PcFibre*)calloc(1, sizeof(PcFibre));
    uint64_t* frame;
    uint32_t mxcsr;
    uint16_t fpucw;

    if (f == NULL) {
        return NULL;
    }
    f->stack = pc_lowmem_alloc_stack(stack_size, &f->top);
    if (f->stack == NULL) {
        free(f);
        return NULL;
    }
    f->fn = fn;
    f->arg = arg;

    /* The frame pc_fibre_switch_ctx restores (layout in the .S file); 16 spare bytes above the return address */
    frame = (uint64_t*)(f->top - 0x120);
    memset(frame, 0, 0x120);
    mxcsr = _mm_getcsr();
    __asm__ volatile("fnstcw %0" : "=m"(fpucw));
    memcpy((uint8_t*)frame + 0xA0, &mxcsr, 4);
    memcpy((uint8_t*)frame + 0xA4, &fpucw, 2);
    frame[0xA8 / 8] = f->top;                                                    /* StackBase */
    frame[0xB0 / 8] = (uint64_t)((f->top - stack_size) & ~(uintptr_t)4095);   /* StackLimit */
    frame[0xB8 / 8] = frame[0xB0 / 8];                                           /* DeallocationStack */
    frame[0xC0 / 8] = __readgsqword(0x20);                                       /* FiberData: unchanged */
    frame[0xF8 / 8] = (uint64_t)(uintptr_t)f;                                    /* RBX: the fibre */
    frame[0x108 / 8] = (uint64_t)(uintptr_t)&pc_fibre_trampoline;                /* return address */
    f->sp = frame;
    return f;
}

void pc_fibre_switch(PcFibre* to) {
    PcFibre* from = s_current != NULL ? s_current : pc_fibre_init_main();
    if (to == from) {
        return;
    }
    s_current = to;
    pc_fibre_switch_ctx(&from->sp, to->sp);
}

PcFibre* pc_fibre_current(void) { return s_current; }

void pc_fibre_destroy(PcFibre* f) {
    if (f == NULL || f->is_main) {
        return;
    }
    if (f == s_current) {
        fprintf(stderr, "[pc_fibre] refusing to destroy the running fibre\n");
        return;
    }
    pc_lowmem_free_stack(f->stack);
    free(f);
}

void* pc_fibre_stack_top(PcFibre* f) { return f != NULL ? (void*)f->top : NULL; }
