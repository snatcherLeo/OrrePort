/**
 * @file menuFight_exact_80010294.c
 * @brief Exact menuFight move-info block, 0x80010294 - 0x80010588:
 *        menuFightDrawPP and menuFightDrawType.
 *
 * Standalone copy of the canonical implementations in menuFight.c, built with
 * the parent unit's GC/1.3 no-peephole flags. Both read the fight-menu window
 * work (four 0xC-byte move slots at +4, the fight Pokemon at +0x40).
 */
#include "dolphin/types.h"
#include "game/constants/pokemon_status.h"

extern void* menuSubCalcColor(void*, void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowDrawSprite();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSmsgGetRect();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmsgPrintColor();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void msgctrlSetValue();
#endif

/* Fight-menu window work: the four move slots shown by DrawPP/DrawType. */
typedef struct MenuFightMoveSlot {
    u32 valid;
    u32 unk_04;
    u16 type;
    u8 unk_0A;
    u8 unk_0B;
} MenuFightMoveSlot;

typedef struct MenuFightMoveWork {
    u32 unk_00;
    MenuFightMoveSlot slots[4];
    u8 unk_34[0xc];
    void* fightPokemon;
} MenuFightMoveWork;

/* menuFightDrawPP - 0x80010294 | size: 0x1e8 */
#ifdef TARGET_PC
/* PC: windowGetCursor returns the 2-byte cursor record (x at +0x94, y at +0x95) by value. MWCC hands it back
 * left-justified in r3, hence the GameCube's ">> 16"; x86-64 returns it in the low 16 bits, so the PC takes it as a
 * u16 with no shift. The union then sees the record's bytes in memory order on both. (Before 2026-10-18 the shift
 * left the cursor at 0, so the PP and type boxes always showed the first move.) */
extern u16 windowGetCursor(u32 val);
#define MENU_FIGHT_CURSOR(id) windowGetCursor(id)
#else
extern u32 windowGetCursor();
#define MENU_FIGHT_CURSOR(id) (windowGetCursor(id) >> 16)
#endif
void menuFightDrawPP(u8* ctx, u8* npc) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u8* windowGetAllocPtr(u8* a);
    extern u32 windowGetCursor(u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetPokemonPtr(void* obj);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(void* pokemon, s32 p2, s32 p3, s32 p4);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GSmsgPrintRight();
#endif
    union {
        u16 raw;
        struct {
            s8 x;
            s8 y;
        } pos;
    } cursor;
    MenuFightMoveWork* entries;
    u32 color;
    s32 x;
    u16 status;
    void* pokemon;

    entries = (MenuFightMoveWork*)windowGetAllocPtr(ctx);
    color = PC_PTR32(menuSubCalcColor(ctx, npc));
    cursor.raw = MENU_FIGHT_CURSOR(*(u32*)(ctx + 4));
    x = *(s16*)(npc + 0x54) - (s16)(GSmsgGetRect(0x197) >> 16);
    x = (s16)(x / 2);
    GSmsgPrintColor(x, 0, color, 0x197);

    if (cursor.pos.y >= 0 && cursor.pos.y < 4) {
        if (entries->slots[cursor.pos.y].valid != 0) {
            pokemon = fightOutPokemonGetPokemonPtr(entries->fightPokemon);
            if (pokemon != 0) {
                status = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, *(s8*)(ctx + 0x95));
            } else {
                status = 0;
            }
            switch (status) {
            case 0x164:
            case 0:
                GSmsgPrintRight(0, 0, x, *(s16*)(npc + 0x56), color, 0x2BE1);
                GSmsgPrintRight(0, 0, *(s16*)(npc + 0x54), *(s16*)(npc + 0x56), color, 0x2BE1);
                break;
            case 0x165:
                GSmsgPrintRight(0, 0, x, *(s16*)(npc + 0x56), color, 0x2B6D);
                GSmsgPrintRight(0, 0, *(s16*)(npc + 0x54), *(s16*)(npc + 0x56), color, 0x2B6D);
                break;
            default:
                msgctrlSetValue(0x34, entries->slots[cursor.pos.y].unk_0B);
                GSmsgPrintRight(0, 0, x, *(s16*)(npc + 0x56), color, 0xD2);
                msgctrlSetValue(0x34, entries->slots[cursor.pos.y].unk_0A);
                GSmsgPrintRight(0, 0, *(s16*)(npc + 0x54), *(s16*)(npc + 0x56), color, 0xD2);
                break;
            }
        }
    }
}

/* menuFightDrawType - 0x8001047C | size: 0x10c */
void menuFightDrawType(u8* arg1) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* windowGetAllocPtr(u8* a);
    extern u32 windowGetCursor(u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* fightOutPokemonGetPokemonPtr(void* obj);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 pokemonGetStatus(void* pokemon, s32 p2, s32 p3, s32 p4);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void windowDrawSprite(s32 p1, s32 p2, u8* p3, u16 p4, s32 p5);
#endif
    union {
        u16 raw;
        struct {
            s8 x;
            s8 y;
        } pos;
    } cursor;
    MenuFightMoveWork* participant;
    u16 status;
    u16 type;
    void* pokemon;

    participant = windowGetAllocPtr(arg1);
#ifdef TARGET_PC
    if (participant == NULL) {
        /* PC diagnostic (2026-10-17, temporary): Willie's battle crashed here at FIGHT reading address 0xE, the
         * move window's work area being empty. Log once and skip this draw so the cause can be seen; remove once
         * understood (hook-points.md "Diagnostics"). */
        static int reported;
        if (!reported) {
            extern void OSReport(const char*, ...);
            reported = 1;
            OSReport("[menuFight] DrawType: window %p has no work area (state %d, id %d, source +0x60 %08X)\n",
                     (void*)arg1, (int)(s8)arg1[1], (int)*(s32*)(arg1 + 4), (unsigned)*(u32*)(arg1 + 0x60));
        }
        return;
    }
#endif
    cursor.raw = MENU_FIGHT_CURSOR(*(u32*)(arg1 + 4));
    if (cursor.pos.y >= 0 && cursor.pos.y < 4) {
        if (participant->slots[cursor.pos.y].valid != 0) {
            pokemon = fightOutPokemonGetPokemonPtr(participant->fightPokemon);
            if (pokemon != 0) {
                status = pokemonGetStatus(pokemon, 0, POKEMON_STATUS_MOVE, *(s8*)(arg1 + 0x95));
            } else {
                status = 0;
            }
            switch (status) {
            case 0x164:
            case 0:
                type = 0;
                break;
            case 0x165:
                type = 0x5d;
                break;
            default:
                /* The move work's type-icon halfword is stored natively on the PC (2026-10-18 dump: +0C 0F0F004F, sprite
                 * 0x4F), so it needs no swap. */
                type = participant->slots[cursor.pos.y].type;
                break;
            }
#ifdef TARGET_PC
            {
                /* PC diagnostic (2026-10-17, temporary): the move type was beyond the menu sprite table, so
                 * menuSpriteBiosGetPtr returned NULL and windowDrawSprite read address 0xE. Dump the move work once
                 * and skip only the type icon. */
                extern u32 _menu_sprite_number; /* menu sprite count */
                extern void OSReport(const char*, ...);
                static int dumped;
                if (type != 0 && (u32)type >= _menu_sprite_number) {
                    if (!dumped) {
                        u32 n;
                        dumped = 1;
                        OSReport("[menuFight] DrawType: type %u >= %u sprites (slot %d, status %u); move work:\n",
                                 (unsigned)type, (unsigned)_menu_sprite_number, (int)cursor.pos.y, (unsigned)status);
                        for (n = 0; n < 0x48; n += 4) {
                            OSReport("[menuFight]   +%02X %08X\n", (unsigned)n, (unsigned)*(u32*)((u8*)participant + n));
                        }
                    }
                    type = 0;
                }
            }
#endif
            if (type != 0) {
                windowDrawSprite(0, PC_PTR(2), arg1, type, 0);
            }
        }
    }
}
