/**
 * @file menu_exact_801038F8.c
 * @brief menu engine, 0x801038F8-0x80103BA8: _menuUpdateKeyInfo, the
 *        per-frame pad/AGB key-state update.
 *
 * Function-boundary carve of the menu TU (see src/game/menu.c), built with
 * the TU's flags (configure.py). Text only: it uses no pooled literal and
 * no jump table (its switches compile to compare trees). Retail calls
 * _menuGetGcKeyInfo and _menuGetAgbKeyInfo rather than inlining them; in
 * this carve they are defined elsewhere, so GC/1.3.2's -inline auto can't
 * expand them either.
 */
#include "dolphin/types.h"

typedef struct tagWINDOW_WORK MenuWindow;

/* Open window (only the field this function reads). */
struct tagWINDOW_WORK {
    u8 pad_00[0x0B];
    u8 keyPort; /* input port(s) the window reads */
};

typedef struct MenuKeyInfo {
    u16 keys;
    u16 previousKeys;
    u16 pressedKeys;
    u16 repeatKeys;
    u16 heldKeys;
    s8 repeatTimer[16];
} MenuKeyInfo;

/* Window system work. */
typedef struct WINDOW_SYS_WORK {
    u8 pad_00[0x0C];
    PTR32_FIELD(MenuWindow) head; /* 0x0C: 32-bit slot on the PC too (pc/ptr32.h) */
    MenuKeyInfo keyInfo;        /* keys of the active window's port(s) */
    MenuKeyInfo portKeyInfo[4]; /* per-port state, updated every frame */
    u8 enablePort;
    s32 lastError;
    u32 activeId;
} MenuSystem;

extern MenuSystem winSysWork;
extern u8 lbl_8047AD04[4]; /* input type per port: 0 none, 1 GC pad, 2 AGB, 3 AGB lost */
extern u8 lbl_8047AD08[4]; /* the same, one frame earlier */

#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 GSgetFrameStep(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
MenuWindow* windowSearchID(u32 id);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
u32 windowGetActiveID(void);
#endif
#ifndef TARGET_PC /* PC: canonical prototype in pc/prototypes.h */
MenuKeyInfo* windowGetPortKeyInfo(u8 ports);
#endif
u8 _menuGetAgbKeyInfo__FlPUs(s32 port, u16* keys);
u8 _menuGetGcKeyInfo__FlPUs(s32 port, u16* keys);

/* 0x801038F8 | 0x2B0 */
/* Retail ignores the work argument and updates the global directly. */
void _menuUpdateKeyInfo__FP15WINDOW_SYS_WORK(MenuSystem* work) {
    MenuWindow* window;
    MenuKeyInfo* keyInfo;
    s32 i;
    s32 port;
    u16 keys;
    u16 pressed;
    u16 repeat;
    u16 held;
    u16 bit;
    u8 type;

    for (port = 0; port < 4; port++) {
        keyInfo = &winSysWork.portKeyInfo[port];
        lbl_8047AD08[port] = lbl_8047AD04[port];
        keys = 0;
        if (_menuGetGcKeyInfo__FlPUs(port, &keys) == 0) {
            if (_menuGetAgbKeyInfo__FlPUs(port, &keys) == 0) {
                keys = 0;
                type = 0;
            } else {
                type = 2;
            }
        } else {
            type = 1;
        }

        /* The case lists are the values the retail compare trees test. */
        switch (type) {
        case 0:
            switch (lbl_8047AD08[port]) {
            case 0:
            case 1:
            default:
                lbl_8047AD04[port] = 0;
                break;
            case 2:
            case 3:
                /* An AGB was unplugged: report it once as key 0x8000. */
                lbl_8047AD04[port] = 3;
                keys = 0x8000;
                break;
            }
            break;
        case 1:
            lbl_8047AD04[port] = 1;
            break;
        case 2:
            switch (lbl_8047AD08[port]) {
            case 0:
            case 2:
            case 3:
            default:
                lbl_8047AD04[port] = 2;
                break;
            case 1:
                lbl_8047AD04[port] = 2;
                break;
            }
            break;
        default:
            lbl_8047AD04[port] = type;
            break;
        }

        keyInfo->previousKeys = keyInfo->keys;
        pressed = (keyInfo->previousKeys ^ 0xFFFF) & keys;
        repeat = 0;
        held = 0;
        for (i = 0; i < 16; i++) {
            bit = 1 << i;
            if (pressed & bit) {
                keyInfo->repeatTimer[i] = 15;
                repeat |= bit;
            } else if (keys & bit) {
                keyInfo->repeatTimer[i] -= GSgetFrameStep();
                if (keyInfo->repeatTimer[i] <= 0) {
                    keyInfo->repeatTimer[i] = 5;
                    repeat |= bit;
                    held |= bit;
                } else {
                    held |= bit & keyInfo->heldKeys;
                }
            }
        }
        if (pressed & 0xF) {
            keyInfo->repeatTimer[0] = 15;
            keyInfo->repeatTimer[1] = 15;
            keyInfo->repeatTimer[2] = 15;
            keyInfo->repeatTimer[3] = 15;
        }
        keyInfo->keys = keys;
        keyInfo->pressedKeys = pressed;
        keyInfo->repeatKeys = repeat;
        keyInfo->heldKeys = held;
    }

    window = windowSearchID(windowGetActiveID());
    if (window == NULL) {
        type = 1;
    } else {
        type = window->keyPort;
    }
    winSysWork.keyInfo = *windowGetPortKeyInfo(type);
#ifdef TARGET_PC
    {
        /* ORRE_MENUKEY_LOG=1 (2026-10-26; observes only): the menu keys the active window sees, whenever they
         * change: "[menukey] retrace,frame,active window,keys,pressed,repeat,held", to find which window takes a press
         * held across a window closing (P12's box: the console's B stays with the yes/no window, the PC's leaves the
         * box) */
        static int on = -1;
        static u32 last_active = 0xFFFFFFFF;
        static u16 last_keys = 0xFFFF, last_pressed = 0xFFFF;
        if (on < 0) {
            extern char* getenv(const char*);
            const char* env = getenv("ORRE_MENUKEY_LOG");
            on = env != NULL && *env != '\0' && *env != '0';
        }
        /* Also every frame a held key repeats, with the frame step and the four direction timers (2026-10-28:
         * the PC's repeats come 2-3 frames after the console's in P07's and P09's box screens) */
        if (on && (windowGetActiveID() != last_active || winSysWork.keyInfo.keys != last_keys ||
                   winSysWork.keyInfo.pressedKeys != last_pressed || winSysWork.keyInfo.repeatKeys != 0)) {
            extern void OSReport(const char*, ...);
            extern u32 VIGetRetraceCount(void);
            extern volatile u32 pc_vi_heartbeat; /* port/src/pc_vi.c */
            const s8* t = winSysWork.keyInfo.repeatTimer;
            last_active = windowGetActiveID();
            last_keys = winSysWork.keyInfo.keys;
            last_pressed = winSysWork.keyInfo.pressedKeys;
            OSReport("[menukey] %u,%u,%u,0x%X,0x%X,0x%X,0x%X,step %u,timers %d %d %d %d\n", (unsigned)VIGetRetraceCount(),
                     (unsigned)pc_vi_heartbeat, (unsigned)last_active, (unsigned)last_keys, (unsigned)last_pressed,
                     (unsigned)winSysWork.keyInfo.repeatKeys, (unsigned)winSysWork.keyInfo.heldKeys,
                     (unsigned)GSgetFrameStep(), t[0], t[1], t[2], t[3]);
        }
    }
#endif
}
