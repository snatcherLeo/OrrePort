/** Exact PDA-mail attachment callbacks, 0x8004E790 - 0x8004E9C0. */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"

typedef struct PdaMailAttachState { /* menu 0x77's block (menu_pda_mail.c): 32-bit pointer slots on the PC */
    s32 unused;
    s32 mailIndex;
    PTR32_FIELD(s32) selection; /* 0x08 */
} PdaMailAttachState;

typedef struct PdaMailAttachWindow {
    u8 pad00[0x60];
    PTR32_FIELD(PdaMailAttachState) state; /* window +0x60: 32-bit slot on the PC too (pc/ptr32.h) */
} PdaMailAttachWindow;

typedef struct PdaMailWindowA {
    u8 pad00;
    s8 phase;
    s8 guard;
    u8 pad03;
    s32 msgObj;
    u8 pad08[0x58];
    PTR32_FIELD(s32*) field_0x60; /* window +0x60: 32-bit slot on the PC too (pc/ptr32.h) */
} PdaMailWindowA;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 mailGetAttachFileGroup(s32 index);
#endif
extern s32 fn_8017B2CC(u32 fileHandle);
extern s32 fn_8017B448(u32 fileHandle);
extern u32 fsysFindLoadedArchive(u32 fileHandle, u32 index);
extern u32 fsysEntryGetType();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void* p);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqSetMenu(s32 ctx, s32 id);
#endif
extern f32 lbl_8047BE4C;
extern f32 lbl_8047BE50;

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
s32 pdaMailAttachKey(PdaMailAttachWindow* window)
{
    u8* input;
    s32 total;
    s32 count;
    s32 index;
    u32 object;
    PdaMailAttachState* state;
    s32 loaded;
    s32 selection;

    state = P32(window->state);
    input = windowGetKeyInfo();
    selection = *P32(state->selection);
    index = state->mailIndex;
    count = mailGetAttachFileGroup(index);
    if (fn_8017B2CC(count) == 1) {
        loaded = 0;
    } else {
        loaded = 1;
    }
    if (loaded == 0) {
        count = -1;
    } else {
        object = mailGetAttachFileGroup(index);
        total = fn_8017B448(object);
        count = 0;
        index = count;
        while (index < total) {

            if (fsysEntryGetType(fsysFindLoadedArchive(object, index)) == 9) {
                count++;
            }
            index++;
        }
    }
    if (count < 2) {
        return 0;
    }
    if ((*(u16*)(input + 6) & MENU_KEY_RIGHT) != 0) {
        selection++;
        if (selection >= count) {
            selection = 0;
        }
    }
    if ((*(u16*)(input + 6) & MENU_KEY_LEFT) != 0) {
        selection--;
        if (selection < 0) {
            selection = count - 1;
        }
    }
    *P32(state->selection) = selection;
    return 0;
}
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole reset; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole reset

#pragma peephole off
/* NAME-T3 fn_8004E89C */ /* button callback of the mail attachment view (menu 0x77) */
void pdaMailAttachButtonCB(void* widget)
{
    u8* state = windowGetKeyInfo();
    if (!(*(u16*)state & 0x10)) {
        menuButtonNormal(widget);
    }
}
#pragma peephole reset

/* RULE-EXCEPTION(user-approved): local peephole control;
 * see docs/RULE_EXCEPTIONS.md. */
#pragma peephole off
/* NAME-T3 fn_8004E8E0 */ /* tick callback of the mail attachment view (menu 0x77) */
s32 pdaMailAttachCtrl(PdaMailWindowA* window)
{
    s32** field = P32(window->field_0x60);
    switch (window->phase) {
    case 0:
        if (window->guard == 0) {
            winSeqSetMenu(0x77, 0x86);
            window->guard = 1;
        }
        break;
    case 2: {
#ifdef TARGET_PC
        /* PC: menu 0x77's block (PdaMailAttachmentConfig, menu_pda_mail.c) keeps the scroll pointer as a 32-bit slot;
         * a native 8-byte read takes mailId as the high half (2026-10-27, the same fix as fn_8004D26C's) */
        f32* scroll = PTR32_AT_T(f32, (u8*)field, 0);
        f32 result;
        f32 thresh = lbl_8047BE4C;
        f32 val = *scroll;
        result = val + lbl_8047BE50;
        *scroll = result;
        if (result >= thresh) {
            *scroll -= thresh;
        }
#else
        f32 result;
        f32 thresh = lbl_8047BE4C;
        f32 val = *(f32*)*field;
        /* RULE-EXCEPTION(user-approved): no-op copy sets FP web priority;
         * see docs/RULE_EXCEPTIONS.md. */
        val = val;
        result = val + lbl_8047BE50;
        *(f32*)*field = result;
        if (result >= thresh) {
            *(f32*)*field -= thresh;
        }
#endif
        break;
    }
    case 3:
        if (window->guard == 0) {
            winSeqSetMenu(0x77, 0x8a);
            window->guard = 1;
        }
        break;
    }
    return 0;
}
#pragma peephole reset
