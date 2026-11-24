/**
 * @file menu_pda_mail_exact_8004D590.c
 * @brief Mail attachment visibility and message callbacks.
 */
#include "dolphin/types.h"

typedef struct PdaMailWindow {
    u8 pad00[0x60];
    PTR32_FIELD(s32*) field_0x60; /* window +0x60: 32-bit slot on the PC too (pc/ptr32.h) */
} PdaMailWindow;

typedef struct PdaMailOutput {
    u8 pad00[0x4C];
    u32 field_0x4c;
} PdaMailOutput;

extern s32 pdaMailGetMailID(s32 index);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 mailGetAttachFileGroup(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp(void* field, s32 visible);
#endif

s32 pdaMailDrawPicture(PdaMailWindow* window, PdaMailOutput* output)
{
    if (mailGetAttachFileGroup(pdaMailGetMailID(**P32(window->field_0x60))) != 0) {
        output->field_0x4c = 0x36B9;
    } else {
        output->field_0x4c = 0;
    }
    return 0;
}

s32 fn_8004D5EC(PdaMailWindow* window, void* field)
{
    u8 visible;

    if (mailGetAttachFileGroup(pdaMailGetMailID(**P32(window->field_0x60))) != 0) {
        visible = 1;
    } else {
        visible = 0;
    }
    winSpriteSetDisp(field, visible);
    return 0;
}

s32 fn_8004D64C(PdaMailWindow* window, void* field)
{
    u8 visible;

    if (mailGetAttachFileGroup(pdaMailGetMailID(**P32(window->field_0x60))) != 0) {
        visible = 1;
    } else {
        visible = 0;
    }
    winSpriteSetDisp(field, visible);
    return 0;
}
