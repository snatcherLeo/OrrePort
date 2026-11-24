#include "dolphin/types.h"

typedef struct WinSeqCommand WinSeqCommand;

typedef struct WinSequenceState {
    PTR32_FIELD(WinSeqCommand) commands; /* 32-bit slot on the PC too (pc/ptr32.h) */
    s16 commandIndex;
    s16 delay;
    u8 positionMode;
    u8 colorMode;
    u8 scaleMode;
    u8 loopActive;
    s16 startX;
    s16 startY;
    s16 endX;
    s16 endY;
    s16 positionFrame;
    s16 positionDuration;
    u8 startColor[4];
    u8 endColor[4];
    s16 colorFrame;
    s16 colorDuration;
    f32 startScaleX;
    f32 startScaleY;
    f32 endScaleX;
    f32 endScaleY;
    s16 scaleFrame;
    s16 scaleDuration;
    s16 loopCount;
    u8 enabled;
    u8 pad_3B;
} WinSequenceState;

typedef struct tagSPRITE_WORK {
    PTR32_FIELD(struct tagSPRITE_WORK) next;
    u8 pad_04[8];
    WinSequenceState sequence;
} tagSPRITE_WORK;

typedef struct tagWINDOW_WORK {
    s8 flags;
    s8 phase;
    u8 phaseFrame;
    u8 pad_03;
    u32 id;
    u8 pad_08;
    s8 fadePriority;
    u8 transitionDone;
    u8 pad_0B[5];
    PTR32_FIELD(struct tagWINDOW_WORK) next;
    u8 pad_14[5];
    s8 cursorCount;
    u8 cursorMode;
    u8 cursorFlags;
    PTR32_FIELD(tagSPRITE_WORK) sprites;
    PTR32_FIELD(tagSPRITE_WORK) overlaySprites;
    WinSequenceState menuSequence;
} tagWINDOW_WORK;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern tagWINDOW_WORK* windowSearchID(s32 id);
#endif

s32 winSeqCheckMove(s32 id)
{
    tagWINDOW_WORK* window;
    tagSPRITE_WORK* sprite;

    window = windowSearchID(id);
    if (window == NULL) {
        return 0;
    }
    if (P32(window->menuSequence.commands) != NULL) {
        return 1;
    }

    sprite = P32(window->sprites);
    while (sprite != NULL) {
        if (P32(sprite->sequence.commands) != NULL) {
            if (sprite->sequence.enabled == 0) {
                return 1;
            }
        }
        sprite = P32(sprite->next);
    }
    return 0;
}
