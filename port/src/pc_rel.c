/*
 * pc_rel.c: REL modules on the PC: OSLink / OSUnlink (docs/port/rel-loading.md).
 *
 * The game keeps data in REL modules (common_rel: 1.3 MB of tables; one module per room and some menus) and links
 * them at run time (gs_range_8017F3F8_middle.c: load, OSLink, call _prolog; at release _epilog, OSUnlink). The PC
 * cannot run their PowerPC code, but there is almost none: every _prolog / _epilog (all 25 RELs the build knows)
 * is straight-line code that builds addresses with lis / addi and stores them into main.dol globals ("publishes"
 * the module's tables), or stores 0 (the epilog). So the PC:
 *   1. applies the relocations exactly as the SDK does: the module's data stays big-endian (the platform plan's
 *      data model), a pointer into the module becomes its address in MEM1 (MEM1 sits at 0x80000000 on the PC too),
 *      and a pointer into main.dol becomes the PC address of that symbol (the program image is linked below 4 GB,
 *      so it fits the 32-bit slot) through a table the port supplies (pc_rel_set_resolver);
 *   2. runs the relocated _prolog inside OSLink with an interpreter for exactly those instructions (stwu, mr, lis,
 *      addi, li, stw, lwz, blr; anything else stops with a message). A store into a main.dol global writes the
 *      value as a native 32-bit word at the variable's PC address: a pointer variable below 4 GB reads correctly as
 *      a native pointer;
 *   3. clears the header's prolog / epilog fields so the game's own "call _prolog" finds nothing to call, and runs
 *      the _epilog in OSUnlink (the game calls _epilog right before OSUnlink, so the order is the same).
 *
 * The REL header and relocation tables are read by byte offset (big-endian, the GameCube layout), independent of
 * how a C struct for them is laid out on the PC.
 */
#include <dolphin/types.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef u32 (*PcRelResolve)(u32 gcAddress); /* main.dol address -> PC address (< 4 GB), 0 if unknown */

static PcRelResolve s_resolve;
static void (*s_store_hook)(u32 pcAddress, u32 value); /* tests: observe stores instead of writing */

void pc_rel_set_resolver(PcRelResolve resolve) { s_resolve = resolve; }

/* Modules turned into C ahead of time (the project's pcreldata.py: pc_rel_link / pc_rel_unlink, 0 = not one of them) are
 * linked by those; every other module is linked here from the bytes the game loaded. */
static int (*s_module_link)(u32 module);
static int (*s_module_unlink)(u32 module);
static u32 s_prebuilt[16]; /* ids currently linked by the prebuilt linker (0 = free slot) */

void pc_rel_set_module_linker(int (*link)(u32 module), int (*unlink)(u32 module)) {
    s_module_link = link;
    s_module_unlink = unlink;
}
void pc_rel_set_store_hook(void (*hook)(u32 pcAddress, u32 value)) { s_store_hook = hook; }

#define BE32(p) ((u32)(p)[0] << 24 | (u32)(p)[1] << 16 | (u32)(p)[2] << 8 | (u32)(p)[3])
#define BE16(p) ((u16)((p)[0] << 8 | (p)[1]))

static void put_be32(u8* p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static void put_be16(u8* p, u16 v) {
    p[0] = (u8)(v >> 8);
    p[1] = (u8)v;
}

/* REL header fields (GameCube layout) */
enum {
    REL_ID = 0x00,
    REL_NUM_SECTIONS = 0x0C,
    REL_SECTION_INFO = 0x10,
    REL_VERSION = 0x1C,
    REL_BSS_SIZE = 0x20,
    REL_IMP_OFFSET = 0x28,
    REL_IMP_SIZE = 0x2C,
    REL_PROLOG_SECTION = 0x30,
    REL_EPILOG_SECTION = 0x31,
    REL_PROLOG = 0x34,
    REL_EPILOG = 0x38,
};

enum { R_NONE = 0, R_ADDR32 = 1, R_ADDR16 = 3, R_ADDR16_LO = 4, R_ADDR16_HI = 5, R_ADDR16_HA = 6, R_REL24 = 10,
       R_NOP = 201, R_SECTION = 202, R_END = 203 };

#define PC_REL_MAX_MODULES 16

typedef struct PcRelModule {
    u8* base;
    u32 id;
    u32 numSections;
    u32 sectionAddr[32]; /* PC (= MEM1) address of each section, 0 if none */
    u32 epilog;          /* address of the relocated _epilog code, 0 if none */
    u32 lo, hi;          /* the module's own memory (sections and bss): stores there are refused */
} PcRelModule;

static PcRelModule s_modules[PC_REL_MAX_MODULES];

static u32 addr_of(const void* p) { return (u32)(uintptr_t)p; }

static int target_address(PcRelModule* m, u32 module, u32 section, u32 addend, u32* out) {
    if (module == m->id) {
        if (section >= m->numSections || m->sectionAddr[section] == 0) {
            return 0;
        }
        *out = m->sectionAddr[section] + addend;
        return 1;
    }
    if (module == 0) { /* main.dol: the addend is the GameCube address */
        u32 pc = s_resolve != NULL ? s_resolve(addend) : 0;
        if (pc == 0) {
            fprintf(stderr, "[pc_rel] module %u: no PC address for main.dol 0x%08X\n", m->id, addend);
            return 0;
        }
        *out = pc;
        return 1;
    }
    fprintf(stderr, "[pc_rel] module %u: import from module %u is not supported yet\n", m->id, module);
    return 0;
}

static int relocate(PcRelModule* m) {
    u8* b = m->base;
    u32 impOffset = BE32(b + REL_IMP_OFFSET), impSize = BE32(b + REL_IMP_SIZE), i;
    for (i = 0; i < impSize / 8; i++) {
        u32 module = BE32(b + impOffset + i * 8);
        const u8* r = b + BE32(b + impOffset + i * 8 + 4);
        u8* where = NULL;
        for (;; r += 8) {
            u16 offset = BE16(r);
            u8 type = r[2], section = r[3];
            u32 addend = BE32(r + 4), value;
            if (type == R_END) {
                break;
            }
            if (type == R_SECTION) {
                where = (u8*)(uintptr_t)m->sectionAddr[section];
                continue;
            }
            where += offset;
            if (type == R_NOP || type == R_NONE) {
                continue;
            }
            if (!target_address(m, module, section, addend, &value)) {
                return 0;
            }
            switch (type) {
            case R_ADDR32:
                put_be32(where, value);
                break;
            case R_ADDR16:
            case R_ADDR16_LO:
                put_be16(where, (u16)value);
                break;
            case R_ADDR16_HI:
                put_be16(where, (u16)(value >> 16));
                break;
            case R_ADDR16_HA:
                put_be16(where, (u16)((value + 0x8000) >> 16));
                break;
            case R_REL24: /* a branch in PowerPC code: the PC never runs it (the interpreter refuses branches) */
                break;
            default:
                fprintf(stderr, "[pc_rel] module %u: relocation type %u is not supported\n", m->id, type);
                return 0;
            }
        }
    }
    return 1;
}

/* Runs a relocated _prolog / _epilog: straight-line code of stwu / mr / lis / addi / li / stw / lwz / blr. Memory
 * accesses through r1 (or a register copied from it) go to a local stack; every other store is a store into a
 * main.dol global at its PC address. */
static int run_publisher(PcRelModule* m, u32 code, const char* what) {
    u32 reg[32] = {0};
    u8 stack[512];
    u8 stack_is_sp[512 / 4] = {0}; /* which stack words hold a stack address (the frame's back chain) */
    int on_stack[32] = {0};
    int n;
    u32 sp0 = sizeof(stack) - 64;
    reg[1] = sp0;
    on_stack[1] = 1;
    for (n = 0; n < 4096; n++) {
        const u8* p = (const u8*)(uintptr_t)code + 4 * n;
        u32 w = BE32(p), op = w >> 26, d = (w >> 21) & 31, a = (w >> 16) & 31;
        s32 simm = (s16)(w & 0xFFFF);
        if (getenv("PC_REL_TRACE") != NULL) {
            fprintf(stderr, "  +%03X %08X r1=%X r9=%X r11=%X r31=%X\n", 4 * n, w, reg[1], reg[9], reg[11], reg[31]);
        }
        if (w == 0x4E800020) { /* blr */
            return 1;
        }
        switch (op) {
        case 15: /* lis / addis rD, rA, imm */
            reg[d] = (a ? reg[a] : 0) + ((u32)(w & 0xFFFF) << 16);
            on_stack[d] = 0;
            break;
        case 14: /* addi / li */
            reg[d] = (a ? reg[a] : 0) + (u32)simm;
            on_stack[d] = a ? on_stack[a] : 0;
            break;
        case 37: /* stwu rS, d(rA): the stack frame */
        case 36: /* stw rS, d(rA) */
            if (on_stack[a]) {
                u32 at = reg[a] + (u32)simm;
                if ((s64)reg[a] + simm < 0 || (s64)reg[a] + simm + 4 > (s64)sizeof(stack)) {
                    goto unsupported;
                }
                memcpy(stack + at, &reg[d], 4);
                stack_is_sp[at / 4] = (u8)on_stack[d];
            } else {
                u32 at = reg[a] + (u32)simm;
                if (at >= m->lo && at < m->hi) { /* only main.dol globals are expected: module data is big-endian */
                    fprintf(stderr, "[pc_rel] module %u %s: store into the module itself (0x%08X)\n", m->id, what, at);
                    return 0;
                }
                if (s_store_hook != NULL) {
                    s_store_hook(at, reg[d]);
                } else {
                    memcpy((void*)(uintptr_t)at, &reg[d], 4); /* native word: the variable is a PC global */
                }
            }
            if (op == 37) {
                reg[a] += (u32)simm;
            }
            break;
        case 32: /* lwz rD, d(rA): only from the stack (restoring saved registers) */
            if (!on_stack[a] || (s64)reg[a] + simm < 0 || (s64)reg[a] + simm + 4 > (s64)sizeof(stack)) {
                goto unsupported;
            }
            {
                u32 at = reg[a] + (u32)simm; /* 32-bit sum first: a negative offset must not become +4 GB */
                memcpy(&reg[d], stack + at, 4);
                on_stack[d] = stack_is_sp[at / 4];
            }
            break;
        case 31:
            if (((w >> 1) & 0x3FF) == 444) { /* or rA, rS, rB (mr when rS == rB) */
                u32 s = d, b = (w >> 11) & 31;
                reg[a] = reg[s] | reg[b];
                on_stack[a] = s == b ? on_stack[s] : 0;
                break;
            }
            goto unsupported;
        default:
            goto unsupported;
        }
    }
unsupported:
    fprintf(stderr, "[pc_rel] module %u %s: instruction 0x%08X at +0x%X is outside the publishing pattern\n", m->id,
            what, BE32((const u8*)(uintptr_t)code + 4 * n), 4 * n);
    return 0;
}

/* ORRE_RELPTR_LOG (2026-10-26; observes only; the scan is pc_rel_scan_main_pointers in the generated
 * pc_rel_modules.c): the initialized data sections of the modules linked here at run time, which stay big-endian.
 * `index` counts over every linked module's data sections; returns 0 when there are no more. */
int pc_rel_linked_data_range(int index, u32* module, u32* section, u32* lo, u32* hi) {
    int m, s, n = 0;
    for (m = 0; m < PC_REL_MAX_MODULES; m++) {
        const u8* b = s_modules[m].base;
        u32 info;
        if (b == NULL) {
            continue;
        }
        info = BE32(b + REL_SECTION_INFO);
        for (s = 0; s < (int)s_modules[m].numSections; s++) {
            const u32 word = BE32(b + info + s * 8), off = word & ~1u, size = BE32(b + info + s * 8 + 4);
            if (off == 0 || size == 0 || (word & 1u) != 0) {
                continue; /* the bss section, an empty one, or code */
            }
            if (n++ == index) {
                *module = s_modules[m].id;
                *section = (u32)s;
                *lo = addr_of(b) + off;
                *hi = *lo + size;
                return 1;
            }
        }
    }
    return 0;
}

static PcRelModule* module_for(const void* base) {
    int i;
    for (i = 0; i < PC_REL_MAX_MODULES; i++) {
        if (s_modules[i].base == base) {
            return &s_modules[i];
        }
    }
    return NULL;
}

BOOL OSLink(void* module, void* bss) {
    u8* b = (u8*)module;
    PcRelModule* m;
    u32 i, sectionInfo;
    /* ORRE_REL_PREBUILT=0 (2026-11-01, the reference playthrough Part 1 item 2c step 4; for B to review): every module is linked here
     * from the bytes the game loaded, the pre-built ones too (pc_rel_test links all 171 modules on the disc this way),
     * so the exe no longer needs their data. *Updated 2026-11-01:* all 36 replay sittings agreed, so pcreldata.py no
     * longer generates the modules' data by default and s_module_link knows no module: every module takes the path
     * below. The pre-built linker runs only in a build generated with ORRE_REL_PREBUILT=1 (comparisons); this switch
     * then turns it off again at run time */
    static int prebuilt = -1;
    if (prebuilt < 0) {
        const char* env = getenv("ORRE_REL_PREBUILT");
        prebuilt = !(env != NULL && strcmp(env, "0") == 0);
        if (!prebuilt) {
            fprintf(stderr, "[pc_rel] ORRE_REL_PREBUILT=0: every module linked from its loaded bytes\n");
        }
    }
    if (prebuilt && s_module_link != NULL && b != NULL && s_module_link(BE32(b + REL_ID))) {
        for (i = 0; i < 16 && s_prebuilt[i] != 0; i++) {
        }
        if (i < 16) {
            s_prebuilt[i] = BE32(b + REL_ID);
        }
        put_be32(b + REL_PROLOG, 0); /* its prolog stores were done by the prebuilt linker */
        put_be32(b + REL_EPILOG, 0);
        return TRUE;
    }
    if (addr_of(b) == 0 || (uintptr_t)b >> 32 != 0) {
        fprintf(stderr, "[pc_rel] OSLink: module %p is not below 4 GB\n", module);
        return FALSE;
    }
    m = module_for(b);
    if (m == NULL) {
        m = module_for(NULL);
    }
    if (m == NULL) {
        fprintf(stderr, "[pc_rel] more than %d modules linked at once\n", PC_REL_MAX_MODULES);
        return FALSE;
    }
    memset(m, 0, sizeof(*m));
    m->base = b;
    m->id = BE32(b + REL_ID);
    m->numSections = BE32(b + REL_NUM_SECTIONS);
    if (m->numSections > 32) {
        fprintf(stderr, "[pc_rel] module %u: %u sections\n", m->id, m->numSections);
        m->base = NULL;
        return FALSE;
    }
    sectionInfo = BE32(b + REL_SECTION_INFO);
    m->lo = addr_of(b);
    m->hi = addr_of(b);
    for (i = 0; i < m->numSections; i++) {
        u32 off = BE32(b + sectionInfo + i * 8) & ~1u, size = BE32(b + sectionInfo + i * 8 + 4);
        if (off != 0) {
            m->sectionAddr[i] = addr_of(b) + off;
            if (m->sectionAddr[i] + size > m->hi) {
                m->hi = m->sectionAddr[i] + size;
            }
        } else if (size != 0) { /* the bss section */
            m->sectionAddr[i] = addr_of(bss);
            if (bss != NULL) {
                memset(bss, 0, size);
                if (addr_of(bss) + size > m->hi && addr_of(bss) >= m->lo) {
                    m->hi = addr_of(bss) + size; /* bss right after the module, as the game allocates it */
                }
            }
        }
    }
    if (!relocate(m)) {
        m->base = NULL;
        return FALSE;
    }
    if (b[REL_PROLOG_SECTION] != 0) { /* present when it has a section (offset 0 is valid) */
        u32 code = m->sectionAddr[b[REL_PROLOG_SECTION]] + BE32(b + REL_PROLOG);
        if (!run_publisher(m, code, "_prolog")) {
            m->base = NULL;
            return FALSE;
        }
    }
    if (b[REL_EPILOG_SECTION] != 0) {
        m->epilog = m->sectionAddr[b[REL_EPILOG_SECTION]] + BE32(b + REL_EPILOG);
    }
    /* the game calls _prolog / _epilog itself when the header names them (after OSLink the SDK turns the fields into
     * addresses, so the game tests them for non-zero): done here instead, so clear them */
    put_be32(b + REL_PROLOG, 0);
    put_be32(b + REL_EPILOG, 0);
    return TRUE;
}

BOOL OSUnlink(void* module) {
    PcRelModule* m = module_for(module);
    BOOL ok = TRUE;
    u32 i;
    if (m == NULL && module != NULL && s_module_unlink != NULL) {
        u32 id = BE32((u8*)module + REL_ID);
        for (i = 0; i < 16; i++) {
            if (s_prebuilt[i] == id) {
                s_prebuilt[i] = 0;
                return s_module_unlink(id) ? TRUE : FALSE;
            }
        }
    }
    if (m == NULL) {
        return FALSE;
    }
    if (m->epilog != 0) {
        ok = run_publisher(m, m->epilog, "_epilog");
    }
    m->base = NULL;
    return ok;
}
