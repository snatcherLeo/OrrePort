/*
 * pc_scrub.c: see pc_scrub.h (why it exists and who uses it).
 *
 * How: each name is replaced in turn, building a new copy per name (a report's texts are at most a few MB and this runs
 * once per file when a report is made, so the simple way is plenty fast). Matching ignores ASCII case only: Windows paths
 * are case-insensitive, and the same folder is printed with the same accents everywhere, so ASCII folding covers what
 * logs show (the old version used SDL_strncasecmp and SDL's home folder; verified 2026-11-22: two of snatcherLeo's logs, 1.9 MB
 * and 31 KB, and his settings.ini scrubbed identically by both rules, port/test/pc_zip_scrub_test.c --scrub).
 */
#include "pc_scrub.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN /* Studio's build defines it on the command line */
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

static int same_ignoring_case(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i])) return 0;
    }
    return 1;
}

/* every `from` in s (length *len) becomes `to`; s is freed and the new copy returned (NULL when out of memory) */
static char* replace_all(char* s, size_t* len, const char* from, const char* to) {
    const size_t fl = strlen(from), tl = strlen(to);
    if (s == NULL || fl < 3) return s;
    size_t hits = 0;
    for (size_t i = 0; i + fl <= *len;) {
        if (same_ignoring_case(s + i, from, fl)) {
            ++hits;
            i += fl;
        } else {
            ++i;
        }
    }
    if (hits == 0) return s;
    const size_t new_len = *len - hits * fl + hits * tl;
    char* out = (char*)malloc(new_len + 1);
    if (out == NULL) {
        free(s);
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; i < *len;) {
        if (i + fl <= *len && same_ignoring_case(s + i, from, fl)) {
            memcpy(out + o, to, tl);
            o += tl;
            i += fl;
        } else {
            out[o++] = s[i++];
        }
    }
    out[o] = '\0';
    free(s);
    *len = new_len;
    return out;
}

/* the length of "dir<sep>name" at s + i (name: up to the next slash, quote, blank or line end), or 0 when none starts
 * there */
static size_t temp_match(const char* s, size_t len, size_t i, const char* dir, size_t dl, char sep) {
    if (i + dl + 1 > len || !same_ignoring_case(s + i, dir, dl) || s[i + dl] != sep) return 0;
    size_t j = i + dl + 1;
    while (j < len && s[j] != '\\' && s[j] != '/' && s[j] != '"' && s[j] != '\'' && s[j] != ' ' && s[j] != '\t' &&
           s[j] != '\r' && s[j] != '\n')
        ++j;
    return j - i;
}

/* every "<temp folder><sep><first folder under it>" in s becomes "<temp><sep><scratch>"; s is freed and the new copy
 * returned (NULL when out of memory). Why the folder under it too: test tools and other programs make their own folder
 * there, and its name says which program made the run (2026-11-24: this replaced a rule that matched one tool's folder
 * by name, so no tool's name is needed in the code; players' temp folders get the same treatment) */
static char* replace_temp(char* s, size_t* len, const char* dir, char sep) {
    const size_t dl = strlen(dir);
    if (s == NULL || dl < 3) return s;
    char to[] = "<temp>?<scratch>";
    to[6] = sep;
    const size_t tl = strlen(to);
    size_t hits = 0, gone = 0;
    for (size_t i = 0; i < *len;) {
        const size_t m = temp_match(s, *len, i, dir, dl, sep);
        if (m != 0) {
            ++hits;
            gone += m;
            i += m;
        } else {
            ++i;
        }
    }
    if (hits == 0) return s;
    const size_t new_len = *len - gone + hits * tl;
    char* out = (char*)malloc(new_len + 1);
    if (out == NULL) {
        free(s);
        return NULL;
    }
    size_t o = 0;
    for (size_t i = 0; i < *len;) {
        const size_t m = temp_match(s, *len, i, dir, dl, sep);
        if (m != 0) {
            memcpy(out + o, to, tl);
            o += tl;
            i += m;
        } else {
            out[o++] = s[i++];
        }
    }
    out[o] = '\0';
    free(s);
    *len = new_len;
    return out;
}

/* an environment variable as UTF-8 into buf (empty when unset) */
static void env_utf8(const char* name, char* buf, size_t size) {
    buf[0] = '\0';
#ifdef _WIN32
    /* the wide call: the ANSI getenv would turn letters outside the code page into '?', and then nothing matches */
    wchar_t wname[64], wval[1024];
    if (MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, 64) <= 0) return;
    const DWORD n = GetEnvironmentVariableW(wname, wval, 1024);
    if (n == 0 || n >= 1024) return;
    if (WideCharToMultiByte(CP_UTF8, 0, wval, -1, buf, (int)size, NULL, NULL) <= 0) buf[0] = '\0';
#else
    const char* v = getenv(name);
    if (v != NULL && strlen(v) < size) strcpy(buf, v);
#endif
}

char* pc_scrub(const char* text, size_t len, size_t* out_len) {
    char* s = (char*)malloc(len + 1);
    if (s == NULL) return NULL;
    memcpy(s, text, len);
    s[len] = '\0';
    char home[1100], user[300], temp[1100];
#ifdef _WIN32
    env_utf8("USERPROFILE", home, sizeof(home));
    env_utf8("USERNAME", user, sizeof(user));
    env_utf8("TEMP", temp, sizeof(temp));
#else
    env_utf8("HOME", home, sizeof(home));
    env_utf8("USER", user, sizeof(user));
    env_utf8("TMPDIR", temp, sizeof(temp)); /* Linux / macOS: TMPDIR, else /tmp (likely the usual places) */
    if (temp[0] == '\0') strcpy(temp, "/tmp");
#endif
    /* the temp folder first: on Windows it sits inside the home folder, which the next step would rename */
    size_t tml = strlen(temp);
    while (tml > 0 && (temp[tml - 1] == '\\' || temp[tml - 1] == '/')) temp[--tml] = '\0';
    if (tml > 0) {
        char fwd[1100], back[1100];
        for (size_t i = 0; i <= tml; ++i) {
            fwd[i] = temp[i] == '\\' ? '/' : temp[i];
            back[i] = temp[i] == '/' ? '\\' : temp[i];
        }
        s = replace_temp(s, &len, back, '\\');
        s = replace_temp(s, &len, fwd, '/');
    }
    size_t hl = strlen(home);
    while (hl > 0 && (home[hl - 1] == '\\' || home[hl - 1] == '/')) home[--hl] = '\0';
    if (hl > 0) {
        char fwd[1100], back[1100];
        for (size_t i = 0; i <= hl; ++i) {
            fwd[i] = home[i] == '\\' ? '/' : home[i];
            back[i] = home[i] == '/' ? '\\' : home[i];
        }
        s = replace_all(s, &len, back, "<user>");
        s = replace_all(s, &len, fwd, "<user>");
    }
    if (user[0] != '\0') s = replace_all(s, &len, user, "<user>");
    if (s != NULL && out_len != NULL) *out_len = len;
    return s;
}

void pc_scrub_free(char* s) { free(s); }
