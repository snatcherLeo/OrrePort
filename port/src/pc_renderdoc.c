/*
 * pc_renderdoc.c: ORRE_RDOC_CAPTURE=<game frame>[,<game frame>...] asks RenderDoc for a GPU frame capture at those
 * game frames, when the game runs under RenderDoc (renderdoccmd capture ...). A test aid for rendering bugs that only
 * show on one renderer (2026-11-04: the Direct3D black ground at some window sizes, parked-enhancements item 3).
 * Off unless set, and nothing happens unless RenderDoc's own DLL is already in the process (this file never loads
 * it). ORRE_RDOC_PATH=<file prefix> sets where the captures go. Windows only for now; elsewhere it does nothing.
 *
 * RenderDoc's in-app API is a table of function pointers (renderdoc_app.h, MIT); only two entries are used here, by
 * their place in the 1.0.0 table, so the header is not needed: 11 SetCaptureFilePathTemplate, 15 TriggerCapture.
 * TriggerCapture captures the next whole presented frame, so the capture holds that game frame or the one after
 * (Aurora presents from its render worker).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define RDOC_MAX 16

void pc_renderdoc_frame(unsigned frame) {
#ifdef _WIN32
    typedef int(__cdecl * GetApiFn)(int version, void** out);
    typedef void(__cdecl * TriggerFn)(void);
    typedef void(__cdecl * PathFn)(const char* prefix);
    static int s_state = -1; /* -1 not set up yet, 0 off, 1 on */
    static void** s_api;
    static unsigned s_frames[RDOC_MAX];
    static int s_count, s_next;

    if (s_state < 0) {
        const char* list = getenv("ORRE_RDOC_CAPTURE");
        s_state = 0;
        if (list == NULL || *list == '\0') {
            return;
        }
        while (*list != '\0' && s_count < RDOC_MAX) {
            char* end;
            const unsigned long f = strtoul(list, &end, 10);
            if (end == list) {
                break;
            }
            s_frames[s_count++] = (unsigned)f;
            list = *end == ',' ? end + 1 : end;
        }
        {
            HMODULE dll = GetModuleHandleA("renderdoc.dll");
            GetApiFn get = dll != NULL ? (GetApiFn)(void*)GetProcAddress(dll, "RENDERDOC_GetAPI") : NULL;
            if (get == NULL || get(10000 /* eRENDERDOC_API_Version_1_0_0 */, (void**)&s_api) != 1 || s_api == NULL) {
                fprintf(stderr, "[renderdoc] ORRE_RDOC_CAPTURE set, but the game is not running under RenderDoc\n");
                return;
            }
        }
        {
            const char* path = getenv("ORRE_RDOC_PATH");
            if (path != NULL && *path != '\0') {
                ((PathFn)s_api[11])(path);
            }
        }
        s_state = 1;
        fprintf(stderr, "[renderdoc] attached; %d capture frame(s) requested\n", s_count);
    }
    if (s_state == 1 && s_next < s_count && frame >= s_frames[s_next]) {
        fprintf(stderr, "[renderdoc] capture requested at game frame %u\n", frame);
        ((TriggerFn)s_api[15])();
        ++s_next;
    }
#else
    (void)frame;
#endif
}
