/**
 * @file menu_pda_mail_exact_8004D6AC.c
 * @brief Resolve the selected mail body message.
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
extern u32 mailGetContents(s32 id);

s32 fn_8004D6AC(PdaMailWindow* window, PdaMailOutput* output)
{
    output->field_0x4c =
        mailGetContents(pdaMailGetMailID(**P32(window->field_0x60)));
    return 0;
}
