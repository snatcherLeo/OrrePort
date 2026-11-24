/** Exact menuFight suffix, 0x80010C98 - 0x80011288. */
#include "dolphin/types.h"

extern u8 fightFloorCheckFightActionFightOutPokemonIrekaeSelect(s32, void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightOutPokemonGetNicknamePtr(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgOpen(s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void winMsgClose(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_801F18DC(s32);
#endif
extern u8 fightFloorIsUseFightTimerCommand(s32);
extern u8 fightTimerCommandIsOver(void);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 std103_battleGetResult(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSinputButtonsChanged(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_800F7BC4(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightFloorGetGcHeroFightTrainerPtr(s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightTrainerGetValidFightPokemonPtr(u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fightOutPokemonGetTokuseiDataId(void*);
#endif
extern u8 fightTrainerCheckCanIrekaeFightPokemon(u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 fightPokemonGetNicknamePtr(u32);
#endif
extern void GSlogWrite(const char*, ...);
extern u8 lbl_80266788[];
extern u8 lbl_802E4B78[];
extern u32 gbaCommandRequestAction(u32, u32, u32, u32);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 menuIsCheck();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void menuCloseCustom();
#endif

u32 menuPokemonCheckPokemonChange(void* npc, u32 warpId, u32 variant)
{
#define WAIT_FOR_DIALOG(waitLabel, checkLabel, haveLabel, doneLabel) \
    goto checkLabel; \
waitLabel: \
    advance = fn_801F18DC(0); \
    if (advance != 0) { \
        if ((fightFloorIsUseFightTimerCommand(0) == 1) && (fightTimerCommandIsOver() == 1)) { \
            advance = 1; \
            goto haveLabel; \
        } else if (std103_battleGetResult() == 1) { \
            advance = 1; \
            goto haveLabel; \
        } \
    } \
    advance = 0; \
haveLabel: \
    if (advance != 0) { \
        goto doneLabel; \
    } \
    _threadSwitch(); \
checkLabel: \
    inputFlags = GSinputButtonsChanged(1); \
    maskedFlags = fn_800F7BC4(1); \
    maskedFlags &= inputFlags; \
    if ((maskedFlags & 0x300) == 0) { \
        goto waitLabel; \
    } \
doneLabel:

    void* linkedNpc;
    u32 inputFlags;
    u32 maskedFlags;
    u8 relation;
    u8 advance;
    u8 kind;

    relation = fightFloorCheckFightActionFightOutPokemonIrekaeSelect(0, npc, &linkedNpc);
    if (relation == 1) {
        msgctrlSetValue(0xD, (s32)PC_PTR32(fightOutPokemonGetNicknamePtr(npc)));
        winMsgOpen(1, 0x76FB, 1, 0);
        WAIT_FOR_DIALOG(waitRelationOne, checkRelationOne, haveRelationOne, doneRelationOne);
        winMsgClose(1);
        return 0;
    }
    if (relation == 2) {
        fightFloorSetStatus(0, 0, 0x57, 0, (u16)fightOutPokemonGetTokuseiDataId(linkedNpc));
        msgctrlSetValue(0xD, (s32)PC_PTR32(fightOutPokemonGetNicknamePtr(linkedNpc)));
        msgctrlSetValue(0xE, (s32)PC_PTR32(fightOutPokemonGetNicknamePtr(npc)));
        winMsgOpen(1, 0x761F, 1, 0);
        WAIT_FOR_DIALOG(waitRelationTwo, checkRelationTwo, haveRelationTwo, doneRelationTwo);
        winMsgClose(1);
        return 0;
    }
    if (warpId == 0) {
        warpId = fightFloorGetGcHeroFightTrainerPtr(0);
    }
    if (warpId == 0) {
        return 0;
    }
    variant = PC_PTR32_IMPLICIT(fightTrainerGetValidFightPokemonPtr(warpId, (u16)variant));
    if (variant == 0) {
        return 0;
    }
    kind = fightTrainerCheckCanIrekaeFightPokemon(warpId, variant);
    if (kind == 1) {
        msgctrlSetValue(0xD, fightPokemonGetNicknamePtr(PC_PTR(variant)));
        winMsgOpen(1, 0x76FE, 1, 0);
        WAIT_FOR_DIALOG(waitKindOne, checkKindOne, haveKindOne, doneKindOne);
        winMsgClose(1);
        return 0;
    }
    if (kind == 2) {
        msgctrlSetValue(0xD, fightPokemonGetNicknamePtr(PC_PTR(variant)));
        winMsgOpen(1, 0x76FC, 1, 0);
        WAIT_FOR_DIALOG(waitKindTwo, checkKindTwo, haveKindTwo, doneKindTwo);
        winMsgClose(1);
        return 0;
    }
    if (kind == 3) {
        msgctrlSetValue(0xD, fightPokemonGetNicknamePtr(PC_PTR(variant)));
        winMsgOpen(1, 0x76FD, 1, 0);
        WAIT_FOR_DIALOG(waitKindThree, checkKindThree, haveKindThree, doneKindThree);
        winMsgClose(1);
        return 0;
    }
    if (kind == 0) {
        return 1;
    }
    GSlogWrite((const char*)lbl_80266788, (const char*)lbl_802E4B78);
#undef WAIT_FOR_DIALOG
    return 0;
}

#ifdef TARGET_PC
/* PC: void on the GameCube; fight_menu.c reads gbaCommandRequestAction's result from r3, so the PC returns it (2026-10-29;
 * port-gaps 34's lead) */
u32 menuFightOpenGBAMain(u32 a, u32 b, u32 c)
{
    return gbaCommandRequestAction(a, b, c, 0);
}

u32 menuFightOpenGBAIrekae(u32 a, u32 b, u32 c)
{
    return gbaCommandRequestAction(a, b, c, 1);
}
#else
void menuFightOpenGBAMain(u32 a, u32 b, u32 c)
{
    gbaCommandRequestAction(a, b, c, 0);
}

void menuFightOpenGBAIrekae(u32 a, u32 b, u32 c)
{
    gbaCommandRequestAction(a, b, c, 1);
}
#endif

u32 menuFightCloseTarget(void* obj)
{
    if ((u8)menuIsCheck(0xff) != 0) {
        menuCloseCustom(0xff, 0, PC_PTR32_IMPLICIT(obj));
    }
    if ((u8)menuIsCheck(0x104) != 0) {
        menuCloseCustom(0x104, 0, PC_PTR32_IMPLICIT(obj));
    }
    menuIsCheck(0x100);
    return 0;
}
