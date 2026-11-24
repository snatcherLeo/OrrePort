/**
 * @file gs_title.c
 * @brief GStitle -- Title screen, autodemo, and intro sequence.
 *
 * Address range: 0x80020328 - 0x80026000 (~80 functions)
 *
 * This module manages the title screen and pre-game sequences:
 *   - Title logo display and animation
 *   - "Press Start" prompt with blink animation
 *   - Autodemo playback timer and trigger
 *   - New Game / Continue selection
 *   - Intro cutscene orchestration
 *   - Camera fly-through for title background
 *   - Sound effect scheduling for title events
 *
 * Key functions:
 *   titleExit  GStitle_Cleanup              -- 0x8C bytes, clean up title resources
 *   titleMain  GStitle_MainLoop             -- 0xE8 bytes, title screen main loop
 *   titleInit  GStitle_Init                 -- 0xF0 bytes, initialize title screen
 *   fn_8002058C  GStitle_ThreadEntry          -- 0x2C bytes, title thread entry point
 *   fn_800205B8  GStitle_SetMode              -- 8 bytes, set title mode (stw + blr)
 *   fn_800205C0  GStitle_SetDemoPtr           -- 8 bytes, set autodemo pointer
 *   fn_800205C8  GStitle_CheckAutodemo        -- 0x44 bytes, check autodemo timer
 *   fn_8002060C  GStitle_ResetTimer           -- 0xC bytes, reset autodemo countdown
 *   fn_80020618  GStitle_RenderFrame          -- 0x304 bytes, render one title frame
 *   titleGetState  GStitle_GetState             -- 0x10 bytes, return title state
 *   fn_8002092C  GStitle_AnimateLogo          -- 0x90 bytes, logo bounce/spin
 *   fn_800209BC  GStitle_AnimatePrompt        -- 0x90 bytes, "Press Start" blink
 *   fn_80020A4C  GStitle_AnimateBackground    -- 0x90 bytes, background scroll
 *   fn_80020ADC  GStitle_FadeIn               -- 0x58 bytes, fade from black
 *   fn_80020B34  GStitle_FadeOut              -- 0x58 bytes, fade to black
 *   fn_80020B8C  GStitle_GetFadeState         -- 0x14 bytes, return fade progress
 *   menuOptionDrawSoundMode  GStitle_HandleInput          -- 0xFC bytes, Start/A button handler
 *   titleNewContinueSelect  GStitle_MenuSelect           -- 0x200 bytes, New/Continue/Options menu
 *   fn_80020E9C  GStitle_ReturnZero           -- 8 bytes, stub
 *   titleDrawOptionHelp  GStitle_OptionsMenu          -- 0xB0 bytes, options sub-menu
 *   titleDrawContinueInfo  GStitle_SaveFileSelect       -- 0x19C bytes, save file selection
 *   titleMenuMain  GStitle_LoadSaveFile         -- 0x4D4 bytes, load save from memcard
 *   titleTopMenuExit  GStitle_NewGameSetup         -- 0x60 bytes, initialize new game
 *   titleTopMenuMain  GStitle_SetDifficulty        -- 0x20 bytes, set game difficulty
 *   titleIntroStart  GStitle_IntroSequence        -- 0x9C bytes, start intro sequence
 *   fn_800216E0  GStitle_SkipIntro            -- 8 bytes, set skip flag
 *   itemUseFormatMessage  GStitle_NameEntry            -- 0x1D4 bytes, player name input screen
 *   (corrected 2026-11-08: the name-entry / keyboard roles once listed for these were guesses; they are
 *   item-use handlers, roles from the items that use each one)
 *   itemFieldUsePlayTimeFlute  -- 0x1E0 bytes, the TIME FLUTE (renamed 2026-11-08 from an older "Cologne" name)
 *   itemFieldUseCheckUsableList  -- 0x78 bytes, the key items: only if the item is in the "usable now" list
 *   itemFieldUseRecover  (item-use handler)           -- 0x53C bytes, field item use on a party Pokemon (see note at definition)
 *   itemFieldUseBall  -- 0x12C bytes, the Poke Balls (battle slot)
 *   itemBattleUseApplyXItem  -- 0x2FC bytes, the X items, Guard Spec and Dire Hit (battle slot)
 *   itemFieldUseParamItem  (item-use handler)           -- 0x2A8 bytes, field item use dispatch (see note at definition)
 *   itemFieldUseReadEinFile  -- 0x114 bytes, the Ein File reader (corrected 2026-11-06: not a title camera function)
 *   itemFieldUseTM  (corrected 2026-10-17: not a camera spline; it teaches a TM / HM move:
 *                looks up the machine's move and checks the species' TM list, then learns it) -- 0x308 bytes
 *   fn_80022B3C  GStitle_ParticleEffects      -- 0x318 bytes, title particle system
 *   (corrected 2026-11-08: the next eight are the item-use handlers of XD's menuUseItem.c, in the same order;
 *   the particle / title-music roles once listed here were guesses. Roles from the items that use each one.)
 *   menuUseItemCologn  -- 0x90 bytes, the Scents
 *   menuUseItemEvolution  -- 0x184 bytes, the evolution stones
 *   menuUseItemWPointUp  -- 0x20C bytes, PP UP / PP MAX
 *   cbForgetWazaSelect__FP7PokemonUsl  -- 0x7C bytes, forget-a-move callback (same name in XD)
 *   menuUseItemListLVUp  -- 0x470 bytes, RARE CANDY
 *   menuUseItemAlldeathRcv  -- 0x208 bytes, SACRED ASH
 *   menuUseItemPPRecovery  -- 0x234 bytes, Ether / Max Ether / Elixir / Leppa Berry
 *   menuUseItemRecovery  -- 0x20C bytes, Potions, status heals, Revives, vitamins
 *   itemFieldUseForbidden  GStitle_GetBGMState          -- 0x3C bytes
 *   itemFieldUseMeaningless  GStitle_GetSEState           -- 0x3C bytes
 *   itemFieldUseNotNow  GStitle_GetFadeState2        -- 0x3C bytes
 *   fn_80023E5C  GStitle_Nop                  -- 4 bytes, nop
 *   fn_80023E60  GStitle_AutodemoPlayback     -- 0x300 bytes, replay input recording
 *   fn_80024160  GStitle_AutodemoRecord       -- 0x1A8 bytes, record input (debug)
 *   fn_80024308  GStitle_AutodemoSetup        -- 0x130 bytes, setup autodemo
 *
 * titleMain (GStitle_MainLoop) structure:
 *   while (1) {
 *       if (gTitleState == 0x28) {  // TITLE_STATE_AUTODEMO_TIMEOUT
 *           floorLink(0x39C, 0);  // load autodemo camera
 *           gTitleState = 0x3E8;   // TITLE_STATE_PLAY_AUTODEMO
 *           continue;
 *       }
 *       if (!gIsInitialized) continue;
 *       if (gDemoPtr == NULL) continue;
 *       // Accumulate camera float position
 *       gCamAngle += gCamDelta;
 *       if (gCamPhase < 2 && !gPaused) {
 *           // Check distance threshold for camera movement
 *           if (dist >= threshold[gCamPhase]) {
 *               if (gCamPhase == 0)
 *                   fn_80166AB8(0x46E, 0, 0);  // Play whoosh SE
 *               else
 *                   fn_801669E4(0x46E, 0, 0);  // Play landing SE
 *               gCamPhase++;
 *           }
 *       }
 *       _threadSwitch();  // Frame advance
 *   }
 *
 * titleInit (GStitle_Init):
 *   - Clears stale state flags
 *   - Creates the title thread via GSthreadCreate (priority 0x14, stack 0x2000)
 *   - Initializes camera position from lbl_803A1F88 float constants
 *   - Checks save file existence via fn_800FF548
 *   - Calls fn_8002060C if no save found
 *
 * SDA globals:
 *   lbl_8047A310: Title mode (s32)
 *   lbl_8047A314: Frame counter for autodemo timer
 *   lbl_8047A31C: Current title state (s32)
 *   lbl_8047A320: Initialized flag
 *   lbl_8047A324: Demo data pointer
 *   lbl_8047A328: Pause flag
 *   lbl_8047A32C: Thread completion flag (u8)
 *   lbl_8047A330: Title thread handle
 *   lbl_8047A33C: Camera phase counter
 *   lbl_8047A340: Camera angle accumulator (float)
 *   lbl_8047A344: Camera delta per frame (float)
 *   lbl_8047A350: Autodemo playback pointer
 *   lbl_80478878: Frame cycle counter (wraps at 4)
 *
 * BSS globals:
 *   lbl_803A1F88: Camera position array (4 entries, 4 floats each)
 *
 * Rodata (sdata2 float constants):
 *   lbl_8047B814: 0.0f (zero)
 *   lbl_8047B84C: Camera X start position
 *   lbl_8047B850: Camera Y start position
 *   lbl_8047B854: Camera Z position
 *   lbl_8047B858: Camera target X
 *   lbl_8047B85C: Camera target Y
 *   lbl_8047B860: Camera target Z
 *
 * --------------------------------------------------------------------------
 * XD-decomp cross-reference (TeamOrre/xd-decomp Pokemon XD: Gale of Darkness)
 *   Pokemon XD is the direct sequel built on the same Genius Sonority engine.
 *   XD's `game/menuTitle.cpp` (0x800A3150 - 0x800A423C, 0x10EC bytes) is the
 *   probable original-name analogue of THIS file's title-screen functions.
 *   XD is C++; Colosseum is C. Sizes differ because Colosseum's title is
 *   richer (camera fly-through, intro sequence, autodemo recording).
 *
 *   XD title symbols (selected, by behavior, NOT by byte match):
 *     menuTitleInit              0x104   - title screen initialization
 *     menuTitleMain              0x050   - per-frame main loop
 *     menuTitleFunc              0x0D8   - state dispatch
 *     menuTitle                  0x330   - top-level driver
 *     menuTitleHook              0x074
 *     menuTitleButton            0x07C
 *     menuTitleNormalButton      0x074
 *     menuTitleCursorAnime       0x0DC   - cursor blink/animation (~ fn_80024CDC)
 *     menuTitleCursorAnimeMain   0x07C
 *     menuTitleOpenSelectAnime   0x038
 *     menuTitleCloseSelectAnime  0x038
 *     menuTitlePlayBGM           0x040
 *     menuTitlePlayBGMWait       0x090
 *     menuTitleExit              0x074
 *     menuTitleSetSE             0x028
 *     menuTitleBattleSelectFree  0x034
 *     menuTitleBattlePreReadCheck 0x034
 *     menuTitlePreReadWait       0x030
 *     menuTitleReadWazaSelectMenu 0x038
 *     menuTitleSetStartStatus    0x008
 *     menuTitleClose2            0x054
 *     menuTitleOpenMenu          0x044
 *     menuTitleGetSelect / SetSelect  0x018 each
 *     menuTitlePreReadFightRegWzxData 0x058
 *     menuTitleOptionCtrl        0x0DC   - in separate menuTitleOption.cpp
 *     menuTitleOptionCursorControl 0x16C
 *
 *   HSD (HAL Sonata Driver) helpers used by both games:
 *     HSD_JObjSetupMatrix, HSD_JObjMtxIsDirty, modelIntpJObj*,
 *     cameraGetFrameCount, lightGetFrameCount, _modelLoad
 *
 *   Original-name candidates for our extern fn_XXXXXXXX:
 *     menuItemBiosGetPtr / menuDataBiosGetPtr -- linked-list/menu accessors
 *     fn_800D3088 / fn_800D37CC -- frame timing (returns u32/s32 ticks)
 *     fn_800E0CA0 / GSlerpGetLinearInterpolationVector -- vec3 transform helpers
 *     GSmsgGetGSchar / msgctrlSetValue -- message/window callbacks
 *     winMsgOpen / winMsgClose -- text print + wait
 *     fn_801902E0 -- config flag check (returns u8)
 *     fn_80165A20 / soundStop -- audio sequence control
 *     fadeCheck / fadeSet -- BGM volume/fade
 *
 * --------------------------------------------------------------------------
 */

#include "dolphin/types.h"
#include "game/constants/menu_keys.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC */
#include "game/constants/pokemon_status.h"
#include "game/constants/rooms.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_OptionsScreen / OrreHook_OptionsKeys: the Options screen's Language row */
#endif

/* Callees used across the title-screen carves, with their definitions' signatures. */
extern s8 fadeCheck(u8 wait);
extern void itemUseGetTargetPokemon(s32 arg, s32* out1, s32* out2);
extern void itemUseClosePartyMenu(u32 value);
extern s32 itemUseOpenPartyMenu(s32 arg0, s32 arg1);
#ifdef TARGET_PC
extern void menuPokemonClose(void); /* OrreHook_PartyListHold's ORRE_HOLD_LEAVE closes a held party list (step 3.7) */
#endif
extern s32 fn_800D37CC(void);
extern void msgctrlSetValue(u32 id, u32 value);
extern u8 pokemonCheckValid(u8* ptr);
extern s32 winMsgOpen(s8 type, u32 message, u32 wait, u8 pause);
extern void winSpriteSetDisp(void* node, u32 enable);
s32 titleGetState(void);
void titleNewContinueSelect(void);

#if defined(GS_TITLE_SPLIT)
#pragma force_active on
#endif

/* =========================================================================
 * External declarations
 * ========================================================================= */

/* Thread management */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSthreadCreate(s32 priority, void* stack, s32 stackSize,
                            s32 flags, s32 p5, void (*entry)(void));
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  _threadSwitch(void);
#endif
extern void  GSthreadTerminate(void* threadCtx);

/* Scene/camera */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  floorLink(s32 cameraId, s32 mode);
#endif
extern u8    fn_800FF548(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  std092_getCurrentRoomID(s32 floorId);
#endif

/* Sound */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  soundStartRaw(s32 soundId, s32 p2, s32 p3);
#endif
extern void  soundStopRaw(s32 soundId, s32 p2, s32 p3);

/* Save system */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  floorSetFadeScript(s32 p1, s32 p2);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  menuCloseCustom(s32 menuId, s32 mode, s32 force);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winMsgClose(s32 wait);
#endif

/* Input */
extern void  fightPreloadResources(s32 result);

/* Math */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f32   GSmodelGetAnimFrame(void* model);        /* Get distance */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 sin(f32);
#endif

/* Named asm symbols (used in inline asm .inc files) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuModelSetMotion(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemParamConvertOrigFormat(void* dst, void* src);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemParamGetRecoverType(void* param);
#endif
extern s32 pcboxGetPokemonBoxNbEmptySlot(s32, s8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgfxBeginBackFBCapture(void);
#endif

/* =========================================================================
 * SDA globals
 * ========================================================================= */

extern s32   gTitleMode;         /* lbl_8047A310 */
extern s32   gTitleState;        /* lbl_8047A31C */
extern s32   gTitleInitFlag;     /* lbl_8047A320 */
extern void* gAutodemoPtr;       /* lbl_8047A324 */
extern s32   gTitlePaused;       /* lbl_8047A328 */
extern u8    gTitleThreadDone;   /* lbl_8047A32C */
extern void* gTitleThreadHandle; /* lbl_8047A330 */
extern s32   gCameraPhase;      /* lbl_8047A33C */
extern f32   gCameraAngle;      /* lbl_8047A340 */
extern f32   gCameraDelta;      /* lbl_8047A344 */
extern void* gDemoPlaybackPtr;   /* lbl_8047A350 */
extern s32   gFrameCycleCount;   /* lbl_80478878 */

/* =========================================================================
 * Function: GStitle_Init
 * Address:  0x8002049C
 * Size:     0xF0
 * ========================================================================= */

/* External declarations required by titleInit.
 *
 * The sdata2 float anchors (lbl_8047B814..lbl_8047B860) and the sbss/bss
 * anchors (lbl_8047A344, lbl_803A1F88) reference data whose ownership lives
 * outside this translation unit (see split map + symbols.txt).
 * These externs are the standard access pattern for cross-TU sdata2/bss
 * anchors -- they do not create new data, they just name existing symbols.
 */
extern u8 lbl_8047A32C;
extern void* lbl_8047A330;
extern s32 lbl_8047A314;
extern void* lbl_8047A324;
extern s32 lbl_8047A31C;
extern s32 lbl_8047A320;
extern s32 lbl_8047A328;
extern s32 lbl_8047A33C;
extern f32 lbl_8047A340;
extern f32 lbl_8047A344;
extern u32 lbl_80478878;
extern u8 lbl_803A1F88[];
extern f32 lbl_8047B814;
extern f32 lbl_8047B84C;
extern f32 lbl_8047B850;
extern f32 lbl_8047B854;
extern f32 lbl_8047B858;
extern f32 lbl_8047B85C;
extern f32 lbl_8047B860;
extern u32 lbl_8047B808;
extern u32 lbl_8047B80C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* floorGetCurrentContext(void);
#endif

/* Forward declarations for same-TU callees used before their definitions. */
void fn_8002058C(void);
void fn_8002060C(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void itemUseFormatMessage(void*, s32, u8*, s16, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonIsDarkPokemon(s32);
#endif
extern f32 pokemonGetDp(void*);
extern u8 lbl_803A1B90[];

/* titleInit now lives in gs_title_8001EF78.c (the head of this TU). */

/* =========================================================================
 * Function: GStitle_MainLoop
 * Address:  0x800203B4
 * Size:     0xE8
 * ========================================================================= */

/* titleMain now lives in gs_title_8001EF78.c (the head of this TU). */

/* =========================================================================
 * Function: GStitle_Cleanup
 * Address:  0x80020328
 * Size:     0x8C
 *
 * Cancels active events on slots 0x13, 0x15, 0x16, closes any open
 * message boxes, then waits for the title thread to complete.
 * ========================================================================= */

/* titleExit now lives in gs_title_8001EF78.c (the head of this TU). */

/* ===================================================================
 * AUTO-GENERATED accessor functions
 * Generated by tools/gen_accessors.py
 * 4 functions matched
 * =================================================================== */

extern u32 lbl_8047A310;
extern u32 lbl_8047A350;
extern u32 lbl_8047A358;

/* Address: 0x800205B8 | Size: 0x8 | Pattern: sda_setter */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800205B8)
void fn_800205B8(u32 val) {
    lbl_8047A310 = val;
}
#endif

/* Address: 0x800205C0 | Size: 0x8 | Pattern: sda_setter */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800205C0)
void fn_800205C0(u32 val) {
    lbl_8047A350 = val;
}
#endif

/* Address: 0x80020E9C | Size: 0x8 | Pattern: return_constant */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80020E9C)
u32 fn_80020E9C(void) { return 0; }
#endif

/* Address: 0x800216E0 | Size: 0x8 | Pattern: sda_setter */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800216E0)
void fn_800216E0(u32 val) {
    lbl_8047A358 = val;
}
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuItemBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif

/* Menu table at lbl_80478DDC (count at *lbl_80478DD8), 0x10 per entry; +0x0 is the story flag that shows the entry.
 * Both title tables are common_rel data: big-endian on the PC (DISC_STRUCT). */
#ifdef TARGET_PC
/* the table's count (*lbl_80478DD8) is common_rel data too: big-endian (read natively, 9 became 0x09000000 and the world
 * map's loop ran past the table into garbage flags, golden-slice M21, 2026-10-17) */
#define TITLE_TABLE_COUNT(p) (DISC_U32(p))
#else
#define TITLE_TABLE_COUNT(p) (*(u32*)(p))
#endif
#ifdef TARGET_PC
#define TITLE_ENTRY_FLAG(ctx, off) ((s32)DISC_U32((u8*)(ctx) + (off)))
#else
#define TITLE_ENTRY_FLAG(ctx, off) PC_PTR32_IMPLICIT(*(void**)((u8*)(ctx) + (off)))
#endif
typedef struct DISC_STRUCT TitleMenuEntry {
    u32 unk0;
    u32 id;
    u32 item;
    u32 value;
} TitleMenuEntry;

/* Title slot table at lbl_80478DE4 (count at *lbl_80478DE0), 0x28 per slot. */
typedef struct DISC_STRUCT TitleSlot {
    u32 flag;
    u32 msg[9];
} TitleSlot;

/* The index-th visible item of the menu, or NULL. */
static inline u8* titleGetMenuItemAt(u8* menu, s32 index) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
    u8* result;
    s32 count;
    u8* item;

    item = menuItemBiosGetPtr(DISC_S16(menuDataBiosGetPtr(*(u32*)(menu + 4)) + 4));
    count = 0;
    while (1) {
        if (((u32)*(volatile u8*)item >> 7) & 1) {
            if (index == count) {
                result = item;
                break;
            }
            count++;
        }
        if ((((u32)*(volatile u8*)item >> 6) & 1) == 0) {
            item = menuItemBiosGetPtr(DISC_S16(item + 0x18));
        } else {
            result = NULL;
            break;
        }
    }
    return result;
}

/* The visible item of the menu at the cursor position *indexp. */
static inline u8* titleGetMenuItem(u8* menu, u32* indexp) {
    s32 index = *indexp;

    return titleGetMenuItemAt(menu, index);
}

/* Index of the table entry whose item is the given one, or 0. */
static inline s32 titleFindEntry(u8* item) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
    extern u32 lbl_80478DD8;
    extern u32 lbl_80478DDC;
    u32 i;

    for (i = 0; i < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8)); i++) {
        if (item == menuItemBiosGetPtr((*(TitleMenuEntry**)&lbl_80478DDC)[i].item)) {
            return i;
        }
    }
    return 0;
}

/* itemFieldUseForbidden - 0x80023DA8 | size: 0x3c */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80023DA8)
s32 itemFieldUseForbidden(void) {
    winMsgOpen(2, 0x44c6, 1, 0);
    winMsgClose(1);
    return 1;
}

/* itemFieldUseMeaningless - 0x80023DE4 | size: 0x3c */
s32 itemFieldUseMeaningless(void) {
    winMsgOpen(2, 0x4261, 1, 0);
    winMsgClose(1);
    return 1;
}

/* itemFieldUseNotNow - 0x80023E20 | size: 0x3c */
s32 itemFieldUseNotNow(void) {
    winMsgOpen(2, 0x426a, 1, 0);
    winMsgClose(1);
    return 1;
}

/* fn_80023E5C - 0x80023E5C | size: 0x4 */
void std018_nop18(void) { }
#endif

/* fn_80023E60 - 0x80023E60 | size: 0x300 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetCursorToItem(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecNormalize(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecDot(void);
#endif
extern u32 lbl_8047A370;
extern u32 lbl_8047A368;
extern u32 lbl_8047A36C;
extern f32 lbl_80478898;
extern u32 lbl_8047A390;
extern u8 lbl_8047A380;
extern void wmapCursorMove(u8*, void*, u16*, u8*);
extern u8 lbl_803A2058[];
extern u8 lbl_803A204C[];
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80023E60)
s32 wmapMenuUpdate(u8* arg0) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetCursorToItem(u8*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecNormalize(f32*, f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 GSvecDot(f32*, f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic(s32, s32, s32);
#endif
    u8* da18;
    u8* r104318;
    u16* temp_r5;
    s32 result;
    s32 target;
    u8* p;
    f32 sp14[3];
    f32 sp08[3];

    if ((s32)lbl_8047A370 == 1) {
        return 0;
    }

    da18 = menuDataBiosGetPtr(*(u32*)(arg0 + 4));
    r104318 = (u8*)PC_PTR(windowGetCursorToItem(arg0));

    if (r104318 == NULL) {
        arg0[0x95] = 0;
        return 0;
    }

    temp_r5 = windowGetKeyInfo();
    wmapCursorMove(arg0, r104318, temp_r5, da18);

    result = (s32)(s8)arg0[0x95] + (s32)(s8)arg0[0x94];
    target = (s32)lbl_8047A368;
    lbl_8047A36C = result;

    if ((u32)target != (u32)result) {
        lbl_80478898 = 90.0f;
        lbl_8047A370 = 1;

        p = titleGetMenuItemAt(arg0, target);
        if (p != NULL) {
            /* the item's x / y: menu items are common_rel data, big-endian on the PC (as +0x18 is read) */
            *(f32*)(lbl_803A2058 + 0) = (f32)(s32)(DISC_S16(p + 2));
            *(f32*)(lbl_803A2058 + 4) = (f32)(s32)(DISC_S16(p + 4));
            *(f32*)(lbl_803A2058 + 8) = 0.0f;
        }

        p = titleGetMenuItemAt(arg0, (s32)lbl_8047A36C);
        if (p != NULL) {
            *(f32*)(lbl_803A204C + 0) = (f32)(s32)(DISC_S16(p + 2));
            *(f32*)(lbl_803A204C + 4) = (f32)(s32)(DISC_S16(p + 4));
            *(f32*)(lbl_803A204C + 8) = 0.0f;
        }

        {
            u8* cursor = (u8*)PC_PTR(lbl_8047A390);
            sp14[0] = *(f32*)(lbl_803A204C + 0) - (f32)(s32)(DISC_S16(cursor + 2));
            sp14[1] = *(f32*)(lbl_803A204C + 4) - (f32)(s32)(DISC_S16(cursor + 4));
            sp14[2] = 0.0f;
            if (0.0f != sp14[0] || 0.0f != sp14[1]) {
                GSvecNormalize(sp08, sp14);
                sp14[0] = 1.0f;
                sp14[1] = 0.0f;
                sp14[2] = 0.0f;
                if (GSvecDot(sp14, sp08) < 0.0f) {
                    lbl_8047A380 = 1;
                } else {
                    lbl_8047A380 = 0;
                }
            }
        }

        std104_soundSetBackgroundMusic(0x464, 0, 0xFF);
    }

    return 0;
}

/* fn_80024160 - 0x80024160 | size: 0x1a8 */
extern u32 lbl_8047A368;
extern u32 lbl_80478DF4;
/* PC: the world map's cursor links come from the room's data like the floor table below (TitleFloorEntry), so they are
 * read big-endian, count included (snatcherLeo's crash 2026-10-18: the native count ran the loop past the table, reading
 * 0x81800004 in wmapCursorMove; golden-slice M32) */
typedef struct DISC_STRUCT TitleMenuLink {
    u8 dirMask;
    u32 from;
    u32 to;
} TitleMenuLink;
extern u32 lbl_80478DF0;
void wmapCursorMove(u8* arg0, void* arg1, u16* arg2, u8* arg3) {
    u8* candidate;
    u8 mask;
    u8 visible_index;
    s32 entry_index;
    u8* active;
    s32 v;

    (void)arg1;
    v = *arg2;
    mask = 0;
    if ((v & 0x1) != 0) mask |= 0x1;
    if ((v & 0x2) != 0) mask |= 0x4;
    if ((v & 0x4) != 0) mask |= 0x8;
    if ((v & 0x8) != 0) mask |= 0x2;

    if (mask != 0) {
        active = titleGetMenuItemAt(arg0, lbl_8047A368);

        entry_index = 0;
        while ((u32)entry_index < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DF0))) {
            visible_index = 0;
            candidate = menuItemBiosGetPtr(DISC_S16(arg3 + 4)); /* the menu record (common_rel data), as titleGetMenuItemAt reads it */
            while (1) {
                if (candidate == 0) {
                    break;
                }
                if (((u32)*(volatile u8*)candidate >> 7) & 1) {
                    if ((((TitleMenuLink*)PC_PTR(lbl_80478DF4))[entry_index].dirMask & mask) == mask) {
                        if (menuItemBiosGetPtr(((TitleMenuLink*)PC_PTR(lbl_80478DF4))[entry_index].from) == active) {
                            if (menuItemBiosGetPtr(((TitleMenuLink*)PC_PTR(lbl_80478DF4))[entry_index].to) == candidate) {
                                arg0[0x95] = visible_index;
                                return;
                            }
                        }
                    }
                    visible_index++;
                }
                if ((((u32)*(volatile u8*)candidate >> 6) & 1) == 0) {
                    candidate = menuItemBiosGetPtr(DISC_S16(candidate + 0x18));
                } else {
                    break;
                }
            }
            entry_index++;
        }
    }
}

/* fn_80024308 - 0x80024308 | size: 0x130
 *
 * Status: 99.1% matched. Remaining: reg-alloc choice in second branch
 * (f0/f1/f2 ordering: B8C8 in f1 vs f2, stack in f2 vs f0, B8898 in f0 vs f1),
 * plus 2 anonymous @NNN@sda21 f64 magic constants for u32->f32 conversion
 * (CW heuristic; target uses lbl_8047B8D0 name).
 * Second branch: separate f3=(f32)(u32)fn_800D3088() to avoid __cvt_fp2unsigned.
 */
extern u32 lbl_8047A370;
extern f32 lbl_8047A378;
extern f32 lbl_8047A374;
extern f32 lbl_80478898;
extern u32 lbl_8047A36C;
extern u32 lbl_8047A368;
s32 wmapMenuInput(u8* arg0) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* windowGetKeyInfo(void);
#endif
    u8* ctx;
    f32 f0;
    f32 f1;
    f32 f2;
    f32 f3;

    ctx = windowGetKeyInfo();
    if (arg0 != 0) {
        if ((s32)lbl_8047A370 != 1) {
            if ((*(u16*)(ctx + 4) & MENU_KEY_A) != 0) {
                arg0[0x98] = 1;
            }
        }
        if ((*(u16*)(ctx + 4) & MENU_KEY_B) != 0) {
            arg0[0x98] = 1;
            arg0[0x99] = 1;
        }
    }

    f2 = (f32)(u32)GSgetFrameStep();
    f3 = lbl_8047A378;
    f0 = f3 * f2 + lbl_8047A374;
    lbl_8047A374 = f0;
    f1 = 12.0f;
    if (f0 < f1) {
        lbl_8047A374 = f1;
        lbl_8047A378 = -f3;
    } else {
        f1 = 20.0f;
        if (f0 > f1) {
            lbl_8047A374 = f1;
            lbl_8047A378 = -f3;
        }
    }

    if ((s32)lbl_8047A370 == 1) {
        f3 = (f32)(u32)GSgetFrameStep();
        f1 = lbl_80478898 - 4.5f * f3;
        lbl_80478898 = f1;
        if (f1 < 0.0f) {
            lbl_80478898 = 0.0f;
            lbl_8047A370 = 0;
            lbl_8047A368 = lbl_8047A36C;
        }
    }
    return 0;
}
#endif


/* =========================================================================
 * Stubs for remaining GStitle functions (0x80024438-0x80025F84)
 * ========================================================================= */

extern u32 lbl_80478DDC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 std081_flagCheckTrue(void*);
#endif

/* 0x4C | fn_80024438 | check_then_call */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80024438)
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_80024438(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x80));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_80024438(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x80));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_80024484 | check_then_call */
/* fn_80024484 - 0x80024484 | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_80024484(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x70));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_80024484(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x70));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_800244D0 | check_then_call */
/* fn_800244D0 - 0x800244D0 | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_800244D0(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x60));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_800244D0(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x60));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_8002451C | check_then_call */
/* fn_8002451C - 0x8002451C | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_8002451C(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x50));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_8002451C(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x50));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_80024568 | check_then_call */
/* fn_80024568 - 0x80024568 | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_80024568(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x40));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_80024568(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x40));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_800245B4 | check_then_call */
/* fn_800245B4 - 0x800245B4 | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_800245B4(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x30));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_800245B4(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x30));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_80024600 | check_then_call */
/* fn_80024600 - 0x80024600 | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_80024600(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x20));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_80024600(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x20));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x4C | fn_8002464C | check_then_call */
/* fn_8002464C - 0x8002464C | size: 0x4c */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_8002464C(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x10));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp(PC_PTR(arg2), 0);
    }
}
#else
void fn_8002464C(u32 arg1, u32 arg2) {
    void* ctx = *(void**)&lbl_80478DDC;
    u32 result = (u32)std081_flagCheckTrue(TITLE_ENTRY_FLAG(ctx, 0x10));
    if ((result & 0xFF) == 0) {
        winSpriteSetDisp((void*)arg2, 0);
    }
}
#endif

/* 0x80024698 | 0x4 -- nop */
void fn_80024698(void) { }

/* 0x8002469C | 0x30 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32);
#endif
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_8002469C(void) {
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0x3cdf)));
}
#else
void fn_8002469C(void) {
    msgctrlSetValue(0x37, (u32)GSmsgGetGSchar(0x3cdf));
}
#endif

/* 0x800246CC | 0x30 */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void fn_800246CC(void) {
    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(0x3ce4)));
}
#else
void fn_800246CC(void) {
    msgctrlSetValue(0x37, (u32)GSmsgGetGSchar(0x3ce4));
}
#endif
#endif

/* 0x800246FC | 0x330
 *
 * fn_800246FC -- XD analogue: menuTitle (the top-level 0x330 driver).
 *
 * Walks an array of menu slot records at lbl_80478DE4 (count in lbl_80478DE0,
 * stride 0x28). For each slot whose condition passes (slot entry == 0 OR
 * fn_801902E0(entry) returns non-zero), it performs three passes of the
 * linked-list walk + lookup table scan:
 *   case 1/subcase 1 (lbl_8047A370 == 1 && lbl_80478898 > lbl_8047B8D8):
 *       uses lbl_8047A368 as target index; alpha = lbl_8047B8DC * ((t-b)/b)
 *   case 1/subcase 2 (lbl_8047A370 == 1 && lbl_80478898 <= lbl_8047B8D8):
 *       uses lbl_8047A36C as target index; alpha = lbl_8047B8DC * ((b-t)/b)
 *   default (lbl_8047A370 != 1):
 *       uses lbl_8047A368; alpha = 0xFF (fully opaque)
 * After matching, writes alpha to arg1[0x67] and invokes msgctrlSetValue(0x37,..)
 * with a value pulled from lbl_80478DE4 indexed by lbl_802E4F58[table_index].
 *
 * The three inner blocks are the titleGetMenuItem/titleFindEntry inlines.
 *
 * Status: exact.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuDataBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuItemBiosGetPtr(u32);
#endif
extern TitleSlot* lbl_80478DE4;
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u32 lbl_8047A368;
extern u32 lbl_80478DDC;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A36C;
extern u8 lbl_802E4F58[];
extern u32 lbl_80478DE0;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800246FC)
static inline s32 titleUpdateCursor(u8* menu, u8* cursor) {
    extern f32 lbl_80478898;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A36C;
    extern u32 lbl_8047A370;
    s32 index;
    u8 alpha;

    switch ((s32)lbl_8047A370) {
    case 1:
        if (lbl_80478898 > 45.0f) {
            index = titleFindEntry(titleGetMenuItem(menu, &lbl_8047A368));
            alpha = 255.0f * ((lbl_80478898 - 45.0f) / 45.0f);
        } else {
            index = titleFindEntry(titleGetMenuItemAt(menu, lbl_8047A36C));
            alpha = 255.0f * ((45.0f - lbl_80478898) / 45.0f);
        }
        break;
    default:
        index = titleFindEntry(titleGetMenuItemAt(menu, lbl_8047A368));
        alpha = 0xFF;
        break;
    }
    cursor[0x67] = alpha;
    return index;
}

static inline u32 titleSlotMessage(u32 slot, u32 index) {
    extern u8 lbl_802E4F58[];
    extern TitleSlot* lbl_80478DE4;

    if (index > 9) {
        index = 0;
    }
    return lbl_80478DE4[slot].msg[lbl_802E4F58[index]];
}

/* RULE-EXCEPTION(title-path): walk-helper choice (address vs value) made only for register assignment — see docs/RULE_EXCEPTIONS.md */
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
void wmapDrawInfo(u8* arg0, u8* arg1) {
    extern u8 lbl_802E4F58[];
    extern u32 lbl_80478DD8;
    extern u32 lbl_80478DE0;
    extern TitleSlot* lbl_80478DE4;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std081_flagCheckTrue(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmsgGetGSchar(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32, void*);
#endif
    u32 slot;
    s32 index;

    for (slot = 0; slot < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DE0)); slot++) { /* big-endian common_rel count (2026-10-21) */
        if (lbl_80478DE4[slot].flag == 0 ||
            (u8)std081_flagCheckTrue(PC_PTR32_IMPLICIT((void*)PC_PTR(lbl_80478DE4[slot].flag))) != 0) {
            index = titleUpdateCursor(arg0, arg1);
            if ((u32)index < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
                msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(titleSlotMessage(slot, index))));
                return;
            }
            break;
        }
    }

    msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(1)));
}
#else
void wmapDrawInfo(u8* arg0, u8* arg1) {
    extern u8 lbl_802E4F58[];
    extern u32 lbl_80478DD8;
    extern u32 lbl_80478DE0;
    extern TitleSlot* lbl_80478DE4;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std081_flagCheckTrue(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* GSmsgGetGSchar(u32);
#endif
    u32 slot;
    s32 index;

    for (slot = 0; slot < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DE0)); slot++) { /* big-endian common_rel count (2026-10-21) */
        if (lbl_80478DE4[slot].flag == 0 ||
            (u8)std081_flagCheckTrue(PC_PTR32_IMPLICIT((void*)PC_PTR(lbl_80478DE4[slot].flag))) != 0) {
            index = titleUpdateCursor(arg0, arg1);
            if ((u32)index < *(u32*)lbl_80478DD8) {
                msgctrlSetValue(0x37, (u32)GSmsgGetGSchar(titleSlotMessage(slot, index)));
                return;
            }
            break;
        }
    }

    msgctrlSetValue(0x37, (u32)GSmsgGetGSchar(1));
}
#endif

/* 0x80024A2C | 0x178
 *
 * fn_80024A2C -- simplified version of fn_800246FC (one inner block only).
 * Walks the linked list to find the node whose counter-position matches
 * lbl_8047A368, then scans lbl_80478DDC[] for a matching pbVar3 and writes
 * either alpha computed via the fade curve OR 0xFF into arg1[0x67]. If the
 * table lookup succeeds, writes the corresponding entry value to arg1[0x58]
 * (special-cased to 0xC5F1200 when the entry id is 0x66 and fn_801902E0(0x45D)
 * returns non-zero - likely an "override" for a specific menu option).
 *
 * Status: exact.
 */
extern u32 lbl_8047A368;
extern u32 lbl_80478DDC;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
void fn_80024A2C(u8* arg0, u8* arg1) {
    s32 index;
    u32 value;

    index = titleFindEntry(titleGetMenuItem(arg0, &lbl_8047A368));
    switch ((s32)lbl_8047A370) {
    case 1:
        arg1[0x67] = 255.0f * (lbl_80478898 / 90.0f);
        break;
    default:
        arg1[0x67] = 0xFF;
        break;
    }

    if ((u32)index < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
        if ((*(TitleMenuEntry**)&lbl_80478DDC)[index].id == 0x66 && (u8)std081_flagCheckTrue(PC_PTR32_IMPLICIT((void*)0x45D)) != 0) {
            value = 0x0C5F1200;
        } else {
            value = (*(TitleMenuEntry**)&lbl_80478DDC)[index].value;
        }
        *(u32*)(arg1 + 0x58) = value;
    }
}

/* 0x80024BA4 | 0x138
 *
 * fn_80024BA4 -- same linked-list walk as fn_80024A2C, but without the alpha
 * computation. Just finds the menu item matching lbl_8047A36C and writes the
 * entry payload to arg1[0x58] (with the 0x66 + fn_801902E0(0x45D) override).
 *
 * Likely corresponds to a menu cursor hover-highlight update:
 *   arg0 = menu context, arg1 = cursor/sprite target receiving the entry
 *
 * Status: exact.
 */
extern u32 lbl_8047A36C;
extern u32 lbl_80478DDC;
extern u32 lbl_80478DD8;
void fn_80024BA4(u8* arg0, u8* arg1) {
    s32 index;
    u32 value;

    index = titleFindEntry(titleGetMenuItem(arg0, &lbl_8047A36C));
    if ((u32)index < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
        if ((*(TitleMenuEntry**)&lbl_80478DDC)[index].id == 0x66 && (u8)std081_flagCheckTrue(PC_PTR32_IMPLICIT((void*)0x45D)) != 0) {
            value = 0x0C5F1200;
        } else {
            value = (*(TitleMenuEntry**)&lbl_80478DDC)[index].value;
        }
        *(u32*)(arg1 + 0x58) = value;
    }
}
#endif

/* 0x80024CDC | 0xE0
 *
 * fn_80024CDC -- XD analogue: menuTitleCursorAnime (0xDC, close size match).
 * Animates a pulsing cursor/icon by:
 *   1. Copying position from lbl_8047A390 (cursor source) plus 8px offset
 *      into arg1[0x50]/arg1[0x52] (dst x/y, only when lbl_8047A390 set).
 *   2. Computing iVar4 = speed * tickCounter + currentAlpha.
 *   3. Clamping iVar4 to [0x40, 0xFF] and flipping lbl_8047A37C (speed)
 *      sign when hitting either rail - classic ping-pong animation.
 *   4. Writing clamped alpha back to arg1[0x67].
 *
 * Status: code exact. `iVar4 +=` keeps the alpha and the converted sum in
 * one variable (retail's r31); a separate `iVar4 = (s32)(... + iVar4)`
 * splits them (99.5%). Linked in gs_title_80023DA8.c, which owns the
 * int->float bias doubles (lbl_8047B8B8 / lbl_8047B8D0) in its pool.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
extern u32 lbl_8047A390;
extern f32 lbl_8047A37C;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80024CDC)
void fn_80024CDC(s32 arg0, u8* arg1) {
    f32 fVar2;
    s32 iVar4;
    u32 uVar3;

    if (lbl_8047A390 != 0) {
        /* the cursor item's x / y: menu items are big-endian on the PC (snatcherLeo 2026-10-18: crosshair in the wrong place) */
        *(s16*)(arg1 + 0x50) = DISC_S16((u8*)PC_PTR(lbl_8047A390) + 2) + 8;
        *(s16*)(arg1 + 0x52) = DISC_S16((u8*)PC_PTR(lbl_8047A390) + 4) + 8;
    }

    iVar4 = *(u8*)(arg1 + 0x67);          /* current alpha (0..255)    */
    uVar3 = GSgetFrameStep();                /* u32 tick counter          */
    fVar2 = lbl_8047A37C;                 /* f32 pulse speed           */
    iVar4 += fVar2 * (f32)uVar3;          /* one web: alpha stays in r31 */

    /* Clamp alpha to [0x40, 0xFF] and flip direction on rail.   */
    /* This produces the classic ping-pong / throb animation.   */
    if (iVar4 < 0x40) {
        iVar4 = 0x40;
        lbl_8047A37C = -fVar2;            /* reverse pulse direction   */
    } else if (0xFF < iVar4) {
        iVar4 = 0xFF;
        lbl_8047A37C = -fVar2;
    }

    *(u8*)(arg1 + 0x67) = (u8)iVar4;
}
#endif

/* 0x80024DBC | 0x170
 *
 * fn_80024DBC -- one of four "twin" functions. Fade-aware position writer.
 *   - case 1 (lbl_8047A370 == 1): call fn_800E0CA0(fade_amt) + GSlerpGetLinearInterpolationVector
 *     to interpolate a float-pair position into local_48[0..1]; convert to int.
 *   - default: linked-list walk to find the menu item at lbl_8047A368,
 *     pull its x (pbVar3+2) and y (pbVar3+4) shorts.
 * Then writes (s16)(base_x + float(x) + offset) to arg1[0x50]/[0x52].
 *
 * The four twin functions differ only in the sign applied to each axis:
 *   fn_80024DBC: +x +y
 *   fn_80024F2C: +x -y
 *   fn_8002509C: -x +y
 *   fn_8002520C: -x -y
 *
 * Status: 96.6% matched. Remaining diffs: @NNN@sda21 anonymous f64 bias
 * constants vs target lbl_8047B8B8@sda21 named refs.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f32 fn_800E0CA0(f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u8 lbl_803A2058[];
extern u8 lbl_803A204C[];
extern u32 lbl_80478DD8;
extern u32 lbl_8047A368;
extern f32 lbl_8047A374;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80024DBC)
void fn_80024DBC(s32 arg0, u8* arg1) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_80478DD8;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A370;
    extern f32 lbl_8047A374;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    s32 iVar2;
    u8 *pbVar3;
    s32 uVar5;
    s32 unaff_r28;
    s32 uVar4;
    s32 unaff_r31;
    f32 fB8E0;
    f32 local_48[4];

    switch ((s32)lbl_8047A370) {
    case 1:

        GSlerpGetLinearInterpolationVector(local_48, lbl_803A2058, lbl_803A204C, fn_800E0CA0(lbl_80478898));
        unaff_r28 = (s32)local_48[0];
        unaff_r31 = (s32)local_48[1];
        break;
    case 0:
    default:
        uVar4 = (s32)lbl_8047A368;
        if ((u32)uVar4 < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
            iVar2 = (s32)PC_PTR32(menuDataBiosGetPtr(*(u32*)PC_PTR((arg0 + 4))));
            pbVar3 = menuItemBiosGetPtr(DISC_S16(PC_PTR((iVar2 + 4)))); /* menu record: big-endian on the PC */
            uVar5 = 0;
            while (1) {
                if (((u32)*(volatile u8*)pbVar3 >> 7) & 1) {
                    if (uVar4 == uVar5) {
                        if (((!lbl_803A2058) && (!lbl_803A2058)) && (!lbl_803A2058)) {
                        }
                        goto LAB_80024E94;
                    }
                    uVar5 = uVar5 + 1;
                }
                if (((u32)*(volatile u8*)pbVar3 >> 6) & 1) break;
                pbVar3 = menuItemBiosGetPtr(DISC_S16(pbVar3 + 0x18));
            }
            pbVar3 = (u8*)0;
LAB_80024E94:
            if (pbVar3 != 0) {
                unaff_r28 = DISC_S16(pbVar3 + 2);
                unaff_r31 = DISC_S16(pbVar3 + 4);
            }
        }
        break;
    }
    /* (f32)(s32)(s16)x forces sign-extend via extsh before the   */
    /* xoris + stw magic f64 pattern for int-to-float conversion. */
    /* Outer (s16) cast (not (s32)!) avoids an extra pre-store    */
    /* extsh that target doesn't emit.                            */
    fB8E0 = 9.0f;
    *(s16*)(arg1 + 0x50) = (s16)((f32)(s32)(s16)unaff_r28 + lbl_8047A374 + fB8E0);
    *(s16*)(arg1 + 0x52) = (s16)((f32)(s32)(s16)unaff_r31 + lbl_8047A374 + fB8E0);
}

/* 0x80024F2C | 0x170
 *
 * fn_80024F2C -- twin of fn_80024DBC, using +offset on x and -offset on y.
 * Same structure: case-1 calls GSlerpGetLinearInterpolationVector to interpolate, default walks list.
 * See fn_80024DBC doc for full semantics.
 *
 * Status: 94.9% matched. Same SDA2 anonymous-constant blocker.
 */
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A368;
extern f32 lbl_8047A374;
void fn_80024F2C(s32 arg0, u8* arg1) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_80478DD8;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A370;
    extern f32 lbl_8047A374;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    s32 iVar2;
    u8 *pbVar3;
    s32 uVar5;
    s32 unaff_r28;
    s32 uVar4;
    s32 unaff_r31;
    f32 fB8E0;
    f32 local_48[4];

    switch ((s32)lbl_8047A370) {
    case 1:

        GSlerpGetLinearInterpolationVector(local_48, lbl_803A2058, lbl_803A204C, fn_800E0CA0(lbl_80478898));
        unaff_r28 = (s32)local_48[0];
        unaff_r31 = (s32)local_48[1];
        break;
    case 0:
    default:
        uVar4 = (s32)lbl_8047A368;
        if ((u32)uVar4 < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
            iVar2 = (s32)PC_PTR32(menuDataBiosGetPtr(*(u32*)PC_PTR((arg0 + 4))));
            pbVar3 = menuItemBiosGetPtr(DISC_S16(PC_PTR((iVar2 + 4)))); /* menu record: big-endian on the PC */
            uVar5 = 0;
            while (1) {
                if (((u32)*(volatile u8*)pbVar3 >> 7) & 1) {
                    if (uVar4 == uVar5) {
                        if (((!unaff_r28) && (!unaff_r28)) && (!unaff_r28)) {
                        }
                        goto LAB_80025004;
                    }
                    uVar5 = uVar5 + 1;
                }
                if (((u32)*(volatile u8*)pbVar3 >> 6) & 1) break;
                pbVar3 = menuItemBiosGetPtr(DISC_S16(pbVar3 + 0x18));
            }
            pbVar3 = (u8*)0;
LAB_80025004:
            if (pbVar3 != 0) {
                unaff_r28 = DISC_S16(pbVar3 + 2);
                unaff_r31 = DISC_S16(pbVar3 + 4);
            }
        }
        break;
    }
    fB8E0 = 9.0f;
    *(s16*)(arg1 + 0x50) = (s16)((f32)(s32)(s16)unaff_r28 + lbl_8047A374 + fB8E0);
    *(s16*)(arg1 + 0x52) = (s16)((f32)(s32)(s16)unaff_r31 - lbl_8047A374 + fB8E0);
}

/* 0x8002509C | 0x170
 *
 * fn_8002509C -- twin of fn_80024DBC. Uses -offset on x, +offset on y.
 * See fn_80024DBC doc.
 *
 * Status: 92.4% matched. Same SDA2 anonymous-constant blocker.
 */
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A368;
extern f32 lbl_8047A374;
void fn_8002509C(s32 arg0, u8* arg1) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_80478DD8;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A370;
    extern f32 lbl_8047A374;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    s32 iVar2;
    u8 *pbVar3;
    s32 uVar5;
    s32 unaff_r28;
    s32 uVar4;
    s32 unaff_r31;
    f32 fB8E0;
    f32 local_48[4];

    switch ((s32)lbl_8047A370) {
    case 1:

        GSlerpGetLinearInterpolationVector(local_48, lbl_803A2058, lbl_803A204C, fn_800E0CA0(lbl_80478898));
        unaff_r28 = (s32)local_48[0];
        unaff_r31 = (s32)local_48[1];
        break;
    case 0:
    default:
        uVar4 = (s32)lbl_8047A368;
        if ((u32)uVar4 < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
            iVar2 = (s32)PC_PTR32(menuDataBiosGetPtr(*(u32*)PC_PTR((arg0 + 4))));
            pbVar3 = menuItemBiosGetPtr(DISC_S16(PC_PTR((iVar2 + 4)))); /* menu record: big-endian on the PC */
            uVar5 = 0;
            while (1) {
                if (((u32)*(volatile u8*)pbVar3 >> 7) & 1) {
                    if (uVar4 == uVar5) {
                        if (arg1) {
                            goto LAB_80025174;
                        } else {
                            goto LAB_80025174;
                        }
                    }
                    uVar5 = uVar5 + 1;
                }
                if (((u32)*(volatile u8*)pbVar3 >> 6) & 1) break;
                pbVar3 = menuItemBiosGetPtr(DISC_S16(pbVar3 + 0x18));
            }
            pbVar3 = (u8*)0;
LAB_80025174:
            if (pbVar3 != 0) {
                unaff_r28 = DISC_S16(pbVar3 + 2);
                unaff_r31 = DISC_S16(pbVar3 + 4);
            }
        }
        break;
    }
    fB8E0 = 9.0f;
    *(s16*)(arg1 + 0x50) = (s16)((f32)(s32)(s16)unaff_r28 - lbl_8047A374 + fB8E0);
    *(s16*)(arg1 + 0x52) = (s16)((f32)(s32)(s16)unaff_r31 + lbl_8047A374 + fB8E0);
}

/* 0x8002520C | 0x170
 *
 * fn_8002520C -- twin of fn_80024DBC. Uses -offset on BOTH x and y.
 * See fn_80024DBC doc.
 *
 * Status: 90.5% matched. Same SDA2 anonymous-constant blocker.
 */
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A368;
extern f32 lbl_8047A374;
void fn_8002520C(s32 arg0, u8* arg1) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_80478DD8;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A370;
    extern f32 lbl_8047A374;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    s32 iVar2;
    u8 *pbVar3;
    s32 uVar5;
    s32 unaff_r28;
    s32 uVar4;
    s32 unaff_r31;
    f32 fB8E0;
    f32 local_48[4];

    switch ((s32)lbl_8047A370) {
    case 1:

        GSlerpGetLinearInterpolationVector(local_48, lbl_803A2058, lbl_803A204C, fn_800E0CA0(lbl_80478898));
        unaff_r28 = (s32)local_48[0];
        unaff_r31 = (s32)local_48[1];
        break;
    case 0:
    default:
        uVar4 = (s32)lbl_8047A368;
        if ((u32)uVar4 < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
            iVar2 = (s32)PC_PTR32(menuDataBiosGetPtr(*(u32*)PC_PTR((arg0 + 4))));
            pbVar3 = menuItemBiosGetPtr(DISC_S16(PC_PTR((iVar2 + 4)))); /* menu record: big-endian on the PC */
            uVar5 = 0;
            while (1) {
                if (((u32)*(volatile u8*)pbVar3 >> 7) & 1) {
                    if (uVar4 == uVar5) {
                        if (((!pbVar3) && (!pbVar3)) && (!pbVar3)) {
                        }
                        goto LAB_800252E4;
                    }
                    uVar5 = uVar5 + 1;
                }
                if (((u32)*(volatile u8*)pbVar3 >> 6) & 1) break;
                pbVar3 = menuItemBiosGetPtr(DISC_S16(pbVar3 + 0x18));
            }
            pbVar3 = (u8*)0;
LAB_800252E4:
            if (pbVar3 != 0) {
                unaff_r28 = DISC_S16(pbVar3 + 2);
                unaff_r31 = DISC_S16(pbVar3 + 4);
            }
        }
        break;
    }
    fB8E0 = 9.0f;
    *(s16*)(arg1 + 0x50) = (s16)((f32)(s32)(s16)unaff_r28 - lbl_8047A374 + fB8E0);
    *(s16*)(arg1 + 0x52) = (s16)((f32)(s32)(s16)unaff_r31 - lbl_8047A374 + fB8E0);
}

/* 0x8002537C | 0x114
 *
 * fn_8002537C -- single-axis position writer. Writes (s16)(x + 0xf) to
 * arg1[0x50] and 0 to arg1[0x52]. Only returns an X coord; Y is always 0,
 * meaning this is likely for horizontal-slider cursors.
 *
 * case 1: GSlerpGetLinearInterpolationVector returns a pair, use local_28[0] (x)
 * default: linked-list walk, read pbVar2[2] (the x short of a menu item)
 *
 * Status: 98.4% matched. Remaining 2 diffs: bne+b vs beq pattern in the
 * counter-match check. CW 1.3 optimizer collapses explicit goto-pair
 * source back into beq.
 */
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A368;
void fn_8002537C(s32 arg0, u8* arg1) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_80478DD8;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A370;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    s32 iVar1;
    u8 *pbVar2;
    s32 uVar4;
    s32 unaff_r29;
    s32 uVar3;
    f32 local_28[4];

    switch ((s32)lbl_8047A370) {
    case 1:

        GSlerpGetLinearInterpolationVector(local_28, lbl_803A2058, lbl_803A204C, fn_800E0CA0(lbl_80478898));
        unaff_r29 = (s32)local_28[0];
        break;
    case 0:
    default:
        uVar3 = (s32)lbl_8047A368;
        if ((u32)uVar3 < TITLE_TABLE_COUNT(PC_PTR(lbl_80478DD8))) {
            iVar1 = (s32)PC_PTR32(menuDataBiosGetPtr(*(u32*)PC_PTR((arg0 + 4))));
            pbVar2 = menuItemBiosGetPtr(DISC_S16(PC_PTR((iVar1 + 4)))); /* menu record: big-endian on the PC (the crosshair lines) */
            uVar4 = 0;
            while (1) {
                if (((u32)*(volatile u8*)pbVar2 >> 7) & 1) {
                    if (uVar3 == uVar4) {
                        if ((pbVar2 && pbVar2) && pbVar2) {
                        }
                        goto LAB_80025450;
                    }
                    uVar4 = uVar4 + 1;
                }
                if (((u32)*(volatile u8*)pbVar2 >> 6) & 1) break;
                pbVar2 = menuItemBiosGetPtr(DISC_S16(pbVar2 + 0x18));
            }
            pbVar2 = (u8*)0;
LAB_80025450:
            if (pbVar2 != 0) {
                unaff_r29 = DISC_S16(pbVar2 + 2);
            }
        }
        break;
    }
    *(s16*)(arg1 + 0x50) = (s16)(unaff_r29 + 0xf);
    *(u16*)(arg1 + 0x52) = 0;
}

/* 0x80025490 | 0x114
 *
 * fn_80025490 -- exact twin of fn_8002537C but returns the Y coord (into
 * arg1[0x52]) with X always 0. Same code structure.
 *
 * case 1: use local_24[1] (local_24[0+4] offset)
 * default: read pbVar2[4] (y short, 2 bytes after x at pbVar2[2])
 *
 * Status: 98.4% matched. Same bne+b vs beq CW arcana.
 */
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern u32 lbl_80478DD8;
extern u32 lbl_8047A368;
inline u32 inline_fn(u32* arg0) {
    return TITLE_TABLE_COUNT(arg0); /* only used for the table count (big-endian on the PC) */
}

void fn_80025490(s32 arg0, u8* arg1) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_80478DD8;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A370;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuItemBiosGetPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* menuDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(void*, void*, void*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    s32 iVar1;
    u8 *pbVar2;
    s32 uVar4;
    s32 unaff_r29;
    s32 uVar3;
    f32 local_24[4];

    switch ((s32)lbl_8047A370) {
    case 1:

        GSlerpGetLinearInterpolationVector(local_24, lbl_803A2058, lbl_803A204C, fn_800E0CA0(lbl_80478898));
        unaff_r29 = (s32)local_24[1];
        break;
    case 0:
    default:
        uVar3 = (s32)lbl_8047A368;
        if ((u32)uVar3 < inline_fn((u32*)PC_PTR(lbl_80478DD8))) {
            iVar1 = (s32)PC_PTR32(menuDataBiosGetPtr(*(u32*)PC_PTR((arg0 + 4))));
            pbVar2 = menuItemBiosGetPtr(DISC_S16(PC_PTR((iVar1 + 4)))); /* menu record: big-endian on the PC (the crosshair lines) */
            uVar4 = 0;
            while (1) {
                if (((u32)*(volatile u8*)pbVar2 >> 7) & 1) {
                    if (uVar3 == uVar4) {
                        if (!iVar1 && !iVar1 && !iVar1) {
                        }
                        goto LAB_80025564;
                    }
                    uVar4 = uVar4 + 1;
                }
                if (((u32)*(volatile u8*)pbVar2 >> 6) & 1) break;
                pbVar2 = menuItemBiosGetPtr(DISC_S16(pbVar2 + 0x18));
            }
            pbVar2 = (u8*)0;
LAB_80025564:
            if (pbVar2 != 0) {
                unaff_r29 = DISC_S16(pbVar2 + 4);
            }
        }
        break;
    }
    *(u16*)(arg1 + 0x50) = 0;
    *(s16*)(arg1 + 0x52) = (s16)(unaff_r29 + 0xf);
}

/* 0x800255A4 | 0x18C
 *
 * fn_800255A4 -- title-screen audio/timing update driven by lbl_8047A384.
 *   IF title active (lbl_8047A384 != 0):
 *     fadeSet(lbl_8047B8E4, 3)     -- set BGM volume fade target
 *     soundStop(0x449, 0)            -- play SE cue
 *     while (accumulator < limit) {    -- time-based integration loop
 *       _threadSwitch();                 -- frame advance
 *       accumulator += (f32)fn_800D3088() / (f32)(s32)fn_800D37CC();
 *     }
 *     fn_801653C4(); fn_801656F8(0x7d0, 0)
 *   ELSE:
 *     fadeSet(lbl_8047B8E8, 3)     -- different fade curve
 *     fn_801653C4(); fn_801656F8(0x1f4, 0)
 *   Check sound channels 0xbd and 0xc3; stop them if still active.
 *   If lbl_8047A388/38C particle handles set, free them via GStextureFree.
 *
 * fn_800D37CC returns s32 (signed ticks; negative if paused).
 * fn_800D3088 returns u32 (absolute frame count).
 *
 * Status: 97.5% matched. Remaining 8 diffs are register-numbering
 * (limit ends up in f28 vs target f31) and anonymous @257/@255 bias
 * constants vs named lbl_8047B8B8/lbl_8047B8D0 (unfixable without
 * explicit manual int-to-float idiom). Frame size 0x70 now correct.
 * fn_801653C4 chains into fn_801656F8 (3 args).
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeSet(f32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStop(void);
extern void std228_soundGetCurrentBackgroundMusic(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std137_soundStopAll(void);
extern void menuIsCheck(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GStextureFree(void);
#endif
extern u32 lbl_8047A384;
extern u32 lbl_8047A388;
extern u32 lbl_8047A38C;
void worldMapPost(void) {
    extern u32 lbl_8047A384;
    extern u32 lbl_8047A388;
    extern u32 lbl_8047A38C;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
    extern void GStextureFree(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 menuIsCheck(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std228_soundGetCurrentBackgroundMusic(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std137_soundStopAll(u32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStop(s32, s32);
    extern void fadeSet(f32, s32);
#endif
    f32 accum;

    if (lbl_8047A384 != 0) {
        /* duration first, as this file declares fadeSet on the GameCube (the int and float travel in separate
         * registers there); on the PC fadeSet(2.0f, 3) went through the canonical (type, duration) prototype as type 2
         * (fade in) for 3 s instead of type 3 (fade to black) for 2 s: the end of the world-map bike ride brightened
         * from black instead of fading out, about a second longer (2026-10-21, verified against Dolphin's
         * screenshots) */
        fadeSetDurationFirst(2.0f, 3);
        soundStop(0x449, 0);
        accum = 0.0f;
        while (accum < 1.0f) {
            _threadSwitch();
            accum = accum + (f32)GSgetFrameStep() / (f32)fn_800D37CC();
        }
        std137_soundStopAll(std228_soundGetCurrentBackgroundMusic(), 0x7d0, 0);
    } else {
        fadeSetDurationFirst(0.5f, 3);
        std137_soundStopAll(std228_soundGetCurrentBackgroundMusic(), 0x1f4, 0);
    }
    fadeCheck(1);
    if (menuIsCheck(0xbd) == 1) menuClose(0xbd);
    if (menuIsCheck(0xc3) == 1) menuClose(0xc3);
    if (lbl_8047A384 != 0) {
        if (lbl_8047A388 != 0) GStextureFree(lbl_8047A388);
        if (lbl_8047A38C != 0) GStextureFree(lbl_8047A38C);
    }
}

/* 0x80025730 | 0x280 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuItemBiosSetSelectFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuOpenCustom(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorGetPrevFloorID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D3074(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GStextureCreate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuOpen(void);
#endif
extern u32 lbl_80478DDC;
extern u32 lbl_8047A368;
extern u32 lbl_8047A390;
extern u32 lbl_8047A3AC;
extern u32 lbl_80478DD8;
extern u8 lbl_8047A3A8;
extern u32 lbl_8047A388;
extern void worldMapBikeRide(void);
void worldMapMain(void) {
    /* common_rel data, big-endian on the PC like TitleMenuEntry above (a native view read the flag id 0x0000xxxx as
     * 0xxxxx0000, negative: the world map crashed in std081_flagCheckTrue, the slice run 2026-10-17) */
    typedef struct DISC_STRUCT TitleFloorEntry {
        u32 flag;
        u32 floor;
        u32 item;
        u32 pad;
    } TitleFloorEntry;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuItemBiosGetPtr(u32 item);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuItemBiosSetSelectFlag(u32 item, u8 enable);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 std081_flagCheckTrue(u32 flag);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32 id, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 floorGetPrevFloorID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800D3074(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GStextureCreate(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSgfxBeginBackFBCapture(u32 texture, void (*callback)(void), s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuOpen(s32 id, s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void wmapCaptureDone(void);
#endif
    s32 selected;
    u8 enable;
    u32 floor;
    u32 i;
    s32 result;
    u32 count;
    u8 change;
    TitleFloorEntry* entry;

    selected = 0;
    lbl_8047A368 = 0;
    lbl_8047A390 = PC_PTR32_IMPLICIT(menuItemBiosGetPtr((*(TitleFloorEntry**)&lbl_80478DDC)[0].item));
    for (i = count = 0; i < TITLE_TABLE_COUNT(*(u32**)&lbl_80478DD8); i++) {
        entry = &(*(TitleFloorEntry**)&lbl_80478DDC)[i];
        if (lbl_8047A3AC == entry->floor) {
            lbl_8047A368 = count;
            selected = count;
            lbl_8047A390 = PC_PTR32_IMPLICIT(menuItemBiosGetPtr(entry->item));
        }
        enable = 1;
        if ((*(TitleFloorEntry**)&lbl_80478DDC)[i].flag != 0 &&
            std081_flagCheckTrue((*(TitleFloorEntry**)&lbl_80478DDC)[i].flag) == 0) {
            enable = 0;
        }
        menuItemBiosSetSelectFlag((*(TitleFloorEntry**)&lbl_80478DDC)[i].item, enable);
        if (enable) {
            count++;
        }
    }

    result = menuOpenCustom(0xBD, windowGetActiveID(), &selected, 0, 1, 0);
    if (result < 0) {
        floor = floorGetPrevFloorID();
    } else {
        TitleFloorEntry* entries = *(TitleFloorEntry**)&lbl_80478DDC;
        u32 j;

        for (j = 0; j < TITLE_TABLE_COUNT(*(u32**)&lbl_80478DD8); j++) {
            if ((u32)result == entries[j].item) {
                break;
            }
        }
        if (j >= TITLE_TABLE_COUNT(*(u32**)&lbl_80478DD8)) {
            j = 0;
        }
        if (entries[j].floor == ROOM_D4_OUT && std081_flagCheckTrue(0x45D) != 0) {
            floor = ROOM_D4_OUT_2;
        } else {
            floor = (*(TitleFloorEntry**)&lbl_80478DDC)[j].floor;
        }
    }

    if (floor != floorGetPrevFloorID()) {
        if (floor == ROOM_M4_OUT_2 && floorGetPrevFloorID() == ROOM_M2_OUT) {
            change = 0;
        } else if (floor == ROOM_M2_OUT && floorGetPrevFloorID() == ROOM_M4_OUT_2) {
            change = 0;
        } else {
            change = 1;
        }
    } else {
        change = 0;
    }
    if (change == 1) {
        fn_800D3074(2);
        lbl_8047A3A8 = 0;
        GSgfxBeginBackFBCapture(PC_PTR(lbl_8047A388 = PC_PTR32_IMPLICIT(GStextureCreate(0, 0, 0x44, 0, 0))),
                                wmapCaptureDone, 0);
        while (lbl_8047A3A8 == 0) {
            _threadSwitch();
        }
        menuOpen(0xC3, 0);
        menuClose(0xBD);
        worldMapBikeRide();
    }
    floorLink(floor, 0);
}
#endif

/* 0x800259B0 | 0xCC */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std114_mapObjectSetVisibility(void);
#endif
extern u32 lbl_8047A368;
extern u32 lbl_8047A36C;
extern u32 lbl_8047A370;
extern f32 lbl_80478898;
extern f32 lbl_8047A374;
extern f32 lbl_8047A378;
extern f32 lbl_8047A37C;
extern u8 lbl_8047A380;
extern u32 lbl_8047A384;
extern u32 lbl_8047A388;
extern u32 lbl_8047A390;
extern u32 lbl_8047A3AC;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800259B0)
void worldMapPre(void) {
    extern u8 lbl_803A2058[];
    extern u8 lbl_803A204C[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std114_mapObjectSetVisibility(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 floorGetPrevFloorID(void);
#endif
    f32* vec1 = (f32*)lbl_803A2058;
    f32* vec2 = (f32*)lbl_803A204C;
    u32 result;

    lbl_8047A368 = 0;
    lbl_8047A36C = 0;
    lbl_8047A370 = 0;
    lbl_80478898 = 90.0f;
    vec1[0] = 0.0f;
    vec1[1] = 0.0f;
    vec1[2] = 0.0f;
    vec2[0] = 0.0f;
    vec2[1] = 0.0f;
    vec2[2] = 0.0f;
    lbl_8047A374 = 0.0f;
    lbl_8047A378 = 0.5f;
    lbl_8047A37C = 8.0f;
    lbl_8047A380 = 0;
    lbl_8047A384 = 0;
    lbl_8047A388 = 0;
    lbl_8047A390 = 0;
    std114_mapObjectSetVisibility(0x0C6A1000, 0);
    std114_mapObjectSetVisibility(0x0C6A1001, 0);
    std114_mapObjectSetVisibility(0x0C6A1002, 0);
    result = floorGetPrevFloorID();
    lbl_8047A3AC = result;
    if (result == 0x7b) {
        lbl_8047A3AC = 0x66;
    }
}

#if 0
void fn_800259B0_old(void) {
    extern u8 lbl_803A204C[];
    extern u8 lbl_803A2058[];
    extern f32 lbl_80478898;
    extern u32 lbl_8047A368;
    extern u32 lbl_8047A36C;
    extern u32 lbl_8047A370;
    extern f32 lbl_8047A374;
    extern f32 lbl_8047A378;
    extern f32 lbl_8047A37C;
    extern u8 lbl_8047A380;
    extern u32 lbl_8047A384;
    extern u32 lbl_8047A388;
    extern u32 lbl_8047A390;
    extern u32 lbl_8047A3AC;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorGetPrevFloorID();
#endif
    extern void fn_801CB954();
    u8 sp[0x10];
    u32 tmp = 0;
    u32 r3 = 0;
    u32 r4 = 0;
    u32 r5 = 0;
    u32 r6 = 0;
    f32 f0 = 0.0f;
    f32 f1 = 0.0f;
    f32 f2 = 0.0f;
    f32 f3 = 0.0f;

    f3 = 90.0f;
    r3 = (u32)lbl_803A2058;
    tmp = 0x0;
    f2 = 0.0f;
    r6 = (u32)lbl_803A2058;
    f1 = 0.5f;
    f0 = 8.0f;
    r3 = (u32)lbl_803A204C;
    r5 = (u32)lbl_803A204C;
    r3 = 0xC6A0000;
    lbl_8047A368 = tmp;
    r3 = r3 + 0x1000;
    r4 = 0x0;
    lbl_8047A36C = tmp;
    lbl_8047A370 = tmp;
    lbl_80478898 = f3;
    *(f32*)((u8*)r6 + 0x0) = f2;
    *(f32*)((u8*)r6 + 0x4) = f2;
    *(f32*)((u8*)r6 + 0x8) = f2;
    *(f32*)((u8*)r5 + 0x0) = f2;
    *(f32*)((u8*)r5 + 0x4) = f2;
    *(f32*)((u8*)r5 + 0x8) = f2;
    lbl_8047A374 = f2;
    lbl_8047A378 = f1;
    lbl_8047A37C = f0;
    lbl_8047A380 = tmp;
    lbl_8047A384 = tmp;
    lbl_8047A388 = tmp;
    lbl_8047A390 = tmp;
    ((void (*)(RegWord, RegWord))fn_801CB954)(r3, r4);
    r3 = 0xC6A0000;
    r4 = 0x0;
    r3 = r3 + 0x1001;
    ((void (*)(RegWord, RegWord))fn_801CB954)(r3, r4);
    r3 = 0xC6A0000;
    r4 = 0x0;
    r3 = r3 + 0x1002;
    ((void (*)(RegWord, RegWord))fn_801CB954)(r3, r4);
    r3 = ((u32 (*)(void))floorGetPrevFloorID)();
    lbl_8047A3AC = r3;
    if (r3 == 0x7b) {
        tmp = 0x66;
        lbl_8047A3AC = tmp;
    }
    return;
}
#endif

/* 0x80025A7C | 0x4 -- nop */
void fn_80025A7C(void) { }
#endif

/* 0x80025A80 | 0x19C
 *
 * fn_80025A80(s32 param_1) -> s32 -- cursor quad rendering + decay.
 *   1. Set up GX state (fn_800D9B58/A4C4/A2BC/A1E8/A100/A028/9ED8) for
 *      blended additive rendering with channel configuration.
 *   2. Call fn_80025C1C twice to draw two blended quads:
 *        first at the cursor current position using lbl_8047A3A0 (x scale)
 *        second at offset from lbl_803A204C[0,1] using lbl_8047A3A4 (y scale)
 *   3. Decay both scales:
 *        lbl_8047A3A0 -= lbl_8047B908; clamp >= 0
 *        lbl_8047A3A4 -= lbl_8047B8C8; clamp >= 0
 *   4. Return 1 if either scale is still > 0 (quad still visible), else 0.
 *
 * Used by GSgfxBeginBackFBCapture as a particle callback; re-registered until it
 * returns 0 (fully faded).
 *
 * Status: exact.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetOrtho(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetAlphaCompare(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(void);
#endif
extern f32 lbl_8047A3A0;
extern u32 lbl_8047A388;
extern f32 lbl_8047A3A4;
extern void wmapDrawCaptureQuad(s32, f32, f32, f32, s32, s32, f32*);
/*
 * RULE-EXCEPTION(title-path): reconstructed stripped function, kept only for
 * the .sdata2 pool order - see docs/RULE_EXCEPTIONS.md. Retail's pool has
 * 320.0f, 240.0f, 640.0f, 480.0f and 1.2f (0x8047B8F0-0x8047B900) ahead of
 * fn_80025A80's own 3.0f, although fn_80025A80 uses 640/480 before 320/240
 * and only fn_80025F84 (which comes later) reads 1.2f. MWCC adds a literal
 * when it first compiles it, so a function compiled before fn_80025A80 used
 * them in that order and the linker stripped it. Its body is the shape of
 * fn_80025F84's capture-centre store and 50 Hz-scaled frame wait; the name
 * is a placeholder. force_active is off so mwld strips it as it did retail's.
 */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80025A80)
#pragma push
#pragma force_active off
void gsTitleStrippedCaptureWait(void)
{
    extern u8 lbl_803A2040[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetOrtho(f32, f32, f32, f32);
#endif
    u32 frames;
    u32 elapsed;
    s32 tick;

    *(f32*)lbl_803A2040 = 320.0f;
    *(f32*)(lbl_803A2040 + 4) = 240.0f;
    *(f32*)(lbl_803A2040 + 8) = 0.0f;
    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    frames = 0x78 - (u32)((f32)fn_800D37CC() * 2.0f);
    tick = fn_800D37CC();
    if (tick == 0x32) {
        frames = (f32)frames / 1.2f;
    }
    for (elapsed = 0; elapsed < frames; elapsed += tick) {
        _threadSwitch();
        tick = GSgetFrameStep();
    }
}
#pragma pop
#endif


#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80025A80)
s32 wmapRideOverlay(s32 param_1) {
    extern u8 lbl_803A204C[];
    extern u32 lbl_8047A388;
    extern f32 lbl_8047A3A0;
    extern f32 lbl_8047A3A4;
    extern void wmapDrawCaptureQuad(s32, f32, f32, f32, s32, s32, f32*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetOrtho(f32, f32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800D9ED8(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetCullMode(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetAlphaCompare(s32, s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetZMode(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetUpdates(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetBlendMode(s32, s32, s32);
#endif
    f32 local_18[3];

    gfxSetOrtho(0.0f, 0.0f, 640.0f, 480.0f);
    gfxSetBlendMode(1, 6, 7);
    gfxSetUpdates(1, 1, 0);
    gfxSetZMode(0, 1, 1);
    gfxSetAlphaCompare(0, 7, 0, 1, 7, 0);
    gfxSetCullMode(0);
    fn_800D9ED8(1);

    local_18[0] = 320.0f;
    local_18[1] = 240.0f;
    local_18[2] = 0.0f;
    wmapDrawCaptureQuad(param_1, lbl_8047A3A0, 1.0f, 3.0f, 1, 0, local_18);

    lbl_8047A3A0 -= 2.25f;
    if (lbl_8047A3A0 < 0.0f) lbl_8047A3A0 = 0.0f;

    local_18[0] = 640.0f - (9.0f + ((f32*)lbl_803A204C)[0]);
    local_18[1] = 480.0f - (9.0f + ((f32*)lbl_803A204C)[1]);
    local_18[2] = 0.0f;
    wmapDrawCaptureQuad((s32)lbl_8047A388, lbl_8047A3A4, 1.0f, 2.0f, 1, 1, local_18);

    lbl_8047A3A4 = lbl_8047A3A4 - 4.5f;
    if (lbl_8047A3A4 < 0.0f) lbl_8047A3A4 = 0.0f;

    if (lbl_8047A3A4 <= 0.0f && lbl_8047A3A0 <= 0.0f) {
        return 0;
    }
    return 1;
}

/* 0x80025C1C | 0x358
 *
 * fn_80025C1C(s32 tex, f32 rot, f32 size, f32 uv_ext, s32 flag_a,
 *             s32 flag_b, f32 *pos)  -- GX quad render primitive.
 *
 *   1. Build two vec3s (vec_a, vec_b) based on flag_a (1 = symmetric scale,
 *      else diagonal). Call fn_800E0CA0(rot) + GSlerpGetLinearInterpolationVector to combine them
 *      into a rotated offset vector at result_1.
 *   2. Multiply result_1.x by lbl_8047B8DC (alpha) and store as scaled.
 *   3. Build second vec3 pair based on flag_b, call fn_800E0CA0+090C again,
 *      feed through fn_800E042C to get transformed coords into result_2.
 *   4. Third set: combine pos[0], pos[1] with lbl_8047B8F0/F4 UV corners,
 *      via fn_800E0CA0+090C+fn_800E03B4 for the inverse transform.
 *   5. Set GX render state (TEV stage 3, prim TRIANGLE_FAN count 4):
 *        fn_800D88DC(3), fn_800D888C(4), fn_800D6A00(4),
 *        fn_800D7820(lbl_80314AE8), fn_800D85D4(0, tex), fn_800D67BC(4)
 *   6. Emit 4 vertices forming a textured quad. Each vertex:
 *        GSvecTransform(vert_out, result_2, uv_corner)  -- compute position
 *        fn_800D6680(vert.x, vert.y, vert.z)         -- write pos
 *        gfxVtxColor4u8(0, 0xff, 0xff, 0xff, scaled)    -- RGBA color
 *        fn_800D59B8(0, u, v)                        -- UV
 *      4 UV corners: (0,0) (1,0) (0,1) (1,1) via lbl_8047B8AC/B8B0.
 *   7. fn_800D6728() to flush the primitive.
 *
 * result_2 is a 3x4 matrix (fn_800E042C builds it, fn_800E03B4 updates it,
 * GSvecTransform applies it), which accounts for retail's 0xA0 frame.
 *
 * Status: exact.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E042C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800E03B4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxLoadTexture(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSvecTransform(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxTexCoord(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif
extern u8 lbl_80314AE8[];
void wmapDrawCaptureQuad(s32 arg_r3, f32 arg_f1, f32 arg_f2, f32 arg_f3,
                 s32 arg_r4, s32 arg_r5, f32 *arg_r6) {
    extern u8 lbl_80314AE8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxTexCoord(s32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxColor4u8(s32, s32, s32, s32, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos(f32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(u8*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxLoadTexture(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSvecTransform(f32*, f32 (*)[4], f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E03B4(f32 (*)[4], f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800E042C(f32 (*)[4], f32*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSlerpGetLinearInterpolationVector(f32*, f32*, f32*, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern f32 fn_800E0CA0(f32);
#endif
    f32 mtx[3][4];
    f32 result_1[3];
    f32 vec_b[3];
    f32 vec_a[3];
    s32 scaled;

    if ((u8)arg_r4 == 1) {
        vec_b[0] = arg_f2;       vec_b[1] = arg_f2;       vec_b[2] = arg_f2;
        vec_a[0] = 0.0f; vec_a[1] = 0.0f; vec_a[2] = 0.0f;
    } else {
        vec_b[0] = 0.0f; vec_b[1] = 0.0f; vec_b[2] = 0.0f;
        vec_a[0] = arg_f2;       vec_a[1] = arg_f2;       vec_a[2] = arg_f2;
    }

    GSlerpGetLinearInterpolationVector(result_1, vec_b, vec_a, fn_800E0CA0(arg_f1));
    scaled = (s32)(255.0f * result_1[0]);

    if ((u8)arg_r5 == 1) {
        vec_b[0] = 1.0f; vec_b[1] = 1.0f; vec_b[2] = 1.0f;
        vec_a[0] = arg_f3;       vec_a[1] = arg_f3;       vec_a[2] = 1.0f;
    } else {
        vec_b[0] = arg_f3;       vec_b[1] = arg_f3;       vec_b[2] = 1.0f;
        vec_a[0] = 1.0f; vec_a[1] = 1.0f; vec_a[2] = 1.0f;
    }

    GSlerpGetLinearInterpolationVector(result_1, vec_b, vec_a, fn_800E0CA0(arg_f1));
    fn_800E042C(mtx, result_1);

    vec_b[0] = 320.0f; vec_b[1] = 240.0f; vec_b[2] = 0.0f;
    vec_a[0] = arg_r6[0]; vec_a[1] = arg_r6[1]; vec_a[2] = 0.0f;

    GSlerpGetLinearInterpolationVector(result_1, vec_b, vec_a, fn_800E0CA0(arg_f1));
    fn_800E03B4(mtx, result_1);

    gfxSetFlags(3);
    gfxClearFlags(4);
    gfxSetPrimitive(4);
    gfxSetVtxDesc(lbl_80314AE8);
    gfxLoadTexture(0, PC_PTR(arg_r3));
    gfxBegin(4);

    vec_b[0] = (-320.0f); vec_b[1] = (-240.0f); vec_b[2] = 0.0f;
    GSvecTransform(vec_a, mtx, vec_b);
    gfxVtxPos(vec_a[0], vec_a[1], vec_a[2]);
    gfxVtxColor4u8(0, 0xff, 0xff, 0xff, scaled);
    gfxVtxTexCoord(0, 0.0f, 0.0f);

    vec_b[0] = 320.0f; vec_b[1] = (-240.0f); vec_b[2] = 0.0f;
    GSvecTransform(vec_a, mtx, vec_b);
    gfxVtxPos(vec_a[0], vec_a[1], vec_a[2]);
    gfxVtxColor4u8(0, 0xff, 0xff, 0xff, scaled);
    gfxVtxTexCoord(0, 1.0f, 0.0f);

    vec_b[0] = (-320.0f); vec_b[1] = 240.0f; vec_b[2] = 0.0f;
    GSvecTransform(vec_a, mtx, vec_b);
    gfxVtxPos(vec_a[0], vec_a[1], vec_a[2]);
    gfxVtxColor4u8(0, 0xff, 0xff, 0xff, scaled);
    gfxVtxTexCoord(0, 0.0f, 1.0f);

    vec_b[0] = 320.0f; vec_b[1] = 240.0f; vec_b[2] = 0.0f;
    GSvecTransform(vec_a, mtx, vec_b);
    gfxVtxPos(vec_a[0], vec_a[1], vec_a[2]);
    gfxVtxColor4u8(0, 0xff, 0xff, 0xff, scaled);
    gfxVtxTexCoord(0, 1.0f, 1.0f);

    gfxEnd();
}
#endif

/* 0x80025F74 | 0x10 */
extern u8 lbl_8047A3A8;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80025F74)
s32 wmapCaptureDone(void) {
    lbl_8047A3A8 = 1;
    return 0;
}
#endif

/* 0x80025F84 | 0x3EC
 *
 * fn_80025F84 -- title splash setup (called once when title first shown).
 *   1. Activate 3 sprite handles 0xC6A1000/1001/1002 (fn_801CB954 activate=1).
 *   2. Pick an intro texture based on fn_801902E0(0x3E5):
 *        ret == 1: use 0xC6B1000 variant
 *        else:     use 0xC6C1000 variant
 *      Load it via fn_801CBA0C + fn_801CB954(activate=0).
 *   3. Set up TEV/sprite chain via fn_80113F48 + GSresGetResource composition:
 *        GSmodelSetBoundCheck, GSmodelSetShadowFlags(2), GSmodelSetShadowLight, GSmodelSetShadowSurface(1, &obj_c),
 *        GSmodelSetShadowTextureSize(0x280, 0x1E0)  -- viewport 640x480
 *        GSmodelLinkToGSparticleBank, GSmodelSetGSparticleLinkAttachMode(4)
 *      Activates tex handle, binds 0xC6A1000 slot 0, pushes animation.
 *      Stores handle in lbl_8047A384 (title active flag).
 *   4. Per lbl_8047A380 (debug flag?), pick one of two frame-sequence tables
 *      (lbl_80478DEC + idx*0x10), read 2 u32s, advance index (wrap > 9).
 *   5. Reset cursor scales: lbl_8047A3A4 = lbl_8047A3A0 = lbl_8047B8A8 (0).
 *   6. Register fn_80025A80 as a particle callback (GSgfxBeginBackFBCapture).
 *   7. Start BGM via cameraPlayAnime, then run two timing delay loops
 *      (_threadSwitch + fn_800D3088 accumulator vs 1 or 0xAE target).
 *   8. Store title origin coords to lbl_803A2040[0..2] and start the final
 *      fade-in loop using __cvt_fp2unsigned for FP-to-int conversion.
 *
 * Status: 99.1% matched (#pragma optimization_level 2). Remaining diffs:
 * register allocation (frame_a/b in table section, first timing loop delay),
 * and anonymous f64 bias constants (lbl_8047B8B8/B8D0, unfixable per CW heuristic).
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std112_modelGetResourceID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSresGetResource(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetBoundCheck(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetShadowFlags(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetShadowLight(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetShadowSurface(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetShadowTextureSize(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelLinkToGSparticleBank(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmodelSetGSparticleLinkAttachMode(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std166_mapObjectAttachToObject(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std115_mapObjectPlayAnimation(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std104_soundSetBackgroundMusic(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void cameraPlayAnime(void);
#endif
#ifndef TARGET_PC /* wrong type (see the block-scope declaration below); hidden on the PC */
extern void __cvt_fp2unsigned(void);
#endif
extern u8 lbl_8047A380;
extern u32 lbl_8047A384;
extern u32 lbl_8047A394;
extern u32 lbl_80478DEC;
extern u32 lbl_8047A398;
extern f32 lbl_8047A3A4;
extern f32 lbl_8047A3A0;
extern u32 lbl_8047A38C;
extern u8 lbl_803A2040[];
extern u32 lbl_8047A39C;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80025F84)
/* RULE-EXCEPTION(user-approved): local compiler-control pragma (optimization_level 2) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma optimization_level 2
void worldMapBikeRide(void) {
    extern u8 lbl_803A2040[];
    extern u32 lbl_80478DEC;
    extern u8 lbl_8047A380;
    extern u32 lbl_8047A384;
    extern u32 lbl_8047A38C;
    extern u32 lbl_8047A394;
    extern u32 lbl_8047A398;
    extern u32 lbl_8047A39C;
    extern f32 lbl_8047A3A0;
    extern f32 lbl_8047A3A4;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 __cvt_fp2unsigned(f64); /* MSL runtime helper; the file-scope `void` declaration above is hidden on the PC */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSgetFrameStep(void);
    extern void GSgfxBeginBackFBCapture(u32, void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetGSparticleLinkAttachMode(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelLinkToGSparticleBank(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetBoundCheck(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowTextureSize(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowLight(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowSurface(u32, s32, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmodelSetShadowFlags(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GStextureCreate(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSresGetResource(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(u32, u32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 std081_flagCheckTrue(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std166_mapObjectAttachToObject(u32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std115_mapObjectPlayAnimation(u32, u32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std114_mapObjectSetVisibility(u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(u32);
#endif
    extern s32 wmapRideOverlay(s32);
    u32 obj_c;
    u32 obj_d;
    u32 obj_e;
    u32 obj_b;
    u32 obj_a;
    u32 start;
    s32 tick;
    u32 tex;
    u32 elapsed;
    u32 frame_b;
    u32 frame_a;
    u32 delay;
    u32 wait2;

    std114_mapObjectSetVisibility(0xC6A1000, 1);
    std114_mapObjectSetVisibility(0xC6A1001, 1);
    std114_mapObjectSetVisibility(0xC6A1002, 1);
    if (std081_flagCheckTrue(0x3e5) == 1) {
        tex = std112_modelGetResourceID(PC_PTR(0xC6B1000));
        std114_mapObjectSetVisibility(tex, 0);
        start = 0;
    } else {
        tex = std112_modelGetResourceID(PC_PTR(0xC6C1000));
        std114_mapObjectSetVisibility(tex, 0);
        start = 0;
    }

    obj_a = PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), tex));
    GSmodelSetBoundCheck(PC_PTR(obj_a), 0);
    obj_b = PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), tex));
    obj_c = PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), 0xC6A1002));
    obj_d = PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), 0xC6A1603));
    {
        u32 obj_c_local = obj_c;
        GSmodelSetShadowFlags(PC_PTR(obj_b), 2);
        GSmodelSetShadowLight(obj_b, obj_d);
        GSmodelSetShadowSurface(PC_PTR(obj_b), 1, &obj_c_local);
    }
    GSmodelSetShadowTextureSize(0x280, 0x1e0);
    obj_e = PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), 0xC831400));
    GSmodelLinkToGSparticleBank(PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), tex)), PC_PTR(obj_e));
    GSmodelSetGSparticleLinkAttachMode(PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), tex)), 4);
    std114_mapObjectSetVisibility(tex, 1);
    std166_mapObjectAttachToObject(tex, 0xC6A1000, 0);
    std115_mapObjectPlayAnimation(tex, start, 0, 1);
    std104_soundSetBackgroundMusic(0x449, 0, 0xff);
    lbl_8047A384 = tex;

    {
        if (lbl_8047A380 == 0) {
            u32 idx;
            u32 off;
            u32 base;

            idx = lbl_8047A394;
            off = idx << 4;
            base = (u32)lbl_80478DEC; /* RULE-EXCEPTION(title-path): no-op cast for register order - see docs/RULE_EXCEPTIONS.md */
#ifdef TARGET_PC
            /* the route table is common_rel data, big-endian on the PC like the title tables above. Corrected
             * 2026-10-21 (verified with screenshots): read natively, the bike's route animation and the camera
             * animation ids came out byte-swapped, so the world-map ride played the wrong camera path (a ground-level
             * shot of the empty desert with the bike a speck in the distance; snatcherLeo: "transitions ... completely off") */
            frame_a = DISC_U32((u8*)PC_PTR(base + off + 41 * 4));
            frame_b = DISC_U32((u8*)PC_PTR(base + off + 42 * 4));
#else
            frame_a = *(u32*)PC_PTR((base + off + 41 * 4));
            frame_b = *(u32*)PC_PTR((base + off + 42 * 4));
#endif
            lbl_8047A394 = idx + 1;
            if ((s32)lbl_8047A394 > 9) lbl_8047A394 = 0;
        } else {
            u32 idx;
            u32 off;
            u32 base;

            idx = lbl_8047A398;
            off = idx << 4;
            base = (u32)lbl_80478DEC; /* RULE-EXCEPTION(title-path): no-op cast for register order - see docs/RULE_EXCEPTIONS.md */
#ifdef TARGET_PC /* common_rel data, big-endian (as above) */
            frame_a = DISC_U32((u8*)PC_PTR(base + off + 1 * 4));
            frame_b = DISC_U32((u8*)PC_PTR(base + off + 2 * 4));
#else
            frame_a = *(u32*)PC_PTR((base + off + 1 * 4));
            frame_b = *(u32*)PC_PTR((base + off + 2 * 4));
#endif
            lbl_8047A398 = idx + 1;
            if ((s32)lbl_8047A398 > 9) lbl_8047A398 = 0;
        }
        lbl_8047A3A4 = 90.0f;
        lbl_8047A3A0 = 90.0f;
        lbl_8047A38C = PC_PTR32_IMPLICIT(GStextureCreate(0, 0, 0x44, 0, 0));
        GSgfxBeginBackFBCapture(PC_PTR(lbl_8047A38C), (void*)wmapRideOverlay, 0);
        cameraPlayAnime(std093_getCurrentGroupID(), frame_b, 0, 0);

        delay = 1;
        tick = fn_800D37CC();
        if (tick == 0x32) {
            delay = (f32)delay / 1.2f;
            if (delay < 1) delay = 1;
        }
        for (elapsed = 0; elapsed < delay; elapsed = elapsed + tick) {
            _threadSwitch();
            tick = GSgetFrameStep();
        }
        std115_mapObjectPlayAnimation(0xC6A1000, frame_a, 0, 0);
        wait2 = 0xae;
        tick = fn_800D37CC();
        if (tick == 0x32) {
            wait2 = (f32)wait2 / 1.2f;
            if (wait2 < 1) wait2 = 1;
        }
        for (elapsed = 0; elapsed < wait2; elapsed = elapsed + tick) {
            _threadSwitch();
            tick = GSgetFrameStep();
        }
    }

    *(f32*)lbl_803A2040 = 320.0f;
    *(f32*)(lbl_803A2040 + 4) = 240.0f;
    *(f32*)(lbl_803A2040 + 8) = 0.0f;
    lbl_8047A39C = PC_PTR32_IMPLICIT(GSresGetResource(std093_getCurrentGroupID(), lbl_8047A384));
    lbl_8047A3A4 = 90.0f;

    wait2 = 0x78 - (u32)((f32)fn_800D37CC() * 2.0f);
    if (wait2 != 0) {
        tick = fn_800D37CC();
        if (tick == 0x32) {
            wait2 = (f32)wait2 / 1.2f;
            if (wait2 < 1) wait2 = 1;
        }
    }
    for (elapsed = 0; elapsed < wait2; elapsed = elapsed + tick) {
        _threadSwitch();
        tick = GSgetFrameStep();
    }
}
#endif

/* ===== Phase 2 recovery stubs ===== */

/* fn_8002058C - 0x8002058C | size: 0x2c */
extern u8 lbl_8047A32C;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_8002058C)
void fn_8002058C(void) {
    lbl_8047A32C = 0;
    fightPreloadResources(-1);
    lbl_8047A32C = 1;
    for (;;) {
        _threadSwitch();
    }
}
#endif

/* fn_800205C8 - 0x800205C8 | size: 0x44 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor(u32 a, u32 b, s32 c, u32 d);
#endif
extern u32 lbl_8047A350;
extern u32 lbl_80478880;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800205C8)
void fn_800205C8(u8* obj) {
    s32 mask = (s32)-0x100;
    u32 b = obj[0x8b];
    GSmsgPrintColor(0, 0, b | mask, (&lbl_80478880)[lbl_8047A350]);
}
#endif

/* fn_8002060C - 0x8002060C | size: 0xc */
extern u32 lbl_8047A350;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_8002060C)
void fn_8002060C(void) {
    lbl_8047A350 = 0;
}
#endif

/* fn_80020618 - 0x80020618 | size: 0x304 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* windowGetKeyInfo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 gamedatasaveGetStatus(s32, s32);
#endif
extern void soundSetOutputMode(u32);
extern void GSinputMotorStart(s32, s32, s32, s32, s32);
extern void gamedatasaveSetStatus(s32, s32, s32);
extern u32 lbl_8047B878;
extern u8 lbl_803A1FC8[];
extern u32 lbl_8047B880;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80020618)
/* Exact (2026-09-30). The option state is read straight out of
 * lbl_803A1FC8 at each use, not through a pointer local: retail rebuilds the
 * address per region (and caches it in r29 only where it uses it twice),
 * while one pointer keeps it in r31 throughout and drops a saved register.
 * The idle test compares the constant against the timer in that order, and
 * the sine phase is built in two steps, so the constant multiplies the timer
 * before the speed scales the product. */
s32 fn_80020618(void) {
    typedef struct TitleOptionState {
        s32 selection;
        s32 unk04;
        s32 counter;
        s32 toggle;
        f32 output;
        f32 unk14;
        f32 amplitude;
        f32 delta;
        f32 timer;
        f32 phase;
        f32 speed;
    } TitleOptionState;
    u8* keyInfo;
    f32 tickScale;
    f32 phase;
    s32 value;

    keyInfo = windowGetKeyInfo();
    tickScale = (f32)(s32)fn_800D37CC();
    ((TitleOptionState*)lbl_803A1FC8)->delta = (f32)(u32)GSgetFrameStep() / tickScale;
    ((TitleOptionState*)lbl_803A1FC8)->phase += ((TitleOptionState*)lbl_803A1FC8)->delta;
    if (((TitleOptionState*)lbl_803A1FC8)->phase >= 1.0f) {
        ((TitleOptionState*)lbl_803A1FC8)->phase = 0.0f;
    }

    if (gamedatasaveGetStatus(0, 9) == 1) {
        if (0.0f != ((TitleOptionState*)lbl_803A1FC8)->timer) {
            ((TitleOptionState*)lbl_803A1FC8)->timer += ((TitleOptionState*)lbl_803A1FC8)->delta;
            if (((TitleOptionState*)lbl_803A1FC8)->timer >= 1.0f) {
                ((TitleOptionState*)lbl_803A1FC8)->timer = 0.0f;
                ((TitleOptionState*)lbl_803A1FC8)->counter = 0;
            }
        }
    } else {
        ((TitleOptionState*)lbl_803A1FC8)->timer += ((TitleOptionState*)lbl_803A1FC8)->delta;
        if (((TitleOptionState*)lbl_803A1FC8)->timer >= 1.0f) {
            ((TitleOptionState*)lbl_803A1FC8)->timer = 0.0f;
            ((TitleOptionState*)lbl_803A1FC8)->counter++;
            if ((((TitleOptionState*)lbl_803A1FC8)->counter % 2) == 0) {
                ((TitleOptionState*)lbl_803A1FC8)->toggle ^= 1;
                if (((TitleOptionState*)lbl_803A1FC8)->toggle != 0) {
                    ((TitleOptionState*)lbl_803A1FC8)->amplitude = 0.0f;
                } else {
                    ((TitleOptionState*)lbl_803A1FC8)->amplitude = 0.20943952f;
                }
            }
        }
    }

    phase = 6.2831855f * ((TitleOptionState*)lbl_803A1FC8)->timer;
    phase = phase * ((TitleOptionState*)lbl_803A1FC8)->speed;
    ((TitleOptionState*)lbl_803A1FC8)->output = ((TitleOptionState*)lbl_803A1FC8)->amplitude * (f32)sin(phase);

    value = *(u16*)(keyInfo + 6);
#ifdef TARGET_PC
    /* OrrePort's rows (orre_hooks.h): the row order and the Language row; 0 leaves the keys to the game below */
    if (OrreHook_OptionsKeys != NULL && OrreHook_OptionsKeys(&((TitleOptionState*)lbl_803A1FC8)->selection, (u16)value)) {
        return 0;
    }
#endif
    if (value & MENU_KEY_DOWN) {
        if (((TitleOptionState*)lbl_803A1FC8)->selection < 2) {
            ((TitleOptionState*)lbl_803A1FC8)->selection++;
        }
    } else if (value & MENU_KEY_UP) {
        if (((TitleOptionState*)lbl_803A1FC8)->selection > 0) {
            ((TitleOptionState*)lbl_803A1FC8)->selection--;
        }
    } else {
        keyInfo = windowGetKeyInfo();
        switch (((TitleOptionState*)lbl_803A1FC8)->selection) {
        case 0:
            if (*(u16*)(keyInfo + 6) & MENU_KEY_LEFT) {
                soundSetOutputMode(1);
            }
            if (*(u16*)(keyInfo + 6) & MENU_KEY_RIGHT) {
                soundSetOutputMode(0);
            }
            break;
        case 1:
            if (*(u16*)(keyInfo + 6) & MENU_KEY_LEFT) {
                if (gamedatasaveGetStatus(0, 9) != 0) {
                    GSinputMotorStart(1, 0, 0xFF, 0x1E, 0);
                }
                gamedatasaveSetStatus(0, 9, 0);
                ((TitleOptionState*)lbl_803A1FC8)->counter = 1;
                ((TitleOptionState*)lbl_803A1FC8)->timer = 1.0f;
                ((TitleOptionState*)lbl_803A1FC8)->toggle = 1;
            }
            if (*(u16*)(keyInfo + 6) & MENU_KEY_RIGHT) {
                gamedatasaveSetStatus(0, 9, 1);
                ((TitleOptionState*)lbl_803A1FC8)->amplitude = 0.0f;
            }
            break;
        }
    }
    return 0;
}
#endif

/* titleGetState - 0x8002091C | size: 0x10 */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_8002091C)
s32 titleGetState(void) {
    return *(s32*)lbl_803A1FC8;
}

/* fn_8002092C - 0x8002092C | size: 0x90 */
void fn_8002092C(void* r3, u8* r4) {
    f32 f2;
    f32 f0;
    f2 = (f32)sin(6.2831855f * *(f32*)(lbl_803A1FC8 + 0x24));
    f0 = 0.1f * f2 + 1.1f;
    *(f32*)(r4 + 0x6c) = f0;
    *(f32*)(r4 + 0x68) = f0;
    if (*(s32*)lbl_803A1FC8 == 2) {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) | 2;
    } else {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) & ~2;
    }
}

/* fn_800209BC - 0x800209BC | size: 0x90 */
void fn_800209BC(void* r3, u8* r4) {
    f32 f2;
    f32 f0;
    f2 = (f32)sin(6.2831855f * *(f32*)(lbl_803A1FC8 + 0x24));
    f0 = 0.1f * f2 + 1.1f;
    *(f32*)(r4 + 0x6c) = f0;
    *(f32*)(r4 + 0x68) = f0;
    if (*(s32*)lbl_803A1FC8 == 1) {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) | 2;
    } else {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) & ~2;
    }
}

/* fn_80020A4C - 0x80020A4C | size: 0x90 */
void fn_80020A4C(void* r3, u8* r4) {
    f32 f2;
    f32 f0;
    f2 = (f32)sin(6.2831855f * *(f32*)(lbl_803A1FC8 + 0x24));
    f0 = 0.1f * f2 + 1.1f;
    *(f32*)(r4 + 0x6c) = f0;
    *(f32*)(r4 + 0x68) = f0;
    if (*(s32*)lbl_803A1FC8 == 0) {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) | 2;
    } else {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) & ~2;
    }
}

/* fn_80020ADC - 0x80020ADC | size: 0x58 */
extern u32 GSsndGetOutputMode(void);
void fn_80020ADC(void* r3, u8* r4) {
    if (GSsndGetOutputMode() != 1) {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) | 2;
    } else {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) & ~2;
    }
}

/* fn_80020B34 - 0x80020B34 | size: 0x58 */
void fn_80020B34(void* r3, u8* r4) {
    if (GSsndGetOutputMode() == 1) {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) | 2;
    } else {
        *(s8*)(r4 + 4) = *(s8*)(r4 + 4) & ~2;
    }
}

/* fn_80020B8C - 0x80020B8C | size: 0x14 */
void fn_80020B8C(void* r3, u8* r4) {
    *(f32*)(r4 + 0x70) = *(f32*)(lbl_803A1FC8 + 0x10);
}

/* menuOptionDrawSoundMode - 0x80020BA0 | size: 0xfc */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowDrawSprite(s32, s32, void*, s32, s32);
#endif
extern u8 lbl_802EF0A8[];
void menuOptionDrawSoundMode(void* arg0, u8* arg1) {
    s16* ptr;

    if (GSsndGetOutputMode() == 0) {
        ptr = (s16*)(lbl_802EF0A8 + 0x8FF0);
    } else {
        ptr = (s16*)(lbl_802EF0A8 + 0x900C);
    }
    windowDrawSprite(PC_PTR((s16)(DISC_EL_S16(ptr, 1) - *(s16*)(arg1 + 0x50))), PC_PTR((s16)(DISC_EL_S16(ptr, 2) - *(s16*)(arg1 + 0x52))), arg0, 0x192, 0);

    if (gamedatasaveGetStatus(0, 9) == 1) {
        ptr = (s16*)(lbl_802EF0A8 + 0x8FD4);
    } else {
        ptr = (s16*)(lbl_802EF0A8 + 0x8FB8);
    }
    windowDrawSprite(PC_PTR((s16)(DISC_EL_S16(ptr, 1) - *(s16*)(arg1 + 0x50))), PC_PTR((s16)(DISC_EL_S16(ptr, 2) - *(s16*)(arg1 + 0x52))), arg0, 0x192, 0);
}

/* titleNewContinueSelect - 0x80020C9C | size: 0x200 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winSeqCheckMove(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void memcardGetFlag41(void);
extern void fn_8001E074(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void memcardExecTask(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom(s32, s32, s32);
#endif
void titleNewContinueSelect(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 winSeqCheckMove(s32);
#endif
    extern void fn_800205C0(s32);
    extern u32 GSsndGetOutputMode(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 gamedatasaveGetStatus(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpen(s32, s32);
#endif
    extern void soundSetOutputMode(u32);
    extern void gamedatasaveSetStatus(s32, s32, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 memcardGetFlag41(void);
    extern u8 fn_8001E074(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void memcardExecTask(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom(s32, s32, s32);
#endif

    s32 state;
    s32 active;
    u32 region;
    s32 fmt;
    s32* sp;

    state = 0;
    active = 1;
    while (winSeqCheckMove(0xAA) != 0) {
        _threadSwitch();
    }
    fn_800205C0(0);

    *(s32*)((u8*)&lbl_803A1FC8 + 0x0) = 0;
    *(f32*)((u8*)&lbl_803A1FC8 + 0x14) = 0.0f;
    *(f32*)((u8*)&lbl_803A1FC8 + 0x10) = 0.0f;
    *(s32*)((u8*)&lbl_803A1FC8 + 0x4) = 0;
    *(f32*)((u8*)&lbl_803A1FC8 + 0x20) = 0.0f;
    *(f32*)((u8*)&lbl_803A1FC8 + 0x28) = 3.0f;
    *(f32*)((u8*)&lbl_803A1FC8 + 0x18) = 0.20943952f;
    *(s32*)((u8*)&lbl_803A1FC8 + 0x8) = 0;
    *(s32*)((u8*)&lbl_803A1FC8 + 0xc) = 0;

    do {
        switch (state) {
        case 0:
            region = GSsndGetOutputMode();
            fmt = gamedatasaveGetStatus(0, 9);
#ifdef TARGET_PC
            if (OrreHook_OptionsScreen != NULL) OrreHook_OptionsScreen(0); /* opening: OrrePort's rows (orre_hooks.h) */
#endif
            menuOpen(0x7F, 0);
            menuOpen(0x80, 0);
            menuOpen(0x7C, 0);
            menuOpen(0x7D, 0);
            menuOpen(0x7E, 0);
            sp = (s32*)&lbl_803A1FC8;
            do {
                if (menuOpen(0x7B, 1) < 0) {
#ifdef TARGET_PC
                    /* B keeps the choices when OrrePort asks (orre_hooks.h); otherwise as on the GameCube */
                    if (OrreHook_OptionsCancelKeeps == NULL || !OrreHook_OptionsCancelKeeps())
#endif
                    {
                        soundSetOutputMode(region);
                        gamedatasaveSetStatus(0, 9, fmt);
                    }
#ifdef TARGET_PC
                    if (OrreHook_OptionsScreen != NULL) OrreHook_OptionsScreen(1); /* cancelled: OrrePort's rows too */
#endif
                    break;
                }
            } while (sp[0] != 2);
            state = 0x64;
            break;
        case 0x64:
            *(s32*)&lbl_803A1FC8 = -1;
            if (gamedatasaveGetStatus(0, 9) != fmt) {
                if (memcardGetFlag41() != 0) {
                    winMsgOpen(1, 0x3D82, 1, 0);
                    if ((s8)menuSubOpenYesNo(0, 0x3C, 0xAA, 1) == 0) {
                        memcardExecTask(7, 2, 0);
                    }
                    winMsgClose(1);
                }
            }
            menuCloseCustom(0x80, 0, 1);
#ifdef TARGET_PC
            if (OrreHook_OptionsScreen != NULL) OrreHook_OptionsScreen(2); /* left: OrrePort's rows (orre_hooks.h) */
#endif
            active = 0;
            break;
        }
    } while (active != 0);
}
#endif

/* titleDrawOptionHelp - 0x80020EA4 | size: 0xb0 */
extern u32 lbl_8047A360;
extern u32 lbl_8047A364;
extern u32 lbl_802E4EF0[];
extern u32 lbl_802E4ED8[];
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80020EA4)
void titleDrawOptionHelp(u8* r3, u8* r4) {
    u8* r31;
    r31 = r3;
    if ((s32)(&lbl_8047A360)[1] == 4) {
        if (titleGetState() >= 0) {
            GSmsgPrintColor(0, -8, r31[0x8b] | (s32)-0x100, lbl_802E4EF0[titleGetState()]);
        }
    } else if ((s32)(&lbl_8047A360)[1] != 5) {
#ifdef TARGET_PC
        /* OrrePort's own main-menu entries (LOAD SAVE, choice 6) have no slot in the game's six-key help table, and
         * reading past it would show the Options screen's first line: their key comes from the hook (vanilla: the
         * hook is NULL and no such entry exists). Windows, Linux and macOS alike. */
        if ((s32)lbl_8047A360 >= 6) {
            u32 key = OrreHook_TopMenuHelp != NULL ? OrreHook_TopMenuHelp((s32)lbl_8047A360) : 0;
            if (key != 0) {
                GSmsgPrintColor(0, -8, r31[0x8b] | (s32)-0x100, key);
            }
            return;
        }
        /* *Added 2026-11-16 (snatcherLeo: "the helper line for CONTINUE will show ... what save file is loaded"):* for the
         * game's own six choices the hook may name another line (OrrePort's CONTINUE line with the loaded save's name);
         * 0 keeps the game's own. Vanilla: the hook is NULL, so the game's line. Windows, Linux and macOS alike. */
        if ((s32)lbl_8047A360 >= 0 && OrreHook_TopMenuHelp != NULL) {
            u32 key = OrreHook_TopMenuHelp((s32)lbl_8047A360);
            if (key != 0) {
                GSmsgPrintColor(0, -8, r31[0x8b] | (s32)-0x100, key);
                return;
            }
        }
#endif
        if ((s32)lbl_8047A360 >= 0) {
            GSmsgPrintColor(0, -8, r31[0x8b] | (s32)-0x100,
                        lbl_802E4ED8[(s32)lbl_8047A360]);
#ifdef TARGET_PC
            /* OrreHook_TopMenuHelpDrawn (docs/port/hook-points.md; 2026-11-21, the accurate tag, snatcherLeo): the game's own
             * help line for one of its six choices has just been printed (its key, and the help's fade alpha), so
             * OrrePort may print a small piece of its own beside it in the same box (the yellow "GameCube-accurate"
             * tag on CONTINUE's line in an accurate session; pc_title_loadsave.cpp). The game's words are untouched.
             * Nothing registered: nothing. All platforms */
            if (OrreHook_TopMenuHelpDrawn != NULL) {
                OrreHook_TopMenuHelpDrawn((s32)lbl_8047A360, lbl_802E4ED8[(s32)lbl_8047A360], r31[0x8b], r31);
            }
#endif
        }
    }
}
#endif

/* titleDrawContinueInfo - 0x80020F54 | size: 0x19c */
extern void* menuModelRender(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos2s16(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 darkPokemonCountFlag14(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801021F8(u32, u32);
#endif
extern u8 lbl_803A1FF8[];
extern u8 lbl_80314F98[];
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80020F54)
void titleDrawContinueInfo(u8* arg0, u8* arg1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(u8*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxLoadTexture(s32, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos2s16(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxColor4u8(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxTexCoord(s32, f32, f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif

    void* model;
    s16 code;

    code = *(s16*)(arg1 + 6);
    switch ((s32)code) {
    case 0x90E:
        if ((s32)lbl_8047A360 >= 0) {
            lbl_8047A360 = (s32)(s8)arg0[0x95];
        }
        break;
    case 0x2D2:
        model = menuModelRender(lbl_803A1FF8);
        if (model != 0) {
            gfxSetFlags(3);
            gfxClearFlags(4);
            gfxSetPrimitive(7);
            gfxSetVtxDesc(lbl_80314F98);
            gfxLoadTexture(0, model);
            gfxBegin(2);
            gfxVtxPos2s16(0, 0);
            gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
            gfxVtxTexCoord(0, 0.0f, 0.0f);
            gfxVtxPos2s16(*(s16*)(arg1 + 0x54), *(s16*)(arg1 + 0x56));
            gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
            gfxVtxTexCoord(0, 1.0f, 1.0f);
            gfxEnd();
        }
        /* FALL THROUGH */
    case 0x2CD:
    case 0x2D1:
        if (darkPokemonCountFlag14() == 0) {
            ((s8*)arg1)[4] &= ~2;
        }
        break;
    case 0x912:
    case 0x913:
    case 0x914:
    case 0x915:
    case 0x916:
    case 0x917:
        if ((s32)(&lbl_8047A360)[1] == 5) {
            fn_801021F8(0xAA, 0);
        } else {
            fn_801021F8(0xAA, 1);
        }
        break;
    }
}
#endif

/* titleMenuMain - 0x800210F0 | size: 0x4d4
 *
 * GStitle_LoadSaveFile: the title menu loop. Exact (2026-09-30). The shapes
 * that decide its registers, each read off the target:
 *  - 0x8047A360 is one 8-byte object (symbols.txt), so the menu result and
 *    the mode beside it are indexed, as the neighbouring functions already
 *    spell them: retail keeps the base in r30 and stores the mode at
 *    0x4(r30), where two @sda21 symbols would address each word separately;
 *  - the loop tests its flag at the bottom (do/while), so the switch is
 *    entered directly instead of jumping to the test first;
 *  - the inner menu switch lays out case 1 after cases 2 and 3;
 *  - the three message values are their own object, not the address-taken
 *    work variables: those are passed to fn_8018F4C8 by address, so reusing
 *    them puts the values in memory, while retail holds them in r27/r28
 *    across the calls. As an array their elements outrank the state web and
 *    take r28, leaving state r27; as separate scalars state takes r28 and
 *    every "li" for the mode stores merges with it (26 lines);
 *  - the confirmation answer is s8, the return type of menuSubOpenYesNo, and
 *    is its own variable rather than state: assigning it to state splits that
 *    web into a second register (6 lines).
 * The name buffer is 0x20 bytes, which is what puts the saved registers at
 * 0x3c and makes the frame 0x50.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuColosseumBattle(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void floorRequestChange(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8006A718(void);
#endif
extern void std113_mapObjectUnk_8018db68(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuModelTerm(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuNameEntryOpen(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuNameEntryGetLastName(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void savedataCreate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800056E4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gamedataSetPlaytime(void);
#endif
extern void heroMoveSyncWithHero(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuModelInit(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuPokemonModelRequestById(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void peopleInfoBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8018F4C8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80005748(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void darkPokemonCountFlag12(void);
#endif
extern void floorReturnToSavedRoom(void);
extern u32 lbl_8047A35C;
extern u32 lbl_8047A358;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800210F0)
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 titleMenuMain(void)
{
    extern u8 lbl_802EF0A8[];
    extern u8 lbl_803A1FF8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuColosseumBattle(s32);
#endif
    extern void fn_800205B8(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 gamedatasaveGetStatus(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8006A718(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 memcardGetFlag41(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 floorRequestChange(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuModelInit(void*, s16, s16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuModelSetMotion(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonModelRequestById(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* peopleInfoBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8018F4C8(void*, s32, s32*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuNameEntryOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuNameEntryGetLastName(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataCreate(u32, const void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800056E4(u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gamedataSetPlaytime(f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorLink(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80005748(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 darkPokemonCountFlag14(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 darkPokemonCountFlag12(void);
#endif
    s32 state;
    s32 loop;
    s32 result;
    s32 workA;
    s32 workB;
    s8 answer;
    s32 msgVals[3];
    u8 nameBuf[0x20];

    state = 0;
    loop = 1;
    (&lbl_8047A360)[1] = 1;

    do {
        switch (state) {
        case 0:
            state = 1;
            break;

        case 1:
            /* OrrePort's own main-menu entries (LOAD SAVE) are linked in each time the menu opens (vanilla: NULL).
             * *Moved 2026-11-15* from the function's start: the game comes back here after Options, where the language
             * can change, and LOAD's pictures follow the language (pc_title_loadsave.cpp). Windows, Linux and macOS
             * alike; the GameCube build keeps its merged copy of this function (below), which has no hook. */
            if (OrreHook_TopMenuOpen != NULL) {
                OrreHook_TopMenuOpen();
            }
            (&lbl_8047A360)[1] = 1;
            menuOpen(0x7F, 0);
            result = menuOpenCustom(0xAA, windowGetActiveID(),
                                    &lbl_8047A360, 0, 1, 0);
            if (result < 0) {
                lbl_8047A360 = -1;
            }
            if (result == 0) {
                state = 5;
                break;
            }

            switch ((s32)lbl_8047A360) {
            case 0:
                break;
            case 2:
                menuColosseumBattle(0);
                loop = 0;
                break;
            case 3:
                menuColosseumBattle(3);
                loop = 0;
                break;
            case 1:
                state = 2;
                break;
            case 4:
                (&lbl_8047A360)[1] = 4;
                state = 4;
                break;
            default:
                /* OrrePort's own entries (LOAD SAVE, choice 6) land here, where the game sends Quit back to the
                 * title: the hook takes them first (vanilla: NULL, so Quit and cancel go on as on the GameCube) */
                if (OrreHook_TopMenuChoice != NULL && OrreHook_TopMenuChoice((s32)lbl_8047A360, &state) != 0) {
                    break;
                }
                fn_800205B8(1);
                floorRequestChange(ROOM_TITLE);
                state = 0x64;
                break;
            }
            break;

        case 5:
            if (gamedatasaveGetStatus(0, 4) == 0) {
                result = 0x3C3A;
            } else if ((u16)fn_8006A718(0) == 1) {
                result = 0x44C9;
            } else {
                result = 0x3C35;
            }
            winMsgOpen(2, result, 1, 1);
            (&lbl_8047A360)[1] = 5;
            /* the cursor's start (1 = NO, the game's own). OrrePort's tweak may start on YES when there is no story to
             * lose (the game's own test just above: save status 4 is 0, the "start a new Story?" wording); with a
             * story saved it stays NO (snatcherLeo, 2026-11-14). Vanilla: NULL, so NO as on the GameCube */
            answer = (s8)menuSubOpenYesNo(0, 0x3C, 0xA6, ORRE_NEW_GAME_INITIAL(1, gamedatasaveGetStatus(0, 4) != 0));
            winMsgClose(1);
            if (answer == 0) {
                menuCloseCustom(0xAA, 0, 1);
                menuCloseCustom(0x7A, 0, 1);
                menuCloseCustom(0x7F, 0, 1);
                std113_mapObjectUnk_8018db68(lbl_8047A35C);
                menuModelTerm(lbl_803A1FF8);
                std104_soundSetBackgroundMusic(1, 0x3E8, 0xFF);
                /* OrreHook_NameEntryCaller (orre_hooks.h): New Game can take a back-out from Wes's name menu (B there,
                 * snatcherLeo 2026-11-15; port/src/pc_name_keyboard.cpp decides, off in GameCube-accurate mode). The screen
                 * then returns ORRE_NAME_ENTRY_LEFT, which the game never returns (0 or 1), so with nothing
                 * registered this is the GameCube's own path. Windows, Linux and macOS alike */
                OrreHook_NameEntryCaller = ORRE_NAME_CALLER_NEW_GAME;
                if (menuNameEntryOpen(0, 0) == ORRE_NAME_ENTRY_LEFT) {
                    /* left without a name: no save is made (savedataCreate), the play time and the hero aren't
                     * touched and the opening isn't linked; the card's save the title loaded stays in memory as it
                     * was. The top menu room loads fresh, the way the title screen enters it (gs_title_8001EF78.c,
                     * floorLink(0x3A1, 0)): its setup (titleIntroStart) starts the song, the background model and
                     * the camera that YES closed above, and the menu opens on NEW GAME (lbl_8047A358, set at 0x64) */
                    OrreHook_NameEntryCaller = 0;
                    floorLink(0x3A1, 0);
                    state = 0x64;
                    break;
                }
                OrreHook_NameEntryCaller = 0;
                menuNameEntryGetLastName(nameBuf);
                savedataCreate(0, nameBuf);
                fn_800056E4(1);
                gamedataSetPlaytime(0.0f);
                heroMoveSyncWithHero();
                floorLink(0x3A0, 0);
                state = 0x64;
            } else {
                state = 1;
            }
            break;

        case 2:
            fn_800056E4(0);
            if (gamedatasaveGetStatus(0, 4) != 0) {
                state = 3;
                break;
            }
            if ((u8)memcardGetFlag41() == 0) {
                winMsgOpen(2, 0x44EA, 1, 0);
            } else {
                winMsgOpen(2, 0x444B, 1, 0);
            }
            winMsgClose(1);
            state = 1;
            break;

        case 3:
            menuCloseCustom(0xAA, 0, 1);
            menuCloseCustom(0x7F, 0, 1);
            menuModelInit(lbl_803A1FF8,
                          DISC_S16(lbl_802EF0A8 + 0x4EFE),
                          DISC_S16(lbl_802EF0A8 + 0x4F00));
            menuPokemonModelRequestById(lbl_803A1FF8, 0xF70400);
            fn_8018F4C8(peopleInfoBiosGetPtr(PC_PTR(0xF70400)), 1, &workA, &workB);
            menuModelSetMotion(lbl_803A1FF8, workA);
            msgVals[0] = fn_80005748();
            msgVals[1] = darkPokemonCountFlag14();
            msgVals[2] = darkPokemonCountFlag12();
            msgctrlSetValue(0x4C, PC_PTR32_IMPLICIT((void*)msgVals[0]));
            msgctrlSetValue(0x2F, PC_PTR32_IMPLICIT((void*)msgVals[2]));
            msgctrlSetValue(0x30, PC_PTR32_IMPLICIT((void*)msgVals[1]));
            menuOpen(0x16, 0);
            /* OrreHook_ContinueQuestion (orre_hooks.h): right after a save is picked in LOAD's SAVE FILES list, OrrePort's
             * own two-line question in the same window ("Your save file has been loaded!" / "Continue your story
             * now?", snatcherLeo 2026-11-15 / 16). Vanilla: NULL, so the game's 0x3C22. Windows, Linux and macOS alike */
            winMsgOpen(1, ORRE_CONTINUE_QUESTION(0x3C22), 0, 1);
            /* the last argument is where the cursor starts (1 = NO, the game's own); OrrePort's YES-first tweak may
             * change it through the hook (vanilla: NULL, so NO as on the GameCube) */
            answer = (s8)menuSubOpenYesNo(0, 0x3C, 0xD6, ORRE_CONTINUE_INITIAL(1));
            if (answer == 0) {
                OrreFieldSpot spot;
                heroMoveSyncWithHero();
                /* OrreHook_ContinueSpot (docs/port/hook-points.md): a save made through PC MENU goes on from the exact
                 * spot it was made at, as a room change to that position (floorChangePos), with the room-change record
                 * as floorReturnToSavedRoom leaves it (the room before, the entry, no pending story return point).
                 * Nothing registered or no spot: the save's return point, as on the GameCube */
                if (OrreHook_ContinueSpot != NULL && OrreHook_ContinueSpot(&spot) == 1 && spot.room != 0) {
                    extern u8 _floorChangeInfo[];
                    floorChangePos(spot.room, (s16)spot.facingDegrees, spot.x, spot.y, spot.z);
                    *(u32*)(_floorChangeInfo + 0xC) = spot.entry;
                    *(u8*)(_floorChangeInfo + 0x51) = 0;
                    *(u32*)(_floorChangeInfo + 0x0) = spot.prevRoom;
                } else {
                    floorReturnToSavedRoom();
                }
                state = 0x64;
                fn_800056E4(1);
            } else if (answer == 1) {
                state = 1;
                fn_800056E4(1);
            } else {
                state = 1;
                fn_800056E4(1);
            }
            menuCloseCustom(0x16, 0, 1);
            winMsgClose(1);
            menuModelTerm(lbl_803A1FF8);
            break;

        case 4:
            menuCloseCustom(0x7F, 0, 1);
            titleNewContinueSelect();
            (&lbl_8047A360)[1] = 1;
            state = 1;
            break;

        case 0x64:
            result = (s32)lbl_8047A360;
            if (result >= 5 || result < 0) {
                result = 0;
            }
            lbl_8047A358 = result;
            loop = 0;
            break;

        default:
            break;
        }
    } while (loop != 0);

    menuCloseCustom(0xAA, 0, 1);
    menuCloseCustom(0x7A, 0, 1);
    menuCloseCustom(0x7F, 0, 1);
    std113_mapObjectUnk_8018db68(lbl_8047A35C);
    return lbl_8047A360;
}
#else
s32 titleMenuMain(void)
{
    extern u8 lbl_802EF0A8[];
    extern u8 lbl_803A1FF8[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuOpenCustom(s32, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseCustom(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuColosseumBattle(s32);
#endif
    extern void fn_800205B8(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 gamedatasaveGetStatus(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fn_8006A718(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 memcardGetFlag41(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 floorRequestChange(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuModelInit(void*, s16, s16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuModelSetMotion(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonModelRequestById(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* peopleInfoBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8018F4C8(void*, s32, s32*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuNameEntryOpen(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuNameEntryGetLastName(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void savedataCreate(u32, const void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800056E4(u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gamedataSetPlaytime(f32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorLink(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 fn_80005748(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 darkPokemonCountFlag14(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 darkPokemonCountFlag12(void);
#endif
    s32 state;
    s32 loop;
    s32 result;
    s32 workA;
    s32 workB;
    s8 answer;
    s32 msgVals[3];
    u8 nameBuf[0x20];

    state = 0;
    loop = 1;
    (&lbl_8047A360)[1] = 1;

    do {
        switch (state) {
        case 0:
            state = 1;
            break;

        case 1:
            (&lbl_8047A360)[1] = 1;
            menuOpen(0x7F, 0);
            result = menuOpenCustom(0xAA, windowGetActiveID(),
                                    &lbl_8047A360, 0, 1, 0);
            if (result < 0) {
                lbl_8047A360 = -1;
            }
            if (result == 0) {
                state = 5;
                break;
            }

            switch ((s32)lbl_8047A360) {
            case 0:
                break;
            case 2:
                menuColosseumBattle(0);
                loop = 0;
                break;
            case 3:
                menuColosseumBattle(3);
                loop = 0;
                break;
            case 1:
                state = 2;
                break;
            case 4:
                (&lbl_8047A360)[1] = 4;
                state = 4;
                break;
            default:
                fn_800205B8(1);
                floorRequestChange(ROOM_TITLE);
                state = 0x64;
                break;
            }
            break;

        case 5:
            if (gamedatasaveGetStatus(0, 4) == 0) {
                result = 0x3C3A;
            } else if ((u16)fn_8006A718(0) == 1) {
                result = 0x44C9;
            } else {
                result = 0x3C35;
            }
            winMsgOpen(2, result, 1, 1);
            (&lbl_8047A360)[1] = 5;
            answer = (s8)menuSubOpenYesNo(0, 0x3C, 0xA6, 1);
            winMsgClose(1);
            if (answer == 0) {
                menuCloseCustom(0xAA, 0, 1);
                menuCloseCustom(0x7A, 0, 1);
                menuCloseCustom(0x7F, 0, 1);
                std113_mapObjectUnk_8018db68(lbl_8047A35C);
                menuModelTerm(lbl_803A1FF8);
                std104_soundSetBackgroundMusic(1, 0x3E8, 0xFF);
                menuNameEntryOpen(0, 0);
                menuNameEntryGetLastName(nameBuf);
                savedataCreate(0, nameBuf);
                fn_800056E4(1);
                gamedataSetPlaytime(0.0f);
                heroMoveSyncWithHero();
                floorLink(0x3A0, 0);
                state = 0x64;
            } else {
                state = 1;
            }
            break;

        case 2:
            fn_800056E4(0);
            if (gamedatasaveGetStatus(0, 4) != 0) {
                state = 3;
                break;
            }
            if ((u8)memcardGetFlag41() == 0) {
                winMsgOpen(2, 0x44EA, 1, 0);
            } else {
                winMsgOpen(2, 0x444B, 1, 0);
            }
            winMsgClose(1);
            state = 1;
            break;

        case 3:
            menuCloseCustom(0xAA, 0, 1);
            menuCloseCustom(0x7F, 0, 1);
            menuModelInit(lbl_803A1FF8,
                          DISC_S16(lbl_802EF0A8 + 0x4EFE),
                          DISC_S16(lbl_802EF0A8 + 0x4F00));
            menuPokemonModelRequestById(lbl_803A1FF8, 0xF70400);
            fn_8018F4C8(peopleInfoBiosGetPtr(PC_PTR(0xF70400)), 1, &workA, &workB);
            menuModelSetMotion(lbl_803A1FF8, workA);
            msgVals[0] = fn_80005748();
            msgVals[1] = darkPokemonCountFlag14();
            msgVals[2] = darkPokemonCountFlag12();
            msgctrlSetValue(0x4C, (u32)msgVals[0]);
            msgctrlSetValue(0x2F, (u32)msgVals[2]);
            msgctrlSetValue(0x30, (u32)msgVals[1]);
            menuOpen(0x16, 0);
            winMsgOpen(1, 0x3C22, 0, 1);
            answer = (s8)menuSubOpenYesNo(0, 0x3C, 0xD6, 1);
            if (answer == 0) {
                heroMoveSyncWithHero();
                floorReturnToSavedRoom();
                state = 0x64;
                fn_800056E4(1);
            } else if (answer == 1) {
                state = 1;
                fn_800056E4(1);
            } else {
                state = 1;
                fn_800056E4(1);
            }
            menuCloseCustom(0x16, 0, 1);
            winMsgClose(1);
            menuModelTerm(lbl_803A1FF8);
            break;

        case 4:
            menuCloseCustom(0x7F, 0, 1);
            titleNewContinueSelect();
            (&lbl_8047A360)[1] = 1;
            state = 1;
            break;

        case 0x64:
            result = (s32)lbl_8047A360;
            if (result >= 5 || result < 0) {
                result = 0;
            }
            lbl_8047A358 = result;
            loop = 0;
            break;

        default:
            break;
        }
    } while (loop != 0);

    menuCloseCustom(0xAA, 0, 1);
    menuCloseCustom(0x7A, 0, 1);
    menuCloseCustom(0x7F, 0, 1);
    std113_mapObjectUnk_8018db68(lbl_8047A35C);
    return lbl_8047A360;
}
#endif
#endif

/* titleTopMenuExit - 0x800215C4 | size: 0x60 */
extern u32 lbl_8047A35C;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800215C4)
/* NAME-T3 fn_800215C4 */ /* exit callback of room 0x3A1 topmenu */
void titleTopMenuExit(void) {
    menuCloseCustom(0xaa, 0, 1);
    menuCloseCustom(0x7a, 0, 1);
    menuCloseCustom(0x7f, 0, 1);
    std113_mapObjectUnk_8018db68(lbl_8047A35C);
    menuModelTerm(lbl_803A1FF8);
}
#endif

/* titleTopMenuMain - 0x80021624 | size: 0x20 */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80021624)
/* NAME-T3 fn_80021624 */ /* main callback of room 0x3A1 topmenu */
void titleTopMenuMain(void) {
    titleMenuMain();
}
#endif

/* titleIntroStart - 0x80021644 | size: 0x9c */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSscene_SetMode(void);
#endif
extern u32 lbl_8047A35C;
extern u32 lbl_8047A358;
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80021644)
void titleIntroStart(void) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* std093_getCurrentGroupID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std112_modelGetResourceID(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSresGetResource(void*, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cameraPlayAnime(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSscene_SetMode(s32);
#endif
    void* handle;

#ifdef TARGET_PC
    { extern void OSReport(const char*, ...); OSReport("[topmenu] setup: restored=%d\n", (int)fn_800FF548()); } /* PC test log (the testing lane) */
#endif
    if (fn_800FF548() == 0) {
        std104_soundSetBackgroundMusic(0x446, 0, 0xff);
        handle = PC_PTR(std093_getCurrentGroupID());
        lbl_8047A35C = std112_modelGetResourceID(PC_PTR(0x0FFE1000));
        GSresGetResource(PC_PTR32_IMPLICIT(handle), lbl_8047A35C);
        cameraPlayAnime(0x632, 0x0FFF1800, 0, 1);
        GSscene_SetMode(4);
        lbl_8047A360 = lbl_8047A358;
        (&lbl_8047A360)[1] = 0;
    }
}
#endif

/* itemUseFormatMessage - 0x800216E8 | size: 0x1d4 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgGetText(void*, s32, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* pokemonBiosGetNicknamePtr(s32);
#endif
extern u8 lbl_80266C7C[];
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800216E8)
void itemUseFormatMessage(void* arg0, s32 arg1, u8* arg2, s16 arg3, s32 arg4) {
    struct TitleNameCand {
        s32 code;
        s32 msg;
        s32 link;
    };
    struct TitleNameTable {
        struct TitleNameCand e[21];
    } table;
    /* RULE-EXCEPTION(user-approved): separate locals for the second search, declared first for register allocation - see docs/RULE_EXCEPTIONS.md */
    u8* cand2;
    struct TitleNameCand* entry2;
    s32 code2;
    s32 j;
    u8* cand;
    struct TitleNameCand* entry;
    s32 i;
    s32 code;
    s32 link;
    s16 x;
    s16 y;
    s32 msg;
    void* name;

    table = *(struct TitleNameTable*)lbl_80266C7C;

    if (arg3 <= 0) {
        GSmsgGetText(arg0, arg1 + 1, PC_PTR32_IMPLICIT((void*)0x4261));
        return;
    }

    entry = table.e;
    for (i = 0; i < 21; entry++, i++) {
        code = entry->code;
        cand = arg2;
        for (j = 0; j < arg3; cand += 8, j++) {
            if (code == *(s32*)cand) {
                break;
            }
        }
        if (j < arg3) {
            break;
        }
    }

    if (i >= 21) {
        GSmsgGetText(arg0, arg1 + 1, PC_PTR32_IMPLICIT((void*)0x4261));
        return;
    }

    msg = table.e[i].msg;
    link = table.e[i].link;
    x = *(s16*)(arg2 + j * 8 + 4);
    y = *(s16*)(arg2 + j * 8 + 6);

    if (link >= 0) {
        i++;
        entry2 = &table.e[i];
        for (; i < 21; entry2++, i++) {
            if (entry2->link >= 0 && link != entry2->link) {
                code2 = entry2->code;
                cand2 = arg2;
                for (j = 0; j < arg3; cand2 += 8, j++) {
                    if (code2 == *(s32*)cand2) {
                        break;
                    }
                }
                if (j < arg3) {
                    break;
                }
            }
        }
        if (i < 21) {
            msg = 0x4201;
        }
    }

    name = pokemonBiosGetNicknamePtr(PC_PTR(arg4));
    msgctrlSetValue(0x32, PC_PTR32(name));
    msgctrlSetValue(0x2F, (u32)((s32)x));
    msgctrlSetValue(0x30, (u32)((s32)y));
    GSmsgGetText(arg0, arg1 + 1, PC_PTR32_IMPLICIT((void*)msg));
}

#if !defined(GS_TITLE_800216E8_ONLY)
/* itemFieldUsePlayTimeFlute - 0x800218BC | size: 0x1e0 */
extern u8 lbl_803A1B90[];
extern u32 lbl_8047B8A0;
/* NAME-T3 fn_800218BC */ /* only the TIME FLUTE uses it: checks the "usable now" list, then purifies Shadow Pokemon */
s32 itemFieldUsePlayTimeFlute(u32 arg0, u32* arg1)
{
    extern f32 pokemonGetDp(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgClose(s32);
#endif
    f32 thresh;
    void* sp_c;
    s32 sp_8;
    s32 count;
    s32 idx;
    s32 ok;
    s32 ctr;
    u8* p;
    s32 i;

    p = lbl_803A1B90;
    for (count = *((s32*) (p + 0x40)), ctr = count, i = 0; ctr > 0; ctr--) {
        if (arg0 == (*((u16*) p))) {
            thresh = thresh;
            break;
        }
        p += 8;
        i++;
    }

    if (i >= count) {
        winMsgOpen(2, 0x426A, 1, 0);
        winMsgClose(1);
        return 1;
    }
    i = 0;
    thresh = *((f32*) (&lbl_8047B8A0));
    do {
        itemUseGetTargetPokemon(i, &sp_c, &sp_8);
        if (((u8) pokemonCheckValid((u8*)sp_c)) != 0) {
            if (pokemonIsDarkPokemon((s32) sp_c) != 0) {
                if (pokemonGetDp(sp_c) > thresh) {
                    break;
                }
            }
        }
        i++;
    } while (i < 6);

    if (i >= 6) {
        winMsgOpen(2, 0x4261, 1, 0);
        winMsgClose(1);
        return 1;
    }
    idx = itemUseOpenPartyMenu((s32)arg0, 1);
    if (idx >= 0) {
        itemUseGetTargetPokemon(idx, &sp_c, &sp_8);
        ok = 0;
        if (pokemonIsDarkPokemon((s32) sp_c) != 0) {
            if (pokemonGetDp(sp_c) > (*((f32*) (&lbl_8047B8A0)))) {
                ok = 1;
                ok = 1;
            }
        }
        if (ok == 0) {
            winMsgOpen(2, 0x4261, 1, 0);
            winMsgClose(1);
        }
    }
    itemUseClosePartyMenu(idx);
    if ((idx >= 0) && (ok != 0)) {
        *arg1 = 1;
        return 2;
    }
    return 1;
}
#endif /* !GS_TITLE_800216E8_ONLY */
#endif

/* itemFieldUseCheckUsableList - 0x80021A9C | size: 0x78 */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80021A9C)
/* NAME-T3 fn_80021A9C */ /* bag handler of the 38 key items: usable only if listed in lbl_803A1B90, else msg 0x426A */
s32 itemFieldUseCheckUsableList(u32 r3, u32* r4) {
    u8* r5;
    s32 count;

    r5 = lbl_803A1B90;
    count = *(s32*)(lbl_803A1B90 + 0x40);
    for (; count > 0; count--) {
        if (r3 == (u32)*(u16*)r5) {
            *r4 = 0;
            return 2;
        }
        r5 += 8;
    }
    winMsgOpen(2, 0x426a, 1, 0);
    winMsgClose(1);
    return 1;
}
#endif

/* itemFieldUseRecover - 0x80021B14 | size: 0x53c
 *
 * Field item-use handler: dispatches on itemParamGetRecoverType() of the
 * current item, applies it to the chosen party Pokemon and reports through
 * the message window; returns 0 when the item was used (and sets *arg1).
 *
 * STATUS (99.82%, one instruction): the 0/1 `result` of the 3..11 case.
 * Retail builds it in r0 at a join and returns it with `mr r3,r0`; a u8
 * result gives `clrlwi r3,r0,24` and any 32-bit result is coalesced into r3.
 * Ruled out: every result/return type, declaration position, if/else /
 * goto / ternary / shared-return forms, whole-case and tail inline helpers,
 * C++, peephole and compiler version. The only other function with this
 * join is menuPokemonModelIsLoaded, where it comes from a u8 result fed by two
 * expansions of a multi-return inline (faceKeyEqual), so the original
 * probably used a shared inline helper here too (itemFieldUseParamItem ends the same
 * way). Reusing arg0 as the result (permuter lead) builds it in arg0's saved
 * register (r29) instead of r0, and reusing any other case local lands it
 * in r3 or on the stack. Details: Working/refs/upstream-matches-leftovers.tsv.
 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80142EF8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801431AC(void);
#endif
extern s32 fn_80014110(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonIsJoutaiDataId(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s16 itemUse2Pokemon(void*, s32, s32, u16, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStart(s32, s32, s32, s32);
#endif
extern void menuPokemonRefresh(void);
extern u8 lbl_80478890[8];
extern u8 lbl_80266DB0[];
extern u8 lbl_80266D78[];
extern s32 menuUseItemPPRecovery(s32, u32*);
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80021B14) || \
    defined(GS_TITLE_CANDIDATE_80022478)
/* Healing-item result: when the item took effect, set *arg1 from the item
 * id and return 0; otherwise return 1. Shared by itemFieldUseRecover and
 * itemFieldUseParamItem. */
/* RULE-EXCEPTION(user-approved): inline helper, register-only evidence - see docs/RULE_EXCEPTIONS.md */
static inline s32 gsTitleRecoverItemResult(s32 slot, s16 effect_count, u32 arg0, u32* arg1) {
    if (slot >= 0 && effect_count > 0) {
        switch (arg0) {
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
            *arg1 = 0;
            break;
        default:
            *arg1 = 1;
            break;
        }
        return 0;
    }
    return 1;
}
#endif
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80021B14)
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 itemFieldUseRecover(u32 arg0, u32* arg1) {
    typedef struct TitleEffectEntry {
        s32 code;
        s16 x;
        s16 y;
    } TitleEffectEntry;
    typedef struct TitleSpecialEntry {
        s32 code;
        s32 message;
    } TitleSpecialEntry;
    typedef struct TitleSpecialTable {
        TitleSpecialEntry entries[7];
    } TitleSpecialTable;
    typedef struct TitleBgmTable {
        u16 entries[5];
    } TitleBgmTable;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemUseFormatMessage(void*, s32, void*, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUseOpenPartyMenu(void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonIsJoutaiDataId(s32, s32);
#endif
    u8 item_param[0x10];
    u8 result;

    itemParamConvertOrigFormat(item_param, lbl_80478890);
    switch ((s32)itemParamGetRecoverType(item_param)) {
    case 0: {
        char message[0x84];
        TitleEffectEntry effects[32];
        TitleSpecialTable special;
        TitleBgmTable bgm;
        s32 slot;
        s32 data;
        s32 pokemon;
        s32 effectCount;
        s32 text;
        s32 code;
        s32 specialIndex;
        s32 effectIndex;
        TitleEffectEntry* effect;
        s32 bgmIndex;
        s32 y;
        s32 x;
        TitleSpecialEntry* entry;
        s32 owner;

        slot = fn_80014110();
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E)) {
            msgctrlSetValue(0x32, PC_PTR32_IMPLICIT((void*)pokemonBiosGetNicknamePtr(PC_PTR(pokemon))));
            winMsgOpen(2, 0x424D, 1, 0);
            winMsgClose(1);
            return 1;
        }

        effectCount = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, 0);
        if ((s16)effectCount > 0) {
            bgm = *(TitleBgmTable*)lbl_80266DB0;
            for (bgmIndex = 0; bgmIndex < 5; bgmIndex++) {
                if (arg0 == bgm.entries[bgmIndex]) {
                    break;
                }
            }
            soundStart(bgmIndex < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
            menuPokemonRefresh();
        }

        owner = pokemon;
        special = *(TitleSpecialTable*)lbl_80266D78;
        if ((s16)effectCount <= 0) {
            GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
        } else {
            entry = special.entries;
            for (specialIndex = 0; specialIndex < 7; entry++, specialIndex++) {
                code = entry->code;
                effect = effects;
                for (effectIndex = 0; effectIndex < (s16)effectCount; effect++, effectIndex++) {
                    if (code == effect->code) {
                        break;
                    }
                }
                if (effectIndex < (s16)effectCount) {
                    break;
                }
            }

            if (specialIndex >= 7) {
                GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
            } else {
                text = special.entries[specialIndex].message;
                x = effects[effectIndex].x;
                y = effects[effectIndex].y;
                msgctrlSetValue(0x32, PC_PTR32_IMPLICIT((void*)pokemonBiosGetNicknamePtr(PC_PTR(owner))));
                msgctrlSetValue(0x2F, PC_PTR32_IMPLICIT((void*)x));
                msgctrlSetValue(0x30, PC_PTR32_IMPLICIT((void*)y));
                GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)PC_PTR(text)));
            }
        }

        msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(message));
        winMsgOpen(2, 0xE0, 1, 0);
        winMsgClose(1);
        if ((s16)effectCount > 0) {
            *arg1 = 1;
            return 0;
        }
        return 1;
    }
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11: {
        u8 name[0x84];
        s32 data;
        TitleBgmTable bgm;
        u8 effects[0x100];
        s32 effect_count;
        s32 slot;
        s32 pokemon;
        s32 i;

        slot = itemUseOpenPartyMenu(PC_PTR32_IMPLICIT((void*)PC_PTR(arg0)), 1);
        if (slot >= 0) {
            itemUseGetTargetPokemon(slot, &pokemon, &data);
            if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) == 0) {
                effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, 0);
                if ((s16)effect_count > 0) {
                    bgm = *(TitleBgmTable*)lbl_80266DB0;
                    for (i = 0; i < 5; i++) {
                        if (arg0 == bgm.entries[i]) {
                            break;
                        }
                    }
                    soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                    menuPokemonRefresh();
                }
                itemUseFormatMessage(name, 0x40, effects, effect_count, pokemon);
                msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(name));
                winMsgOpen(2, 0xE0, 1, 0);
                winMsgClose(1);
            } else {
                msgctrlSetValue(0x32, PC_PTR32_IMPLICIT((void*)pokemonBiosGetNicknamePtr(PC_PTR(pokemon))));
                winMsgOpen(2, 0x424D, 1, 0);
                winMsgClose(1);
                effect_count = 0;
            }
        }
        itemUseClosePartyMenu(slot);
        if (slot >= 0 && (s16)effect_count > 0) {
            switch (arg0) {
            case 0x27:
            case 0x28:
            case 0x29:
            case 0x2A:
            case 0x2B:
                *arg1 = 0;
                break;
            default:
                *arg1 = 1;
                break;
            }
            result = 0;
        } else {
            result = 1;
        }
        return result;
    }
    case 21:
        return menuUseItemPPRecovery(arg0, arg1);
    default:
        winMsgOpen(2, 0x426A, 1, 0);
        winMsgClose(1);
        return 1;
    }
}
#else
s32 itemFieldUseRecover(u32 arg0, u32* arg1) {
    typedef struct TitleEffectEntry {
        s32 code;
        s16 x;
        s16 y;
    } TitleEffectEntry;
    typedef struct TitleSpecialEntry {
        s32 code;
        s32 message;
    } TitleSpecialEntry;
    typedef struct TitleSpecialTable {
        TitleSpecialEntry entries[7];
    } TitleSpecialTable;
    typedef struct TitleBgmTable {
        u16 entries[5];
    } TitleBgmTable;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemUseFormatMessage(void*, s32, void*, s32, s32);
    extern u8 pokemonIsJoutaiDataId(s32, s32);
#endif
    u8 item_param[0x10];
    s32 result;

    itemParamConvertOrigFormat(item_param, lbl_80478890);
    switch ((s32)itemParamGetRecoverType(item_param)) {
    case 0: {
        char message[0x84];
        TitleEffectEntry effects[32];
        TitleSpecialTable special;
        TitleBgmTable bgm;
        s32 slot;
        s32 data;
        s32 pokemon;
        s32 effectCount;
        s32 text;
        s32 code;
        s32 specialIndex;
        s32 effectIndex;
        TitleEffectEntry* effect;
        s32 bgmIndex;
        s32 y;
        s32 x;
        TitleSpecialEntry* entry;
        s32 owner;

        slot = fn_80014110();
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if (pokemonIsJoutaiDataId(pokemon, 0x3E)) {
            msgctrlSetValue(0x32, (u32)pokemonBiosGetNicknamePtr(pokemon));
            winMsgOpen(2, 0x424D, 1, 0);
            winMsgClose(1);
            return 1;
        }

        effectCount = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, 0);
        if ((s16)effectCount > 0) {
            bgm = *(TitleBgmTable*)lbl_80266DB0;
            for (bgmIndex = 0; bgmIndex < 5; bgmIndex++) {
                if (arg0 == bgm.entries[bgmIndex]) {
                    break;
                }
            }
            soundStart(bgmIndex < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
            menuPokemonRefresh();
        }

        owner = pokemon;
        special = *(TitleSpecialTable*)lbl_80266D78;
        if ((s16)effectCount <= 0) {
            GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
        } else {
            entry = special.entries;
            for (specialIndex = 0; specialIndex < 7; entry++, specialIndex++) {
                code = entry->code;
                effect = effects;
                for (effectIndex = 0; effectIndex < (s16)effectCount; effect++, effectIndex++) {
                    if (code == effect->code) {
                        break;
                    }
                }
                if (effectIndex < (s16)effectCount) {
                    break;
                }
            }

            if (specialIndex >= 7) {
                GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
            } else {
                text = special.entries[specialIndex].message;
                x = effects[effectIndex].x;
                y = effects[effectIndex].y;
                msgctrlSetValue(0x32, (u32)pokemonBiosGetNicknamePtr(owner));
                msgctrlSetValue(0x2F, (u32)x);
                msgctrlSetValue(0x30, (u32)y);
                GSmsgGetText(message, 0x41, (void*)text);
            }
        }

        msgctrlSetValue(0x4D, (u32)message);
        winMsgOpen(2, 0xE0, 1, 0);
        winMsgClose(1);
        if ((s16)effectCount > 0) {
            *arg1 = 1;
            return 0;
        }
        return 1;
    }
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11: {
        u8 name[0x84];
        s32 data;
        TitleBgmTable bgm;
        u8 effects[0x100];
        s32 effect_count;
        s32 slot;
        s32 pokemon;
        s32 i;

        slot = itemUseOpenPartyMenu((s32)arg0, 1);
        if (slot >= 0) {
            itemUseGetTargetPokemon(slot, &pokemon, &data);
            if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) == 0) {
                effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, 0);
                if ((s16)effect_count > 0) {
                    bgm = *(TitleBgmTable*)lbl_80266DB0;
                    for (i = 0; i < 5; i++) {
                        if (arg0 == bgm.entries[i]) {
                            break;
                        }
                    }
                    soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                    menuPokemonRefresh();
                }
                itemUseFormatMessage(name, 0x40, effects, effect_count, pokemon);
                msgctrlSetValue(0x4D, (u32)name);
                winMsgOpen(2, 0xE0, 1, 0);
                winMsgClose(1);
            } else {
                msgctrlSetValue(0x32, (u32)pokemonBiosGetNicknamePtr(pokemon));
                winMsgOpen(2, 0x424D, 1, 0);
                winMsgClose(1);
                effect_count = 0;
            }
        }
        itemUseClosePartyMenu(slot);
        result = gsTitleRecoverItemResult(slot, effect_count, arg0, arg1);
        break;
    }
    case 21:
        result = menuUseItemPPRecovery(arg0, arg1);
        break;
    default:
        winMsgOpen(2, 0x426A, 1, 0);
        winMsgClose(1);
        result = 1;
        break;
    }
    return result;
}
#endif
#endif

/* itemFieldUseBall - 0x80022050 | size: 0x12c */
extern s32 pcboxGetNbPokemonBox(void);
extern s32 fn_801347E8(s32, s8);
extern void fn_800140FC(s32*, s32*);
extern s32 fightTrainerIsSelectedItemBall(s32);
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80022050)
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 itemFieldUseBall(s32 arg0, s32* arg1) {
    s32 sp14;
    s32 sp10;
    s32 spC;
    s32 sp8;
    s32 iVar4;
    s32 iVar3;
    s8 cVar1;
    s8 cVar2;

    iVar3 = 0;
    iVar4 = 0;
    do {
        itemUseGetTargetPokemon(iVar4, &spC, &sp8);
        if ((u8)pokemonCheckValid(PC_PTR(spC)) != 0) {
            iVar3 = iVar3 + 1;
        }
        iVar4 = iVar4 + 1;
    } while (iVar4 < 6);
    if (iVar3 >= 6) {
        cVar1 = pcboxGetNbPokemonBox();
        iVar3 = 0;
        while (iVar3 < cVar1) {
            cVar2 = pcboxGetPokemonBoxNbEmptySlot(0, (s8)iVar3);
            if (cVar2 > 0) {
                break;
            }
            iVar3 = iVar3 + 1;
        }
        if (iVar3 >= cVar1) {
            winMsgOpen(2, 0x4416, 1, 0);
            winMsgClose(1);
            return 1;
        }
    }
    fn_800140FC(&sp14, &sp10);
    if ((u8)fightTrainerIsSelectedItemBall(sp10) != 0) {
        winMsgOpen(2, 0x426d, 1, 0);
        winMsgClose(1);
        return 1;
    }
    *arg1 = 0;
    return 0;
}
#else
s32 itemFieldUseBall(s32 arg0, s32* arg1) {
    s32 sp14;
    s32 sp10;
    s32 spC;
    s32 sp8;
    s32 iVar4;
    s32 iVar3;
    s8 cVar1;
    s8 cVar2;

    iVar3 = 0;
    iVar4 = 0;
    do {
        itemUseGetTargetPokemon(iVar4, &spC, &sp8);
        if ((u8)pokemonCheckValid((u8*)spC) != 0) {
            iVar3 = iVar3 + 1;
        }
        iVar4 = iVar4 + 1;
    } while (iVar4 < 6);
    if (iVar3 >= 6) {
        cVar1 = pcboxGetNbPokemonBox();
        iVar3 = 0;
        while (iVar3 < cVar1) {
            cVar2 = pcboxGetPokemonBoxNbEmptySlot(0, (s8)iVar3);
            if (cVar2 > 0) {
                break;
            }
            iVar3 = iVar3 + 1;
        }
        if (iVar3 >= cVar1) {
            winMsgOpen(2, 0x4416, 1, 0);
            winMsgClose(1);
            return 1;
        }
    }
    fn_800140FC(&sp14, &sp10);
    if ((u8)fightTrainerIsSelectedItemBall(sp10) != 0) {
        winMsgOpen(2, 0x426d, 1, 0);
        winMsgClose(1);
        return 1;
    }
    *arg1 = 0;
    return 0;
}
#endif
#endif

/* itemBattleUseApplyXItem - 0x8002217C | size: 0x2fc */
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_EXACT_8002217C)
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
/* NAME-T3 fn_8002217C */ /* battle handler of the X items, Guard Spec and Dire Hit (item table +0x20) */
s32 itemBattleUseApplyXItem(u32 wazaDataId, u32* result) {
    typedef struct TitleEffectEntry {
        s32 code;
        s16 x;
        s16 y;
    } TitleEffectEntry;
    typedef struct TitleSpecialEntry {
        s32 code;
        s32 message;
    } TitleSpecialEntry;
    typedef struct TitleSpecialTable {
        TitleSpecialEntry entries[7];
    } TitleSpecialTable;
    typedef struct TitleBgmTable {
        u16 entries[5];
    } TitleBgmTable;
    TitleEffectEntry effects[33];
    char message[0x84];
    TitleSpecialTable special;
    TitleBgmTable bgm;
    s32 slot;
    s32 pokemon;
    s32 data;
    s16 effectCount;
    s32 soundId;
    s32 text;
    s32 code;
    s32 specialIndex;
    s32 effectIndex;
    TitleEffectEntry* effect;
    s32 bgmIndex;
    s32 y;
    void* nickname;
    s32 x;
    TitleSpecialEntry* entry;
    s32 owner;

    slot = fn_80014110();
    itemUseGetTargetPokemon(slot, &pokemon, &data);
    if ((u8)pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E)) {
        nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
        msgctrlSetValue(0x32, PC_PTR32_IMPLICIT(nickname));
        winMsgOpen(2, 0x424D, 1, 0);
        winMsgClose(1);
        return 1;
    }

    effectCount =
        itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), wazaDataId, 0);
    if (effectCount > 0) {
        bgm = *(TitleBgmTable*)lbl_80266DB0;
        for (bgmIndex = 0; bgmIndex < 5; bgmIndex++) {
            if (wazaDataId == bgm.entries[bgmIndex]) {
                break;
            }
        }
        soundId = bgmIndex < 5 ? 0x466 : 0x465;
        soundStart(soundId, 0, 0xFF, 0);
        menuPokemonRefresh();
    }

    owner = pokemon;
    special = *(TitleSpecialTable*)lbl_80266D78;
    if (effectCount <= 0) {
        GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
    } else {
        entry = special.entries;
        for (specialIndex = 0; specialIndex < 7; entry++, specialIndex++) {
            code = entry->code;
            effect = effects;
            for (effectIndex = 0; effectIndex < effectCount; effect++, effectIndex++) {
                if (code == effect->code) {
                    break;
                }
            }
            if (effectIndex < effectCount) {
                break;
            }
        }

        if (specialIndex >= 7) {
            GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
        } else {
            text = special.entries[specialIndex].message;
            x = effects[effectIndex].x;
            y = effects[effectIndex].y;
            msgctrlSetValue(0x32, PC_PTR32_IMPLICIT((void*)pokemonBiosGetNicknamePtr(PC_PTR(owner))));
            msgctrlSetValue(0x2F, PC_PTR32_IMPLICIT((void*)x));
            msgctrlSetValue(0x30, PC_PTR32_IMPLICIT((void*)y));
            GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)PC_PTR(text)));
        }
    }

    msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(message));
    winMsgOpen(2, 0xE0, 1, 0);
    winMsgClose(1);
    if (effectCount > 0) {
        *result = 1;
        return 0;
    }
    return 1;
}
#else
/* NAME-T3 fn_8002217C */
s32 itemBattleUseApplyXItem(u32 wazaDataId, u32* result) {
    typedef struct TitleEffectEntry {
        s32 code;
        s16 x;
        s16 y;
    } TitleEffectEntry;
    typedef struct TitleSpecialEntry {
        s32 code;
        s32 message;
    } TitleSpecialEntry;
    typedef struct TitleSpecialTable {
        TitleSpecialEntry entries[7];
    } TitleSpecialTable;
    typedef struct TitleBgmTable {
        u16 entries[5];
    } TitleBgmTable;
    TitleEffectEntry effects[33];
    char message[0x84];
    TitleSpecialTable special;
    TitleBgmTable bgm;
    s32 slot;
    s32 pokemon;
    s32 data;
    s16 effectCount;
    s32 soundId;
    s32 text;
    s32 code;
    s32 specialIndex;
    s32 effectIndex;
    TitleEffectEntry* effect;
    s32 bgmIndex;
    s32 y;
    void* nickname;
    s32 x;
    TitleSpecialEntry* entry;
    s32 owner;

    slot = fn_80014110();
    itemUseGetTargetPokemon(slot, &pokemon, &data);
    if ((u8)pokemonIsJoutaiDataId(pokemon, 0x3E)) {
        nickname = pokemonBiosGetNicknamePtr(pokemon);
        msgctrlSetValue(0x32, (u32)nickname);
        winMsgOpen(2, 0x424D, 1, 0);
        winMsgClose(1);
        return 1;
    }

    effectCount =
        itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), wazaDataId, 0);
    if (effectCount > 0) {
        bgm = *(TitleBgmTable*)lbl_80266DB0;
        for (bgmIndex = 0; bgmIndex < 5; bgmIndex++) {
            if (wazaDataId == bgm.entries[bgmIndex]) {
                break;
            }
        }
        soundId = bgmIndex < 5 ? 0x466 : 0x465;
        soundStart(soundId, 0, 0xFF, 0);
        menuPokemonRefresh();
    }

    owner = pokemon;
    special = *(TitleSpecialTable*)lbl_80266D78;
    if (effectCount <= 0) {
        GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
    } else {
        entry = special.entries;
        for (specialIndex = 0; specialIndex < 7; entry++, specialIndex++) {
            code = entry->code;
            effect = effects;
            for (effectIndex = 0; effectIndex < effectCount; effect++, effectIndex++) {
                if (code == effect->code) {
                    break;
                }
            }
            if (effectIndex < effectCount) {
                break;
            }
        }

        if (specialIndex >= 7) {
            GSmsgGetText(message, 0x41, PC_PTR32_IMPLICIT((void*)0x4261));
        } else {
            text = special.entries[specialIndex].message;
            x = effects[effectIndex].x;
            y = effects[effectIndex].y;
            msgctrlSetValue(0x32, (u32)pokemonBiosGetNicknamePtr(owner));
            msgctrlSetValue(0x2F, (u32)x);
            msgctrlSetValue(0x30, (u32)y);
            GSmsgGetText(message, 0x41, (void*)text);
        }
    }

    msgctrlSetValue(0x4D, (u32)message);
    winMsgOpen(2, 0xE0, 1, 0);
    winMsgClose(1);
    if (effectCount > 0) {
        *result = 1;
        return 0;
    }
    return 1;
}
#endif
#endif

#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_CANDIDATE_80022478)
/* itemFieldUseParamItem - 0x80022478 | size: 0x2a8 */
extern void jumptable_802E4F00();
extern u8 lbl_80478888[8];
extern s32 menuUseItemWPointUp(u32, u32*);
extern s32 menuUseItemListLVUp(u32, u32*);
extern s32 menuUseItemAlldeathRcv(u32, u32*);
#ifdef TARGET_PC /* PC: our pre-merge copy (PC fixes); the GameCube keeps the merged copy */
s32 itemFieldUseParamItem(u32 arg0, u32* arg1) {
    typedef struct TitleBgmList {
        u16 entries[5];
    } TitleBgmList;
    u8 name[0x84];
    s32 data;
    s16 effect_count;
    u8 item_param[0x10];
    u8 effects[0x100];
    s32 pokemon;
    s32 slot;
    TitleBgmList bgm;
    s32 i;
    u8 result;

    itemParamConvertOrigFormat(item_param, lbl_80478888);
    switch (itemParamGetRecoverType(item_param)) {
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
        slot = itemUseOpenPartyMenu(PC_PTR32_IMPLICIT((void*)PC_PTR(arg0)), 1);
        if (slot >= 0) {
            itemUseGetTargetPokemon(slot, &pokemon, &data);
            if ((u8)pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) == 0) {
                effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), (u16)arg0, 0);
                if (effect_count > 0) {
                    bgm = *(TitleBgmList*)lbl_80266DB0;
                    for (i = 0; i < 5; i++) {
                        if (arg0 == bgm.entries[i]) {
                            break;
                        }
                    }
                    soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                    menuPokemonRefresh();
                }
                itemUseFormatMessage(name, 0x40, (u8*)effects, effect_count, pokemon);
                msgctrlSetValue(0x4D, PC_PTR32_IMPLICIT(name));
                winMsgOpen(2, 0xE0, 1, 0);
                winMsgClose(1);
            } else {
                msgctrlSetValue(0x32, PC_PTR32_IMPLICIT((void*)pokemonBiosGetNicknamePtr(PC_PTR(pokemon))));
                winMsgOpen(2, 0x424D, 1, 0);
                winMsgClose(1);
                effect_count = 0;
            }
        }
        itemUseClosePartyMenu(slot);
        if (slot >= 0 && effect_count > 0) {
            switch (arg0) {
            case 0x27:
            case 0x28:
            case 0x29:
            case 0x2A:
            case 0x2B:
                *arg1 = 0;
                break;
            default:
                *arg1 = 1;
                break;
            }
            result = 0;
        } else {
            result = 1;
        }
        return result;
    case 1:
        return menuUseItemAlldeathRcv(arg0, arg1);
    case 2:
        return menuUseItemListLVUp(arg0, arg1);
    case 19:
    case 20:
        return menuUseItemWPointUp(arg0, arg1);
    case 21:
        return menuUseItemPPRecovery(arg0, arg1);
    default:
        winMsgOpen(2, 0x426A, 1, 0);
        winMsgClose(1);
        return 1;
    }
}
#else
s32 itemFieldUseParamItem(u32 arg0, u32* arg1) {
    typedef struct TitleBgmList {
        u16 entries[5];
    } TitleBgmList;
    u8 name[0x84];
    s32 data;
    s16 effect_count;
    u8 item_param[0x10];
    u8 effects[0x100];
    s32 pokemon;
    s32 slot;
    TitleBgmList bgm;
    s32 i;
    s32 result;

    itemParamConvertOrigFormat(item_param, lbl_80478888);
    switch (itemParamGetRecoverType(item_param)) {
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 10:
    case 11:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
        slot = itemUseOpenPartyMenu((s32)arg0, 1);
        if (slot >= 0) {
            itemUseGetTargetPokemon(slot, &pokemon, &data);
            if ((u8)pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) == 0) {
                effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), (u16)arg0, 0);
                if (effect_count > 0) {
                    bgm = *(TitleBgmList*)lbl_80266DB0;
                    for (i = 0; i < 5; i++) {
                        if (arg0 == bgm.entries[i]) {
                            break;
                        }
                    }
                    soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                    menuPokemonRefresh();
                }
                itemUseFormatMessage(name, 0x40, (u8*)effects, effect_count, pokemon);
                msgctrlSetValue(0x4D, (u32)name);
                winMsgOpen(2, 0xE0, 1, 0);
                winMsgClose(1);
            } else {
                msgctrlSetValue(0x32, (u32)pokemonBiosGetNicknamePtr(pokemon));
                winMsgOpen(2, 0x424D, 1, 0);
                winMsgClose(1);
                effect_count = 0;
            }
        }
        itemUseClosePartyMenu(slot);
        result = gsTitleRecoverItemResult(slot, effect_count, arg0, arg1);
        break;
    case 1:
        result = menuUseItemAlldeathRcv(arg0, arg1);
        break;
    case 2:
        result = menuUseItemListLVUp(arg0, arg1);
        break;
    case 19:
    case 20:
        result = menuUseItemWPointUp(arg0, arg1);
        break;
    case 21:
        result = menuUseItemPPRecovery(arg0, arg1);
        break;
    default:
        winMsgOpen(2, 0x426A, 1, 0);
        winMsgClose(1);
        result = 1;
        break;
    }
    return result;
}
#endif
#endif

#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_EXACT_80022720)
/* itemFieldUseReadEinFile - 0x80022720 | size: 0x114
 *
 * itemFieldUseReadEinFile(u32 arg0, u32* arg1) -> s32  -- the Ein File reader (using an
 * Ein File from the bag, 2026-11-06, verified from the table and the
 * pick-up path; proposed name itemFieldUseEinFile). *Corrected 2026-11-06:* an
 * older comment here called it a name-entry validation error dispatcher.
 * Copies the 5-row {item id, text id} table at lbl_80266C54 (each entry:
 * 2-byte item, 2-byte pad, 4-byte message id; items 0x21A-0x21D and 0x223,
 * the Ein Files), compares arg0 (the item) against each row's item (offsets
 * 0, 8, 16, 24, 32) and shows that row's text. If no row matches, the index
 * becomes 5, one row past the table's end (no not-found branch; vanilla never
 * gets there, since only Ein Files call it).
 *
 * Source form uses Ghidra comma-chain idiom so iVar1 increments per
 * failed compare:
 *   iVar1 = 0;
 *   if (!(a != h0 && (iVar1=1, a != h1) && (iVar1=2, a != h2) && ...))
 *     iVar1 = 5;
 *
 * Calls winMsgOpen(2, msg_from_row_iVar1, 1, 0) then winMsgClose(1) to
 * print the error message, writes 0 to *arg1, returns 0.
 *
 * The table copy and dispatcher match in the isolated 0x80022720 unit.
 */
extern u8 lbl_80266C54[];
typedef struct TitleMsgEnt {
    u16 id;
    u32 msg;
} TitleMsgEnt;
typedef struct TitleMsgTable {
    TitleMsgEnt e[5];
} TitleMsgTable;
/* NAME-T3 fn_80022720 */
s32 itemFieldUseReadEinFile(u32 arg0, u32* arg1) {
    TitleMsgTable t;
    s32 i;

    t = *(TitleMsgTable*)lbl_80266C54;
    i = 0;
    for (i = 0; i < 5; i++) {
        if (arg0 == t.e[i].id) {
            break;
        }
    }
    winMsgOpen(2, t.e[i].msg, 1, 0);
    winMsgClose(1);
    *arg1 = 0;
    return 0;
}
#endif

#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_CANDIDATE_80022834)

/* itemFieldUseTM - 0x80022834 | size: 0x308 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetWazaMachineNo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetWazaIDByWazaMachineNo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void itemDataBiosGetHidenMachineNo(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosGetPokemonWazaDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonBiosGetPokemonDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonDataBiosGetWazaMcn(void);
#endif
#ifndef TARGET_PC /* wrong type (evolutionWazaLearn takes six arguments; see below); hidden on the PC */
extern void evolutionWazaLearn(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonWazaCreate(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonGetSoubiItemSoubiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonGetFriendFormPokemonFriendFilterId(void);
#endif
/* RULE-EXCEPTION(user-approved): single-use inline helper, register-only
 * evidence - see docs/RULE_EXCEPTIONS.md. Its inlined return temp gives
 * retail's tmp/effect copy and register numbering for itemFieldUseTM. */
static inline s32 gsTitleTeachWazaMachine(s32 c, u32 status) {
    extern u32 itemDataBiosGetWazaIDByWazaMachineNo(u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetPokemonWazaDataId(s32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosGetPokemonDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonDataBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonDataBiosGetWazaMcn(s32, s32);
#endif
    extern s32 evolutionWazaLearn(s32, u32, void*, s32, void*, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCreate(s32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemSoubiDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonGetFriendFormPokemonFriendFilterId(s32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cbForgetWazaSelect__FP7PokemonUsl(void);
#endif
    s32 i;
    s32 tmp;
    u32 v2;
    u8 buf;

    if (pokemonIsDarkPokemon(PC_PTR(c)) != 0) {
        winMsgOpen(2, 0x424C, 1, 0);
        winMsgClose(1);
        return 0;
    } else {
        v2 = itemDataBiosGetWazaIDByWazaMachineNo(status);
        for (i = 0; i < 4; i++) {
            if ((u16)v2 == (u16)pokemonBiosGetPokemonWazaDataId(PC_PTR(c), i)) {
                break;
            }
        }
        if (i < 4) {
            msgctrlSetValue(0x32, PC_PTR32(pokemonBiosGetNicknamePtr(PC_PTR(c))));
            msgctrlSetValue(0x39, (u32)((u16)v2));
            winMsgOpen(2, 0x4244, 1, 0);
            winMsgClose(1);
            return 0;
        } else {
#ifdef TARGET_PC
            /* PC: the species id as an argument; on the GameCube pokemonBiosGetPokemonDataId's result stays in r3 */
            if (pokemonDataBiosGetWazaMcn(pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(PC_PTR(c))), status) == 0) {
#else
            pokemonBiosGetPokemonDataId(c);
            if (pokemonDataBiosGetWazaMcn(pokemonDataBiosGetPtr(), status) == 0) {
#endif
                msgctrlSetValue(0x32, PC_PTR32(pokemonBiosGetNicknamePtr(PC_PTR(c))));
                msgctrlSetValue(0x39, (u32)((u16)v2));
                winMsgOpen(2, 0x423F, 1, 0);
                winMsgClose(1);
                return 0;
            } else {
                tmp = evolutionWazaLearn(c, v2, &buf, 1, cbForgetWazaSelect__FP7PokemonUsl, 0);
                if (tmp != 0) {
                    pokemonWazaCreate(PC_PTR(c), buf, v2 & 0xFFFF);
                    pokemonGetFriendFormPokemonFriendFilterId(PC_PTR(c), pokemonGetSoubiItemSoubiDataId(PC_PTR(c)), 4);
                }
            }
        }
    }
    return tmp;
}

s32 itemFieldUseTM(u32 arg0, u32* arg1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetWazaMachineNo(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetWazaIDByWazaMachineNo(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 itemDataBiosGetHidenMachineNo(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuSubOpenYesNo(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetPokemonWazaDataId(s32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetPokemonDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 pokemonDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonDataBiosGetWazaMcn(s32, s32);
#endif
    extern s32 evolutionWazaLearn(s32, u32, void*, s32, void*, s32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCreate(s32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetSoubiItemSoubiDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonGetFriendFormPokemonFriendFilterId(s32, u32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void cbForgetWazaSelect__FP7PokemonUsl(void);
#endif

    u16 value16;
    u8 status;
    s32 msg;
    void* handle;
    s32 state;
    s32 type_byte;
    s32 action;
    s32 effect;
    s32 slot;
    u32 sc;
    s32 sd;

    handle = itemDataBiosGetPtr((u16)arg0);
    status = itemDataBiosGetWazaMachineNo(handle) & 0xFF;
    value16 = itemDataBiosGetWazaIDByWazaMachineNo(status);
#ifdef TARGET_PC
    /* OrreHook_TmSkipAsk (step 3.7, the TM design; pc/orre_hooks.h): when the party list stays open after the TM was not
     * learned (a refusal, or "did not learn"; *changed 2026-11-20*, snatcherLeo's hand check: a taught TM goes back to the bag,
     * and every new use from the bag asks), the boot-up sound, "Booted up a TM." and "Teach it to a Pokémon?" are
     * skipped, as the player already said yes: straight back to the party list. NULL or 0: the game's own, as on the
     * GameCube. All platforms */
    if (ORRE_TM_SKIP_ASK(arg0)) {
        type_byte = itemDataBiosGetHidenMachineNo(handle);
        goto orre_tm_list;
    }
#endif
    soundStart(0x4CB, 0, 0xFF, 0);
    type_byte = itemDataBiosGetHidenMachineNo(handle);
    if ((u8)type_byte != 0xFF) {
        msg = 0x4260;
    } else {
        msg = 0x4265;
    }
    msgctrlSetValue(0x39, (u32)value16);
    winMsgOpen(2, msg, 1, 0);
    winMsgClose(1);
    winMsgOpen(2, 0x426B, 1, 0);
    state = (s8)menuSubOpenYesNo(0, -1, -1, 0);
    winMsgClose(1);
    switch (state) {
    case 0:  action = 0; break;
    case 1:  action = 1; break;
    default: action = 2; break;
    }
    if (action == 1 || action == 2) {
        return 1;
    }

#ifdef TARGET_PC
orre_tm_list:
#endif
    slot = itemUseOpenPartyMenu((s32)arg0, 1);
    if (slot >= 0) {
        itemUseGetTargetPokemon(slot, (s32*)&sc, (s32*)&sd);
        effect = gsTitleTeachWazaMachine(sc, status);
    }

    itemUseClosePartyMenu(slot);
    if (slot >= 0 && effect != 0) {
        if ((u8)type_byte != 0xFF) {
            *arg1 = 0;
        } else {
#ifdef TARGET_PC
            /* OrreHook_TmUsedCount (step 3.7, infinite TMs; pc/orre_hooks.h): 0 keeps the TM, the path an HM takes.
             * NULL: one used, as on the GameCube. All platforms */
            *arg1 = ORRE_TM_USED_COUNT(arg0, 1);
#else
            *arg1 = 1;
#endif
        }
        return 0;
    }
    return 1;
}

/* fn_80022B3C - 0x80022B3C | size: 0x318 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 pokemonDataBiosGetVoice(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStartSE(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSsndGetStatus(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonInitPersistentJoutai(s32, s32);
#endif
extern void pokemonAddDpFormPokemonDpFilterId(s32, s32, s32);
extern u8 lbl_80266C30[];
extern u32 lbl_8047B8A0;
/* Pokémon data pointer of a party Pokémon, or 0 when the slot is empty or
 * invalid. */
/* RULE-EXCEPTION(user-approved): single-use inline helper, register-only evidence - see docs/RULE_EXCEPTIONS.md */
static inline void* gsTitlePokemonDataPtr(u32 target) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonBiosGetPokemonDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonDataBiosGetPtr(void);
#endif

    if (target == 0) {
        return 0;
    }
    if ((u8)pokemonCheckValid((u8*)PC_PTR(target)) == 0) {
        return 0;
    }
#ifdef TARGET_PC
    /* PC: the species id as an argument; on the GameCube pokemonBiosGetPokemonDataId's result stays in r3 */
    return pokemonDataBiosGetPtr(pokemonBiosGetPokemonDataId(PC_PTR(target)));
#else
    pokemonBiosGetPokemonDataId(target);
    return pokemonDataBiosGetPtr();
#endif
}

/* Starts the Pokémon's cry and returns its voice id (0 without data). */
/* RULE-EXCEPTION(user-approved): single-use inline helper, register-only evidence - see docs/RULE_EXCEPTIONS.md */
static inline s32 gsTitlePlayPokemonVoice(void* bios) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonDataBiosGetVoice(void*);
#endif
    s32 voice;

    if (bios == 0) {
        return 0;
    }
    voice = pokemonDataBiosGetVoice(bios);
    soundStartSE(voice);
    return voice;
}

/* Yields to other threads for the given number of frames. */
static inline void gsTitleWaitFrames(s32 frames) {
    s32 wait;

    for (wait = 0; wait < frames; wait += GSgetFrameStep()) {
        _threadSwitch();
    }
}

/* XD's name for this function: menuUseItemScriptCologn (same slot in menuUseItem.c, before menuUseItemCologn).
 * The std163 name is kept: the std number is its slot in the script engine's native table. */
s32 std163_useCologne(s32 arg0, s32 arg1) {
    typedef struct TitleItemMessage {
        u16 item;
        u32 useMessage;
        u32 dpMessage;
    } TitleItemMessage;
    typedef struct TitleItemMessageTable {
        TitleItemMessage entry[3];
    } TitleItemMessageTable;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetPokemonDataId(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* pokemonDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 pokemonDataBiosGetVoice(void*);
#endif
    s32 pokemon;
    s32 data;
    TitleItemMessageTable table;
    s32 i;
    s32 entryOffset;
    s32 voice;
    void* bios;
    s32 j;
    void* nickname;

    table = *(TitleItemMessageTable*)lbl_80266C30;
    itemUseGetTargetPokemon(arg1, &pokemon, &data);
    if ((u8)pokemonCheckValid((u8*)PC_PTR(pokemon)) == 0) {
        return 1;
    }

    for (i = 0; i < 3; i++) {
        if ((u16)arg0 == table.entry[i].item) {
            break;
        }
    }
    if (i >= 3) {
        return 1;
    }
    nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
    msgctrlSetValue(0x32, PC_PTR32(nickname));
    msgctrlSetValue(0x2D, (u32)arg0);
    entryOffset = i * sizeof(TitleItemMessage);
    winMsgOpen(2, *(u32*)((u8*)&table.entry[0].useMessage + entryOffset), 1, 0);
    winMsgClose(1);

    if (gamedatasaveGetStatus(0, 9) == 0) {
        for (j = 0; j < 2; j++) {
            GSinputMotorStart(1, 0, 0xFF, 0x15, 0);
            gsTitleWaitFrames(0x3C);
        }
        GSinputMotorStart(1, 0, 0xFF, 0x30, 0);
        gsTitleWaitFrames(0x60);
    }

    bios = gsTitlePokemonDataPtr(pokemon);
    voice = gsTitlePlayPokemonVoice(bios);
    while (GSsndGetStatus(voice) == 2) {
        _threadSwitch();
    }

    nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
    msgctrlSetValue(0x32, PC_PTR32(nickname));
    winMsgOpen(2, 0x3AF7, 1, 0);
    winMsgClose(1);
    if ((u8)pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) != 0) {
        pokemonInitPersistentJoutai(PC_PTR(pokemon), 0x3E);
        winMsgOpen(2, 0x4277, 1, 0);
        winMsgClose(1);
    }
    if (pokemonGetDp((void*)PC_PTR(pokemon)) > *(f32*)&lbl_8047B8A0) {
        pokemonAddDpFormPokemonDpFilterId(pokemon, (u16)arg0, 4);
        nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
        msgctrlSetValue(0x32, PC_PTR32(nickname));
        winMsgOpen(2, *(u32*)((u8*)&table.entry[0].dpMessage + entryOffset), 1, 0);
        winMsgClose(1);
    }
    return 1;
}
#endif

/* menuUseItemCologn - 0x80022E54 | size: 0x90 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fadeSet(f32, s32);
#endif
extern void std017_menuQueueAdd(void*, s32);
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80022E54)
/* NAME-T1 fn_80022E54 */ /* XD's menuUseItem.c, same slot (between ScriptCologn and Evolution); used by the three Scents */
s32 menuUseItemCologn(void* r3, u32* r4) {
    void* r29;
    u32* r30;
    s32 r31;
    u32 sc, sd;
    r29 = r3;
    r30 = r4;
    r31 = itemUseOpenPartyMenu((s32)PC_PTR32(r29), 1);
    if (r31 >= 0) {
        itemUseGetTargetPokemon(r31, (s32*)&sc, (s32*)&sd);
    }
    itemUseClosePartyMenu(r31);
    if (r31 >= 0) {
        std017_menuQueueAdd(r29, 0x5960010);
        *r30 = 1;
        return 2;
    }
    return 1;
}

/* menuUseItemEvolution - 0x80022EE4 | size: 0x184 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonEvolutionCheck(s32, s32, u16, void*, void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuOffScreenSetDisp(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonEvolutionAll(s32, s32, u16, void*, s32, s32, s32, s32);
#endif
/* NAME-T1 fn_80022EE4 */ /* XD's menuUseItem.c, same slot; the evolution stones (was itemFieldUseEvolutionStone) */
s32 menuUseItemEvolution(u32 arg0, u32* arg1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, s32);
#endif
    u8 text_buf[0x100];
    s32 slot;
    s32 effect;
    s32 sc;
    s32 sd;
    u16 sp8;
    u32 spC;

    slot = itemUseOpenPartyMenu((s32)arg0, 1);
    if (slot >= 0) {
        itemUseGetTargetPokemon(slot, (s32*)&sc, (s32*)&sd);
        if ((u8)pokemonIsDarkPokemon(PC_PTR(sc)) == 0) {
            effect = itemUse2Pokemon(text_buf, PC_PTR(sc), PC_PTR(sd), (u16)arg0, 0);
            if ((s16)effect <= 0) {
                winMsgOpen(2, 0x4261, 1, 0);
                winMsgClose(1);
            }
        } else {
            winMsgOpen(2, 0x424C, 1, 0);
            winMsgClose(1);
        }
    }

    itemUseClosePartyMenu(slot);

    if (slot >= 0 && (s16)effect > 0) {
        effect = pokemonEvolutionCheck(PC_PTR(sc), 1, (u16)arg0, &sp8, &spC);
#ifdef TARGET_PC
        /* OrreHook_PartyListHold (step 3.7, snatcherLeo's hand check 2026-11-20; pc/orre_hooks.h): the evolution scene is another
         * room, so a party list OrrePort kept open at itemUseClosePartyMenu above is closed now, at the same moment the
         * game closes it. NULL or 0: already closed above, as on the GameCube. All platforms */
        if (ORRE_PARTY_LIST_HOLD(ORRE_HOLD_LEAVE, slot)) {
            menuPokemonClose();
        }
#endif
        fadeSetDurationFirst(0.5f, 3);
        fadeCheck(1);
        menuOffScreenSetDisp(0);
        pokemonEvolutionAll(PC_PTR(sc), effect, sp8, &spC, 0, 1, 0, 0);
        menuOffScreenSetDisp(1);
        fadeSetDurationFirst(0.5f, 2);
        fadeCheck(1);
        *arg1 = 1;
        return 0;
    }

    return 1;
}
#endif

/* menuUseItemWPointUp - 0x80023068 | size: 0x20c */
extern s32 menuPokemonOpenWazaLight(s32);
extern void menuPokemonCloseWazaLight(void);
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80023068)
/* NAME-T1 fn_80023068 */ /* XD's menuUseItem.c, same slot; PP UP / PP MAX (was _itemUseHealStatus, a wrong name) */
s32 menuUseItemWPointUp(u32 arg0, u32* arg1) {
    typedef struct TitleBgmList {
        u16 entries[5];
    } TitleBgmList;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemUseFormatMessage(void*, s32, void*, s32, s32);
    extern u8 pokemonIsJoutaiDataId(s32, s32);
#endif
    s32 pokemon;
    s32 slot;
    TitleBgmList bgm;
    u8 name[0x84];
    u8 effects[0x108];
    s32 effect_count;
    s32 mode;
    s32 data;
    s32 blocked;
    s32 i;
    void* nickname;

    blocked = 0;
    do {
        slot = itemUseOpenPartyMenu((s32)arg0, 1);
        if (slot < 0) {
            break;
        }
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) != 0) {
            blocked = 1;
            break;
        }
        mode = menuPokemonOpenWazaLight((s8)slot);
        menuPokemonCloseWazaLight();
    } while (mode < 0);

    if (slot >= 0) {
        if (blocked != 0) {
            nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
            msgctrlSetValue(0x32, PC_PTR32(nickname));
            winMsgOpen(2, 0x424D, 1, 0);
            winMsgClose(1);
            effect_count = 0;
        } else {
            effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, mode);
            if ((s16)effect_count > 0) {
                bgm = *(TitleBgmList*)lbl_80266DB0;
                for (i = 0; i < 5; i++) {
                    if (arg0 == bgm.entries[i]) {
                        break;
                    }
                }
                soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                menuPokemonRefresh();
            }
            itemUseFormatMessage(name, 0x40, effects, effect_count, pokemon);
            msgctrlSetValue(0x4D, PC_PTR32(name));
            winMsgOpen(2, 0xE0, 1, 0);
            winMsgClose(1);
        }
    }

    itemUseClosePartyMenu(slot);
    if (slot >= 0 && (s16)effect_count > 0) {
        *arg1 = 1;
        return 0;
    }
    return 1;
}
#endif

/* cbForgetWazaSelect__FP7PokemonUsl - 0x80023274 | size: 0x7c */
extern s32 fn_80097B04(s32, s32);
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_80023274)
s32 cbForgetWazaSelect__FP7PokemonUsl(s32 r3, s32 r4) {
    s32 result;

    fadeSetDurationFirst(0.5f, 3);
    fadeCheck(1);
    result = fn_80097B04(r3, r4);
    if (result >= 4) {
        result = -1;
    }
    fadeSetDurationFirst(0.5f, 2);
    fadeCheck(1);
    return (s8)result;
}
#endif

/* menuUseItemListLVUp - 0x800232F0 | size: 0x470 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 pokemonGetStatus(u32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std130_soundUnk_801653cc(s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonBiosGetLevel(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpenLevelUpFiledStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgCloseLevelUpStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonSearchWazaDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonGetOboeWazaDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8011FC74(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80106D3C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801069FC(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80132A38(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8011F4F0(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8011F4A8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80105D48(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80105C68(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801236F8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80123B5C(void);
#endif
#ifndef TARGET_PC /* fn_802600E4 is evolutionWazaLearn (declared with its real type in this file) */
extern void fn_802600E4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80123D58(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_80128A64(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801C41C8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801C40F0(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_801096F8(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_8012805C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 cbForgetWazaSelect__FP7PokemonUsl(s32, s32);
#endif
#if !defined(GS_TITLE_SPLIT) || defined(GS_TITLE_RANGE_800232F0)
/* NAME-T1 fn_800232F0 */ /* XD's menuUseItem.c, right after cbForgetWazaSelect as in XD; RARE CANDY (was _itemUseRareCandy) */
s32 menuUseItemListLVUp(u32 wazaDataId, u32* result) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonBiosGetLevel(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 winMsgOpenLevelUpFiledStatus(void*, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgCloseLevelUpStatus(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 pokemonSearchWazaDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetOboeWazaDataId();
#endif
    extern s32 evolutionWazaLearn();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonWazaCreate();
#endif

    typedef struct TitleLevelUpStatus {
        u8 mode;
        s16 hp;
        s16 attack;
        s16 defense;
        s16 speed;
        s16 spAttack;
        s16 spDefense;
    } TitleLevelUpStatus;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
    s32 pokemon;
    s32 data;
    u16 evolveTo;
    u32 evolveData;
    u8 learnSlot;
    u8 index;
    s16 oldHp;
    TitleLevelUpStatus delta;
    s16 oldAttack;
    s16 oldDefense;
    s16 oldSpAttack;
    s32 target;
    TitleLevelUpStatus after;
    u8 effects[0x100];
    s16 oldSpDefense;
    s16 oldSpeed;
    s32 slot;
    s32 effectCount;
    u32 move;
    u32 level;
    u32 evolution;
    void* nickname;

    index = 0;
    slot = itemUseOpenPartyMenu((s32)wazaDataId, 1);
    if (slot >= 0) {
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if (pokemonIsDarkPokemon(PC_PTR(pokemon)) == 0) {
            oldHp = pokemonGetStatus(PC_PTR(target = pokemon), 0, POKEMON_STATUS_MAX_HP, 0);
            oldAttack = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_ATTACK, 0);
            oldDefense = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_DEFENSE, 0);
            oldSpAttack = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SP_ATTACK, 0);
            oldSpDefense = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SP_DEFENSE, 0);
            oldSpeed = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SPEED, 0);
            effectCount = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), wazaDataId, 0);
            if ((s16)effectCount > 0) {
                menuPokemonRefresh();
            } else {
                effectCount = 0;
            }
        } else {
            effectCount = -1;
        }

        switch ((s16)effectCount) {
        case 0:
            winMsgOpen(2, 0x4261, 1, 0);
            winMsgClose(1);
            break;
        case -1:
            winMsgOpen(2, 0x424C, 1, 0);
            winMsgClose(1);
            break;
        default:
            std130_soundUnk_801653cc(0x4CA, 0, 0xFF);
            nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
            msgctrlSetValue(0x32, PC_PTR32(nickname));
            level = pokemonBiosGetLevel(PC_PTR(pokemon));
            msgctrlSetValue(0x2F, (u32)((u8)level));
            winMsgOpen(2, 0x423E, 1, 0);
            winMsgClose(1);

            target = pokemon;
            after.hp = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_MAX_HP, 0);
            after.attack = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_ATTACK, 0);
            after.defense = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_DEFENSE, 0);
            after.spAttack = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SP_ATTACK, 0);
            after.spDefense = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SP_DEFENSE, 0);
            after.speed = pokemonGetStatus(PC_PTR(target), 0, POKEMON_STATUS_SPEED, 0);
            delta.hp = after.hp - oldHp;
            delta.attack = after.attack - oldAttack;
            delta.defense = after.defense - oldDefense;
            delta.spAttack = after.spAttack - oldSpAttack;
            delta.spDefense = after.spDefense - oldSpDefense;
            delta.speed = after.speed - oldSpeed;
            delta.mode = 1;
            winMsgOpenLevelUpFiledStatus(PC_PTR32_IMPLICIT(&delta), 1);
            after.mode = 0;
            winMsgOpenLevelUpFiledStatus(PC_PTR32_IMPLICIT(&after), 1);
            winMsgCloseLevelUpStatus(1);

            level = pokemonBiosGetLevel(PC_PTR(pokemon));
            index = 0;
            while ((u16)(move = pokemonGetOboeWazaDataId(PC_PTR(pokemon), level, &index)) != 0) {
                if (pokemonSearchWazaDataId(PC_PTR(pokemon), move) == -1 &&
                    evolutionWazaLearn(pokemon, move, &learnSlot, 1,
                                       cbForgetWazaSelect__FP7PokemonUsl, 0) != 0) {
                    pokemonWazaCreate(PC_PTR(pokemon), learnSlot, move);
                }
                index++;
            }
            evolution = pokemonEvolutionCheck(PC_PTR(pokemon), 0, 0, &evolveTo, &evolveData);
            break;
        }
    }

    itemUseClosePartyMenu(slot);
    if (slot >= 0 && (s16)effectCount > 0) {
        if ((u16)evolution != 0) {
#ifdef TARGET_PC
            /* OrreHook_PartyListHold: as in menuUseItemEvolution, the held party list is closed before the scene */
            if (ORRE_PARTY_LIST_HOLD(ORRE_HOLD_LEAVE, slot)) {
                menuPokemonClose();
            }
#endif
            fadeSetDurationFirst(0.5f, 3);
            fadeCheck(1);
            menuOffScreenSetDisp(0);
            pokemonEvolutionAll(PC_PTR(pokemon), evolution, evolveTo, &evolveData, 0, 1, 1, 0);
            menuOffScreenSetDisp(1);
            fadeSetDurationFirst(0.5f, 2);
            fadeCheck(1);
        }
        *result = 1;
        return 0;
    }
    return 1;
}

/* menuUseItemAlldeathRcv - 0x80023760 | size: 0x208 */
/* NAME-T1 fn_80023760 */ /* XD's menuUseItem.c, same slot; SACRED ASH ("all fainted: recover") */
s32 menuUseItemAlldeathRcv(u32 arg0, u32* arg1) {
    typedef struct TitleBgmList {
        u16 entries[5];
    } TitleBgmList;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemUseFormatMessage(void*, s32, void*, s32, s32);
    extern u8 pokemonIsJoutaiDataId(s32, s32);
#endif
    s32 pokemon;
    u8* text;
    s32 slot;
    u8 name[0x84];
    u8 effects[0x108];
    u16 total;
    TitleBgmList bgm;
    s32 effect_count;
    s32 i;
    s32 result;
    s32 data;

    total = 0;
    itemUseOpenPartyMenu((s32)arg0, 0);
    text = name;
    for (slot = 0; slot < 6; slot++) {
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if ((u8)pokemonCheckValid((u8*)PC_PTR(pokemon)) == 0) {
            continue;
        }
        if ((s32)pokemonGetStatus(PC_PTR(pokemon), 0, POKEMON_STATUS_HP, 0) > 0) {
            continue;
        }
        if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) != 0) {
            continue;
        }
        effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, 0);
        if ((s16)effect_count <= 0) {
            continue;
        }
        bgm = *(TitleBgmList*)lbl_80266DB0;
        for (i = 0; i < 5; i++) {
            if (arg0 == bgm.entries[i]) {
                break;
            }
        }
        soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
        menuPokemonRefresh();
        itemUseFormatMessage(name, 0x40, effects, effect_count, pokemon);
        msgctrlSetValue(0x4D, PC_PTR32(text));
        winMsgOpen(2, 0xE0, 1, 0);
        winMsgClose(1);
        total += effect_count;
    }

    if (total == 0) {
        winMsgOpen(2, 0x4261, 1, 0);
        winMsgClose(1);
        result = -1;
    } else {
        result = 1;
    }
    itemUseClosePartyMenu(-1);
    if (result < 0) {
        return 1;
    }
    *arg1 = 1;
    return 0;
}

/* menuUseItemPPRecovery - 0x80023968 | size: 0x234 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* itemDataBiosGetItemEffectParam(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* itemParamGetPtr(void*);
#endif
extern u8 itemParamGetPPSelectFlag(void*);
/* RULE-EXCEPTION(user-approved): arg0 is s32 so the real s32 itemUseOpenPartyMenu call keeps retail's register allocation; the BGM-table compare goes through (u32) to keep retail's unsigned cmplw - see docs/RULE_EXCEPTIONS.md */
/* NAME-T1 fn_80023968 */ /* XD's menuUseItem.c, same slot; Ether / Max Ether / Elixir / Leppa Berry (was _itemUsePPRestore) */
s32 menuUseItemPPRecovery(s32 arg0, u32* arg1) {
    typedef struct TitleBgmList {
        u16 entries[5];
    } TitleBgmList;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemUseFormatMessage(void*, s32, void*, s32, s32);
    extern u8 pokemonIsJoutaiDataId(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* itemDataBiosGetPtr(u16);
#endif
    s32 pokemon;
    s32 slot;
    TitleBgmList bgm;
    u8 name[0x84];
    u8 effects[0x108];
    s32 effect_count;
    s32 mode;
    s32 data;
    s32 blocked;
    s32 i;
    void* nickname;

    blocked = 0;
    do {
        slot = itemUseOpenPartyMenu(arg0, 1);
        if (slot < 0) {
            break;
        }
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) != 0) {
            blocked = 1;
            break;
        }
        if (itemParamGetPPSelectFlag(itemParamGetPtr(itemDataBiosGetItemEffectParam(
                itemDataBiosGetPtr(arg0))))) {
            mode = menuPokemonOpenWazaLight((s8)slot);
            menuPokemonCloseWazaLight();
        } else {
            mode = 0;
        }
    } while (mode < 0);

    if (slot >= 0) {
        if (blocked != 0) {
            nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
            msgctrlSetValue(0x32, PC_PTR32(nickname));
            winMsgOpen(2, 0x424D, 1, 0);
            winMsgClose(1);
            effect_count = 0;
        } else {
            effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, mode);
            if ((s16)effect_count > 0) {
                bgm = *(TitleBgmList*)lbl_80266DB0;
                for (i = 0; i < 5; i++) {
                    if ((u32)arg0 == bgm.entries[i]) {
                        break;
                    }
                }
                soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                menuPokemonRefresh();
            }
            itemUseFormatMessage(name, 0x40, effects, effect_count, pokemon);
            msgctrlSetValue(0x4D, PC_PTR32(name));
            winMsgOpen(2, 0xE0, 1, 0);
            winMsgClose(1);
        }
    }

    itemUseClosePartyMenu(slot);
    if (slot >= 0 && (s16)effect_count > 0) {
        *arg1 = 1;
        return 0;
    }
    return 1;
}

/* menuUseItemRecovery - 0x80023B9C | size: 0x20c */
/* NAME-T1 fn_80023B9C */ /* XD's menuUseItem.c, same slot; Potions, status heals, Revives, vitamins (was itemFieldUseStatBoost) */
s32 menuUseItemRecovery(u32 arg0, u32* arg1) {
    typedef struct TitleBgmList {
        u16 entries[5];
    } TitleBgmList;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 itemUse2Pokemon(void*, s32, s32, u16, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void itemUseFormatMessage(void*, s32, void*, s32, s32);
    extern u8 pokemonIsJoutaiDataId(s32, s32);
#endif
    s32 pokemon;
    s32 data;
    TitleBgmList bgm;
    u8 effects[0x100];
    u8 name[0x84];
    s32 slot;
    s32 effect_count;
    s32 i;
    void* nickname;

    slot = itemUseOpenPartyMenu((s32)arg0, 1);
    if (slot >= 0) {
        itemUseGetTargetPokemon(slot, &pokemon, &data);
        if (pokemonIsJoutaiDataId(PC_PTR(pokemon), 0x3E) == 0) {
            effect_count = itemUse2Pokemon(effects, PC_PTR(pokemon), PC_PTR(data), arg0, 0);
            if ((s16)effect_count > 0) {
                bgm = *(TitleBgmList*)lbl_80266DB0;
                for (i = 0; i < 5; i++) {
                    if (arg0 == bgm.entries[i]) {
                        break;
                    }
                }
                soundStart(i < 5 ? 0x466 : 0x465, 0, 0xFF, 0);
                menuPokemonRefresh();
            }
            itemUseFormatMessage(name, 0x40, effects, effect_count, pokemon);
            msgctrlSetValue(0x4D, PC_PTR32(name));
            winMsgOpen(2, 0xE0, 1, 0);
            winMsgClose(1);
        } else {
            nickname = pokemonBiosGetNicknamePtr(PC_PTR(pokemon));
            msgctrlSetValue(0x32, PC_PTR32(nickname));
            winMsgOpen(2, 0x424D, 1, 0);
            winMsgClose(1);
            effect_count = 0;
        }
    }
    itemUseClosePartyMenu(slot);
    if (slot >= 0 && (s16)effect_count > 0) {
        switch (arg0) {
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2A:
        case 0x2B:
            *arg1 = 0;
            break;
        default:
            *arg1 = 1;
            break;
        }
        return 0;
    }
    return 1;
}
#endif
