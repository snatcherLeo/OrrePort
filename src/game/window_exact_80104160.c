/**
 * @file window_exact_80104160.c
 * @brief windowDrawSprite2 (0x80104160 - 0x80104318): draws one menu-sprite
 *        BIOS entry at a position/size through winSpriteDraw, with a
 *        throw-away draw context when none is given.
 *
 * Function-boundary carve of the window TU, built with its flags (GC/2.5,
 * -O4,p, "-opt nopeephole"; see window_exact_80104318.c). Text only.
 *
 * The node layout is winSpriteDraw's (win_sprite.c). The two scales read the
 * window TU's own 1.0f pool literal (0x8047CDEC, shared with windowOpen)
 * through the same extern stand-in window_r50_80104CA0_suffix.c uses.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* DISC_STRUCT: the menu tables stay big-endian bytes on the PC (port-gaps.md 6.2, 10) */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dst, int val, u32 size);
#endif
extern void fn_800FE6AC(s16* x, s16* y);
/* RULE-EXCEPTION(title-path): extern stand-in for the window TU's pool literal - see docs/RULE_EXCEPTIONS.md */
extern const f32 lbl_8047CDEC;

typedef void WinSpriteDrawFn(u8*, struct WinSpriteDrawNode*);
typedef struct WinSpriteDrawNode {
    PTR32_FIELD(struct WinSpriteDrawNode) next; /* 32-bit slots on the PC too (pc/ptr32.h) */
    s8 flags;
    u8 drawFlags;
    u8 pad_06[2];
    u32 primitive;
    u8 pad_0C[0x48 - 0x0C];
    PTR32_FIELD(WinSpriteDrawFn) drawCallback;
    PTR32_FIELD(void) drawArg;
    s16 x;
    s16 y;
    s16 width;
    s16 height;
    u32 texture_id;
    s16 crop_x;
    s16 crop_y;
    s16 crop_width;
    s16 crop_height;
    union {
        u8 color[4];
        u32 rgba;
    };
    f32 scale_x;
    f32 scale_y;
    f32 rotation;
    u8 kind;
} WinSpriteDrawNode;

typedef struct WinSpriteContext {
    u8 pad_00[0x84];
    s16 x;
    s16 y;
    s32 layer;
    u8 pad_8C[0xB4 - 0x8C];
} WinSpriteContext;

typedef struct DISC_STRUCT WinSpriteInfo {
    u8 pad_bits : 2;
    u8 type : 2;
    u8 pad_bits2 : 4;
    u8 pad_01[4];
    s8 offsetX;
    s8 offsetY;
    u8 alpha;
    s16 cropX;
    s16 cropY;
    s16 cropW;
    s16 cropH;
    u32 texture;
} WinSpriteInfo;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern WinSpriteInfo* menuSpriteBiosGetPtr(u16 id);
#endif

extern void winSpriteDraw(WinSpriteContext* context, WinSpriteDrawNode* sprite);

/* 0x80104160 | 0x1B8 */
void windowDrawSprite2(s16 x, s16 y, s16 width, s16 height, u32 color,
                       WinSpriteContext* context, s32 spriteId, u32 flags) {
    WinSpriteDrawNode sprite;
    WinSpriteDrawNode* sp = &sprite; /* RULE-EXCEPTION(title-path): pointer copy only for retail's r30 - see docs/RULE_EXCEPTIONS.md */
    WinSpriteContext localContext;
    WinSpriteInfo* info;

#ifdef TARGET_PC
    /* the 52 callers declare this function themselves, several with the window as an s32 / u32 (a 32-bit slot value);
     * on Windows x64 the 6th argument is an 8-byte stack slot, and a 4-byte store leaves its upper half garbage, so
     * the item menu's gauge ring crashed on a non-canonical address in Willie's battle (2026-10-17). Everything the
     * game sees lives below 4 GB in OrrePort's layout, so the low 32 bits are the pointer for every caller. */
    context = (WinSpriteContext*)(uintptr_t)(u32)(uintptr_t)context;
#endif
    info = menuSpriteBiosGetPtr((u16)spriteId);
    memset(sp, 0, sizeof(sprite));
    sprite.flags = 7;
#ifdef TARGET_PC
    /* winSpriteDraw reads the colour as the bytes R, G, B, A (the GameCube's memory order), and the alpha is scaled in
     * color[3] below: store it big-endian. A native store put R in color[3] and A in color[0], so the name entry's
     * half-transparent white balls drew as opaque cyan (A, 2026-10-17). */
    sprite.color[0] = (u8)(color >> 24);
    sprite.color[1] = (u8)(color >> 16);
    sprite.color[2] = (u8)(color >> 8);
    sprite.color[3] = (u8)color;
#else
    sprite.rgba = color;
#endif
    sprite.scale_x = lbl_8047CDEC;
    sprite.scale_y = lbl_8047CDEC;
    if (info->type == 1) {
        sprite.drawFlags |= 1;
        sprite.texture_id = info->texture;
    } else if (info->type == 2) {
        sprite.drawFlags |= 2;
        sprite.primitive = info->texture;
    }
    sprite.x = x + info->offsetX;
    sprite.y = y + info->offsetY;
    sprite.width = width;
    sprite.height = height;
    sprite.crop_x = info->cropX;
    sprite.crop_y = info->cropY;
    sprite.crop_width = info->cropW;
    sprite.crop_height = info->cropH;
    sprite.color[3] = (info->alpha * sprite.color[3]) / 255;
    if (flags & 1) {
        sprite.width = -(s16)width; /* RULE-EXCEPTION(title-path): no-op cast, only for retail's extsh - see docs/RULE_EXCEPTIONS.md */
    }
    if (flags & 2) {
        sprite.height = -sprite.height;
    }
    if (context == NULL) {
        context = &localContext;
        memset(context, 0, sizeof(WinSpriteContext));
        fn_800FE6AC(&context->x, &context->y);
        context->layer = -1;
    }
#ifdef TARGET_PC
    { /* OrreHook_2DPiece (observation; frame smoothing's 2D at 120, round 12): names this window sprite (group 4: the
       * sprite's id; the window context as its id) for the draws it makes. The many screens that draw their sprites
       * themselves, outside the menu daemon's list (the PC screen's icon wheel, the P*DA), come through here. Inside an
       * already named piece (a gauge, a menu sprite's callback) the port keeps the outer name (pc_smooth.c).
       * All platforms */
        extern void pc_orre_2d_tag(s32 kind, u32 variant, const void* id, f32 value);
        /* no window given: the stack copy above has no lasting address, so the id is 0 (the draw order tells them
         * apart) */
        pc_orre_2d_tag(0x40000 | (s32)(spriteId & 0xFFFF), 0, context == &localContext ? NULL : context, 0.0f);
        winSpriteDraw(context, sp);
        pc_orre_2d_tag(0, 0, (const void*)0, 0.0f);
    }
#else
    winSpriteDraw(context, sp);
#endif
}
