/**
 * @file gs_task.c
 * @brief GStask -- Scene/FSYS task management and archive loading callbacks.
 *
 * Address range: 0x80006630 - 0x80009178 (~45 functions)
 *
 * This module manages the task system that sits between the main loop and
 * the individual game subsystems. It handles:
 *   - FSYS archive loading via task callbacks (fightTrainerGetStatus)
 *   - Scene transition sequencing with menuOpenCustom dispatch
 *   - Archive completion callbacks with scene ID routing
 *   - Resource group management (models, textures, scripts)
 *
 * The functions follow a clear pattern:
 *   1. Load an SDA global (lbl_8047A288) for the current scene/map ID
 *   2. Call fightTrainerGetStatus to initiate an archive load with a priority level
 *   3. On success, call winMsgOpenFight and winMsgCloseFight to activate the loaded data
 *   4. Return 1 on success, -1 on failure
 *
 * Key functions:
 *   dbgMenuCameraResetFloor  (calls GSscene_CameraSetPosition; decompiled under its real name below)
 *   dbgMenuCameraChangeDisp  (loads archive slot 6 / topmenu; decompiled under its real name below)
 *   dbgMenuCameraSetType  (dispatches to GSscene_SetMode based on mode; decompiled below --
 *                          retail symbol name is a reused debug-menu name)
 *   fn_80006724  (loads archive slot 6, priority 3 / pda_menu; decompiled below)
 *   fn_8000677C  (loads archive slot 6, priority 2 / pocket_menu; decompiled below)
 *   fn_800067D4  (loads archive slot 6, priority 1 / pcbox_menu; decompiled below)
 *   fn_8000682C  (loads archive slot 6, priority 0 / battle; decompiled below)
 *   fn_80006884  GStask_SelectRandomNPC     -- random NPC selection from scene data
 *   fn_80006908  GStask_ProcessSceneEvent   -- 0x6A4 bytes, large scene event dispatcher
 *   fn_80006FAC  GStask_ProcessEventResult  -- handles event return codes
 *   dbgMenuFightStopTimeOut  GStask_GetField1           -- struct field accessor (offset 0x00)
 *   dbgMenuFightStopHostLose  GStask_GetField2           -- struct field accessor (offset 0x04)
 *   dbgMenuFightStopHostWin  GStask_GetField3           -- struct field accessor (offset 0x08)
 *   dbgMenuFightFightPokemonSelect5  GStask_LoadSlot0           -- archive load helper, slot 0
 *   dbgMenuFightFightPokemonSelect4  GStask_LoadSlot1           -- archive load helper, slot 1
 *   dbgMenuFightFightPokemonSelect3  GStask_LoadSlot2           -- archive load helper, slot 2
 *   dbgMenuFightFightPokemonSelect2  GStask_LoadSlot3           -- archive load helper, slot 3
 *   dbgMenuFightFightPokemonSelect1  GStask_LoadSlot4           -- archive load helper, slot 4
 *   dbgMenuFightFightPokemonSelect0  GStask_LoadSlot5           -- archive load helper, slot 5
 *   menuFightPokemonSelectSub  GStask_InitSceneResources  -- 0x2F8 bytes, resource init
 *   dbgMenuFightFightTrainerPokemonPartDataEdit  GStask_ShutdownResources   -- resource cleanup
 *   dbgMenuFightFightTrainerAiDataEdit  GStask_UpdateLoadState     -- resource loading state machine
 *   dbgMenuFightFightTrainerAiAddsubCombo  GStask_GetLoadFlag0        -- small accessor (0x20 bytes)
 *   dbgMenuFightFightTrainerAiAddsubItem  GStask_GetLoadFlag1        -- small accessor
 *   dbgMenuFightFightTrainerAiAddsubWazaUsewaza  GStask_GetLoadFlag2        -- small accessor
 *   dbgMenuFightFightTrainerAiAddsubWazaDefpokemon  GStask_GetLoadFlag3        -- small accessor
 *   dbgMenuFightFightTrainerAiAddsubWaza  GStask_ReturnZero          -- stub, returns 0
 *   dbgMenuFightFightTrainerAiAddsubIrekaeDasu  GStask_GetLoadFlag4        -- small accessor
 *   dbgMenuFightFightTrainerAiAddsubIrekaeModosu  GStask_GetLoadFlag5        -- small accessor
 *   dbgMenuFightFightTrainerAiAddsubIrekae  GStask_ReturnZero2         -- stub, returns 0
 *   dbgMenuFightFightTrainerAiAddsubDataEdit  GStask_ValidateScene       -- validates scene index bounds
 *   dbgMenuFightFightTrainerSelect1  GStask_ArchiveCB0          -- archive completion callback
 *   dbgMenuFightFightTrainerSelect0  GStask_ArchiveCB1          -- archive completion callback
 *   dbgMenuFightFightSideSelectHostEnemy  GStask_ArchiveCB2          -- archive completion callback
 *   dbgMenuFightFightSideSelectHost  GStask_ArchiveCB3          -- archive completion callback
 *   dbgMenuFightFightFloorEditTenkou  GStask_ArchiveCB4          -- archive completion callback
 *   dbgMenuFightWazaEdit  GStask_DispatchArchiveCBs  -- routes to the above callbacks
 *   dbgMenuFightFlagEdit  GStask_MainUpdate          -- 0x4AC bytes, main task state machine
 *   dbgMenuFightStop  GStask_GetSceneCount       -- return scene table size
 *   dbgMenuFightFightFloorEdit  GStask_GetEventCount       -- return event table size
 *   fn_8000804C  GStask_SetupTransition     -- scene transition setup (0xF8 bytes)
 *   dbgMenuFightMain  GStask_Accessor0           -- 8-byte accessor (lwz/blr)
 *   dbgMenuFightGetYubiwohuruWazaDataId  GStask_Accessor1           -- 8-byte accessor
 *   dbgMenuFightGetSoubikoukaFlag  GStask_Accessor2           -- 8-byte accessor
 *   dbgMenuFightGetFightPokeStMenuOpenFlag  GStask_Accessor3           -- 8-byte accessor
 *   dbgMenuFightGetFightTrainerAiFlowDispFlag  GStask_Accessor4           -- 8-byte accessor
 *   dbgMenuFightGetFightDebugMenuOpenFlag  GStask_Accessor5           -- 8-byte accessor
 *   dbgMenuFightGetAllFightTrainerOperateFlag  GStask_Accessor6           -- 8-byte accessor
 *   dbgMenuFightGetTuikakoukaFlag  GStask_Accessor7           -- 8-byte accessor
 *   dbgMenuFightGetMsgSpeedToFrame  GStask_FinalizeLoad        -- 0xC0 bytes, finalize resource load
 *   _dbgMenuFightGetFightTrainerAiAddsubValueDataIdSub  GStask_BuildResourceList   -- 0x14C bytes, compile resource table
 *
 * Code patterns:
 *   - SDA globals lbl_8047A288 (current scene halfword), fight_encount_data_number (the battle table's count pointer)
 *   - menuOpenCustom is the main event dispatch (takes slot, params, flags)
 *   - menuCloseCustom is the event cancel/cleanup function
 *   - menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs is a bounded random selection helper
 *   - menuSubCloseNumberInput is a random result commit function
 */

#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/disc_types.h" /* DISC_STRUCT: common_rel tables are big-endian on the PC */
#include "game/constants/pokemon_status.h"

/* =========================================================================
 * External declarations
 * ========================================================================= */

/* Scene system */
extern void  cameraResetFloor(void);                   /* GSscene_CameraSetPosition */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSscene_SetMode(s32 mode);               /* GSscene_SetMode */
#endif

/* FSYS archive loading */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTrainerGetStatus(s32 slot, u16 sceneId, s32 priority, s32 group);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   winMsgOpenFight(void* handle, s32 a, s32 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   winMsgCloseFight(s32 slot);
#endif

/* Event dispatch */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    menuIsCheck(s32 slot);               /* Check if event slot busy */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   menuClose(s32 slot);               /* Cancel active event */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   menuOpenCustom(s32 slot, ...);           /* Dispatch scene event */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  menuCloseCustom(s32 slot, s32 p1, s32 p2);  /* Event cleanup */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   menuGetCursorItemID(s32 slot);               /* Get event result code */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32   menuOpen(s32 slot, s32 p1);       /* Query event state */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  menuSetPosition(s32 slot, s32 p1, s32 p2);  /* Set event params */
#endif

/* Resource management */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightEncountDataBiosGetPtr(u16 id);                   /* Get scene resource table */
#endif
extern void  fightEncountDataBiosSetFightKind(void* tbl, u8 idx);      /* Set resource field 0 */
extern void  fightEncountDataBiosSetTrainer(void* tbl, u8 idx);      /* Set resource field 1 */
extern void  fightEncountDataBiosSetFightFloorDataId(void* tbl, u16 val);     /* Set resource field 2 */
extern u16   fightEncountDataBiosGetFightTrainerDataId(void* tbl, s32 slot);    /* Get resource subfield */
extern void  fightEncountDataBiosSetFightTrainerDataId(void* tbl, s32 slot, u16 val);  /* Set resource subfield */
extern void  fightEncountDataBiosSetGSInputDevice(void* tbl, s32 slot, s32 val);  /* Set resource property */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fightEncountDataBiosGetFightKind(void* tbl);              /* Get resource type */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fightEncountDataBiosGetTrainer(void* tbl);              /* Get resource subtype */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   fightEncountDataBiosGetFightFloorDataId(void* tbl);              /* Get resource size */
#endif
extern u16   fightEncountDataBiosGetGSInputDevice(void* tbl, s32 slot);    /* Get resource slot value */

/* Random selection helpers */
extern u8    menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(volatile unsigned int count, u32* outResult, void* validationFn);
extern void  menuSubCloseNumberInput(void);

/* Callback function pointers used in validation */
extern void* _dbgMenuFightGetFightKindDataIdSub(u32 id);   /* Validate NPC index (type byte) */
extern void* fn_800086EC(u32 id);   /* Validate NPC index (type byte) */
extern void* _dbgMenuFightGetFightFloorDataIdSub(u32 id);   /* Validate NPC index (type u16) */
extern void* fn_8000868C(u32 id);   /* Validate scene item index */
extern void* fn_800085D8(s32 difficulty);   /* Validate encounter difficulty */
extern void* _dbgMenuFightGetZokuseiDataIdSub(u32 id);
extern void* fn_800087FC(u32 id);
extern u8 fightIsBattleActive(void);
extern u32 _target_fight_trainer_ptr;
extern u32 fight_type_data_number;
extern u32 fight_kind_data_number;
extern u32 fight_floor_data_number;
s32 fn_80006908(u16 id);

/* =========================================================================
 * SDA global variables
 * ========================================================================= */

extern u16  gCurrentSceneId;     /* lbl_8047A288 : .sbss -- current scene/map halfword */
extern u16  lbl_8047A288;
/* The g* declarations below are unused early guesses (nothing references them). Corrected 2026-11-08: XD's map names
 * the variables they described: fight_encount_data_number (the battle table's count), fight_kind_data_number,
 * fight_floor_data_number and fight_type_data_number (count pointers for the battle-kind, arena and battle-type
 * tables, set when common_rel loads). */
extern void* gSceneTable;       /* was a guess for fight_encount_data_number */
extern void* gSceneItemTable;   /* lbl_80478F20 : .sbss -- scene item lookup table ptr */
extern void* gNPCIndexTable;    /* was a guess for fight_kind_data_number */
extern void* gNPCSubTable;      /* was a guess for fight_floor_data_number */
extern void* gEncounterTable;   /* was a guess for fight_type_data_number */

/* =========================================================================
 * Rodata references
 * ========================================================================= */

/* lbl_802666B0: Shift-JIS encoded scene name string (unknown use) */
/* lbl_802666E0: Scene type mapping table (8 entries, 2x u32 each) */
/* lbl_80266700: Scene resource ID mapping table (14 entries) */
/* jumptable_802E28D0: Jump table for fn_80006908 scene event dispatch */

#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_EXACT_80006630)
/* =========================================================================
 * Function: dbgMenuCameraResetFloor (orphan fiction "GStask_InitCamera" renamed --
 *           address 0x80006630 / size 0x24 matches config/GC6E01/symbols.txt
 *           exactly, and this was the only code in the tree covering that
 *           address; the unmatched target had no other candidate)
 * Address:  0x80006630
 * Size:     0x24
 *
 * Simple wrapper that calls GSscene_CameraSetPosition to reset the
 * camera, then returns 0. Called at the start of scene transitions.
 * ========================================================================= */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; scheduling off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma scheduling off
s32 dbgMenuCameraResetFloor(void) {
    cameraResetFloor();
    return 0;
}
#pragma pop

/* =========================================================================
 * Function: dbgMenuCameraChangeDisp (orphan fiction "GStask_LoadTopMenu" renamed --
 *           address/size match symbols.txt exactly)
 * Address:  0x80006654
 * Size:     0x70
 *
 * Checks if archive slot 6 is busy; if so, cancels it and returns 0.
 * Otherwise, dispatches a new archive load for the top menu FSYS with
 * default parameters and sets up the viewport at (0x14, 0x104).
 * ========================================================================= */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; scheduling off; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma scheduling off
#pragma peephole off
s32 dbgMenuCameraChangeDisp(void) {
    if (menuIsCheck(6) != 0) {
        menuClose(6);
    } else {
        menuOpenCustom(6, 0, 0, 0, 1, 0);
        menuSetPosition(6, 0x14, 0x104);
    }
    return 0;
}
#pragma pop

/* =========================================================================
 * Function: dbgMenuCameraSetType (orphan fiction "GStask_SetSceneType"
 *           renamed -- address 0x800066C4 / size 0x60 matches
 *           config/GC6E01/symbols.txt exactly; the retail symbol name is a
 *           reused debug-menu name, not this function's real purpose)
 * Address:  0x800066C4
 * Size:     0x60
 *
 * Given a mode parameter in r4, dispatches to GSscene_SetMode with the
 * appropriate scene type constant:
 *   mode 0 or >=3: type 0 (normal overworld)
 *   mode 1:        type 5 (battle arena)
 *   mode 2:        type 6 (cutscene)
 * Always returns 0.
 * ========================================================================= */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; scheduling off; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma scheduling off
#pragma peephole off
s32 dbgMenuCameraSetType(void* unused, s32 mode) {
    switch (mode) {
    case 0:
    default:
        GSscene_SetMode(0);
        break;
    case 1:
        GSscene_SetMode(5);
        break;
    case 2:
        GSscene_SetMode(6);
        break;
    }
    return 0;
}
#pragma pop

#endif /* GS_TASK_RANGE_EXACT_80006630 */

#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_RESIDUAL_80006724)
/* =========================================================================
 * Function: fn_80006724 (orphan fiction "GStask_LoadPDAMenu" renamed --
 *           address/size match symbols.txt exactly)
 * Address:  0x80006724
 * Size:     0x58
 *
 * Loads the PDA menu archive (priority 3) into the FSYS system.
 * Uses the current scene ID from lbl_8047A288 as a context parameter.
 * On success, activates the archive and returns 1; on failure returns -1.
 * ========================================================================= */
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; scheduling off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma scheduling off
s32 fn_80006724(void) {
    extern u16 lbl_8047A288;
    void* result;
    u16 sceneId;

    sceneId = *(volatile u16*)&lbl_8047A288;
    result = PC_PTR(fightTrainerGetStatus(0, sceneId, 8, 3));
    if (result == NULL) {
        return -1;
    }
    winMsgOpenFight(PC_PTR32_IMPLICIT(result), 1, 1);
    winMsgCloseFight(1);
    return 1;
}

/* =========================================================================
 * Function: fn_8000677C (orphan fiction "GStask_LoadPocketMenu" renamed --
 *           address/size match symbols.txt exactly)
 * Address:  0x8000677C
 * Size:     0x58
 *
 * Loads the pocket/bag menu archive (priority 2).
 * Same structure as fn_80006724 but with priority 2.
 * ========================================================================= */
s32 fn_8000677C(void) {
    extern u16 lbl_8047A288;
    void* result;
    u16 sceneId;

    sceneId = *(volatile u16*)&lbl_8047A288;
    result = PC_PTR(fightTrainerGetStatus(0, sceneId, 8, 2));
    if (result == NULL) {
        return -1;
    }
    winMsgOpenFight(PC_PTR32_IMPLICIT(result), 1, 1);
    winMsgCloseFight(1);
    return 1;
}

/* =========================================================================
 * Function: fn_800067D4 (orphan fiction "GStask_LoadPCBoxMenu" renamed --
 *           address/size match symbols.txt exactly)
 * Address:  0x800067D4
 * Size:     0x58
 *
 * Loads the PC box menu archive (priority 1).
 * ========================================================================= */
s32 fn_800067D4(void) {
    extern u16 lbl_8047A288;
    void* result;
    u16 sceneId;

    sceneId = *(volatile u16*)&lbl_8047A288;
    result = PC_PTR(fightTrainerGetStatus(0, sceneId, 8, 1));
    if (result == NULL) {
        return -1;
    }
    winMsgOpenFight(PC_PTR32_IMPLICIT(result), 1, 1);
    winMsgCloseFight(1);
    return 1;
}

/* =========================================================================
 * Function: fn_8000682C (orphan fiction "GStask_LoadBattleMenu" renamed --
 *           address/size match symbols.txt exactly)
 * Address:  0x8000682C
 * Size:     0x58
 *
 * Loads the battle menu archive (priority 0 -- highest).
 * ========================================================================= */
s32 fn_8000682C(void) {
    extern u16 lbl_8047A288;
    void* result;
    u16 sceneId;

    sceneId = *(volatile u16*)&lbl_8047A288;
    result = PC_PTR(fightTrainerGetStatus(0, sceneId, 8, 0));
    if (result == NULL) {
        return -1;
    }
    winMsgOpenFight(PC_PTR32_IMPLICIT(result), 1, 1);
    winMsgCloseFight(1);
    return 1;
}
#pragma pop

#endif /* GS_TASK_RANGE_RESIDUAL_80006724 */

typedef struct GsTaskCountTable {
    u32 count;
} GsTaskCountTable;

extern GsTaskCountTable* lbl_80478F20;

#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_EXACT_80006884)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 fn_80006884(void) {
    u32 result;
    s32 val;
    s32 ret;
    u16 selected;

    if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(lbl_8047A288, &result, fn_8000868C) == 0) {
        val = -1;
    } else {
        if (result >= lbl_80478F20->count) {
            result = lbl_80478F20->count - 1;
        }
        menuSubCloseNumberInput();
        val = (s32)result;
    }

    if (val < 0) {
        return -1;
    }

    selected = (u16)val;
    ret = 1;
    lbl_8047A288 = selected;
    return ret;
}
#pragma pop
#endif /* GS_TASK_RANGE_EXACT_80006884 */

/* =========================================================================
 * Remaining functions in this module (0x80006884 - 0x80009178) are not
 * fully decompiled here. They follow the same patterns:
 *
 * fn_80006884 (GStask_SelectRandomNPC):
 *   Uses menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs to randomly select an NPC from the scene table,
 *   bounds-checks against lbl_80478F20, stores result in lbl_8047A288.
 *
 * fn_80006908 (GStask_ProcessSceneEvent):
 *   The largest function (0x6A4 bytes) - a massive switch statement that
 *   dispatches scene events based on event codes returned by menuGetCursorItemID.
 *   Uses jumptable_802E28D0 for the initial dispatch, then performs
 *   NPC selection, resource loading, and encounter setup based on the
 *   event type. Each case in the switch:
 *     - Gets a resource from the scene table via fightEncountDataBiosGetFightKind/fightEncountDataBiosGetTrainer
 *     - Validates the result using menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs with a callback
 *     - Commits the selection via menuSubCloseNumberInput
 *     - Updates the resource table via fightEncountDataBiosSetFightKind/fightEncountDataBiosSetTrainer/etc.
 *
 * fn_80006FAC - dbgMenuFightStopHostWin (accessor cluster):
 *   Small field accessors for the scene resource structure.
 *
 * dbgMenuFightFightPokemonSelect5 - dbgMenuFightFightPokemonSelect0 (archive load helpers):
 *   Six nearly identical functions that load archives for slots 0-5,
 *   each following the same load/activate/commit pattern.
 *
 * menuFightPokemonSelectSub (GStask_InitSceneResources):
 *   0x2F8 bytes. Initializes the resource table for a new scene,
 *   setting up all model/texture/script entries.
 *
 * dbgMenuFightFlagEdit (GStask_MainUpdate):
 *   0x4AC bytes. The main task update state machine that coordinates
 *   archive loading, scene transitions, and event processing.
 *
 * dbgMenuFightGetMsgSpeedToFrame - _dbgMenuFightGetFightTrainerAiAddsubValueDataIdSub (finalization):
 *   Finalize loaded resources and build the active resource list.
 * ========================================================================= */

/* Accessor functions (0x80008390 - 0x80009178) follow a repeating pattern:
 *
 * _dbgMenuFightGetFightTrainerPokemonPartDataIdSub (size 0x6C) - Get party slot 0 species
 * _dbgMenuFightGetWazaDataIdSub (size 0x64) - Get party slot 0 level
 * dbgMenuFightGetWazaTypeId (size 0x60) - Get party slot 0 HP
 * _dbgMenuFightGetWazaTypeIdSub (size 0x58) - Get party slot 0 status
 * dbgMenuFightGetZokuseiDataId (size 0x64) - Get party slot 1 species
 * ...and so on for each party slot property
 *
 * These are thin wrappers that index into the party data structure,
 * extract a field, and return it. They are likely used as callback
 * function pointers passed to the UI rendering system.
 */

/* ===================================================================
 * Lookup/accessor functions
 * =================================================================== */

/* External functions used by lookup helpers */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32 id);
#endif
extern void* zokuseiDataBiosGetName(u16 index);
extern void* fightEncountDataBiosGetFightName(void* ptr);

/* Global state references */
extern u32 lbl_80478B38;

extern u8 _fight_pokemon_status_menu_open_flag;
extern u8 _tuikakouka_flag;
extern u8 _soubikakouka_flag;
extern u16 _yubiwohuru_waza_data_id;
extern u8 _all_fight_trainer_operate_flag;
extern u8 _fight_debug_menu_open_flag;
extern u8 _fight_trainer_ai_flow_disp_flag;

/* 0x80008868 | 0x3D8 */
#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_RESIDUAL_80008868)
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuDataBiosGetType(u32);
#endif
extern void windowDrawPanel(s32, s32, s32, s32);
extern void menuDrawRect(s32, s32, s32, s32, s32, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 sprintf(char*, const char*, ...);
#endif
extern s32 GSmsgPrintf(s32, s32, s32, ...);
extern u32 std082_flagGetValue(s32);
extern u8 lbl_80478838[8];
extern u8 lbl_803A19C8[];
extern u8 lbl_80266688[];
extern u8 lbl_8047B6C8[3];
typedef struct DISC_STRUCT GsTaskFlagConfig { /* common_rel flag data: big-endian view on the PC */
    u32 count;
    s16 first;
} GsTaskFlagConfig;

typedef struct DISC_STRUCT GsTaskFlagDef { /* common_rel flag data: big-endian view on the PC */
    u8 fields[6];
    s16 next;
} GsTaskFlagDef;

extern GsTaskFlagConfig* lbl_80478F98;
extern GsTaskFlagDef* lbl_80478F9C;

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
static inline u16 gsTaskFlagGetId(s32 index) {
    s16 id;
    s32 i;

    if (index < 0) {
        return 0;
    }
    if ((u32)index >= ((GsTaskFlagConfig*)PC_PTR(lbl_80478F98))->count) {
        return 0;
    }
    id = ((GsTaskFlagConfig*)PC_PTR(lbl_80478F98))->first;
    if (index != 0) {
        for (i = 0; i < index - 1; i++) {
            id = ((GsTaskFlagDef*)PC_PTR(lbl_80478F9C))[id].next;
        }
        id = ((GsTaskFlagDef*)PC_PTR(lbl_80478F9C))[id].next;
    }
    return id;
}
#else
static inline u16 gsTaskFlagGetId(s32 index) {
    s16 id;
    s32 i;

    if (index < 0) {
        return 0;
    }
    if ((u32)index >= lbl_80478F98->count) {
        return 0;
    }
    id = lbl_80478F98->first;
    if (index != 0) {
        for (i = 0; i < index - 1; i++) {
            id = lbl_80478F9C[id].next;
        }
        id = lbl_80478F9C[id].next;
    }
    return id;
}
#endif

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
/* NAME-T3 fn_80008868 */ /* draw callback of the debug flag menu (menu 0xDD) */
void dbgFlagMenuDraw(u8* window)
{
    GsTaskFlagConfig* config;
    s32 flagCount;
    u16 flagId;
    s32 right;
    volatile s32 width;
    s32 pageSize;
    s32 row;
    s32 rowHeight;
    s32 visibleCount;
    u32 menuId;
    s32 rowY;
    s32 center;
    s32 flagIndex;
    s32 color;

    menuId = *(u32*)(window + 4);
    rowY = 40;
    pageSize = (u8)menuDataBiosGetType(menuId);
    width = 0;
    width = 480;
    config = (GsTaskFlagConfig*)PC_PTR(lbl_80478F98);
    flagCount = config->count;
    visibleCount = flagCount;
    if (flagCount > pageSize) {
        visibleCount = pageSize;
    }

    rowHeight = visibleCount * 13;
    windowDrawPanel(30, 40, width + 25, rowHeight);

    if (*(s16*)lbl_80478838 > 0) {
        center = width / 2;
        menuDrawRect(center + 20, 33, center + 40, 33, center + 30, 23);
    }

    config = (GsTaskFlagConfig*)PC_PTR(lbl_80478F98);
    if ((s32)config->count - *(s16*)lbl_80478838 > pageSize) {
        center = width / 2;
        menuDrawRect(center + 20, rowHeight + 47, center + 40,
                    rowHeight + 47, center + 30, rowHeight + 57);
    }

    row = 0;
    right = width;
    while (row < visibleCount) {
        if (*(s16*)(lbl_80478838 + 2) == row) {
            color = 0xFF0000FF;
        } else {
            color = -1;
        }

        flagIndex = row + *(s16*)lbl_80478838;
        flagId = gsTaskFlagGetId(flagIndex);

        sprintf((char*)lbl_803A19C8, (const char*)lbl_80266688, flagIndex,
                (u16)flagId);
        GSmsgPrintf(30, rowY, color, lbl_803A19C8);

        flagIndex = row + *(s16*)lbl_80478838;
        flagId = gsTaskFlagGetId(flagIndex);

        GSmsgPrintf(right + 30, rowY, color, lbl_8047B6C8,
                    std082_flagGetValue((u16)flagId));
        rowY += 13;
        row++;
    }
}
#else
/* NAME-T3 fn_80008868 */
void dbgFlagMenuDraw(u8* window)
{
    GsTaskFlagConfig* config;
    s32 flagCount;
    s32 width;
    s32 pageSize;
    s32 row;
    s32 rowHeight;
    s32 visibleCount;
    u32 menuId;
    s32 rowY;
    s32 center;
    s32 flagIndex;
    s32 color;

    menuId = *(u32*)(window + 4);
    rowY = 40;
    pageSize = menuDataBiosGetType(menuId);
    width = 0;
    width = 480;
    config = lbl_80478F98;
    flagCount = config->count;
    visibleCount = flagCount;
    if (flagCount > pageSize) {
        visibleCount = pageSize;
    }

    rowHeight = visibleCount * 13;
    /* RULE-EXCEPTION(user-approved): codegen-only identity cast; reading width through its address keeps it a stack local that is reloaded after each call and hoisted out of the row loop, as in retail - see docs/RULE_EXCEPTIONS.md */
    windowDrawPanel(30, 40, *(s32*)&width + 25, rowHeight);

    if (*(s16*)lbl_80478838 > 0) {
        center = width / 2;
        menuDrawRect(center + 20, 33, center + 40, 33, center + 30, 23);
    }

    config = lbl_80478F98;
    if ((s32)config->count - *(s16*)lbl_80478838 > pageSize) {
        center = width / 2;
        menuDrawRect(center + 20, rowHeight + 47, center + 40,
                    rowHeight + 47, center + 30, rowHeight + 57);
    }

    row = 0;
    while (row < visibleCount) {
        if (*(s16*)(lbl_80478838 + 2) == row) {
            color = 0xFF0000FF;
        } else {
            color = -1;
        }

        flagIndex = row + *(s16*)lbl_80478838;
        sprintf((char*)lbl_803A19C8, (const char*)lbl_80266688, flagIndex,
                gsTaskFlagGetId(flagIndex));
        GSmsgPrintf(30, rowY, color, lbl_803A19C8);

        GSmsgPrintf(width + 30, rowY, color, lbl_8047B6C8,
                    std082_flagGetValue(gsTaskFlagGetId(row + *(s16*)lbl_80478838)));
        rowY += 13;
        row++;
    }
}
#endif

/* 0x80008C40 | 0x538 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* windowGetKeyInfo(void);
#endif
extern void std078_flagSetFalse(s32);
extern void std079_flagSetTrue(s32);
typedef struct GsTaskFlagCursor {
    s16 top;
    s16 cursor;
} GsTaskFlagCursor;

#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
/* PC: the helper our copy calls (the merged file writes it out in place) */
static inline u16 gsTaskFlagGetLinkedId(s32 index) {
    s16 id;
    s32 i;
    GsTaskFlagDef* def;

    if (index < 0) {
        return 0;
    }
    if ((u32)index >= ((GsTaskFlagConfig*)PC_PTR(lbl_80478F98))->count) {
        return 0;
    }
    id = ((GsTaskFlagConfig*)PC_PTR(lbl_80478F98))->first;
    if (index != 0) {
        for (i = 0; i < index - 1; i++) {
            id = ((GsTaskFlagDef*)PC_PTR(lbl_80478F9C))[id].next;
        }
        def = (GsTaskFlagDef*)PC_PTR(lbl_80478F9C);
        def += id;
        id = def->next;
    }
    return id;
}

void dbgFlagMenuCursor(u8* window) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 menuDataBiosGetType(u32);
#endif
    u16 repeat;
    u16 trigger;
    s16 count;
    s16 page;
    GsTaskFlagCursor pos;

    repeat = ((const u16*)windowGetKeyInfo())[3];
    trigger = ((const u16*)windowGetKeyInfo())[0];
    count = (s16)((GsTaskFlagConfig*)PC_PTR(lbl_80478F98))->count;
    page = menuDataBiosGetType(*(u32*)(window + 4));
    if (count < page) {
        page = count;
    }

    pos = *(GsTaskFlagCursor*)lbl_80478838;

    if (repeat & MENU_KEY_UP) {
        pos.cursor--;
    } else if (repeat & MENU_KEY_DOWN) {
        pos.cursor++;
    }

    if (repeat & MENU_KEY_LEFT) {
        pos.cursor = pos.cursor - (menuDataBiosGetType(*(u32*)(window + 4)) - 1);
    } else if (repeat & MENU_KEY_RIGHT) {
        pos.cursor = pos.cursor + (menuDataBiosGetType(*(u32*)(window + 4)) - 1);
    }

    if (pos.cursor < 0) {
        pos.top += pos.cursor;
        pos.cursor = 0;
        if (pos.top < 0) {
            pos.cursor = page - 1;
            pos.top = count - page;
        }
    } else if (pos.cursor >= page) {
        pos.top = pos.top + (pos.cursor - (page - 1));
        pos.cursor = page - 1;
        if (pos.top + pos.cursor >= count) {
            pos.top = 0;
            pos.cursor = 0;
        }
    }

    if (trigger & MENU_KEY_Y) {
        std078_flagSetFalse(gsTaskFlagGetLinkedId(pos.top + pos.cursor));
    } else if (trigger & MENU_KEY_X) {
        std079_flagSetTrue(gsTaskFlagGetLinkedId(pos.top + pos.cursor));
    }

    *(GsTaskFlagCursor*)lbl_80478838 = pos;
    *(u32*)(window + 0x80) = gsTaskFlagGetLinkedId(pos.top + pos.cursor);
}
#else
void dbgFlagMenuCursor(u8* window) {
    u16 repeat;
    u16 trigger;
    s16 count;
    s16 page;
    GsTaskFlagCursor pos;

    repeat = ((const u16*)windowGetKeyInfo())[3];
    trigger = ((const u16*)windowGetKeyInfo())[0];
    count = (s16)lbl_80478F98->count;
    page = menuDataBiosGetType(*(u32*)(window + 4));
    if (count < page) {
        page = count;
    }

    pos = *(GsTaskFlagCursor*)lbl_80478838;

    if (repeat & MENU_KEY_UP) {
        pos.cursor--;
    } else if (repeat & MENU_KEY_DOWN) {
        pos.cursor++;
    }

    if (repeat & MENU_KEY_LEFT) {
        pos.cursor = pos.cursor - (menuDataBiosGetType(*(u32*)(window + 4)) - 1);
    } else if (repeat & MENU_KEY_RIGHT) {
        pos.cursor = pos.cursor + (menuDataBiosGetType(*(u32*)(window + 4)) - 1);
    }

    if (pos.cursor < 0) {
        pos.top += pos.cursor;
        pos.cursor = 0;
        if (pos.top < 0) {
            pos.cursor = page - 1;
            pos.top = count - page;
        }
    } else if (pos.cursor >= page) {
        pos.top = pos.top + (pos.cursor - (page - 1));
        pos.cursor = page - 1;
        if (pos.top + pos.cursor >= count) {
            pos.top = 0;
            pos.cursor = 0;
        }
    }

    if (trigger & MENU_KEY_Y) {
        std078_flagSetFalse(gsTaskFlagGetId(pos.top + pos.cursor));
    } else if (trigger & MENU_KEY_X) {
        std079_flagSetTrue(gsTaskFlagGetId(pos.top + pos.cursor));
    }

    *(GsTaskFlagCursor*)lbl_80478838 = pos;
    *(u32*)(window + 0x80) = gsTaskFlagGetId(pos.top + pos.cursor);
}
#endif
#endif /* GS_TASK_RANGE_RESIDUAL_80008868 */

#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_RESIDUAL_80006908)
/* ===== Phase 2 recovery stubs ===== */

/* fn_80006908 - 0x80006908 | size: 0x6a4 */
extern void jumptable_802E28D0();
extern u32 fight_encount_data_number;
extern u32 fight_kind_data_number;
extern u32 fight_type_data_number;
extern u32 fight_floor_data_number;
typedef struct GsTaskEncounterBackup {
    u32 words[14];
} GsTaskEncounterBackup;

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (push; peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma push
#pragma peephole off
s32 fn_80006908(u16 id)
{
    void* encounter;
    GsTaskEncounterBackup backup;
    u32 fightKind;
    u32 trainer;
    u32 floor;
    u32 trainer0;
    u32 device0;
    u32 trainer1;
    u32 device1;
    u32 trainer2;
    u32 device2;
    u32 trainer3;
    u32 device3;
    s32 event;
    s32 value;

    if (id == 0) {
        return 1;
    }
    if (id >= *(u32*)(void*)PC_PTR(fight_encount_data_number)) {
        return 1;
    }

    encounter = fightEncountDataBiosGetPtr(id);
    backup = *(GsTaskEncounterBackup*)encounter;

    for (;;) {
        value = menuOpenCustom(0xCD, 0, 0, 0, 1, 1, id);
        if (value == -1) {
            menuCloseCustom(0xCD, 0, 1);
            *(GsTaskEncounterBackup*)encounter = backup;
            return -1;
        }
        if (value == -2) {
            if (menuOpen(0x44, 1) != 0) {
                menuCloseCustom(0x44, 0, 1);
                continue;
            }
            menuCloseCustom(0x44, 0, 1);
            break;
        }

        event = menuGetCursorItemID(0xCD);
        switch (event) {
        case 0xD3F:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetFightKind(encounter), &fightKind,
                    _dbgMenuFightGetFightKindDataIdSub) == 0) {
                value = -1;
            } else {
                if (fightKind >= *(u32*)(void*)PC_PTR(fight_kind_data_number)) {
                    fightKind = *(u32*)(void*)PC_PTR(fight_kind_data_number) - 1;
                }
                menuSubCloseNumberInput();
                value = fightKind;
            }
            if (value >= 0) {
                fightEncountDataBiosSetFightKind(encounter, (u8)value);
            }
            break;

        case 0xD40:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetTrainer(encounter), &trainer,
                    fn_800086EC) == 0) {
                value = -1;
            } else {
                if (trainer >= *(u32*)(void*)PC_PTR(fight_type_data_number)) {
                    trainer = *(u32*)(void*)PC_PTR(fight_type_data_number) - 1;
                }
                menuSubCloseNumberInput();
                value = trainer;
            }
            if (value >= 0) {
                fightEncountDataBiosSetTrainer(encounter, (u8)value);
            }
            break;

        case 0xD41:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetFightFloorDataId(encounter), &floor,
                    _dbgMenuFightGetFightFloorDataIdSub) == 0) {
                value = -1;
            } else {
                if (floor >= *(u32*)(void*)PC_PTR(fight_floor_data_number)) {
                    floor = *(u32*)(void*)PC_PTR(fight_floor_data_number) - 1;
                }
                menuSubCloseNumberInput();
                value = floor;
            }
            if (value >= 0) {
                fightEncountDataBiosSetFightFloorDataId(encounter, (u16)value);
            }
            break;

        case 0xD42:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetFightTrainerDataId(encounter, 0),
                    &trainer0, fn_8000868C) == 0) {
                value = -1;
            } else {
                if (trainer0 >= lbl_80478F20->count) {
                    trainer0 = lbl_80478F20->count - 1;
                }
                menuSubCloseNumberInput();
                value = trainer0;
            }
            if (value < 0) {
                break;
            }
            fightEncountDataBiosSetFightTrainerDataId(encounter, 0, (u16)value);

            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetGSInputDevice(encounter, 0), &device0,
                    fn_800085D8) == 0) {
                value = -1;
            } else {
                if (device0 >= 5) {
                    device0 = 4;
                }
                menuSubCloseNumberInput();
                value = device0;
            }
            if (value >= 0) {
                fightEncountDataBiosSetGSInputDevice(encounter, 0, value);
            }
            break;

        case 0xD43:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetFightTrainerDataId(encounter, 1),
                    &trainer1, fn_8000868C) == 0) {
                value = -1;
            } else {
                if (trainer1 >= lbl_80478F20->count) {
                    trainer1 = lbl_80478F20->count - 1;
                }
                menuSubCloseNumberInput();
                value = trainer1;
            }
            if (value < 0) {
                break;
            }
            fightEncountDataBiosSetFightTrainerDataId(encounter, 1, (u16)value);

            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetGSInputDevice(encounter, 1), &device1,
                    fn_800085D8) == 0) {
                value = -1;
            } else {
                if (device1 >= 5) {
                    device1 = 4;
                }
                menuSubCloseNumberInput();
                value = device1;
            }
            if (value >= 0) {
                fightEncountDataBiosSetGSInputDevice(encounter, 1, value);
            }
            break;

        case 0xD44:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetFightTrainerDataId(encounter, 2),
                    &trainer2, fn_8000868C) == 0) {
                value = -1;
            } else {
                if (trainer2 >= lbl_80478F20->count) {
                    trainer2 = lbl_80478F20->count - 1;
                }
                menuSubCloseNumberInput();
                value = trainer2;
            }
            if (value < 0) {
                break;
            }
            fightEncountDataBiosSetFightTrainerDataId(encounter, 2, (u16)value);

            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetGSInputDevice(encounter, 2), &device2,
                    fn_800085D8) == 0) {
                value = -1;
            } else {
                if (device2 >= 5) {
                    device2 = 4;
                }
                menuSubCloseNumberInput();
                value = device2;
            }
            if (value >= 0) {
                fightEncountDataBiosSetGSInputDevice(encounter, 2, value);
            }
            break;

        case 0xD45:
            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetFightTrainerDataId(encounter, 3),
                    &trainer3, fn_8000868C) == 0) {
                value = -1;
            } else {
                if (trainer3 >= lbl_80478F20->count) {
                    trainer3 = lbl_80478F20->count - 1;
                }
                menuSubCloseNumberInput();
                value = trainer3;
            }
            if (value < 0) {
                break;
            }
            fightEncountDataBiosSetFightTrainerDataId(encounter, 3, (u16)value);

            if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(
                    fightEncountDataBiosGetGSInputDevice(encounter, 3), &device3,
                    fn_800085D8) == 0) {
                value = -1;
            } else {
                if (device3 >= 5) {
                    device3 = 4;
                }
                menuSubCloseNumberInput();
                value = device3;
            }
            if (value >= 0) {
                fightEncountDataBiosSetGSInputDevice(encounter, 3, value);
            }
            break;
        }
    }

    menuCloseCustom(0xCD, 0, 1);
    return 1;
}
#pragma pop
#endif /* GS_TASK_RANGE_RESIDUAL_80006908 */

#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_EXACT_80006FAC)
/* fn_80006FAC - 0x80006FAC | size: 0xdc */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSinputButtonsChanged(s32 port);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_800F7BC4(s32 port);
#endif
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
void menuFightButtonNormal(u8* ctx) {
    u32 buttons;
    u32 held;

    if (ctx == 0) return;

    buttons = GSinputButtonsChanged(1);
    held = fn_800F7BC4(1);
    if ((held & buttons) & 0x100) {
        *(u8*)(ctx + 0x98) = 1;
        *(u32*)(ctx + 0x80) = menuGetCursorItemID(*(u32*)(ctx + 0x4));
    }

    buttons = GSinputButtonsChanged(1);
    held = fn_800F7BC4(1);
    if ((held & buttons) & 0x1000) {
        *(u8*)(ctx + 0x98) = 1;
        *(s32*)(ctx + 0x80) = -2;
    }

    buttons = GSinputButtonsChanged(1);
    held = fn_800F7BC4(1);
    if ((held & buttons) & 0x200) {
        *(u8*)(ctx + 0x98) = 1;
        *(u8*)(ctx + 0x99) = 1;
        *(s32*)(ctx + 0x80) = -1;
    }
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightStopTimeOut - 0x80007088 | size: 0x44 */
extern u8   fightIsBattleActive(void);
extern void fightFloorSetTimeOutAllFightResult(s32 mode);
extern void dbgMenuClose(void);
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightStopTimeOut(void) {
    if (fightIsBattleActive() == 0) {
        return -1;
    }
    fightFloorSetTimeOutAllFightResult(0);
    dbgMenuClose();
    return 0;
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightStopHostLose - 0x800070CC | size: 0x44 */
extern void battleSetResult(s32 mode);
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightStopHostLose(void) {
    if (fightIsBattleActive() == 0) {
        return -1;
    }
    battleSetResult(3);
    dbgMenuClose();
    return 0;
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightStopHostWin - 0x80007110 | size: 0x44 */
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightStopHostWin(void) {
    if (fightIsBattleActive() == 0) {
        return -1;
    }
    battleSetResult(2);
    dbgMenuClose();
    return 0;
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightFightPokemonSelect5 - 0x80007154 | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl menuFightPokemonSelectSub vs bl menuFightPokemonSelectSub (same addr) */
extern s32  menuFightPokemonSelectSub();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fightTrainerGetValidFightPokemonPtr(u32 ptr, s32 slot);
#endif
extern u32  _target_fight_trainer_ptr;
extern u32  _target_fight_pokemon_ptr;
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightFightPokemonSelect5(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_pokemon_ptr = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(_target_fight_trainer_ptr, 5));
    if (_target_fight_pokemon_ptr == 0) return -1;
    return menuFightPokemonSelectSub(_target_fight_pokemon_ptr); /* the Pokemon just stored, still in r3 (assembly) */
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightFightPokemonSelect4 - 0x800071AC | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl menuFightPokemonSelectSub vs bl menuFightPokemonSelectSub (same addr) */
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightFightPokemonSelect4(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_pokemon_ptr = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(_target_fight_trainer_ptr, 4));
    if (_target_fight_pokemon_ptr == 0) return -1;
    return menuFightPokemonSelectSub(_target_fight_pokemon_ptr); /* the Pokemon just stored, still in r3 (assembly) */
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightFightPokemonSelect3 - 0x80007204 | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl menuFightPokemonSelectSub vs bl menuFightPokemonSelectSub (same addr) */
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightFightPokemonSelect3(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_pokemon_ptr = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(_target_fight_trainer_ptr, 3));
    if (_target_fight_pokemon_ptr == 0) return -1;
    return menuFightPokemonSelectSub(_target_fight_pokemon_ptr); /* the Pokemon just stored, still in r3 (assembly) */
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightFightPokemonSelect2 - 0x8000725C | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl menuFightPokemonSelectSub vs bl menuFightPokemonSelectSub (same addr) */
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightFightPokemonSelect2(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_pokemon_ptr = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(_target_fight_trainer_ptr, 2));
    if (_target_fight_pokemon_ptr == 0) return -1;
    return menuFightPokemonSelectSub(_target_fight_pokemon_ptr); /* the Pokemon just stored, still in r3 (assembly) */
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightFightPokemonSelect1 - 0x800072B4 | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl menuFightPokemonSelectSub vs bl menuFightPokemonSelectSub (same addr) */
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightFightPokemonSelect1(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_pokemon_ptr = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(_target_fight_trainer_ptr, 1));
    if (_target_fight_pokemon_ptr == 0) return -1;
    return menuFightPokemonSelectSub(_target_fight_pokemon_ptr); /* the Pokemon just stored, still in r3 (assembly) */
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif

/* dbgMenuFightFightPokemonSelect0 - 0x8000730C | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl menuFightPokemonSelectSub vs bl menuFightPokemonSelectSub (same addr) */
#if 1
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
s32 dbgMenuFightFightPokemonSelect0(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_pokemon_ptr = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(_target_fight_trainer_ptr, 0));
    if (_target_fight_pokemon_ptr == 0) return -1;
    return menuFightPokemonSelectSub(_target_fight_pokemon_ptr); /* the Pokemon just stored, still in r3 (assembly) */
}
#if !defined(GS_TASK_RANGE_EXACT_80006FAC)
#endif
#endif
#endif /* GS_TASK_RANGE_EXACT_80006FAC */

/* menuFightPokemonSelectSub - 0x80007364 | size: 0x2f8 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightFloorGetStatus(s32 a, u16 b, s32 c, s32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightPokemonGetPokemonPtr(u32 ctx);
#endif
extern void* fightFloorGetFightPokemonPtrToFightTrainerPtr(s32 a, u32 ctx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8   figthPokemonGetLevel(u32 ctx);
#endif
extern u8   fightTrainerCheckCanIrekaeFightPokemon(void* ptr, u32 ctx);
extern void* fightTrainerGetFightPokemonPtrToFightOutPokemonPtr(void* ptr, u32 ctx);
#ifndef TARGET_PC /* PC: canonical prototypes in pc/prototypes.h */
extern u8   fightOutPokemonIsUseHensinBuff(void* ptr);
extern void fightOutPokemonGetTokuseiDataId(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonGetZokuseiDataId(void* ptr, s32 a);
#endif
extern s32  dbgMenuPokemonEdit(void* ptr, s32 a, u8* b, u8* c, u8* d, u8* e);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  pokemonGetStatus(void* ptr, u16 b, s32 c, s32 d);
#endif
extern void menuFaceRequestPokemonFace(void* ptr, s32 a, s32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
extern u16  pokemonGetFaceSpriteId(void* ptr);
extern s8   menuFaceIsReady(u16 id);
extern void* pokemonCreateSequence(void* ptr);
extern void battleGridReplacePokemon(void* ptr, void* src);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void battleGridUpdate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSetStatus(void* ptr, s32 a, s32 b, s32 c, s32 d);
#endif
extern void wazaSequenceOwnerFree(void* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerSetVisibility(void* ptr, s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonWriteJoutaiDataId(void* ptr, s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightMenuFightOutPokemonRenewStatusMenu(void* ptr, u16 a, s32 b);
#endif
extern void pokemonSetSequenceStatus(void* a, u32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonGetTokuseiDataId(void* ptr);
#endif
extern void fightOutPokemonSetTokuseiDataId(void* ptr, void* a);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightOutPokemonSetZokuseiDataId(void* ptr, u8 a, u16 b);
#endif
extern void fightMenuFightTrainerRenewStatusMenu(void* ptr, u16 a);
#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_RESIDUAL_80007364)
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 menuFightPokemonSelectSub(u32 ctx) {
    s32 result;
    u8 prevLevel;
    u16 savedId;
    void* encounter;
    void* archive;
    void* scene;
    u16 itemId;
    u32 subItem;
    u8 sp_b;
    u8 sp_a;
    u8 sp_9;
    u8 sp_8;
    u16 loopIdx;

    sp_b = 0;
    sp_a = 0;
    sp_9 = 0;
    sp_8 = 0;

    savedId = (u16)(u32)fightFloorGetStatus(0, 0, 0x14, 0);
    scene = fightPokemonGetPokemonPtr(PC_PTR(ctx));
    if (scene == 0) {
        return -1;
    }

    archive = fightFloorGetFightPokemonPtrToFightTrainerPtr(0, ctx);
    if (archive == 0) {
        return -1;
    }

    prevLevel = figthPokemonGetLevel(ctx);

    if (fightTrainerCheckCanIrekaeFightPokemon(archive, ctx) == 2) {
        encounter = fightTrainerGetFightPokemonPtrToFightOutPokemonPtr(archive, ctx);
        if (fightOutPokemonIsUseHensinBuff(encounter) == 1) {
            return -1;
        }

        if (encounter != 0) {
            fightOutPokemonGetTokuseiDataId(encounter);
            for (loopIdx = 0; (u16)loopIdx < 2; loopIdx++) {
                fightOutPokemonGetZokuseiDataId(encounter, 0);
            }
        }

        result = dbgMenuPokemonEdit(scene, 1, &sp_b, &sp_a, &sp_9, &sp_8);
        if (result == 1) {
            itemId = pokemonGetStatus(scene, 0, POKEMON_STATUS_SPECIES, 0);
            if (encounter != 0) {
                subItem = pokemonGetStatus(encounter, 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
                if (subItem != 0) {
                    void* animPtr;

                    menuFaceRequestPokemonFace(scene, 0, 0);
                    while (menuFaceIsReady(pokemonGetFaceSpriteId(scene)) == 0) { /* r3 from the previous call (assembly) */
                        _threadSwitch();
                    }
                    animPtr = pokemonCreateSequence(scene);
                    battleGridReplacePokemon((void*)PC_PTR((u32)subItem), animPtr);
                    battleGridUpdate();
                    pokemonSetStatus(encounter, 0, POKEMON_STATUS_SEQUENCE_PTR, 0, (s32)PC_PTR32(animPtr));
                    wazaSequenceOwnerFree((void*)PC_PTR((u32)subItem));
                    wazaSequenceOwnerSetVisibility(animPtr, 1);
                    fightOutPokemonWriteJoutaiDataId(encounter, PC_PTR(0x14));
                }
                {
                    u16 newId;

                    newId = (u16)(u32)fightFloorGetStatus(0, 0, 0x14, 0);
                    fightMenuFightOutPokemonRenewStatusMenu(PC_PTR32_IMPLICIT(encounter), newId, 1);
                }
                pokemonSetSequenceStatus(
                    scene, pokemonGetStatus(encounter, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
                fightOutPokemonSetTokuseiDataId(
                    encounter, PC_PTR(pokemonGetTokuseiDataId(scene)));
                {
                    u32 i;

                    for (i = 0; (u16)i < 2; i++) {
                        u16 val;

                        val = pokemonGetStatus(0, itemId, POKEMON_STATUS_SPECIES_TYPE, i);
                        fightOutPokemonSetZokuseiDataId(encounter, (u8)i, val);
                    }
                }
            }
        }
    } else {
        result = dbgMenuPokemonEdit(scene, 1, 0, 0, 0, 0);
    }

    if ((u8)figthPokemonGetLevel(ctx) > (u8)prevLevel) {
        pokemonSetStatus((void*)PC_PTR(ctx), 0, POKEMON_STATUS_LEVEL_UP_FLAG, 0, 1);
    }

    fightMenuFightTrainerRenewStatusMenu(archive, savedId);
    return result;
}
#else
s32 menuFightPokemonSelectSub(u32 ctx) {
    u32 i;
    u8 prevLevel;
    void* archive;
    void* scene;
    s32 result;
    void* encounter;
    u32 subItem;
    void* animPtr;
    u32 loopIdx;
    u16 itemId;
    u8 sp_b;
    u8 sp_a;
    u8 sp_9;
    u8 sp_8;
    u16 savedId;

    sp_b = 0;
    sp_a = 0;
    sp_9 = 0;
    sp_8 = 0;

    savedId = (u16)(u32)fightFloorGetStatus(0, 0, 0x14, 0);
    scene = fightPokemonGetPokemonPtr(PC_PTR(ctx));
    if (scene == 0) {
        return -1;
    }

    archive = fightFloorGetFightPokemonPtrToFightTrainerPtr(0, ctx);
    if (archive == 0) {
        return -1;
    }

    prevLevel = figthPokemonGetLevel(ctx);

    if (fightTrainerCheckCanIrekaeFightPokemon(archive, ctx) == 2) {
        encounter = fightTrainerGetFightPokemonPtrToFightOutPokemonPtr(archive, ctx);
        if (fightOutPokemonIsUseHensinBuff(encounter) == 1) {
            return -1;
        }

        if (encounter != 0) {
            fightOutPokemonGetTokuseiDataId(encounter);
            for (loopIdx = 0; (u16)loopIdx < 2; loopIdx++) {
                fightOutPokemonGetZokuseiDataId(encounter, 0);
            }
        }

        result = dbgMenuPokemonEdit(scene, 1, &sp_b, &sp_a, &sp_9, &sp_8);
        if (result == 1) {
            itemId = pokemonGetStatus(scene, 0, POKEMON_STATUS_SPECIES, 0);
            if (encounter != 0) {
                subItem = pokemonGetStatus(encounter, 0, POKEMON_STATUS_SEQUENCE_PTR, 0);
                if (subItem != 0) {
                    menuFaceRequestPokemonFace(scene, 0, 0);
                    while (menuFaceIsReady(pokemonGetFaceSpriteId(scene)) == 0) {
                        _threadSwitch();
                    }
                    animPtr = pokemonCreateSequence(scene);
                    battleGridReplacePokemon((void*)PC_PTR((u32)subItem), animPtr);
                    battleGridUpdate();
                    pokemonSetStatus(encounter, 0, POKEMON_STATUS_SEQUENCE_PTR, 0, (s32)PC_PTR32(animPtr));
                    wazaSequenceOwnerFree((void*)PC_PTR((u32)subItem));
                    wazaSequenceOwnerSetVisibility(animPtr, 1);
                    fightOutPokemonWriteJoutaiDataId(encounter, PC_PTR(0x14));
                }
                fightMenuFightOutPokemonRenewStatusMenu(
                    encounter, (u16)(u32)fightFloorGetStatus(0, 0, 0x14, 0), 1);
                pokemonSetSequenceStatus(
                    scene, pokemonGetStatus(encounter, 0, POKEMON_STATUS_SEQUENCE_PTR, 0));
                fightOutPokemonSetTokuseiDataId(
                    encounter, pokemonGetTokuseiDataId(scene));
                for (i = 0; (u16)i < 2; i++) {
                    fightOutPokemonSetZokuseiDataId(
                        encounter, (u8)i, pokemonGetStatus(0, itemId, POKEMON_STATUS_SPECIES_TYPE, i));
                }
            }
        }
    } else {
        result = dbgMenuPokemonEdit(scene, 1, 0, 0, 0, 0);
    }

    if ((u8)figthPokemonGetLevel(ctx) > (u8)prevLevel) {
        pokemonSetStatus((void*)PC_PTR(ctx), 0, POKEMON_STATUS_LEVEL_UP_FLAG, 0, 1);
    }

    fightMenuFightTrainerRenewStatusMenu(archive, savedId);
    return result;
}
#endif
#endif /* GS_TASK_RANGE_RESIDUAL_80007364 */

#if !defined(GS_TASK_RANGE_SPLIT) || defined(GS_TASK_RANGE_EXACT_8000765C)
/* dbgMenuFightFightTrainerPokemonPartDataEdit - 0x8000765C | size: 0xac */
extern void* _dbgMenuFightGetFightTrainerPokemonPartDataIdSub(u32 id);
extern void fn_80051710(u16 id);
extern u16 lbl_80478830;
extern u32 fight_trainer_pokemon_part_data_number;
#if 1
s32 dbgMenuFightFightTrainerPokemonPartDataEdit(void) {
    u32 result;
    s32 val;

    if (fightIsBattleActive() == 0) {
        return -1;
    }

    for (;;) {
        if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(lbl_80478830, &result, _dbgMenuFightGetFightTrainerPokemonPartDataIdSub) == 0) {
            val = -1;
        } else {
            if (result >= *(u32*)PC_PTR(fight_trainer_pokemon_part_data_number)) {
                result = *(u32*)PC_PTR(fight_trainer_pokemon_part_data_number) - 1;
            }
            menuSubCloseNumberInput();
            val = (s32)result;
        }
        if (val < 0) {
            return 1;
        }
        lbl_80478830 = (u16)val;
        fn_80051710((u16)val);
    }
}
#endif

/* dbgMenuFightFightTrainerAiDataEdit - 0x80007708 | size: 0x70 */
extern s32  fn_80051E38(u32 slot);
#if 1
s32 dbgMenuFightFightTrainerAiDataEdit(void) {
    u16 tmp;
    u16 slot;
    tmp  = (u16)(u32)fightTrainerGetStatus(PC_PTR((s32)_target_fight_trainer_ptr), 0, 0x43, 0);
    slot = (u16)(u32)fightTrainerGetStatus(0, tmp, 2, 0);
    if (fightIsBattleActive() == 0) return -1;
    return fn_80051E38(slot);
}
#endif

/* dbgMenuFightFightTrainerAiAddsubCombo - 0x80007778 | size: 0x20 */
extern void fn_8004EADC(void);
#if 1
/* NAME-T1 fn_80007778 */
void dbgMenuFightFightTrainerAiAddsubCombo(void) {
    fn_8004EADC();
}
#endif

/* dbgMenuFightFightTrainerAiAddsubItem - 0x80007798 | size: 0x20 */
extern void fn_8004EC54(void);
#if 1
/* NAME-T1 fn_80007798 */
void dbgMenuFightFightTrainerAiAddsubItem(void) {
    fn_8004EC54();
}
#endif

/* dbgMenuFightFightTrainerAiAddsubWazaUsewaza - 0x800077B8 | size: 0x20 */
extern void dbgMenuTrainerAiEditA(void);
#if 1
/* NAME-T1 fn_800077B8 */
void dbgMenuFightFightTrainerAiAddsubWazaUsewaza(void) {
    dbgMenuTrainerAiEditA();
}
#endif

/* dbgMenuFightFightTrainerAiAddsubWazaDefpokemon - 0x800077D8 | size: 0x20 */
extern void fn_8004F860(void);
#if 1
void dbgMenuFightFightTrainerAiAddsubWazaDefpokemon(void) {
    fn_8004F860();
}
#endif

/* Address: 0x800077F8 | Size: 0x8 | Pattern: return_constant */
/* NAME-T1 fn_800077F8 */
u32 dbgMenuFightFightTrainerAiAddsubWaza(void) { return 1; }

/* dbgMenuFightFightTrainerAiAddsubIrekaeDasu - 0x80007800 | size: 0x20 */
extern void fn_8004FE3C(void);
#if 1
void dbgMenuFightFightTrainerAiAddsubIrekaeDasu(void) {
    fn_8004FE3C();
}
#endif

/* dbgMenuFightFightTrainerAiAddsubIrekaeModosu - 0x80007820 | size: 0x20 */
extern void fn_80050844(void);
#if 1
/* NAME-T1 fn_80007820 */
void dbgMenuFightFightTrainerAiAddsubIrekaeModosu(void) {
    fn_80050844();
}
#endif

/* Address: 0x80007840 | Size: 0x8 | Pattern: return_constant */
/* NAME-T1 fn_80007840 */
u32 dbgMenuFightFightTrainerAiAddsubIrekae(void) { return 1; }

/* dbgMenuFightFightTrainerAiAddsubDataEdit - 0x80007848 | size: 0xa4 */
extern void dbgMenuFightTrainerDataStatusInputDigit(u16 id, s32 a, s32 b, s32 c, s32 d);
extern u16 lbl_8047A28A;
extern u32 fight_trainer_ai_value_addsub_data_number;
extern void* _dbgMenuFightGetFightTrainerAiAddsubValueDataIdSub(u32 id);
#if 1
/* NAME-T1 fn_80007848 */
s32 dbgMenuFightFightTrainerAiAddsubDataEdit(void) {
    u32 result;
    s32 val;

    for (;;) {
        if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(lbl_8047A28A, &result, _dbgMenuFightGetFightTrainerAiAddsubValueDataIdSub) == 0) {
            val = -1;
        } else {
            if (result > *(u32*)PC_PTR(fight_trainer_ai_value_addsub_data_number)) {
                result = *(u32*)PC_PTR(fight_trainer_ai_value_addsub_data_number) - 1;
            }
            menuSubCloseNumberInput();
            val = (s32)result;
        }
        if (val < 0) {
            return 1;
        }
        lbl_8047A28A = (u16)val;
        dbgMenuFightTrainerDataStatusInputDigit((u16)val, 0x3e, 0, 0xc8, -0xc8);
    }
}
#endif

/* dbgMenuFightFightTrainerSelect1 - 0x800078EC | size: 0x58 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fightSideGetValidFightTrainerPtr(u32 ptr, s32 mode);
#endif
extern u32  _target_fight_side_ptr;
#if 1
s32 dbgMenuFightFightTrainerSelect1(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_trainer_ptr = fightSideGetValidFightTrainerPtr(_target_fight_side_ptr, 1);
    if (_target_fight_trainer_ptr == 0) return -1;
    return 1;
}
#endif

/* dbgMenuFightFightTrainerSelect0 - 0x80007944 | size: 0x58 */
#if 1
s32 dbgMenuFightFightTrainerSelect0(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_trainer_ptr = fightSideGetValidFightTrainerPtr(_target_fight_side_ptr, 0);
    if (_target_fight_trainer_ptr == 0) return -1;
    return 1;
}
#endif

/* dbgMenuFightFightSideSelectHostEnemy - 0x8000799C | size: 0x58 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32  fightTargetGetPtrAsNowFightType(s32 a, s32 b);
#endif
#if 1
s32 dbgMenuFightFightSideSelectHostEnemy(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_side_ptr = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(5, 0));
    if (_target_fight_side_ptr == 0) return -1;
    return 1;
}
#endif

/* dbgMenuFightFightSideSelectHost - 0x800079F4 | size: 0x58 */
#if 1
s32 dbgMenuFightFightSideSelectHost(void) {
    if (fightIsBattleActive() == 0) return -1;
    _target_fight_side_ptr = PC_PTR32_IMPLICIT(fightTargetGetPtrAsNowFightType(4, 0));
    if (_target_fight_side_ptr == 0) return -1;
    return 1;
}
#endif

/* dbgMenuFightFightFloorEditTenkou - 0x80007A4C | size: 0x38 */
#if 1
s32 dbgMenuFightFightFloorEditTenkou(void) {
    if (fightIsBattleActive() == 0) return -1;
    return 1;
}
#endif

/* dbgMenuFightWazaEdit - 0x80007A84 | size: 0xac | SYMBOL-NAME WALL 97.67%: bl dbgMenuFightWazaEditSub vs bl dbgMenuFightWazaEditSub (same addr) */
extern void dbgMenuFightWazaEditSub(u16 id);
extern u16 lbl_8047882A;
extern void* _dbgMenuFightGetWazaDataIdSub(u32 id);
#if 1
s32 dbgMenuFightWazaEdit(void) {
    u32 result;
    s32 val;

    if (fightIsBattleActive() == 0) {
        return -1;
    }

    for (;;) {
        if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(lbl_8047882A, &result, _dbgMenuFightGetWazaDataIdSub) == 0) {
            val = -1;
        } else {
            if (result >= 0x163) {
                result = 0x162;
            }
            menuSubCloseNumberInput();
            val = (s32)result;
        }
        if (val < 0) {
            return 1;
        }
        lbl_8047882A = (u16)val;
        dbgMenuFightWazaEditSub((u16)val);
    }
}
#endif

/* dbgMenuFightFlagEdit - 0x80007B30 | size: 0x4ac */
extern u8 fightSeqGetEffectAminFlag(void);
extern void fightSeqSetEffectAminFlag(u8 val);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s8 menuSubOpenYesNo(s32 max, s32 a, s32 b, s32 initial);
#endif
extern u8 fn_8001E224(u8 val, u32* out, s32 a, s32 b, s32 c, s32 d);
extern u8 fightSeqCheckYubiwohuruWazaDataId(u16 id);
extern u8 lbl_80478828;
extern u8 _tuikakouka_flag;
extern u8 lbl_8047A270;
extern u8 _all_fight_trainer_operate_flag;
extern u8 _fight_debug_menu_open_flag;
extern u8 _fight_trainer_ai_flow_disp_flag;
extern u8 _soubikakouka_flag;
extern u16 _yubiwohuru_waza_data_id;
extern u8 _fight_pokemon_status_menu_open_flag;
#if 1
/* NAME-T1 fn_80007B30 */
s32 dbgMenuFightFlagEdit(void) {
    u8 save_sfx;
    u8 save_speed;
    u8 save_rumble;
    u8 save_cam;
    u16 save_species;
    u8 save_nickname;
    u8 save_vol;
    u8 save_battle;
    u8 save_field;
    u8 save_stereo;
    s32 evt;

    save_vol = lbl_80478828;
    save_stereo = _tuikakouka_flag;
    save_field = lbl_8047A270;
    save_battle = _all_fight_trainer_operate_flag;
    save_speed = _fight_debug_menu_open_flag;
    save_sfx = fightSeqGetEffectAminFlag();
    save_rumble = _fight_trainer_ai_flow_disp_flag;
    save_cam = _soubikakouka_flag;
    save_species = _yubiwohuru_waza_data_id;
    save_nickname = _fight_pokemon_status_menu_open_flag;

    for (;;) {
        evt = menuOpenCustom(0xe, 0, 0, 0, 1, 0, 0);
        if (evt == -1) {
            menuCloseCustom(0xe, 0, 1);
            if (save_vol < 1) save_vol = 1;
            if ((u8)save_vol > 9) save_vol = 9;
            lbl_80478828 = save_vol;
            _tuikakouka_flag = save_stereo;
            lbl_8047A270 = save_field;
            _all_fight_trainer_operate_flag = save_battle;
            _fight_debug_menu_open_flag = save_speed;
            fightSeqSetEffectAminFlag(save_sfx);
            _fight_trainer_ai_flow_disp_flag = save_rumble;
            _soubikakouka_flag = save_cam;
            _yubiwohuru_waza_data_id = save_species;
            _fight_pokemon_status_menu_open_flag = save_nickname;
            return -1;
        }
        if (evt == -2) {
            if (menuOpen(0x44, 1) != 0) {
                menuCloseCustom(0x44, 0, 1);
                continue;
            }
            menuCloseCustom(0x44, 0, 1);
            break;
        }

        if (evt == 0x57) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, _tuikakouka_flag == 0);
            if (r == 0) {
                _tuikakouka_flag = 1;
            } else if (r == 1) {
                _tuikakouka_flag = 0;
            }
            continue;
        }

        if (evt == 0x59) {
            u8 ret;
            u32 tmp;
            ret = fn_8001E224(lbl_80478828, &tmp, 1, 0x32, 0x32, 0);
            if (ret == 0) {
                menuSubCloseNumberInput();
                continue;
            }
            {
                u8 v;
                v = (u8)tmp;
                if ((u8)v < 1) v = 1;
                if ((u8)v > 9) v = 9;
                lbl_80478828 = v;
            }
            menuSubCloseNumberInput();
            continue;
        }

        if (evt == 0x5b) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, lbl_8047A270 == 0);
            if (r == 0) {
                lbl_8047A270 = 1;
            } else if (r == 1) {
                lbl_8047A270 = 0;
            }
            continue;
        }

        if (evt == 0x5d) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, fightSeqGetEffectAminFlag() == 0);
            if (r == 0) {
                fightSeqSetEffectAminFlag(1);
            } else if (r == 1) {
                fightSeqSetEffectAminFlag(0);
            }
            continue;
        }

        if (evt == 0x5f) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, _all_fight_trainer_operate_flag == 0);
            if (r == 0) {
                _all_fight_trainer_operate_flag = 1;
            } else if (r == 1) {
                _all_fight_trainer_operate_flag = 0;
            }
            continue;
        }

        if (evt == 0x6de) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, _fight_debug_menu_open_flag == 0);
            if (r == 0) {
                _fight_debug_menu_open_flag = 1;
            } else if (r == 1) {
                _fight_debug_menu_open_flag = 0;
            }
            continue;
        }

        if (evt == 0x6f9) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, _fight_trainer_ai_flow_disp_flag == 0);
            if (r == 0) {
                _fight_trainer_ai_flow_disp_flag = 1;
            } else if (r == 1) {
                _fight_trainer_ai_flow_disp_flag = 0;
            }
            continue;
        }

        if (evt == 0x778) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, _soubikakouka_flag == 0);
            if (r == 0) {
                _soubikakouka_flag = 1;
            } else if (r == 1) {
                _soubikakouka_flag = 0;
            }
            continue;
        }

        if (evt == 0x77a) {
            if (fightIsBattleActive() != 0) {
                s32 maxVal;
                s32 sid;
                u32 tmp;
                maxVal = 0x162;
                sid = _yubiwohuru_waza_data_id;
                for (;;) {
                    if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs((u16)sid, &tmp, _dbgMenuFightGetWazaDataIdSub) == 0) {
                        sid = -1;
                    } else {
                        if (tmp >= 0x163) {
                            tmp = (u32)maxVal;
                        }
                        menuSubCloseNumberInput();
                        sid = tmp;
                    }
                    if (sid < 0) break;
                    if (fightSeqCheckYubiwohuruWazaDataId((u16)sid) == 1) {
                        _yubiwohuru_waza_data_id = (u16)sid;
                        break;
                    }
                }
            }
            continue;
        }

        if (evt == 0x1198) {
            s8 r;
            r = menuSubOpenYesNo(0x7f, -1, -1, _fight_pokemon_status_menu_open_flag == 0);
            if (r == 0) {
                _fight_pokemon_status_menu_open_flag = 1;
            } else if (r == 1) {
                _fight_pokemon_status_menu_open_flag = 0;
            }
            continue;
        }
    }

    menuCloseCustom(0xe, 0, 1);
    return 1;
}
#endif

/* dbgMenuFightStop - 0x80007FDC | size: 0x38 */
#if 1
s32 dbgMenuFightStop(void) {
    if (fightIsBattleActive() == 0) return -1;
    return 1;
}
#endif

/* dbgMenuFightFightFloorEdit - 0x80008014 | size: 0x38 */
#if 1
s32 dbgMenuFightFightFloorEdit(void) {
    if (fightIsBattleActive() == 0) return -1;
    return 1;
}
#endif

/* fn_8000804C - 0x8000804C | size: 0xf8 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* floorGetCurrentContext(void);
#endif
extern void fightStartEncount(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSthreadCreate(s32 a, void* b, s32 c, s32 d, s32 e, void* f);
#endif
extern void GSthreadSetArgs(void* task, s32 a, ...);
extern u16 lbl_8047882C;
extern u32 fight_encount_data_number;
#if 1
s32 fn_8000804C(void) {
    u32 result;
    s32 val;

    for (;;) {
        if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs(lbl_8047882C, &result, fn_800087FC) == 0) {
            val = -1;
        } else {
            if (result >= *(u32*)(void*)PC_PTR(fight_encount_data_number)) {
                result = *(u32*)(void*)PC_PTR(fight_encount_data_number) - 1;
            }
            menuSubCloseNumberInput();
            val = (s32)result;
        }
        if (val < 0) {
            break;
        }
        lbl_8047882C = (u16)val;
        if ((u16)lbl_8047882C == 0) {
            continue;
        }
        if (fn_80006908((u16)val) < 0) {
            continue;
        }
        if (fightIsBattleActive() != 0) {
            break;
        }
        {
            u16 saved;
            void* taskPtr;
            saved = lbl_8047882C;
            taskPtr = GSthreadCreate(1, floorGetCurrentContext(), 0x4000, 1, 1, (void*)fightStartEncount);
            if (taskPtr != 0) {
                GSthreadSetArgs(taskPtr, 1, saved);
            }
            return 0;
        }
    }
    return -1;
}
#endif

/* Address: 0x80008144 | Size: 0x8 | Pattern: return_constant */
/* NAME-T1 fn_80008144 */
u32 dbgMenuFightMain(void) { return 1; }

/* Address: 0x8000814C | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_8000814C */
u16 dbgMenuFightGetYubiwohuruWazaDataId(void) { /* Metronome debug override move id */
    return _yubiwohuru_waza_data_id;
}

/* Address: 0x80008154 | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_80008154 */
u8 dbgMenuFightGetSoubikoukaFlag(void) { /* held-item ("equip") effect debug flag */
    return _soubikakouka_flag;
}

/* Address: 0x8000815C | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_8000815C */
u8 dbgMenuFightGetFightPokeStMenuOpenFlag(void) {
    return _fight_pokemon_status_menu_open_flag;
}

/* Address: 0x80008164 | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_80008164 */
u8 dbgMenuFightGetFightTrainerAiFlowDispFlag(void) {
    return _fight_trainer_ai_flow_disp_flag;
}

/* Address: 0x8000816C | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_8000816C */
u8 dbgMenuFightGetFightDebugMenuOpenFlag(void) {
    return _fight_debug_menu_open_flag;
}

/* Address: 0x80008174 | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_80008174 */
u8 dbgMenuFightGetAllFightTrainerOperateFlag(void) { /* player chooses for every trainer, AI ones too */
    return _all_fight_trainer_operate_flag;
}

/* Address: 0x8000817C | Size: 0x8 | Pattern: sda_getter */
/* NAME-T1 fn_8000817C */
u8 dbgMenuFightGetTuikakoukaFlag(void) { /* added-effect debug flag: forces the secondary-effect roll to 99 */
    return _tuikakouka_flag;
}

/* dbgMenuFightGetMsgSpeedToFrame - 0x80008184 | size: 0xc0 */
extern u8 winMsgCloseCheckFight(void);
extern u8 lbl_80266678[];
#if 1
u32 dbgMenuFightGetMsgSpeedToFrame(u32 value) {
    extern u32 GSinputButtonsChanged(s32 id);
    /* RULE-EXCEPTION(user-approved): caller-side prototype returning u32 (input.c defines it returning u16); retail uses the result without re-extending it (and r0,r3,r31, no clrlwi) - see docs/RULE_EXCEPTIONS.md */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fn_800F7BC4(s32 id);
#endif
    u32 buttons;
    u32 held;
    u8 idx;
    u8 buf[10];

    idx = lbl_80478828;
    *(u32*)&buf[0] = *(u32*)&lbl_80266678[0];
    *(u32*)&buf[4] = *(u32*)&lbl_80266678[4];
    *(u16*)&buf[8] = *(u16*)&lbl_80266678[8];

    if (winMsgCloseCheckFight() == 1) {
        value = (value * buf[idx]) / 100;
        if (lbl_8047A270 == 1) {
            value = 0;
            for (;;) {
                buttons = GSinputButtonsChanged(1);
                held = fn_800F7BC4(1);
                if ((held & buttons) & 0x100) {
                    break;
                }
                _threadSwitch();
            }
        }
    }

    return value;
}
#endif

/* _dbgMenuFightGetFightTrainerAiAddsubValueDataIdSub - 0x80008244 | size: 0x14c */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(s32 slot, void* ptr);
#endif
extern u32 fight_trainer_ai_value_addsub_data_number;
#if 1
void* _dbgMenuFightGetFightTrainerAiAddsubValueDataIdSub(u32 id) {
    void* r31;
    void* r30;
    void* r29;
    void* r28;
    if (id >= *(u32*)(void*)PC_PTR(fight_trainer_ai_value_addsub_data_number)) {
        r28 = 0;
        r31 = (void*)0xEB63;
        r30 = r31;
        r29 = r31;
    } else {
        r31 = PC_PTR(fightTrainerGetStatus(0, (u16)id, 0x40, 0));
        r30 = PC_PTR(fightTrainerGetStatus(0, (u16)id, 0x41, 0));
        r29 = PC_PTR(fightTrainerGetStatus(0, (u16)id, 0x3f, 0));
        r28 = PC_PTR(fightTrainerGetStatus(0, (u16)id, 0x3e, 0));
    }
    if (r29 == 0) r29 = (void*)0xEB63;
    if (r31 == 0) r31 = (void*)0xEB63;
    if (r30 == 0) r30 = (void*)0xEB63;
    msgctrlSetValue(0xd,  PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(r31))));
    msgctrlSetValue(0xe,  PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(r30))));
    msgctrlSetValue(0x4d, PC_PTR32_IMPLICIT(GSmsgGetGSchar(PC_PTR32(r29))));
    msgctrlSetValue(0x2f, PC_PTR32_IMPLICIT(r28));
    return GSmsgGetGSchar(0xF159);
}
#endif

/* _dbgMenuFightGetFightTrainerPokemonPartDataIdSub - 0x80008390 | size: 0x6c */
extern u32 fight_trainer_pokemon_part_data_number;
void* _dbgMenuFightGetFightTrainerPokemonPartDataIdSub(u32 id) {
    if (id == 0) return GSmsgGetGSchar(0xEB63);
    if (id >= *(u32*)(void*)PC_PTR(fight_trainer_pokemon_part_data_number)) return GSmsgGetGSchar(0xEB63);
    return GSmsgGetGSchar((u32)fightTrainerGetStatus(0, (u16)id, 0xb, 0));
}

/* _dbgMenuFightGetWazaDataIdSub - 0x800083FC | size: 0x64 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* wazaGetStatus(s32 a, u16 b, s32 c, s32 d);
#endif
#if 1
void* _dbgMenuFightGetWazaDataIdSub(u32 id) {
    if (id == 0) return GSmsgGetGSchar(0xEB63);
    if (id >= 0x163) return GSmsgGetGSchar(0xEB63);
    return GSmsgGetGSchar((u32)wazaGetStatus(0, (u16)id, 1, 0));
}
#endif

/* dbgMenuFightGetWazaTypeId - 0x80008460 | size: 0x60 */
extern void* _dbgMenuFightGetWazaTypeIdSub(u32 id);   /* forward decl for callback */
#if 1
s32 dbgMenuFightGetWazaTypeId(s32 id) {
    u32 result;
    if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs((u8)id, &result, _dbgMenuFightGetWazaTypeIdSub) == 0) return -1;
    if (result > 0xb) result = 0xb;
    menuSubCloseNumberInput();
    return (s32)result;
}
#endif

/* _dbgMenuFightGetWazaTypeIdSub - 0x800084C0 | size: 0x58 | SYMBOL-NAME WALL 95.45%: bl wazaGetWazaTypeIdName vs bl wazaGetWazaTypeIdName (same addr) */
extern void* wazaGetWazaTypeIdName(u8 idx);
#if 1
void* _dbgMenuFightGetWazaTypeIdSub(u32 id) {
    if (id == 0) return GSmsgGetGSchar(0xEB63);
    if (id > 0xb) return GSmsgGetGSchar(0xEB63);
    return GSmsgGetGSchar(PC_PTR32(wazaGetWazaTypeIdName((u8)id)));
}
#endif

/* dbgMenuFightGetZokuseiDataId - 0x80008518 | size: 0x64 */
#if 1
s32 dbgMenuFightGetZokuseiDataId(u32 id) {
    u32 result;
    if (menuSubOpenNumberInputSub__FUlPUlUcssbPFUl_PUs((u16)id, &result, _dbgMenuFightGetZokuseiDataIdSub) == 0) return -1;
    if (result >= lbl_80478B38) result = lbl_80478B38 - 1;
    menuSubCloseNumberInput();
    return (s32)result;
}
#endif

/* =======================================================================
 * _dbgMenuFightGetZokuseiDataIdSub -- GStask_LookupResourceById
 *
 * Looks up a resource by ID. If ID is 9, uses a fixed constant.
 * If ID >= the global count (lbl_80478B38), also uses the constant.
 * Otherwise, converts via zokuseiDataBiosGetName and passes to GSmsgGetGSchar.
 *
 * Address: 0x8000857C  Size: 0x5C (92 bytes)
 * ======================================================================= */
void* _dbgMenuFightGetZokuseiDataIdSub(u32 id)
{
    if (id == 9) {
        return GSmsgGetGSchar(0x0000EB63);
    }
    if (id >= lbl_80478B38) {
        return GSmsgGetGSchar(0x0000EB63);
    }
    {
        u32 idx;
        idx = (u16)id;
        return GSmsgGetGSchar(PC_PTR32(zokuseiDataBiosGetName(idx)));
    }
}

/* fn_800085D8 - 0x800085D8 | size: 0xb4 */
#if 1
void* fn_800085D8(s32 difficulty) {
    u32 id;
    if (difficulty >= 5) {
        id = 0xEB63;
    } else {
        u32 v;
        if (difficulty == 0)      v = 0x0000ED04;
        else if (difficulty == 1) v = 0x0000ED05;
        else if (difficulty == 2) v = 0x0000ED06;
        else if (difficulty == 3) v = 0x0000ED07;
        else if (difficulty == 4) v = 0x0000ED08;
        else                      v = 0x0000ED04;
        id = v;
    }
    if (id == 0) id = 0xEB63;
    return GSmsgGetGSchar(id);
}
#endif

/* fn_8000868C - 0x8000868C | size: 0x60 */
#if 1
void* fn_8000868C(u32 id) {
    u32 result;
    if (id >= *(u32*)lbl_80478F20) {
        result = 0xEB63;
    } else {
        result = (u32)fightTrainerGetStatus(0, (u16)id, 3, 0);
    }
    if (result == 0) result = 0xEB63;
    return GSmsgGetGSchar(result);
}
#endif

/* fn_800086EC - 0x800086EC | size: 0x58 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTypeDataBiosGetPtr(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTypeDataBiosGetName(void* ptr);
#endif
#if 1
void* fn_800086EC(u32 id) {
    u32 result;
    if (id >= *(u32*)PC_PTR(fight_type_data_number)) {
        result = 0xEB63;
    } else {
        result = PC_PTR32(fightTypeDataBiosGetName(fightTypeDataBiosGetPtr((u16)id)));
    }
    if (result == 0) result = 0xEB63;
    return GSmsgGetGSchar(result);
}
#endif

/* _dbgMenuFightGetFightKindDataIdSub - 0x80008744 | size: 0x58 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightKindDataBiosGetPtr(u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightKindDataBiosGetName(void* ptr);
#endif
#if 1
void* _dbgMenuFightGetFightKindDataIdSub(u32 id) {
    u32 result;
    if (id >= *(u32*)PC_PTR(fight_kind_data_number)) {
        result = 0xEB63;
    } else {
        result = PC_PTR32(fightKindDataBiosGetName(fightKindDataBiosGetPtr((u16)id)));
    }
    if (result == 0) result = 0xEB63;
    return GSmsgGetGSchar(result);
}
#endif

/* _dbgMenuFightGetFightFloorDataIdSub - 0x8000879C | size: 0x60 */
#if 1
void* _dbgMenuFightGetFightFloorDataIdSub(u32 id) {
    u32 result;
    if (id >= *(u32*)PC_PTR(fight_floor_data_number)) {
        result = 0xEB63;
    } else {
        result = (u32)fightFloorGetStatus(0, (u16)id, 1, 0);
    }
    if (result == 0) result = 0xEB63;
    return GSmsgGetGSchar(result);
}
#endif

/* =======================================================================
 * fn_800087FC -- GStask_LookupEventById
 *
 * Looks up an event resource by ID. Returns NULL-equivalent constant
 * on invalid input or out-of-range. Otherwise, chains through
 * fightEncountDataBiosGetPtr -> fightEncountDataBiosGetFightName -> GSmsgGetGSchar.
 *
 * Address: 0x800087FC  Size: 0x6C (108 bytes)
 * ======================================================================= */
void* fn_800087FC(u32 id)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightEncountDataBiosGetPtr(u16 id);
#endif
    extern u32 fight_encount_data_number;
    u32 result;

    if (id == 0) {
        result = 0x0000EB63;
    } else if (id >= *(u32*)PC_PTR(fight_encount_data_number)) {
        result = 0x0000EB63;
    } else {
        result = PC_PTR32(fightEncountDataBiosGetFightName(fightEncountDataBiosGetPtr((u16)id)));
    }
    if (result == 0) {
        result = 0x0000EB63;
    }

    return GSmsgGetGSchar(result);
}
#endif /* GS_TASK_RANGE_EXACT_8000765C */
