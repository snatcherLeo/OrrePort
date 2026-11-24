/* Name-entry entry point, 0x80028FBC-0x80029558.
 * RULE-EXCEPTION(user-approved): retained widened choice local;
 * see docs/RULE_EXCEPTIONS.md. Compiler controls are at unit scope.
 */
#include "dolphin/types.h"
#include "pc/disc_types.h" /* the menu item table (lbl_802EF0A8) is big-endian on the PC */
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_NameEntryCommit: OrrePort's full names (PC only) */
#endif

typedef struct NameEntryChoiceList { /* one record of the name-entry kind table lbl_80266DD8 (16 bytes on the PC too) */
    u8 pad00[8];
    PTR32_FIELD(s32) choices; /* a 32-bit slot on the PC (rodata_80266D78.c, corrected 2026-10-21) */
    s32 count;
} NameEntryChoiceList;

void menuNameEntry(void) {
    /* --- module globals (block-scope typed externs, TU convention) --- */
    extern u8  lbl_803A2068[];     /* GSmap context block            */
    extern u8  lbl_80266DC0[];     /* map data blob                  */
    extern u8  lbl_802EF0A8[];     /* far read-only data blob        */
    extern u8  lbl_803A2094[];     /* menu-model handle              */
    extern s32 lbl_804788A0;       /* first-frame latch flag         */
    extern u32 lbl_8047A3C0;  /* canonical; per-site reinterpret cast */
    extern u32 lbl_8047A3BC;  /* canonical; per-site reinterpret cast */
    extern u32 lbl_8047A3B8;  /* canonical; per-site reinterpret cast */
    extern u32 lbl_8047A3B4;  /* canonical; per-site reinterpret cast */
    extern u32 lbl_8047A3B0;  /* canonical; per-site reinterpret cast */
    extern f32 lbl_8047B940;       /* 0.0f constant                  */

    /* --- callees (minimal real signatures inferred from each bl site) --- */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32  GSmsgGetGSchar(u32 id);                       /* id -> resource ptr        */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32  heroGetStatus(u8* ptr, u32 selector, u32 idx);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32  pokemonCheckValid();                         /* returns u8 status         */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32  pokemonBiosGetNicknamePtr(u32 a);
#endif
    extern u32  pcboxGetPokemonBoxName(s32 a, s32 b);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void GScharCpy(void* dst, u8* src);           /* copy/build name struct    */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* menuItemBiosGetPtr(s32 id);                      /* returns struct ptr        */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuModelInit(void* handle, s16 a, s16 b);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonModelRequestById(void* handle, s32 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void* peopleInfoBiosGetPtr(s32 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_8018F4C8(void* info, s32 a, s32* outA, s32* outB);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuModelSetMotion(void* handle, s32 motion);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuPokemonModelRequest(void* handle, u32 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuModelCheck(void* handle, s32 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuModelTerm(void* handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fadeSet(s32 mode, f32 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fadeCheck(s32 v);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32  windowGetActiveID(void);                         /* returns context handle    */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32  menuOpenCustom(s32 id, u32 ctx, s32 a, s32 b, s32 c, s32 d, ...);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern u32  soundStartSE(s32 size);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void msgctrlSetValue(s32 id, u32 name);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgOpen(s32 a, s32 b, s32 c, s32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s8   menuSubOpenYesNo(s32 a, s32 b, s32 c, s32 d);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void winMsgClose(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuClose(s32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void menuCloseSync(s32 id, s32 a);
#endif
    extern s32  inputName__FPUsPUsiii(u8* ctx, void* nameBuf, s32 mode, s32 subIndex, s32 last);
    extern void heroSetStatus(s32 a, s32 b, u8* ctx);
    extern void pokemonBiosSetNicknamePtr(u32 v, u8* ctx);
    extern void pcboxSetPokemonBoxName(s32 a, s32 name, u8* ctx);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern s32  GScharCmp(u8* ctx, void* nameBuf);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorBattleEndTerminateScripts(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void floorSetFadeScript(s32 a, u32 b);
#endif

    u8* ctx;        /* lbl_803A2068 context block          */
    u8* data;       /* lbl_80266DC0 map data blob          */
    NameEntryChoiceList* entries;
    u8* ctx2;
    s32 mode;       /* ctx +0x18                           */
    s32 subIndex;   /* ctx +0x1c                           */
    s32 r0;         /* generic selection result            */
    u32 sel;        /* selection / pokemon handle          */
    s32 ok;
    s32* listPtr;
    s32 accepted;
    s32 count;
    u32 pkm;
    s32* listp;
    void* mdl;      /* struct ptr from menuItemBiosGetPtr          */
    s32 entryBuf[4];
    u16 nameBuf[12];
    s32 menuArg[2];
    s32 listArg[2];
    s32 motOut;     /* fn_8018F4C8 out word @ sp+0xc        */
    s32 motTmp;     /* fn_8018F4C8 out word @ sp+0x8        */
    s32 ans;
#ifdef TARGET_PC
    s32 canLeave = 0; /* B may leave the name menu (OrreHook_NameEntryListBack) */
    s32 left = 0;     /* B left it: nothing is stored */
#endif

    data = lbl_80266DC0;

    /* First-frame latch: copy fixed display params out of the big data blob. */
    if (lbl_804788A0 != 0) {
        lbl_804788A0 = 0;
        (*(s32*)&lbl_8047A3C0) = (s32)DISC_S16(lbl_802EF0A8 + 0x6fca);
        (*(s32*)&lbl_8047A3BC) = (s32)DISC_S16(lbl_802EF0A8 + 0x6fcc);
        (*(s32*)&lbl_8047A3B8) = (s32)DISC_S16(lbl_802EF0A8 + 0x20ec2);
        (*(s32*)&lbl_8047A3B4) = (s32)DISC_S16(lbl_802EF0A8 + 0x73ba);
        (*(s32*)&lbl_8047A3B0) = (s32)DISC_S16(lbl_802EF0A8 + 0x70fe);
    }

    ctx = lbl_803A2068;
    mode = *(s32*)(ctx + 0x18);
    subIndex = *(s32*)(ctx + 0x1c);

    /* --- Resolve the current selection (r3) from the mode switch. --- */
    sel = 0;
    switch (mode) {
    case 0:
        sel = PC_PTR32_IMPLICIT(GSmsgGetGSchar(*(u32*)(data + 0x0)));
        break;
    case 1:
#ifdef TARGET_PC /* PC: data + 0xC is the next symbol (lbl_80266DCC), not adjacent here (pastend class) */
        { extern u32 lbl_80266DCC[]; sel = PC_PTR32_IMPLICIT(GSmsgGetGSchar(lbl_80266DCC[0])); }
#else
        sel = PC_PTR32_IMPLICIT(GSmsgGetGSchar(*(u32*)(data + 0xc)));
#endif
        break;
    case 2:
        r0 = (s32)heroGetStatus(0, 3, (u16)subIndex);
        if ((pokemonCheckValid(PC_PTR(r0)) & 0xff) == 0) { /* r3 = heroGetStatus's result (assembly) */
            sel = 0;
        } else {
            sel = PC_PTR32_IMPLICIT(pokemonBiosGetNicknamePtr((u32)r0));
        }
        break;
    case 3:
        sel = pcboxGetPokemonBoxName(0, (s32)(s8)subIndex);
        break;
    default:
        sel = 0;
        break;
    }

    /* Build the name buffer from the selection; if none, zero the head. */
    if (sel == 0) {
        ok = 0;
    } else {
        GScharCpy(nameBuf, (u8*)PC_PTR(sel));
        ok = 1;
    }
    if (ok == 0) {
        *(u16*)nameBuf = 0;
    }

    /* Snapshot four words of the data header into a scratch frame array
       (these are indexed by 'mode' below). */
    mode = *(s32*)(ctx + 0x18);
#ifdef TARGET_PC /* PC: data + 0x88..0x94 is lbl_80266E18 + 0x30..0x3C, a raw big-endian table here (pastend class) */
    {
        extern u8 lbl_80266E18[];
        entryBuf[0] = DISC_S32(lbl_80266E18 + 0x30);
        entryBuf[1] = DISC_S32(lbl_80266E18 + 0x34);
        entryBuf[2] = DISC_S32(lbl_80266E18 + 0x38);
        entryBuf[3] = DISC_S32(lbl_80266E18 + 0x3C);
    }
#else
    entryBuf[0] = *(s32*)(data + 0x88);
    entryBuf[1] = *(s32*)(data + 0x8c);
    entryBuf[2] = *(s32*)(data + 0x90);
    entryBuf[3] = *(s32*)(data + 0x94);
#endif
    subIndex = *(s32*)(ctx + 0x1c);

    /* Fetch the model descriptor (0xd3a for mode 2, else 0xd39) and pose it. */
    mdl = menuItemBiosGetPtr((mode == 2) ? 0xd3a : 0xd39);
    menuModelInit(lbl_803A2094, DISC_S16((u8*)mdl + 0x6), DISC_S16((u8*)mdl + 0x8));

    /* --- Per-mode model setup. --- */
    switch (mode) {
    case 0:
    case 1:
        sel = entryBuf[mode];
        menuPokemonModelRequestById(lbl_803A2094, sel);
        fn_8018F4C8(peopleInfoBiosGetPtr(PC_PTR(sel)), 1, &motOut, &motTmp);
        menuModelSetMotion(lbl_803A2094, motOut);
        break;
    case 2:
        sel = heroGetStatus(0, 3, (u16)subIndex);
        if ((pokemonCheckValid(PC_PTR(sel)) & 0xff) != 0) {
            menuPokemonModelRequest(lbl_803A2094, PC_PTR(sel));
        }
        break;
    case 3:
        menuPokemonModelRequestById(lbl_803A2094, entryBuf[mode]);
        break;
    default:
        break;
    }
    menuModelCheck(lbl_803A2094, 1);

    /* --- Open the primary menu window (id 0x6f) seeded with the context. --- */
    menuArg[0] = *(s32*)(ctx + 0x18);
    menuArg[1] = *(s32*)(ctx + 0x1c);
    menuOpenCustom(0x6f, windowGetActiveID(), 0, 0, 1, 1, menuArg);

    /* On async/render frames, prime the transition. */
    ctx2 = lbl_803A2068;
    if (*(s32*)(ctx2 + 0x28) == 0) {
        fadeSet(2, lbl_8047B940);
        fadeCheck(1);
    }

    /* --- List/confirm loop over the current mode's entry array. --- */
#ifdef TARGET_PC /* PC: data + 0x18 is lbl_80266DD8, compiled as native structs here (pastend class) */
    { extern u8 lbl_80266DD8[]; entries = (NameEntryChoiceList*)lbl_80266DD8; }
#else
    entries = (NameEntryChoiceList*)(data + 0x18);
#endif
    if (entries[*(s32*)(ctx + 0x18)].count > 0) {
#ifdef TARGET_PC
        /* OrreHook_NameEntryListBack (orre_hooks.h), asked once as the name menu opens: may B leave it? (Wes's from
         * New Game, snatcherLeo 2026-11-15; the rule per kind is OrrePort's.) Windows, Linux and macOS alike. NULL or 0: B
         * does nothing on the name menu, as on the GameCube */
        canLeave = OrreHook_NameEntryListBack != NULL &&
                   OrreHook_NameEntryListBack(*(s32*)(ctx + 0x18), *(s32*)(ctx + 0x1c), OrreHook_NameEntryCaller) != 0;
#endif
        listp = listArg;
        for (;;) {
#ifdef TARGET_PC
            NameEntryChoiceList* entry = entries;
#else
            NameEntryChoiceList* entry = (NameEntryChoiceList*)(data + 0x18);
#endif
            s64 choiceName;
            entry += *(s32*)(ctx + 0x18);
            count = entry->count;
            listPtr = P32(entry->choices);
            for (;;) {
                s32 pick;
                listArg[0] = (s32)PC_PTR32(listPtr);
                listArg[1] = count;
#ifdef TARGET_PC
                /* the list's button callback ignores B (menuNameEntrySelectButton: the game's own, B on the name menu
                 * does nothing). While this list may be left, OrreHook_NameMenuCanLeave lets its B close it with -1,
                 * for as long as the list is open (menuOpenCustom returns when it closes) */
                OrreHook_NameMenuCanLeave = canLeave;
                pick = menuOpenCustom(0x70, windowGetActiveID(), 0, 0, 1, 1, listp);
                OrreHook_NameMenuCanLeave = 0;
#else
                pick = menuOpenCustom(0x70, windowGetActiveID(), 0, 0, 1, 1, listp);
#endif
                if (pick == 0) {
                    soundStartSE(0x24);
                    accepted = 0;
                    break;
                }
                if (pick == -1) {
#ifdef TARGET_PC
                    /* B on the name menu (only possible when canLeave let it through, above): leave the screen. The
                     * list closes below as for a pick, and no keyboard opens */
                    if (canLeave) {
                        left = 1;
                        accepted = 0;
                        break;
                    }
#endif
                    continue;
                }
                choiceName = PC_PTR32(GSmsgGetGSchar((u32)listPtr[pick - 1]));
                {
                    s32 yes;
                    soundStartSE(0x440);
                    msgctrlSetValue(0x4d, choiceName);
                    winMsgOpen(2, 0x2ef6, 1, 0);
                    ans = menuSubOpenYesNo(0, -1, -1, 0);
                    winMsgClose(1);
                    if (ans == 1 || ans == -1) {
                        yes = 0;
                    } else {
                        yes = 1;
                    }
                    if (yes == 0) {
                        continue;
                    }
                }
                accepted = 1;
                break;
            }
            menuClose(0x70);
            menuCloseSync(0x70, 1);
            {
                s32 copied;
                if (accepted) {
                    GScharCpy(lbl_803A2068, (u8*)choiceName);
                    copied = 1;
                } else {
                    copied = 0;
                }
                if (copied) {
                    break;
                }
            }
#ifdef TARGET_PC
            if (left) {
                break; /* left by B: straight to the teardown, no keyboard */
            }
#endif
            if (inputName__FPUsPUsiii(lbl_803A2068, nameBuf, *(s32*)(ctx + 0x18), *(s32*)(ctx + 0x1c), 0) != 0) {
                break;
            }
        }
    } else {
        inputName__FPUsPUsiii(lbl_803A2068, nameBuf, *(s32*)(ctx + 0x18), *(s32*)(ctx + 0x1c), 1);
    }

    /* --- Tear down the menu model and finalize the selection by mode. --- */
    fadeSet(3, lbl_8047B940);
    fadeCheck(1);
    menuModelTerm(lbl_803A2094);
    menuClose(0x6f);
    menuCloseSync(0x6f, 1);

    mode = *(s32*)(ctx + 0x18);
    subIndex = *(s32*)(ctx + 0x1c);
#ifdef TARGET_PC
    if (left) {
        /* left by B: no name is stored for any mode and OrrePort's commit hook isn't told. The request block's name is
         * cleared (a name typed before a NO is still in it: the keyboard types straight into it) and the result says
         * "left" (ORRE_NAME_ENTRY_LEFT; the game writes only 0 or 1), which the caller that asked for it reads
         * (OrreHook_NameEntryCaller; the title's New Game goes back to the main menu). The room still pops below */
        *(u16*)lbl_803A2068 = 0;
        *(s32*)(lbl_803A2068 + 0x20) = ORRE_NAME_ENTRY_LEFT;
        goto finish;
    }
#endif
    switch (mode) {
    case 0:
        break;
    case 1:
        heroSetStatus(0, 0x17, lbl_803A2068);
        break;
    case 2:
        pkm = heroGetStatus(0, 3, (u16)subIndex);
        if ((pokemonCheckValid(PC_PTR(pkm)) & 0xff) != 0) {
            pokemonBiosSetNicknamePtr(pkm, lbl_803A2068);
        }
        break;
    case 3:
        pcboxSetPokemonBoxName(0, (s32)(s8)subIndex, lbl_803A2068);
        break;
    default:
        break;
    }
#ifdef TARGET_PC
    /* OrrePort keeps the full name beside the stored GameCube name (orre_hooks.h; observation only) */
    if (OrreHook_NameEntryCommit != NULL) {
        OrreHook_NameEntryCommit(mode, subIndex, lbl_803A2068, mode == 2 ? (void*)PC_PTR(pkm) : NULL);
    }
#endif

    /* result = (name changed) ? 1 : 0 */
    if (GScharCmp(lbl_803A2068, nameBuf) == 0) {
        *(s32*)(lbl_803A2068 + 0x20) = 0;
    } else {
        *(s32*)(lbl_803A2068 + 0x20) = 1;
    }

#ifdef TARGET_PC
finish:
#endif
    /* Async finalize: post the appropriate completion event. */
    if (*(s32*)(ctx2 + 0x28) != 0) {
        floorBattleEndTerminateScripts();
        if (*(s32*)(lbl_803A2068 + 0x24) != 0) {
            floorSetFadeScript(0, 0x05960008);
        } else {
            floorSetFadeScript(0, 0);
        }
    }
}
