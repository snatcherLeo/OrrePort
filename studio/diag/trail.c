/* trail.c: see trail.h. Plain C; only the clock differs by system: Windows' GetTickCount64 (MinGW's C library has no
 * timespec_get), POSIX clock_gettime(CLOCK_MONOTONIC) on Linux and macOS. */
#include "trail.h"

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

enum { kKeep = 64, kText = 200, kCat = 12 };

typedef struct {
    double t;          /* seconds since the first note's clock start */
    char cat[kCat];
    char text[kText];
} Note;

static Note s_ring[kKeep];
static int s_count;  /* notes ever made */
static void (*s_sink)(const char*);
static double s_t0 = -1;

static double now_s(void) {
#ifdef _WIN32
    const double t = (double)GetTickCount64() / 1000.0;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    const double t = (double)ts.tv_sec + ts.tv_nsec / 1e9;
#endif
    if (s_t0 < 0) s_t0 = t;
    return t - s_t0;
}

void trail_set_sink(void (*sink)(const char* line)) {
    s_sink = sink;
    now_s();  /* the clock starts with the program, not with the first note */
}

static int format(const Note* n, char* out, size_t size) {
    return snprintf(out, size, "%8.3f %-7s %s", n->t, n->cat, n->text);
}

void trail_note(const char* category, const char* text) {
    Note* n = &s_ring[s_count % kKeep];
    n->t = now_s();
    snprintf(n->cat, sizeof n->cat, "%s", category ? category : "");
    snprintf(n->text, sizeof n->text, "%s", text ? text : "");
    ++s_count;
    if (s_sink) {
        char line[kText + kCat + 24];
        format(n, line, sizeof line);
        s_sink(line);
    }
}

int trail_last(int n, char* out, size_t size) {
    if (!out || size == 0) return 0;
    out[0] = '\0';
    const int have = s_count < kKeep ? s_count : kKeep;
    if (n > have) n = have;
    if (n < 0) n = 0;
    size_t used = 0;
    int written = 0;
    for (int i = s_count - n; i < s_count; ++i) {
        char line[kText + kCat + 24];
        const int len = format(&s_ring[i % kKeep], line, sizeof line);
        if (len < 0 || used + (size_t)len + 2 > size) break;
        memcpy(out + used, line, (size_t)len);
        used += (size_t)len;
        out[used++] = '\n';
        out[used] = '\0';
        ++written;
    }
    return written;
}
