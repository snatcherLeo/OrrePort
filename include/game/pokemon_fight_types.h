/**
 * @file pokemon_fight_types.h
 * @brief Shared externs/typedefs for the fight-engine cluster split out of
 * the former game/pokemon.c bucket (0x801F000C-0x801F7F80).
 *
 * That bucket was mislabeled "pokemon" but is entirely the XD-era
 * fight-engine cluster: fightTarget/fightAction/fightFloor/fightFloorBios/
 * fightSide/fightSideBios/fightTrainer(head). This header centralizes the
 * file-scope externs and forward declarations that were previously
 * duplicated at the top of pokemon.c, so every split translation unit can
 * include one header instead of re-declaring cross-TU prototypes.
 */

#ifndef GAME_POKEMON_FIGHT_TYPES_H
#define GAME_POKEMON_FIGHT_TYPES_H

#include "game/pokemon.h"

/* =========================================================================
 * External function declarations
 * ========================================================================= */

/* dbgMenuFightGetMsgSpeedToFrame: GSthread_GetCurrentContext or similar - called from fightMainWaitFrame */
extern u32 dbgMenuFightGetMsgSpeedToFrame(void);

/* _threadSwitch: VSync/frame wait - called in frame loop */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void _threadSwitch(void);
#endif

/* fn_800D3088: Returns frame delta or similar timing value */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif

/**
 * FightFloorData: static floor-definition record used by the
 * fightFloorDataBios* accessors (game/fight_floor_bios.c). Table stride
 * confirmed at 0x18 bytes by fightFloorDataBiosGetPtr's index scaling and
 * corroborated by every fightFloorDataBiosGet/Set field offset in that
 * file (0x0, 0x2, 0x4, 0x8, 0xC, 0x10, 0x14/0x16).
 */
typedef struct FightFloorData {
    u8  tikeiDataId;         /* 0x0 */
    u16 floorDataId;         /* 0x2 */
    u32 name;                /* 0x4 */
    u32 syoukaiWzxDataId;    /* 0x8 */
    u32 bgmSndId;            /* 0xC */
    u32 envSndId;            /* 0x10 */
    u16 fightSideDataId[2];  /* 0x14, 0x16 */
} FightFloorData; /* size 0x18 */

/* fight_floor_data_number: pointer to the u32 entry count for the FightFloorData table */
extern u32 *fight_floor_data_number;
/* fight_floor_data: pointer to the base of the FightFloorData table */
extern FightFloorData *fight_floor_data;
/* lbl_8046DD90: singleton runtime FightFloor object (defined in
 * game/data/bss_8046D500.c); accessed as an opaque byte buffer by every
 * fightFloorBios* Get/Set accessor in game/fight_floor_bios.c. */
extern u8 lbl_8046DD90[0xA4E8];

/* fightFloorDataBiosGetPtr: Resolve floor id to static FightFloorData record */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern FightFloorData* fightFloorDataBiosGetPtr(u16 idx);
#endif

/* fightFloorBiosGetFightFloorPtr: Get pointer to the singleton FightFloor object */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* fightFloorBiosGetFightFloorPtr(void);
#endif

/* fightFloorGetNowPtr: Get Pokemon system context pointer */
extern struct Pokemon* fightFloorGetNowPtr(void);

/* fightOutPokemonGetFightActionPri: Pokemon nature/friendship comparison helper */
extern s32 fightOutPokemonGetFightActionPri(struct Pokemon* pokemon);

/* Forward declarations for fightFloorGetStatus asm wrapper */
extern u32 jumptable_803754AC[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiDataBiosGetStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiCheckWriteJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void joutaiIsJoutaiDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void pokemonGetStatus(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void std103_battleGetResult(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTrainerGetValidFightOutPokemonPtr(void);
#endif
extern u32 fightEncountGetEnvSndDataId(u16 encountDataIndex);
extern u32 fightEncountGetBgmSndDataId(u16 encountDataIndex);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightEncountDataBiosGetFightFloorDataId(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightEncountDataBiosGetTrainer(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightEncountDataBiosGetFightKind(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightEncountDataBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTypeGetFightSideFightOutPokemonMax(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTypeDataBiosGetFightoutPokemonNum(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTypeDataBiosGetEntryPokemonNum(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTypeDataBiosGetTrainerNum(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTypeDataBiosGetName(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightTypeDataBiosGetPtr(void);
#endif

#pragma pack(2)
typedef struct CopyBuf {
    u32 a;
    u32 b;
    u32 c;
    u32 d;
    u32 e;
    u32 f;
    u16 g;
} CopyBuf;
#pragma pack()

extern const CopyBuf lbl_80279C28;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetHostEnemyMsgFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetName(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetPokemonStatusMenuSubbarFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDarkpokemonHypermodeFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetMonohiroiFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDorobouFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetBossFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetKeikentihueruFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoItemSoubiTokukoutokubouupFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoHizukiMiyaburiFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoHizukiAiFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoCriticalAttackFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetGetInfectPokerusFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetGetFriendFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetGetNekoniKobanFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetGetOkaneFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetOkanePoolFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetGetExpFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDrawFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetNigeruFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetCallFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetUseItemFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoZukanTukamaetaFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoZukanMitaFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetDoBadgeCheckFlag(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetPtr(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fightKindDataBiosGetBackSaveDataFlag(void);
#endif

extern u32 encount_floor_id;

/* Forward declarations for converted accessor functions */
u16 fightFloorDataBiosGetFloorDataId(u8* ptr);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightFloorDataBiosGetName(u8* ptr);
#endif
u32 fightFloorDataBiosGetFightSideDataId(u8* ptr, u8 idx);

/* Forward declarations for functions referenced by fightFloorGetStatus dispatcher */
u8* fightFloorBiosGetFightOutPokemonPtrAryPtr(u8* ptr);
u32 fightFloorBiosGetFightOutPokemonPtrAry(u8* ptr, u16 idx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8* fightFloorBiosGetFightSidePtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8* fightFloorBiosGetJoutaiPtr(u8* ptr, u16 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightSideGetValidFightTrainerPtr(u32 param_1, u32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightSideCheckValid(u32 param_1);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void* fightTargetGetPtrAsNowFightType(u32 slotType, u32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightTargetGetPtr(u32 slotType, u32 ptr, u32 count);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightFloorLoopValidFightTrainer(u32, void (*)(u32, u32, u32), u32, u8);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightFloorSortFightOutPokemonPtrArySub(void*, u32*, u16, u8);
#endif
s32 fightFloorCmpfightOutPokemonNimbleness(void*, void*, void*, u8);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u8 fightFloorSetStatus(u32, u16, u32, u16, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightFloorDataBiosSetName(u8* ptr, u32 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightFloorDataBiosSetFloorDataId(u8* ptr, u16 val);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightSideRegistFightSideEnemyPokemonFightAll(u32, u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fightSideGetFightPokemonMaxHp(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
s32 fightSideGetFightPokemonNokoriHp(u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightSideInitAry(u32, u16);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightTrainerToMenuBallStatus(u8* ptr, u8* arr);
#endif
u32 fightTrainerGetTemotiNormalItemDataIdAry(u8* ptr, u16* out, u16 count, u8 mode);

#endif /* GAME_POKEMON_FIGHT_TYPES_H */
