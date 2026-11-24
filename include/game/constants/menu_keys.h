#ifndef GAME_CONSTANTS_MENU_KEYS_H
#define GAME_CONSTANTS_MENU_KEYS_H

/*
 * Menu keys: the game's own 16-bit key word that every window and menu reads (windowGetKeyInfo, menuGetKeyInfo),
 * built once per frame per port by _menuUpdateKeyInfo from a GameCube pad (_menuGetGcKeyInfo) or, with no pad, a
 * linked GBA (_menuGetAgbKeyInfo). Verified 2026-10-16 from _menuGetGcKeyInfo, which maps each SDK pad bit
 * (PAD_BUTTON_* / PAD_TRIGGER_* in dolphin/pad/Pad.h) to one of these, and the control stick past 0x20 from centre to
 * the four directions (a diagonal sets two). The raw pad readers (fn_800F7BC4, GSinputButtonsChanged) return the SDK
 * bits, not these. Table in docs/engine/message-windows.md, "Window input keys".
 * Added 2026-11-09 (named constants, snatcherLeo's plan of 2026-11-08: "controller buttons").
 */

#define MENU_KEY_UP                   0x0001
#define MENU_KEY_DOWN                 0x0002
#define MENU_KEY_LEFT                 0x0004
#define MENU_KEY_RIGHT                0x0008
#define MENU_KEY_A                    0x0010
#define MENU_KEY_B                    0x0020
#define MENU_KEY_X                    0x0040
#define MENU_KEY_Y                    0x0080
#define MENU_KEY_Z                    0x0100  /* GBA: Select */
#define MENU_KEY_L                    0x0200
#define MENU_KEY_R                    0x0400
#define MENU_KEY_START                0x0800

#endif /* GAME_CONSTANTS_MENU_KEYS_H */
