#include "dolphin/types.h"

typedef struct PdaSprite {
    u8 pad00[0x50];
    s16 field_50;
    s16 field_52;
} PdaSprite;

extern s8 lbl_8047A47C;
extern s8 lbl_8047A47D;
extern f32 lbl_8047A484;
extern f32 lbl_8047A488;
extern f32 lbl_8047A48C;
extern f32 lbl_8047A494;
extern f32 lbl_8047BA64;
extern f32 lbl_8047BA68;
extern f32 lbl_8047BA6C;
extern f32 lbl_8047BA70;
extern void* windowGetKeyInfo(void);
extern f64 cos(f64 angle);
extern f64 sin(f64 angle);

static inline f32 pdaOrbitAbs(f32 v)
{
    if (v > 0.0f) {
        return v;
    }
    return -v;
}

static inline f32 pdaOrbitBaseAngle(f32 baseAngle)
{
    return lbl_8047A484 - baseAngle;
}

static inline void pdaUpdateOrbitSprite(PdaSprite* sprite, f32 baseAngle,
                                        s32 updateTarget)
{
    extern f32 lbl_802E52A8[4];
    f32 angle;
    f32 roundedStep;
    f32 step;
    f32 distance;
    f32 magnitude;
    s8 direction;
#ifdef TARGET_PC
    /* OrreHook_PcMenuWheel (docs/port/hook-points.md; 2026-11-15): with OrrePort's rows on the PC screen the
     * wheel has five icons, so a fifth choice's target (the table has exactly four entries: it must never be read for
     * choice 4), a slower step (each row still takes as long to turn to as with four) and how far apart the icons are
     * drawn (*Changed 2026-11-15, snatcherLeo: "They are too bunched together ... we use that same spacing the original game
     * had":* the game's 90 degrees between neighbours, not 72; icons two rows away go off the screen's right edge, as
     * the opposite one does with four) come from the hook. Nothing registered: the game's own. Kept in step with
     * pda_range_80037158.c. All platforms */
    extern void (*OrreHook_PcMenuWheel)(s32 choice, f32* target, f32* stepScale, f32* angleScale);
    f32 orreStep = 1.0f;
    f32 orreSpread = 1.0f;
    if (OrreHook_PcMenuWheel != NULL) {
        f32 target = (u8)lbl_8047A47C < 4 ? lbl_802E52A8[lbl_8047A47C] : lbl_8047A488;
        OrreHook_PcMenuWheel(lbl_8047A47C, &target, &orreStep, &orreSpread);
        if (updateTarget) {
            lbl_8047A488 = target;
        }
    } else
#endif
    if (updateTarget) {
        lbl_8047A488 = lbl_802E52A8[lbl_8047A47C];
    }
    angle = pdaOrbitBaseAngle(baseAngle);
    if (angle < 0.0f) {
        angle += 6.28318548f;
    }
    if (angle >= 6.28318548f) {
        angle -= 6.28318548f;
    }
#ifdef TARGET_PC
    if (orreSpread != 1.0f) {
        /* drawn spread out from the front slot (3 pi / 2: the wheel's left, nearest the list), within half a turn */
        f32 rel = angle - 4.71238898f;
        if (rel > 3.14159274f) {
            rel -= 6.28318548f;
        }
        if (rel <= -3.14159274f) {
            rel += 6.28318548f;
        }
        angle = 4.71238898f + rel * orreSpread;
    }
#endif

    windowGetKeyInfo();
    step = lbl_8047BA64 * lbl_8047A494;
#ifdef TARGET_PC
    step *= orreStep;
#endif
    if (lbl_8047A484 != lbl_8047A488) {
        if (lbl_8047A488 - lbl_8047A484 < 0.0f) {
            step = -step;
        }
        roundedStep = (f32)(f64)step;
        lbl_8047A48C = step;
        lbl_8047A484 += roundedStep;
        if (lbl_8047A484 >= 6.28318548f) {
            lbl_8047A484 -= 6.28318548f;
        }
        if (lbl_8047A484 < 0.0f) {
            lbl_8047A484 += 6.28318548f;
        }

        distance = pdaOrbitAbs(lbl_8047A484 - lbl_8047A488);
        magnitude = pdaOrbitAbs(roundedStep);
        if (distance < magnitude) {
            lbl_8047A484 = lbl_8047A488;
            lbl_8047A48C = 0.0f;
        }
    } else if (lbl_8047A47D != lbl_8047A47C) {
        direction = lbl_8047A47D - lbl_8047A47C;
        if (direction > 0) {
            lbl_8047A48C = -step;
        } else if (direction < 0) {
            lbl_8047A48C = step;
        }
        lbl_8047A47D = lbl_8047A47C;
    }

    sprite->field_50 =
        (s16)(lbl_8047BA6C * (f32)sin(angle) + lbl_8047BA68);
    sprite->field_52 =
        (s16)(lbl_8047BA6C * (f32)cos(angle) + lbl_8047BA70);
}

#ifndef PDA_ORBIT_ONLY_FOURTH
void fn_800379E8(void* window, PdaSprite* sprite)
{
    extern f32 lbl_802E52B8[4];
    (void)window;
    pdaUpdateOrbitSprite(sprite, lbl_802E52B8[3], 0);
}

void fn_80037BB0(void* window, PdaSprite* sprite)
{
    extern f32 lbl_802E52B8[4];
    (void)window;
    pdaUpdateOrbitSprite(sprite, lbl_802E52B8[2], 0);
}

void fn_80037D78(void* window, PdaSprite* sprite)
{
    extern f32 lbl_802E52B8[4];
    (void)window;
    pdaUpdateOrbitSprite(sprite, lbl_802E52B8[1], 0);
}
#endif

#ifdef PDA_ORBIT_INCLUDE_FOURTH
void fn_80037F40(void* window, PdaSprite* sprite)
{
    extern f32 lbl_802E52B8[4];
    (void)window;
    pdaUpdateOrbitSprite(sprite, lbl_802E52B8[0], 1);
}
#endif
