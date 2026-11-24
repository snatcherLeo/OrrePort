/**
 * @file fight_target.c
 * @brief Fight-target lookup helpers.
 *
 * Split out of the former game/pokemon.c CodeCandidate bucket
 * (0x801F000C-0x801F7F80), which was mislabeled "pokemon" but is
 * entirely the XD-era fight-engine cluster. Address range covered by
 * this translation unit: 0x801F0058-0x801F0898 (9 functions), per
 * config/GC6E01/splits.txt.
 */

#include "game/pokemon_fight_types.h"
#include "pc/disc_types.h" /* the target table fight_target_data is raw big-endian data on the PC */

#if !defined(FIGHT_TARGET_EXACT_801F0058_ONLY) && \
    !defined(FIGHT_TARGET_801F0134_ONLY) && \
    !defined(FIGHT_TARGET_EXACT_801F0204_ONLY)
#define FIGHT_TARGET_ALL
#endif

#if defined(FIGHT_TARGET_ALL) || defined(FIGHT_TARGET_EXACT_801F0058_ONLY)
/* 0x801F0058 | size: 0x78 | small */
u32 fightTargetIsHostSide(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtr();
#endif
    u32 slotVal;
    u32 cmpVal;
    slotVal = fightTargetGetPtr(4, 0, param2);
    if (slotVal == 0)
        return 0;
    cmpVal = fightTargetGetPtr(2, param1, param2);
    if (cmpVal == 0)
        return 0;
    return (u32)__cntlzw(slotVal - cmpVal) >> 5;
}

/* 0x801F00D0 | size: 0x64 | small */
u32 fightTargetGetRelativeHostSideFightTargetIdToTragetPtr(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtr();
#endif
    u32 slot;
    u32 result;
    slot = fightTargetGetPtr(4, 0, param2);
    if (slot == 0)
        return 0;
    result = fightTargetGetPtr(param1, slot, param2);
    if (result == 0)
        result = 0;
    return result;
}
#endif

#if defined(FIGHT_TARGET_ALL) || defined(FIGHT_TARGET_801F0134_ONLY)
/* RULE-EXCEPTION(user-approved): single-use inline helper with
 * register-only evidence -- see docs/RULE_EXCEPTIONS.md. */
static inline u32 fightTargetSearchRelativeHostSideFightTargetId(u32 target, u32 hostSlot,
                                                                 u32 fightType) {
    extern u8 fight_target_data[];
    extern u32 fight_target_data_number;
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtr();
#endif
    u32 i;
    u32 index;
    u8* entry;
    u32 byte;
    u32 count;

    for (i = 0; (index = (i & 0xFFFF)) < (count = fight_target_data_number); i++) {
        if ((u16)i != 0) {
            entry = &fight_target_data[((u32)(u16)i) * 8];
            if (index >= count)
                entry = NULL;
            if (entry != NULL) {
                if (entry == NULL)
                    byte = 0;
                else
                    byte = entry[1];
                if ((u8)byte != 0) {
                    if ((void*)PC_PTR(fightTargetGetPtr(i, hostSlot, fightType)) == (void*)PC_PTR(target))
                        return i;
                }
            }
        }
    }
    return 0;
}

/* 0x801F0134 | size: 0xD0 | medium */
u32 fightTargetGetTragetPtrToRelativeHostSideFightTargetId(u32 param1, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtr();
#endif
    u32 slotBase;

    slotBase = fightTargetGetPtr(4, 0, param2);
    if (slotBase == 0)
        return 0;
    return fightTargetSearchRelativeHostSideFightTargetId(param1, slotBase, param2);
}
#endif

#if defined(FIGHT_TARGET_ALL) || defined(FIGHT_TARGET_EXACT_801F0204_ONLY)
/* Address: 0x801F0204 | Size: 0x18 | Pattern: nullcheck_getter */
u32 fightTargetDataBiosGetBuff(u8* ptr) {
    if (ptr == NULL) { return 0; }
    /* PC: read big-endian; natively a 1 (the second position on a side) read as 0x01000000, whose low half is 0, so
     * both positions got the first one's status menu (one HP box per side in Willie's double battle, 2026-10-17) */
    return DISC_U32(&ptr[0x4]);
}

/* Address: 0x801F021C | Size: 0x18 | Pattern: nullcheck_getter */
u8 fightTargetDataBiosGetStatusKid(u8* ptr) {
    if (ptr == NULL) { return 0; }
    return *(u8*)(&ptr[0x0]);
}

/* 0x801F0234 | size: 0x28 | small */
u8* fightTargetDataBiosGetPtr(u32 index) {
    extern u8 fight_target_data[];
    extern u32 fight_target_data_number;
    u8* entry = &fight_target_data[((u32)(u16)index) * 8];
    if ((u16)index < fight_target_data_number)
        return entry;
    return NULL;
}

/* 0x801F025C | size: 0x50 | small */
void* fightTargetGetPtrAsNowFightType(u32 slotType, u32 idx) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTargetGetPtr();
#endif
    u32 count;
    count = (u16)fightFloorGetStatus(0, 0, 0x14, 0);
    return (void*)PC_PTR(fightTargetGetPtr(slotType, idx, count));
}

/* 0x801F02AC | size: 0x46C | large */
u32 fightTargetGetPtr(u32 slotType, u32 ptr, u32 count) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus(u32, u32, u32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetFightoutPokemonNum(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetTrainerNum(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetPtr(u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightSideGetStatus(u32, u32, u32, u32);
#endif
    extern u32 _fightTargetGetTargetPtrToFightSidePtr__FPvUs(u32, u32);
    u32 slots[2];
    u32 sidePkm[4];
    u32 pkmAbil[8];
    u32 ctx;
    u32 ctxObj;
    u8 numSides;
    u32 i;
    u32 n;
    u32 k;
    u32 m;
    u32 j;
    u32 savedPtr;
    u8 flag;

    if ((u16)slotType == 0) {
        return 0;
    }
    ctx = PC_PTR32(fightFloorGetNowPtr());
    if ((u16)slotType == 1) {
        return ctx;
    }

    switch ((u16)slotType) {
    case 0x11: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x36, 0);
    case 0x12: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x42, 0);
    case 0x13: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x44, 0);
    case 0x14: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x45, 0);
    case 0x15: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x46, 0);
    case 0x16: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x47, 0);
    case 0x17: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x48, 0);
    case 0x18: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x49, 0);
    case 0x19: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x4b, 0);
    case 0x1a: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x4c, 0);
    case 0x1b: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x4d, 0);
    case 0x1c: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x4e, 0);
    case 0x1d: return fightFloorGetStatus(PC_PTR(ctx), 0, 0x4f, 0);
    }

    ctxObj = PC_PTR32_IMPLICIT(fightTypeDataBiosGetPtr(count));
    if (ctxObj == 0) {
        return 0;
    }
    if (ptr != 0) {
        savedPtr = ptr;
        ptr = _fightTargetGetTargetPtrToFightSidePtr__FPvUs(ptr, count);
        if (ptr == 0) {
            return 0;
        }
    }
    n = 0;
    m = 0;
    count = fightTypeDataBiosGetTrainerNum(PC_PTR(ctxObj));
    ctxObj = (u8)fightTypeDataBiosGetFightoutPokemonNum(PC_PTR(ctxObj));
    numSides = (u8)(u64)count;
    i = 0;
    while ((u16)i < 2) {
        slots[(u16)i] = fightFloorGetStatus(PC_PTR(ctx), 0, 0x35, i);
        if ((u16)slotType == 4 && (u16)i == 0) {
            return slots[(u16)i];
        }
        if ((u16)slotType == 5 && (u16)i == 1) {
            return slots[(u16)i];
        }
        if ((void*)PC_PTR(slots[(u16)i]) == (void*)PC_PTR(ptr)) {
            flag = 1;
            if ((u16)slotType == 2) return slots[(u16)i];
        } else {
            flag = 0;
            if ((u16)slotType == 3) return slots[(u16)i];
        }
        j = 0;
        count = 0;
        while ((s32)(u16)count < (s32)numSides) {
            sidePkm[(u16)n] = fightSideGetStatus(PC_PTR(slots[(u16)i]), 0, 7, count);
            if ((u16)slotType == 0xb && (u16)i == 0 && (u16)count == 0) {
                return sidePkm[(u16)n];
            }
            if (flag == 1) {
                if ((u16)slotType == 6 && (u16)count == 0) return sidePkm[(u16)n];
                if ((u16)slotType == 7 && (u16)count == 1) return sidePkm[(u16)n];
                if ((u16)slotType == 8) {
                    if (savedPtr != sidePkm[(u16)n]) return sidePkm[(u16)n];
                }
            } else {
                if ((u16)slotType == 9 && (u16)count == 0) return sidePkm[(u16)n];
                if ((u16)slotType == 0xa && (u16)count == 1) return sidePkm[(u16)n];
            }
            k = 0;
            while ((s32)(u16)k < (s32)ctxObj) {
                pkmAbil[(u16)m] = fightTrainerGetStatus(PC_PTR(sidePkm[(u16)n]), 0, 0x46, k);
                if (flag == 1) {
                    if ((u16)slotType == 0xc && (u16)j == 0) return pkmAbil[(u16)m];
                    if ((u16)slotType == 0xd && (u16)j == 1) return pkmAbil[(u16)m];
                    if ((u16)slotType == 0xe) {
                        if (savedPtr != pkmAbil[(u16)m]) return pkmAbil[(u16)m];
                    }
                } else {
                    if ((u16)slotType == 0xf && (u16)j == 0) return pkmAbil[(u16)m];
                    if ((u16)slotType == 0x10 && (u16)j == 1) return pkmAbil[(u16)m];
                }
                m++;
                j++;
                k++;
            }
            n++;
            count++;
        }
        i++;
    }
    return 0;
}

/* 0x801F0718 | size: 0x180 | medium */
u32 _fightTargetGetTargetPtrToFightSidePtr__FPvUs(u32 target, u32 param2) {
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightFloorGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightSideGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTrainerGetStatus();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetFightoutPokemonNum();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetEntryPokemonNum();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetTrainerNum();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32 fightTypeDataBiosGetPtr();
#endif
    u32 base;
    u32 group;
    u32 numSub1r;
    u32 numSlotsr;
    u32 numSub2r;
    u32 k;
    u32 sub;
    u32 slot;
    u32 side;
    u32 j;
    base = fightFloorGetStatus(0, 0, 0, 0);
    group = PC_PTR32_IMPLICIT(fightTypeDataBiosGetPtr(param2));
    if (group == 0)
        return 0;
    numSlotsr = fightTypeDataBiosGetTrainerNum(PC_PTR(group));
    numSub1r = fightTypeDataBiosGetFightoutPokemonNum(PC_PTR(group));
    numSub2r = fightTypeDataBiosGetEntryPokemonNum(group);
    numSub2r = (u8)(u64)numSub2r;
    numSub1r = (u8)(u64)numSub1r;
    numSlotsr = (u8)(u64)numSlotsr;
    k = 0;
    while ((u16)k < 2) {
        slot = fightFloorGetStatus(PC_PTR(base), 0, 0x35, k);
        if (slot == target)
            return slot;
        sub = 0;
        while ((s32)(u16)sub < (s32)numSlotsr) {
            side = fightSideGetStatus(PC_PTR(slot), 0, 7, sub);
            if (side == target)
                return slot;
            j = 0;
            while ((s32)(u16)j < (s32)numSub2r) {
                if ((void*)fightTrainerGetStatus(PC_PTR(side), 0, 0x45, j) == (void*)PC_PTR(target))
                    return slot;
                j++;
            }
            j = 0;
            while ((s32)(u16)j < (s32)numSub1r) {
                if ((void*)fightTrainerGetStatus(PC_PTR(side), 0, 0x46, j) == (void*)PC_PTR(target))
                    return slot;
                j++;
            }
            sub++;
        }
        k++;
    }
    return 0;
}
#endif
