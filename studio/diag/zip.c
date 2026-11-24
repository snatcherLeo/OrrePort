/* zip.c: stored (uncompressed) zip files, written whole (zip.h says why). The format: for each file a local header and
 * its bytes, then a central directory listing them, then the end record (PKWARE's APPNOTE, the parts every unzip
 * reads: version 2.0, method 0 "stored", CRC-32, sizes, the name flagged as UTF-8 with bit 11). Little-endian fields
 * written byte by byte, so the host's byte order doesn't matter. Plain C; every platform. */
#include "zip.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

static uint32_t crc32_of(const void* data, size_t n) {
    static uint32_t table[256];
    static int ready = 0;
    if (!ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        ready = 1;
    }
    const uint8_t* p = (const uint8_t*)data;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static void put16(FILE* f, uint32_t v) {
    fputc((int)(v & 0xFF), f);
    fputc((int)((v >> 8) & 0xFF), f);
}
static void put32(FILE* f, uint32_t v) {
    put16(f, v & 0xFFFF);
    put16(f, v >> 16);
}

#ifdef _WIN32
static wchar_t* wide(const char* s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t* w = (wchar_t*)malloc(sizeof(wchar_t) * (size_t)(n > 0 ? n : 1));
    if (w) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
#endif

static FILE* open_utf8(const char* path) {
#ifdef _WIN32
    wchar_t* w = wide(path);
    FILE* f = w ? _wfopen(w, L"wb") : NULL;
    free(w);
    return f;
#else
    return fopen(path, "wb");
#endif
}

static int replace_file(const char* from, const char* to) {
#ifdef _WIN32
    wchar_t *a = wide(from), *b = wide(to);
    const int ok = a && b && MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING);
    free(a);
    free(b);
    return ok;
#else
    return rename(from, to) == 0;
#endif
}

static void remove_utf8(const char* path) {
#ifdef _WIN32
    wchar_t* w = wide(path);
    if (w) DeleteFileW(w);
    free(w);
#else
    remove(path);
#endif
}

int diag_zip_write(const char* path, const DiagZipFile* files, int count) {
    if (!path || count < 0 || (count > 0 && !files)) return 0;
    const size_t plen = strlen(path);
    char* part = (char*)malloc(plen + 6);
    uint32_t* offsets = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)(count > 0 ? count : 1));
    uint32_t* crcs = (uint32_t*)malloc(sizeof(uint32_t) * (size_t)(count > 0 ? count : 1));
    if (!part || !offsets || !crcs) { free(part); free(offsets); free(crcs); return 0; }
    memcpy(part, path, plen);
    memcpy(part + plen, ".part", 6);
    FILE* f = open_utf8(part);
    if (!f) { free(part); free(offsets); free(crcs); return 0; }
    /* the files' date and time, in DOS form (local time, two-second steps) */
    const time_t now = time(NULL);
    const struct tm* t = localtime(&now);
    const uint32_t dos_time = t ? (uint32_t)((t->tm_hour << 11) | (t->tm_min << 5) | (t->tm_sec / 2)) : 0;
    const uint32_t dos_date = t ? (uint32_t)(((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday) : 0;
    uint32_t at = 0;
    for (int i = 0; i < count; ++i) {
        const uint32_t nlen = (uint32_t)strlen(files[i].name), size = (uint32_t)files[i].size;
        crcs[i] = crc32_of(files[i].data, files[i].size);
        offsets[i] = at;
        put32(f, 0x04034B50u);  /* local file header */
        put16(f, 20);           /* version needed: 2.0 */
        put16(f, 1u << 11);     /* the name is UTF-8 */
        put16(f, 0);            /* stored */
        put16(f, dos_time);
        put16(f, dos_date);
        put32(f, crcs[i]);
        put32(f, size);
        put32(f, size);
        put16(f, nlen);
        put16(f, 0);
        fwrite(files[i].name, 1, nlen, f);
        if (size) fwrite(files[i].data, 1, size, f);
        at += 30 + nlen + size;
    }
    const uint32_t dir_at = at;
    for (int i = 0; i < count; ++i) {
        const uint32_t nlen = (uint32_t)strlen(files[i].name), size = (uint32_t)files[i].size;
        put32(f, 0x02014B50u);  /* central directory entry */
        put16(f, 20);           /* made by: 2.0 */
        put16(f, 20);
        put16(f, 1u << 11);
        put16(f, 0);
        put16(f, dos_time);
        put16(f, dos_date);
        put32(f, crcs[i]);
        put32(f, size);
        put32(f, size);
        put16(f, nlen);
        put16(f, 0);  /* extra */
        put16(f, 0);  /* comment */
        put16(f, 0);  /* disk */
        put16(f, 0);  /* internal attributes */
        put32(f, 0);  /* external attributes */
        put32(f, offsets[i]);
        fwrite(files[i].name, 1, nlen, f);
        at += 46 + nlen;
    }
    put32(f, 0x06054B50u);  /* end of central directory */
    put16(f, 0);
    put16(f, 0);
    put16(f, (uint32_t)count);
    put16(f, (uint32_t)count);
    put32(f, at - dir_at);
    put32(f, dir_at);
    put16(f, 0);
    const int ok = ferror(f) == 0;
    fclose(f);
    int done = ok && replace_file(part, path);
    if (!done) remove_utf8(part);
    free(part);
    free(offsets);
    free(crcs);
    return done;
}
