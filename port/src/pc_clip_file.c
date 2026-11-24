/*
 * pc_clip_file.c: see pc_clip_file.h (why, who shares it, and the platforms).
 *
 * How (Windows): the clipboard needs an owner window, or SetClipboardData fails after EmptyClipboard (the Win32
 * documentation for OpenClipboard with a NULL window). So the file makes its own message-only window once and keeps it
 * for the life of the program: no caller has to pass its window (Studio has several, OrrePort's is SDL's), and the
 * window outliving the call keeps the clipboard's owner valid. The data is a DROPFILES header followed by the path in
 * UTF-16 and two terminating zeros (one list of one file), exactly what Explorer puts there for a single copied file.
 */
#include "pc_clip_file.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN /* Studio's build defines it on the command line */
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlobj.h> /* DROPFILES */
#include <string.h>

static HWND owner_window(void) {
    static HWND s_owner = NULL;
    if (s_owner == NULL) {
        s_owner = CreateWindowExW(0, L"STATIC", L"OrrePort clipboard", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL,
                                  GetModuleHandleW(NULL), NULL);
    }
    return s_owner;
}

int pc_clipboard_set_file(const char* utf8_path) {
    if (utf8_path == NULL || utf8_path[0] == '\0') return 0;
    const int wn = MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, NULL, 0); /* characters, the final zero included */
    if (wn <= 1) return 0;
    const SIZE_T bytes = sizeof(DROPFILES) + ((SIZE_T)wn + 1) * sizeof(wchar_t); /* the path, its zero, the list's zero */
    HGLOBAL drop = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    HGLOBAL effect = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(DWORD));
    if (drop == NULL || effect == NULL) {
        if (drop != NULL) GlobalFree(drop);
        if (effect != NULL) GlobalFree(effect);
        return 0;
    }
    DROPFILES* df = (DROPFILES*)GlobalLock(drop);
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    wchar_t* path = (wchar_t*)((char*)df + sizeof(DROPFILES));
    MultiByteToWideChar(CP_UTF8, 0, utf8_path, -1, path, wn);
    for (int i = 0; path[i] != 0; ++i) {
        if (path[i] == L'/') path[i] = L'\\'; /* the shell wants Windows' separators */
    }
    GlobalUnlock(drop);
    *(DWORD*)GlobalLock(effect) = DROPEFFECT_COPY;
    GlobalUnlock(effect);

    int ok = 0;
    if (OpenClipboard(owner_window())) {
        EmptyClipboard();
        ok = SetClipboardData(CF_HDROP, drop) != NULL;
        if (ok) drop = NULL; /* the clipboard owns it now */
        const UINT effect_format = RegisterClipboardFormatW(L"Preferred DropEffect");
        if (ok && effect_format != 0 && SetClipboardData(effect_format, effect) != NULL) effect = NULL;
        CloseClipboard();
    }
    if (drop != NULL) GlobalFree(drop);
    if (effect != NULL) GlobalFree(effect);
    return ok;
}

#else

int pc_clipboard_set_file(const char* utf8_path) {
    (void)utf8_path; /* not written for Linux / macOS yet (pc_clip_file.h): the caller says "saved", not "copied" */
    return 0;
}

#endif
