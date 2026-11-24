/*
 * pc_ime_win32.cpp: who draws the input method's (IME's) composition and choice list, per moment, on Windows (the project's
 * IME work, 2026-11-15; docs/port/naming-screen.md "Typing with an input method").
 *
 * Why this file exists: OrrePort's naming keyboard draws what the player is composing in the name's own letter slots
 * and the IME's choices in the game's window style (snatcherLeo approved the design, 2026-11-15). For that SDL must hand the
 * composition and the choices to the app and hide Windows' own strip and list: SDL_HINT_IME_IMPLEMENTED_UI =
 * "composition,candidates". On Windows SDL reads that hint once, when its IME support first starts (IME_Init, at the
 * first SDL_StartTextInput / SDL_StopTextInput of the run; verified in SDL 3.4's SDL_windowskeyboard.c), so it can't be
 * switched on only while the name screen types. But OrrePort's hub and overlay text fields (RmlUi) don't draw a
 * composition themselves and need Windows' own strip and list.
 *
 * How: the hint is set at start-up, before Aurora starts SDL (pc_ime_win32_hint), so SDL's IME support is the
 * app-drawn kind for the whole run; and the game window's message handler is wrapped (pc_ime_win32_install): while
 * OrrePort's name keyboard is not typing, every IME message (WM_IME_SETCONTEXT, ..._STARTCOMPOSITION, ..._COMPOSITION,
 * ..._ENDCOMPOSITION, ..._NOTIFY) skips SDL and goes straight to Windows' default handling, exactly what SDL does with
 * the hint off: Windows draws its strip and list, and the finished text arrives as WM_CHAR, which SDL turns into text
 * input as before. While the name keyboard types (pc_ime_win32_own_ui(1)) the messages go to SDL, which hides Windows'
 * windows and sends SDL_EVENT_TEXT_EDITING / _CANDIDATES. Each switch re-sends WM_IME_SETCONTEXT so the IME's own
 * window picks up which parts it should draw (it reads that only from this message).
 *
 * The "あ IME" tag: SDL doesn't say whether the IME is on, so this file asks Windows (ImmGetOpenStatus and the
 * conversion mode, on the window's own thread: when the IME reports a change, and at each switch) and keeps the answer
 * for the game thread.
 *
 * Platforms: this whole file is Windows only (imm32). Linux: SDL's IBus and fcitx code read the hint live (a hint
 * callback), so the keyboard just sets and clears the hint around its own typing (pc_name_keyboard.cpp; likely, not
 * run there yet); there is no "IME on" query there, so the tag shows while a composition exists. macOS: Cocoa always
 * hands composition (marked text) to the app and draws its own list (likely); nothing like this file is needed.
 */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>

#include <atomic>
#include <cstdio>

#include <SDL3/SDL.h>

namespace {
HWND s_hwnd = nullptr;
WNDPROC s_sdlProc = nullptr;              /* SDL's own window procedure, which we pass messages on to */
std::atomic<int> s_ownUi{0};              /* 1 while OrrePort's name keyboard types (draws composition and choices) */
std::atomic<int> s_open{0};               /* the IME is on and in its native mode (kana, hangul, Chinese) */
std::atomic<unsigned> s_lang{0};          /* the keyboard layout's primary language (LANG_JAPANESE ...) */

bool ime_message(UINT msg) {
    return msg == WM_IME_SETCONTEXT || msg == WM_IME_STARTCOMPOSITION || msg == WM_IME_COMPOSITION ||
           msg == WM_IME_ENDCOMPOSITION || msg == WM_IME_NOTIFY;
}

/* on the window's thread: is the IME on, and in its own script (not typing plain Latin letters)? */
void refresh_status(HWND hwnd) {
    s_lang = PRIMARYLANGID(LOWORD((DWORD_PTR)GetKeyboardLayout(0)));
    HIMC himc = ImmGetContext(hwnd);
    int open = 0;
    if (himc != nullptr) {
        DWORD conv = 0, sentence = 0;
        open = ImmGetOpenStatus(himc) && ImmGetConversionStatus(himc, &conv, &sentence) && (conv & IME_CMODE_NATIVE) != 0;
        ImmReleaseContext(hwnd, himc);
    }
    s_open = open;
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUTLANGCHANGE || msg == WM_IME_SETCONTEXT ||
        (msg == WM_IME_NOTIFY && (wParam == IMN_SETOPENSTATUS || wParam == IMN_SETCONVERSIONMODE)))
        refresh_status(hwnd);
    if (!s_ownUi.load() && ime_message(msg)) return DefWindowProcW(hwnd, msg, wParam, lParam); /* Windows draws it */
    return CallWindowProcW(s_sdlProc, hwnd, msg, wParam, lParam);
}
} // namespace

/* before Aurora starts SDL: SDL's IME support in its app-drawn form for this run (see above) */
extern "C" void pc_ime_win32_hint(void) { SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition,candidates"); }

/* once the window exists (and again if it was made anew): wrap its message handler */
extern "C" void pc_ime_win32_install(SDL_Window* window) {
    if (window == nullptr) return;
    HWND hwnd = (HWND)SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    if (hwnd == nullptr || hwnd == s_hwnd) return;
    WNDPROC old = (WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    if (old == proc) return;
    s_sdlProc = old;
    s_hwnd = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)proc);
    refresh_status(hwnd);
    std::printf("[ime] Windows: the game window's IME messages go to SDL only while the name keyboard types\n");
}

/* the name keyboard starts (1) or stops (0) drawing the IME's parts itself. Called from the game's input hook, which
 * runs on the main thread (the game's fibres and SDL's message pump share it, pc_vi.c), the thread that owns the
 * window, so the message below is handled at once (proc refreshes the tag's state on it) */
extern "C" void pc_ime_win32_own_ui(int on) {
    if (s_ownUi.exchange(on ? 1 : 0) == (on ? 1 : 0) || s_hwnd == nullptr) return;
    /* the IME's window reads which parts to draw from WM_IME_SETCONTEXT only: send it again so the switch takes
     * effect now (SDL masks out the strip and the list when it gets it; DefWindowProc keeps them otherwise) */
    if (GetFocus() == s_hwnd) SendMessageW(s_hwnd, WM_IME_SETCONTEXT, TRUE, (LPARAM)ISC_SHOWUIALL);
}

/* the "あ IME" tag's state: 1 the IME is on in its own script; *lang the layout's primary language */
extern "C" int pc_ime_win32_status(unsigned* lang) {
    if (lang != nullptr) *lang = s_lang.load();
    return s_open.load();
}
#endif
