#ifndef GAME_FIGHT_ACTION_H
#define GAME_FIGHT_ACTION_H

#include "dolphin/types.h"

typedef u32 (*FightActionCallback)(void* action);

u32 fightActionFlowNullFunc(void* action);
u32 fightActionFlowKaijou(void* action);
u32 fightActionFlowKaisiPre(void* action);
u32 fightActionFlowKaisiNyuujouTrainer(void* action);
u32 fightActionFlowKaisiNyuujouPokemon(void* action);
u32 fightActionFlowKaisiPost(void* action);
u32 fightActionFlowTenkouInit(void* action);
u32 fightActionFlowAllFightTrainerSelectFightAction(void* action);
u32 fightActionFlowFightNigeru(void* action);
u32 fightActionFlowFightOutPokemonIrekae(void* action);
u32 fightActionFlowFightTrainerCall(void* action);
u32 fightActionFlowAllFightOutPokemonDoFightAction(void* action);
u32 fightActionFlowWazaKiaipantiPre(void* action);
u32 fightActionFlowOneTurnPost(void* action);
u32 fightActionFlowSyuuryouPre(void* action);
u32 fightActionFlowSyuuryou(void* action);
u32 fightActionFlowSyuuryouPost(void* action);
u32 fightActionFlowHeijou(void* action);
u32 fightActionFlowFightTrainerUseItem(void* action);
u32 fightActionFlowFightOutPokemonOutWaza(void* action);

u32 fightActionDispNullFunc(void* action);
u32 fightActionDispKaijou(void* action);
u32 fightActionDispKaisiPre(void* action);
u32 fightActionDispKaisiNyuujouTrainer(void* action);
u32 fightActionDispKaisiNyuujouPokemon(void* action);
u32 fightActionDispKaisiPost(void* action);
u32 fightActionDispTenkouInit(void* action);
u32 fightActionDispAllFightTrainerSelectFightAction(void* action);
u32 fightActionDispFightNigeru(void* action);
u32 fightActionDispFightOutPokemonIrekae(void* action);
u32 fightActionDispFightTrainerCall(void* action);
u32 fightActionDispAllFightOutPokemonDoFightAction(void* action);
u32 fightActionDispWazaKiaipantiPre(void* action);
u32 fightActionDispOneTurnPost(void* action);
u32 fightActionDispSyuuryouPre(void* action);
u32 fightActionDispSyuuryou(void* action);
u32 fightActionDispSyuuryouPost(void* action);
u32 fightActionDispHeijou(void* action);
u32 fightActionDispFightTrainerUseItem(void* action);
u32 fightActionDispFightOutPokemonOutWaza(void* action);

typedef struct FightActionKindData {
    /* 0x00 */ u8 rawPriority;
    /* 0x01 */ u8 pad_01[3];
    /* 0x04 */ FightActionCallback flow;
    /* 0x08 */ FightActionCallback display;
} FightActionKindData;

extern FightActionKindData fight_action_kind_data[20];

typedef struct FightActionData {
    /* 0x00 */ u16 kind;
    /* 0x02 */ u8 pad_02[2];
    /* 0x04 */ u32 buff;
} FightActionData;

/* The pointer fields are 32-bit slots on the PC (pc/ptr32.h, PTR32_FIELD; read with P32, write with P32_SET): a
 * FightAction is embedded in each battle Pokémon's record at +0x62C, right before its two types (+0x65C) and ability
 * (+0x660), so it must keep the GameCube's 0x30 bytes. 2026-10-19 (verified): with 8-byte native pointers it was
 * 0x48 bytes, and fightActionInit's reset at every turn start wrote fifoIndex = -1 over both types and displayBuff over
 * the ability: every type check then saw "no type" (no super effective, no immunities; snatcherLeo's Shadow Ball /
 * Thunderbolt / Brick Break reports). */
typedef struct FightAction {
    /* 0x00 */ u16 kind;
    /* 0x02 */ u8 pad_02[2];
    /* 0x04 */ u32 buff;
    /* 0x08 */ PTR32_FIELD(FightActionData) data;
    /* 0x0C */ PTR32_FIELD(void) buffData;
    /* 0x10 */ u32 buffDataId;
    /* 0x14 */ PTR32_FIELD(void) actorTarget;
    /* 0x18 */ PTR32_FIELD(void) motoActionData;
    /* 0x1C */ s32 fifoIndex;
    /* 0x20 */ u32 displayBuff[4];
} FightAction;
#ifdef TARGET_PC
_Static_assert(sizeof(FightAction) == 0x30, "FightAction keeps the GameCube layout (embedded in the battle record)");
#endif

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetFifoBanme(FightAction* action, s32 fifoIndex);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetMotoFightActionDataPtr(FightAction* action, void* motoData);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetBuffDataId(FightAction* action, u32 buffDataId);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetBuffDataPtr(FightAction* action, void* buffData);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetActorFightTargetPtr(FightAction* action, void* actorTarget);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetFightActionDataPtr(FightAction* action, FightActionData* data);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetBuff(FightAction* action, u32 buff);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetKind(FightAction* action, u32 kind);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void fightActionBiosSetDispBuff(FightAction* action, u32 index, u32 value);
#endif
FightActionCallback fightActionKindDataBiosGetDispFuncPtr(FightActionKindData* data);
FightActionCallback fightActionKindDataBiosGetFlowFuncPtr(FightActionKindData* data);
s32 fightActionKindDataBiosGetPri(FightActionKindData* data);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
FightActionKindData* fightActionKindDataBiosGetPtr(u16 index);
#endif
u32 fightActionDataBiosGetBuff(FightActionData* data);
u16 fightActionDataBiosGetKind(FightActionData* data);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
FightActionData* fightActionBiosGetFightActionDataPtr(FightAction* action);
#endif
void* fightActionBiosGetActorFightTargetPtr(FightAction* action);
void* fightActionBiosGetBuffDataPtr(FightAction* action);
u32 fightActionBiosGetBuffDataId(FightAction* action);
u32 fightActionBiosGetBuff(FightAction* action);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u16 fightActionBiosGetKind(FightAction* action);
#endif
u16 fightActionGetKindDataId(void* action);
s32 fightActionGetPri(void* action);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 fightActionCheckValid(void* action);
#endif
s32 fightActionCreate(FightAction* action, void* motoAction, void* actorTarget,
                      u32 kind, u32 buff, FightActionData* actionData);
u32 fightActionFlowFifo(void* action);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
void* fightOutPokemonCreateFightActionAttackWaza(
    void* ctx, void* motoAction, u32 kind, u32 buff,
    FightActionData* actionData, u32 moveId, u32 targetId, u32 slot, u8 mode);
#endif

#endif
