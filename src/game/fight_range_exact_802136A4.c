/**
 * @file fight_range_exact_802136A4.c
 * @brief Strict fight-sequence bytecode/status island, 0x802136A4 - 0x80213A78.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the battle move scripts are big-endian disc data on the PC: operands read big-endian */

extern u8* lbl_8047B610;
extern u8 lbl_8047B614;

extern void fightOutPokemonInitOneSelfTurn(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void statusSetStatus(u8, u32, u32, u16, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 statusGetStatus(u8, u32, u32, u16, u32);
#endif

s32 _fightOutPokemonInitOneSelfTurnSub(void* p)
{
    fightOutPokemonInitOneSelfTurn(p);
    return 1;
}

void WS_STATUS_SET32(void)
{
    int pc = (int)PC_PTR32(lbl_8047B610);
    u32* p2 = DISC_PTR_AT(u32, PC_PTR(pc + 2));
    u8 id = *(u8*)PC_PTR((pc + 1));
    u16 value = DISC_U16(PC_PTR(pc + 0xa));
    u8* p6 = DISC_PTR_AT(u8, PC_PTR(pc + 6));
    u8* pC = DISC_PTR_AT(u8, PC_PTR(pc + 0xc));
    u32* p10 = DISC_PTR_AT(u32, PC_PTR(pc + 0x10));
    u32 a2 = p2 == NULL ? 0 : *p2;
    u32 a3 = p6 == NULL ? 0 : *p6;
    u32 a5 = pC == NULL ? 0 : *pC;

    statusSetStatus(id, a2, a3, value, a5, *p10);
    lbl_8047B610 += 0x14;
}

void WS_STATUS_SET16(void)
{
    int pc = (int)PC_PTR32(lbl_8047B610);
    u32* p2 = DISC_PTR_AT(u32, PC_PTR(pc + 2));
    u8 id = *(u8*)PC_PTR((pc + 1));
    u16 value = DISC_U16(PC_PTR(pc + 0xa));
    u8* p6 = DISC_PTR_AT(u8, PC_PTR(pc + 6));
    u8* pC = DISC_PTR_AT(u8, PC_PTR(pc + 0xc));
    u16* p10 = DISC_PTR_AT(u16, PC_PTR(pc + 0x10));
    u32 a2 = p2 == NULL ? 0 : *p2;
    u32 a3 = p6 == NULL ? 0 : *p6;
    u32 a5 = pC == NULL ? 0 : *pC;

    statusSetStatus(id, a2, a3, value, a5, *p10);
    lbl_8047B610 += 0x14;
}

void WS_STATUS_SET8(void)
{
    int pc = (int)PC_PTR32(lbl_8047B610);
    u32* p2 = DISC_PTR_AT(u32, PC_PTR(pc + 2));
    u8 id = *(u8*)PC_PTR((pc + 1));
    u16 value = DISC_U16(PC_PTR(pc + 0xa));
    u8* p6 = DISC_PTR_AT(u8, PC_PTR(pc + 6));
    u8* pC = DISC_PTR_AT(u8, PC_PTR(pc + 0xc));
    u8* p10 = DISC_PTR_AT(u8, PC_PTR(pc + 0x10));
    u32 a2 = p2 == NULL ? 0 : *p2;
    u32 a3 = p6 == NULL ? 0 : *p6;
    u32 a5 = pC == NULL ? 0 : *pC;

    statusSetStatus(id, a2, a3, value, a5, *p10);
    lbl_8047B610 += 0x14;
}

void WS_STATUS_GET32(void)
{
    int pc = (int)PC_PTR32(lbl_8047B610);
    u32* p2 = DISC_PTR_AT(u32, PC_PTR(pc + 2));
    u8 id = *(u8*)PC_PTR((pc + 1));
    u16 value = DISC_U16(PC_PTR(pc + 0xa));
    u8* p6 = DISC_PTR_AT(u8, PC_PTR(pc + 6));
    u8* pC = DISC_PTR_AT(u8, PC_PTR(pc + 0xc));
    u32* p10 = DISC_PTR_AT(u32, PC_PTR(pc + 0x10));
    u32 a2 = p2 == NULL ? 0 : *p2;
    u32 a3 = p6 == NULL ? 0 : *p6;
    u32 a5 = pC == NULL ? 0 : *pC;

    *p10 = statusGetStatus(id, a2, a3, value, a5);
#ifdef TARGET_PC
    if (id == 7 && value == 0x3E) {
        /* observe only (2026-10-27, P12's battle 208: the end-of-turn sandstorm damage wsCmd96 hands on is
         * read back here; the PC skipped the damage line) */
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        static int logGet = -1;
        if (logGet < 0) {
            logGet = getenv("ORRE_BATTLE_LOG") != 0;
        }
        if (logGet) {
            OSReport("[battle] status get32: kind 7 field 0x3E -> %u (into %p)\n", (unsigned)*p10, (void*)p10);
        }
    }
#endif
    lbl_8047B610 += 0x14;
}

void WS_STATUS_GET16(void)
{
    int pc = (int)PC_PTR32(lbl_8047B610);
    u32* p2 = DISC_PTR_AT(u32, PC_PTR(pc + 2));
    u8 id = *(u8*)PC_PTR((pc + 1));
    u16 value = DISC_U16(PC_PTR(pc + 0xa));
    u8* p6 = DISC_PTR_AT(u8, PC_PTR(pc + 6));
    u8* pC = DISC_PTR_AT(u8, PC_PTR(pc + 0xc));
    u16* p10 = DISC_PTR_AT(u16, PC_PTR(pc + 0x10));
    u32 a2 = p2 == NULL ? 0 : *p2;
    u32 a3 = p6 == NULL ? 0 : *p6;
    u32 a5 = pC == NULL ? 0 : *pC;

    *p10 = (u16)statusGetStatus(id, a2, a3, value, a5);
    lbl_8047B610 += 0x14;
}

void WS_STATUS_GET8(void)
{
    int pc = (int)PC_PTR32(lbl_8047B610);
    u32* p2 = DISC_PTR_AT(u32, PC_PTR(pc + 2));
    u8 id = *(u8*)PC_PTR((pc + 1));
    u16 value = DISC_U16(PC_PTR(pc + 0xa));
    u8* p6 = DISC_PTR_AT(u8, PC_PTR(pc + 6));
    u8* pC = DISC_PTR_AT(u8, PC_PTR(pc + 0xc));
    u8* p10 = DISC_PTR_AT(u8, PC_PTR(pc + 0x10));
    u32 a2 = p2 == NULL ? 0 : *p2;
    u32 a3 = p6 == NULL ? 0 : *p6;
    u32 a5 = pC == NULL ? 0 : *pC;

    *p10 = (u8)statusGetStatus(id, a2, a3, value, a5);
    lbl_8047B610 += 0x14;
}

void WS_GETEND(void)
{
    lbl_8047B614 = 1;
}

void WS_ITEMEND(void)
{
    lbl_8047B614 = 1;
}

u8* wsCmdF5(void)
{
    return lbl_8047B610++;
}

u8* wsCmdF4(void)
{
    return lbl_8047B610++;
}

u8* wsCmdF3(void)
{
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 5;
    return pc;
}

u8* wsCmdF2(void)
{
    return lbl_8047B610++;
}

u8* wsCmdF1(void)
{
    u8* pc = lbl_8047B610;
    lbl_8047B610 = pc + 5;
    return pc;
}
