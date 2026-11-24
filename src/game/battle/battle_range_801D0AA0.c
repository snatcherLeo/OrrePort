/**
 * @file battle_range_801D0AA0.c
 * @brief battle-domain (direct calls into battle_*.c), 0x801D0AA0 - 0x801D1470.
 *
 * Boundary evidence-verified from asm (sdata clusters, callee families,
 * static linkage, call chains) - mixed-block split pass, 2026-07-01.
 * All functions asm-only until matched.
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

u8 heroGetPokemonBallId(u32 index)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataGetStatus(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroBiosGetPokemonPtr(void*, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonCheckValid(void*);
#endif
    extern u8 pokemonBiosGetCatchBallId(void*);
    void* pokemon;

    pokemon = heroBiosGetPokemonPtr(savedataGetStatus(0, 2), (u16)index);
    if ((u8)pokemonCheckValid(pokemon) == 0) {
        return 0;
    }
    return pokemonBiosGetCatchBallId(pokemon);
}

u16 fn_801D0AFC(s32 mode)
{
    extern u16 lbl_8047B3D8;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* savedataGetStatus(u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* heroBiosGetPokemonPtr(void*, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8 pokemonCheckValid(void*);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(void*, u32, u32, u32);
#endif
    extern u8 pokemonBiosGetCatchBallId(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void pokemonAllKaihuku(void*);
#endif
    u32 i;
    u32 status;
    u8 valid;
    void* pokemon;

    lbl_8047B3D8 = i = 0;
    while ((u16)i < 6) {
        valid = 1;
        pokemon = heroBiosGetPokemonPtr(savedataGetStatus(0, 2), (u16)i);
        if (!pokemonCheckValid(pokemon)) {
            valid = 0;
        } else {
            status = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_SPECIES, 0);
            pokemonGetStatus(pokemon, (u16)status, POKEMON_STATUS_SPECIES_NAME, 0);
            pokemonBiosGetCatchBallId(pokemon);
            pokemonAllKaihuku(pokemon);
        }
        if (valid) {
            lbl_8047B3D8++;
        }
        i++;
    }
    return lbl_8047B3D8;
}

void std083_unk_801d0bd0(s32 arg0, s32 arg1, s32 mode)
{
    extern void pokemonCenterHealSequence(void);
    extern void pokemonCenterBallMachineSequence(s32, s32);

    switch (mode) {
    case 0:
        pokemonCenterBallMachineSequence(arg0, arg1);
        break;
    case 1:
        pokemonCenterHealSequence();
        break;
    case 2:
        pokemonCenterHealSequence();
        break;
    case 3:
        pokemonCenterHealSequence();
        break;
    }
}

void pokemonCenterHealSequence(void)
{
    extern f32 lbl_8047E184;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void _threadSwitch(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fadeSet(f32 duration, s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8 fadeCheck(s32 wait);
#endif
    extern u32 soundPauseMusic(void);
    extern void soundResumeMusic(u32 handle);
    extern void soundPlayFanfare(s32 id, s32 fade, s32 volume);
    s32 running;
    s32 state;
    s32 timer;
    u32 handle;

    running = 1;
    state = 0;
    timer = 0;
    do {
        switch (state) {
        case 0:
            timer = 0;
            fn_801D0AFC(1);
            state = 10;
            break;
        case 10:
            fadeSet(lbl_8047E184, 3);
            state = 12;
            handle = soundPauseMusic();
            break;
        case 12:
            if (fadeCheck(0) == 0) {
                state = 6;
            } else {
                _threadSwitch();
            }
            break;
        case 6:
            timer++;
            if (timer >= 10) {
                state = 8;
                soundPlayFanfare(0x19, 2000, 0xFF);
                timer = 0;
            } else {
                _threadSwitch();
            }
            break;
        case 8:
            timer++;
            if (timer >= 10) {
                state = 9;
            } else {
                _threadSwitch();
            }
            break;
        case 9:
            soundResumeMusic(handle);
            fadeSet(lbl_8047E184, 2);
            state = 11;
            break;
        case 11:
            if (fadeCheck(0) == 0) {
                state = 1000;
            } else {
                _threadSwitch();
            }
            break;
        case 1000:
            running = 0;
            break;
        }
    } while (running != 0);
}
