/*
 * pc_update.c: the updater both programs share (pc_update.h says what and why; docs/port/updater-plan.md the design).
 *
 * The release (built by tools/local/port/make_release.py, signed by orre_sign): two small files next to the zip on the
 * public repo's GitHub Releases, read through the fixed ".../releases/latest/download/<name>" address:
 *   update.txt  "key = value" lines, like the language files (no JSON parser needed in either program):
 *               version, file (the zip's name), size, sha512, and notes.<lang> (what's new; "\n" for a line break)
 *   update.sig  the Ed25519 signature of update.txt's exact bytes, 128 hex digits
 * The zip holds app\ only; it is downloaded as <file>.part (resumable), checked against size and sha512, unpacked
 * with zlib into data\system\update\<version>\app\, and marked ready. The launcher swaps it in (orre_launcher.c).
 *
 * The shared state, so what one program finds or downloads the other shows: data\system\update\state.ini (state,
 * latest, size, done, error, last_check), update.txt (the verified notes), lock (held open while downloading, deleted
 * by Windows when the holder closes or crashes: FILE_FLAG_DELETE_ON_CLOSE).
 *
 * Test switches (never settings; 2026-11-23, for the dummy-update test): ORRE_UPDATE_URL=<folder or https base>
 * replaces the release address (a local folder works without a network); ORRE_UPDATE_TEST_PUBKEY=<64 hex> replaces
 * the built-in public key, so a test key can sign a test release. Neither is set for players.
 *
 * Platforms: Windows (WinHTTP, Win32 threads and files; verified by the dummy test). Linux / macOS: not written yet:
 * libcurl or the system's HTTP, pthreads, rename(2) (likely); pc_update_init returns 0 there, so the programs just
 * show no update. Monocypher and zlib are the same everywhere.
 */
#include "pc_update.h"
#include "pc_zip.h" /* pc_fopen_utf8 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#include <tlhelp32.h>

#include <zlib.h>

#include "monocypher.h"
#include "monocypher-ed25519.h"

/* The release address and the public key. The key is snatcherLeo's (made 2026-11-23 with orre_sign keygen on his PC; the secret
 * half never leaves his key file and its two backups): only a release signed with it is accepted. The address is a
 * placeholder until the public repo exists (Phase 5). */
#define PC_UPDATE_URL_BASE "https://github.com/snatcherLeo/OrrePort/releases/latest/download/"
static const char* const kPublicKeyHex = "6531dbdc2591699ae7664387ae9356c8e81401b0b6eb81fa7b94e1e934a7a231";

static int s_ready;                 /* pc_update_init succeeded */
static wchar_t s_dir[1024];         /* data\system\update\ */
static wchar_t s_app[1024];         /* app\ (for "the other program is running") */
static char s_version[32], s_lang[16];
static volatile LONG s_job;         /* 1 check, 2 download (a bit set each) */
static volatile LONG s_auto, s_playing, s_quit, s_allowed, s_force;
static HANDLE s_thread, s_wake;
static FILETIME s_seen;             /* state.ini's last change seen by poll */
static PcUpdateStatus s_last;

/* ---- small helpers ---- */
static void path_in(wchar_t* out, size_t n, const wchar_t* name) { _snwprintf(out, n, L"%ls%ls", s_dir, name); out[n - 1] = 0; }
static int widen(const char* s, wchar_t* out, int n) { return MultiByteToWideChar(CP_UTF8, 0, s, -1, out, n) > 0; }

static char* read_all_w(const wchar_t* path, size_t* len) {
    FILE* f = _wfopen(path, L"rb");
    char* buf = NULL;
    long n;
    if (f == NULL) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n >= 0 && n < (16 << 20) && (buf = (char*)malloc((size_t)n + 1)) != NULL) {
        if (fread(buf, 1, (size_t)n, f) != (size_t)n) { free(buf); buf = NULL; }
        else { buf[n] = 0; if (len) *len = (size_t)n; }
    }
    fclose(f);
    return buf;
}

static int write_all_w(const wchar_t* path, const void* data, size_t len) {
    wchar_t tmp[1100];
    FILE* f;
    int ok;
    _snwprintf(tmp, 1100, L"%ls.new", path);
    tmp[1099] = 0;
    f = _wfopen(tmp, L"wb");
    if (f == NULL) return 0;
    ok = fwrite(data, 1, len, f) == len;
    ok = fclose(f) == 0 && ok;
    return ok && MoveFileExW(tmp, path, MOVEFILE_REPLACE_EXISTING);
}

/* a "key = value" line's value from text (first match), into out; 1 when found */
static int ini_get(const char* text, const char* key, char* out, size_t n) {
    const size_t kl = strlen(key);
    const char* p = text;
    while (p != NULL && *p) {
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, key, kl) == 0) {
            const char* q = p + kl;
            while (*q == ' ' || *q == '\t') q++;
            if (*q == '=') {
                size_t i = 0;
                q++;
                while (*q == ' ' || *q == '\t') q++;
                while (*q && *q != '\r' && *q != '\n' && i + 1 < n) out[i++] = *q++;
                while (i > 0 && (out[i - 1] == ' ' || out[i - 1] == '\t')) i--;
                out[i] = 0;
                return 1;
            }
        }
        p = strchr(p, '\n');
        if (p) p++;
    }
    if (n) out[0] = 0;
    return 0;
}

/* "0.1.10" > "0.1.9": numbers compared part by part; 1 when a is newer than b */
static int newer(const char* a, const char* b) {
    while (*a || *b) {
        const long x = strtol(a, (char**)&a, 10), y = strtol(b, (char**)&b, 10);
        if (x != y) return x > y;
        if (*a == '.') a++;
        if (*b == '.') b++;
        if (!*a && !*b) break;
    }
    return 0;
}

static int hex_bytes(const char* hex, uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1) return 0;
        out[i] = (uint8_t)v;
    }
    return 1;
}

/* ---- the shared state file ---- */
static void state_write(PcUpdateState st, const char* latest, unsigned long long size, unsigned long long done,
                        const char* error) {
    wchar_t p[1100];
    char text[512];
    char prev_check[32] = "";
    char* old;
    path_in(p, 1100, L"state.ini");
    old = read_all_w(p, NULL);
    if (old) { ini_get(old, "last_check", prev_check, sizeof prev_check); free(old); }
    snprintf(text, sizeof text, "state = %d\nlatest = %s\nsize = %llu\ndone = %llu\nerror = %s\nlast_check = %s\n",
             (int)st, latest ? latest : "", size, done, error ? error : "", prev_check);
    write_all_w(p, text, strlen(text));
}

static void stamp_check_time(void) {
    wchar_t p[1100];
    char* old;
    char text[512], line[64];
    path_in(p, 1100, L"state.ini");
    old = read_all_w(p, NULL);
    {
        /* wall-clock seconds, so the other program (a different tick base) can compare */
        FILETIME ft;
        ULARGE_INTEGER u;
        GetSystemTimeAsFileTime(&ft);
        u.LowPart = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        snprintf(line, sizeof line, "last_check = %llu\n", u.QuadPart / 10000000ULL);
    }
    text[0] = 0;
    if (old) {
        /* every line but the old last_check */
        const char* s = old;
        size_t used = 0;
        while (*s) {
            const char* e = strchr(s, '\n');
            const size_t len = e ? (size_t)(e - s + 1) : strlen(s);
            if (strncmp(s, "last_check", 10) != 0 && used + len < sizeof text) { memcpy(text + used, s, len); used += len; }
            s += len;
        }
        text[used] = 0;
        free(old);
    }
    strncat(text, line, sizeof text - strlen(text) - 1);
    write_all_w(p, text, strlen(text));
}

/* ---- fetching: a local folder (tests) or https through WinHTTP ---- */
typedef int (*sink_fn)(void* user, const void* data, size_t n); /* 0 stops */

static int fetch(const char* name, unsigned long long from, sink_fn sink, void* user, unsigned long long* total) {
    const char* base = getenv("ORRE_UPDATE_URL");
    char url[1200];
    wchar_t wurl[1200];
    {
        /* a test folder given without its trailing separator still works (Studio's finding) */
        const char* b = base != NULL && *base ? base : PC_UPDATE_URL_BASE;
        const size_t bl = strlen(b);
        snprintf(url, sizeof url, "%s%s%s", b, bl > 0 && b[bl - 1] != '/' && b[bl - 1] != '\\' ? "/" : "", name);
    }
    if (total) *total = 0;
    if (strncmp(url, "https://", 8) != 0) { /* a local folder (test switch) */
        FILE* f = pc_fopen_utf8(url, "rb");
        char buf[1 << 16];
        size_t n;
        int ok = 1;
        if (f == NULL) return 2; /* not there: nothing published (as a 404 below) */
        _fseeki64(f, 0, SEEK_END);
        if (total) *total = (unsigned long long)_ftelli64(f);
        _fseeki64(f, (long long)from, SEEK_SET);
        while (ok && (n = fread(buf, 1, sizeof buf, f)) > 0) ok = sink(user, buf, n);
        fclose(f);
        return ok;
    }
    if (!widen(url, wurl, 1200)) return 0;
    {
        URL_COMPONENTSW uc;
        wchar_t host[256], path[1024];
        HINTERNET ses = NULL, con = NULL, req = NULL;
        int ok = 0;
        memset(&uc, 0, sizeof uc);
        uc.dwStructSize = sizeof uc;
        uc.lpszHostName = host;
        uc.dwHostNameLength = 256;
        uc.lpszUrlPath = path;
        uc.dwUrlPathLength = 1024;
        if (!WinHttpCrackUrl(wurl, 0, 0, &uc)) return 0;
        /* "OrrePort-updater" and nothing about the player (snatcherLeo: "one small request, nothing about the player sent") */
        ses = WinHttpOpen(L"OrrePort-updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (ses) con = WinHttpConnect(ses, host, uc.nPort, 0);
        if (con) req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                          WINHTTP_FLAG_SECURE);
        if (req) {
            wchar_t range[64];
            DWORD status = 0, sl = sizeof status;
            _snwprintf(range, 64, L"Range: bytes=%llu-", from);
            ok = WinHttpSendRequest(req, from > 0 ? range : WINHTTP_NO_ADDITIONAL_HEADERS, from > 0 ? (DWORD)-1L : 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                 WinHttpReceiveResponse(req, NULL) &&
                 WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &sl, NULL) &&
                 (status == 200 || (status == 206 && from > 0));
            if (!ok && status == 404) {
                /* the server answered "not there": no release has been published at this address (before the public
                 * repo exists, or before its first release). Not a failure the player should see: 2 = nothing newer */
                WinHttpCloseHandle(req);
                if (con) WinHttpCloseHandle(con);
                if (ses) WinHttpCloseHandle(ses);
                return 2;
            }
            if (ok && status == 200 && from > 0) ok = 0; /* the server ignored the range: start over (caller) */
            if (ok && total) {
                wchar_t len[32];
                DWORD ll = sizeof len;
                if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, NULL, len, &ll, NULL))
                    *total = from + _wcstoui64(len, NULL, 10);
            }
            while (ok) {
                char buf[1 << 16];
                DWORD got = 0;
                if (!WinHttpReadData(req, buf, sizeof buf, &got)) { ok = 0; break; }
                if (got == 0) break;
                if (!sink(user, buf, got)) { ok = 0; break; }
            }
        }
        if (req) WinHttpCloseHandle(req);
        if (con) WinHttpCloseHandle(con);
        if (ses) WinHttpCloseHandle(ses);
        return ok;
    }
}

typedef struct { char* data; size_t len, cap; } Mem;
static int to_mem(void* user, const void* d, size_t n) {
    Mem* m = (Mem*)user;
    if (m->len + n + 1 > m->cap) {
        size_t cap = (m->cap ? m->cap * 2 : 4096);
        char* nd;
        while (cap < m->len + n + 1) cap *= 2;
        if (cap > (4u << 20) || (nd = (char*)realloc(m->data, cap)) == NULL) return 0; /* the small files only */
        m->data = nd;
        m->cap = cap;
    }
    memcpy(m->data + m->len, d, n);
    m->len += n;
    m->data[m->len] = 0;
    return 1;
}

/* update.txt + update.sig fetched and the signature checked; text returned (malloc), NULL with *why set */
static char* fetch_verified_manifest(const char** why) {
    Mem txt = {0}, sig = {0};
    uint8_t key[32], s[64];
    const char* keyhex = getenv("ORRE_UPDATE_TEST_PUBKEY");
    *why = "update.failed.network";
    {
        const int got = fetch("update.txt", 0, to_mem, &txt, NULL);
        if (got == 2) { /* nothing published: up to date, not an error (fetch says why) */
            *why = "none";
            free(txt.data);
            return NULL;
        }
        if (got != 1) {
            free(txt.data);
            return NULL;
        }
    }
    if (fetch("update.sig", 0, to_mem, &sig, NULL) != 1 || txt.len == 0 || sig.len < 128) {
        free(txt.data);
        free(sig.data);
        return NULL;
    }
    *why = "update.failed.signature";
    if (!hex_bytes(keyhex != NULL && strlen(keyhex) == 64 ? keyhex : kPublicKeyHex, key, 32) || !hex_bytes(sig.data, s, 64) ||
        crypto_ed25519_check(s, key, (const uint8_t*)txt.data, txt.len) != 0) {
        free(txt.data);
        free(sig.data);
        return NULL;
    }
    free(sig.data);
    *why = NULL;
    return txt.data;
}

/* ---- the zip: deflate or stored entries, unpacked under a folder (the release's app\) ---- */
static unsigned g16(const uint8_t* p) { return p[0] | p[1] << 8; }
static unsigned g32(const uint8_t* p) { return g16(p) | (unsigned)g16(p + 2) << 16; }

static int unzip_to(const wchar_t* zip_path, const wchar_t* dest) {
    size_t len = 0;
    uint8_t* z = NULL;
    FILE* f = _wfopen(zip_path, L"rb");
    long long n;
    size_t e;
    int ok = 1;
    if (f == NULL) return 0;
    _fseeki64(f, 0, SEEK_END);
    n = _ftelli64(f);
    _fseeki64(f, 0, SEEK_SET);
    if (n < 22 || (z = (uint8_t*)malloc((size_t)n)) == NULL || fread(z, 1, (size_t)n, f) != (size_t)n) ok = 0;
    fclose(f);
    if (!ok) { free(z); return 0; }
    len = (size_t)n;
    for (e = len - 22;; e--) { /* the end record */
        if (g32(z + e) == 0x06054b50) break;
        if (e == 0 || len - e > 22 + 0xFFFF) { free(z); return 0; }
    }
    {
        const unsigned count = g16(z + e + 10);
        size_t at = g32(z + e + 16);
        for (unsigned i = 0; ok && i < count; i++) {
            const unsigned method = g16(z + at + 10), nl = g16(z + at + 28), xl = g16(z + at + 30), cl = g16(z + at + 32);
            const size_t csize = g32(z + at + 20), usize = g32(z + at + 24), lo = g32(z + at + 42);
            char name[600];
            wchar_t wname[600], out[1600];
            size_t data;
            if (g32(z + at) != 0x02014b50 || nl >= sizeof name || lo + 30 > len) { ok = 0; break; }
            memcpy(name, z + at + 46, nl);
            name[nl] = 0;
            at += 46 + nl + xl + cl;
            if (strstr(name, "..") != NULL || name[0] == '/' || name[0] == '\\') { ok = 0; break; } /* stays inside dest */
            for (char* c = name; *c; c++) if (*c == '/') *c = '\\';
            if (!widen(name, wname, 600)) { ok = 0; break; }
            _snwprintf(out, 1600, L"%ls%ls", dest, wname);
            out[1599] = 0;
            /* the folders on the way */
            for (wchar_t* c = out + wcslen(dest); *c; c++)
                if (*c == L'\\') { *c = 0; CreateDirectoryW(out, NULL); *c = L'\\'; }
            if (nl > 0 && name[nl - 1] == '\\') continue; /* a folder entry */
            data = lo + 30 + g16(z + lo + 26) + g16(z + lo + 28);
            if (data + csize > len) { ok = 0; break; }
            if (method == 0) {
                ok = write_all_w(out, z + data, csize);
            } else if (method == 8) {
                uint8_t* buf = (uint8_t*)malloc(usize ? usize : 1);
                z_stream s;
                memset(&s, 0, sizeof s);
                ok = buf != NULL && inflateInit2(&s, -MAX_WBITS) == Z_OK;
                if (ok) {
                    s.next_in = z + data;
                    s.avail_in = (uInt)csize;
                    s.next_out = buf;
                    s.avail_out = (uInt)usize;
                    ok = inflate(&s, Z_FINISH) == Z_STREAM_END && s.total_out == usize;
                    inflateEnd(&s);
                }
                if (ok) ok = write_all_w(out, buf, usize);
                free(buf);
            } else {
                ok = 0;
            }
        }
    }
    free(z);
    return ok;
}

/* ---- the download ---- */
typedef struct { FILE* f; crypto_sha512_ctx sha; unsigned long long done, size; ULONGLONG last_note; const char* latest; } Dl;
static int to_file(void* user, const void* d, size_t n) {
    Dl* dl = (Dl*)user;
    if (s_quit) return 0;
    if (fwrite(d, 1, n, dl->f) != n) return 0;
    crypto_sha512_update(&dl->sha, (const uint8_t*)d, n);
    dl->done += n;
    if (GetTickCount64() - dl->last_note > 500) { /* the shared progress, twice a second */
        dl->last_note = GetTickCount64();
        state_write(PC_UPDATE_DOWNLOADING, dl->latest, dl->size, dl->done, NULL);
    }
    if (s_playing) Sleep(20); /* slower while a game is played: it never stutters (about 3 MB/s at most) */
    return 1;
}

static HANDLE take_lock(void) {
    wchar_t p[1100];
    path_in(p, 1100, L"lock");
    return CreateFileW(p, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);
}

static void do_download(void) {
    const char* why = NULL;
    char* manifest = NULL;
    char latest[32], file[256], sha_hex[160], size_s[32];
    wchar_t part[1100], verdir[1100], appdir[1100], wfile[256];
    HANDLE lock = take_lock();
    Dl dl;
    uint8_t want[64], got[64];
    if (lock == INVALID_HANDLE_VALUE) return; /* the other program is downloading: poll follows its state */
    manifest = fetch_verified_manifest(&why);
    if (manifest == NULL) {
        state_write(PC_UPDATE_FAILED, "", 0, 0, why != NULL && strcmp(why, "none") == 0 ? "update.failed.network" : why);
        CloseHandle(lock);
        return;
    }
    ini_get(manifest, "version", latest, sizeof latest);
    ini_get(manifest, "file", file, sizeof file);
    ini_get(manifest, "sha512", sha_hex, sizeof sha_hex);
    ini_get(manifest, "size", size_s, sizeof size_s);
    if (!newer(latest, s_version) || strlen(sha_hex) != 128 || strchr(file, '/') || strchr(file, '\\') || !widen(file, wfile, 256)) {
        state_write(PC_UPDATE_FAILED, latest, 0, 0, "update.failed.signature");
        free(manifest);
        CloseHandle(lock);
        return;
    }
    {
        wchar_t p[1100];
        path_in(p, 1100, L"update.txt");
        write_all_w(p, manifest, strlen(manifest)); /* the notes both programs show */
    }
    _snwprintf(part, 1100, L"%ls%ls.part", s_dir, wfile);
    memset(&dl, 0, sizeof dl);
    dl.size = strtoull(size_s, NULL, 10);
    dl.latest = latest;
    crypto_sha512_init(&dl.sha);
    /* (a resumed download would need the hash of what's already there: it simply starts over; a cut download costs
     * one re-download, which is rare enough) */
    dl.f = _wfopen(part, L"wb");
    if (dl.f == NULL || fetch(file, 0, to_file, &dl, NULL) != 1) {
        if (dl.f) fclose(dl.f);
        state_write(s_quit ? PC_UPDATE_AVAILABLE : PC_UPDATE_FAILED, latest, dl.size, 0, s_quit ? NULL : "update.failed.network");
        free(manifest);
        CloseHandle(lock);
        return;
    }
    fclose(dl.f);
    crypto_sha512_final(&dl.sha, got);
    if (dl.done != dl.size || !hex_bytes(sha_hex, want, 64) || memcmp(want, got, 64) != 0) {
        DeleteFileW(part); /* our own download that doesn't match the signed list */
        state_write(PC_UPDATE_FAILED, latest, dl.size, 0, "update.failed.signature");
        free(manifest);
        CloseHandle(lock);
        return;
    }
    {
        wchar_t wlatest[32];
        widen(latest, wlatest, 32);
        _snwprintf(verdir, 1100, L"%ls%ls\\", s_dir, wlatest);
        CreateDirectoryW(verdir, NULL);
    }
    if (!unzip_to(part, verdir)) {
        state_write(PC_UPDATE_FAILED, latest, dl.size, 0, "update.failed.unpack");
    } else {
        wchar_t ready[1100];
        _snwprintf(appdir, 1100, L"%lsapp", verdir);
        path_in(ready, 1100, L"ready");
        write_all_w(ready, latest, strlen(latest)); /* the launcher's cue: swap in <version>\app */
        DeleteFileW(part);
        state_write(PC_UPDATE_READY, latest, dl.size, dl.size, NULL);
    }
    free(manifest);
    CloseHandle(lock);
}

/* Is <latest> really ready to swap in? The launcher's marker ("ready", naming the version) and the unpacked
 * <latest>\app\ both still there. *Fixed 2026-11-23 (Studio's finding in snatcherLeo's hand check):* state.ini kept "READY,
 * 0.1.1" after the launcher had swapped 0.1.1 in (removing the marker and the folder) and Go back had returned to
 * 0.1.0, so both programs showed "Update ready" with nothing to apply, and the check never ran again (it skips while
 * READY). Now READY is believed only while this holds; otherwise the poll shows AVAILABLE (that version is newer than
 * this one) or UP_TO_DATE, and the check runs as usual and rewrites state.ini. Windows (the file calls); Linux / macOS
 * would test the same two things (likely). */
static int ready_present(const char* latest) {
    wchar_t p[1100], wlatest[32], dir[1100];
    char* marker;
    int ok;
    DWORD attr;
    if (latest == NULL || !latest[0] || !widen(latest, wlatest, 32)) return 0;
    path_in(p, 1100, L"ready");
    marker = read_all_w(p, NULL);
    if (marker == NULL) return 0;
    for (size_t n = strlen(marker); n > 0 && (unsigned char)marker[n - 1] <= ' '; --n) marker[n - 1] = '\0';
    ok = strcmp(marker, latest) == 0;
    free(marker);
    _snwprintf(dir, 1100, L"%ls%ls\\app", s_dir, wlatest);
    dir[1099] = 0;
    attr = GetFileAttributesW(dir);
    return ok && attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static void do_check(void) {
    const char* why = NULL;
    char* manifest;
    char latest[32] = "", size_s[32] = "", now_state[16] = "";
    char* st;
    wchar_t p[1100];
    path_in(p, 1100, L"state.ini");
    st = read_all_w(p, NULL);
    if (st) {
        char last[32] = "", was_latest[32] = "";
        FILETIME ft;
        ULARGE_INTEGER u;
        ini_get(st, "last_check", last, sizeof last);
        ini_get(st, "state", now_state, sizeof now_state);
        ini_get(st, "latest", was_latest, sizeof was_latest);
        /* a READY with nothing left to swap in (ready_present says why) doesn't hold the check back */
        if (atoi(now_state) == PC_UPDATE_READY && !ready_present(was_latest)) {
            now_state[0] = '\0';
            last[0] = '\0'; /* and the three-hour skip doesn't either: the state needs rewriting now */
        }
        GetSystemTimeAsFileTime(&ft);
        u.LowPart = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        free(st);
        /* the other program checked in the last three hours (unless the player pressed "Check for updates":
         * pc_update_check(2)), or something is downloaded or downloading: no request */
        if ((!InterlockedExchange(&s_force, 0) && last[0] && u.QuadPart / 10000000ULL - strtoull(last, NULL, 10) < 3 * 3600) ||
            atoi(now_state) == PC_UPDATE_READY || atoi(now_state) == PC_UPDATE_DOWNLOADING)
            return;
    }
    state_write(PC_UPDATE_CHECKING, "", 0, 0, NULL);
    manifest = fetch_verified_manifest(&why);
    stamp_check_time();
    if (manifest == NULL) {
        if (why != NULL && strcmp(why, "none") == 0) state_write(PC_UPDATE_UP_TO_DATE, "", 0, 0, NULL); /* nothing published */
        else state_write(PC_UPDATE_FAILED, "", 0, 0, why);
        return;
    }
    ini_get(manifest, "version", latest, sizeof latest);
    ini_get(manifest, "size", size_s, sizeof size_s);
    if (newer(latest, s_version)) {
        path_in(p, 1100, L"update.txt");
        write_all_w(p, manifest, strlen(manifest));
        state_write(PC_UPDATE_AVAILABLE, latest, strtoull(size_s, NULL, 10), 0, NULL);
        if (s_auto) InterlockedOr(&s_job, 2); /* "Download updates automatically" */
    } else {
        state_write(PC_UPDATE_UP_TO_DATE, latest, 0, 0, NULL);
    }
    stamp_check_time();
    free(manifest);
}

static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    while (!s_quit) {
        LONG job;
        WaitForSingleObject(s_wake, 1000);
        job = InterlockedExchange(&s_job, 0);
        if (job & 1) {
            do_check();
            job |= InterlockedExchange(&s_job, 0) & 2; /* "Download updates automatically" asked for it */
        }
        if (job & 2) do_download();
    }
    return 0;
}

/* ---- the calls ---- */
int pc_update_init(const char* root, const char* version, const char* lang) {
    wchar_t wroot[900];
    if (s_ready || root == NULL || version == NULL || !widen(root, wroot, 900)) return s_ready;
    {
        const size_t n = wcslen(wroot);
        const wchar_t* sep = n > 0 && (wroot[n - 1] == L'\\' || wroot[n - 1] == L'/') ? L"" : L"\\";
        wchar_t sys[1000], data[1000];
        _snwprintf(data, 1000, L"%ls%lsdata", wroot, sep);
        _snwprintf(sys, 1000, L"%ls\\system", data);
        _snwprintf(s_dir, 1024, L"%ls\\update\\", sys);
        _snwprintf(s_app, 1024, L"%ls%lsapp\\", wroot, sep);
        /* each level, a fresh folder may have none of them yet (found by the test, 2026-11-23) */
        CreateDirectoryW(data, NULL);
        CreateDirectoryW(sys, NULL);
        CreateDirectoryW(s_dir, NULL);
        if (GetFileAttributesW(s_dir) == INVALID_FILE_ATTRIBUTES) return 0;
    }
    snprintf(s_version, sizeof s_version, "%s", version);
    snprintf(s_lang, sizeof s_lang, "%s", lang != NULL ? lang : "en");
    s_wake = CreateEventW(NULL, FALSE, FALSE, NULL);
    s_thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    s_ready = s_wake != NULL && s_thread != NULL;
    return s_ready;
}

void pc_update_set_auto_download(int on) { InterlockedExchange(&s_auto, on != 0); }

void pc_update_check(int allowed) {
    InterlockedExchange(&s_allowed, allowed != 0);
    if (!s_ready || !allowed) return;
    if (allowed == 2) InterlockedExchange(&s_force, 1); /* the player asked: check now, whatever the other program did */
    InterlockedOr(&s_job, 1);
    SetEvent(s_wake);
}

void pc_update_download(void) {
    if (!s_ready || !s_allowed) return;
    InterlockedOr(&s_job, 2);
    SetEvent(s_wake);
}

void pc_update_set_playing(int playing) { InterlockedExchange(&s_playing, playing != 0); }

/* another process whose program file is inside app\ (the other program; not this one) */
static int other_program_running(void) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe;
    int found = 0;
    const DWORD me = GetCurrentProcessId();
    if (snap == INVALID_HANDLE_VALUE) return 0;
    pe.dwSize = sizeof pe;
    for (BOOL more = Process32FirstW(snap, &pe); more && !found; more = Process32NextW(snap, &pe)) {
        HANDLE h;
        wchar_t img[1100];
        DWORD n = 1100;
        if (pe.th32ProcessID == me) continue;
        h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (h == NULL) continue;
        if (QueryFullProcessImageNameW(h, 0, img, &n) && _wcsnicmp(img, s_app, wcslen(s_app)) == 0) found = 1;
        CloseHandle(h);
    }
    CloseHandle(snap);
    return found;
}

int pc_update_poll(PcUpdateStatus* out) {
    wchar_t p[1100];
    WIN32_FILE_ATTRIBUTE_DATA fa;
    int changed = 0;
    if (out == NULL) return 0;
    if (!s_ready || !s_allowed) {
        memset(out, 0, sizeof *out);
        out->state = s_ready && !s_allowed ? PC_UPDATE_OFF : PC_UPDATE_IDLE;
        snprintf(out->current, sizeof out->current, "%s", s_version);
        return 0;
    }
    path_in(p, 1100, L"state.ini");
    if (GetFileAttributesExW(p, GetFileExInfoStandard, &fa) &&
        CompareFileTime(&fa.ftLastWriteTime, &s_seen) != 0) {
        char* st = read_all_w(p, NULL);
        s_seen = fa.ftLastWriteTime;
        if (st) {
            char v[64];
            memset(&s_last, 0, sizeof s_last);
            ini_get(st, "state", v, sizeof v);
            s_last.state = (PcUpdateState)atoi(v);
            ini_get(st, "latest", s_last.latest, sizeof s_last.latest);
            ini_get(st, "size", v, sizeof v);
            s_last.size = strtoull(v, NULL, 10);
            ini_get(st, "done", v, sizeof v);
            s_last.done = strtoull(v, NULL, 10);
            ini_get(st, "error", s_last.error_key, sizeof s_last.error_key);
            free(st);
            if (s_last.latest[0] && !newer(s_last.latest, s_version) && s_last.state != PC_UPDATE_FAILED)
                s_last.state = PC_UPDATE_UP_TO_DATE; /* this program is already that version (after the swap) */
            else if (s_last.state == PC_UPDATE_READY && !ready_present(s_last.latest))
                s_last.state = PC_UPDATE_AVAILABLE; /* nothing to swap in any more (after Go back): offered again */
            {
                /* the notes, in the player's language, else English */
                char* m;
                char key[32];
                path_in(p, 1100, L"update.txt");
                m = read_all_w(p, NULL);
                if (m) {
                    snprintf(key, sizeof key, "notes.%s", s_lang);
                    if (!ini_get(m, key, s_last.notes, sizeof s_last.notes)) ini_get(m, "notes.en", s_last.notes, sizeof s_last.notes);
                    for (char *r = s_last.notes, *w = s_last.notes; ; r++, w++) { /* "\n" into a line break */
                        if (r[0] == '\\' && r[1] == 'n') { *w = '\n'; r++; } else *w = *r;
                        if (!*r) break;
                    }
                    free(m);
                }
            }
            changed = 1;
        }
    }
    {
        char* pv;
        path_in(p, 1100, L"previous\\version.txt");
        pv = read_all_w(p, NULL);
        snprintf(s_last.previous, sizeof s_last.previous, "%s", pv ? pv : "");
        free(pv);
        /* *Fixed 2026-11-23 (Studio's finding in snatcherLeo's hand check):* version.txt ends in a line break (the staging
         * writes the repo's VERSION file whole), which was copied too, so About's "Go back to 0.1.0" broke its line;
         * the number alone is kept (any trailing space, CR or LF dropped) */
        for (size_t n = strlen(s_last.previous); n > 0 && (unsigned char)s_last.previous[n - 1] <= ' '; --n)
            s_last.previous[n - 1] = '\0';
        /* *Added 2026-11-23 (Studio's proposal from snatcherLeo's hand check):* after a Go back, the kept version is the newer
         * one; "Go back to 0.1.1" reads wrong, and the update line already offers it. So "previous" is only reported
         * when it is older than this version, and Go back's row hides (both programs read this) */
        if (s_last.previous[0] && !newer(s_version, s_last.previous)) s_last.previous[0] = '\0';
    }
    snprintf(s_last.current, sizeof s_last.current, "%s", s_version);
    {
        const int other = s_last.state == PC_UPDATE_READY ? other_program_running() : 0;
        if (other != s_last.other_running) changed = 1;
        s_last.other_running = other;
    }
    *out = s_last;
    return changed;
}

int pc_update_request_rollback(void) {
    wchar_t p[1100];
    char* pv;
    path_in(p, 1100, L"previous\\version.txt");
    pv = read_all_w(p, NULL);
    if (pv == NULL) return 0;
    for (size_t n = strlen(pv); n > 0 && (unsigned char)pv[n - 1] <= ' '; --n) pv[n - 1] = '\0';
    if (!newer(s_version, pv)) { /* the kept version isn't older (after a Go back): nothing to go back to */
        free(pv);
        return 0;
    }
    free(pv);
    path_in(p, 1100, L"rollback");
    return write_all_w(p, "1", 1);
}

void pc_update_shutdown(void) {
    if (!s_ready) return;
    InterlockedExchange(&s_quit, 1);
    SetEvent(s_wake);
    WaitForSingleObject(s_thread, 3000);
}

#else /* not Windows yet: no updates (pc_update.h; the programs show nothing) */

int pc_update_init(const char* root, const char* version, const char* lang) { (void)root; (void)version; (void)lang; return 0; }
void pc_update_set_auto_download(int on) { (void)on; }
void pc_update_check(int allowed) { (void)allowed; }
void pc_update_download(void) {}
void pc_update_set_playing(int playing) { (void)playing; }
int pc_update_poll(PcUpdateStatus* out) { if (out) { memset(out, 0, sizeof *out); out->state = PC_UPDATE_OFF; } return 0; }
int pc_update_request_rollback(void) { return 0; }
void pc_update_shutdown(void) {}

#endif
