/*
 * pc_sdk_gaps.c: SDK / runtime functions the first link had as logging stubs, for the PC (2026-10-16).
 *
 * fn_8009A92C, OSAlloc's free-list insert (the SDK's DLInsert): the OSAlloc units the PC compiles
 * (sdk_range_8009A9D8.c -> sdk_range_8009A2D8.c) call it from OSFreeToHeap, and as a stub (returning 0) every free
 * replaced the heap's free list with NULL. Its own unit, sdk_exact_8009A92C.c, views a cell as three 4-byte words; on
 * the PC the cells are sdk_range_8009A2D8.c's OSAllocCell with native pointers, so this is the same logic over that
 * layout: insert the cell in address order, then merge it with the next free cell and with the previous one when
 * they touch. Returns the new list head.
 *
 * fn_800CAA3C, the C runtime's 16-bit string copy (wcscpy, src/crt/string.c; the PC doesn't build src/crt): the save
 * data code calls it (cardesavedata.c).
 */
#include <stddef.h>
#include <stdint.h>

typedef struct PcOSAllocCell { /* = sdk_range_8009A2D8.c's OSAllocCell */
    struct PcOSAllocCell* prev;
    struct PcOSAllocCell* next;
    int32_t size;
} PcOSAllocCell;

PcOSAllocCell* fn_8009A92C(PcOSAllocCell* head, PcOSAllocCell* cell) {
    PcOSAllocCell* prev = NULL;
    PcOSAllocCell* next = head;

    while (next != NULL && cell > next) {
        prev = next;
        next = next->next;
    }
    cell->next = next;
    cell->prev = prev;
    if (next != NULL) {
        next->prev = cell;
        if ((uint8_t*)cell + cell->size == (uint8_t*)next) {
            cell->size += next->size;
            next = next->next;
            cell->next = next;
            if (next != NULL) {
                next->prev = cell;
            }
        }
    }
    if (prev != NULL) {
        prev->next = cell;
        if ((uint8_t*)prev + prev->size == (uint8_t*)cell) {
            prev->size += cell->size;
            prev->next = next;
            if (next != NULL) {
                next->prev = prev;
            }
        }
        return head;
    }
    return cell;
}

uint16_t* fn_800CAA3C(uint16_t* dst, const uint16_t* src) {
    uint16_t* d = dst;
    while ((*d++ = *src++) != 0) {
    }
    return dst;
}
