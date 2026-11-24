#ifndef GAME_WIN_SEQUENCE_H
#define GAME_WIN_SEQUENCE_H

/* winSeq types, externs and the per-sprite/per-window move blocks shared by
 * the winSeq/winSprite TU (win_sprite.c) and its head carves. The move blocks
 * are expanded six and two times in retail with no out-of-line copy
 * (find_inline_expansions.py block 0x801072A4 0x8010739C scores 1.000 at
 * fn_80106F98, winSeqMoveMenu and fn_80107F38; block 0x801071EC 0x801072A0
 * scores 0.911 at winSeqSetMenu with the same four calls). */

#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT */

/* The flag bits are bitfields (retail extracts them with rlwinm):
 * relative (0x80) makes a move's target relative to the current position,
 * interp (0x60) picks the move's easing, keep (0x18) keeps the current x
 * (bit 0) and/or y (bit 1). */
typedef struct DISC_STRUCT WinSeqCommand { /* menu-sequence commands are data in the executable, kept big-endian on the PC: a big-endian view (GCC lays the bit-fields out big-endian too) */
    u8 relative : 1;
    u8 interp : 2;
    u8 keep : 2;
    u8 pad_00 : 3;
    u8 type;
    s16 duration;
    s32 value0;
    s32 value1;
} WinSeqCommand;

typedef struct WinSeqState {
    PTR32_FIELD(WinSeqCommand) commands; /* runtime record: 32-bit slot on the PC too (pc/ptr32.h) */
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
} WinSeqState;

typedef struct WinSeqColor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} WinSeqColor;

/* The state _winSeqMoveSub animates: a copy of the sprite's or window's
 * drawable fields, written back once the frame's steps have run. */
typedef struct WinSeqTarget {
    s16 x;
    s16 y;
    WinSeqColor color;
    s16 baseX;
    s16 baseY;
    f32 scaleX;
    f32 scaleY;
    u32 image;
    s16 imageX;
    s16 imageY;
    s16 imageW;
    s16 imageH;
    s8 flags;
} WinSeqTarget;

typedef struct DISC_STRUCT WinSeqSpriteData { /* the menu sprite table (menuSpriteBiosGetPtr), data in the executable: big-endian on the PC, as window_exact_80104160.c's WinSpriteInfo */
    u8 pad_00[8];
    s16 x;
    s16 y;
    s16 width;
    s16 height;
    u32 image;
} WinSeqSpriteData;

typedef struct DISC_STRUCT MENU_ITEM { /* the menu item table (menuItemBiosGetPtr): big-endian on the PC, as menu.c's MenuItem; read natively the items' base x / y were byte-swapped (top-menu cards and buttons, 2026-10-16) */
    u8 pad_00[2];
    s16 x;
    s16 y;
} MENU_ITEM;

typedef struct DISC_STRUCT MENU_DATA { /* the menu table (menuDataBiosGetPtr): big-endian on the PC, as menu.c's MenuData */
    u8 pad_00[6];
    s16 x;
    s16 y;
} MENU_DATA;

typedef struct tagSPRITE_WORK {
    PTR32_FIELD(struct tagSPRITE_WORK) next;
    s8 flags;
    u8 pad_05;
    s16 itemId;
    u8 pad_08[4];
    WinSeqState sequence;
    u8 pad_48[8];
    s16 x;
    s16 y;
    u8 pad_54[4];
    u32 image;
    s16 imageX;
    s16 imageY;
    s16 imageW;
    s16 imageH;
    WinSeqColor color;
    f32 scaleX;
    f32 scaleY;
    u8 pad_70[8];
} tagSPRITE_WORK;

typedef struct tagWINDOW_WORK {
    s8 flags;
    u8 pad_01[3];
    u32 id;
    u8 pad_08[0x14];
    PTR32_FIELD(tagSPRITE_WORK) sprites;
    PTR32_FIELD(tagSPRITE_WORK) overlaySprites;
    WinSeqState menuSequence;
    u8 pad_60[0x24];
    s16 x;
    s16 y;
    WinSeqColor color;
} tagWINDOW_WORK;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MENU_DATA* menuDataBiosGetPtr(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern MENU_ITEM* menuItemBiosGetPtr(s16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern WinSeqSpriteData* menuSpriteBiosGetPtr(s32 id);
#endif
extern WinSeqCommand* menuSeqBiosGetPtr(u32 id);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern tagWINDOW_WORK* windowSearchID(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern tagSPRITE_WORK* windowSearchItemID(tagWINDOW_WORK* window, u16 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
extern f64 sqrt(f64 value);

extern WinSeqTarget lbl_80404B68;  /* sprite move target */
extern WinSeqTarget lbl_80404B8C;  /* window move target */
extern tagSPRITE_WORK* lbl_8047AD1C;  /* sprite pool, 0x168 entries */

void _winSeqMoveSub(WinSeqTarget* target, WinSeqState* state);



/* Runs one frame of a sprite's sequence through the shared move target. */
static inline void winSeqMoveSprite(tagSPRITE_WORK* sprite) {
    u32 i;

    lbl_80404B68.x = sprite->x;
    lbl_80404B68.y = sprite->y;
    lbl_80404B68.color = sprite->color;
    lbl_80404B68.scaleX = sprite->scaleX;
    lbl_80404B68.scaleY = sprite->scaleY;
    lbl_80404B68.flags = sprite->flags;
    lbl_80404B68.image = sprite->image;
    lbl_80404B68.imageX = sprite->imageX;
    lbl_80404B68.imageY = sprite->imageY;
    lbl_80404B68.imageW = sprite->imageW;
    lbl_80404B68.imageH = sprite->imageH;
    lbl_80404B68.baseX = ((MENU_ITEM*)menuItemBiosGetPtr(sprite->itemId))->x;
    lbl_80404B68.baseY = ((MENU_ITEM*)menuItemBiosGetPtr(sprite->itemId))->y;
    for (i = 0; i < GSgetFrameStep(); i++) {
        _winSeqMoveSub(&lbl_80404B68, &sprite->sequence);
    }
    sprite->x = lbl_80404B68.x;
    sprite->y = lbl_80404B68.y;
    sprite->color = lbl_80404B68.color;
    sprite->scaleX = lbl_80404B68.scaleX;
    sprite->scaleY = lbl_80404B68.scaleY;
    sprite->flags = lbl_80404B68.flags;
    sprite->image = lbl_80404B68.image;
    sprite->imageX = lbl_80404B68.imageX;
    sprite->imageY = lbl_80404B68.imageY;
    sprite->imageW = lbl_80404B68.imageW;
    sprite->imageH = lbl_80404B68.imageH;
}

/* The same for the window's own menu sequence. */
static inline void winSeqMoveWindow(tagWINDOW_WORK* window) {
    u32 i;

    lbl_80404B8C.x = window->x;
    lbl_80404B8C.y = window->y;
    lbl_80404B8C.color = window->color;
    lbl_80404B8C.flags = window->flags;
    lbl_80404B8C.baseX = ((MENU_DATA*)menuDataBiosGetPtr(window->id))->x;
    lbl_80404B8C.baseY = ((MENU_DATA*)menuDataBiosGetPtr(window->id))->y;
    for (i = 0; i < GSgetFrameStep(); i++) {
        _winSeqMoveSub(&lbl_80404B8C, &window->menuSequence);
    }
    window->x = lbl_80404B8C.x;
    window->y = lbl_80404B8C.y;
    window->color = lbl_80404B8C.color;
    window->flags = lbl_80404B8C.flags;
}

#endif /* GAME_WIN_SEQUENCE_H */
