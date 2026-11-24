/*
 * pc_zip_scrub_test: the two pieces Studio shares with OrrePort's Report a problem (2026-11-22, 4.3 part 3):
 * pc_zip.c (the stored-zip writer, now without SDL) and pc_scrub.c (personal details out of report texts).
 *
 * Why: both moved off SDL so Studio can compile them, and on Windows both now go through UTF-8 -> UTF-16 for paths and
 * environment variables (a home folder like C:\Users\Jörg\ broke the old ANSI calls). This checks, with no game and no
 * SDL:
 *   1. the scrub rule: the home folder in both slash styles and any ASCII case, the user name, a name too short to
 *      replace, the temp folder and the folder under it, with a home folder and user name outside ASCII;
 *   2. a zip written to a folder whose name is outside ASCII, written again over itself (the .new file put in place),
 *      and every entry found again with pc_zip_find.
 *
 * Usage: pc_zip_scrub_test <scratch folder>      (the zip goes into <scratch folder>/Prüfung ü/)
 *        pc_zip_scrub_test --scrub <in> <out>    (the real environment's scrub of one file, to compare by hand)
 *        pc_zip_scrub_test --clipboard <file>    (Windows: pc_clip_file.c, the report copied for Ctrl+V; it puts back
 *                                                 the clipboard's text, and skips when the clipboard holds more)
 * Platforms: plain C. The environment set-up below is Windows (SetEnvironmentVariableW, _wmkdir); Linux and macOS would
 * use setenv and mkdir (not written yet: the test says so and skips part 1 there).
 */
#include "pc_clip_file.h"
#include "pc_scrub.h"
#include "pc_zip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <direct.h>
#include <windows.h>
#include <shellapi.h> /* DragQueryFileW, for the clipboard test */
#endif

static int s_failed = 0;

static void check(int ok, const char* what) {
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) s_failed = 1;
}

static int scrub_file(const char* in, const char* out) {
    FILE* f = pc_fopen_utf8(in, "rb");
    if (f == NULL) return 1;
    fseek(f, 0, SEEK_END);
    const long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    const size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    size_t len = 0;
    char* s = pc_scrub(buf, got, &len);
    FILE* o = pc_fopen_utf8(out, "wb");
    if (s == NULL || o == NULL) return 1;
    fwrite(s, 1, len, o);
    fclose(o);
    pc_scrub_free(s);
    free(buf);
    return 0;
}

#ifdef _WIN32
/* --clipboard <file>: pc_clipboard_set_file, read back as the shell reads a paste (DragQueryFileW). The clipboard is
 * someone's: it runs only when the clipboard holds nothing or plain text, and puts that text back afterwards */
static int clipboard_test(const char* file) {
    wchar_t* saved = NULL;
    if (!OpenClipboard(NULL)) return 2;
    UINT f = 0;
    int other = 0;
    while ((f = EnumClipboardFormats(f)) != 0) {
        if (f != CF_UNICODETEXT && f != CF_TEXT && f != CF_OEMTEXT && f != CF_LOCALE) other = 1;
    }
    if (!other) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        const wchar_t* t = h != NULL ? (const wchar_t*)GlobalLock(h) : NULL;
        if (t != NULL) {
            saved = _wcsdup(t);
            GlobalUnlock(h);
        }
    }
    CloseClipboard();
    if (other) {
        printf("skip clipboard test: the clipboard holds more than text (it would be lost)\n");
        return 0;
    }
    check(pc_clipboard_set_file(file) == 1, "clipboard: the file put on it");
    wchar_t got[1300] = L"";
    UINT count = 0;
    if (OpenClipboard(NULL)) {
        HANDLE h = GetClipboardData(CF_HDROP);
        if (h != NULL) {
            count = DragQueryFileW((HDROP)h, 0xFFFFFFFF, NULL, 0);
            DragQueryFileW((HDROP)h, 0, got, 1300);
        }
        CloseClipboard();
    }
    wchar_t want[1300];
    MultiByteToWideChar(CP_UTF8, 0, file, -1, want, 1300);
    for (int i = 0; want[i] != 0; ++i) {
        if (want[i] == L'/') want[i] = L'\\';
    }
    check(count == 1 && wcscmp(got, want) == 0, "clipboard: one file, its path read back as a paste reads it");
    if (OpenClipboard(NULL)) { /* the text that was there before (or nothing) */
        EmptyClipboard();
        if (saved != NULL) {
            const size_t bytes = (wcslen(saved) + 1) * sizeof(wchar_t);
            HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (g != NULL) {
                memcpy(GlobalLock(g), saved, bytes);
                GlobalUnlock(g);
                if (SetClipboardData(CF_UNICODETEXT, g) == NULL) GlobalFree(g);
            }
        }
        CloseClipboard();
    }
    free(saved);
    printf("%s\n", s_failed ? "FAILED" : "all passed");
    return s_failed;
}
#endif

int main(int argc, char** argv) {
    if (argc == 4 && strcmp(argv[1], "--scrub") == 0) return scrub_file(argv[2], argv[3]);
#ifdef _WIN32
    if (argc == 3 && strcmp(argv[1], "--clipboard") == 0) return clipboard_test(argv[2]);
#endif
    if (argc != 2) {
        fprintf(stderr, "usage: pc_zip_scrub_test <scratch folder> | --scrub <in> <out>\n");
        return 2;
    }
#ifdef _WIN32
    /* 1. the scrub rule, with a home folder and user name outside ASCII ("Jörg Ä" in UTF-16) */
    SetEnvironmentVariableW(L"USERPROFILE", L"C:\\Users\\J\x00F6rg \x00C4");
    SetEnvironmentVariableW(L"USERNAME", L"J\x00F6rg \x00C4");
    /* the temp folder inside the home folder, as Windows puts it: it must go first, folder under it included */
    SetEnvironmentVariableW(L"TEMP", L"C:\\Users\\J\x00F6rg \x00C4\\AppData\\Local\\Temp\\");
    const char* in = "a C:\\Users\\J\xC3\xB6rg \xC3\x84\\x.log b c:/users/J\xC3\xB6RG \xC3\x84/y c J\xC3\xB6rg \xC3\x84 d "
                     "C:\\Users\\J\xC3\xB6rg \xC3\x84\\AppData\\Local\\Temp\\sometool\\e "
                     "c:/users/J\xC3\xB6rg \xC3\x84/AppData/Local/Temp/sometool/f";
    const char* want = "a <user>\\x.log b <user>/y c <user> d <temp>\\<scratch>\\e <temp>/<scratch>/f";
    size_t n = 0;
    char* s = pc_scrub(in, strlen(in), &n);
    check(s != NULL && n == strlen(want) && strcmp(s, want) == 0,
          "scrub: home (both slashes, any ASCII case), user, temp folder and the folder under it");
    if (s != NULL && strcmp(s, want) != 0) printf("     got: %s\n", s);
    pc_scrub_free(s);
    SetEnvironmentVariableW(L"USERPROFILE", L"C:\\Users\\Al");
    SetEnvironmentVariableW(L"USERNAME", L"Al");
    s = pc_scrub("Also Al", 7, &n);
    check(s != NULL && strcmp(s, "Also Al") == 0, "scrub: a name under 3 letters is left alone");
    pc_scrub_free(s);
#else
    printf("skip scrub rule: the test's environment set-up is Windows-only so far\n");
#endif

    /* 2. a zip in a folder outside ASCII, written twice, read back */
    char dir[1200], path[1300];
    snprintf(dir, sizeof(dir), "%s/Pr\xC3\xBC" "fung \xC3\xBC", argv[1]);
#ifdef _WIN32
    wchar_t wdir[1200];
    MultiByteToWideChar(CP_UTF8, 0, dir, -1, wdir, 1200);
    _wmkdir(wdir);
#endif
    snprintf(path, sizeof(path), "%s/report.zip", dir);
    const char big[] = "the log's text\n";
    const PcZipFile files[3] = {{"note.txt", "a note", 6}, {"log/run.log", big, sizeof(big) - 1}, {"empty.bin", "", 0}};
    check(pc_zip_write(path, files, 3) == 1, "zip: written into a folder outside ASCII");
    check(pc_zip_write(path, files, 2) == 1, "zip: written again over itself");
    FILE* f = pc_fopen_utf8(path, "rb");
    check(f != NULL, "zip: opened again by its UTF-8 path");
    if (f != NULL) {
        char buf[4096];
        const size_t got = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        size_t sz = 0;
        const void* p = pc_zip_find(buf, got, "log/run.log", &sz);
        check(pc_zip_is_zip(buf, got), "zip: starts like a zip");
        check(p != NULL && sz == sizeof(big) - 1 && memcmp(p, big, sz) == 0, "zip: log/run.log found with its bytes");
        check(pc_zip_find(buf, got, "empty.bin", &sz) == NULL, "zip: the second write replaced the first (2 entries)");
    }
    char tmp[1310];
    snprintf(tmp, sizeof(tmp), "%s.new", path);
    FILE* t = pc_fopen_utf8(tmp, "rb");
    check(t == NULL, "zip: no .new file left behind");
    if (t != NULL) fclose(t);
    printf("%s\n", s_failed ? "FAILED" : "all passed");
    return s_failed;
}
