/*
 * pc_apps_test.c: checks pc_apps.c from outside both programs (2026-11-23). Windows only, like pc_apps.c's
 * working half (Linux and macOS have only its stubs). Every mode names the OrrePort folder (<root>), as pc_apps does:
 * its markers are per folder.
 *
 *   pc_apps_test fake-studio <root>   stands in for Studio: claims "Studio is open" in root, publishes a plain window,
 *                                     and runs until that window is closed (60 s at most). Copied into a test folder
 *                                     as "OrrePort Studio.exe", it also stands in for the launcher (pc_apps_start);
 *                                     then its root is its own folder.
 *   pc_apps_test fake-hub <root> [game]   stands in for OrrePort the same way (Studio's request, 2026-11-23): its
 *                                     "is open", its window, and with "game" the game marker too, so Studio's "Open
 *                                     OrrePort" / "OrrePort is opened" / "Close OrrePort" (refused while the game runs)
 *                                     can be checked without a game window
 *   pc_apps_test state <root>         what the markers say: hub open, Studio open, game running
 *   pc_apps_test close <hub|studio> <root>   pc_apps_close and its answer (1 asked, 0 not open, -1 the game runs)
 *   pc_apps_test show <hub|studio> <root>    pc_apps_show and its answer
 *   pc_apps_test start <hub|studio> <root>   pc_apps_start from that folder
 *   pc_apps_test self                 the rules on their own, in two made-up folders (no other program needed):
 *                                     claims, the per-folder split, the game marker, close refused while it runs;
 *                                     exit code 0 when all pass
 * Why a separate program: the markers are about two processes, so a test inside one program can't show them.
 */
#include "../src/pc_apps.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CLOSE) {
        printf("fake: WM_CLOSE received, closing\n");
        fflush(stdout);
        DestroyWindow(h);
        return 0;
    }
    if (m == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static PcApp app_of(const char* s) { return strcmp(s, "hub") == 0 ? PC_APP_HUB : PC_APP_STUDIO; }

static int fake_app(PcApp app, const char* root, int game) {
    const int first = pc_apps_claim(app, root);
    printf("fake-%s: claim %d in %s\n", app == PC_APP_HUB ? "hub" : "studio", first, root);
    if (!first) return 0;
    if (game) {
        pc_apps_game_begin(root); /* as the hub does at PLAY: Studio must then refuse to close it */
        printf("fake-hub: the game marker is set\n");
    }
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"OrrePortAppsTest";
    RegisterClassW(&wc);
    HWND h = CreateWindowW(L"OrrePortAppsTest", app == PC_APP_HUB ? L"pc_apps_test: fake OrrePort" : L"pc_apps_test: fake Studio",
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 60, 420, 160, NULL, NULL, wc.hInstance, NULL);
    pc_apps_set_window(app, root, h);
    printf("fake: window published\n");
    fflush(stdout);
    MSG msg;
    const DWORD start = GetTickCount();
    for (;;) { /* ends on WM_CLOSE, or after 60 s at most (a test never leaves it open) */
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (GetTickCount() - start > 60000) {
            printf("fake: 60 s passed, ending\n");
            return 0;
        }
        Sleep(10);
    }
}

static int fake_studio(const char* root) { return fake_app(PC_APP_STUDIO, root, 0); }

int main(int argc, char** argv) {
    /* started as "OrrePort Studio.exe" by pc_apps_start: the fake Studio, in its own folder */
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, MAX_PATH);
    if (strstr(self, "OrrePort Studio.exe") != NULL) {
        char* slash = strrchr(self, '\\');
        if (slash != NULL) *slash = '\0';
        return fake_studio(self);
    }
    const char* mode = argc > 1 ? argv[1] : "";
    if (strcmp(mode, "fake-studio") == 0 && argc > 2) return fake_studio(argv[2]);
    if (strcmp(mode, "fake-hub") == 0 && argc > 2) return fake_app(PC_APP_HUB, argv[2], argc > 3 && strcmp(argv[3], "game") == 0);
    if (strcmp(mode, "state") == 0 && argc > 2) {
        printf("hub open %d, studio open %d, game running %d\n", pc_apps_running(PC_APP_HUB, argv[2]),
               pc_apps_running(PC_APP_STUDIO, argv[2]), pc_apps_game_running(argv[2]));
        return 0;
    }
    if (strcmp(mode, "close") == 0 && argc > 3) {
        printf("close %s: %d\n", argv[2], pc_apps_close(app_of(argv[2]), argv[3]));
        return 0;
    }
    if (strcmp(mode, "show") == 0 && argc > 3) {
        printf("show %s: %d\n", argv[2], pc_apps_show(app_of(argv[2]), argv[3]));
        return 0;
    }
    if (strcmp(mode, "start") == 0 && argc > 3) {
        printf("start %s: %d\n", argv[2], pc_apps_start(app_of(argv[2]), argv[3]));
        return 0;
    }
    if (strcmp(mode, "self") == 0) {
        int fails = 0;
#define CHECK(c, what)                                                                                                   \
    do {                                                                                                                 \
        const int ok_ = (c);                                                                                             \
        printf("%s %s\n", ok_ ? "PASS" : "FAIL", what);                                                                  \
        fails += !ok_;                                                                                                   \
    } while (0)
        /* made-up folders: nothing real runs from them */
        const char* a = "C:\\pc_apps_test\\Folder A";
        const char* a_other_spelling = "c:/PC_APPS_TEST/folder a/";
        const char* b = "C:\\pc_apps_test\\Folder B";
        CHECK(!pc_apps_running(PC_APP_STUDIO, a), "Studio not open in folder A before the claim");
        CHECK(pc_apps_claim(PC_APP_STUDIO, a) == 1, "first claim succeeds");
        CHECK(pc_apps_running(PC_APP_STUDIO, a), "Studio reads as open in folder A");
        CHECK(pc_apps_running(PC_APP_STUDIO, a_other_spelling), "... also when folder A is spelled another way");
        CHECK(!pc_apps_running(PC_APP_STUDIO, b), "... but not in folder B");
        CHECK(!pc_apps_running(PC_APP_HUB, a), "the hub doesn't read as open there");
        CHECK(pc_apps_show(PC_APP_STUDIO, a) == 0, "show finds no window before one is published");
        CHECK(pc_apps_start(PC_APP_STUDIO, "C:\\no such folder") == 0, "start without a launcher answers 0");
        CHECK(pc_apps_close(PC_APP_HUB, a) == PC_APPS_CLOSE_NOT_OPEN, "closing a hub that isn't open answers 0");
        pc_apps_game_begin(a);
        CHECK(pc_apps_game_running(a), "the game marker reads as running in folder A once begun");
        CHECK(!pc_apps_game_running(b), "... and not in folder B");
        printf("%s\n", fails ? "SOME CHECKS FAILED" : "all passed");
        return fails ? 1 : 0;
    }
    fprintf(stderr, "usage: pc_apps_test fake-studio <root> | fake-hub <root> [game] | state <root> | close|show|start <hub|studio> <root> | self\n");
    return 2;
}
#else
int main(void) {
    printf("pc_apps_test: Windows only (pc_apps.c has stubs elsewhere)\n");
    return 0;
}
#endif
