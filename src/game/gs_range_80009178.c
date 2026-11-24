/**
 * @file gs_range_80009178.c
 * @brief gs-engine, 0x80009178 - 0x800096B4.
 *
 * Boundary evidence-verified from asm (sdata clusters, callee families,
 * static linkage, call chains) -- mixed-block split pass, 2026-07-01.
 * The three functions in this range are reconstructed in C.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/disc_types.h" /* DISC_STRUCT: common_rel tables are big-endian on the PC */
#include "dolphin/pad/Pad.h"

/* RULE-EXCEPTION(user-approved): file-scope peephole-control pragma — see docs/RULE_EXCEPTIONS.md */
#pragma peephole off

typedef struct GsMenuKeyInfo {
    u16 buttons;
    u16 previous;
    u16 down;
    u16 repeat;
} GsMenuKeyInfo;

typedef struct GsMenuNumberInput {
    u8 pad_00[4];
    s32 menuId;
    u8 pad_08[0x58];
    void* pokemon;
    s32* amount;
    u8 pad_68[0x18];
    s32 result;
    u8 pad_84[0x11];
    u8 pokemonValid;
    u8 pad_96[2];
    u8 done;
} GsMenuNumberInput;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern const GsMenuKeyInfo* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonCheckValid(void* pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuDataBiosGetPtr(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorItemID(s32 menuId);
#endif

typedef struct DISC_STRUCT GsMenuListState { /* common_rel flag data: big-endian view on the PC */
    u8 pad_00[4];
    s16 head;
} GsMenuListState;

typedef struct DISC_STRUCT GsMenuListEntry { /* the common_rel flag-definition table: big-endian view on the PC */
    u8 pad_00[3];
    u8 item;
    u8 pad_04[2];
    s16 next;
} GsMenuListEntry;

typedef struct GsMenuCursor {
    s8 row;
    s8 column;
} GsMenuCursor;

static inline s32 gsMenuCountValidEntries(void)
{
    extern GsMenuListState* lbl_80478F98;
    extern GsMenuListEntry* lbl_80478F9C;
    extern u8* lbl_80478EFC;
    GsMenuListEntry* entry;
    s32 entryIndex;
    u8* scenes;
    GsMenuListEntry* entries;
    s32 count;

    count = 0;
    entryIndex = lbl_80478F98->head;
    entries = lbl_80478F9C;
    scenes = lbl_80478EFC;
    while (entryIndex != -1) {
        entry = &entries[entryIndex];
        if (entry->item != 0 &&
            DISC_SLOT(scenes + entry->item * 0x18 + 0x14, 0) != NULL) {
            count++;
        }
        entryIndex = entry->next;
    }
    return count;
}

static inline s32 gsMenuGetValidEntry(s32 target)
{
    extern GsMenuListState* lbl_80478F98;
    extern GsMenuListEntry* lbl_80478F9C;
    extern u8* lbl_80478EFC;
    GsMenuListEntry* entries;
    s32 entryIndex;
    s32 count;

    count = 0;
    entryIndex = lbl_80478F98->head;
    entries = lbl_80478F9C;
    while (entryIndex != -1) {
        /* PC: the scene table holds disc pointers in big-endian order (DISC_SLOT; the same load on the GameCube) */
        if (entries[entryIndex].item != 0 &&
            DISC_SLOT(lbl_80478EFC + entries[entryIndex].item * 0x18 + 0x14, 0) != NULL) {
            if (target == count) {
                return entryIndex;
            }
            count++;
        }
        entryIndex = entries[entryIndex].next;
    }
    return -1;
}

void fn_80009178(GsMenuNumberInput* input)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuDataBiosGetType(s32 menuId);
#endif
    GsMenuCursor cursor;
    s8 validCount;
    s8 columns;
    u16 repeat;

    repeat = ((const GsMenuKeyInfo*)windowGetKeyInfo())->repeat;
    validCount = gsMenuCountValidEntries();

    columns = menuDataBiosGetType(input->menuId);
    if (validCount < columns) {
        columns = validCount;
    }

    cursor = *(GsMenuCursor*)&input->pad_84[0x10];
    if (repeat & MENU_KEY_UP) {
        cursor.column--;
    } else if (repeat & MENU_KEY_DOWN) {
        cursor.column++;
    }
    if (repeat & MENU_KEY_LEFT) {
        cursor.column -= columns - 1;
    } else if (repeat & MENU_KEY_RIGHT) {
        cursor.column += columns - 1;
    }

    if (cursor.column < 0) {
        cursor.row += cursor.column;
        cursor.column = 0;
        if (cursor.row < 0) {
            cursor.column = columns - 1;
            cursor.row = validCount - columns;
        }
    } else if (cursor.column >= columns) {
        cursor.row += cursor.column - (columns - 1);
        cursor.column = columns - 1;
        if (cursor.row + cursor.column >= validCount) {
            cursor.row = 0;
            cursor.column = 0;
        }
    }
    *(GsMenuCursor*)&input->pad_84[0x10] = cursor;

    input->result = gsMenuGetValidEntry(cursor.row + cursor.column);
}

s32 fn_800093D0(void)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800F7BC4(s32 padId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpen(s32 menuId, s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std082_flagGetValue(s32 flagId);
#endif
    extern u8 menuSelectInput(u32 value, u32* out);
    extern void _flagSet(s32 flagId, u32 value);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 menuId);
#endif
    extern void fn_8018FE30(s32 value);
    s32 result;
    u32 selected;
    u32 value;

    if (fn_800F7BC4(1) & PAD_TRIGGER_R) {
        for (;;) {
            result = menuOpen(0xDD, 1);
            if (result >= 0) {
                selected = result;
                if (menuSelectInput(std082_flagGetValue(selected), &value) == 0) {
                    continue;
                }
                _flagSet(selected, value);
            } else {
                if (result == -1) {
                    menuClose(0xDD);
                    return -1;
                }
                break;
            }
        }
    } else {
        result = menuOpen(0xB, 1);
        menuClose(0xB);
        if (result >= 0) {
            fn_8018FE30(result);
        } else if (result == -1) {
            return -1;
        }
    }

    return 0;
}

void fn_800094A4(GsMenuNumberInput* input)
{
    void* pokemon;
    s32* amount;
    u16 repeat;
    u16 buttons;
    s32 change;
    s32 cursor;

    pokemon = input->pokemon;
    amount = input->amount;
    if (input == NULL) {
        return;
    }

    repeat = ((const GsMenuKeyInfo*)windowGetKeyInfo())->repeat;
    buttons = ((const GsMenuKeyInfo*)windowGetKeyInfo())->buttons;
    if (repeat & MENU_KEY_B) {
        input->result = -1;
        input->done = 1;
        return;
    }
    if (repeat & MENU_KEY_A) {
        input->result = -2;
        input->done = 1;
        return;
    }
    if (pokemon == NULL) {
        input->result = -1;
        input->done = 1;
        return;
    }

    change = 0;
    if ((repeat & MENU_KEY_Y) && (buttons & MENU_KEY_L) && (buttons & MENU_KEY_R)) {
        change = 100;
    } else if ((repeat & MENU_KEY_X) && (buttons & MENU_KEY_L) &&
               (buttons & MENU_KEY_R)) {
        change = -100;
    } else if ((repeat & MENU_KEY_Y) && (buttons & MENU_KEY_L)) {
        change = 10;
    } else if ((repeat & MENU_KEY_X) && (buttons & MENU_KEY_L)) {
        change = -10;
    } else if (repeat & MENU_KEY_Y) {
        change = 1;
    } else if (repeat & MENU_KEY_X) {
        change = -1;
    } else if ((repeat & MENU_KEY_Z) && (buttons & MENU_KEY_L)) {
        change = 0xFFFF;
    } else if (repeat & MENU_KEY_Z) {
        change = -0xFFFF;
    }

    if (!pokemonCheckValid(pokemon)) {
        input->pokemonValid = 0;
        cursor = 0xE;
    } else {
        menuDataBiosGetPtr(input->menuId);
        cursor = menuGetCursorItemID(input->menuId);
    }
    if (change == 0) {
        return;
    }
    if (amount != NULL) {
        *amount = change;
    }
    input->result = cursor;
    input->done = 1;
}

