#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 mailGetAttachFileGroup(s32 index);
#endif
extern s32 fn_8017B2CC(u32 fileHandle);
extern s32 fn_8017B448(u32 fileHandle);
extern u32 fsysFindLoadedArchive(u32 fileHandle, u32 index);
extern u32 fsysEntryGetType(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(s32 messageId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor(s32 x, s32 y, u32 color, s32 msgId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp(void* fieldHandle, s32 value);
#endif
extern const f32 lbl_8047BE38;
extern const f32 lbl_8047BE3C;

/* RULE-EXCEPTION(user-approved): reconstructed inline helpers for these callbacks; see docs/RULE_EXCEPTIONS.md. */
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

/* RULE-EXCEPTION(user-approved): local peephole setting preserves these callbacks' codegen; see docs/RULE_EXCEPTIONS.md. */
#pragma peephole off
s32 pdaMailAttachDrawTitle(u8* context, u8* object)
{
    u8* attachmentState;
    u32 fileHandle;
    s32 count;
    s32 current;
    s32 index;
    s32 selectedWidth;
    s32 normalWidth;
    s32 x;

    attachmentState = PTR32_AT_T(u8, context, 0x60);
    current = *PTR32_AT_T(s32, attachmentState, 8); /* menu 0x77's block: 32-bit slots (menu_pda_mail.c) */
    count = pdaMailCountAttachItems((s32*)attachmentState);
    if (count <= 0) {
        return 0;
    }

    selectedWidth = GSmsgGetRect(0x36CE) >> 16;
    normalWidth = GSmsgGetRect(0x36CF) >> 16;
    x = *(s16*)(object + 0x54) / 2 -
        (selectedWidth + normalWidth * (count - 1)) / 2;
    for (index = 0; index < count; index++) {
        if (index == current) {
            GSmsgPrintColor(x, 0, 0xE6AA00FF, 0x36CE);
            x += selectedWidth;
        } else {
            GSmsgPrintColor(x, 0, 0xAAAAAAFF, 0x36CF);
            x += normalWidth;
        }
    }
    return 0;
}
#pragma peephole reset

/* RULE-EXCEPTION(user-approved): local compiler-control pragma kept for byte match - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 fn_8004E2E0(u8* context, u8* object)
{
    u8* attachmentState;
    u32 fileHandle;
    s32 count;

    attachmentState = PTR32_AT_T(u8, context, 0x60);
    count = pdaMailCountAttachItems((s32*)attachmentState);

    if (count < 2) {
        winSpriteSetDisp(object, 0);
    } else {
        winSpriteSetDisp(object, 1);
    }
    if (*(s16*)(object + 6) == 0x507) {
        *(s16*)(object + 0x50) =
            lbl_8047BE38 * *PTR32_AT_T(f32, attachmentState, 0) +
            (f32)*(s32*)(attachmentState + 0xC);
    } else {
        *(s16*)(object + 0x50) =
            lbl_8047BE3C * *PTR32_AT_T(f32, attachmentState, 0) +
            (f32)*(s32*)(attachmentState + 0x10);
    }
    return 0;
}
#pragma peephole reset
