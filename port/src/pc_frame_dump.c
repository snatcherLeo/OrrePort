/*
 * pc_frame_dump.c: ORRE_DUMP_FRAMES=600,3000,8000 writes those game frames as PNG files into
 * build-pc/firstlink/frames/ (more precisely: frames/ under the working folder), so a run can be checked without a
 * display or a desktop capture (2026-10-16; the project's request). Method as in A's pc_gfx_test
 * (tests/pc_hsd/main_gfx.c, PC_GFX_SCREEN_DUMP): at the end of the chosen frame the EFB is copied out as RGB565 in the
 * GameCube's 4x4 tiles (GXCopyTex to a key address, which Aurora uses only as an identifier) and read back through
 * Aurora's readback stream; when the result arrives (a frame or two later) it is untiled and written as a PNG (24-bit RGB).
 * *Updated 2026-11-02 (the reference playthrough's checks; B's file, flagged):* a stream keeps only its latest completed capture, so
 * one stream meant one capture in flight and dumps about 6 frames apart; a ring of DUMP_RING streams, each with its own
 * copy key, now keeps several in flight, so a range "A-B" (every frame from A to B, mixed freely with single frames:
 * ORRE_DUMP_FRAMES=6800-6860,20000) is captured frame by frame, for one-frame flickers.
 *
 * The copy changes the GX texture-copy settings (source rectangle, destination format); the game sets those itself
 * before each of its own copies, so this is likely harmless, but a dump frame is not guaranteed to be undisturbed.
 */
#include <dolphin/gx.h>
#include <dolphin/gx/GXAurora.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pc_paths.h" /* the screenshots' folder (4.1) */
#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define make_dir(path) mkdir(path, 0755)
#endif

#define DUMP_W 640
#define DUMP_H 480
#define DUMP_MAX 4096
#define DUMP_RING 8

static u32 s_frames[DUMP_MAX];
static int s_count = -1; /* -1: not parsed yet */
static int s_next;       /* index of the next frame to capture */
static u8 s_key[16];     /* the screenshot's copy key and stream */
static AuroraReadbackStream s_stream;
static u8 s_ring_key[DUMP_RING][16];             /* each slot's copy key (an identifier only) */
static AuroraReadbackStream s_ring_stream[DUMP_RING];
static u32 s_ring_frame[DUMP_RING];              /* the frame captured in each slot, 0 = free */

static int ring_busy(void) {
    for (int i = 0; i < DUMP_RING; i++) {
        if (s_ring_frame[i] != 0) {
            return 1;
        }
    }
    return 0;
}

static void parse(void) {
    const char* list = getenv("ORRE_DUMP_FRAMES");
    s_count = 0;
    while (list != NULL && *list != '\0' && s_count < DUMP_MAX) {
        char* end;
        unsigned long v = strtoul(list, &end, 10), last = v;
        if (end == list) {
            break;
        }
        if (*end == '-') { /* a range: every frame from v to last */
            last = strtoul(end + 1, &end, 10);
        }
        for (unsigned long f = v; f <= last && s_count < DUMP_MAX; f++) {
            s_frames[s_count++] = (u32)f;
        }
        list = *end == ',' ? end + 1 : end;
    }
    if (s_count > 0) {
        make_dir("frames");
    }
}

/* PNG without a compression library: zlib "stored" (uncompressed) deflate blocks, so only CRC-32 and Adler-32 are
 * needed; about 0.9 MB per 640x480 frame. */
static u32 crc_table[256];

static u32 crc32_update(u32 crc, const u8* p, size_t n) {
    if (crc_table[1] == 0) {
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) {
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            crc_table[i] = c;
        }
    }
    for (size_t i = 0; i < n; i++) {
        crc = crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static void put_be32(u8* p, u32 v) {
    p[0] = (u8)(v >> 24);
    p[1] = (u8)(v >> 16);
    p[2] = (u8)(v >> 8);
    p[3] = (u8)v;
}

static void write_chunk(FILE* f, const char* type, const u8* data, u32 len) {
    u8 head[8];
    u8 tail[4];
    u32 crc;
    put_be32(head, len);
    memcpy(head + 4, type, 4);
    fwrite(head, 1, 8, f);
    if (len != 0) {
        fwrite(data, 1, len, f);
    }
    crc = crc32_update(0xFFFFFFFFu, (const u8*)type, 4);
    crc = crc32_update(crc, data, len) ^ 0xFFFFFFFFu;
    put_be32(tail, crc);
    fwrite(tail, 1, 4, f);
}

static void write_png_to(const char* path, const u8* tiled);

static void write_png(u32 frame, const u8* tiled) {
    char path[64];
    snprintf(path, sizeof(path), "frames/frame_%06u.png", (unsigned)frame);
    write_png_to(path, tiled);
}

static void write_png_to(const char* path, const u8* tiled) {
    enum { ROW = 1 + DUMP_W * 3, RAW = ROW * DUMP_H, BLOCK = 65535 };
    static u8 raw[RAW];
    static u8 idat[2 + RAW + (RAW / BLOCK + 1) * 5 + 4];
    u8 ihdr[13];
    FILE* f;
    size_t pos = 0, off = 0;
    u32 a = 1, b = 0;

    for (int y = 0; y < DUMP_H; y++) {
        u8* row = &raw[(size_t)y * ROW];
        row[0] = 0; /* filter: none */
        for (int x = 0; x < DUMP_W; x++) {
            /* RGB565 tiles: 4x4 texels, 32 bytes each, tiles row-major; each texel a big-endian u16 */
            const u32 tile = (u32)(y / 4) * (DUMP_W / 4) + (u32)(x / 4);
            const u8* t = tiled + tile * 32 + ((y % 4) * 4 + (x % 4)) * 2;
            const u16 p = (u16)((t[0] << 8) | t[1]);
            u8* o = &row[1 + x * 3];
            o[0] = (u8)(((p >> 11) & 31) * 255 / 31);
            o[1] = (u8)(((p >> 5) & 63) * 255 / 63);
            o[2] = (u8)((p & 31) * 255 / 31);
        }
    }
    idat[pos++] = 0x78; /* zlib header: deflate, 32K window, no preset dictionary */
    idat[pos++] = 0x01;
    while (off < RAW) {
        const u32 n = (u32)(RAW - off < BLOCK ? RAW - off : BLOCK);
        idat[pos++] = off + n == RAW ? 1 : 0; /* final-block bit, type 00 (stored) */
        idat[pos++] = (u8)n;
        idat[pos++] = (u8)(n >> 8);
        idat[pos++] = (u8)~n;
        idat[pos++] = (u8)(~n >> 8);
        memcpy(&idat[pos], &raw[off], n);
        pos += n;
        off += n;
    }
    for (size_t i = 0; i < RAW; i++) {
        a = (a + raw[i]) % 65521;
        b = (b + a) % 65521;
    }
    put_be32(&idat[pos], (b << 16) | a);
    pos += 4;

    f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "[frame_dump] cannot write %s\n", path);
        return;
    }
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    put_be32(ihdr, DUMP_W);
    put_be32(ihdr + 4, DUMP_H);
    ihdr[8] = 8;  /* bit depth */
    ihdr[9] = 2;  /* RGB */
    ihdr[10] = 0; /* deflate */
    ihdr[11] = 0; /* adaptive filtering */
    ihdr[12] = 0; /* no interlace */
    write_chunk(f, "IHDR", ihdr, 13);
    write_chunk(f, "IDAT", idat, (u32)pos);
    write_chunk(f, "IEND", NULL, 0);
    fclose(f);
    fprintf(stderr, "[frame_dump] -> %s\n", path);
}

/* The screenshot key (pc_debug.c, F12 / Print Screen): the next frame, saved as
 * <exe folder>/screenshots/OrrePort_<commit>_<date>_<time>.png (the commit from the build stamp; _2, _3, ... for more
 * shots in the same second, screenshot_path). Same capture as the
 * frame dumps, with its own key, so it works whether or not ORRE_DUMP_FRAMES is set. */
#define SHOT_KEY 0x5C000000u
extern const char* pc_platform_data_path(const char* name, char* out, size_t size);
extern const char* pc_debug_build_stamp(void);
static int s_shot_requested;
static int s_shot_in_flight;
static u32 s_shot_frame;
static u32 s_shot_serial;
static char s_shot_path[1300];

void pc_frame_dump_request_screenshot(void) { s_shot_requested = 1; }

/* Report a problem (pc_settings_overlay.cpp): the same screenshot, saved to the given file instead of screenshots/ */
static int s_shot_to_path;
void pc_frame_dump_request_screenshot_to(const char* path) {
    snprintf(s_shot_path, sizeof(s_shot_path), "%s", path);
    s_shot_to_path = 1;
    s_shot_requested = 1;
}
/* 1 while a screenshot is asked for or on its way */
int pc_frame_dump_screenshot_busy(void) { return s_shot_requested || s_shot_in_flight; }

/* The debug toolkit's Pause (pc_debug.c): the same capture, handed over as 640x480 RGBA8 instead of a PNG, so the
 * paused picture can be drawn again while the game is held (nothing else draws then). */
static void (*s_pause_done)(const u8* rgba, int width, int height);
static int s_shot_for_pause;

void pc_frame_dump_request_pause_capture(void (*done)(const u8* rgba, int width, int height)) {
    s_pause_done = done;
    s_shot_for_pause = 1;
    s_shot_requested = 1;
}

static void untile_rgba(const u8* tiled, u8* rgba) {
    for (int y = 0; y < DUMP_H; y++) {
        for (int x = 0; x < DUMP_W; x++) {
            const u32 tile = (u32)(y / 4) * (DUMP_W / 4) + (u32)(x / 4);
            const u8* t = tiled + tile * 32 + ((y % 4) * 4 + (x % 4)) * 2;
            const u16 p = (u16)((t[0] << 8) | t[1]);
            u8* o = &rgba[((size_t)y * DUMP_W + x) * 4];
            o[0] = (u8)(((p >> 11) & 31) * 255 / 31);
            o[1] = (u8)(((p >> 5) & 63) * 255 / 63);
            o[2] = (u8)((p & 31) * 255 / 31);
            o[3] = 255;
        }
    }
}

static void screenshot_path(void) {
    char dir[1100], commit[16] = "unknown";
    const char* stamp = pc_debug_build_stamp();
    const char* dash = stamp != NULL ? strstr(stamp, " - ") : NULL;
    time_t now = time(NULL);
    struct tm* t = localtime(&now);
    if (dash != NULL) {
        int n = 0;
        for (const char* p = dash + 3; n < 12 && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')); p++) {
            commit[n++] = *p;
        }
        commit[n > 0 ? n : 0] = '\0';
        if (n == 0) {
            strcpy(commit, "unknown");
        }
    }
    /* Pokemon Colosseum\Screenshots in the portable layout, screenshots next to the exe in the flat one (4.1) */
    pc_paths_dir(PC_PLACE_SCREENSHOTS, dir, sizeof(dir));
    make_dir(dir);
    /* *Changed 2026-11-17:* the name has only whole seconds, so several presses within one second wrote the same file
     * and kept only the last (snatcherLeo, 2026-11-16: 6 presses gave 3 files). Now a taken name gets a counter instead:
     * OrrePort_<commit>_<date>_<time>.png, then _2, _3, ... in the same second (they sort after the first). Checking
     * the file works because a press is only turned into a capture once the previous one is written (s_shot_in_flight
     * holds it back), and it also keeps a shot from an earlier run in the same second. A counter rather than
     * milliseconds: plain C on every platform (time() has whole seconds; a finer clock would need Windows' and POSIX's
     * own calls). All platforms. */
    for (int n = 1; n <= 999; n++) {
        FILE* taken;
        char suffix[8] = "";
        if (n > 1) {
            snprintf(suffix, sizeof(suffix), "_%d", n);
        }
        snprintf(s_shot_path, sizeof(s_shot_path), "%s/OrrePort_%s_%04d%02d%02d_%02d%02d%02d%s.png", dir, commit,
                 t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec, suffix);
        taken = fopen(s_shot_path, "rb");
        if (taken == NULL) {
            break; /* a free name (999 in one second: the last one is overwritten, as before) */
        }
        fclose(taken);
    }
}

static void screenshot_end_of_frame(u32 frame) {
    static u8 pixels[DUMP_W * DUMP_H * 2];
    if (s_shot_in_flight) {
        AuroraReadbackDest dest = {SHOT_KEY + s_shot_serial, pixels, sizeof(pixels), GX_FALSE};
        if (AuroraReadbackGet(s_stream, &dest, 1) && dest.found) {
            if (s_shot_for_pause) {
                static u8 rgba[DUMP_W * DUMP_H * 4];
                untile_rgba(pixels, rgba);
                s_shot_for_pause = 0;
                if (s_pause_done != NULL) {
                    s_pause_done(rgba, DUMP_W, DUMP_H);
                }
            } else {
                write_png_to(s_shot_path, pixels);
            }
            s_shot_in_flight = 0;
        } else if (frame > s_shot_frame + 120) {
            fprintf(stderr, "[frame_dump] screenshot: no readback result\n");
            s_shot_in_flight = 0;
            s_shot_for_pause = 0;
        }
        return;
    }
    if (s_shot_requested && !ring_busy()) { /* the screenshot waits for the dumps in flight */
        AuroraReadbackCopySrc src = {SHOT_KEY + ++s_shot_serial, s_key};
        s_shot_requested = 0;
        if (s_stream == 0) {
            s_stream = AuroraReadbackCreateStream();
        }
        if (!s_shot_for_pause && !s_shot_to_path) {
            screenshot_path();
        }
        s_shot_to_path = 0;
        GXSetTexCopySrc(0, 0, DUMP_W, DUMP_H);
        GXSetTexCopyDst(DUMP_W, DUMP_H, GX_TF_RGB565, GX_FALSE);
        GXCopyTex(s_key, GX_FALSE);
        if (AuroraReadbackCaptureCopies(s_stream, &src, 1)) {
            s_shot_in_flight = 1;
            s_shot_frame = frame;
        } else {
            fprintf(stderr, "[frame_dump] screenshot refused (readback unavailable on this GPU?)\n");
            s_shot_for_pause = 0; /* the pause goes ahead without its picture (pc_debug.c's fallback) */
        }
    }
}

/* ORRE_DUMP_FULLRES=<frame>[,<frame>...] (the project's build helper, 2026-11-16; a test switch, never a setting): those game
 * frames written at full resolution by Aurora (aurora_orre_request_frame_dump) as frames/full_<frame>_render.ppm (the
 * frame buffer at the render size) and _window.ppm (after the present resampler, at its size in the window). Why: the
 * 640x480 dumps above are an EFB copy at the console's size, so they can't show how sharp text or edges are at 1080p
 * or 1440p (the text-softness question). Platform-neutral. */
extern void aurora_orre_request_frame_dump(const char* pathPrefix);
static u32 s_full[64];
static int s_full_count = -1, s_full_next;

static void fullres_end_of_frame(u32 frame) {
    if (s_full_count < 0) {
        const char* list = getenv("ORRE_DUMP_FULLRES");
        s_full_count = 0;
        while (list != NULL && *list != '\0' && s_full_count < 64) {
            char* end;
            const unsigned long v = strtoul(list, &end, 10);
            if (end == list) {
                break;
            }
            s_full[s_full_count++] = (u32)v;
            list = *end == ',' ? end + 1 : end;
        }
        if (s_full_count > 0) {
            make_dir("frames");
        }
    }
    if (s_full_next < s_full_count && frame >= s_full[s_full_next]) {
        char prefix[64];
        while (s_full_next < s_full_count && s_full[s_full_next] <= frame) {
            s_full_next++;
        }
        snprintf(prefix, sizeof(prefix), "frames/full_%06u", (unsigned)frame);
        aurora_orre_request_frame_dump(prefix);
        fprintf(stderr, "[frame_dump] full resolution -> %s_render.ppm / _window.ppm\n", prefix);
    }
}

/* Each frame, before Aurora's frame is ended (pc_vi.c: at the game's display copy, or at VIWaitForRetrace before
 * the first copy) */
void pc_frame_dump_end_of_frame(u32 frame) {
    fullres_end_of_frame(frame);
    if (s_count < 0) {
        parse();
    }
    if (s_shot_in_flight || (s_shot_requested && !ring_busy())) { /* a dump in flight finishes first */
        screenshot_end_of_frame(frame);
        return;
    }
    if (s_count == 0) {
        return;
    }
    for (int i = 0; i < DUMP_RING; i++) { /* collect the captures in flight */
        static u8 pixels[DUMP_W * DUMP_H * 2];
        AuroraReadbackDest dest;
        if (s_ring_frame[i] == 0) {
            continue;
        }
        /* keyed by its frame: the stream keeps its latest *completed* capture, so until this one completes a Get
         * returns the previous dump's picture; with per-capture keys that one is simply not found (A, 2026-10-17:
         * with the frame boundary at the display copy, each PNG held the previous capture) */
        dest = (AuroraReadbackDest){s_ring_frame[i], pixels, sizeof(pixels), GX_FALSE};
        if (AuroraReadbackGet(s_ring_stream[i], &dest, 1) && dest.found) {
            write_png(s_ring_frame[i], pixels);
            s_ring_frame[i] = 0;
        } else if (frame > s_ring_frame[i] + 120) {
            fprintf(stderr, "[frame_dump] frame %u: no readback result (readback unavailable?)\n",
                    (unsigned)s_ring_frame[i]);
            s_ring_frame[i] = 0;
        }
    }
    if (s_next < s_count && frame >= s_frames[s_next]) {
        int slot = -1;
        for (int i = 0; i < DUMP_RING && slot < 0; i++) {
            if (s_ring_frame[i] == 0) {
                slot = i;
            }
        }
        while (s_next < s_count && s_frames[s_next] <= frame) { /* this frame stands for any it caught up with */
            s_next++;
        }
        if (slot < 0) {
            fprintf(stderr, "[frame_dump] frame %u: all %d capture slots busy, skipped\n", (unsigned)frame, DUMP_RING);
            return;
        }
        {
            AuroraReadbackCopySrc src = {frame, s_ring_key[slot]};
            if (s_ring_stream[slot] == 0) {
                s_ring_stream[slot] = AuroraReadbackCreateStream();
            }
            GXSetTexCopySrc(0, 0, DUMP_W, DUMP_H);
            GXSetTexCopyDst(DUMP_W, DUMP_H, GX_TF_RGB565, GX_FALSE);
            GXCopyTex(s_ring_key[slot], GX_FALSE);
            if (AuroraReadbackCaptureCopies(s_ring_stream[slot], &src, 1)) {
                s_ring_frame[slot] = frame;
            } else {
                fprintf(stderr, "[frame_dump] frame %u: capture refused (readback unavailable on this GPU?)\n",
                        (unsigned)frame);
            }
        }
    }
}
