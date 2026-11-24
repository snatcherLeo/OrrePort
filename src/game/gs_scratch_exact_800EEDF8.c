/**
 * @file gs_scratch_exact_800EEDF8.c
 * @brief GSscratchInit, 0x800EEDF8 - 0x800EEF48 (the end of the retail
 *        GSscratch translation unit).
 *
 * GSscratchInit clears the 32 allocation records, enables the Gekko locked
 * cache at 0xE0000000, and reserves the first `reservedBlocks` 0x200-byte
 * blocks. When it reserves any, it moves the caller's stack into the top of
 * the reserved area and returns straight to the caller's saved LR. That
 * stack move was written in assembly by the original developers, so the
 * routine is first-party assembly: the evidence, including a compiler
 * probe, is docs/asm_evidence/gs_scratch.md (registered in
 * docs/asm_evidence/registry.json).
 *
 * gs_scratch.c includes this file so the candidate units built from it keep
 * seeing the same definition.
 */

#include "dolphin/types.h"

extern u8 _blockTable[];
extern u8* _scratchPadBase;
extern u32 lbl_8047ABE4;
extern u8 _scratchInvalid;
extern u32 _blockMap;
extern u32 lbl_8047ABD8;

extern void LCEnable(void);

/* First-party assembly; evidence: docs/asm_evidence/gs_scratch.md */
#ifndef TARGET_PC /* GameCube assembly (GSscratchInit); the PC version lives in port/ */
asm BOOL GSscratchInit(register u8 reservedBlocks) {
    nofralloc
    stwu r1, -0x10(r1)
    mflr r0
    stw r0, 0x14(r1)
    stw r31, 0xc(r1)
    mr r31, r1
    stw r30, 0x8(r1)
    mr r30, r3
    mflr r3
    li r7, 0x0
    stw r3, lbl_8047ABE4
    stw r7, _blockMap
    lis r4, _blockTable@ha
    li r0, 0x4
    addi r4, r4, _blockTable@l
    li r6, 0xff
    mtctr r0
L_800EEE38:
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    stbx r6, r4, r7
    addi r7, r7, 0x8
    bdnz L_800EEE38
    bl LCEnable
    clrlslwi r0, r30, 24, 9
    lis r6, 0xe000
    clrlwi. r4, r30, 24
    stw r0, lbl_8047ABD8
    stw r6, _scratchPadBase
    beq L_800EEEE8
    mr r7, r30
    li r6, 0x0
    lis r4, 0x8000
    b L_800EEEAC
L_800EEEA8:
    srwi r4, r4, 1
L_800EEEAC:
    clrlwi. r0, r6, 24
    subi r6, r6, 0x1
    bne L_800EEEA8
    b L_800EEECC
L_800EEEBC:
    lwz r0, _blockMap
    or r0, r0, r4
    srwi r4, r4, 1
    stw r0, _blockMap
L_800EEECC:
    clrlwi. r0, r7, 24
    subi r7, r7, 0x1
    bne L_800EEEBC
    li r0, 0x0
    lis r4, _blockTable@ha
    stbu r0, _blockTable@l(r4)
    stb r30, 0x1(r4)
L_800EEEE8:
    li r4, 0x0
    clrlwi. r0, r30, 24
    stb r4, _scratchInvalid
    bne L_800EEF00
    li r3, 0x1
    b L_800EEF28
L_800EEF00:
    lwz r3, _scratchPadBase
    lwz r5, lbl_8047ABD8
    add r3, r3, r5
    subi r1, r3, 0x8
    li r3, -0x1
    stw r3, 0x0(r1)
    lwz r3, lbl_8047ABE4
    mtlr r3
    blr
    li r3, 0x1
L_800EEF28:
    mr r10, r31
    lwz r31, 0xc(r31)
    lwz r30, 0x8(r10)
    lwz r10, 0x0(r1)
    lwz r0, 0x4(r10)
    mr r1, r10
    mtlr r0
    blr
}
#endif
