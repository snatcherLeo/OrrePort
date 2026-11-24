#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fightTrainerAiIsPokemonJoutai();
#endif
extern u8 fightTrainerAiGetHpRatio();

s32 fightTrainerAiGetFightOutPokemonIrekaeJoutaiBadJoutaiAddsbuDataId(void* ctx, u32 param1) {
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x9)) == 1) {
        return 0x9;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0xA)) == 1) {
        return 0xA;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x1E)) == 1 && fightTrainerAiGetHpRatio(ctx, param1) == 0) {
        return 0xB;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0xE)) == 1) {
        return 0xC;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x17)) == 1) {
        return 0xD;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x18)) == 1) {
        return 0xE;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x19)) == 1) {
        return 0xF;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x1B)) == 1) {
        return 0x10;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x1C)) == 1) {
        return 0x11;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x1D)) == 1) {
        return 0x12;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x26)) == 1) {
        return 0x13;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x27)) == 1) {
        return 0x14;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x28)) == 1) {
        return 0x15;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x29)) == 1) {
        return 0x16;
    }
    if (fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x2A)) == 1) {
        return 0x17;
    }
    return fightTrainerAiIsPokemonJoutai(ctx, PC_PTR(param1), PC_PTR(0x30)) == 1 ? 0x18 : 0;
}
