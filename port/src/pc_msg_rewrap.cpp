/*
 * pc_msg_rewrap.cpp: dialogue re-wrapping (2026-11-14).
 *
 * Why: the game never checks a line against its text box. The writers put every line break (control 0x00) and every
 * new box (0x03) into the text by hand, sized for names of the console's lengths. OrrePort's longer names (12 letters,
 * decided 2026-11-14), translations and modded lines can therefore run past the box's right edge. snatcherLeo chose automatic
 * re-wrapping as the safety net (decisions log 2026-11-14, mockup 97: "We can do what you propose then, makes sense"):
 *   - the writer's own breaks stay;
 *   - only the words that would cross the box's edge move down a line;
 *   - a new box (the A-press arrow) is added only when the box has no room for another line.
 * A message whose lines all fit is left exactly as written, byte for byte, so every vanilla line with the console's
 * names draws as on the GameCube.
 *
 * How:
 *   1. The game calls OrreHook_MessageBox (win_msg.c, winMsgDraw) every frame a message window draws, with the
 *      message's key and the box's text item size. The battle window (0x50) sizes itself to its message and isn't
 *      passed.
 *   2. On the first frame, before any letter is revealed (the task's cursor still at its text's start), the text is
 *      measured piece by piece with the game's own walker (pc_game_ui.msg_measure, in src/game/gs_msg.c): the same
 *      font, widths, scale and inserts the game will draw with.
 *   3. When a piece would end past the box's right edge, the last space before it on that line becomes a line break,
 *      or a new box if the box is full. A writer's line break that no longer fits the box becomes a new box as well.
 *   4. The new text is a copy in low memory (the game keeps text pointers in 32-bit slots); the task's start, page
 *      start and cursor are pointed at it. The game then reveals and draws it as its own.
 *   Words are moved whole: an insert (a name, an item name) is one piece, so a break goes before it, never inside it.
 *   A single word wider than the box stays as it is (nothing better to do).
 *
 * GameCube-accurate mode: off, so the game's own layout runs unchanged (snatcherLeo: "in Gamecube-accurate mode, just default
 * to the default game's thing"). Automated runs: off unless ORRE_REWRAP=1, so recorded routes keep the console's box
 * count (a new box means one more A press). With the console's names no vanilla line should overflow; the replay test
 * with ORRE_REWRAP=1 checks that (every re-wrap is logged).
 *
 * Test switch: ORRE_REWRAP_TEST_WIDTH=<px> narrows every box to that width, so ordinary lines wrap (the mechanism seen
 * anywhere). Test switch: ORRE_TEST_HERO_NAME=<text> shows that name wherever the game inserts the player's name (controls 0x2B
 * Hero and 0x13 MyName), through OrreHook_MessageInsert, so a long name can be tried on any save without changing it.
 *
 * Platforms: plain C++ with no OS calls; the same on Windows, Linux and macOS.
 * XD: the message system (GSmsg) and the window types are shared engine code (likely the same walker and hook spot;
 * XD's box sizes are its own).
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <SDL3/SDL.h>

#include "pc_font_files.h" /* stand-in codes for the test name's emoji */
#include "pc_lowmem.h"
#include "pc_options.h"

extern "C" {
#include "dolphin/types.h"
int pc_platform_automated_run(void); /* pc_platform.c */
extern void (*OrreHook_MessageBox)(u32 key, s32 width, s32 height);
extern void* (*OrreHook_MessageInsert)(u32 control, void* text);
}
#include "pc_game_ui.h"

namespace {

/* the text a line may reach: the box's text item width, less a small margin (likely; the P13 replay's log of the
 * widest vanilla line per box confirms that no line the writers made is wrapped) */
const float kRightMargin = 4.f;
const int kUnits = 1024;      /* pieces per message (the longest vanilla e-mails are a few hundred) */
const int kBufBytes = 4096;   /* bytes per re-laid-out copy */
const int kBufCount = 8;      /* copies kept alive: one message window shows at a time; a few frames of overlap */

PcMsgUnit s_units[kUnits];
uint8_t* s_buf[kBufCount];
int s_bufNext = 0;
/* the message last examined, so each is measured once, not every frame */
uint8_t* s_seenTask = nullptr;
uint32_t s_seenText = 0, s_seenKey = 0;
/* the widest line seen per box width in messages left as written (the margin's check, logged when it grows) */
float s_widest[2048];

/* ORRE_REWRAP=1 turns it on in any run, accurate preset included: the test that replays a the reference playthrough sitting (recorded in
 * accurate mode) with re-wrapping on, to prove no line the writers made gets wrapped */
bool enabled() {
    if (!pc_game_ui.ready || pc_game_ui.msg_measure == nullptr) return false;
    static const bool forced = [] {
        const char* force = SDL_getenv("ORRE_REWRAP");
        return force != nullptr && force[0] == '1';
    }();
    if (forced) return true;
    return !pc_option_gamecube_accurate() && !pc_platform_automated_run();
}

uint32_t u32_at(const uint8_t* p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
float f32_at(const uint8_t* p) {
    float v;
    std::memcpy(&v, p, 4);
    return v;
}

struct Edit {
    uint32_t offset; /* where the replaced code starts */
    uint32_t oldLen; /* 2 for a space, 3 for a line break */
    uint8_t code;    /* the control written instead: 0x00 a line break, 0x03 a new box */
};

/* test switch: ORRE_REWRAP_TEST_WIDTH=<px> treats every box as at most that wide, so ordinary dialogue wraps and the
 * mechanism can be seen anywhere (a 12-letter name overflows only about a dozen of the game's lines) */
int test_width() {
    static const int w = [] {
        const char* v = SDL_getenv("ORRE_REWRAP_TEST_WIDTH");
        return v != nullptr ? std::atoi(v) : 0;
    }();
    return w;
}

void message_box(u32 key, s32 width, s32 height) {
    if (test_width() > 0 && width > test_width()) width = test_width();
    if (width <= 0 || height <= 0 || !enabled()) return;
    uint8_t* task = pc_game_ui.msg_task(key);
    if (task == nullptr) return;
    const uint32_t text = u32_at(task + 0x28), cursor = u32_at(task + 0x30);
    if (text == 0 || cursor != text) return; /* already revealing: too late to change the layout */
    for (uint8_t* b : s_buf)
        if (b != nullptr && text == (uint32_t)(uintptr_t)b) return; /* our own copy */
    if (task == s_seenTask && text == s_seenText && key == s_seenKey) return;
    s_seenTask = task;
    s_seenText = text;
    s_seenKey = key;

    const int n = pc_game_ui.msg_measure(task, s_units, kUnits);
    if (n <= 0 || s_units[n - 1].kind != PC_MSG_END) return;

    const float startX = f32_at(task + 0x04);
    const float limit = startX + (float)width - kRightMargin;
    /* how many lines a box holds: the box's height over the line height (cell height plus spacing, times the scale:
     * msgctrlCR's step), and never fewer than the writer put on one box of this message (some boxes are taller) */
    const float lineH = ((float)task[0x23] + (float)(int8_t)task[0x42]) * f32_at(task + 0x64);
    int capacity = lineH > 0.f ? (int)((float)height / lineH) : 1;
    {
        int lines = 1;
        for (int i = 0; i < n; i++) {
            if (s_units[i].kind == PC_MSG_CR && ++lines > capacity) capacity = lines;
            if (s_units[i].kind == PC_MSG_PAGE) lines = 1;
        }
    }
    if (capacity < 1) capacity = 1;

    std::vector<Edit> edits;
    int lines = 1, lastSpace = -1, newBoxes = 0;
    float shift = 0.f, widest = 0.f; /* shift: how far the current line's start sits right of startX in the walk */
    for (int i = 0; i < n; i++) {
        const PcMsgUnit& u = s_units[i];
        switch (u.kind) {
        case PC_MSG_CR:
            if (++lines > capacity) { /* the writer's break, but the box is full now: a new box instead */
                edits.push_back({u.offset, 3, 0x03});
                lines = 1;
                newBoxes++;
            }
            shift = u.x_after - startX;
            lastSpace = -1;
            break;
        case PC_MSG_PAGE:
            lines = 1;
            shift = u.x_after - startX;
            lastSpace = -1;
            break;
        case PC_MSG_SPACE: lastSpace = i; break;
        case PC_MSG_LETTER:
        case PC_MSG_CONTROL: {
            const float x = u.x_after - shift;
            if (x > widest) widest = x;
            if (x > limit && lastSpace >= 0) {
                const bool box = ++lines > capacity;
                edits.push_back({s_units[lastSpace].offset, 2, (uint8_t)(box ? 0x03 : 0x00)});
                if (box) {
                    lines = 1;
                    newBoxes++;
                }
                shift = s_units[lastSpace].x_after - startX;
                lastSpace = -1;
            }
            break;
        }
        default: break;
        }
    }
    if (edits.empty()) {
        const int w = width < 2048 ? width : 2047;
        if (widest - startX > s_widest[w]) {
            s_widest[w] = widest - startX;
            std::printf("[rewrap] widest line left as written: %.0f px in a %d px box (message 0x%X)\n",
                        (double)s_widest[w], width, (unsigned)key);
        }
        return;
    }

    /* the copy: the text with each chosen space (2 bytes) or line break (3 bytes) replaced by a 3-byte control */
    const uint32_t total = s_units[n - 1].offset + 2; /* through the ending 0x0000 */
    uint32_t size = total;
    for (const Edit& e : edits) size += 3 - e.oldLen;
    if (size > (uint32_t)kBufBytes) return;
    uint8_t*& buf = s_buf[s_bufNext];
    if (buf == nullptr) buf = static_cast<uint8_t*>(pc_lowmem_alloc(kBufBytes));
    if (buf == nullptr) return;
    s_bufNext = (s_bufNext + 1) % kBufCount;
    const uint8_t* src = (const uint8_t*)(uintptr_t)text;
    uint32_t from = 0, to = 0;
    for (const Edit& e : edits) {
        std::memcpy(buf + to, src + from, e.offset - from);
        to += e.offset - from;
        buf[to++] = 0xFF;
        buf[to++] = 0xFF;
        buf[to++] = e.code;
        from = e.offset + e.oldLen;
    }
    std::memcpy(buf + to, src + from, total - from);
    const uint32_t mine = (uint32_t)(uintptr_t)buf;
    std::memcpy(task + 0x28, &mine, 4);
    std::memcpy(task + 0x2C, &mine, 4);
    std::memcpy(task + 0x30, &mine, 4);
    std::printf("[rewrap] message 0x%X re-wrapped: %d line break(s), %d new box(es) (box %d x %d)\n", (unsigned)key,
                (int)edits.size() - newBoxes, newBoxes, width, height);
}

/* ---- the test switch: a long player name without touching the save ---- */
uint8_t* s_testName = nullptr;

void* message_insert(u32 control, void* text) {
    (void)text;
    if (control == 0x2B || control == 0x13) return s_testName; /* msgctrlHero, msgctrlMyName */
    return nullptr;
}

/* UTF-8 to the game's text (big-endian UTF-16, 0-ended), in low memory */
uint8_t* game_text(const char* utf8) {
    std::vector<uint16_t> units;
    for (const unsigned char* p = (const unsigned char*)utf8; *p != 0;) {
        uint32_t c;
        int k;
        if (*p < 0x80) c = *p, k = 1;
        else if (*p >= 0xF0) c = *p & 0x07, k = 4;
        else if (*p >= 0xE0) c = *p & 0x0F, k = 3;
        else c = *p & 0x1F, k = 2;
        for (int j = 1; j < k && p[j] != 0; j++) c = (c << 6) | (p[j] & 0x3F);
        p += k;
        /* *Updated 2026-11-16 (the Noto wiring):* a character past U+FFFF (an emoji) gets its stand-in code, as a
         * typed one does (pc_font_files.h), so the test name can carry emoji too */
        if (c >= 0x10000) c = pc_text_standin(c);
        if (c != 0) units.push_back((uint16_t)c);
    }
    uint8_t* out = static_cast<uint8_t*>(pc_lowmem_alloc(units.size() * 2 + 2));
    if (out == nullptr) return nullptr;
    for (size_t i = 0; i < units.size(); i++) {
        out[i * 2] = (uint8_t)(units[i] >> 8);
        out[i * 2 + 1] = (uint8_t)units[i];
    }
    out[units.size() * 2] = 0;
    out[units.size() * 2 + 1] = 0;
    return out;
}

} // namespace

/* called once at start-up (pc_platform.c). The hook stays registered and asks enabled() each message, so accurate
 * mode always gets the game's own layout */
extern "C" void pc_msg_rewrap_start(void) {
    OrreHook_MessageBox = message_box;
    const char* name = SDL_getenv("ORRE_TEST_HERO_NAME");
    if (name != nullptr && name[0] != 0) {
        s_testName = game_text(name);
        if (s_testName != nullptr) {
            OrreHook_MessageInsert = message_insert;
            std::printf("[rewrap] test switch: the player's name shows as \"%s\" (the save is unchanged)\n", name);
        }
    }
}
