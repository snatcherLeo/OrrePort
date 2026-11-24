/**
 * @file input_exact_800F76E4.c
 * @brief Exact input resource registration helper at 0x800F76E4.
 */
#include "dolphin/types.h"

#ifdef TARGET_PC
#include "game/gs_vm.h"
#include <stdint.h>
#include <stdlib.h>

/* PC: link and relocate a loaded script (the GameCube body below does the same through raw offsets). The file is
 * big-endian disc data, so the header is read through its big-endian view (game/gs_vm.h), relocation adds the base in
 * big-endian (DISC_WORD_ADD), and one PC-only step follows: the globals are swapped to native order, because the VM
 * and the natives read and write them natively. Layout (checked on all 164 room scripts, 2026-10-15): header,
 * function offsets, relocation table, globals, then the code at the first non-zero function offset. */
void cms_RegisterScript(u8* arg) {
    GSVMScript* script = (GSVMScript*)arg;
    GSVMPool* pool = lbl_80478B00;
    u32 i;
    u32 code;
    u32 n;
    u32* tbl;
    u32* g;

    { /* PC test log: where each script lives (for ORRE_WATCH on its code bytes, 2026-10-21) */
        extern void OSReport(const char*, ...);
        OSReport("[script] register 0x%04X at %p (relocated %u)\n", (unsigned)script->id, (void*)arg,
                 (unsigned)script->relocated);
        {
            /* ORRE_WATCH_SCRIPT=<id hex>,<offset hex>[,<offset hex>...]: a write-watch (pc_crash_win32.c) on those
             * code bytes of that script, armed when it registers (Phenac's sack scene: func37's code is overwritten) */
            extern void pc_crash_watch_add(unsigned long long addr, unsigned len);
            /* "<id>/<function count>" picks one room's script (every room script has id 0x0100; M1_out has 48) */
            const char* v = getenv("ORRE_WATCH_SCRIPT");
            char* e = NULL;
            u32 want_id = v != NULL && *v != '\0' ? (u32)strtoul(v, &e, 16) : 0xFFFFFFFFu;
            u32 want_funcs = e != NULL && *e == '/' ? (u32)strtoul(e + 1, &e, 10) : 0;
            if (want_id == (u32)script->id && (want_funcs == 0 || want_funcs == (u32)script->funcCount)) {
                while (*e == ',') {
                    u32 off = (u32)strtoul(e + 1, &e, 16);
                    pc_crash_watch_add((unsigned long long)(uintptr_t)(arg + off), 4);
                }
            }
        }
    }
    DP_SET(script->next, NULL);
    if (pool->scripts != NULL) {
        DP_SET(script->next, pool->scripts);
    }
    pool->scripts = script;
    if (script->relocated != 0) {
        return;
    }
    tbl = (u32*)(arg + script->relocOffset);
    for (i = 0; i < script->relocCount; i++) {
        DISC_WORD_ADD((u32*)(arg + GS_VM_RD32(&tbl[i])), PC_PTR32(arg));
    }
    code = 0xFFFFFFFFu;
    for (i = 0; i < script->funcCount; i++) {
        if (script->funcOffsets[i] != 0 && script->funcOffsets[i] < code) {
            code = script->funcOffsets[i];
        }
    }
    if (code != 0xFFFFFFFFu && code > (u32)script->globalsOffset) {
        g = (u32*)(arg + script->globalsOffset);
        for (n = (code - (u32)script->globalsOffset) / 4; n > 0; n--, g++) {
            *g = __builtin_bswap32(*g);
        }
    }
    script->relocated = 1;
}
#else
extern u8* lbl_80478B00;

void cms_RegisterScript(u8* arg) {
    u8* head;
    u32 off;
    u32* tbl;
    s32 i;

    *(u32*)(arg + 0x14) = 0;
    head = (u8*)PC_PTR(*(u32*)(lbl_80478B00 + 0x8));
    if (head == NULL) {
        *(u32*)(lbl_80478B00 + 0x8) = PC_PTR32(arg);
    } else {
        *(u32*)(lbl_80478B00 + 0x8) = PC_PTR32(arg);
        *(u32*)(arg + 0x14) = PC_PTR32(head);
    }
    if (*(u8*)(arg + 0xa) != 0) {
        return;
    }
    tbl = (u32*)(arg + *(u32*)(arg + 0xc));
    i = 0;
    while (i < (s32)*(u16*)(arg + 0x6)) {
        off = *tbl;
        tbl += 1;
        i += 1;
        *(u32*)(arg + off) += PC_PTR32(arg);
    }
    *(u8*)(arg + 0xa) = 1;
}
#endif
