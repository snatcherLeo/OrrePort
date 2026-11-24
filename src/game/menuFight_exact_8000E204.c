/**
 * @file menuFight_exact_8000E204.c
 * @brief menuFightDrawTargetCursor and menuFightCtrlTarget, 0x8000E204 - 0x8000E290.
 *
 * Function-boundary carve of the menuFight TU (see menuFight.c). GC/1.3
 * -O4,p with the TU's unit-wide -opt nopeephole, no pragmas.
 */
#include "dolphin/types.h"

/* =========================================================================
 * External declarations
 * ========================================================================= */

/* renamed symbols referenced by asm incs (symbolmap port) */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 _menuFightIsUse__FP16MENU_WAZA_STATUSUs();
#endif
extern void* menuSubCalcColor(void*, void*);

/* NPC/People system */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winSpriteSetDisp(void* npc, s32 direction);  /* Set NPC facing */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* menuItemBiosGetPtr(s16 npcId);                  /* Lookup NPC data by ID */
#endif

/* Text/dialog system */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  windowDrawSprite();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winSeqSetMenu(void* ctx, s32 state);       /* Set dialog state */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  fn_801081F8(void* ctx, s32 msgId, s32 flags); /* Display message */
#endif

/* Rendering */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   GSmsgGetRect();                           /* Get model dimensions */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmsgPrintColor();
#endif

/* Map/warp */
extern u8    fightFloorCheckFightActionFightOutPokemonIrekaeSelect(s32 p1, void* warpId, void* outDest);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightOutPokemonGetNicknamePtr(void* mapData);              /* Get map name string */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winMsgOpen(s32 slot, s32 msgId, s32 p3, s32 p4);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  winMsgClose(s32 slot);                   /* Close message box */
#endif

/* Input/frame */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8    fn_801F18DC(s32 controller);             /* Check input ready */
#endif
extern u8    fightFloorIsUseFightTimerCommand(s32 controller);             /* Check button pressed */
extern u8    fightTimerCommandIsOver(void);                       /* Check A button */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16   std103_battleGetResult(void);                       /* Get input state */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  _threadSwitch(void);                       /* Frame advance */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   GSinputButtonsChanged(s32 slot);                   /* Get render flags */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32   fn_800F7BC4(s32 slot);                   /* Get VSync flags */
#endif

/* Battle bridge */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuClose();
#endif

/* menuFightDrawTargetCursor - 0x8000E204 | size: 0x88 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuItemBiosGetSelectFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuGetCursorItemID(void);
#endif
void menuFightDrawTargetCursor(u8* arg1, u8* arg2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 menuItemBiosGetSelectFlag(s16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32 menuGetCursorItemID(u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winSpriteSetDisp(u8* a, u32 b);
#endif
    if ((u8)menuItemBiosGetSelectFlag(*(s16*)(arg2 + 6)) != 0) {
        if (*(s16*)(arg2 + 6) == menuGetCursorItemID(*(u32*)(arg1 + 4))) {
            winSpriteSetDisp(arg2, 1);
        } else {
            winSpriteSetDisp(arg2, 0);
        }
    } else {
        winSpriteSetDisp(arg2, 0);
    }
}

/* menuFightCtrlTarget - 0x8000E28C | size: 0x4 */
void menuFightCtrlTarget(void) {}
