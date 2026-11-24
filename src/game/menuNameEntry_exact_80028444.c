/**
 * @file menuNameEntry_exact_80028444.c
 * @brief Name-entry selection labels and button callback.
 */
#include "dolphin/types.h"
#include "game/constants/menu_keys.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetGSchar(u32 messageId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(s32 slot, u32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16* windowGetKeyInfo(void);
#endif

s32 menuNameEntrySelectDrawSelText3(void* window, u8* output)
{
    u8* outputBytes;
    void* context;
    void* messages;
    void* message;

    outputBytes = output;
    context = ((void*)PTR32_AT(window, 0x60));
    messages = PTR32_AT_T(void, (u8*)context, 0); /* 32-bit slots: the list argument and its id list */
    message = PTR32_AT_T(void, (u8*)messages, 8);
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(message))));
    *(u32*)(outputBytes + 0x4C) = 0xCF;
    return 0;
}

s32 menuNameEntrySelectDrawSelText2(void* window, u8* output)
{
    u8* outputBytes;
    void* context;
    void* messages;
    void* message;

    outputBytes = output;
    context = ((void*)PTR32_AT(window, 0x60));
    messages = PTR32_AT_T(void, (u8*)context, 0); /* 32-bit slots: the list argument and its id list */
    message = PTR32_AT_T(void, (u8*)messages, 4);
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(message))));
    *(u32*)(outputBytes + 0x4C) = 0xCF;
    return 0;
}

s32 menuNameEntrySelectDrawSelText1(void* window, u8* output)
{
    u8* outputBytes;
    void* context;
    void* messages;
    void* message;

    outputBytes = output;
    context = ((void*)PTR32_AT(window, 0x60));
    messages = PTR32_AT_T(void, (u8*)context, 0); /* 32-bit slots: the list argument and its id list */
    message = PTR32_AT_T(void, (u8*)messages, 0);
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(message))));
    *(u32*)(outputBytes + 0x4C) = 0xCF;
    return 0;
}

void menuNameEntrySelectButton(void* window)
{
    u8* work;
    u16* keys;

    work = window;
    keys = windowGetKeyInfo();
#ifdef TARGET_PC
    {
        /* OrreHook_NameMenuCanLeave (orre_hooks.h): the game's name menu ignores B (only A, and only while B isn't
         * down, closes it). While menuNameEntry says this name menu may be left (Wes's from New Game, snatcherLeo
         * 2026-11-15; OrrePort's rule), a B press closes it with "back" (+0x99, so menuOpenCustom returns -1), as a
         * normal menu's B does. 0, the default: B ignored, as on the GameCube. Windows, Linux and macOS alike */
        extern s32 OrreHook_NameMenuCanLeave;
        extern void soundStartSE(u32 id);                      /* as pc/prototypes.h */
        extern void* menuSeBiosGetPtr(s32 group);              /* menu.c */
        extern u16 menuSeTableLookup(void* table, s32 event); /* menu.c */
        extern void (*OrreHook_NameMenuLeft)(void);
        if (OrreHook_NameMenuCanLeave != 0 && (keys[2] & MENU_KEY_B) != 0) {
            work[0x98] = 1;
            work[0x99] = 1;
            /* OrreHook_NameMenuLeft (orre_hooks.h): the port shortens the fades on the way back (NULL: none) */
            if (OrreHook_NameMenuLeft != NULL) OrreHook_NameMenuLeft();
            /* the game's own "back" sound (snatcherLeo, 2026-11-15: "it should play the SFX that is associated with hitting
             * B normally", then, after hearing it: "the SFX that plays when you click into LOAD, and then back out of
             * it by hitting B, that SFX we can use"): closing through this callback skips the menu daemon's B branch,
             * which plays a window's sound event 3; so it is played here the daemon's way (menu.c MENU_PLAY_SE: a
             * sound group's table, event 3), with sound group 1, the group of the SAVE FILES list (and the bag) whose
             * B sound snatcherLeo picked, not this list's own */
            {
                void* table = menuSeBiosGetPtr(1);
                const u16 se = table != NULL ? menuSeTableLookup(table, 3) : 0;
                if (se != 0) soundStartSE(se);
            }
            return;
        }
    }
#endif
    if ((keys[0] & MENU_KEY_B) == 0) {
        if ((keys[2] & MENU_KEY_A) != 0) {
            work[0x98] = 1;
        }
    }
}
