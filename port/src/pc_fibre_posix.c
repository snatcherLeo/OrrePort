/*
 * pc_fibre on Linux / macOS (ucontext). See pc_fibre.h.
 *
 * NOT BUILT OR TESTED YET (Windows comes first). ucontext is deprecated on macOS but still works; if it becomes a
 * problem, a small assembly context switch (as in minicoro or boost.context) can replace it behind this interface.
 */
#include "pc_fibre.h"
#include "pc_lowmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>

struct PcFibre {
    ucontext_t ctx;
    void* stack;
    uintptr_t top;
    PcFibreFunc fn;
    void* arg;
    int is_main;
};

static PcFibre s_main;
static PcFibre* s_current;
static PcFibre* s_starting; /* makecontext can only pass int arguments portably, so the new fibre reads this */

static void fibre_start(void) {
    PcFibre* self = s_starting;
    self->fn(self->arg);
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
    if (f == NULL) {
        return NULL;
    }
    f->stack = pc_lowmem_alloc_stack(stack_size, &f->top);
    if (f->stack == NULL || getcontext(&f->ctx) != 0) {
        pc_lowmem_free_stack(f->stack);
        free(f);
        return NULL;
    }
    f->fn = fn;
    f->arg = arg;
    f->ctx.uc_stack.ss_sp = (void*)(f->top - stack_size);
    f->ctx.uc_stack.ss_size = stack_size;
    f->ctx.uc_link = NULL;
    makecontext(&f->ctx, fibre_start, 0);
    return f;
}

void pc_fibre_switch(PcFibre* to) {
    PcFibre* from = s_current != NULL ? s_current : pc_fibre_init_main();
    if (to == from) {
        return;
    }
    s_current = to;
    s_starting = to;
    swapcontext(&from->ctx, &to->ctx);
}

PcFibre* pc_fibre_current(void) { return s_current; }

void pc_fibre_destroy(PcFibre* f) {
    if (f == NULL || f->is_main || f == s_current) {
        return;
    }
    pc_lowmem_free_stack(f->stack);
    free(f);
}

void* pc_fibre_stack_top(PcFibre* f) { return f != NULL ? (void*)f->top : NULL; }
