/** Byte-exact PDA model-texture renderer, 0x800492CC - 0x800495C8. */
#include "dolphin/types.h"

typedef struct PdaSprite {
    u8 pad00[0x54];
    s16 x;
    s16 y;
} PdaSprite;

extern u8 lbl_803A6818[];
extern u8 lbl_80314F98[];
extern s32 lbl_804788C0;
extern s16 lbl_804788C8[2];
extern s16 lbl_804788CC[2];
extern s16 lbl_804788D0[2];

#ifdef TARGET_PC
/* PC (port-gaps 61, the reference playthrough P02, spotted by snatcherLeo): these are separate 2-byte symbols (symbols.txt), each an x with its
 * y in the next symbol (0x804788CE, 0x804788D2) or, for the position itself, in unnamed bytes after it. On the
 * GameCube `[1]` reaches the y next door; on the PC each symbol stands alone, so `[1]` read something else (the
 * memo's list / Detail / Size models sat about 40 lines too high: the console's y offsets are 40 and 64) and wrote
 * past lbl_804788C8. So the PC reads the y halves by their own names and keeps the position in a pair of its own. */
extern s16 lbl_804788CE;
extern s16 lbl_804788D2;
static s16 s_pdaModelPos[2];
#define PDA_POS s_pdaModelPos
#define PDA_LIST_Y lbl_804788CE
#define PDA_SIZE_Y lbl_804788D2
#else
#define PDA_POS lbl_804788C8
#define PDA_LIST_Y lbl_804788CC[1]
#define PDA_SIZE_Y lbl_804788D0[1]
#endif
extern f32 lbl_8047BCA0;
extern f32 lbl_8047BC94;
extern f32 lbl_8047BCBC;

extern u32 menuModelRender();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxVtxTexCoord();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxVtxColor4u8();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxVtxPos2s16();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxEnd();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxBegin();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxSetPrimitive();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxSetVtxDesc();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxLoadTexture();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxClearFlags();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 gfxSetFlags();
#endif

/* RULE-EXCEPTION(user-approved): local compiler-control pragma (peephole off) needed for the byte match of the code it covers - see docs/RULE_EXCEPTIONS.md */
#pragma peephole off
void fn_800492CC(u8* context, PdaSprite* sprite)
{
    u32 firstTexture;
    u32 secondTexture;
    s32 alpha;
    s32 mode;

    secondTexture = 0;
    if (lbl_804788C0 == 0) {
        return;
    }
    mode = *(s32*)((u8*)&lbl_803A6818 + 0x1C);
    if (mode == 0xC) {
        return;
    }
    switch (mode) {
    case 1:
    case 2:
        PDA_POS[0] = lbl_804788CC[0];
        PDA_POS[1] = PDA_LIST_Y;
        break;
    case 4:
        PDA_POS[0] = 0;
        PDA_POS[1] = 0;
        break;
    case 5:
        PDA_POS[0] = lbl_804788D0[0];
        PDA_POS[1] = PDA_SIZE_Y;
        break;
    }
#ifdef TARGET_PC
    { /* PC (observe only): ORRE_PDA_MODEL_LOG=1 logs the stored offsets once */
        extern char* getenv(const char*);
        extern void OSReport(const char*, ...);
        static int once;
        if (!once && getenv("ORRE_PDA_MODEL_LOG") != NULL) {
            once = 1;
            OSReport("[pdamodel] stored offsets: list %d,%d size %d,%d (the console's: -130,40 and 0,64)\n",
                     lbl_804788CC[0], lbl_804788CE, lbl_804788D0[0], lbl_804788D2);
        }
    }
#endif
    if (mode == 5) {
        if (*((u8*)&lbl_803A6818 + 0x214) != 0) {
            firstTexture = menuModelRender((u8*)&lbl_803A6818 + 0x7C);
            secondTexture = menuModelRender((u8*)&lbl_803A6818 + 0xC4);
        } else {
            firstTexture = 0;
            secondTexture = 0;
        }
    } else if (*((u8*)&lbl_803A6818 + 0x214) != 0) {
        firstTexture = menuModelRender((u8*)&lbl_803A6818 + 0x7C);
    } else {
        firstTexture = 0;
    }
    alpha = lbl_8047BCA0 * *(f32*)((u8*)&lbl_803A6818 + 0x5C);
    if (firstTexture != 0) {
        gfxSetFlags(3);
        gfxClearFlags(4);
        gfxSetPrimitive(7);
        gfxSetVtxDesc(lbl_80314F98);
        gfxLoadTexture(0, PC_PTR(firstTexture));
        gfxBegin(2);
        gfxVtxPos2s16(PDA_POS[0], PDA_POS[1]);
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, alpha);
        gfxVtxTexCoord(0, lbl_8047BC94, lbl_8047BC94);
        gfxVtxPos2s16((s16)(PDA_POS[0] + (s16)sprite->x),
                     (s16)(PDA_POS[1] + (s16)sprite->y));
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, alpha);
        gfxVtxTexCoord(0, lbl_8047BCBC, lbl_8047BCBC);
        gfxEnd();
    }
    if (secondTexture != 0) {
        gfxSetFlags(3);
        gfxClearFlags(4);
        gfxSetPrimitive(7);
        gfxSetVtxDesc(lbl_80314F98);
        gfxLoadTexture(0, PC_PTR(secondTexture));
        gfxBegin(2);
        gfxVtxPos2s16(PDA_POS[0], PDA_POS[1]);
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, alpha);
        gfxVtxTexCoord(0, lbl_8047BC94, lbl_8047BC94);
        gfxVtxPos2s16((s16)(PDA_POS[0] + (s16)sprite->x),
                     (s16)(PDA_POS[1] + (s16)sprite->y));
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, alpha);
        gfxVtxTexCoord(0, lbl_8047BCBC, lbl_8047BCBC);
        gfxEnd();
    }
}
#pragma peephole reset
