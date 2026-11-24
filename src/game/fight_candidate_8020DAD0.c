/**
 * @file fight_candidate_8020DAD0.c
 * @brief fn_8020DAD0 carve (fight encount start), 0x8020DAD0 - 0x8020DD44,
 *        with its literal pool (.sdata2 0x8047E528 - 0x8047E530), linked.
 *
 * Standalone copy of the fight.c body. Its two fade literals (1.0f, 0.5f)
 * are the only users of 0x8047E528/0x8047E52C, so the carve owns them;
 * fadeSet takes (f32, u32) as in pokemon_evolution.h, so 0.5f stays single.
 */
#include "dolphin/types.h"

typedef struct FightKindData FightKindData;

u32 fightStartEncount(u32 p1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std092_getCurrentRoomID();
#endif
    extern void std098_mapWarpToMenuRoom();
    extern void floorStartSoundScript();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript();
#endif
    extern void floorSetPrevFloorID();
    extern void floorReturnToSavedRoom();
    extern void std162_fieldClearPending();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void heroDecPokedoru();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 heroGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
    extern void scriptSoundStop();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void std104_soundSetBackgroundMusic();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void soundStartRaw();
#endif
    extern void fn_8018DA88();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 std081_flagCheckTrue();
#endif
    extern void std079_flagSetTrue();
    extern void std078_flagSetFalse();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fadeCheck();
#endif
#ifdef TARGET_PC /* the callee's real order and types (effect/fade.c); the GameCube-order declaration below converted
                  * every argument of fadeSetEXFloatsFirst's reordered call to the wrong type on the PC: the battle wipe
                  * got type 0xC00C and a NULL init, and jumped to address 0 at Willie's battle (2026-10-17) */
    extern void* fadeSetEX(u16 type, void (*init)(void), u8 snapshot, f32 duration, f32 hookDuration);
#else
    extern void fadeSetEX(f32, f32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fadeSet(f32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_801D0AFC();
#endif
    extern void mailMainReceiveTerminate();
    extern void fightSetEncountId();
    extern void battleSetResult();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 std103_battleGetResult();
#endif
    extern void fightResetSoundFlags();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightFloorIsGcHeroWin();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
    extern u32 fightTrainerDataBiosGetNameMsgId();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerDataBiosGetPtr();
#endif
    extern u16 fightEncountDataBiosGetWipeEffectSndID();
    extern f32 fightEncountDataBiosGetWipeEffectTime();
    extern u32 fightEncountDataBiosGetWipeSnapshotUse();
    extern u32 fightEncountDataBiosGetWipeFunction();
    extern u32 fightEncountWipeDataBiosGetPtr();
    extern u8 fightEncountDataBiosGetZenmetuFlag();
    extern u32 fightEncountDataBiosGetWipeId();
    extern u32 fightEncountDataBiosGetFightTrainerDataId(); /* u16 id; u32 here so its r3 passes on unchanged, as in the assembly */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightEncountDataBiosGetFightFloorDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightEncountDataBiosGetFightKind();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightEncountDataBiosGetPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightKindDataBiosGetPokemonStatusMenuSubbarFlag(FightKindData* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern FightKindData* fightKindDataBiosGetPtr(u16 index);
#endif
    extern u16 charNameBiosSearchIndex();
    extern u16 charNameBiosGetHearFlag();

    u32 uVar1;
    u32 wipeData;
    u32 msgValue;
    u32 uVar2;
    u16 uVar7;
    u8 uVar9;
    FightKindData* iVar3;
    u32 trainerData;
    u8 cVar10;
    u16 sVar8;
    u32 uVar4;
    u32 uVar6;

#ifdef TARGET_PC
    /* PC test log (the replay_report.py, like fieldStartEncount's in gs_range_801C766C.c): trainer battles start here */
    { extern void OSReport(const char*, ...); OSReport("[battle] start %u\n", (unsigned)(p1 & 0xffff)); }
#endif
    if ((p1 & 0xffff) == 0) {
        uVar1 = 0;
    } else {
        uVar1 = PC_PTR32_IMPLICIT(fightEncountDataBiosGetPtr(p1));
        battleSetResult(0);
        std079_flagSetTrue(0x9b0);
        fightSetEncountId(p1);
        uVar2 = std092_getCurrentRoomID();
        fightFloorSetStatus(0, 0, 0x4a, 0, uVar2);
        uVar4 = fightEncountDataBiosGetFightFloorDataId(uVar1);
        uVar7 = fightFloorGetStatus(0, uVar4, 2, 0);
        mailMainReceiveTerminate();
        uVar9 = fightEncountDataBiosGetFightKind(uVar1);
        iVar3 = fightKindDataBiosGetPtr(uVar9);
        if ((iVar3 != 0) && (cVar10 = fightKindDataBiosGetPokemonStatusMenuSubbarFlag(iVar3), cVar10 != 0)) {

            trainerData = PC_PTR32_IMPLICIT(fightTrainerDataBiosGetPtr(fightEncountDataBiosGetFightTrainerDataId(uVar1, 1))); /* r3 = the trainer data id (assembly) */
            if (trainerData != 0) {
                msgValue = fightTrainerDataBiosGetNameMsgId(trainerData);
                sVar8 = charNameBiosSearchIndex(msgValue);
#ifdef TARGET_PC
                /* the search index is still in r3 on the GameCube (assembly); passing it changes MWCC's code, so the
                   GameCube keeps the original text */
                if ((sVar8 != 0) && (sVar8 = charNameBiosGetHearFlag(sVar8), sVar8 != 0)) {
#else
                if ((sVar8 != 0) && (sVar8 = charNameBiosGetHearFlag(), sVar8 != 0)) {
#endif
                    std078_flagSetFalse(sVar8);
                }
                msgctrlSetValue(0x59, msgValue);
            }
        }
        std104_soundSetBackgroundMusic(1, 1000, 0xff);
        scriptSoundStop(1000);
        wipeData = fightEncountWipeDataBiosGetPtr(fightEncountDataBiosGetWipeId(uVar1));
        fadeSetEXFloatsFirst(1.0f, fightEncountDataBiosGetWipeEffectTime(wipeData), 9,
                  fightEncountDataBiosGetWipeFunction(wipeData),
                  fightEncountDataBiosGetWipeSnapshotUse(wipeData));
        sVar8 = fightEncountDataBiosGetWipeEffectSndID(wipeData);
        if (sVar8 != 0) {
            soundStartRaw(sVar8, 0, 0);
        }
        fightResetSoundFlags();
        std098_mapWarpToMenuRoom(uVar7);
        floorSetFadeScript(0, 0);
        _threadSwitch();
        floorSetPrevFloorID(uVar7);
        cVar10 = fightEncountDataBiosGetZenmetuFlag(uVar1);
        if (cVar10 != 0) {
            uVar1 = std103_battleGetResult();
            cVar10 = fightFloorIsGcHeroWin(0, uVar1);
            if (cVar10 == 0) {
                fightSetEncountId(0);
                std079_flagSetTrue(0xe05);
                uVar6 = heroGetStatus(0, 0xc, 0);
                heroDecPokedoru(0, (s32)uVar6 / 2);
                fn_801D0AFC(1);
                fn_8018DA88();
                floorReturnToSavedRoom();
                floorSetFadeScript(0, 0x5960008);
                _threadSwitch();
                uVar1 = std103_battleGetResult();
                return uVar1;
            }
        }
        std078_flagSetFalse(0x9b0);
        floorStartSoundScript();
        std162_fieldClearPending();
        cVar10 = std081_flagCheckTrue(0xe05);
        if (cVar10 == 0) {
            fadeSetDurationFirst(0.5f, 2);
            fadeCheck(1);
        }
        fightSetEncountId(0);
        uVar1 = std103_battleGetResult();
    }
    return uVar1;
}
