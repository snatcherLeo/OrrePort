#ifndef GAME_WIN_SPRITE_H
#define GAME_WIN_SPRITE_H

#include "dolphin/types.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void winSpriteSetDisp(void* node, u32 enable);
#endif

#endif /* GAME_WIN_SPRITE_H */
