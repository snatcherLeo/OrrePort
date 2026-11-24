/* pc_osalloc_test: the PC version of OSAlloc's free-list insert (port/src/pc_sdk_gaps.c, fn_8009A92C) keeps cells in
 * address order and merges a freed cell with the free cells right before and after it (2026-10-16). */
#include <stdio.h>
#include <stdint.h>
typedef struct PcOSAllocCell { struct PcOSAllocCell* prev; struct PcOSAllocCell* next; int32_t size; } PcOSAllocCell;
PcOSAllocCell* fn_8009A92C(PcOSAllocCell* head, PcOSAllocCell* cell);
static _Alignas(32) unsigned char arena[4096];
#define C(off) ((PcOSAllocCell*)(arena + (off)))
int main(void) {
    int bad = 0;
    PcOSAllocCell* head = NULL;
    C(0)->size = 256; C(512)->size = 256; C(1024)->size = 256; C(256)->size = 256;
    head = fn_8009A92C(head, C(512));           /* one cell */
    head = fn_8009A92C(head, C(0));             /* before it, not touching */
    head = fn_8009A92C(head, C(1024));          /* after it, not touching */
    bad |= head != C(0) || C(0)->next != C(512) || C(512)->next != C(1024) || C(1024)->prev != C(512);
    head = fn_8009A92C(head, C(256));           /* touches both 0 and 512: one 768-byte cell, then 1024 */
    bad |= head != C(0) || C(0)->size != 768 || C(0)->next != C(1024) || C(1024)->prev != C(0) || C(0)->prev != NULL;
    printf("%s\n", bad ? "FAIL" : "PASS: address order and both merges");
    return bad;
}
