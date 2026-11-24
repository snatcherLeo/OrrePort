#include "dolphin/types.h"
#include "game/constants/abilities.h"
#include "game/constants/conditions.h"

extern u8 lbl_80478D78[8];
extern u32 ServerStatusFlag;
extern u8 lbl_8047B628;
extern void* lbl_8047B62C;
extern u8 lbl_80379945[];
extern u8 lbl_8037994A[];

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wsRunScript(void* context, void* script, u8 preserveState);
#endif

void _wsCheckSynchronizeAttacker(void* context) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
    u16 value = fightOutPokemonGetTokuseiDataId(context);
    u8 state;

    if ((u8)fightOutPokemonCheckFightOut(context) != 0 && value == ABILITY_SYNCHRONIZE &&
        (ServerStatusFlag & 0x4000) != 0) {
        state = lbl_8047B628;
        ServerStatusFlag = ServerStatusFlag & ~0x4000;
        state = state & 0x3F;
        lbl_8047B628 = state;
        if (state == 6) {
            lbl_8047B628 = 2;
        }
        lbl_80478D78[3] = lbl_8047B628;
        fightFloorSetStatus(0, 0, 0x4B, 0, PC_PTR32(context));
        ServerStatusFlag = ServerStatusFlag | 0x2000;
        wsRunScript(lbl_8047B62C, (void*)&lbl_80379945, 0);
    }
}

void fn_8022D2CC(void* attacker, void* context) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u16 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
    u16 value = fightOutPokemonGetTokuseiDataId(context);
    u8 state;

    if ((u8)fightOutPokemonCheckFightOut(context) != 0 && value == ABILITY_SYNCHRONIZE &&
        (ServerStatusFlag & 0x4000) != 0) {
        state = lbl_8047B628;
        ServerStatusFlag = ServerStatusFlag & ~0x4000;
        state = state & 0x3F;
        lbl_8047B628 = state;
        if (state == 6) {
            lbl_8047B628 = 2;
        }
        lbl_80478D78[3] = lbl_8047B628 + 0x40;
        fightFloorSetStatus(0, 0, 0x4B, 0, PC_PTR32(context));
        ServerStatusFlag = ServerStatusFlag | 0x2000;
        wsRunScript(lbl_8047B62C, (void*)&lbl_80379945, 0);
    }
}

u32 _fightTokuseiCureJoutaiSub(u32 pokemon) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonCheckFightOut();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetTokuseiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightOutPokemonGetPokemonPtr();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsPokemonJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 joutaiDataBiosGetName();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonWriteJoutaiDataId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonInitJoutai();
#endif
    extern void fightOutPokemonResetSeqStatus();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 fightOutPokemonIsUseHensinBuff();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightOutPokemonSetHensinPokemonStatusId();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fightFloorSetStatus();
#endif
    u8 result;
    u32 ability;
    u32 pokemonPtr;

    ability = pokemon;
    while (1) {
        if (fightOutPokemonCheckFightOut(PC_PTR(ability)) == 0) {
            return 1;
        }

        pokemon = fightOutPokemonGetTokuseiDataId(PC_PTR(ability));
        pokemonPtr = PC_PTR32_IMPLICIT(fightOutPokemonGetPokemonPtr(ability));
        result = 0;

        switch ((u16)pokemon) {
        case 0x11:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_POISON)) == 1 || fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_BAD_POISON)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(3))));
                result = 1;
            }
            break;
        case 0x14:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_CONFUSION)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(9))));
                result = 2;
            }
            break;
        case 7:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_PARALYSIS)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(5))));
                result = 1;
            }
            break;
        case 0xF:
        case 0x48:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_SLEEP)) == 1) {
                fightOutPokemonWriteJoutaiDataId(PC_PTR(ability), PC_PTR(0x17));
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(8))));
                result = 1;
            }
            break;
        case 0x29:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_BURN)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(6))));
                result = 1;
            }
            break;
        case 0x28:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_FREEZE)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(7))));
                result = 1;
            }
            break;
        case 0xC:
            if (fightOutPokemonIsPokemonJoutaiDataId(PC_PTR(ability), PC_PTR(10)) == 1) {
                msgctrlSetValue(0xD, PC_PTR32_IMPLICIT(GSmsgGetGSchar(joutaiDataBiosGetName(10))));
                result = 3;
            }
            break;
        }

        if (result == 0) {
            goto return_one;
        }

        switch (result) {
        case 1:
            pokemonInitJoutai(PC_PTR(pokemonPtr));
            fightOutPokemonResetSeqStatus(ability, 0);
            goto apply_hensin;
        case 2:
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ability), PC_PTR(CONDITION_CONFUSION));
            goto apply_hensin;
        case 3:
            fightOutPokemonWriteJoutaiDataId(PC_PTR(ability), PC_PTR(10));
        default:
        apply_hensin:
            if (fightOutPokemonIsUseHensinBuff(PC_PTR(ability)) == 1) {
                fightOutPokemonSetHensinPokemonStatusId(PC_PTR(ability), 0x7C, 0, 0);
            }
            fightFloorSetStatus(0, 0, 0x4B, 0, ability);
            break;
        }

        wsRunScript(lbl_8047B62C, lbl_8037994A, 0);
    }

return_one:
    return 1;
}
