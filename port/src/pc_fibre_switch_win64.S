/*
 * pc_fibre_switch_win64.S: the fibre context switch for Windows x64 (GNU assembler, MinGW). See pc_fibre_win32.c.
 *
 * Windows fibers (CreateFiber) always place the stack themselves, usually above 4 GB; snatcherLeo's 32-bit pointer
 * decision needs GS thread stacks below 4 GB (the game stores addresses of stack locals in u32 slots), so the port
 * switches stacks itself. What a switch must keep, by the Windows x64 calling convention: the non-volatile
 * registers RBX, RBP, RDI, RSI, R12-R15, XMM6-XMM15, the MXCSR and x87 control words, and the stack bounds Windows
 * keeps in the thread's TEB (StackBase gs:[0x08], StackLimit gs:[0x10], DeallocationStack gs:[0x1478], plus
 * FiberData gs:[0x20]); exception dispatch and stack probes check RSP against those. Boost.Context's
 * jump_x86_64_ms_pe_gas does the same; this is our own minimal version.
 *
 * Saved frame (from the saved stack pointer upwards, 0x110 bytes, 16-byte aligned):
 *   0x00-0x9F XMM6-XMM15   0xA0 MXCSR   0xA4 x87 control word   0xA8 StackBase   0xB0 StackLimit
 *   0xB8 DeallocationStack   0xC0 FiberData   0xC8 R15  0xD0 R14  0xD8 R13  0xE0 R12  0xE8 RSI  0xF0 RDI  0xF8 RBX
 *   0x100 RBP   0x108 return address
 * pc_fibre_win32.c builds this same frame for a new fibre, with pc_fibre_trampoline as the return address.
 */
    .intel_syntax noprefix
    .text

/* void pc_fibre_switch_ctx(void** save_sp (RCX), void* load_sp (RDX)) */
    .globl pc_fibre_switch_ctx
    .def pc_fibre_switch_ctx; .scl 2; .type 32; .endef
    .p2align 4
pc_fibre_switch_ctx:
    push rbp
    push rbx
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 0xC8
    movaps [rsp+0x00], xmm6
    movaps [rsp+0x10], xmm7
    movaps [rsp+0x20], xmm8
    movaps [rsp+0x30], xmm9
    movaps [rsp+0x40], xmm10
    movaps [rsp+0x50], xmm11
    movaps [rsp+0x60], xmm12
    movaps [rsp+0x70], xmm13
    movaps [rsp+0x80], xmm14
    movaps [rsp+0x90], xmm15
    stmxcsr [rsp+0xA0]
    fnstcw [rsp+0xA4]
    mov rax, qword ptr gs:[0x08]
    mov [rsp+0xA8], rax
    mov rax, qword ptr gs:[0x10]
    mov [rsp+0xB0], rax
    mov rax, qword ptr gs:[0x1478]
    mov [rsp+0xB8], rax
    mov rax, qword ptr gs:[0x20]
    mov [rsp+0xC0], rax

    mov [rcx], rsp
    mov rsp, rdx

    mov rax, [rsp+0xC0]
    mov qword ptr gs:[0x20], rax
    mov rax, [rsp+0xB8]
    mov qword ptr gs:[0x1478], rax
    mov rax, [rsp+0xB0]
    mov qword ptr gs:[0x10], rax
    mov rax, [rsp+0xA8]
    mov qword ptr gs:[0x08], rax
    fldcw [rsp+0xA4]
    ldmxcsr [rsp+0xA0]
    movaps xmm6, [rsp+0x00]
    movaps xmm7, [rsp+0x10]
    movaps xmm8, [rsp+0x20]
    movaps xmm9, [rsp+0x30]
    movaps xmm10, [rsp+0x40]
    movaps xmm11, [rsp+0x50]
    movaps xmm12, [rsp+0x60]
    movaps xmm13, [rsp+0x70]
    movaps xmm14, [rsp+0x80]
    movaps xmm15, [rsp+0x90]
    add rsp, 0xC8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    pop rbp
    ret

/* First code a new fibre runs: RBX holds its PcFibre; call pc_fibre_entry(fibre), which never returns. */
    .globl pc_fibre_trampoline
    .def pc_fibre_trampoline; .scl 2; .type 32; .endef
    .p2align 4
pc_fibre_trampoline:
    mov rcx, rbx
    and rsp, -16
    sub rsp, 32
    call pc_fibre_entry
    ud2
