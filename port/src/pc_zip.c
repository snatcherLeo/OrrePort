/*
 * pc_zip.c: zip files with their contents stored as they are (2026-11-15).
 *
 * Why: the one-file .orresave (2.4 part 5; snatcherLeo approved the project's recommendation, "a zip, readable and documented, the same
 * as .orremod") bundles a save, its long names and its settings into one file. A zip is the format every system and
 * tool can open: rename it to .zip and Windows shows what's inside. OrrePort's files are small and the save inside is
 * encrypted (it wouldn't shrink), so entries are stored, not compressed: no compression library is needed, and the
 * reader only has to find stored entries. The report's zip writer (pc_report.cpp) works the same way.
 *
 * The format (the standard ZIP "local file header / central directory / end of central directory" layout, PKWARE's
 * APPNOTE): each file as a 30-byte local header, its name and its bytes; then a 46-byte central record per file; then
 * the 22-byte end record pointing at them. Names are UTF-8 (flag bit 11). CRC-32 as zip defines it.
 *
 * Platforms: plain C with no SDL (*changed 2026-11-22*: it used SDL's file calls, which has no SDL,
 * compiles this file too, for its Report a problem zip, so the two programs share one writer instead of copying it,
 * 4.3 part 3). Paths are UTF-8 everywhere. Windows: the C runtime's fopen reads a char path in the ANSI code page, so a
 * user folder with letters outside it (\Users\Jörg\ on some systems, any Japanese name) would fail; pc_fopen_utf8
 * converts to UTF-16 and uses _wfopen, and the finished file replaces the old one with MoveFileExW (verified on
 * Windows). Linux and macOS: fopen and rename take UTF-8 paths as they are, and rename replaces atomically (likely:
 * not built there yet). Byte order is written explicitly, so the same everywhere.
 * XD: the same (the format is game-independent).
 */
#include "pc_zip.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN /* Studio's build defines it on the command line */
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* UTF-8 -> a malloc'd UTF-16 string (Windows only: its wide file calls are the ones that take any name) */
static wchar_t* widen(const char* s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t* w = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)n);
    if (w != NULL && MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        w = NULL;
    }
    return w;
}
#endif

FILE* pc_fopen_utf8(const char* path, const char* mode) {
#ifdef _WIN32
    /* _wfopen shares the file for reading and writing like fopen (_SH_DENYNO), so a log still being written can be
     * read (pc_report.cpp reads this run's own log) */
    wchar_t* wp = widen(path);
    wchar_t* wm = widen(mode);
    FILE* f = wp != NULL && wm != NULL ? _wfopen(wp, wm) : NULL;
    free(wp);
    free(wm);
    return f;
#else
    return fopen(path, mode);
#endif
}

/* pc_zip.h says why. Windows: SetFileTime on a handle opened for writing attributes only (verified on Windows); Linux
 * and macOS: utime with NULL, "now" (likely; not built there yet). */
#ifndef _WIN32
#include <utime.h>
#endif
int pc_file_touch_utf8(const char* path) {
#ifdef _WIN32
    wchar_t* wp = widen(path);
    HANDLE h = wp != NULL ? CreateFileW(wp, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)
                          : INVALID_HANDLE_VALUE;
    FILETIME now;
    int ok = 0;
    free(wp);
    if (h == INVALID_HANDLE_VALUE) return 0;
    GetSystemTimeAsFileTime(&now);
    ok = SetFileTime(h, NULL, NULL, &now) != 0;
    CloseHandle(h);
    return ok;
#else
    return utime(path, NULL) == 0;
#endif
}

/* replaces `to` with `from` (the finished .new file); 1 on success */
static int replace_file(const char* from, const char* to) {
#ifdef _WIN32
    wchar_t* wf = widen(from);
    wchar_t* wt = widen(to);
    const int ok = wf != NULL && wt != NULL && MoveFileExW(wf, wt, MOVEFILE_REPLACE_EXISTING) != 0;
    free(wf);
    free(wt);
    return ok;
#else
    return rename(from, to) == 0;
#endif
}

static void remove_file(const char* path) {
#ifdef _WIN32
    wchar_t* wp = widen(path);
    if (wp != NULL) _wremove(wp);
    free(wp);
#else
    remove(path);
#endif
}

static uint32_t crc32_of(const uint8_t* p, size_t n) {
    static uint32_t table[256];
    static int made = 0;
    if (!made) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        made = 1;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t* p, uint32_t v) { put16(p, v & 0xFFFF); put16(p + 2, v >> 16); }
static uint32_t get16(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t get32(const uint8_t* p) { return get16(p) | get16(p + 2) << 16; }

int pc_zip_write(const char* path, const PcZipFile* files, int count) {
    size_t total = 22;
    for (int i = 0; i < count; ++i) total += 30 + 46 + 2 * strlen(files[i].name) + files[i].size;
    uint8_t* out = (uint8_t*)malloc(total);
    uint32_t* offsets = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)(count > 0 ? count : 1));
    uint32_t* crcs = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)(count > 0 ? count : 1));
    if (out == NULL || offsets == NULL || crcs == NULL) {
        free(out);
        free(offsets);
        free(crcs);
        return 0;
    }
    const time_t now = time(NULL);
    const struct tm* t = localtime(&now);
    const uint32_t dos_time = (uint32_t)((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2));
    const uint32_t dos_date = (uint32_t)(((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday);
    size_t at = 0;
    for (int i = 0; i < count; ++i) { /* each file: its local header, name and bytes */
        const size_t nl = strlen(files[i].name);
        offsets[i] = (uint32_t)at;
        crcs[i] = crc32_of((const uint8_t*)files[i].data, files[i].size);
        uint8_t* h = out + at;
        put32(h, 0x04034b50);
        put16(h + 4, 20);
        put16(h + 6, 0x0800); /* names in UTF-8 */
        put16(h + 8, 0);      /* stored */
        put16(h + 10, dos_time);
        put16(h + 12, dos_date);
        put32(h + 14, crcs[i]);
        put32(h + 18, (uint32_t)files[i].size);
        put32(h + 22, (uint32_t)files[i].size);
        put16(h + 26, (uint32_t)nl);
        put16(h + 28, 0);
        memcpy(h + 30, files[i].name, nl);
        memcpy(h + 30 + nl, files[i].data, files[i].size);
        at += 30 + nl + files[i].size;
    }
    const size_t cd = at;
    for (int i = 0; i < count; ++i) { /* the central directory */
        const size_t nl = strlen(files[i].name);
        uint8_t* c = out + at;
        memset(c, 0, 46);
        put32(c, 0x02014b50);
        put16(c + 4, 20);
        put16(c + 6, 20);
        put16(c + 8, 0x0800);
        put16(c + 12, dos_time);
        put16(c + 14, dos_date);
        put32(c + 16, crcs[i]);
        put32(c + 20, (uint32_t)files[i].size);
        put32(c + 24, (uint32_t)files[i].size);
        put16(c + 28, (uint32_t)nl);
        put32(c + 42, offsets[i]);
        memcpy(c + 46, files[i].name, nl);
        at += 46 + nl;
    }
    uint8_t* e = out + at; /* the end record */
    memset(e, 0, 22);
    put32(e, 0x06054b50);
    put16(e + 8, (uint32_t)count);
    put16(e + 10, (uint32_t)count);
    put32(e + 12, (uint32_t)(at - cd));
    put32(e + 16, (uint32_t)cd);
    at += 22;
    /* written whole as <path>.new, then put in place (the Save As window already asked before replacing a file) */
    char tmp[1700];
    int ok = snprintf(tmp, sizeof(tmp), "%s.new", path) < (int)sizeof(tmp);
    FILE* f = ok ? pc_fopen_utf8(tmp, "wb") : NULL;
    ok = f != NULL;
    if (ok) {
        ok = fwrite(out, 1, at, f) == at;
        ok = fclose(f) == 0 && ok;
        ok = ok && replace_file(tmp, path);
        if (!ok) remove_file(tmp); /* a half-written .new isn't left behind */
    }
    free(out);
    free(offsets);
    free(crcs);
    return ok;
}

int pc_zip_is_zip(const void* data, size_t size) {
    const uint8_t* p = (const uint8_t*)data;
    return size >= 4 && p[0] == 'P' && p[1] == 'K' && p[2] == 3 && p[3] == 4;
}

const void* pc_zip_find(const void* zip, size_t zip_size, const char* name, size_t* size_out) {
    const uint8_t* z = (const uint8_t*)zip;
    if (size_out != NULL) *size_out = 0;
    if (z == NULL || zip_size < 22) return NULL;
    size_t e = zip_size - 22; /* the end record, searched back past a possible comment */
    for (;; --e) {
        if (get32(z + e) == 0x06054b50) break;
        if (e == 0 || zip_size - e > 22 + 0xFFFF) return NULL;
    }
    const uint32_t count = get16(z + e + 10), cd = get32(z + e + 16);
    const size_t want = strlen(name);
    size_t at = cd;
    for (uint32_t i = 0; i < count; ++i) {
        if (at + 46 > zip_size || get32(z + at) != 0x02014b50) return NULL;
        const uint32_t method = get16(z + at + 10), size = get32(z + at + 20), stored = get32(z + at + 24);
        const uint32_t nl = get16(z + at + 28), xl = get16(z + at + 30), cl = get16(z + at + 32), lo = get32(z + at + 42);
        if (at + 46 + nl > zip_size) return NULL;
        if (nl == want && memcmp(z + at + 46, name, nl) == 0) {
            if (method != 0 || size != stored || (size_t)lo + 30 > zip_size) return NULL;
            const size_t data = (size_t)lo + 30 + get16(z + lo + 26) + get16(z + lo + 28);
            if (data + stored > zip_size) return NULL;
            if (size_out != NULL) *size_out = stored;
            return z + data;
        }
        at += 46 + nl + xl + cl;
    }
    return NULL;
}
