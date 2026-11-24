/**
 * @file menu_bios_range_8005D7F8.c
 * @brief menu core bios accessor farm, 0x8005D7F8 - 0x8005DA48.
 *
 * Split out of the former game/menu/menuCB_Battle.c bucket (2026-07-07) into
 * true XD source-unit segments. menuSeBios/menuSeqBios/menuSpriteBios/
 * menuItemBios/menuDataBios accessor farm; sits inside XD's menu-core TU
 * next to menuPanel-family / menuTool_imasugu-family (XD 0x8007C958-0x8007CD64). 8 XD
 * anchors, all monotonic. All functions asm-only.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC */
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_MenuExtra: OrrePort's items and sprites past the tables' ends; OrreHook_MenuSprite */
#endif

extern u8 *menu_se;
extern u32 *menu_se_number;
extern u32 _menu_seq_number;
extern u32 _menu_sprite_number;
extern u32 lbl_80478968;
extern u32 _menu_data_number;
extern u8 _menu_seq[];
extern u8 _menu_sprite[];
extern u8 lbl_802EF0A8[];
extern u8 _menu_data[];

void* menuSeBiosGetPtr(s32 idx) {
    if (menu_se == NULL) {
        return NULL;
    }
    if ((u32)idx >= *menu_se_number) {
        return NULL;
    }
    return menu_se + (idx * 0xA);
}

void* menuSeqBiosGetPtr(u32 idx) {
    if (idx >= _menu_seq_number) {
        return NULL;
    }
    return _menu_seq + (idx * 0xC);
}

void* menuSpriteBiosGetPtr(u32 idx) {
    if (idx >= _menu_sprite_number) {
#ifdef TARGET_PC
        /* OrrePort's own sprites past the table's end (orre_hooks.h; the Options screen's Language label) */
        if (OrreHook_MenuExtra != NULL) return OrreHook_MenuExtra(1, idx);
#endif
        return NULL;
    }
#ifdef TARGET_PC
    /* the chosen language's piece of a word picture instead of the US one (orre_hooks.h, OrreHook_MenuSprite; language
     * piece 4, port/src/pc_word_pictures.cpp); NULL keeps the game's record. All platforms */
    if (OrreHook_MenuSprite != NULL) {
        void* swapped = OrreHook_MenuSprite(idx, _menu_sprite + (idx * 0x18));
        if (swapped != NULL) return swapped;
    }
#endif
    return _menu_sprite + (idx * 0x18);
}

void* menuItemBiosSetXY(s32 idx, s16 x, s16 y) {
    s16* item;

    if ((u32)idx >= lbl_80478968) {
        item = NULL;
    } else {
        item = (s16*)(lbl_802EF0A8 + (u32)idx * 0x1C);
    }
    if (item != NULL) {
        DISC_EL_SET16(item, 1, x);
        DISC_EL_SET16(item, 2, y);
    }
    return item;
}

u32 menuItemBiosGetSelectFlag(u32 idx) {
    u8* item;

    if (idx >= lbl_80478968) {
        item = NULL;
    } else {
        item = lbl_802EF0A8 + (u32)idx * 0x1C;
    }
    if (item != NULL) {
        return item[0] & 0x80 ? 1 : 0;
    }
    return 0;
}

void* menuItemBiosSetSelectFlag(u32 idx, u32 flag) {
    u8* item;

    if (idx >= lbl_80478968) {
        item = NULL;
    } else {
        item = lbl_802EF0A8 + (u32)idx * 0x1C;
    }
    if (item != NULL) {
        item[0] = (u8)((flag << 7) & 0x80) | (item[0] & ~0x80);
    }
    return item;
}

void* menuItemBiosGetPtr(s32 idx) {
    if ((u32)idx >= lbl_80478968) {
#ifdef TARGET_PC
        /* OrrePort's own items past the table's end (orre_hooks.h; the Options screen's Language row) */
        if (OrreHook_MenuExtra != NULL) return OrreHook_MenuExtra(0, (u32)idx);
#endif
        return NULL;
    }
    return lbl_802EF0A8 + idx * 0x1C;
}

#ifdef TARGET_PC
/* OrrePort's own whole menus past the menu table's end (orre_hooks.h, OrreHook_MenuDataExtra; the main menu's SAVE FILES
 * list): the record OrrePort serves for that number, or NULL to fall back to menu 1 as the game does. All platforms */
static u8* orreMenuData(s32 idx) {
    return (idx >= 0 && (u32)idx >= _menu_data_number && OrreHook_MenuDataExtra != NULL)
               ? (u8*)OrreHook_MenuDataExtra((u32)idx)
               : NULL;
}
#endif

void menuDataBiosGetXY(s32 idx, s16* x, s16* y) {
    s16* data;

#ifdef TARGET_PC
    if ((data = (s16*)orreMenuData(idx)) != NULL) {
        if (x != NULL) *x = DISC_EL_S16(data, 3);
        if (y != NULL) *y = DISC_EL_S16(data, 4);
        return;
    }
#endif
    if (idx < 0) {
        idx = 0;
    }
    if ((u32)idx >= _menu_data_number) {
        idx = 1;
    }
    data = (s16*)(_menu_data + idx * 0x1C);
    if (x != NULL) {
        *x = DISC_EL_S16(data, 3);
    }
    if (y != NULL) {
        *y = DISC_EL_S16(data, 4);
    }
}

u32 menuDataBiosSetXY(s32 idx, s16 x, s16 y) {
#ifdef TARGET_PC
    {
        s16* own = (s16*)orreMenuData(idx);
        if (own != NULL) {
            DISC_EL_SET16(own, 3, x);
            DISC_EL_SET16(own, 4, y);
            return PC_PTR32(own);
        }
    }
#endif
    if (idx < 0) {
        idx = 0;
    }
    if ((u32)idx >= _menu_data_number) {
        idx = 1;
    }
    {
        s16* data = (s16*)(_menu_data + idx * 0x1C);
        DISC_EL_SET16(data, 3, x);
        DISC_EL_SET16(data, 4, y);
        return PC_PTR32(data);
    }
}

u32 menuDataBiosGetType(s32 idx) {
#ifdef TARGET_PC
    {
        u8* own = orreMenuData(idx);
        if (own != NULL) return own[0x2];
    }
#endif
    if (idx < 0) {
        idx = 0;
    }
    if ((u32)idx >= _menu_data_number) {
        idx = 1;
    }
    return ((u8*)(_menu_data + (u32)idx * 0x1C))[0x2];
}

void* menuDataBiosGetPtr(s32 idx) {
#ifdef TARGET_PC
    {
        u8* own = orreMenuData(idx);
        if (own != NULL) return own;
    }
#endif
    if (idx < 0) {
        idx = 0;
    }
    if ((u32)idx >= _menu_data_number) {
        idx = 1;
    }
    return (void*)(_menu_data + (u32)idx * 0x1C);
}
