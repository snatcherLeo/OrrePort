/**
 * @file menuFight_exact_8000DE24.c
 * @brief menuFightButtonSecretKousan, 0x8000DE24 - 0x8000DEC4.
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

/* menuFightButtonSecretKousan - 0x8000DE24 | size: 0xa0 */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuButtonNormal(void);
#endif
void menuFightButtonSecretKousan(u8* ptr) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuButtonNormal(u8* a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fn_801F18DC(s32 a);
#endif
    extern u8 fightFloorIsUseFightTimerCommand(s32 a);
    extern u8 fightTimerCommandIsOver(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 std103_battleGetResult(void);
#endif
    u8 flag;
    menuButtonNormal(ptr);
    if (!(u8)fn_801F18DC(0)) goto _zero;
    if ((u8)fightFloorIsUseFightTimerCommand(0) == 1 && (u8)fightTimerCommandIsOver() == 1) { flag = 1; goto _check; }
    if ((u16)std103_battleGetResult() == 1) { flag = 1; goto _check; }
    _zero:
    flag = 0;
    _check:
    if (flag) {
        ptr[0x98] = 1;
        ptr[0x99] = 1;
    }
}
