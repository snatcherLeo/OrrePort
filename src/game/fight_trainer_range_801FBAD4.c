#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceSysStopWaza(void* ptr, u16 field, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 wazaSequenceSysIsPlayingWaza(void* ptr, u16 field, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceSysStartWaza(void* ptr, u16 field, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void wazaSequenceOwnerLoadWaza(void* ptr, u16 field, u32 size, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTrainerGetStatus(void* ctx, u32 slot, u32 field, u32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemGetStatus(u32 a, u32 b, u32 c, u32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightOutPokemonGetPokemonPtr(void* ctx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus(void* pokemon, u32 a, u32 b, u32 c);
#endif

void fightTrainerHokakuThrowEffect(void* context, u32 param, u8 mode)
{
    void* data;
    u32 animId;

    if ((data = PC_PTR(fightTrainerGetStatus(context, 0, 0x4C, 0))) == NULL) {
        return;
    }
    animId = itemGetStatus(0, param, 0x12, 0);
    if (mode == 0) {
        wazaSequenceOwnerLoadWaza(data, animId, 4, 0);
    } else if (mode == 1) {
        wazaSequenceSysStartWaza(data, animId, 4);
        do {
            if ((u8)wazaSequenceSysIsPlayingWaza(data, animId, 4) == 0) {
                return;
            }
            _threadSwitch();
        } while (1);
    } else if (mode == 2) {
        wazaSequenceSysStopWaza(data, animId, 4);
    } else if (mode == 3) {
        wazaSequenceOwnerLoadWaza(data, 0x57, 4, 0);
    } else if (mode == 4) {
        wazaSequenceSysStartWaza(data, 0x57, 4);
    } else if (mode == 5) {
        do {
            if ((u8)wazaSequenceSysIsPlayingWaza(data, 0x57, 4) == 0) {
                return;
            }
            _threadSwitch();
        } while (1);
    } else if (mode == 6) {
        wazaSequenceSysStopWaza(data, 0x57, 4);
    }
}

void fightTrainerBallThrowEffect(void* context, void* trainerCtx, u8 mode)
{
    void* data;
    u8 typeId;
    u32 animId;

    if ((data = PC_PTR(fightTrainerGetStatus(context, 0, 0x4C, 0))) == NULL) {
        return;
    }
    typeId = (u8)(u32)pokemonGetStatus(fightOutPokemonGetPokemonPtr(trainerCtx),
                                       0, POKEMON_STATUS_BALL, 0);
    animId = itemGetStatus(0, typeId, 0x11, 0);
    if (mode == 0) {
        wazaSequenceOwnerLoadWaza(data, animId, 4, 0);
    } else if (mode == 1) {
        wazaSequenceSysStartWaza(data, animId, 4);
    } else if (mode == 2) {
        do {
            if ((u8)wazaSequenceSysIsPlayingWaza(data, animId, 4) == 0) {
                return;
            }
            _threadSwitch();
        } while (1);
    } else if (mode == 3) {
        wazaSequenceSysStopWaza(data, animId, 4);
    }
}
