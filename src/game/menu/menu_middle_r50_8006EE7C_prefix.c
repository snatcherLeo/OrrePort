/**
 * @file menu_middle_r50_8006EE7C_prefix.c
 * @brief menu_middle carve, 0x8006EE7C - 0x8006EFF8: menuRuleItemListButtonCB
 *        (toggle the ban flag of the regulation item under the cursor on the
 *        trigger key) and menuRuleItemListCursorCB (scroll the 60-item list with
 *        the repeat keys when the cursor is on row 0 or 10). Both belong to
 *        menu 0xB4, the rule editor's item list. Note: the field named
 *        `cursor` below holds the list's scroll offset (0..50) and `scroll`
 *        the visible row (0..10), as menuRuleItemListCtrl reads them.
 *
 * Text only, GC/1.3 -O4,p with the peephole pass off for the whole unit
 * (retail keeps `beq; b` pairs the pass would fold; the range file wraps
 * both functions in `#pragma peephole off`). Both are menu state handlers:
 * in state 2 they act on the key, otherwise they fall through to the
 * default button/cursor handler.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"

typedef struct MenuKeyInfo {
    u32 pad0;
    u16 trigger;
    u16 repeat;
} MenuKeyInfo;

typedef struct MenuMiddleState {
    u8 pad0;
    s8 state;
    u8 pad2[0x92];
    s8 cursor;
    s8 scroll;
    u8 pad96[2];
    u8 dirty;
} MenuMiddleState;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MenuKeyInfo* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetParam(void* window, s32 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void* menu);
#endif
extern void menuCursorNormal(void* menu);

/* NAME-T3 fn_8006EE7C */
void menuRuleItemListButtonCB(void* menu)
{
    MenuMiddleState* state = menu;
    u8* params;
    s32 index;

    switch (state->state) {
    case 2:
        if (((MenuKeyInfo*)windowGetKeyInfo())->trigger & MENU_KEY_A) {
            index = state->scroll + state->cursor;
            if (index < 60) {
                params = PC_PTR(windowGetParam(menu, 0));
                params[index] = params[index] == 0;
                state->dirty = 0;
                return;
            }
        }
        break;
    }
    menuButtonNormal(menu);
}

/* NAME-T3 fn_8006EF24 */
void menuRuleItemListCursorCB(void* menu)
{
    MenuMiddleState* state = menu;

    switch (state->state) {
    case 2:
        if (state->scroll == 0) {
            if (((MenuKeyInfo*)windowGetKeyInfo())->repeat & MENU_KEY_UP) {
                state->cursor--;
                if (state->cursor < 0) {
                    state->cursor = 0;
                }
                return;
            }
        } else if (state->scroll == 10) {
            if (((MenuKeyInfo*)windowGetKeyInfo())->repeat & MENU_KEY_DOWN) {
                state->cursor++;
                if (state->cursor > 50) {
                    state->cursor = 50;
                }
                return;
            }
        }
        break;
    }
    menuCursorNormal(menu);
}
