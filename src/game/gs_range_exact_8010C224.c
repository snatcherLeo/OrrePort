/**
 * @file gs_range_exact_8010C224.c
 * @brief menuFaceInit, 0x8010C224 - 0x8010C364: allocates and clears the two
 *        per-slot tables (16- and 8-byte entries) and each slot's 0x6EC0
 *        buffer; menuInit calls it with 0x18.
 *
 * Function-boundary carve of gs_range_80109C88.c. Text only (the tables and
 * handles are extern .sbss). Built GC/2.0 -O4,p like the rest of the range,
 * but with the peephole pass off: retail keeps the extsb before the slot
 * byte store (the peephole deletes it), and the range's other functions
 * already carry local "peephole off" pragmas. The count is re-read from its
 * global after every call, as retail does.
 */
#include "dolphin/types.h"

#ifdef TARGET_PC
/* PC: the same layout as the portrait slot everywhere else (FaceSlot, gs_range_80109C88.c), with its two pointers
 * native. The padded GameCube copy below is 16 bytes there but 24 on the PC against FaceSlot's 32:
 * the array was allocated too small and every slot index landed at the wrong place, so the queue saw garbage slots
 * "waiting to load" and spun forever (2026-10-17, verified by the slot report from Willie's battle). */
typedef struct SlotEntry {
    void* data;
    u16 key;
    u8 state;
    s8 slot;
    void* (*callback)(u32);
    u32 arg;
} SlotEntry;
#else
typedef struct SlotEntry {
    void* data;
    u8 padding[2];
    u8 state;
    s8 slot;
    u8 padding2[8];
} SlotEntry;
#endif

typedef struct SlotBuffer {
    u8 padding[2];
    u16 handle;
    void* data;
} SlotBuffer;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
extern u16 GSmemAlloc(u32 size, u32 alignment);

extern s32 lbl_8047AD48;
extern SlotEntry* lbl_8047AD4C;
extern u16 lbl_8047AD50;
extern SlotBuffer* lbl_8047AD54;
extern u16 lbl_8047AD58;

/* 0x8010C224 | 0x140 */
void menuFaceInit(s32 count)
{
    s32 i;

    lbl_8047AD48 = count;
    if (lbl_8047AD50 == 0) {
        lbl_8047AD50 = _toolentryAlloc__FUl(lbl_8047AD48 * sizeof(SlotEntry));
        lbl_8047AD4C = GSmemLock(lbl_8047AD50);
    }
    memset(lbl_8047AD4C, 0, lbl_8047AD48 * sizeof(SlotEntry));

    if (lbl_8047AD58 == 0) {
        lbl_8047AD58 = _toolentryAlloc__FUl(lbl_8047AD48 * sizeof(SlotBuffer));
        lbl_8047AD54 = GSmemLock(lbl_8047AD58);
    }
    memset(lbl_8047AD54, 0, lbl_8047AD48 * sizeof(SlotBuffer));

    for (i = 0; i < lbl_8047AD48; i++) {
        if (lbl_8047AD54[i].handle == 0) {
            lbl_8047AD54[i].handle = GSmemAlloc(0x6ec0, 0x20);
            lbl_8047AD54[i].data = GSmemLock(lbl_8047AD54[i].handle);
        }
        memset(lbl_8047AD54[i].data, 0, 0x6ec0);
        lbl_8047AD4C[i].slot = i;
    }
}
