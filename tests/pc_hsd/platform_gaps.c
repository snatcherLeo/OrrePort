/*
 * SDK functions Aurora does not provide (or provides differently), as the HSD test needs them. Compiled without the
 * game's forced prototypes header. Test-local; the platform layer (B) owns the real versions.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

/* VI: Aurora presents frames itself (no external frame buffers, no retrace interrupt to wait for) */
void VISetBlack(int black) { (void)black; }
void VISetNextFrameBuffer(void* fb) { (void)fb; }

/* fn_800060F0: the game's panic (file, line, message): print and stop */
void fn_800060F0(const char* file, int line, const char* msg, ...) {
    va_list ap;
    fprintf(stderr, "[panic] %s:%d: ", file != NULL ? file : "?", line);
    va_start(ap, msg);
    if (msg != NULL) {
        vfprintf(stderr, msg, ap);
    }
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
    abort();
}

/* Declared in Aurora's headers but not implemented (2026-10-15) */
typedef void (*VIRetraceCallback)(unsigned int retraceCount);
static VIRetraceCallback pre_cb, post_cb;
void GXSetCopyClamp(int clamp) { (void)clamp; }            /* fn_800B9874: EFB copy clamp; no EFB copies here */
unsigned int VIGetNextField(void) { return 0; }             /* fn_800AA2F0: progressive output, field 0 */
VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) { VIRetraceCallback old = pre_cb; pre_cb = cb; return old; }
VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) { VIRetraceCallback old = post_cb; post_cb = cb; return old; }
/* One simulated retrace per wait: the game counts retraces in HSD's post-retrace callback (GSgfx's frame pacing waits
 * for that count to move), and nothing on the PC raises the VI interrupt in this test. */
static unsigned int retrace_count;
void VIWaitForRetrace(void) {
    retrace_count++;
    if (pre_cb != NULL) pre_cb(retrace_count);
    if (post_cb != NULL) post_cb(retrace_count);
}
unsigned int VIGetRetraceCount(void) { return retrace_count; }

/* Not in Aurora: GXWaitDrawDone (fn_800B8DA8) and GXInitFogAdjTable (fn_800BCB14) */
void GXWaitDrawDone(void) {}
void GXInitFogAdjTable(void* table, unsigned short width, const float projmtx[4][4]) { (void)table; (void)width; (void)projmtx; }
void fn_800B8DA8(void) { GXWaitDrawDone(); }   /* address names, for units built without the alias headers */
void fn_800BCB14(void* table, unsigned short width, const float projmtx[4][4]) { GXInitFogAdjTable(table, width, projmtx); }
unsigned int fn_800AA2F0(void) { return VIGetNextField(); }
VIRetraceCallback fn_800A880C(VIRetraceCallback cb) { return VISetPreRetraceCallback(cb); }
VIRetraceCallback fn_800A8850(VIRetraceCallback cb) { return VISetPostRetraceCallback(cb); }
void fn_800B9874(int clamp) { GXSetCopyClamp(clamp); }

/* Diagnostics: how much drawing reaches Aurora (linked with -Wl,--wrap=GXCallDisplayList) */
unsigned int pc_hsd_dl_calls, pc_hsd_dl_bytes;
void __real_GXCallDisplayList(const void* list, unsigned int nbytes);
void __wrap_GXCallDisplayList(const void* list, unsigned int nbytes) {
    extern int pc_hsd_trace;
    pc_hsd_dl_calls++;
    if (pc_hsd_trace > 0) {
        printf("  gx: CallDisplayList %u bytes\n", nbytes);
        pc_hsd_trace--;
    }
    pc_hsd_dl_bytes += nbytes;
    __real_GXCallDisplayList(list, nbytes);
}

/* fn_800B84E0, the game's GXSetArray(attr, data, stride). Aurora's PC version also needs the array's size in bytes
 * (it uploads that many bytes to the GPU) and its byte order. The game never knows the size: on the GameCube the
 * hardware reads whatever the indices ask for. Here: arrays inside the loaded archive are big-endian disc data and
 * run at most to the end of the archive's data block (pc_hsd_array_lo / _hi, set by main.c); anything else is treated
 * as a native (little-endian) array of up to 64 KB. A real port needs a better rule (the index range of the meshes
 * using the array, computed once at load). */
typedef unsigned char u8;
typedef unsigned int u32;
void GXSetArray(int attr, const void* data, u32 size, u8 stride, _Bool le);
u32 pc_hsd_array_size(const void* data);
const unsigned char* pc_hsd_array_lo;
const unsigned char* pc_hsd_array_hi;
void fn_800B84E0(int attr, const void* data, u8 stride) {
    const unsigned char* p = (const unsigned char*)data;
    u32 known = pc_hsd_array_size(data); /* array_sizes.c: from the meshes' display lists */
    if (known != 0) {
        GXSetArray(attr, data, known, stride, 0);
    } else if (p >= pc_hsd_array_lo && p < pc_hsd_array_hi) {
        GXSetArray(attr, data, (u32)(pc_hsd_array_hi - p), stride, 0);
    } else {
        GXSetArray(attr, data, 0x10000, stride, 1);
    }
}

/* fn_800B90A4 (GXSetDrawDoneCallback): sdk_pc_alias.h keeps the address name because B's platform layer delivers
 * draw-done on the game thread (port/src/pc_gx_hooks.c). The test is single-threaded: forward directly. */
typedef void (*GXDrawDoneCallback)(void);
GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback cb);
GXDrawDoneCallback fn_800B90A4(GXDrawDoneCallback cb) { return GXSetDrawDoneCallback(cb); }
