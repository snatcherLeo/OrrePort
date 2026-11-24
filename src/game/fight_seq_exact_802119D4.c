#include "dolphin/types.h"

extern u32 ServerStatusFlag;
extern void fightSeqTurnEnd(void);

void fightSeqPost(void)
{
    ServerStatusFlag &= ~0x00100000u;
    fightSeqTurnEnd();
}
