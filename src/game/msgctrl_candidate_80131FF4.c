/**
 * @file msgctrl_candidate_80131FF4.c
 * @brief msgctrl.c carve, 0x80131FF4 - 0x80132834: the battle name and
 *        message-variable getters, the digit wrappers, and the text control
 *        codes msgctrlWait, msgctrlPalette, msgctrlColor, msgctrlFont, the
 *        Ruby codes, msgctrlKeyWait, msgctrlKeyEnd and msgctrlCR.
 *
 * GSmsgDispatchControl reaches these through the msgctrlcode table. Built
 * with the TU's -O4,p and the peephole pass off (one unit-wide flag in
 * configure.py, as for msgctrl.c and msgctrl_exact_80132A38.c).
 *
 * msgctrlCR's int-to-float conversion bias is msgctrl.c's only .sdata2
 * literal (0x8047D0E0, read by nothing else), so this unit owns .sdata2
 * 0x8047D0E0 - 0x8047D0E8.
 *
 * The four *Mons getters share one static inline: retail expands the same
 * trainer-or-nickname body in each (and in msgctrlClientnowork).
 *
 * The file includes game/msgctrl_obj.h rather than effect_util_types.h,
 * whose placeholder prototypes for these callees take no arguments.
 */
#include "dolphin/types.h"
#include "game/msgctrl_obj.h"
#include "pc/disc_types.h" /* DISC_U32: message text is disc data */

extern u32 lbl_8047ADC8;
extern u32 lbl_8047ADCC;
extern u32 lbl_8047ADD0;
extern u32 _ATTACK_MONS;
extern u32 _DEFENCE_MONS;
extern u32 _CLIENT_MONS;
extern u32 _TSUIKA_MONS;
extern u16 _Item;
extern u16 _Item2;
extern u32 lbl_8047AE54;
extern u32 lbl_8047AE58;
extern u32 lbl_8047AE5C;
extern u32 _Pokemon;
extern u32 _Pokemon2;
extern u32 lbl_8047AE68;
extern u32 lbl_8047AE6C;
extern u32 _MenuPokemon;
extern u32 _MenuMsg;
extern u32 _MenuMsg2;
extern u16 _Waza;
extern u8 lbl_80427150[];
extern u8 lbl_80427170[];
extern u8 lbl_80427190[];
extern u8 lbl_804271B0[];

typedef struct MsgPaletteColor {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} MsgPaletteColor;

extern u32* lbl_80478E88;
extern MsgPaletteColor* lbl_80478E8C;

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightFloorGetFightOutPokemonPtrToFightTrainerPtr(void* obj, u32 search);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 fn_801F18DC(u32 param);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightTrainerGetNamePtr(void* trainer);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fightOutPokemonGetNicknamePtr(u32 pokemon);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmsgGetGSchar(u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* wazaDataBiosGetPtr(u16 idx);
#endif
extern u32 wazaDataBiosGetName(void* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* itemDataBiosGetPtr(u16 index);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 itemDataBiosGetName(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* savedataGetStatus(u32 side, u32 slotType);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroBiosGetHizukiNamePtr(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 heroBiosGetNamePtr(void* ptr);
#endif
extern void GSmsgSetColor(EffectUtilCommandObj* obj);
extern void GSmsgSetFontInfo(EffectUtilCommandObj* obj);
extern void GSmsgInitRuby(EffectUtilCommandObj* obj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 menuIsCheck(u32 objID);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStartSE(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(u32 id, u32 value);
#endif
extern void* _msgctrlMakeDigit__FPUslUll(void* table, u32 stride, u32 count, u32 type);

static inline void* msgctrlOutPokemonName(u32 pokemon)
{
    void* trainer = fightFloorGetFightOutPokemonPtrToFightTrainerPtr(0, PC_PTR(pokemon));

    if (fn_801F18DC(0) == 1 && trainer != 0) {
        msgctrlSetValue(0x4D, PC_PTR32(fightTrainerGetNamePtr(trainer)));
        msgctrlSetValue(0x57, PC_PTR32(fightOutPokemonGetNicknamePtr(pokemon)));
        return GSmsgGetGSchar(0x7721);
    }
    return fightOutPokemonGetNicknamePtr(PC_PTR(pokemon));
}

void* msgctrlTsuikaMons(void)
{
    return msgctrlOutPokemonName(_TSUIKA_MONS);
}

void* msgctrlClientMos(void)
{
    return msgctrlOutPokemonName(_CLIENT_MONS);
}

void* msgctrlDeffenceMons(void)
{
    return msgctrlOutPokemonName(_DEFENCE_MONS);
}

void* msgctrlAttackMons(void)
{
    return msgctrlOutPokemonName(_ATTACK_MONS);
}

u32 msgctrlEvStrBuf2(void) { return lbl_8047ADD0; }

u32 msgctrlEvStrBuf1(void) { return lbl_8047ADCC; }

u32 msgctrlEvStrBuf0(void) { return lbl_8047ADC8; }

u32 msgctrlWaza(void)
{
    return wazaDataBiosGetName(wazaDataBiosGetPtr(_Waza));
}

u32 msgctrlMenuMsg2(void) { return _MenuMsg2; }

u32 msgctrlMenuMsg(void) { return _MenuMsg; }

u32 msgctrlMenuPokemon(void) { return _MenuPokemon; }

u32 msgctrlMenuDigit2(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427150, 0x10, lbl_8047AE6C, 0));
}

u32 msgctrlMenuDigit(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427170, 0x10, lbl_8047AE68, 0));
}

u32 msgctrlPokemon2(void) { return _Pokemon2; }

u32 msgctrlPokemon(void) { return _Pokemon; }

u32 msgctrlMsgID(void) { return lbl_8047AE5C; }

u32 msgctrlDigit2(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_80427190, 0x10, lbl_8047AE58, 0));
}

u32 msgctrlDigit(void) /* the text engine uses the result (msgctrlcode mode 1/2): r3 is the last call's result (assembly) */
{
    return PC_PTR32(_msgctrlMakeDigit__FPUslUll(lbl_804271B0, 0x10, lbl_8047AE54, 0));
}

u32 msgctrlItem2(void)
{
    u32 name = itemDataBiosGetName(itemDataBiosGetPtr(_Item2));

    if (name == 0) {
        name = 0x2B6E;
    }
    return name;
}

u32 msgctrlItem(void)
{
    u32 name = itemDataBiosGetName(itemDataBiosGetPtr(_Item));

    if (name == 0) {
        name = 0x2B6E;
    }
    return name;
}

u32 msgctrlHizuki(void)
{
    return PC_PTR32_IMPLICIT(heroBiosGetHizukiNamePtr(PC_PTR(savedataGetStatus(0, 2))));
}

u32 msgctrlHero(void)
{
    return heroBiosGetNamePtr(PC_PTR(savedataGetStatus(0, 2)));
}

s32 msgctrlWait(EffectUtilCommandObj* obj)
{
    if (obj->activeFlag == 0) {
        if (obj->waitCounter == 0) {
            obj->waitCounter = P32(obj->stream)[0] + 1;
        }
        if (--obj->waitCounter <= 0) {
            obj->waitCounter = 0;
        } else {
            P32_SET(obj->stream, P32(obj->stream) - 3);
            return 1;
        }
    }
    P32_SET(obj->stream, P32(obj->stream) + 1);
    return 0;
}

s32 msgctrlPalette(EffectUtilCommandObj* obj)
{
    u8 idx;

    if (obj->activeFlag != 0) {
        idx = P32(obj->stream)[0];
        if (idx >= *lbl_80478E88) {
            idx = 0;
        }
        obj->colorRgba = (lbl_80478E8C[idx].r << 24) | (lbl_80478E8C[idx].g << 16) |
                         (lbl_80478E8C[idx].b << 8) | lbl_80478E8C[idx].a;
        GSmsgSetColor(obj);
    }
    P32_SET(obj->stream, P32(obj->stream) + 1);
    return 0;
}

s32 msgctrlColor(EffectUtilCommandObj* obj)
{
    if (obj->activeFlag != 0) {
        obj->colorRgba = DISC_U32(P32(obj->stream)); /* the RGBA bytes follow the code in the big-endian text */
        GSmsgSetColor(obj);
    }
    P32_SET(obj->stream, P32(obj->stream) + 4);
    return 0;
}

s32 msgctrlFont(EffectUtilCommandObj* obj)
{
    if (obj->activeFlag == 0) {
        obj->commandValue = P32(obj->stream)[0];
        GSmsgSetFontInfo(obj);
    } else {
        obj->commandValue = P32(obj->stream)[0];
        GSmsgSetFontInfo(obj);
    }
    P32_SET(obj->stream, P32(obj->stream) + 1);
    return 0;
}

s32 msgctrlRubyEnd(EffectUtilCommandObj* obj)
{
    obj->field_4B = 0;
    return 0;
}

s32 msgctrlRubyTop(EffectUtilCommandObj* obj)
{
    obj->field_4B = 2;
    return 0;
}

s32 msgctrlRubyStart(EffectUtilCommandObj* obj)
{
    if (obj->activeFlag != 0) {
        GSmsgInitRuby(obj);
    }
    obj->field_4B = 1;
    return 0;
}

s32 msgctrlKeyWait(EffectUtilCommandObj* obj)
{
    if (obj->flags & 2) {
        obj->pendingFlag = 1;
    }
    if (menuIsCheck(0x0A)) {
        obj->pendingFlag = 0;
    }
    if (obj->activeFlag == 0) {
        if (obj->pendingFlag != 0) {
            obj->pendingFlag = 0;
            P32_SET(obj->savedStream, P32(obj->stream));
            if ((obj->flags & 2) == 0) {
                soundStartSE(0x24);
            }
        } else {
            P32_SET(obj->stream, P32(obj->stream) - 3);
            obj->doneFlag = 1;
        }
    } else {
        obj->field_0C = obj->field_04;
        obj->field_10 = obj->field_08;
    }
    return 1;
}

s32 msgctrlKeyEnd(EffectUtilCommandObj* obj)
{
    if (obj->flags & 2) {
        obj->pendingFlag = 1;
    }
    if (menuIsCheck(0x0A)) {
        obj->pendingFlag = 0;
    }
    if (obj->pendingFlag != 0) {
        obj->pendingFlag = 0;
    } else {
        P32_SET(obj->stream, P32(obj->stream) - 3);
    }
    return 1;
}

/* Control codes 0 and 1: back to the line origin (0x04) and down one line,
 * the glyph height plus msgctrlLineSpace's spacing, times the scale. */
s32 msgctrlCR(EffectUtilCommandObj* obj)
{
    obj->field_0C = obj->field_04;
    /* RULE-EXCEPTION(user-approved): explicit conversions preserve retail scheduling - see docs/RULE_EXCEPTIONS.md */
    obj->field_10 += obj->field_64 * (f32)(obj->field_23 + (s8)(u8)obj->field_42);
    return 0;
}
