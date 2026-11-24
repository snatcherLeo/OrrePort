/**
 * @file gs_msg.c
 * @brief GSmsg -- GSAPI message/text-box subsystem (font, VM opcodes,
 *        message task scheduling).
 *
 * Address range: 0x800F96E4 - 0x800FE35C (37 functions).
 * XD class: game/pxdvs/GSAPI/GSmsg/GSmsg.cpp
 *
 * Split out of the former monolithic game/gs_thread_hi.c
 * (0x800F8268-0x800FF0A0 per config/GC6E01/splits.txt).
 */
#include "game/gs_msg_task.h"
#include "dolphin/types.h"
#include "dolphin/gx/GX.h"
#include "game/gs_thread.h"
#include "game/gs_texture.h"

/*
 * The standalone linked units compile only their own text range from this
 * file: GS_MSG_CHARCPY_ONLY (GScharMakeFromSJIS through GScharCpy,
 * 0x800F9D04-0x800F9EE4), GS_MSG_CHARCMP_ONLY (0x800F9EE4),
 * GS_MSG_GETGSCHAR_ONLY (0x800FA280),
 * GS_MSG_GETLENGTH_ONLY (0x800FA314), GS_MSG_OPENCLOSE_ONLY (GSmsgClose
 * through GSmsgSetCtrlFunc, 0x800FC1D0-0x800FC528), GS_MSG_INIT_ONLY
 * (0x800FC528), GS_MSG_MAKESTR_ONLY (GSmsgGetText, 0x800F96E4-0x800F9AEC)
 * and GS_MSG_GBACONV_ONLY (GScharToGBA and GScharFromGBA, 0x800F9AEC-0x800F9D04).
 *
 * GS_MSG_POOL_OBJECT (GSmsg_800F9D04.c) builds the linked object for
 * 0x800F9D04-0x800FE35C: everything except GSmsgGetText, GScharToGBA and
 * GScharFromGBA, plus the TU's .bss (0x80401DE0-0x80402518) and its .sdata2
 * literal pool (0x8047CD00-0x8047CD50). Any full (non-partial) compile
 * defines the .bss objects and the pool's named entries.
 */
#if defined(GS_MSG_CHARCPY_ONLY) || \
    defined(GS_MSG_CHARCMP_ONLY) || defined(GS_MSG_GETGSCHAR_ONLY) || \
    defined(GS_MSG_GETLENGTH_ONLY) || defined(GS_MSG_OPENCLOSE_ONLY) || \
    defined(GS_MSG_INIT_ONLY) || defined(GS_MSG_MAKESTR_ONLY) || \
    defined(GS_MSG_GBACONV_ONLY)
#define GS_MSG_PARTIAL
#endif

typedef u8 M2C_UNK;
#define M2C_FIELD(base, type, offset) (*(type)((u8*)(base) + (offset)))


/* ===== External SDK / engine functions ===== */
extern void  GSlogWrite(const void* fmt, ...);          /* OSReport */
extern u16   GSmemAllocRaw(u32 size);                    /* _toolentryAlloc__FUl */
extern void* GSmemGetPtr(u16 handle);                    /* GSmemLock */
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);                      /* fn_800E24B0 */
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void  GSmemFree(u16 handle);                      /* GSmemFree */
#endif
extern u16   GSmemAlloc(u32 alignment, u32 size);        /* GSmemAlloc */
extern void  OSSetIdleFunction(void* func, void* arg,
                          void* stackTop, u32 stackSize); /* OSCreateFiber-like */
extern void  OSDisableInterrupts(void);
extern void  OSRestoreInterrupts(void);
extern void  fn_800D30A0(void (*callback)(u32));          /* GSgfx retrace callback */
extern void  threadSaveGPRRegisters(void);                /* GSthread context init */
extern void  threadSaveFPRRegisters(void);                           /* GSthread FPU context init */
/* renamed symbols referenced by asm incs (symbolmap port) */
extern void GSscratchFree(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern f64 cos(f64 angle);
#endif

/* ===== String constants (rodata references) ===== */
extern const char lbl_80271008[]; /* "GSthreadCreate. Warning: 'usesFPU==FALE' OK?\n" */

/* ===== Forward declarations for internal functions ===== */
extern void gappVSyncCallback(u32 retraceCount); /* GStaskSwapCallback */
extern void fn_800F0F4C(u32 arg);          /* GSthread trampoline / entry wrapper */
extern void PADRead(void* buf);
#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */
extern u32 SIProbe(u32 buttonIdx);
#endif
extern void fn_800AB4FC(void*);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSmemFree(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* fn_800E24B0(u16 handle);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* GSmemLock(u16 handle);
#endif
extern u16 GSmemAlloc(u32 alignment, u32 size);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u16 _toolentryAlloc__FUl(u32 size);
#endif
extern u32 SJIStoGSchar(u16* destination, const u8* source);
extern void fn_800DBEB4(u32 index, GXColor color);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxColor4u8(s32 a, s32 b, s32 c, s32 d, s32 e);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxPos2s16(s16 x, s16 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxEnd(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxBegin(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetPrimitive(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetVtxDesc(void* ptr);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxLoadTexture(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxClearFlags(u32 mask);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetFlags(u32 mask);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800D9ED8(void);
#endif
extern void fn_800DC1D4(s32 a);
extern void logVsnprintf_float(char* output, u32 capacity, const char* format, void* arguments);
extern void soundStopSE(u32 type);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800CDBE0(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DBF78(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DBFD4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DC04C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DC0D4(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800DC14C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetTevStage(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void windowDrawSprite(s32 x, s32 y, s32 context, u32 key, u32 flags);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void soundStartSE(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxVtxTexCoord(s32 slot, f32 xScale, f32 yScale);
#endif
extern void gfxVtxColor1u32(s32 slot, u32 color);
extern void gfxSetScissor(u16 a, u16 b, u16 c, u16 d);
extern f64 tan(void);
extern void gfxMultMatrix(void* mtx);
extern void gfxLoadIdentity(void);
extern void gfxSetPerspective(f32 a, f32 b, f32 c, f32 d);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetCullMode(s32 a);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetAlphaCompare(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetZMode(s32 a, s32 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetUpdates(s32 a, s32 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void gfxSetBlendMode(s32 a, s32 b, s32 c);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void set__5GSvecFfff(void* dst, f32 x, f32 y, f32 z);
#endif
extern void GSmtxLookAt(void* dst, void* a, void* b, void* c);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memset(void* dest, int val, u32 n);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void* memcpy(void* dst, const void* src, u32 n);
#endif

/* ===== BSS/SDA symbol externs (for asm{} blocks) ===== */
/* BSS/data/rodata symbols accessed via lis/@ha + addi/@l pairs */
extern u32 lbl_80401C10;
/* .bss symbols */
#if !defined(GS_MSG_PARTIAL)
/*
 * The TU's .bss, 0x80401DE0-0x80402518 (XD GSmsg.o's .bss is all anonymous
 * statics). MWCC addresses file statics off the pooled .bss base, which is
 * what GSmsgPrintf's `addi r31,base; addi r30,r31,0x5D0` shape comes from.
 * GC/1.3.2 keeps those object offsets out of the store displacements (GC/1.3
 * folds them), so GSmsgPrintf's fill pointer stays a second addi until
 * post-allocation CSE turns it into retail's `mr r7,r30`. lbl_80401DE0 and
 * lbl_804024E8 stay global: the GSmsgGetText unit and the .sdata pointer
 * _pMw name them.
 */
u8 lbl_80401DE0[0x68];
static u8 lbl_80401E48[0x68];
static u16 lbl_80401EB0[0x200]; /* GSmsgPrintf's GSchar print buffer */
static char lbl_804022B0[0x100]; /* GSmsgPrintf's formatted string */
static u8 lbl_804023B0[0x68]; /* GSmsgPrintf's print task */
static u8 lbl_80402418[0x68];
static u8 lbl_80402480[0x68];
#else
extern u8  lbl_80401DE0[];
extern u8  lbl_80401E48[];
extern u8  lbl_80402418[];
extern u8  lbl_80402480[];
#endif
/* .data symbols */
extern u8  lbl_80314E08[];
extern u8  lbl_80314F98[];
extern u8  lbl_80315678[];
/* .rodata symbols */
extern u8  lbl_80271300[];
extern u8  lbl_80271500[];
extern u8  lbl_80271700[];
extern u8  lbl_80271730[];
extern u8  lbl_80271754[];
extern u8  lbl_8027177C[];
extern u8  lbl_802717B4[];
extern u8  lbl_802717D4[];
/* .sdata symbol */
extern float lbl_80478AC0;
/* sdata2 (r2) float/double constants used in asm blocks */
extern f64 lbl_8047CCC8;  /* f64 */
extern f32 lbl_8047CCD0;  /* f32 */
extern f32 lbl_8047CCD4;  /* f32 */
extern f32 lbl_8047CCD8;  /* f32 */
extern f32 lbl_8047CCDC;  /* f32 */
extern f64 lbl_8047CCE0;  /* f64 */
extern f64 lbl_8047CCE8;  /* f64 */
extern f64 lbl_8047CCF0;  /* f64 */
extern f64 lbl_8047CCF8;  /* f64 */
#if !defined(GS_MSG_PARTIAL)
/*
 * The TU's .sdata2 pool, 0x8047CD00-0x8047CD50. The two whites and the
 * 1.0f come first: retail creates the 1.0f in GSmsgGetText, which is not in
 * the linked object, so the three are named here and the rest are
 * anonymous literals in creation order. RULE-EXCEPTION(title-path): the
 * 1.0f is a one-element const array so MWCC cannot fold it into a second,
 * anonymous 1.0f literal, and the whites are named statics - see
 * docs/RULE_EXCEPTIONS.md. The GSmsgGetText unit names lbl_8047CD08.
 */
static const GXColor lbl_8047CD00 = { 255, 255, 255, 255 };
static const GXColor lbl_8047CD04 = { 255, 255, 255, 255 };
const f32 lbl_8047CD08[1] = { 1.0f };
#define GS_MSG_ONE (lbl_8047CD08[0])
#else
extern const f32 lbl_8047CD08; /* 1.0f */
#define GS_MSG_ONE lbl_8047CD08
#endif
extern f64 lbl_8047CD50;  /* f64 */
extern f32 lbl_8047CD58;  /* f32 */
extern f32 lbl_8047CD5C;  /* f32 */
extern f32 lbl_8047CD60;  /* f32 */
extern f32 lbl_8047CD64;  /* f32 */
extern f32 lbl_8047CD68;  /* f32 */
extern f32 lbl_8047CD6C;  /* f32 */
extern f32 lbl_8047CD70;  /* f32 */
extern f32 lbl_8047CD74;  /* f32 */
extern f32 lbl_8047CD78;  /* f32 */
/*
 * Message system state. _pMw (.sdata) points at the 0x2C-byte
 * record GSmsgInit clears in .bss (lbl_804024E8). Offsets are evidenced by
 * GSmsgInit, the font/task scans, GSmsgDaemon and the renderer.
 */
struct MessageSystem { /* GameCube layout on the PC too: pointer fields are 32-bit slots (pc/ptr32.h) */
    u16 taskCount;                   /* 0x00 */
    u16 taskHandle;                  /* 0x02 */
    u16 fontCount;                   /* 0x04 */
    u16 fontHandle;                  /* 0x06 */
    PTR32_FIELD(struct MessageGroup) groups;     /* 0x08 */
    PTR32_FIELD(GStextureHandle) textures[2];    /* 0x0C: double-buffered glyph atlas */
    PTR32_FIELD(void) image;                     /* 0x14: locked atlas image */
    s16 atlasX;                      /* 0x18: glyph atlas cursor */
    s16 atlasY;                      /* 0x1A */
    u8 unk1C;                        /* 0x1C */
    s8 textureIndex;                 /* 0x1D */
    u8 reserved_1E[2];               /* 0x1E */
    PTR32_FIELD(u8) tasks;                       /* 0x20: taskCount 0x68-byte task records */
    PTR32_FIELD(struct FontSlot) fonts;          /* 0x24: fontCount slots */
    PTR32_FIELD(struct MessageControl) controls; /* 0x28: GSmsgSetCtrlFunc table */
};
/* sbss (r13) symbols -- task and thread system */
#if !defined(GS_MSG_PARTIAL)
struct MessageSystem lbl_804024E8;

/*
 * RULE-EXCEPTION(title-path): unreferenced helper that only fixes the
 * pooled .bss order - see docs/RULE_EXCEPTIONS.md. MWCC lays the .bss out
 * in first-reference order. Retail has the print code buffer before the
 * print string buffer, and lbl_80402480 before lbl_804024E8, although
 * GSmsgPrintf writes the string first and GSmsgInit (earlier than
 * _msgGetSize) references lbl_804024E8 first. This static function is never
 * called and is dead-stripped at link time.
 */
static void msgBssLayout(void) {
    lbl_80401DE0[0] = 0;
    lbl_80401E48[0] = 0;
    lbl_80401EB0[0] = 0;
    lbl_804022B0[0] = 0;
    lbl_804023B0[0] = 0;
    lbl_80402418[0] = 0;
    lbl_80402480[0] = 0;
    lbl_804024E8.taskCount = 0;
}
#else
extern struct MessageSystem lbl_804024E8;
#endif
extern struct MessageSystem* _pMw; /* = &lbl_804024E8 */
extern u32 lbl_80478B10;
extern u32 lbl_80478B14;
extern u32 _activeThread;
extern u32 lbl_8047AC04;
extern u32 lbl_8047AC08;
extern u32 _rescan;
extern u32 _usesFPURegisters;
extern u32 _tempGPR2;
extern u32 _tempGPR1;
extern u32 _destRegStatePtr;
extern u32 _threadRegStatePtr;
extern u32 _localRegStatePtr;
extern u32 _threadList;
extern u32 lbl_8047AC2C;
extern u32 _maxThreads;
extern u32 lbl_8047AC34;
extern u32 lbl_8047AC38;
extern u32 lbl_8047AC3C;
extern u32 lbl_8047AC40;
extern u32 lbl_8047AC44;
extern u32 lbl_8047AC48;
extern u32 lbl_8047AC4C;
extern u32 lbl_8047AC50;
extern u32 lbl_8047AC54;
extern u16 lbl_8047AC58;
extern u32 lbl_8047AC5C;
extern u32 lbl_8047AC60;
extern u32 lbl_8047AC64;
extern s32 lbl_8047AC68;
extern u32 lbl_8047AC6C;
extern u32 lbl_8047AC70;
extern u32 lbl_8047AC72;
extern u32 lbl_8047AC74;
extern u32 lbl_8047AC78;
extern u32 _gappStructList;
extern u32 lbl_8047AC80;
extern u32 lbl_8047AC84;
extern u32 lbl_8047AC88;
extern u32 lbl_8047AC8C;
extern u32 lbl_8047AC90;
extern u32 lbl_8047AC94;
extern u32 _gappListTop;
extern u32 lbl_8047AC9C;

/* Forward declarations for all asm-wrapped functions in this block */
extern void inputPadCallback(void);
extern void inputUpdateMotors();
extern void inputSmoothStick();
extern void inputClampSticks();
extern u32 GSresFindById(u32 key);
extern void GSresInit(u32 count);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* GSmsgGetText(u8* destination, s32 capacity, u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 GScharToGBA(void* outbuf, const u16* src, s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GScharMakeFromSJIS(u16* destination, const u8* source);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u8* GScharCpy(u8* dst, const u8* src);
#endif
extern void GSmsgSetColor(void* obj);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSmsgGetRect();
#endif
extern void GSmsgInitRuby();
extern s32 GSmsgPrintf(s32, s32, u32, const char*, ...);
extern s32 std001_messageFormatLookup();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSmsgPrintColor();
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSmsgPrintRight(s32 x, s32 y, s16 width, s16 height, s32 color, u32 key);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern s32 GSmsgPrintRect(s32 x, s32 y, s16 width, s16 height, s32 color, u32 key);
#endif
extern void GSmsgDaemon(void);
extern s32 GSmsgExec();
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern u32 fn_800FC2A8(void* ptr);
#endif
extern void* GSmsgFontOpen();
extern s32 GSmsgInit(u16 taskCount, u16 fontCount);
extern s32 msgTaskDraw(u8* arg0, u32 arg1, u32 arg2, u32 arg3);
extern void msgDrawRuby(u8* work);
extern void _msgMakeTexture(u8* work, const u8* pixels, s16 width, s16 height, s16 yOffset);
extern u16* _msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(u8* work, u16 code, void** outBank);
extern s32 _msgGetLength__FPCUs(const void* str);
extern s32 _msgGetSize__FPCUs(const u16* text);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE35C(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE38C(s32 x1, s32 y1, s32 x2, s32 y2);
#endif
extern void spriteSetEnv(void);
extern void fn_800FE6A0(f32 a, f32 b);
extern void fn_800FE6AC(s16* outA, s16* outB);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void fn_800FE6D0(s32 a, s32 b);
#endif
extern void GSgappUnblock(u32 taskId);
extern void GSgappBlock(u32 taskId);
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
extern void GSgappTerminate(u32 taskId);
#endif
extern void GSgappUpdate(void);
extern u32 GSgappCreate(s32 state, u8 priority, void* param, void* func);
extern void GSgappInit();
extern void gappBackgroundCallback(void);

/* MessageEntry, MessageGroup, GlyphEntry, FontBank, FontFileHeader: disc layouts, game/gs_msg_disc.h */
#include "game/gs_msg_disc.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_MessageText in GSmsgFindMessage below (the language switch) */
#endif
struct FontSlot {
    u16 id;
    u8 width;
    u8 height;
    struct FontBank* bank;
};

/* Retail repeats this lookup at 0x800FA280/0x800FA314/0x800FBB34/0x800FBF74.
 * The last two expansions retain the optional group-output pointer check. */
static inline void* GSmsgFindMessage(u32 key, struct MessageGroup** outGroup) {
    struct MessageGroup* node;
    struct MessageEntry* entries;
    u32 val;
    u32 lo;
    u32 hi;
    u32 mid;
    u32 index;
    u32 group;

    if (key == 0) return NULL;

    node = P32(_pMw->groups);
    group = key >> 20;
    index = key & 0xFFFFF;

    while (node != NULL) {
        if (node->id == group) {
            hi = node->count;
            entries = node->entries;
            lo = 0;
            while (lo < hi) {
                mid = (lo + hi) / 2;
                val = entries[mid].key;
                if (val == index) {
                    if (outGroup != NULL) *outGroup = node;
#ifdef TARGET_PC
                    /* the language switch (2.2 step 4, 2026-11-13): the chosen language's text for the same key,
                     * in the disc's encoding; NULL (or no hook) keeps the disc's text. The group still gives the font,
                     * so a replacement draws in the same font as the original (orre_hooks.h) */
                    if (OrreHook_MessageText != NULL) {
                        void* other = OrreHook_MessageText(key, (u8*)node + entries[mid].offset);
                        if (other != NULL) return other;
                    }
#endif
                    return (u8*)node + entries[mid].offset;
                }
                if (val < index) lo = mid + 1;
                else hi = mid;
            }
        }
        node = DP(node->next);
    }
#ifdef TARGET_PC
    /* OrrePort's own messages (orre_hooks.h): keys no loaded bank has, e.g. the Options screen's Language row */
    if (OrreHook_MessageExtra != NULL) return OrreHook_MessageExtra(key, (void**)outGroup);
#endif
    return NULL;
}

#if defined(TARGET_PC) && !defined(GS_MSG_PARTIAL)
/* the group header (16 bytes, its font id at byte 3) of the loaded bank holding a key, or NULL: OrrePort's own
 * messages borrow the font of a game message shown in the same place (pc_game_text.cpp; 2026-11-13). Defined in
 * the full unit only (the partial units include this file too). */
void* pc_msg_group_of(u32 key) {
    struct MessageGroup* group = NULL;
    if (GSmsgFindMessage(key, &group) == NULL) return NULL;
    return group;
}
#endif

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_MAKESTR_ONLY)

/* Eight-byte records in msgctrlcode: five high flag bits, then a callback. */
#ifdef TARGET_PC
/* On the PC msgctrlcode is a native C array of void* (data_80363774.c): each record is two 8-byte slots, the flags
 * value in the low 4 bytes of the first (little-endian), the callback in the second: this struct has the same layout.
 * GCC allocates bit-fields from the low bit, so the fields are listed in reverse to keep `mode` in bits 30-31 of the
 * native word, as MWCC's high-bit-first layout has them (the project notes 10). Not DISC_STRUCT: the table is
 * native data, not big-endian disc bytes. */
struct MessageControl {
    unsigned int reserved : 27;
    unsigned int measure : 1;
    unsigned int execute : 1;
    unsigned int stop : 1;
    unsigned int mode : 2;
    u32 (*callback)(u8*);
};
#else
struct MessageControl {
    unsigned int mode : 2;
    unsigned int stop : 1;
    unsigned int execute : 1;
    unsigned int measure : 1;
    unsigned int reserved : 27;
    u32 (*callback)(u8*);
};
#endif

/* RULE-EXCEPTION(title-path): inline helper whose only evidence is register
 * allocation - see docs/RULE_EXCEPTIONS.md. Taking the entry through an
 * inline return creates it after the renderer's other inline temporaries,
 * which gives retail's saved registers in fn_800FC7E0 (r22 and r19 for the
 * two dispatches); every other dispatch expansion is unchanged. */
static inline struct MessageControl* msgCtrlEntry(struct MessageControl* table, u32 control) {
    return &table[control];
}

/* Both renderer passes expand this dispatch; only the first uses its stop bit.
 * XD keeps it as the dead-stripped _msgCallCtrlFunc__FP13MSG_TASK_WORKUc
 * (NXXJ01.map, GSmsg.o, UNUSED 0x1D8). Mode 2 resolves a message key through
 * the same lookup as GSmsgGetGSchar (retail repeats its zero-key test), and
 * the switch has no default: for mode 3 retail pushes the call stack with
 * whatever `next` last held (see the RULE-EXCEPTION note below). */
static inline u8 GSmsgDispatchControl(u8* work, u32 control) {
    struct MessageControl* table;
    struct MessageControl* entry;
    u8* next;
    u32 enabled;
    u32 result;
    u32 mode;

    table = PTR32_AT_T(struct MessageControl, (u8*)_pMw, 0x28);
    if (table == NULL) return 0;
    if (work[1] == 0) {
        enabled = table[control].execute;
    } else {
        enabled = table[control].measure;
    }
    if (enabled == 0) return 0;
    entry = msgCtrlEntry(table, control);
    if (entry->callback != NULL) {
        result = entry->callback(work);
        mode = entry->mode;
        if (mode != 0 && result != 0) {
            /* RULE-EXCEPTION(title-path): uninitialized read (mode 3 has no
             * case, so `next` keeps its previous value, as in retail) â see
             * docs/RULE_EXCEPTIONS.md */
            switch (mode) {
            case 1:
                next = (u8*)PC_PTR(result);
#ifdef TARGET_PC
                if (OrreHook_MessageInsert != NULL) { /* other text for the insert (orre_hooks.h); NULL: the game's */
                    u8* other = (u8*)OrreHook_MessageInsert(control, next);
                    if (other != NULL) next = other;
                }
#endif
                break;
            case 2:
                next = GSmsgFindMessage(result, NULL);
                break;
            }
            if (*(s8*)(work + 0x40) >= 3) {
                GSlogWrite((const char*)lbl_80271700, lbl_80315678);
            } else {
#ifdef TARGET_PC
                { /* push the cursor: 32-bit slots on the PC too */
                    s8 depth_ = (*(s8*)(work + 0x40))++;
                    PTR32_AT_SET(work, 0x34 + depth_ * 4, PTR32_AT(work, 0x30));
                }
#else
                *(u8**)(work + 0x34 + (*(s8*)(work + 0x40))++ * 4) = *(u8**)(work + 0x30);
#endif
                PTR32_AT_SET_T(u8, work, 0x30, next);
            }
        }
    }
    return entry->stop;
}

/* Retail expands this reader in fn_800FC7E0 and _msgGetSize__FPCUs,
 * including a separate zero-code test on the returned halfword. */
static inline u16 GSmsgReadCode(u8* work) {
    u16* cursor;
    u16 code;
    s8 depth;

    for (;;) {
        cursor = P32(((MessageTask*)work)->textCursor);
        code = DISC_U16(cursor); /* message text is UTF-16 big-endian (the disc's message banks); native on the GameCube */
        if (code == 0) {
            depth = ((MessageTask*)work)->textDepth;
            if (depth == 0) return code;
            depth--;
            ((MessageTask*)work)->textDepth = depth;
            P32_SET(((MessageTask*)work)->textCursor, PTR32_AT_T(u16, work + 0x34, depth * 4));
        } else {
            P32_SET(((MessageTask*)work)->textCursor, cursor + 1);
            return code;
        }
    }
}

#if !defined(GS_MSG_POOL_OBJECT)
/* 0x800F96E4 | 0x408 */
u8* GSmsgGetText(u8* destination, s32 capacity, u32 key) {
    struct MessageGroup* bank;
    u8* text;
    u8* work;
    u8* dst;
    u8* savedDst;
    u8* parameters;
    u16 code;
    u32 control;
    s8 savedDepth;
    u32 maxBytes;
    u32 count = 0;
    u32 savedCount;

    if (key == 0) return NULL;
    if (destination == NULL) return NULL;
    if (capacity <= 0) return NULL;

    dst = destination;
    maxBytes = ((u32)capacity - 1) * 2;
    text = GSmsgFindMessage(key, &bank);
    if (text == NULL) return NULL;

    work = (u8*)&lbl_80401DE0;
    memset(work, 0, 0x68);
    lbl_80401DE0[0] = 1;
    /* RULE-EXCEPTION(title-path): extern named stand-in for the TU's own
     * pool literal (lbl_8047CD08, 1.0f), so the function links as its own
     * unit - see docs/RULE_EXCEPTIONS.md */
    *(f32*)(lbl_80401DE0 + 0x60) = GS_MSG_ONE;
    *(f32*)(lbl_80401DE0 + 0x64) = GS_MSG_ONE;
    *(s32*)(lbl_80401DE0 + 0x24) = -1;
    PTR32_AT_SET_T(u8, lbl_80401DE0, 0x28, text);
    PTR32_AT_SET_T(u8, lbl_80401DE0, 0x2C, text);
    PTR32_AT_SET_T(u8, lbl_80401DE0, 0x30, text);
    *(u16*)(lbl_80401DE0 + 0x20) = bank->fontId;
    *(u32*)(lbl_80401DE0 + 0x1C) = key;
    lbl_80401DE0[1] = 1;

    for (;;) {
        savedDst = dst;
        savedCount = count;
        code = GSmsgReadCode(work);
        if (code == 0) break;

        count += 2;
        if (maxBytes < count) break;
#ifdef TARGET_PC
        /* GS text is big-endian in memory on the PC too (GSmsgReadCode reads it so); a native store left the PC box
           names, the PDA's names and the Colosseum message buffer little-endian (seen in a PC save, 2026-10-27) */
        *(u16*)dst = (u16)__builtin_bswap16(code);
#else
        *(u16*)dst = code;
#endif
        dst += 2;
        if (code != 0xFFFF) continue;
#ifdef TARGET_PC

        { /* read a byte and advance the text cursor (a 32-bit slot on the PC) */

            u8* ip_ = (u8*)PTR32_AT(work, 0x30);

            control = *ip_;

            PTR32_AT_SET(work, 0x30, ip_ + 1);

        }
#else

        control = *(*(u8**)(work + 0x30))++;
#endif
        count++;
        if (maxBytes >= count) {
            *dst++ = (u8)control;
        } else {
            dst = savedDst;
            break;
        }

        parameters = PTR32_AT_T(u8, work, 0x30);
        savedDepth = *(s8*)(work + 0x40);
        GSmsgDispatchControl(work, control);
        if (savedDepth == *(s8*)(work + 0x40)) {
            count += (u32)(PTR32_AT_T(u8, work, 0x30) - parameters);
            if (maxBytes >= count) {
                while (PTR32_AT_T(u8, work, 0x30) > parameters) {
                    *dst++ = *parameters++;
                }
            } else {
                dst = savedDst;
                break;
            }
        } else {
            dst = savedDst;
            count = savedCount;
        }
    }

    *(u16*)dst = 0;
    return destination;
}
#endif /* !GS_MSG_POOL_OBJECT */

#endif /* !GS_MSG_PARTIAL || GS_MSG_MAKESTR_ONLY */

#if (!defined(GS_MSG_PARTIAL) && !defined(GS_MSG_POOL_OBJECT)) || \
    defined(GS_MSG_GBACONV_ONLY)

/* 0x800F9AEC | 0x118 */
static inline u32 msgGBAFromGSchar(u8* out, const u16* src, const u16* table) {
    u32 ch;
    const u16* p;
    u32 count;
    s32 idx;

    count = 0;
    if (src == NULL) return count;
    while ((ch = *src) != 0) {
        p = table;
        idx = 0;
        while (ch != *p) {
            idx++;
            p++;
            if (idx >= 0x100) {
                idx = 0xB7;
                break;
            }
        }
        if (out != NULL) {
            *out++ = (u8)idx;
        }
        count++;
        src++;
    }
    return count;
}

/*
 * Retail's dispatch keeps case 7 apart from case 8/default (pivot `beq` on 7,
 * no range merge of 7-8) while all three land on the second table. The no-op
 * self-assignment in case 7 survives the IR long enough to keep that label
 * distinct and is coalesced away by the backend; it emits no code.
 */
u32 GScharToGBA(void* outbuf, const u16* src, s32 mode) {
    switch (mode) {
    case 1:
        return msgGBAFromGSchar((u8*)outbuf, src, (const u16*)lbl_80271300);
    case 7:
        /* RULE-EXCEPTION(user-approved): no-op self-assignment keeps case 7 on its own label — see docs/RULE_EXCEPTIONS.md */
        outbuf = (u8*)outbuf;
        break;
    case 8:
    default:
        break;
    }
    return msgGBAFromGSchar((u8*)outbuf, src, (const u16*)lbl_80271500);
}

/* 0x800F9C04 | 0x100 */
static inline u32 msgGScharFromGBA(u16* out, const u8* src, s32 count, const u16* table) {
    u32 total = 0;

    if (src == NULL) return total;
    while (count != 0 && *src != 0xFF) {
        if (out != NULL) {
            *out = table[*src++];
            out++;
        }
        total++;
        count--;
    }
    if (out != NULL) *out = 0;
    return total;
}

/* Same dispatch shape as GScharToGBA (see the note there). */
u32 GScharFromGBA(void* outbuf, const u8* src, s32 count, s32 mode) {
    switch (mode) {
    case 1:
        return msgGScharFromGBA((u16*)outbuf, src, count, (const u16*)lbl_80271300);
    case 7:
        /* RULE-EXCEPTION(user-approved): no-op self-assignment keeps case 7 on its own label — see docs/RULE_EXCEPTIONS.md */
        outbuf = (u16*)outbuf;
        break;
    case 8:
    default:
        break;
    }
    return msgGScharFromGBA((u16*)outbuf, src, count, (const u16*)lbl_80271500);
}

#endif /* (!GS_MSG_PARTIAL && !GS_MSG_POOL_OBJECT) || GS_MSG_GBACONV_ONLY */

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_CHARCPY_ONLY)

/* 0x800F9D04 | 0x20 */
void GScharMakeFromSJIS(u16* destination, const u8* source) {
    SJIStoGSchar(destination, source);
}

/* 0x800F9D24 | 0x14C */
void* GScharLenCpy(u16* dst, const u16* src, s32 maxlen) {
    s32 length;
    s32 i;

    if (maxlen <= 0) return dst;
    length = ((u32)_msgGetSize__FPCUs(src) + 1) >> 1;
    if (length >= maxlen) length = maxlen - 1;
    memcpy(dst, src, length * sizeof(*dst));
    i = length;
    while (i < maxlen) {
        dst[i++] = 0;
    }
    return dst;
}

/* 0x800F9E70 | 0x74 */
u8* GScharCpy(u8* dst, const u8* src) {
    if (dst == NULL) return NULL;
    if (src == NULL) {
        *(u16*)dst = 0;
    } else {
        memcpy(dst, src, _msgGetSize__FPCUs((const u16*)src));
    }
    return dst;
}

#endif /* !GS_MSG_PARTIAL || GS_MSG_CHARCPY_ONLY */

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_CHARCMP_ONLY)
/* 0x800F9EE4 | 0x180 */
#ifdef TARGET_PC
/* The same comparison on character codes: GS text is big-endian in memory on the PC too (GSmsgReadCode), so a native
   halfword compare would sort names differently from the console */
s32 GScharCmp(const u16* str1, const u16* str2) {
    u32 len1 = _msgGetLength__FPCUs(str1);
    u32 len2 = _msgGetLength__FPCUs(str2);
    u32 n = len1 < len2 ? len1 : len2;
    u32 i;

    for (i = 0; i < n; i++) {
        u16 a = DISC_U16(&str1[i]);
        u16 b = DISC_U16(&str2[i]);
        if (a != b) return a > b ? 1 : -1;
    }
    return len1 == len2 ? 0 : (len1 > len2 ? 1 : -1);
}
#else
s32 GScharCmp(const u16* str1, const u16* str2) {
    u32 len1;
    u32 len2;
    u32 i;

    len1 = _msgGetLength__FPCUs(str1);
    len2 = _msgGetLength__FPCUs(str2);

    if (len1 == len2) {
        for (i = 0; i < len2; i++) {
            if (str1[i] != str2[i]) {
                if (str1[i] > str2[i]) return 1;
                return -1;
            }
        }
        return 0;
    } else if (len1 > len2) {
        for (i = 0; i < len2; i++) {
            if (str1[i] != str2[i]) {
                if (str1[i] > str2[i]) return 1;
                return -1;
            }
        }
        return 1;
    } else {
        for (i = 0; i < len1; i++) {
            if (str1[i] != str2[i]) {
                if (str1[i] > str2[i]) return 1;
                return -1;
            }
        }
        return -1;
    }
}
#endif /* TARGET_PC */

#endif

#if !defined(GS_MSG_PARTIAL)
/* 0x800FA064 | 0xFC */
void GSmsgAdjustAlign(u8* o) {
    s16 r5;

    if (*(s16*)(o + 0x18) == 0) return;
    r5 = (s16)((u32)GSmsgGetRect(*(u32*)(o + 0x1C)) >> 16);

    switch (o[0x4A]) {
    case 0:
        *(f32*)(o + 0xC) = *(f32*)(o + 0x4);
        break;
    case 1:
        *(f32*)(o + 0xC) = *(f32*)(o + 0x4) + (f32)((*(s16*)(o + 0x18) - r5) / 2);
        break;
    case 2:
        *(f32*)(o + 0xC) = *(f32*)(o + 0x4) + (f32)*(s16*)(o + 0x18) - (f32)r5;
        break;
    }
}

/* 0x800FA160 | 0x5C */
void GSmsgSetColor(void* obj) {
    GXColor clr = lbl_8047CD04;
    u32 color;

    color = *(u32*)((u8*)obj + 0x24);
    clr.r = color >> 24;
    clr.g = (color >> 16) & 0xFF;
    clr.b = (color >> 8) & 0xFF;
    clr.a = color & 0xFF;
    fn_800DBEB4(0, clr);
}

/* XD keeps this task-record reset as the dead-stripped
 * _msgInitTask__FP13MSG_TASK_WORKPUc (NXXJ01.map GSmsg.o, UNUSED 0x6C). */
static inline void msgInitTask(u8* work, u8* text) {
    memset(work, 0, 0x68);
    work[0] = 1;
    ((MessageTask*)work)->scaleX = GS_MSG_ONE;
    ((MessageTask*)work)->scaleY = GS_MSG_ONE;
    ((MessageTask*)work)->unk24 = -1;
    P32_SET(((MessageTask*)work)->unk28, text);
    P32_SET(((MessageTask*)work)->unk2C, text);
    PTR32_AT_SET_T(u8, work, 0x30, text);
}

/* Font-slot lookup: copies the slot's glyph width/height into the task work
 * and derives the line height. Retail expands this body in GSmsgGetRect (and
 * the other task initializers) after storing the font id through the global
 * work record, so the id is forwarded from that store rather than reloaded.
 * Pokemon XD's GSmsgSetFontInfo (GXXE01 0x80107200, trevor403/xd-asm
 * b1087f18 code/func_FUN_80107200.s) tests `id == 1 || id == 3` for the
 * six-pixel line height; Colosseum emits the same two-test disjunction
 * (`cmplwi 1; beq; bne`) with both font ids equal to 1. */
static inline void msgSetFontInfo(u8* o) {
    s32 index;
    struct FontSlot* entry;
    u32 val;

    for (index = 0; index < _pMw->fontCount; index++) {
        entry = &P32(_pMw->fonts)[index];
        if (entry->id == *(u16*)(o + 0x20)) {
            o[0x22] = entry->width;
            o[0x23] = entry->height;
            val = *(u16*)(o + 0x20);
            if (val == 0) {
                *(u8*)(o + 0x42) = 0xB;
            } else if (val == 1 || val == 1) { /* RULE-EXCEPTION(title-path): duplicated condition â see docs/RULE_EXCEPTIONS.md */
                *(u8*)(o + 0x42) = 6;
            } else {
                *(s8*)(o + 0x42) = (s8)(s32)(0.5 * (f64)(u32)o[0x23] + 1.0);
            }
            break;
        }
    }
}

/* 0x800FA1BC | 0xC4 */
void GSmsgSetFontInfo(void* obj) {
    u8* o;
    u8* head;
    s32 count;
    s32 index;
    struct FontSlot* entry;
    u32 val;

    o = (u8*)obj;
    head = (u8*)_pMw;
    count = *(u16*)(head + 0x4);
    for (index = 0; index < count; index++) {
        entry = PTR32_AT_T(struct FontSlot, head, 0x24) + index;
        if (entry->id == *(u16*)(o + 0x20)) {
            o[0x22] = entry->width;
            o[0x23] = entry->height;
            val = *(u16*)(o + 0x20);
            if (val == 0) {
                *(u8*)(o + 0x42) = 0xB;
            } else if (val == 1 || val == 1) { /* RULE-EXCEPTION(title-path): duplicated condition â see docs/RULE_EXCEPTIONS.md */
                *(u8*)(o + 0x42) = 6;
            } else {
                *(s8*)(o + 0x42) = (s8)(s32)(0.5 * (f64)(u32)o[0x23] + 1.0);
            }
            break;
        }
    }
}

#endif /* !GS_MSG_PARTIAL */

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_GETGSCHAR_ONLY)
/* 0x800FA280 | 0x94 */
void* GSmsgGetGSchar(u32 key) {
#ifdef TARGET_PC
    /* the string itself, which the game may copy or compare (a species name into a Pokémon's nickname at a snag or an
     * evolution, and "is this nickname its species name?"): the language switch keeps such text the disc's
     * (OrreHook_MessageForCopy, orre_hooks.h), so the save keeps the US names whatever language is shown */
    void* text;
    OrreHook_MessageForCopy++;
    text = GSmsgFindMessage(key, NULL);
    OrreHook_MessageForCopy--;
    return text;
#else
    return GSmsgFindMessage(key, NULL);
#endif
}
#endif

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_GETLENGTH_ONLY)
/* 0x800FA314 | 0xBC */
s32 GSmsgGetLength(u32 key) {
    if (key == 0) return 0;
    return _msgGetLength__FPCUs(GSmsgFindMessage(key, NULL));
}
#endif

#if !defined(GS_MSG_PARTIAL)
/* 0x800FA3D0 | 0x74 */
static inline u8* GSmsgFindCheck(u8* head, u32 key)
{
    s32 count;
    u8* entry;
    u8 i;

    count = *(u16*)head;
    for (i = 0; i < count;) {
        entry = PTR32_AT_T(u8, head, 0x20) + i++ * 0x68;
        if (entry[0] != 0 && *(u32*)(entry + 0x1C) == key) {
            return entry;
        }
    }
    return NULL;
}

s32 GSmsgIsCheck(u32 key) {
    u8* head;
    u8* entry;

    head = (u8*)_pMw;
    entry = GSmsgFindCheck(head, key);
    if (entry != NULL && entry[0] == 1) {
        return 1;
    }
    return 0;
}

/* 0x800FA444 | 0x654
 * Measures a message: runs a scratch task over the text in measure mode and
 * returns (width - 1) << 16 | height. Instruction-exact in the canonical
 * report; its object stays unlinked because the int-to-float conversion
 * constants are the TU's shared .sdata2 pool (lbl_8047CD08-lbl_8047CD28),
 * which other, unlinked GSmsg functions reference by name. */
#ifdef TARGET_PC
/* OrreHook_MessageMeasured (an observation hook; docs/port/hook-points.md, port/src/pc_event_log.c): the expanded
 * characters this measuring pass walked (names, moves and numbers already spliced in), handed over once it ends, so
 * the event log can write the battle text box's line as text. NULL (nothing attached) or not, the measurement and its
 * result are the same. */
extern void (*OrreHook_MessageMeasured)(u32 key, const u16* chars, u32 count);
#endif

s32 GSmsgGetRect(arg0)
    u32 arg0;
{
    struct MessageGroup *bank;
    u8 *text;
    u8 *work;
    u8 *ip;
    u16 code;
    u32 control;
    s16 maxX = 0;
    s16 maxY = 0;
    u8 lineStart = 0;
    void *fontInfo;
#ifdef TARGET_PC
    u16 pcChars[400];
    u32 pcCount = 0;
    f32 pcLineY;
#endif

    if (arg0 == 0) {
        return 0;
    }

    text = GSmsgFindMessage(arg0, &bank);
    if (text == NULL) {
        return 0;
    }

    work = (u8 *)&lbl_80401E48;
    memset(work, 0, 0x68);
    lbl_80401E48[0] = 1;
    *(f32 *)(lbl_80401E48 + 0x60) = GS_MSG_ONE;
    *(f32 *)(lbl_80401E48 + 0x64) = GS_MSG_ONE;
    *(s32 *)(lbl_80401E48 + 0x24) = -1;
    *(u32 *)(lbl_80401E48 + 0x28) = PC_PTR32(text);
    *(u32 *)(lbl_80401E48 + 0x2C) = PC_PTR32(text);
    *(u32 *)(lbl_80401E48 + 0x30) = PC_PTR32(text);
    *(u16 *)(lbl_80401E48 + 0x20) = bank->fontId;
    *(u32 *)(lbl_80401E48 + 0x1C) = arg0;
    lbl_80401E48[1] = 1;
    msgSetFontInfo(work);

    for (;;) {
        code = GSmsgReadCode(work);
        if (code == 0) break;
        if (code == 0xFFFF) {
            ip = PTR32_AT_T(u8, work, 0x30);
            PTR32_AT_SET_T(u8, work, 0x30, ip + 1);
            control = *ip;

            if (control == 3) {
                ((MessageTask*)work)->cursorX += (f32)((MessageTask*)work)->unk22;
                if ((f32)maxX < ((MessageTask*)work)->cursorX) {
                    maxX = (s16)((MessageTask*)work)->cursorX;
                }
            }

#ifdef TARGET_PC
            pcLineY = *(f32 *)(work + 0x10);
#endif
            GSmsgDispatchControl(work, control);
#ifdef TARGET_PC
            if (*(f32 *)(work + 0x10) != pcLineY && pcCount < 400) {
                pcChars[pcCount++] = 0x20; /* a new line: a space in the log's one-line text */
            }
#endif

            if (lineStart != 0 && ((MessageTask*)work)->cursorX == ((MessageTask*)work)->startX) {
                ((MessageTask*)work)->cursorX += (f32)(s32)((MessageTask*)work)->unk22;
            }
        } else {
            if (((MessageTask*)work)->mode == 2) continue;
#ifdef TARGET_PC
            if (pcCount < 400) {
                pcChars[pcCount++] = code;
            }
#endif
            if (code == 0x20) {
                ((MessageTask*)work)->unk14 = (f32)((((MessageTask*)work)->unk22 / 2) * ((MessageTask*)work)->scaleX);
            } else {
                fontInfo = _msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(work, code, NULL);
                if (fontInfo == NULL) {
                    ((MessageTask*)work)->unk14 = (f32)((MessageTask*)work)->unk22 * ((MessageTask*)work)->scaleX;
                } else {
                    ((MessageTask*)work)->unk14 = (f32)((u8 *)fontInfo)[2] * ((MessageTask*)work)->scaleX;
                }
            }

            if ((s8)((MessageTask*)work)->unk41 == 0) {
                if (code == 0x300C) {
                    lineStart = 1;
                }
                if (code == 0x300D) {
                    lineStart = 0;
                }
            }

            ((MessageTask*)work)->cursorX += ((MessageTask*)work)->unk14;
        }
        if ((f32)maxX < *(f32 *)(work + 0x0C)) {
            maxX = (s16)*(f32 *)(work + 0x0C);
        }
        if ((f32)maxY < *(f32 *)(work + 0x10)) {
            maxY = (s16)*(f32 *)(work + 0x10);
        }
    }

    maxY += (f32)((MessageTask*)work)->unk23 * ((MessageTask*)work)->scaleY + GS_MSG_ONE;
#ifdef TARGET_PC
    if (OrreHook_MessageMeasured != NULL) {
        OrreHook_MessageMeasured(arg0, pcChars, pcCount);
    }
    /* OrrePort: the size of a print it fits to a fixed place, so what the game places after it (an arrow) follows */
    if (OrreHook_MessageFitRect != NULL) {
        return OrreHook_MessageFitRect(arg0, ((u32)(maxX - 1) << 0x10) | maxY);
    }
#endif
    return ((u32)(maxX - 1) << 0x10) | maxY;
}

#ifdef TARGET_PC
#include "../../port/src/pc_game_ui.h" /* struct PcMsgUnit */

/* For OrrePort's dialogue re-wrapping (2026-11-14; port/src/pc_msg_rewrap.cpp, docs/port/text-rewrap.md).
 *
 * Why here: the game never measures a line against its box (the writer's line and page codes do all the layout, and
 * the message task doesn't know the box's width), so a longer name or a modded line runs past the box. Re-wrapping
 * needs each line's width exactly as the game will draw it, inserts included, and the walker that does that (the read,
 * the control dispatch, the font set-up) is private to this file. So this walks a running message's text the way
 * GSmsgGetRect does, in the measuring mode (work[1] = 1: each control runs its "measure" side, as GSmsgGetRect relies
 * on, so nothing waits, sounds or draws), with the task's own font and scale, and reports each top-level piece with
 * the pen's x after it. Inside an insert (a name, an item, another message) the pieces aren't reported; the insert's
 * own piece takes the x after all of it. Nothing about the task changes.
 *
 * Returns the number of pieces (the last one PC_MSG_END), or -1 if there are more than cap (the message is left as it
 * is). Platforms: plain C, the same everywhere. XD: GSmsg is shared engine code (likely the same walker). */
static u8 s_pcMeasureWork[0x68];

u8* pc_msg_task_of(u32 key) {
    return GSmsgFindCheck((u8*)_pMw, key);
}

s32 pc_msg_measure_units(const u8* task, struct PcMsgUnit* out, s32 cap) {
    u8* work = s_pcMeasureWork;
    u8* text;
    u8* ip;
    u16 code;
    u32 control;
    s32 n = 0;
    s32 top = -1; /* the last top-level piece: an insert's x is written into it */
    u8 lineStart = 0;
    void* fontInfo;

    if (task == NULL || out == NULL || cap <= 0) return -1;
    text = PTR32_AT_T(u8, (u8*)task, 0x28);
    if (text == NULL) return -1;
    memset(work, 0, 0x68);
    work[0] = 1;
    *(f32*)(work + 0x60) = *(const f32*)(task + 0x60);
    *(f32*)(work + 0x64) = *(const f32*)(task + 0x64);
    *(f32*)(work + 0x04) = *(const f32*)(task + 0x04);
    *(f32*)(work + 0x08) = *(const f32*)(task + 0x08);
    *(f32*)(work + 0x0C) = *(const f32*)(task + 0x04);
    *(f32*)(work + 0x10) = *(const f32*)(task + 0x08);
    *(s32*)(work + 0x24) = -1;
    PTR32_AT_SET_T(u8, work, 0x28, text);
    PTR32_AT_SET_T(u8, work, 0x2C, text);
    PTR32_AT_SET_T(u8, work, 0x30, text);
    *(u16*)(work + 0x20) = *(const u16*)(task + 0x20);
    *(u32*)(work + 0x1C) = *(const u32*)(task + 0x1C);
    work[1] = 1;
    msgSetFontInfo(work);

    for (;;) {
        s32 isTop;
        u32 offset;
        code = GSmsgReadCode(work);
        isTop = ((MessageTask*)work)->textDepth == 0;
        /* where this code sits in the message's own text (meaningful for top-level codes only) */
        offset = (u32)((u8*)PTR32_AT(work, 0x30) - text) - (code == 0 ? 0 : 2);
        if (code == 0) {
            if (n >= cap) return -1;
            out[n].offset = offset;
            out[n].kind = PC_MSG_END;
            out[n].x_after = ((MessageTask*)work)->cursorX;
            return n + 1;
        }
        if (code == 0xFFFF) {
            u32 kind;
            ip = PTR32_AT_T(u8, work, 0x30);
            PTR32_AT_SET_T(u8, work, 0x30, ip + 1);
            control = *ip;
            GSmsgDispatchControl(work, control);
            if (lineStart != 0 && ((MessageTask*)work)->cursorX == ((MessageTask*)work)->startX) {
                ((MessageTask*)work)->cursorX += (f32)(s32)((MessageTask*)work)->unk22;
            }
            kind = (control == 0 || control == 1) ? PC_MSG_CR
                 : control == 3                  ? PC_MSG_PAGE
                 : control == 2                  ? PC_MSG_WAIT
                                                 : PC_MSG_CONTROL;
            if (isTop) {
                if (n >= cap) return -1;
                out[n].offset = offset;
                out[n].kind = kind;
                out[n].x_after = ((MessageTask*)work)->cursorX;
                top = n++;
            } else if (top >= 0) {
                out[top].x_after = ((MessageTask*)work)->cursorX;
            }
            continue;
        }
        if (((MessageTask*)work)->mode == 2) continue;
        if (code == 0x20) {
            ((MessageTask*)work)->unk14 = (f32)((((MessageTask*)work)->unk22 / 2) * ((MessageTask*)work)->scaleX);
        } else {
            fontInfo = _msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(work, code, NULL);
            if (fontInfo == NULL) {
                ((MessageTask*)work)->unk14 = (f32)((MessageTask*)work)->unk22 * ((MessageTask*)work)->scaleX;
            } else {
                ((MessageTask*)work)->unk14 = (f32)((u8*)fontInfo)[2] * ((MessageTask*)work)->scaleX;
            }
        }
        if ((s8)((MessageTask*)work)->unk41 == 0) {
            if (code == 0x300C) lineStart = 1;
            if (code == 0x300D) lineStart = 0;
        }
        ((MessageTask*)work)->cursorX += ((MessageTask*)work)->unk14;
        if (isTop) {
            if (n >= cap) return -1;
            out[n].offset = offset;
            out[n].kind = code == 0x20 ? PC_MSG_SPACE : PC_MSG_LETTER;
            out[n].x_after = ((MessageTask*)work)->cursorX;
            top = n++;
        } else if (top >= 0) {
            out[top].x_after = ((MessageTask*)work)->cursorX;
        }
    }
}
#endif /* TARGET_PC */

/* 0x800FAA98 | 0x460 */
void GSmsgInitRuby(arg0)
    u8 *arg0;
{
    u8 count0;
    u8 count1;
    u8 pass;
    s16 width0;
    s16 width1;
    u8 savedFlag;
    u16 *savedIp;
    u8 savedDepth;
    s32 savedMode;
    u8 *ip;
    u16 *resume[3];
    f32 rubyHeight;
    u16 code;
    u32 control;
    s16 glyphWidth;
    s32 i;
    void *fontInfo;

    for (i = 0; i < 3; i++) {
        resume[i] = PTR32_AT_T(u16, arg0 + 0x34, i * 4);
    }
    savedIp = P32(((MessageTask*)arg0)->textCursor);
    savedDepth = arg0[0x40];
    savedMode = (s8)((MessageTask*)arg0)->unk45;
    savedFlag = arg0[1];

    *(u32 *)(arg0 + 0x54) = 0;
    ((MessageTask*)arg0)->color[0] = 0;
    ((MessageTask*)arg0)->color[1] = 0;
    arg0[1] = 0;
    ((MessageTask*)arg0)->unk45 = 1;
    ((MessageTask*)arg0)->mode = 1;

    count0 = 0;
    count1 = 0;
    width0 = 0;
    width1 = 0;

    for (pass = 0; pass < 2; pass++) {
        for (;;) {
            code = GSmsgReadCode(arg0);
            if (code == 0) break;
            if (code == 0xFFFF) {
#ifdef TARGET_PC
                { /* read a byte and advance the text cursor (a 32-bit slot on the PC) */
                    u8* ip_ = (u8*)PTR32_AT(arg0, 0x30);
                    control = *ip_;
                    PTR32_AT_SET(arg0, 0x30, ip_ + 1);
                }
#else
                control = *(*(u8 **)(arg0 + 0x30))++;
#endif
                GSmsgDispatchControl(arg0, control);

                if (pass == 0) {
                    if (((MessageTask*)arg0)->mode == 2) {
                        *(u32 *)(arg0 + 0x54) = *(u32 *)(arg0 + 0x30);
                        break;
                    }
                } else if (((MessageTask*)arg0)->mode == 0) {
                    break;
                }
                continue;
            }

            fontInfo = _msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(arg0, code, NULL);
            if (fontInfo != NULL) {
                glyphWidth = ((u8 *)fontInfo)[2];
            } else {
                glyphWidth = ((MessageTask*)arg0)->unk22;
            }

            if (pass == 0) {
                count0++;
                width0 = (s16)((f32)width0 + ((f32)glyphWidth * ((MessageTask*)arg0)->scaleX + 2.0f));
            } else {
                count1++;
                width1 = (s16)((f32)width1 + (0.5f * ((f32)glyphWidth * ((MessageTask*)arg0)->scaleX) + 2.0f));
            }
        }
    }

    rubyHeight = 0.5f * ((f32)((MessageTask*)arg0)->unk22 * ((MessageTask*)arg0)->scaleX);
    ((MessageTask*)arg0)->color[2] = count0;
    ((MessageTask*)arg0)->color[3] = count1;
    ((MessageTask*)arg0)->unk4C = ((MessageTask*)arg0)->cursorX + (f32)((width0 - width1) / 2);
    ((MessageTask*)arg0)->unk5C = rubyHeight;
    ((MessageTask*)arg0)->unk50 = -((0.4f * (f32)((MessageTask*)arg0)->unk23) - ((MessageTask*)arg0)->cursorY);

    *(u32 *)(arg0 + 0x30) = PC_PTR32(savedIp);
    arg0[0x40] = savedDepth;
    for (i = 0; i < 3; i++) {
        PTR32_AT_SET_T(u16, arg0 + 0x34, i * 4, resume[i]);
    }
    ((MessageTask*)arg0)->unk45 = savedMode;
    arg0[1] = savedFlag;
}

#ifdef TARGET_PC
#include "pc/orre_hooks.h"
/* The font renderer's hook point (orre_hooks.h, OrreHook_GlyphFetch; docs/port/font-renderer.md, 2026-11-13,
 * step 2.2 part A). Called where a glyph's bitmap is about to be copied into the text atlas; with nothing registered
 * it changes nothing (the disc's bitmap is drawn, as on the GameCube). It passes the task's font id (the game's own
 * per-message font, the "kind of text"), the cell height and the font's baseline: the bottom of 'H' from the cell's
 * top, measured once per font id from the font's own 'H' (the renderer sits every letter on it). The glyph's width is
 * never changed: it is the pen's advance and the quad's width. Platform-neutral. */
/* The current font's baseline and capital height, measured once per font id from its own 'H' (shared by the glyph
 * fetch above and the missing-glyph hook in _msgGetCodeInfo; *split out 2026-11-13, for part D*) */
static void orre_font_metrics(u8* work, s16* outBaseline, s16* outCap) {
    static s16 s_baseline[8];
    static s16 s_cap[8]; /* the rows of 'H' with ink: the font's capital height (part C sizes its fonts by it) */
    static u8 s_measured[8];
    const u16 font = ((MessageTask*)work)->unk20;
    const s16 cell = (s16)((MessageTask*)work)->unk23;
    s16 baseline = cell;
    s16 cap = 0;
    if (font < 8) {
        if (!s_measured[font]) {
            void* node = NULL;
            u8* h = (u8*)_msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(work, 'H', &node);
            s_measured[font] = 1;
            s_baseline[font] = cell;
            if (h != NULL && node != NULL) {
                const struct GlyphEntry* g = (const struct GlyphEntry*)h;
                const u8* px = (const u8*)node + ((struct FontBank*)node)->dataOffset + (g->offset & 0xFFFFFF);
                const s32 stride = (g->width + 1) / 2;
                s32 row, last = -1, capFirst = -1, capLast = -1;
                for (row = 0; row < g->height; row++) {
                    s32 b;
                    for (b = 0; b < stride; b++) {
                        const u8 v = px[row * stride + b];
                        if (v != 0) last = row;
                        /* the capital height counts only rows at least half dark (a nibble of 8 or more), the same rule
                         * the renderer measures its own 'H' by; *corrected 2026-11-13:* counting any ink took
                         * font 3's faint edge row as a 16th row and sized its letters a step too big (measured) */
                        if ((v >> 4) >= 8 || (v & 0x0F) >= 8) {
                            if (capFirst < 0) capFirst = row;
                            capLast = row;
                        }
                    }
                }
                /* the baseline: under the last half-dark row of 'H' (*corrected 2026-11-13:* under the last row
                 * with any ink put font 3's letters a row low, measured against the game: its 'H' has a faint bottom
                 * edge row; font 0's and font 1's 'H' end on a solid row, so theirs doesn't move) */
                if (capLast >= 0) {
                    s_baseline[font] = (s16)((s8)(g->offset >> 24) + capLast + 1);
                    s_cap[font] = (s16)(capLast - capFirst + 1);
                } else if (last >= 0) {
                    s_baseline[font] = (s16)((s8)(g->offset >> 24) + last + 1);
                }
            }
        }
        baseline = s_baseline[font];
        cap = s_cap[font];
    }
    *outBaseline = baseline;
    *outCap = cap;
}

static void orre_glyph_fetch(u8* work, u16 code, s16 width, const u8** pixels, s16* height, s16* yOffset) {
    s16 baseline, cap;
    /* the character whose picture is copied into the atlas next (part B picks its tracer by it: Japanese characters
     * get the kanji tracer; orre_hooks.h, OrreHook_GlyphCode; 2026-11-15) */
    if (OrreHook_GlyphCode != NULL) {
        OrreHook_GlyphCode(code);
    }
    if (OrreHook_GlyphFetch == NULL) {
        return;
    }
    orre_font_metrics(work, &baseline, &cap);
    OrreHook_GlyphFetch(((MessageTask*)work)->unk20, code, width, (s16)((MessageTask*)work)->unk23, baseline, cap,
                        pixels, height, yOffset);
}
#endif

/* _msgSetChar__FP13MSG_TASK_WORKUs (XD NXXJ01.map GSmsg.o, 0x2B4;
 * GXXE01 0x8010A144, trevor403/xd-asm b1087f18 func_FUN_8010a144.s), inlined. */
static inline void msgSetChar(u8* work, u16 code) {
    extern u8 lbl_80314E08[];
    extern u8 lbl_80314F98[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800D9ED8(s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(u32 mask);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(void* tex);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxLoadTexture(s32 index, void* texture);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos2s16(s16 x, s16 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxColor4u8(s32 a, s32 b, s32 c, s32 d, u32 color);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif
    extern void fn_800DC1D4(s32 arg);
    u8* glyph;
    void* outNode;
    s16 drawX0;
    s16 drawX1;
    s16 drawY0;
    s16 drawY1;
    f32 advance;
    s16 glyphWidth; /* last: its web must colour before the draw temps (r29) */

    if (code == 0x20) {
        ((MessageTask*)work)->unk14 = (f32)(((MessageTask*)work)->unk22 / 2) * ((MessageTask*)work)->scaleX;
        return;
    }
    glyph = (u8*)_msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(work, code, &outNode);
    if (glyph == NULL) {
        drawX0 = (s16)((MessageTask*)work)->cursorX;
        drawY0 = (s16)((s32)((MessageTask*)work)->cursorY + 2);
        drawX1 = (s16)((f32)((MessageTask*)work)->unk22 * ((MessageTask*)work)->scaleX + (f32)drawX0);
        drawY1 = (s16)((f32)((MessageTask*)work)->unk23 * ((MessageTask*)work)->scaleY + (f32)drawY0);
        gfxClearFlags(0x80000002u);
        gfxSetPrimitive(7);
        gfxSetVtxDesc(lbl_80314E08);
        gfxBegin(2);
        gfxVtxPos2s16(drawX0, drawY0);
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxVtxPos2s16(drawX1, drawY1);
        gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
        gfxEnd();
        gfxSetFlags(0x80000002u);
        gfxSetVtxDesc(lbl_80314F98);
        fn_800DC1D4(1);
        advance = (f32)((MessageTask*)work)->unk22 * ((MessageTask*)work)->scaleX;
        ((MessageTask*)work)->unk14 = 2.0f + advance;
    } else {
        glyphWidth = ((struct GlyphEntry*)glyph)->width;
#ifdef TARGET_PC
        { /* the font renderer may swap in its own bitmap (same width; OrreHook_GlyphFetch, orre_glyph_fetch above) */
            const u8* px = (u8*)outNode + ((struct FontBank*)outNode)->dataOffset + (((struct GlyphEntry*)glyph)->offset & 0xFFFFFF);
            s16 h = ((struct GlyphEntry*)glyph)->height;
            s16 yo = (s8)(((struct GlyphEntry*)glyph)->offset >> 24);
            orre_glyph_fetch(work, code, glyphWidth, &px, &h, &yo);
            _msgMakeTexture(work, px, glyphWidth, h, yo);
        }
#else
        _msgMakeTexture(work, (u8*)outNode + ((struct FontBank*)outNode)->dataOffset + (((struct GlyphEntry*)glyph)->offset & 0xFFFFFF),
                    glyphWidth, ((struct GlyphEntry*)glyph)->height, (s8)(((struct GlyphEntry*)glyph)->offset >> 24));
#endif
        ((MessageTask*)work)->unk14 = (f32)glyphWidth * ((MessageTask*)work)->scaleX;
    }
}

/* GSmsgPrintf's task fill: the rest of _msgInitTask (XD NXXJ01.map GSmsg.o,
 * UNUSED 0x6C) followed by the print fields. The caller clears the task
 * itself. Retail fills it through a second pointer (`mr r7,r30`): the task
 * is a pooled static, GC/1.3.2 keeps its offset out of the store
 * displacements, so the fill's addi survives to register allocation and
 * post-allocation CSE rewrites it as a copy of the clear pointer. */
static inline void msgPrintRest(u8* t, u8* text, s32 x, s32 y, u32 color) {
    ((MessageTask*)t)->scaleX = GS_MSG_ONE;
    ((MessageTask*)t)->scaleY = GS_MSG_ONE;
    ((MessageTask*)t)->unk24 = -1;
    P32_SET(((MessageTask*)t)->unk28, text);
    P32_SET(((MessageTask*)t)->unk2C, text);
    PTR32_AT_SET_T(u8, t, 0x30, text);
    ((MessageTask*)t)->startX = (f32)x;
    ((MessageTask*)t)->startY = (f32)y;
    *(u32*)(t + 0x24) = color;
    t[2] = 1;
    ((MessageTask*)t)->unk20 = 2;
}

/* 0x800FAEF8 | 0x544 */
s32 GSmsgPrintf(s32 x, s32 y, u32 color, const char* fmt, ...) {
    typedef struct GSVaList {
        u32 flags;
        void* inputArgArea;
        void* regSaveArea;
    } GSVaList;

    extern u8 lbl_80314F98[];
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800D9ED8(s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetFlags(s32 arg);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxClearFlags(u32 mask);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetVtxDesc(void* tex);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxLoadTexture(s32 index, void* texture);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetPrimitive(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxBegin(s32 mode);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxPos2s16(s16 x, s16 y);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxVtxColor4u8(s32 a, s32 b, s32 c, s32 d, u32 color);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxEnd(void);
#endif
    extern void fn_800DC1D4(s32 arg);

    u8* work;
    u16 code;
    GSVaList args;
    u8* str;

#ifdef TARGET_PC
    {
        /* the host's variadic list; the GameCube passes MWCC's register-save structure (port-gaps 16) */
        __builtin_va_list pcArgs;
        __builtin_va_start(pcArgs, fmt);
        logVsnprintf_float(lbl_804022B0, 0xFF, fmt, pcArgs);
        __builtin_va_end(pcArgs);
    }
#else
    __builtin_va_info(&args);
    logVsnprintf_float(lbl_804022B0, 0xFF, fmt, &args);
#endif
    str = (u8*)lbl_804022B0;
    str[0xFF] = 0;
    SJIStoGSchar(lbl_80401EB0, str);

    /* Cleared through `work`, filled through the static itself (see
     * msgPrintRest). */
    work = lbl_804023B0;
    memset(work, 0, 0x68);
    lbl_804023B0[0] = 1;
    msgPrintRest(lbl_804023B0, (u8*)lbl_80401EB0, x, y, color);
    msgSetFontInfo(work);

    spriteSetEnv();
    fn_800D9ED8(1);
    gfxSetFlags(3);
    gfxClearFlags(4);
    gfxSetVtxDesc(lbl_80314F98);
    gfxLoadTexture(0, P32(_pMw->textures[_pMw->textureIndex]));
    P32_SET(_pMw->image, GStextureLockImage(P32(_pMw->textures[_pMw->textureIndex]), 0));
    ((MessageTask*)work)->cursorX = ((MessageTask*)work)->startX;
    ((MessageTask*)work)->cursorY = ((MessageTask*)work)->startY;

    for (;;) {
        code = GSmsgReadCode(work);
        if (code == 0) {
            break;
        }
        if (code == 0x0A || code == 0x0D) {
            continue;
        }
        msgSetChar(work, code);
        ((MessageTask*)work)->cursorX += ((MessageTask*)work)->unk14;
    }

    GStextureUnlockImage(P32(_pMw->textures[_pMw->textureIndex]));
    return 0;
}

/* GSmsgPrintRect's body. Retail inlines it into the three print wrappers
 * below (XD keeps them as calls: GSmsgPrint2 and GSmsgPrintRight call
 * GSmsgPrintRect, trevor403/xd-asm b1087f18 func_FUN_80108464.s and
 * func_FUN_80108494.s). Out of line, GSmsgPrintRect copies the work address
 * through r0 (as XD's GSmsgPrintRect does); inlined, it is materialized
 * straight into r31, as in the three wrappers. */
static inline s32 msgPrintRect(s32 x, s32 y, s16 width, s16 height, s32 color, u32 key) {
    struct MessageGroup* group;
    void* text;
    u8* work;
    u16 fontId;

    work = (u8*)&lbl_80402418;
    text = GSmsgFindMessage(key, &group);
    if (text == NULL) return -1;

    msgInitTask(work, text);
    fontId = group->fontId;
    ((MessageTask*)work)->unk20 = fontId;
    ((MessageTask*)work)->unk1C = key;
    ((MessageTask*)work)->startX = (f32)x;
    ((MessageTask*)work)->startY = (f32)y;
    ((MessageTask*)work)->unk18 = width;
    ((MessageTask*)work)->unk1A = height;
    ((MessageTask*)work)->unk44 = 3;
    ((MessageTask*)work)->unk24 = color;
    work[2] = 1;

    msgSetFontInfo(work);
#ifdef TARGET_PC
    /* OrrePort: a long name fitted to its fixed place by the print's own scale (orre_hooks.h; NULL: as written) */
    if (OrreHook_MessageFit != NULL) OrreHook_MessageFit(key, work);
#endif
    return msgTaskDraw(work, ((MessageTask*)work)->unk44, 0, 0);
}

/* 0x800FB43C | 0x244 */
/* RULE-EXCEPTION(title-path): local compiler-control pragma â see docs/RULE_EXCEPTIONS.md.
 * Without always_inline, msgPrintRect is inlined but its own inlines
 * (GSmsgFindMessage, msgInitTask, msgSetFontInfo) are called out of line. */
#pragma push
#pragma always_inline on

s32 std001_messageFormatLookup(s32 x, s32 y, u32 key) {
    return msgPrintRect(x, y, 0, 0, -1, key);
}

/* 0x800FB680 | 0x248 */
s32 GSmsgPrintColor(s32 x, s32 y, s32 color, u32 key) {
    return msgPrintRect(x, y, 0, 0, color, key);
}

/* 0x800FB8C8 | 0x26C */
s32 GSmsgPrintRight(s32 x, s32 y, s16 width, s16 height, s32 color, u32 key) {
    x += width - (s16)((u32)GSmsgGetRect(key) >> 16);
    return msgPrintRect(x, y, width, height, color, key);
}

#pragma pop

/* 0x800FBB34 | 0x254 */
s32 GSmsgPrintRect(s32 x, s32 y, s16 width, s16 height, s32 color, u32 key) {
    struct MessageGroup* group;
    void* text;
    u8* work;
    u16 fontId;

    work = (u8*)&lbl_80402418;
    text = GSmsgFindMessage(key, &group);
    if (text == NULL) return -1;

    msgInitTask(work, text);
    fontId = group->fontId;
    ((MessageTask*)work)->unk20 = fontId;
    ((MessageTask*)work)->unk1C = key;
    ((MessageTask*)work)->startX = (f32)x;
    ((MessageTask*)work)->startY = (f32)y;
    ((MessageTask*)work)->unk18 = width;
    ((MessageTask*)work)->unk1A = height;
    ((MessageTask*)work)->unk44 = 3;
    ((MessageTask*)work)->unk24 = color;
    work[2] = 1;

    msgSetFontInfo(work);
#ifdef TARGET_PC
    /* OrrePort: a long name fitted to its fixed place by the print's own scale (orre_hooks.h; NULL: as written) */
    if (OrreHook_MessageFit != NULL) OrreHook_MessageFit(key, work);
#endif
    return msgTaskDraw(work, ((MessageTask*)work)->unk44, 0, 0);
}

/* 0x800FBD88 | 0xF4 */
static inline u32 msgGetSE(u8 type) {
    u32 se = 0;
    switch (type) {
    case 0:
        se = 0;
        break;
    case 1:
        se = 0x57;
        break;
    case 2:
        se = 0x58;
        break;
    case 3:
        se = 0x59;
        break;
    case 4:
        se = 0x497;
        break;
    case 5:
        se = 0x498;
        break;
    }
    return se;
}
void GSmsgFree(u32 key) {
    u8* entry;
    u32 se;

    entry = GSmsgFindCheck((u8*)_pMw, key);
    if (entry == NULL) return;
    se = msgGetSE(entry[3]);
    if (se != 0) soundStopSE(se);
    entry[0] = 0;
}

/* 0x800FBE7C | 0x94 */
s32 winMsgDrawMessage(u32 key, u32 state, u32 flag) {
    u8* head;
    u8* entry;

    head = (u8*)_pMw;
    entry = GSmsgFindCheck(head, key);
    if (entry == NULL) return -1;
    return msgTaskDraw(entry, *(u8*)(entry + 0x44), state, flag);
}

/* 0x800FBF10 | 0x64 */
void GSmsgDaemon(void) {
    GStextureUnlockImage(P32(_pMw->textures[_pMw->textureIndex]));
    _pMw->atlasX = 2;
    _pMw->atlasY = 1;
    _pMw->textureIndex ^= 1;
}

/* 0x800FBF74 | 0x25C */
s32 GSmsgExec(key, mode, type)
    u32 key;
    s8 mode;
    s8 type;
{
    u8* mgr;
    u8* work;
    struct MessageGroup* group;
    void* text;
    s32 count;
    s32 i;

    mgr = (u8*)_pMw;
    count = *(u16*)mgr;
    for (i = 0; i < count; i++) {
        work = PTR32_AT_T(u8, mgr, 0x20) + i * 0x68;
        if (work[0] == 0) {
            break;
        }
    }
    if (i == count) {
        GSlogWrite(lbl_80271730, key);
        return -1;
    }

    text = GSmsgFindMessage(key, &group);
    if (text == NULL) {
        GSlogWrite(lbl_80271754, key);
        return -1;
    }

    msgInitTask(work, text);
    ((MessageTask*)work)->unk20 = group->fontId;
    ((MessageTask*)work)->unk1C = key;
    *(s8*)(work + 0x44) = mode;
    ((MessageTask*)work)->unk03 = type;

    msgSetFontInfo(work);
#ifdef TARGET_PC
    /* OrrePort: the same fit as the prints above, for text started here (2026-11-15: the battle command box's
     * "<name> はどうする？" never reached the prints' hook; orre_hooks.h; NULL: as written) */
    if (OrreHook_MessageFit != NULL) OrreHook_MessageFit(key, work);
#endif
    return 0;
}

#endif /* !GS_MSG_PARTIAL */

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_OPENCLOSE_ONLY)
/* 0x800FC1D0 | 0x74 */
s32 GSmsgClose(struct MessageGroup* group) {
    struct MessageSystem* system;
    struct MessageGroup* node;

    system = _pMw;
    if (P32(system->groups) == NULL) return -1;
    node = P32(system->groups);
    while (node != NULL) {
        if (node == group) {
            if (DP(node->previous) != NULL) DP_SET(DP(node->previous)->next, DP(node->next));
            else P32_SET(system->groups, DP(node->next));
            if (DP(node->next) != NULL) DP_SET(DP(node->next)->previous, DP(node->previous));
            break;
        }
        node = DP(node->next);
    }
    return 0;
}

/* 0x800FC244 | 0x64. Includes the compiler's trailing return at 0x800FC2A4. */
struct MessageGroup* GSmsgOpen(struct MessageGroup* group) {
    struct MessageSystem* system;
    struct MessageGroup* node;

    system = _pMw;
    if (P32(system->groups) == NULL) {
        P32_SET(system->groups, group);
        DP_SET(group->next, NULL);
        DP_SET(group->previous, NULL);
        return group;
    }
    node = P32(system->groups);
    while (1) {
        if (node == group) return NULL;
        if (DP(node->next) == NULL) {
            DP_SET(node->next, group);
            DP_SET(group->next, NULL);
            DP_SET(group->previous, node);
            return group;
        }
        node = DP(node->next);
    }
}

/* 0x800FC2A8 | 0xF4 */
s32 GSmsgFontClose(void* ptr) {
    struct FontFileHeader* file;
    struct MessageSystem* system;
    s32 count;
    struct FontSlot* entry;
    struct FontBank* bank;
    struct FontBank* previous;
    s32 idx;

    file = (struct FontFileHeader*)ptr;
    while (1) {
        system = _pMw;
        count = system->fontCount;
        for (idx = 0; idx < count; idx++) {
            entry = &P32(system->fonts)[idx];
            if (entry->bank != NULL) {
                if (entry->id == file->id) break;
            }
        }
        if (idx != count) {
            bank = entry->bank;
            while (bank != NULL) {
                if (bank == (struct FontBank*)(file + 1)) {
                    previous = DP(bank->previous);
                    if (previous == NULL && DP(bank->next) == NULL) {
                        entry->id = 0xFFFF;
                        entry->bank = NULL;
                    } else {
                        if (previous != NULL) {
                            DP_SET(previous->next, DP(bank->next));
                        } else {
                            entry->bank = DP(bank->next);
                        }
                        if (DP(bank->next) != NULL) {
                            DP_SET(DP(bank->next)->previous, DP(bank->previous));
                        }
                    }
                    break;
                }
                bank = DP(bank->next);
            }
        }
        if (file->nextOffset == 0) break;
        file = (struct FontFileHeader*)((u8*)file + file->nextOffset);
    }
    return 0;
}

/* 0x800FC39C | 0x17C */
void* GSmsgFontOpen(void* ptr) {
    struct FontFileHeader* file;
    struct MessageSystem* system;
    struct FontSlot* entry;
    struct FontBank* bank;
    struct FontBank* node;
    u16 key;
    s32 count;
    s32 idx;
    s32 slot;

    file = (struct FontFileHeader*)ptr;
    while (1) {
        key = file->id;
        if (key == 0xFFFF) return NULL;

        system = _pMw;
        count = system->fontCount;
#ifdef TARGET_PC
        { /* PC, observe only: each font registration (the COLOSSEUM BATTLE list's font-2 text drew as boxes). *Added
             * 2026-11-13 (the text map for step 2.2):* the bank's glyph count and its first and last character,
             * to settle where font 3's letters come from (fonts.md, open questions) */
            extern void OSReport(const char*, ...);
            struct FontBank* b = (struct FontBank*)(file + 1);
            const u32 n = b->count;
            OSReport("[font] open id %u (cell %u x %u) from %p, %d slots, %u glyphs U+%04X-U+%04X\n", (unsigned)key,
                     (unsigned)file->width, (unsigned)file->height, (void*)file, (int)count, (unsigned)n,
                     n > 0 ? (unsigned)b->glyphs[0].code : 0u, n > 0 ? (unsigned)b->glyphs[n - 1].code : 0u);
            { /* the font lab (2026-11-13; docs/port/font-renderer.md "The font lab"): with ORRE_FONT_LAB=<folder>
               * the renderer writes every glyph of this bank twice, the game's picture and its own, for scoring. Does
               * nothing otherwise. Platform-neutral (the renderer writes the files) */
                extern void pc_font_lab_dump(u16 fontId, u8 cellWidth, u8 cellHeight, const void* bank);
                pc_font_lab_dump(key, file->width, file->height, b);
            }
        }
#endif
        for (idx = 0; idx < count; idx++) {
            entry = &P32(system->fonts)[idx];
            if (entry->bank != NULL) {
                if (entry->id == key) break;
            }
        }
        if (idx == count) {
            for (slot = 0; slot < count; slot++) {
                entry = &P32(system->fonts)[slot];
                if (entry->bank == NULL) {
#ifdef TARGET_PC
                    /* the GameCube copies the 8-byte disc header over the slot (id, width, height; the second word is
                       then overwritten by the bank pointer); the PC slot is native, so copy the fields */
                    entry->id = file->id;
                    entry->width = file->width;
                    entry->height = file->height;
#else
                    *(struct FontFileHeader*)entry = *file;
#endif
                    bank = (struct FontBank*)(file + 1);
                    entry->bank = bank;
                    DP_SET(bank->next, NULL);
                    DP_SET(bank->previous, NULL);
                    break;
                }
            }
            if (slot == _pMw->fontCount) {
                GSlogWrite((const char*)lbl_8027177C, file->id);
            }
        } else {
            bank = (struct FontBank*)(file + 1);
            node = entry->bank;
            while (1) {
                if (node == bank) return NULL;
                if (DP(node->next) == NULL) {
                    DP_SET(node->next, bank);
                    DP_SET(bank->next, NULL);
                    DP_SET(bank->previous, node);
                    break;
                }
                node = DP(node->next);
            }
        }
        if (file->nextOffset == 0) break;
        file = (struct FontFileHeader*)((u8*)file + file->nextOffset);
    }
    return ptr;
}

/* 0x800FC518 | 0x10 */
s32 GSmsgSetCtrlFunc(struct MessageControl* controls) {
    P32_SET(_pMw->controls, controls);
    return 0;
}

#endif

#if !defined(GS_MSG_PARTIAL) || defined(GS_MSG_INIT_ONLY)
/* 0x800FC528 | 0x2B8 */
s32 GSmsgInit(u16 taskCount, u16 fontCount) {
    u8* work;
    struct FontSlot* slot;
    s32 index;

    memset(&lbl_804024E8, 0, sizeof(struct MessageSystem));
    _pMw->taskHandle = _toolentryAlloc__FUl(taskCount * (s32)sizeof(MessageTask));
    if (_pMw->taskHandle == 0) {
        GSlogWrite((const char*)lbl_802717B4);
        return -1;
    }
    P32_SET(_pMw->tasks, GSmemLock(_pMw->taskHandle));

    _pMw->fontHandle = _toolentryAlloc__FUl(fontCount * sizeof(struct FontSlot));
    if (_pMw->fontHandle == 0) {
        GSlogWrite((const char*)lbl_802717B4);
        return -1;
    }
    P32_SET(_pMw->fonts, GSmemLock(_pMw->fontHandle));

    for (index = 0; index < taskCount; index++) {
        work = P32(_pMw->tasks) + index * (s32)sizeof(MessageTask);
        memset(work, 0, sizeof(MessageTask));
        ((MessageTask*)work)->scaleX = GS_MSG_ONE;
        ((MessageTask*)work)->scaleY = GS_MSG_ONE;
    }
    _pMw->taskCount = taskCount;
    for (index = 0; index < fontCount; index++) {
        slot = &P32(_pMw->fonts)[index];
        slot->id = 0xFFFF;
        slot->bank = NULL;
    }
    _pMw->fontCount = fontCount;

    P32_SET(_pMw->textures[0], GStextureCreate(0x200, 0x200, 0x40, 0, 0));
    P32_SET(_pMw->textures[1], GStextureCreate(0x200, 0x200, 0x40, 0, 0));
#ifdef TARGET_PC
    { /* the font renderer's part B (port/src/pc_text_atlas.cpp; docs/port/font-renderer.md): a sharper copy of each
       * atlas, which Aurora draws instead; it learns the two atlas images' addresses here (the same pointers
       * _msgMakeTexture writes through, P32(_pMw->image)). A lock / unlock pair only reads the address. All platforms */
        extern void pc_text_atlas_init(const void* image0, const void* image1);
        void* image0 = GStextureLockImage(P32(_pMw->textures[0]), 0);
        void* image1 = GStextureLockImage(P32(_pMw->textures[1]), 0);
        GStextureUnlockImage(P32(_pMw->textures[0]));
        GStextureUnlockImage(P32(_pMw->textures[1]));
        pc_text_atlas_init(image0, image1);
    }
#endif
    return 0;
}

#endif

#if !defined(GS_MSG_PARTIAL)
/* 0x800FC7E0 | 0xB68
 * XD _msgMainSub__FP13MSG_TASK_WORKUcUlb (NXXJ01.map GSmsg.o). Retail keeps
 * arg1 and arg3 unnarrowed in their registers and truncates them at each
 * test (clrlwi inside the loops), so they are word-sized here and cast at
 * use; the K&R u8 form narrows once and hoists that out of the loop. */
s32 msgTaskDraw(u8* arg0, u32 arg1, u32 arg2, u32 arg3)
{
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxLoadTexture(s32, u32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void gfxSetTevStage(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DBF78(s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DC0D4(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DC14C(s32, s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DBFD4(s32, s32, s32, s32, s32);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
    extern void fn_800DC04C(s32, s32, s32, s32, s32, s32);
#endif
    u8 *mgr;
    u16 *cursor;
    u16 *savedCursor;
    u16 code;
    u8 control;
    s8 stackDepth;
    u8 quoteFlag;
    u8 glyphWidth;
    u8 normalFlag;
    u8 savedDepth;
    u8 continueFlag;
    s32 loopCount;
    u32 soundId;
    s32 drawY;
    s32 drawX;
    u32 texHandle;
    f32 angle;
    f32 amp;
    void *fontNode;
    void *fontInfo;
    u32 savedStack[3];
    GXColor color = lbl_8047CD00;
    u32* saved;

    quoteFlag = 0;
    normalFlag = 0;
    if (arg0 == NULL) {
        return -1;
    }
    if (arg0[0] == 0) {
        return -1;
    }

    spriteSetEnv();
    gfxSetFlags(0x80000003);
    gfxClearFlags(4);
    gfxSetVtxDesc(lbl_80314F98);

    mgr = (u8*)_pMw;
    gfxLoadTexture(0, PC_PTR32(P32(_pMw->textures[_pMw->textureIndex])));
    fn_800DC1D4(1);
    gfxSetTevStage(0, 0, 0, 0, 0);
    fn_800DBEB4(0, color);
    fn_800DBF78(0, 0x0C);
    fn_800DC0D4(0, 0x0F, 0x0E, 0x0A, 0x0F);
    fn_800DC14C(0, 0, 0, 0, 1, 0);
    fn_800DBFD4(0, 7, 4, 5, 7);
    fn_800DC04C(0, 0, 0, 0, 1, 0);

    P32_SET(_pMw->image, GStextureLockImage(P32(_pMw->textures[_pMw->textureIndex]), 0));

    if (arg2 & 0x30) {
        ((MessageTask*)arg0)->unk45 = 1;
    } else {
        ((MessageTask*)arg0)->unk45 = 0;
    }
    arg0[1] = 0;
    ((MessageTask*)arg0)->mode = 0;
    ((MessageTask*)arg0)->unk46 = 0;
    continueFlag = 0;

    if ((u8)arg3 == 0) {
        for (loopCount = 0; (u32)loopCount < GSgetFrameStep(); loopCount++) {
            for (;;) {
                code = GSmsgReadCode(arg0);
                if (code == 0) {
                    arg0[0] = 2;
                    break;
                }

                if (code == 0xFFFF) {
                    cursor = P32(((MessageTask*)arg0)->textCursor);
                    P32_SET(((MessageTask*)arg0)->textCursor, (u16*)((u8*)cursor + 1));
                    control = *(u8*)cursor;
                    continueFlag = GSmsgDispatchControl(arg0, control);
                } else if (((MessageTask*)arg0)->mode != 2) {
                    continueFlag = 1;
                    if ((u8)arg1 != 0) {
                        continueFlag = 0;
                    }
                    normalFlag = 1;
                }

                if (continueFlag != 0) {
                    break;
                }
            }
        }
    }

    /* RULE-EXCEPTION(title-path): pointer copy whose only effect is register
     * allocation (retail hoists the array address into saved r25) - see
     * docs/RULE_EXCEPTIONS.md */
    saved = savedStack;
    savedStack[0] = *(u32*)(arg0 + 0x34);
    savedStack[1] = *(u32*)(arg0 + 0x38);
    savedStack[2] = *(u32*)(arg0 + 0x3C);
    savedCursor = P32(((MessageTask*)arg0)->textCursor);
    savedDepth = ((MessageTask*)arg0)->textDepth;

    arg0[1] = 1;
    ((MessageTask*)arg0)->cursorX = ((MessageTask*)arg0)->startX;
    ((MessageTask*)arg0)->cursorY = ((MessageTask*)arg0)->startY;
    *(u32*)(arg0 + 0x30) = *(u32*)(arg0 + 0x2C);
    ((MessageTask*)arg0)->textDepth = 0;
    ((MessageTask*)arg0)->mode = 0;

    for (;;) {
        if (P32(((MessageTask*)arg0)->textCursor) == savedCursor) {
            s32 i;
            stackDepth = ((MessageTask*)arg0)->textDepth;
            for (i = 0; i < stackDepth; i++) {
                if (*(u32*)(arg0 + 0x34 + i * 4) != saved[i]) {
                    break;
                }
            }
            if (i == stackDepth) {
                break;
            }
        }

        code = GSmsgReadCode(arg0);
        if (code == 0) {
            break;
        }

        if (code == 0xFFFF) {
            cursor = P32(((MessageTask*)arg0)->textCursor);
            P32_SET(((MessageTask*)arg0)->textCursor, (u16*)((u8*)cursor + 1));
            control = *(u8*)cursor;
            GSmsgDispatchControl(arg0, control);
            if (quoteFlag != 0 && ((MessageTask*)arg0)->cursorX == ((MessageTask*)arg0)->startX) {
                ((MessageTask*)arg0)->cursorX += (f32)(s32)((MessageTask*)arg0)->unk22;
            }
        } else if (((MessageTask*)arg0)->mode != 2) {
            msgSetChar(arg0, code);
            ((MessageTask*)arg0)->cursorX += ((MessageTask*)arg0)->unk14;
            if ((s8)((MessageTask*)arg0)->unk41 == 0) {
                if (code == 0x300C) {
                    quoteFlag = 1;
                }
                if (code == 0x300D) {
                    quoteFlag = 0;
                }
            }
            if (((MessageTask*)arg0)->mode == 1) {
                msgDrawRuby(arg0);
            }
        }
    }

    if ((u8)arg1 != 0) {
        normalFlag = 0;
    }
    if (normalFlag != ((MessageTask*)arg0)->unk47) {
        soundId = msgGetSE(arg0[3]);
        if (soundId != 0) {
            if (normalFlag != 0) {
                soundStartSE(soundId);
            } else {
                soundStopSE(soundId);
            }
        }
        ((MessageTask*)arg0)->unk47 = normalFlag;
    }

    if (((MessageTask*)arg0)->unk46 != 0) {
        angle = ((3.1415927f * (f32)lbl_8047AC68) / 25.0f);
        drawX = (s32)(2.0f + ((MessageTask*)arg0)->cursorX);
        drawY = (s32)(8.0f + ((MessageTask*)arg0)->cursorY);
        amp = (f32)cos(angle);
        drawY += (s32)(4.0f * amp);
        lbl_8047AC68 += GSgetFrameStep();
        lbl_8047AC68 %= 0x32;
        gfxClearFlags(0x80000000);
        gfxSetVtxDesc(lbl_80314E08);
        windowDrawSprite(PC_PTR(drawX), PC_PTR(drawY), 0, 0xB9, 0);
        gfxSetFlags(0x80000000);
        gfxSetVtxDesc(lbl_80314F98);
        fn_800DC1D4(1);
    }

    *(u32*)(arg0 + 0x30) = PC_PTR32(savedCursor);
    arg0[0x40] = savedDepth;
    *(u32*)(arg0 + 0x34) = savedStack[0];
    *(u32*)(arg0 + 0x38) = savedStack[1];
    *(u32*)(arg0 + 0x3C) = savedStack[2];

    mgr = (u8*)_pMw;
    GStextureUnlockImage(P32(_pMw->textures[_pMw->textureIndex]));
    gfxClearFlags(0x80000000);
    return 0;
}

/* 0x800FD348 | 0x354 */
void msgDrawRuby(u8* arg0)
{
    u8 targetCount;
    u8 drawCount;
    u8 *scan;
    s16 y1;
    s16 y0;
    s16 x1;
    s16 x0;
    u8 glyphHeight;
    void *fontNode;
    u16 code;
    f32 savedX;
    f32 savedY;
    f32 scaleX;
    f32 scaleY;
    u8 glyphWidth;
    void *fontInfo;

    ((MessageTask*)arg0)->color[0]++;
    targetCount = (s32)(((MessageTask*)arg0)->color[3] * ((MessageTask*)arg0)->color[0]) / (s32)((MessageTask*)arg0)->color[2];
    drawCount = targetCount - ((MessageTask*)arg0)->color[1];
    savedX = ((MessageTask*)arg0)->cursorX;
    savedY = ((MessageTask*)arg0)->cursorY;
    ((MessageTask*)arg0)->cursorX = ((MessageTask*)arg0)->unk4C;
    ((MessageTask*)arg0)->cursorY = ((MessageTask*)arg0)->unk50;
    ((MessageTask*)arg0)->scaleX *= 0.5f;
    ((MessageTask*)arg0)->scaleY *= 0.5f;

    scan = P32(((MessageTask*)arg0)->unk54);

    while (drawCount != 0) {
#ifdef TARGET_PC
        /* the text is big-endian disc data: read it as such (port fix, 2026-11-15: read natively, a furigana
         * reading's letters came out byte-swapped on the PC, hiragana あ U+3042 as U+4230, drawn as unrelated CJK
         * characters or white boxes over the kanji; the US game has no furigana, so the Japanese text was the first
         * to reach it) */
        code = (u16)((scan[0] << 8) | scan[1]);
#else
        code = *(u16 *)scan;
#endif
        scan += 2;
        if (code == 0xFFFF) {
            scan++;
            continue;
        }

        if (code == 0x20) {
            ((MessageTask*)arg0)->unk14 = (f32)((((MessageTask*)arg0)->unk22 / 2) * ((MessageTask*)arg0)->scaleX);
        } else {
            fontInfo = _msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(arg0, code, &fontNode);
            if (fontInfo == NULL) {
                glyphHeight = ((MessageTask*)arg0)->unk23;
                x0 = (s16)((MessageTask*)arg0)->cursorX;
                y0 = (s16)((s32)((MessageTask*)arg0)->cursorY + 2);
                scaleX = ((MessageTask*)arg0)->scaleX;
                scaleY = ((MessageTask*)arg0)->scaleY;
                x1 = (s16)((((MessageTask*)arg0)->unk22 * scaleX) + (f32)x0);
                y1 = (s16)(((f32)glyphHeight * scaleY) + (f32)y0);

                gfxClearFlags(0x80000002);
                gfxSetPrimitive(7);
                gfxSetVtxDesc(lbl_80314E08);
                gfxBegin(2);
                gfxVtxPos2s16(x0, y0);
                gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
                gfxVtxPos2s16(x1, y1);
                gfxVtxColor4u8(0, 0xFF, 0xFF, 0xFF, 0xFF);
                gfxEnd();
                gfxSetFlags(0x80000002);
                gfxSetVtxDesc(lbl_80314F98);
                fn_800DC1D4(1);

                scaleX = ((MessageTask*)arg0)->unk22 * ((MessageTask*)arg0)->scaleX;
                ((MessageTask*)arg0)->unk14 = 2.0f + scaleX;
            } else {
                glyphWidth = ((struct GlyphEntry *)fontInfo)->width;
#ifdef TARGET_PC
                { /* ruby text: the same font renderer hook as msgSetChar (OrreHook_GlyphFetch) */
                    const u8* px = (u8 *)fontNode + ((struct FontBank *)fontNode)->dataOffset +
                                   (((struct GlyphEntry *)fontInfo)->offset & 0xFFFFFF);
                    s16 h = ((struct GlyphEntry *)fontInfo)->height;
                    s16 yo = (s8)(((struct GlyphEntry *)fontInfo)->offset >> 0x18);
                    orre_glyph_fetch(arg0, code, glyphWidth, &px, &h, &yo);
                    _msgMakeTexture(arg0, px, glyphWidth, h, yo);
                }
#else
                _msgMakeTexture(arg0,
                            (u8 *)fontNode + ((struct FontBank *)fontNode)->dataOffset +
                                (((struct GlyphEntry *)fontInfo)->offset & 0xFFFFFF),
                            glyphWidth, ((struct GlyphEntry *)fontInfo)->height,
                            (s8)(((struct GlyphEntry *)fontInfo)->offset >> 0x18));
#endif
                ((MessageTask*)arg0)->unk14 = (f32)((s16)glyphWidth * ((MessageTask*)arg0)->scaleX);
            }
        }

        drawCount--;
        ((MessageTask*)arg0)->cursorX += ((MessageTask*)arg0)->unk14;
    }

    ((MessageTask*)arg0)->unk4C = ((MessageTask*)arg0)->cursorX;
    ((MessageTask*)arg0)->unk50 = ((MessageTask*)arg0)->cursorY;
    *(u32 *)(arg0 + 0x54) = PC_PTR32(scan);
    ((MessageTask*)arg0)->color[1] = (u8)targetCount;
    ((MessageTask*)arg0)->cursorX = savedX;
    ((MessageTask*)arg0)->cursorY = savedY;
    ((MessageTask*)arg0)->scaleX *= 2.0f;
    ((MessageTask*)arg0)->scaleY *= 2.0f;
}

/* 0x800FD69C | 0x880 (XD _msgMakeTexture)
 * The swizzle store is written ((hi << 5) + lo) + buffer: MWCC's address
 * lowering takes the inner sum's first term as the stbx base and adds the
 * rest (add lo,lo,image; stbx v,hi,lo). "* 32" or buffer[...] reorders it.
 * The second loop reuses `row`, so its counter is a split web and the
 * zero is shared (li r3,0; mr r0,r3). */
void _msgMakeTexture(u8* arg0, const u8* arg1, s16 arg2, s16 arg3, s16 arg4)
{
    s32 xPos;
    s32 widthRounded;
    u8 outlineAlpha;
    u8 *mgr;
    s16 curX;
    s16 curY;
    s32 row;
    u8 *buffer;
    s32 rowPos;
    s16 drawX0;
    s16 drawX1;
    s16 drawY0;
    s16 drawY1;
    s32 srcOffset;
    s32 destOffset;
    s32 tileOffset;
    s32 texelOffset;
    const u8 *srcRow;
    u32 color;
    f32 scaleX;
    f32 scaleY;
    f32 baseX;
    f32 baseY;
    f32 u0;
    f32 u1;
    f32 v0;
    f32 v1;

    mgr = (u8 *)_pMw;
    if (*(s16 *)(mgr + 0x18) + arg2 + 2 >= 0x200) {
        *(s16 *)(mgr + 0x1A) += *(u8 *)(mgr + 0x1C) + 2;
        mgr = (u8*)_pMw;
        *(s16 *)(mgr + 0x18) = 2;
        _pMw->unk1C = ((MessageTask*)arg0)->unk23 + 2;
        mgr = (u8*)_pMw;
        if (*(s16 *)(mgr + 0x1A) + *(u8 *)(mgr + 0x1C) >= 0x200) {
            *(s16 *)(mgr + 0x1A) = 1;
        }
    }

    mgr = (u8*)_pMw;
    if (*(u8 *)(mgr + 0x1C) < arg3) {
        *(u8 *)(mgr + 0x1C) = ((MessageTask*)arg0)->unk23;
    }

    buffer = (u8*)P32(_pMw->image);
    widthRounded = ((arg2 + 1) & ~1) / 2;

    for (row = -1; row < arg3 + 1; row++) {
        rowPos = _pMw->atlasY + row;
        tileOffset = (rowPos >> 3) * 64;
        texelOffset = (rowPos & 7) << 3;
        for (xPos = -2; xPos < arg2 + 2; xPos += 2) {
            srcOffset = _pMw->atlasX + xPos;
            *(u8*)((((srcOffset >> 3) + tileOffset) << 5) +
                   (((srcOffset & 7) + texelOffset) >> 1) + buffer) = 0;
        }
    }

    for (row = 0, srcOffset = 0; row < arg3; row++) {
        rowPos = _pMw->atlasY + row;
        tileOffset = (rowPos >> 3) * 64;
        texelOffset = (rowPos & 7) << 3;
        srcRow = arg1 + srcOffset;
        for (xPos = 0; xPos < arg2; xPos += 2) {
            destOffset = _pMw->atlasX + xPos;
            *(u8*)((((destOffset >> 3) + tileOffset) << 5) +
                   (((destOffset & 7) + texelOffset) >> 1) + buffer) = *srcRow++;
        }
        srcOffset = srcOffset + widthRounded;
    }

#ifdef TARGET_PC
    { /* the font renderer's part B: the same picture, traced sharp, into the high-resolution copy of this atlas at the
       * same place (port/src/pc_text_atlas.cpp). Look only: the game's own atlas is written as before. All platforms */
        extern void pc_text_atlas_glyph(const void* image, int atlasX, int atlasY, int width, int height,
                                        const u8* pixels);
        pc_text_atlas_glyph(buffer, _pMw->atlasX, _pMw->atlasY, arg2, arg3, arg1);
    }
#endif
    curX = _pMw->atlasX;
    curY = _pMw->atlasY;
    scaleX = ((MessageTask*)arg0)->scaleX;
    scaleY = ((MessageTask*)arg0)->scaleY;
    baseX = ((MessageTask*)arg0)->cursorX;
    baseY = (f32)(s8)((MessageTask*)arg0)->unk43 + (((f32)(s16)arg4 * scaleY) + ((MessageTask*)arg0)->cursorY);
    drawX0 = (s16)baseX;
    drawY0 = (s16)baseY;
    drawX1 = (s16)(((f32)(s16)arg2 * scaleX) + (f32)drawX0);
    drawY1 = (s16)(((f32)(s16)arg3 * scaleY) + (f32)drawY0);

    u0 = (f32)curX / 512.0f;
    v0 = (f32)curY / 512.0f;
    u1 = (f32)(curX + arg2) / 512.0f;
    v1 = (f32)(curY + arg3) / 512.0f;

    color = *(u32 *)(arg0 + 0x24);
    outlineAlpha = (u8)(((color & 0xFF) * 0xC0) / 0xFF);

    gfxSetPrimitive(7);

    switch (arg0[2]) {
    case 0:
    default:
        gfxBegin(2);
        break;

    case 1:
        gfxBegin(4);
        gfxVtxPos2s16(drawX0 + 1, drawY0 + 1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u0, v0);
        gfxVtxPos2s16(drawX1 + 1, drawY1 + 1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u1, v1);
        break;

    case 2:
        gfxBegin(0xA);
        gfxVtxPos2s16(drawX0 - 1, drawY0);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u0, v0);
        gfxVtxPos2s16(drawX1 - 1, drawY1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u1, v1);
        gfxVtxPos2s16(drawX0 + 1, drawY0);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u0, v0);
        gfxVtxPos2s16(drawX1 + 1, drawY1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u1, v1);
        gfxVtxPos2s16(drawX0, drawY0 - 1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u0, v0);
        gfxVtxPos2s16(drawX1, drawY1 - 1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u1, v1);
        gfxVtxPos2s16(drawX0, drawY0 + 1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u0, v0);
        gfxVtxPos2s16(drawX1, drawY1 + 1);
        gfxVtxColor4u8(0, 0, 0, 0, outlineAlpha);
        gfxVtxTexCoord(0, u1, v1);
        break;

    }

    gfxVtxPos2s16(drawX0, drawY0);
    gfxVtxColor1u32(0, *(u32*)(arg0 + 0x24));
    gfxVtxTexCoord(0, u0, v0);
    gfxVtxPos2s16(drawX1, drawY1);
    gfxVtxColor1u32(0, *(u32*)(arg0 + 0x24));
    gfxVtxTexCoord(0, u1, v1);
    gfxEnd();

    mgr = (u8*)_pMw;
    *(s16 *)(mgr + 0x18) += widthRounded * 2 + 2;
}

/* 0x800FDF1C | 0xC8 */
u16* _msgGetCodeInfo__FP13MSG_TASK_WORKUsPP12tagFONT_INFO(u8* work, u16 code, void** outBank) {
    u8* head;
    s32 count;
    struct FontSlot* slot;
    s32 index;
    struct FontBank* bank;
    struct GlyphEntry* entries;
    struct GlyphEntry* entry;
    u32 low;
    u32 high;
    u32 mid;

    head = (u8*)_pMw;
    count = *(u16*)(head + 4);
    for (index = 0; index < count; index++) {
        slot = PTR32_AT_T(struct FontSlot, head, 0x24) + index;
        if (slot->id == ((MessageTask*)work)->unk20) break;
    }
#ifdef TARGET_PC
    if (index == count) { /* PC, observe only */
        extern void OSReport(const char*, ...);
        static u16 lastFont = 0xFFFF;
        if (((MessageTask*)work)->unk20 != lastFont) {
            OSReport("[font] font %u not open (code 0x%04X)\n", (unsigned)((MessageTask*)work)->unk20, (unsigned)code);
            lastFont = ((MessageTask*)work)->unk20;
        }
    }
#endif
    if (index == count) return NULL;

    bank = slot->bank;
    while (bank != NULL) {
        high = bank->count;
        entries = bank->glyphs;
        low = 0;
        while (low < high) {
            mid = (low + high) / 2;
            entry = &entries[mid];
            /* RULE-EXCEPTION(title-path): cast whose only effect is register allocation (the widened code stays in r4) - see docs/RULE_EXCEPTIONS.md */
            if (entry->code == (u32)code) {
                if (outBank != NULL) *outBank = bank;
                return (u16*)entry;
            }
            if (entry->code < code) low = mid + 1;
            else high = mid;
        }
        /* Retail re-tests the search bounds here (a second blt on the
         * loop's CR) and leaves the bank walk if they are still open. */
        if (low < high) break;
        bank = DP(bank->next);
    }
#ifdef TARGET_PC
    { /* PC, observe only: a character the current font has no glyph for (drawn as a white box, or by part D below).
       * *Changed 2026-11-16 (the project's build helper; snatcherLeo's log of 08:25 had 10,216 of these lines):* logged once per
       * (font, letter) for the run. Before, only a repeat of the very last miss was skipped, so a name with two or more
       * such letters (an Arabic nickname on screen) logged every letter every frame. A bit per letter for fonts 0-7
       * (64 KB; the game has fonts 0-5), the old last-miss test for any higher font id. All platforms the same. */
        extern void OSReport(const char*, ...);
        static u32 lastMiss;
        static u8 seenMiss[8][0x10000 / 8];
        const u32 font_ = ((MessageTask*)work)->unk20;
        u32 miss = (font_ << 16) | code;
        int first_;
        if (font_ < 8) {
            first_ = (seenMiss[font_][(u16)code >> 3] & (1 << (code & 7))) == 0;
            seenMiss[font_][(u16)code >> 3] |= (u8)(1 << (code & 7));
        } else {
            first_ = miss != lastMiss;
        }
        if (first_) {
            OSReport("[font] no glyph 0x%04X in font %u (slot %d of %d), text %p cursor %p depth %u\n", (unsigned)code,
                     (unsigned)((MessageTask*)work)->unk20, (int)index, (int)count,
                     (void*)P32(((MessageTask*)work)->unk28), (void*)P32(((MessageTask*)work)->textCursor),
                     (unsigned)work[0x40]);
            lastMiss = miss;
        }
    }
    /* The font renderer's part D (orre_hooks.h, OrreHook_GlyphMissing; docs/port/font-renderer.md, 2026-11-13):
     * a character the current font has no glyph for (the game draws a white box) can get a glyph record from the
     * renderer instead, laid out like the disc's (a big-endian {code, width, height, offset} in a bank whose data
     * offset leads to its picture), so drawing, measuring and line-wrapping all treat it as a letter. 'H' is never
     * asked (the metrics are measured from it). NULL: the white box, as on the GameCube. Platform-neutral. */
    if (OrreHook_GlyphMissing != NULL && code != 'H') {
        s16 baseline, cap;
        void* bank = NULL;
        u16* entry;
        orre_font_metrics(work, &baseline, &cap);
        entry = (u16*)OrreHook_GlyphMissing(((MessageTask*)work)->unk20, code, (s16)((MessageTask*)work)->unk23,
                                            baseline, cap, &bank);
        if (entry != NULL) {
            if (outBank != NULL) *outBank = bank;
            return entry;
        }
    }
#endif
    return NULL;
}

#ifdef TARGET_PC
/* Whether an open game font has its own glyph for a character (the project's IME work, 2026-11-15; port/src/pc_name_keyboard.cpp
 * through pc_game_ui.font_has). Why: the naming keyboard now takes whatever the operating system's input method types
 * (kana, kanji, full-width letters), and the GameCube name in the save may keep only letters a GameCube can draw
 * ("GameCube-safe": fonts 0 and 4 both have them; decided 2026-11-13, pc_gcopy.c). The same bank walk as
 * _msgGetCodeInfo above, but never asking the font renderer's part D (which would draw a missing letter from the
 * open fonts and so always say yes). 1 the font has it, 0 not, -1 the font isn't open. PC only; all OSes. */
s32 orre_font_has_glyph(u16 fontId, u16 code) {
    u8* head = (u8*)_pMw;
    struct FontSlot* slot = NULL;
    struct FontBank* bank;
    s32 count, index;
    if (head == NULL) return -1;
    count = *(u16*)(head + 4);
    for (index = 0; index < count; index++) {
        slot = PTR32_AT_T(struct FontSlot, head, 0x24) + index;
        if (slot->id == fontId) break;
    }
    if (index == count || slot == NULL) return -1;
    for (bank = slot->bank; bank != NULL; bank = DP(bank->next)) {
        u32 low = 0, high = bank->count;
        while (low < high) {
            u32 mid = (low + high) / 2;
            if (bank->glyphs[mid].code == (u32)code) return 1;
            if (bank->glyphs[mid].code < code) low = mid + 1;
            else high = mid;
        }
    }
    return 0;
}
#endif

/* 0x800FDFE4 | 0x2C */
s32 _msgGetLength__FPCUs(const void* str) {
    s32 r;
    r = _msgGetSize__FPCUs(str);
    return (s32)(((u32)r + 1) >> 1) - 1;
}

/* 0x800FE010 | 0x34C */
s32 _msgGetSize__FPCUs(const u16* arg0)
{
    u8 *work;
    u16 code;
    u8 *ip;
    u32 control;

    if (arg0 == NULL) {
        GSlogWrite((const char *)lbl_802717D4);
        return 0;
    }

    work = (u8 *)&lbl_80402480;
    memset(work, 0, 0x68);
    lbl_80402480[0] = 1;
    *(f32 *)(lbl_80402480 + 0x60) = GS_MSG_ONE;
    *(f32 *)(lbl_80402480 + 0x64) = GS_MSG_ONE;
    *(s32 *)(lbl_80402480 + 0x24) = -1;
    *(u32 *)(lbl_80402480 + 0x28) = PC_PTR32(arg0);
    *(u32 *)(lbl_80402480 + 0x2C) = PC_PTR32(arg0);
    *(u32 *)(lbl_80402480 + 0x30) = PC_PTR32(arg0);
    lbl_80402480[1] = 1;
    msgSetFontInfo(work);

    for (;;) {
        code = GSmsgReadCode(work);
        if (code == 0) break;
        if (code == 0xFFFF) {
            ip = PTR32_AT_T(u8, work, 0x30);
            PTR32_AT_SET_T(u8, work, 0x30, ip + 1);
            control = *ip;
            GSmsgDispatchControl(work, control);
        }
    }

    return ((u8 *)PC_PTR(*(u32 *)(work + 0x30)) - (u8 *)arg0) + 2;
}
#endif /* !GS_MSG_PARTIAL */
