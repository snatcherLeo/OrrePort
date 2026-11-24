#include "dolphin/types.h"

typedef struct PdaMailWindowB {
    u8 pad00[0x60];
    PTR32_FIELD(s32) field_0x60; /* window +0x60: 32-bit slot on the PC too (pc/ptr32.h) */
} PdaMailWindowB;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 mailGetAttachFileGroup(s32 index);
#endif
extern s32 fn_8017B2CC(u32 fileHandle);
extern s32 fn_8017B448(u32 fileHandle);
extern u32 fsysFindLoadedArchive(u32 fileHandle, u32 index);
extern u32 fsysEntryGetType(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp(void* fieldHandle, s32 value);
#endif

/* RULE-EXCEPTION(user-approved): reconstructed inline helpers for this callback; see docs/RULE_EXCEPTIONS.md. */
static inline s32 pdaMailIsAttachReady(s32 index)
{
    if (fn_8017B2CC(mailGetAttachFileGroup(index)) == 1) {
        return 0;
    }
    return 1;
}

static inline s32 pdaMailCountAttachItems(s32* state)
{
    u32 object;
    s32 index;
    s32 total;
    s32 count;
    s32 i;

    index = state[1];
    if (!pdaMailIsAttachReady(index)) {
        return -1;
    }
    object = mailGetAttachFileGroup(index);
    total = fn_8017B448(object);
    count = 0;
    for (i = 0; i < total; i++) {
        fsysFindLoadedArchive(object, i);
        if (fsysEntryGetType() == 9) {
            count++;
        }
    }
    return count;
}

/* RULE-EXCEPTION(user-approved): local peephole setting preserves this callback's codegen; see docs/RULE_EXCEPTIONS.md. */
#pragma peephole off
s32 fn_8004E440(PdaMailWindowB* window, void* fieldHandle)
{
    s32 count;

    count = pdaMailCountAttachItems(P32(window->field_0x60));
    if (count <= 0) {
        winSpriteSetDisp(fieldHandle, 0);
    } else {
        winSpriteSetDisp(fieldHandle, 1);
    }
    return 0;
}
#pragma peephole reset
