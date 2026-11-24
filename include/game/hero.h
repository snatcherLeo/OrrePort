#ifndef GAME_HERO_H
#define GAME_HERO_H

#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 heroGetStatus(u8* hero, u32 selector, u32 index);
#endif

#endif /* GAME_HERO_H */
