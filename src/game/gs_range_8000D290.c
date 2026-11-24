/**
 * @file gs_range_8000D290.c
 * @brief gs-engine code, 0x8000D290 - 0x8000DAA8 (4 fns).
 *
 * Range unit assigned from the propagated subsystem map
 * (tools/subsystem_propagation.py, >=80% single-label dominance;
 * campaign 2026-07-01). All functions asm-only until matched; the
 * range name stays honest until internal TU structure is proven.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* big-endian data read in place on the PC */
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* the pause menu's PC MENU: OrreFieldSpot and its hooks */
#endif

/* 0x8000D290 */
u32 fn_8000D290(void) {
    return 1;
}

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSpriteSetDisp(void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* menuItemBiosGetPtr(s16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowDrawSprite(s32, s32, void*, u16, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 menuGetCursorItemID(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor(s16, s16, u32, void*);
#endif

#ifdef TARGET_PC /* item + 0x4C is a 32-bit pointer slot of a GameCube-layout record (read natively it takes the next
                   * field as the upper half) */
#define ITEM_SLOT_4C ((void*)(uintptr_t)*(u32*)(item + 0x4C))
#else
#define ITEM_SLOT_4C (*(void**)(item + 0x4C))
#endif
void fn_8000D298(u8* menu, u8* item)
{
    u8* item_data;
    s16 x;
    s16 y;
    u32 color;

    winSpriteSetDisp(item, 0);
    item_data = menuItemBiosGetPtr(*(s16*)(item + 6));
    if (item_data != NULL) {
        windowDrawSprite(0, 0, menu, DISC_S16(item_data + 0xA), 0);
    }

    if (ITEM_SLOT_4C != NULL) {
#ifdef TARGET_PC /* menu + 4 is a 32-bit slot of a GameCube-layout record: a native 8-byte read took the next field too */
        if (*(s16*)(item + 6) == menuGetCursorItemID(*(u32*)(menu + 4))) {
#else
        if (*(s16*)(item + 6) == menuGetCursorItemID(PC_PTR32_IMPLICIT(*(void**)(menu + 4)))) {
#endif
            color = 0x1EF00000;
        } else {
            color = 0x1E525F00;
        }
        x = (*(s16*)(item + 0x54) -
             (s16)(GSmsgGetRect(PC_PTR32_IMPLICIT(ITEM_SLOT_4C)) >> 16)) / 2;
        y = (*(s16*)(item + 0x56) -
             (s16)(u16)GSmsgGetRect(PC_PTR32_IMPLICIT(ITEM_SLOT_4C))) / 2 - 2;
        GSmsgPrintColor(x, y, color | menu[0x8B], PC_PTR32_IMPLICIT(ITEM_SLOT_4C));
    }
}

typedef struct MenuSequenceMessageState {
    u8 pad0;
    s8 mode;
    s8 initialized;
    u8 pad3;
    s32 menuId;
    u8 pad8[0x8D];
    s8 selection;
    u8 pad96;
    s8 previousSelection;
} MenuSequenceMessageState;

#ifdef TARGET_PC
/* OrreHook_FieldMenuItem (docs/port/hook-points.md): the item that slides for a cursor position past the game's four
 * (PC MENU, position 4); action 0x10 slides it out, 0x15 back. Nothing registered: nothing slides, as on the GameCube */
extern s32 (*OrreHook_FieldMenuItem)(s32 selection);
static void orre_field_menu_slide(MenuSequenceMessageState* state, s32 selection, s32 action) {
    const s32 item = OrreHook_FieldMenuItem != NULL ? OrreHook_FieldMenuItem(selection) : 0;
    if (item != 0) {
        fn_801081F8(state, item, action);
    }
}
#define ORRE_FIELD_MENU_SLIDE(state, selection, action) \
    default:                                            \
        orre_field_menu_slide((state), (selection), (action)); \
        break;
#else
#define ORRE_FIELD_MENU_SLIDE(state, selection, action)
#endif

s32 fn_8000D3AC(MenuSequenceMessageState* state)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winSeqSetMenu(s32 menuId, s32 state);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801081F8(void* context, u32 messageId, s32 action);
#endif

    switch (state->mode) {
    case 0:
        if (state->initialized == 0) {
            winSeqSetMenu(state->menuId, 1);
            if (state->menuId == 0x41) {
                switch (state->selection) {
                case 0:
                    fn_801081F8(state, 0x65, 0x10);
                    break;
                case 1:
                    fn_801081F8(state, 0xDB0, 0x10);
                    break;
                case 2:
                    fn_801081F8(state, 0xDB1, 0x10);
                    break;
                case 3:
                    fn_801081F8(state, 0xDB2, 0x10);
                    break;
                ORRE_FIELD_MENU_SLIDE(state, state->selection, 0x10)
                }
            } else if (state->menuId == 0x109) {
                switch (state->selection) {
                case 0:
                    fn_801081F8(state, 0x12A9, 0x10);
                    break;
                case 1:
                    fn_801081F8(state, 0x12AA, 0x10);
                    break;
                case 2:
                    fn_801081F8(state, 0x12AB, 0x10);
                    break;
                }
            }
            state->initialized = 1;
        }
        break;
    case 2:
        if (state->previousSelection != state->selection) {
            if (state->menuId == 0x41) {
                switch (state->selection) {
                case 0:
                    fn_801081F8(state, 0x65, 0x10);
                    break;
                case 1:
                    fn_801081F8(state, 0xDB0, 0x10);
                    break;
                case 2:
                    fn_801081F8(state, 0xDB1, 0x10);
                    break;
                case 3:
                    fn_801081F8(state, 0xDB2, 0x10);
                    break;
                ORRE_FIELD_MENU_SLIDE(state, state->selection, 0x10)
                }
                switch (state->previousSelection) {
                case 0:
                    fn_801081F8(state, 0x65, 0x15);
                    break;
                case 1:
                    fn_801081F8(state, 0xDB0, 0x15);
                    break;
                case 2:
                    fn_801081F8(state, 0xDB1, 0x15);
                    break;
                case 3:
                    fn_801081F8(state, 0xDB2, 0x15);
                    break;
                ORRE_FIELD_MENU_SLIDE(state, state->previousSelection, 0x15)
                }
            } else if (state->menuId == 0x109) {
                switch (state->selection) {
                case 0:
                    fn_801081F8(state, 0x12A9, 0x10);
                    break;
                case 1:
                    fn_801081F8(state, 0x12AA, 0x10);
                    break;
                case 2:
                    fn_801081F8(state, 0x12AB, 0x10);
                    break;
                }
                switch (state->previousSelection) {
                case 0:
                    fn_801081F8(state, 0x12A9, 0x15);
                    break;
                case 1:
                    fn_801081F8(state, 0x12AA, 0x15);
                    break;
                case 2:
                    fn_801081F8(state, 0x12AB, 0x15);
                    break;
                }
            }
        }
        break;
    case 3:
        if (state->initialized == 0) {
            winSeqSetMenu(state->menuId, 7);
            state->initialized = 1;
        }
        break;
    }
    return 0;
}

typedef struct MenuPendingEvent {
    u8 pad0[0x30];
    u16 eventId;
    u8 pad32[2];
} MenuPendingEvent;

#ifdef TARGET_PC
/* OrreHook_FieldMenuChoice (docs/port/hook-points.md): a pause-menu choice past the game's own (PC MENU, choice 4).
 * The hook says what it is; the game's own PC menu room (0x391, pcbox_menu: POKéMON Storage, Item Storage, Save) is
 * opened here the way the P★DA's is (menuPdaOpen), with the pause menu closed and the field's animations running while
 * it shows, as for the game's own entries. Returns 1 when handled (the pause menu then opens again). Nothing
 * registered: 0, and the menu is left as on the GameCube. Unlike a PC in a Pokémon Center, the "return map" (where
 * Continue and a whiteout put the player, script command 128) is left alone, so a whiteout still goes to the last
 * Pokémon Center */
extern s32 (*OrreHook_FieldMenuChoice)(s32 choice);

/* OrreHook_FieldMenuSpot: where the player stands as PC MENU opens the PC (read before the PC's room is pushed, which
 * becomes the current room). Only a field room (kind 1) with no story area's own pending return point (+0x51, script
 * command 161: the Lab, The Under, the Snagem hideout...) gives a spot to resume; otherwise room 0, and such a save goes
 * on from the last Pokémon Center as on the GameCube */
static void orre_field_menu_spot(void) {
    extern void* pc_lowmem_alloc(size_t size);
    extern void heroMoveGetHeroPos(u32 param);
    extern void heroMoveGetHeroRot(u32 param);
    extern u32 floorGetNextPosIndex(void);
    extern void* floorDataBiosGetPtr(u32 key);
    extern u8 floorDataBiosGetFloorKind(u8* ptr);
    extern u8 _floorChangeInfo[];
    static f32* vec = NULL; /* below 4 GB: the hero getters take a 32-bit address */
    OrreFieldSpot spot;
    if (OrreHook_FieldMenuSpot == NULL) {
        return;
    }
    memset(&spot, 0, sizeof(spot));
    if (vec == NULL) {
        vec = (f32*)pc_lowmem_alloc(sizeof(f32) * 6);
    }
    spot.room = std092_getCurrentRoomID();
    if (vec == NULL || *(u8*)(_floorChangeInfo + 0x51) != 0 ||
        floorDataBiosGetFloorKind((u8*)floorDataBiosGetPtr(spot.room)) != 1) {
        spot.room = 0;
    } else {
        heroMoveGetHeroPos((u32)(uintptr_t)vec);
        heroMoveGetHeroRot((u32)(uintptr_t)(vec + 3));
        spot.prevRoom = floorGetPrevFloorID();
        spot.entry = floorGetNextPosIndex();
        spot.x = vec[0], spot.y = vec[1], spot.z = vec[2];
        /* the facing in whole degrees, as floorChangePos takes it (radians from the model's rotation) */
        spot.facingDegrees = (s32)(vec[4] * 57.29578f + (vec[4] < 0.0f ? -0.5f : 0.5f));
    }
    OrreHook_FieldMenuSpot(&spot);
}

static s32 orre_field_menu_choice(s32 menuId, s32 choice) {
    extern void GSmodelAllPauseAnimation(void);
    extern void GSmodelAllUnpauseAnimation(void);
    extern void std098_mapWarpToMenuRoom(u32 floorId);
    extern u8 _floorChangeInfo[];
    if (OrreHook_FieldMenuChoice == NULL || OrreHook_FieldMenuChoice(choice) != 1) {
        return 0;
    }
    /* the PC's Save writes the return map (+0x3C room, +0x40 the room before it, +0x44 the entry) into the save. Only
     * a PC's script sets it (command 128), and loading a save doesn't, so after Continue it is 0 until a real PC is
     * used: a save made here would then hold room 0 (Continue and whiteouts wouldn't warp). It gets the loaded save's
     * own return point (the last Pokémon Center), which is what the save already says, so whiteouts are unchanged.
     * Written directly, not through command 128, which also clears a story area's own return point (+0x51) */
    if (*(u32*)(_floorChangeInfo + 0x3C) == 0) {
        const u32 room = gamedatasaveGetStatus(NULL, 5);
        if (room == 0) {
            /* no return point anywhere (a new game not yet saved at a PC): the PC isn't opened from here */
            printf("[field_menu] PC MENU: no return point yet (no save at a PC): the PC menu isn't opened\n");
            return 1;
        }
        *(u32*)(_floorChangeInfo + 0x3C) = room;
        *(u32*)(_floorChangeInfo + 0x40) = gamedatasaveGetStatus(NULL, 7);
        *(u8*)(_floorChangeInfo + 0x44) = (u8)gamedatasaveGetStatus(NULL, 8);
    }
    orre_field_menu_spot();
    menuClose(menuId);
    GSmodelAllUnpauseAnimation();
    std098_mapWarpToMenuRoom(0x391);
    floorSetFadeScript(0, 0);
    _threadSwitch();
    GSmodelAllPauseAnimation();
    if (OrreHook_FieldMenuSpot != NULL) {
        OrreHook_FieldMenuSpot(NULL); /* the PC menu room has closed */
    }
    {
        /* OrreHook_FieldMenuReopen (docs/port/hook-points.md; 2026-11-15): 0 while the game is on its way from the
         * PC screen's Main Menu to the title's main menu, so the pause menu closes the game's own way (as after B; the
         * caller's else branch) and the room change can follow (OrreHook_RoomLeave, gs_floor.c). Nothing registered:
         * it opens again */
        extern s32 (*OrreHook_FieldMenuReopen)(void);
        if (OrreHook_FieldMenuReopen != NULL && OrreHook_FieldMenuReopen() == 0) {
            return 0;
        }
    }
    return 1;
}
#endif

s32 heroMoveRunEvent(u8 mode)
{
    extern s32 heroMoveCheckEvent(MenuPendingEvent* events);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroDispatchEvent(s8 kind, u32 eventId);
#endif
    extern void heroMoveInitEvent(void);
    extern void mailMainReceiveTerminate(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
    extern void GSmodelAllPauseAnimation(void);
    extern void GSmodelAllUnpauseAnimation(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_800D37CC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCreateOffScreen(f32 rate);
#endif
    extern u8 menuGetOffScreenFlag(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundDuck(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800D3074(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std082_flagGetValue(u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* floorGetCurrentContext(void);
#endif
    extern u32 GSeffectGetUpdateGapp(void);
    extern void GSgappBlock(u32 task);
    extern void GSgappUnblock(u32 task);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSthreadBlockGroup(void* stack);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSthreadCreate(s32 priority, void* stack, u32 stackSize,
                               s32 usesFpu, s32 group, void* entry);
#endif
    extern void fn_8000DAB0(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 menuId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonOpen(s32, s32, s32);
#endif
    extern void menuPdaOpen(void);
    extern u16 fn_80018F88(s32, u32* value, s32);
    extern u32 fn_80019070(u16 handle);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroMoveAddAutoEvent(u32 eventId, u32 handle, u32 value,
                                     s32, s32);
#endif
    extern s32 std223_askYesNoThenWarp395(void);
    extern void menuOffScreenRelease(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundUnduck(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuReleaseOffScreen(f32 rate);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32 menuId, s32 wait);
#endif
    extern void heroMoveTermEvent(void);
    extern u8 lbl_8047A2A0;
    extern s32 lbl_8047A2A4;
    extern void* lbl_8047A2A8;
    extern void* lbl_8047A2AC;
    extern s32 lbl_8047A2B0;
    extern const f32 lbl_8047B6F0;
    MenuPendingEvent events[4];
    u32 eventValue;
    u16 selectionHandle;
    u32 eventId;
    s32 eventCount;
    s32 menuId;
    s32 i;

    eventCount = heroMoveCheckEvent(events);
    for (i = 0; i < eventCount; i++) {
        heroDispatchEvent(4, events[i].eventId);
    }
#ifdef TARGET_PC
    /* OrreHook_PauseMenuStep, step 0 (step 3.7's Relic Forest shortcuts; pc/orre_hooks.h): the pause menu opens, right
     * after the examine events that fill the "usable now" list (the Time Flute anywhere in the forest adds its entry
     * here, as the stone's function 11 does). NULL: nothing. All platforms */
    if (OrreHook_PauseMenuStep != NULL) {
        OrreHook_PauseMenuStep(0, NULL, NULL);
    }
#endif
    heroMoveInitEvent();
    mailMainReceiveTerminate();
    _threadSwitch();
    GSmodelAllPauseAnimation();
    lbl_8047A2A0 = 1;
    menuCreateOffScreen(lbl_8047B6F0 / (f32)fn_800D37CC());
    while (!menuGetOffScreenFlag()) {
        _threadSwitch();
    }

    soundDuck(0x55, 0x1F4, 1, 1);
    fn_800D3074(1);
    if (std082_flagGetValue(0x8AE) == 0) {
        menuId = 0x41;
    } else {
        menuId = 0x109;
    }

#ifdef TARGET_PC
    {
        /* OrreHook_FieldMenuOpen (docs/port/hook-points.md): OrrePort lays out its PC MENU entry (menu 0x41), or puts
         * the game's own items back. Nothing registered: the menu as on the GameCube */
        extern void (*OrreHook_FieldMenuOpen)(s32 menuId);
        if (mode == 0 && OrreHook_FieldMenuOpen != NULL) {
            OrreHook_FieldMenuOpen(menuId);
        }
    }
#endif
    switch (mode) {
    case 0:
        if (std082_flagGetValue(0x8AE) == 0) {
            for (;;) {
                fn_800D3074(1);
                lbl_8047A2A4 = menuId;
                lbl_8047A2A8 = PC_PTR(floorGetCurrentContext());
                GSgappBlock(GSeffectGetUpdateGapp());
                GSthreadBlockGroup(PC_PTR32_IMPLICIT(lbl_8047A2A8));
                lbl_8047A2AC = (u8*)lbl_8047A2A8 - 0x20;
                GSthreadCreate(0xF, PC_PTR32_IMPLICIT(lbl_8047A2AC), 0x4000, 1, 1,
                               fn_8000DAB0);
                _threadSwitch();
                GSgappUnblock(GSeffectGetUpdateGapp());

                if (lbl_8047A2B0 == 0) {
                    menuClose(menuId);
                    GSmodelAllUnpauseAnimation();
#ifdef TARGET_PC
                    /* OrreHook_PauseMenuStep, steps 1 and 2 (step 3.7, PURIFY in the party menu; pc/orre_hooks.h): the
                     * party screen opens from POKéMON; when it closes, the hook may ask to leave the pause menu and
                     * queue a script event for the hero step (PURIFY: the Relic Stone's own examine event), the way
                     * the ITEMS entry queues an item's event below. NULL: the pause menu opens again, as on the
                     * GameCube. All platforms */
                    if (OrreHook_PauseMenuStep != NULL) {
                        OrreHook_PauseMenuStep(1, NULL, NULL);
                    }
                    menuPokemonOpen(1, 0, 0);
                    GSmodelAllPauseAnimation();
                    if (OrreHook_PauseMenuStep != NULL) {
                        u32 orreEvent = 0;
                        u32 orreValue = 0;
                        if (OrreHook_PauseMenuStep(2, &orreEvent, &orreValue) != 0 && orreEvent != 0) {
                            heroMoveAddAutoEvent(orreEvent, 0, orreValue, 0, 0);
                            break;
                        }
                    }
#else
                    menuPokemonOpen(1, 0, 0);
                    GSmodelAllPauseAnimation();
#endif
                } else if (lbl_8047A2B0 == 1) {
                    menuClose(menuId);
                    GSmodelAllUnpauseAnimation();
                    menuPdaOpen();
                    GSmodelAllPauseAnimation();
                } else if (lbl_8047A2B0 == 2) {
                    menuClose(menuId);
                    GSmodelAllUnpauseAnimation();
                    selectionHandle = fn_80018F88(0, &eventValue, 0);
                    GSmodelAllPauseAnimation();
                    if (selectionHandle != 0) {
                        eventId = fn_80019070(selectionHandle);
                        if (eventId != -1) {
                            heroMoveAddAutoEvent(eventId, selectionHandle,
                                                 eventValue, 0, 0);
                        }
                        break;
                    }
#ifdef TARGET_PC
                } else if (lbl_8047A2B0 > 2 && orre_field_menu_choice(menuId, lbl_8047A2B0)) {
                    /* an entry OrrePort added (PC MENU): its screen has closed, the pause menu opens again */
#endif
                } else {
                    break;
                }
            }
        } else {
            for (;;) {
                fn_800D3074(1);
                lbl_8047A2A4 = menuId;
                lbl_8047A2A8 = PC_PTR(floorGetCurrentContext());
                GSgappBlock(GSeffectGetUpdateGapp());
                GSthreadBlockGroup(PC_PTR32_IMPLICIT(lbl_8047A2A8));
                lbl_8047A2AC = (u8*)lbl_8047A2A8 - 0x20;
                GSthreadCreate(0xF, PC_PTR32_IMPLICIT(lbl_8047A2AC), 0x4000, 1, 1,
                               fn_8000DAB0);
                _threadSwitch();
                GSgappUnblock(GSeffectGetUpdateGapp());

                if (lbl_8047A2B0 == 0) {
                    menuClose(menuId);
                    menuPokemonOpen(1, 0, 0);
                } else if (lbl_8047A2B0 == 1) {
                    if (std223_askYesNoThenWarp395() == 1) {
                        menuOffScreenRelease();
                        lbl_8047A2A0 = 0;
                        _threadSwitch();
                    }
                } else {
                    break;
                }
            }
        }
        break;
    case 1:
        selectionHandle = fn_80018F88(0, &eventValue, 0);
        if (selectionHandle != 0) {
            eventId = fn_80019070(selectionHandle);
            if (eventId != -1) {
                heroMoveAddAutoEvent(eventId, selectionHandle, eventValue,
                                     0, 0);
            }
        }
        break;
    case 2:
        break;
    }

#ifdef TARGET_PC
    /* OrreHook_PauseMenuStep, step 3 (step 3.7's Relic Forest shortcuts; pc/orre_hooks.h): the pause menu closes (the
     * "usable now" list is cleared just below). Observation only. NULL: nothing. All platforms */
    if (OrreHook_PauseMenuStep != NULL) {
        OrreHook_PauseMenuStep(3, NULL, NULL);
    }
#endif
    fn_80019070(selectionHandle);
#ifdef TARGET_PC
    {
        /* OrreHook_FieldMenuUnduckMusic (docs/port/hook-points.md; the project's helper 2026-11-16): 0 = the closing pause menu
         * brings back only the effects' volume and leaves the music at its ducked level, because the field music is
         * about to be replaced (the PC screen's Main Menu → YES). Why: this unduck raised the field music from 85 to 127
         * over 500 ms behind the black screen just before the title's song cut it (snatcherLeo heard it swell; measured +5 dB
         * in the mix). The music's duck is cleared when the title's song replaces it (the slot's release resets the
         * stack). Nothing registered, or nonzero: music and effects, as on the GameCube. All platforms */
        extern s32 (*OrreHook_FieldMenuUnduckMusic)(void);
        soundUnduck(0x1F4, OrreHook_FieldMenuUnduckMusic == NULL || OrreHook_FieldMenuUnduckMusic() != 0, 1);
    }
#else
    soundUnduck(0x1F4, 1, 1);
#endif
    menuClose(menuId);
    menuReleaseOffScreen(lbl_8047B6F0 / (f32)fn_800D37CC());
    menuCloseSync(menuId, 1);
    fn_800D3074(2);
    heroMoveTermEvent();
    GSmodelAllUnpauseAnimation();
    lbl_8047A2A0 = 0;
    return 0;
}
