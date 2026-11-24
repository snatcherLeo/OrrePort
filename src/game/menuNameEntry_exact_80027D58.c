/* Name-entry keyboard cursor, 0x80027D58-0x800280FC.
 * RULE-EXCEPTION(user-approved): unit-scope -opt nopeephole and
 * -opt nostrength (frontend strength reduction off); see
 * docs/RULE_EXCEPTIONS.md. Source mirrors src/game/menuNameEntry.c.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "game/name_entry_arg.h" /* NAME_ENTRY_ARG (32-bit pointer slots on the PC: P32 reads them) */
#include "pc/orre_hooks.h" /* OrreHook_NameEntryInput: OrrePort's naming keyboard (PC only; empty on the GameCube) */

extern s32 GSmsgGetLength(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32);
#endif
extern u8 lbl_80266DD8[];
extern const u32 lbl_8047B920[2];

/* Removes the last letter of the name; FALSE when the name is already empty. */
static inline u8 menuNameEntryDeleteLetter(NAME_ENTRY_ARG* arg)
{
    s32 pos;
    u8 deleted;

    pos = *P32(arg->state);
    if (pos <= 0) {
        deleted = 0;
    } else {
        u16* name = P32(arg->name);
        pos--;
        name += pos;
        *name = 0;
        deleted = 1;
        *P32(arg->state) = pos;
    }
    return deleted;
}

u16 exchangeDakuon__FUs11DAKUON_MODE(u16 letter, s32 mode);
s32 selectLetter__FP14NAME_ENTRY_ARG(NAME_ENTRY_ARG* arg);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16* windowGetKeyInfo(void);
#endif

static inline s32 menuNameEntryGetDakuonMode(u16 letter)
{
    u32* tables;
    s32 kind;
    s32 count;
    s32 i;
    u16* chars;

    kind = 0;
    do {
        tables = (u32*)lbl_8047B920;
        if (tables[kind] != 0) {
            count = GSmsgGetLength((void*)PC_PTR(tables[kind]));
            chars = (u16*)GSmsgGetGSchar(tables[kind]) + 1;
            for (i = 1; i < count; i += 2, chars += 2) {
                if (*chars == letter) {
                    break;
                }
            }
            if (i < count) {
                break;
            }
        }
    } while (++kind < 2);
    if (kind >= 2) {
        kind = 0;
    }
    return kind;
}
s32 menuNameEntryCursor(void* window)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartSE(u32 se);
#endif
    u16* keys;
    u16* letters;
    s32 row;
    u16 converted;
    s32 value;
    u16* name;
    s32* rowp;
    s32 mode;
    s32 length;
    NAME_ENTRY_ARG* arg;
    s32 pos;
    s32 offset;
    s32 kind;

#ifdef TARGET_PC
    /* OrrePort's naming keyboard (orre_hooks.h; 2026-11-14): when registered it handles the frame itself (its
     * pages, Shift, typing, the mouse). NULL in GameCube-accurate mode: the game's own input below. */
    if (OrreHook_NameEntryInput != NULL && OrreHook_NameEntryInput(window) != 0) {
        return 0;
    }
#endif
    keys = windowGetKeyInfo();
    arg = ((NAME_ENTRY_ARG*)PTR32_AT(window, 0x60));
    if (keys[2] & MENU_KEY_X) {
        rowp = P32(arg->row);
        row = *rowp;
        row++;
        if (row >= 2) {
            row = 0;
        }
        *rowp = row;
        soundStartSE(0x27);
        return 0;
    }
    if (keys[2] & MENU_KEY_A) {
        if (selectLetter__FP14NAME_ENTRY_ARG(arg)) {
            ((u8*)window)[0x98] = 1;
            return 0;
        }
        letters = P32(arg->name);
        length = 0;
        while (*letters != 0) {
            letters++;
            length++;
        }
        if (length >= ((s32*)(lbl_80266DD8 + 4))[arg->kind * 4]) {
            *P32(arg->letter) = 0xf;
            *P32(arg->column) = 3;
        }
        return 0;
    }
    if (keys[2] & MENU_KEY_B) {
        if (menuNameEntryDeleteLetter(arg)) {
            soundStartSE(0x25);
        }
        return 0;
    }
    if (keys[2] & MENU_KEY_R) {
        pos = *P32(arg->state) - 1;
        if (pos >= 0) {
            name = P32(arg->name);
            offset = pos;
            kind = menuNameEntryGetDakuonMode(name[offset]);
            mode = kind;
            do {
                mode++;
                if (mode >= 2) {
                    mode = 0;
                }
                converted = exchangeDakuon__FUs11DAKUON_MODE(name[offset], mode);
            } while (converted == 0);
            name[offset] = converted;
        }
        soundStartSE(0x24);
        return 0;
    }
    if (keys[2] & MENU_KEY_START) {
        ((u8*)window)[0x98] = 1;
        return 0;
    }
    if (keys[3] & MENU_KEY_RIGHT) {
        value = *P32(arg->letter) + 1;
        if (value >= 0x10) {
            value = 0xf;
        } else {
            soundStartSE(0x23);
        }
        *P32(arg->letter) = value;
    }
    if (keys[3] & MENU_KEY_LEFT) {
        value = *P32(arg->letter) - 1;
        if (value < 0) {
            value = 0;
        } else {
            soundStartSE(0x23);
        }
        *P32(arg->letter) = value;
    }
    if (keys[3] & MENU_KEY_DOWN) {
        value = *P32(arg->column);
        if (*P32(arg->letter) >= 0xf) {
            if (value == 1) {
                value += 2;
            } else {
                value += 1;
            }
        } else {
            value += 1;
        }
        if (value >= 4) {
            value = 3;
        } else {
            soundStartSE(0x23);
        }
        *P32(arg->column) = value;
    }
    if (keys[3] & MENU_KEY_UP) {
        value = *P32(arg->column);
        if (*P32(arg->letter) >= 0xf) {
            if (value == 2) {
                value -= 2;
            } else {
                value -= 1;
            }
        } else {
            value -= 1;
        }
        if (value < 0) {
            value = 0;
        } else {
            soundStartSE(0x23);
        }
        *P32(arg->column) = value;
    }
    return 0;
}
