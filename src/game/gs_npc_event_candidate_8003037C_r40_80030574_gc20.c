/**
 * @file gs_npc_event_candidate_8003037C_r40_80030574_gc20.c
 * @brief fn_80030574 carve, 0x80030574 - 0x800307A8.
 *
 * NPC event callback: look the event id up in the move-slot columns of
 * the two-entry table lbl_80266F68, fetch the party (1) or storage (2)
 * Pokemon it names and print the name of its move in that slot.
 */
#include "dolphin/types.h"
#include "game/win_sprite.h"
#include "game/constants/pokemon_status.h"

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 heroGetStatus(void* partyData, s32 slot, s32 p3);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue(s32 paramId, s32 value);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintRect(s32, s32, s16, s16, u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 pokemonGetStatus(void* pokemon, u32 slot, u16 tableId, u16 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8 pokemonWazaCheckValid(void* pokemon, u16 slot);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 wazaGetStatus(u32 context, u32 wazaId, u32 field, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSmsgGetGSchar(u32 msg);
#endif

extern u8 lbl_80266F68[];
extern u8 lbl_803A2688[];
extern u32 lbl_8047A420;
extern u32 lbl_8047A424;

void fn_80030574(u8* r3, u8* r4)
{
    u32 combined;
    u8* table;
    s32 kind;
    s32 slot;
    u16 waza;
    void* pokemon;
    u32 msg;
    s32 i;

    pokemon = NULL;
#ifdef TARGET_PC /* the sprite colour is the bytes R, G, B, A (winSpriteDraw): read the word big-endian */
    combined = (__builtin_bswap32(*(u32*)(r4 + 0x64)) & ~0xFF) | *(u8*)(r3 + 0x8B);
#else
    combined = (*(u32*)(r4 + 0x64) & ~0xFF) | *(u8*)(r3 + 0x8B);
#endif
    kind = 0;
    table = lbl_80266F68;
    for (i = 0; i < 2; i++) {
        if (*(s16*)(r4 + 0x6) == *(u16*)(table + 0x8)) {
            kind = *(u16*)(table + 0x0);
            slot = 0;
        }
        if (*(s16*)(r4 + 0x6) == *(u16*)(table + 0xA)) {
            kind = *(u16*)(table + 0x0);
            slot = 1;
        }
        if (*(s16*)(r4 + 0x6) == *(u16*)(table + 0xC)) {
            kind = *(u16*)(table + 0x0);
            slot = 2;
        }
        if (*(s16*)(r4 + 0x6) == *(u16*)(table + 0xE)) {
            kind = *(u16*)(table + 0x0);
            slot = 3;
        }
        table += 0x14;
    }

    switch (kind) {
    case 1:
        pokemon = (void*)PC_PTR(heroGetStatus(NULL, 3, (u16)lbl_8047A424));
        break;
    case 2:
        pokemon = (void*)PC_PTR(heroGetStatus(lbl_803A2688, 3, (u16)lbl_8047A420));
        break;
    }

    if (pokemon != NULL) {
        waza = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, slot);
        if (pokemonWazaCheckValid(pokemon, slot) == 0) {
            waza = 0;
        }
        msg = waza;
        switch (msg) {
        case 0:
            break;
        case 0xFFFF:
            msg = 0x933;
            break;
        case 0xFFFE:
            msg = 0x934;
            break;
        default:
            msg = wazaGetStatus(0, msg, 1, 0);
            break;
        }
        if (msg != 0) {
            msgctrlSetValue(0x37, PC_PTR32_IMPLICIT(GSmsgGetGSchar(msg)));
            GSmsgPrintRect(0, 0, *(s16*)(r4 + 0x54), *(s16*)(r4 + 0x56), combined, 0xE7);
            winSpriteSetDisp(r4, 1);
        } else {
            winSpriteSetDisp(r4, 0);
        }
    } else {
        winSpriteSetDisp(r4, 0);
    }
}
