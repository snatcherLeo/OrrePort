/**
 * @file menu_exact_8007B6A4.c
 * @brief Run the GBA boot worker and publish completion.
 */
#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gbaBootTransfer(u8* context);
#endif

void _gbaBootThread(u8* context)
{
    u8* saved_context = context;

    gbaBootTransfer(saved_context);
    saved_context[0x345] = 1;
}
