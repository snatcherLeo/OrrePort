/*
 * pc_sdk_test: the SDK OS / math functions the platform layer provides (port/src/pc_os_sdk.c, pc_mtx_ps.c;
 * docs/port/sdk-gaps.md). Exit code 0 = all passed.
 *   - interrupt masking: GameCube nesting rules, and real exclusion of a second thread (the audio thread's case);
 *   - PS math: called through the real PSMTX* / PSVEC* symbols (this file does not include Aurora's mtx.h, whose
 *     macros would bypass them), results checked by hand;
 *   - reset functions: priority order, the "not yet" passes, the final pass; GBA ports report "not ready";
 *   - alarms: firing order, not firing early, cancel, cancel by tag, a handler re-setting its alarm, periodic alarms
 *     (pc_os_run_alarms is what VIWaitForRetrace calls once per field).
 */
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

typedef int BOOL;
BOOL OSDisableInterrupts(void);
BOOL OSRestoreInterrupts(BOOL level);
BOOL OSEnableInterrupts(void);
int pc_os_interrupt_depth(void);
void pc_os_audio_enter(void);
void pc_os_audio_leave(void);

typedef float Mtx[3][4];
typedef struct {
    float x, y, z;
} Vec;
void PSMTXIdentity(Mtx m);
void PSMTXTrans(Mtx m, float x, float y, float z);
void PSMTXScale(Mtx m, float x, float y, float z);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);
void PSMTXMultVec(const Mtx m, const Vec* src, Vec* dst);
void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* axb);
float PSVECDotProduct(const Vec* a, const Vec* b);

/* the SDK's OSAlarm (Aurora's dolphin/os/OSAlarm.h), declared here so this file stays free of SDK headers */
typedef long long OSTime;
typedef struct OSAlarm OSAlarm;
typedef void (*OSAlarmHandler)(OSAlarm* alarm, void* context);
struct OSAlarm {
    OSAlarmHandler handler;
    unsigned tag;
    OSTime fire;
    OSAlarm* prev;
    OSAlarm* next;
    OSTime period;
    OSTime start;
};
OSTime OSGetTime(void);
void OSCreateAlarm(OSAlarm* alarm);
void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler);
void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler);
void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler);
void OSCancelAlarm(OSAlarm* alarm);
void OSSetAlarmTag(OSAlarm* alarm, unsigned tag);
void OSCancelAlarms(unsigned tag);
BOOL OSCheckAlarmQueue(void);
void pc_os_run_alarms(void);

/* reset (Aurora's dolphin/os/OSReset.h layout) and the GBA stub */
typedef struct ResetInfo {
    BOOL (*func)(BOOL final);
    unsigned priority;
    struct ResetInfo* next;
    struct ResetInfo* prev;
} ResetInfo;
void OSRegisterResetFunction(ResetInfo* info);
void OSResetSystem(int reset, unsigned resetCode, BOOL forceMenu);
void pc_os_set_reset_exit(void (*fn)(int code));
int GBAGetStatus(int chan, unsigned char* status);

static char s_reset_log[32];
static int s_nreset, s_reset_exited, s_slow_left = 2;
static void reset_note(char c) {
    if (s_nreset < 31) {
        s_reset_log[s_nreset++] = c;
        s_reset_log[s_nreset] = 0;
    }
}
static BOOL reset_a(BOOL final) { reset_note(final ? 'A' : 'a'); return 1; }
static BOOL reset_b(BOOL final) {
    reset_note(final ? 'B' : 'b');
    return final || --s_slow_left <= 0; /* "not yet" twice */
}
static void reset_exit(int code) { s_reset_exited = 1 + code; }

static char s_fired[16];
static int s_nfired;
static OSAlarm s_al[4];
static void fired(OSAlarm* alarm, void* context) {
    (void)context;
    if (s_nfired < 15) {
        s_fired[s_nfired++] = (char)('A' + (alarm - s_al));
    }
}
static int s_rearm_left;
static void rearm(OSAlarm* alarm, void* context) {
    fired(alarm, context);
    if (--s_rearm_left > 0) {
        OSSetAbsAlarm(alarm, 0, rearm); /* due again at once: must wait for the next check, not loop forever */
    }
}

static int check(const char* what, int ok) {
    printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

static SDL_AtomicInt s_entered;
static int SDLCALL audio_thread(void* arg) {
    (void)arg;
    pc_os_audio_enter(); /* blocks while the game thread has interrupts disabled */
    SDL_SetAtomicInt(&s_entered, 1);
    pc_os_audio_leave();
    return 0;
}

int main(void) {
    int ok = 1;
    BOOL outer, inner;
    SDL_Thread* t;
    Mtx a, b, ab;
    Vec v = {1, 2, 3}, r, x = {1, 0, 0}, y = {0, 1, 0};

    /* nesting as the game uses it: old = Disable(); ... Restore(old); */
    outer = OSDisableInterrupts();
    inner = OSDisableInterrupts();
    ok &= check("Disable returns the old state (enabled, then disabled)", outer == 1 && inner == 0);
    OSRestoreInterrupts(inner);
    ok &= check("inner Restore(FALSE) keeps them disabled", pc_os_interrupt_depth() == 1);
    OSRestoreInterrupts(outer);
    ok &= check("outer Restore(TRUE) enables again", pc_os_interrupt_depth() == 0);
    OSRestoreInterrupts(1);
    ok &= check("Restore(TRUE) with nothing to undo is harmless", pc_os_interrupt_depth() == 0);
    OSDisableInterrupts();
    OSDisableInterrupts();
    OSEnableInterrupts();
    ok &= check("Enable undoes every Disable of this thread", pc_os_interrupt_depth() == 0);

    /* exclusion: the audio thread must wait until the game thread restores interrupts */
    outer = OSDisableInterrupts();
    t = SDL_CreateThread(audio_thread, "audio", NULL);
    SDL_Delay(50);
    ok &= check("audio thread waits while interrupts are disabled", SDL_GetAtomicInt(&s_entered) == 0);
    OSRestoreInterrupts(outer);
    SDL_WaitThread(t, NULL);
    ok &= check("audio thread runs once they are restored", SDL_GetAtomicInt(&s_entered) == 1);

    /* PS math through the real symbols */
    PSMTXTrans(a, 10, 20, 30);
    PSMTXScale(b, 2, 2, 2);
    PSMTXConcat(a, b, ab); /* scale, then translate */
    PSMTXMultVec(ab, &v, &r);
    ok &= check("PSMTXConcat / PSMTXMultVec: (1,2,3) scaled by 2 then moved by (10,20,30) = (12,24,36)",
                fabsf(r.x - 12) < 1e-5f && fabsf(r.y - 24) < 1e-5f && fabsf(r.z - 36) < 1e-5f);
    PSVECCrossProduct(&x, &y, &r);
    ok &= check("PSVECCrossProduct: x cross y = z", r.x == 0 && r.y == 0 && r.z == 1);
    ok &= check("PSVECDotProduct: (1,2,3).(1,2,3) = 14", PSVECDotProduct(&v, &v) == 14.0f);
    PSMTXIdentity(a);
    ok &= check("PSMTXIdentity", a[0][0] == 1 && a[1][1] == 1 && a[2][2] == 1 && a[0][3] == 0);

    /* alarms: times far in the past or future, so the test does not depend on the clock's speed */
    {
        OSTime now = OSGetTime();
        int i;
        for (i = 0; i < 4; i++) {
            OSCreateAlarm(&s_al[i]);
        }
        OSSetAbsAlarm(&s_al[2], now - 300, fired);
        OSSetAbsAlarm(&s_al[0], now - 100, fired);
        OSSetAbsAlarm(&s_al[1], now - 200, fired);
        OSSetAlarm(&s_al[3], 40500000LL * 3600, fired); /* an hour from now */
        ok &= check("alarm queue is ordered and linked", OSCheckAlarmQueue());
        pc_os_run_alarms();
        s_fired[s_nfired] = 0;
        ok &= check("due alarms fire earliest first, the future one not at all", strcmp(s_fired, "CBA") == 0);
        OSCancelAlarm(&s_al[3]);
        s_nfired = 0;
        pc_os_run_alarms();
        ok &= check("a cancelled alarm stays empty", s_nfired == 0 && s_al[3].handler == NULL);

        OSSetAbsAlarm(&s_al[0], now - 10, fired);
        OSSetAbsAlarm(&s_al[1], now - 10, fired);
        OSSetAlarmTag(&s_al[0], 7);
        OSCancelAlarms(7);
        pc_os_run_alarms();
        s_fired[s_nfired] = 0;
        ok &= check("OSCancelAlarms(tag) cancels only that tag; equal times keep their order", strcmp(s_fired, "B") == 0);

        s_nfired = 0;
        s_rearm_left = 3;
        OSSetAbsAlarm(&s_al[2], 0, rearm);
        pc_os_run_alarms();
        ok &= check("a handler re-setting its own alarm fires once per check", s_nfired == 1);
        pc_os_run_alarms();
        pc_os_run_alarms();
        pc_os_run_alarms();
        ok &= check("... and again at each later check until it stops", s_nfired == 3);

        s_nfired = 0;
        OSSetPeriodicAlarm(&s_al[0], now - 1000, 400, fired);
        pc_os_run_alarms();
        ok &= check("a periodic alarm started in the past waits for its next period boundary, as in the SDK",
                    s_nfired == 0 && s_al[0].handler != NULL && s_al[0].fire > now &&
                        (s_al[0].fire - (now - 1000)) % 400 == 0);
        OSCancelAlarm(&s_al[0]);
        ok &= check("alarm queue is empty and consistent afterwards", OSCheckAlarmQueue());
    }

    /* reset: lower priority first; repeated until all agree; then the final pass */
    {
        static ResetInfo ra = {reset_a, 127}, rb = {reset_b, 10};
        unsigned char status = 0xFF;
        OSRegisterResetFunction(&ra);
        OSRegisterResetFunction(&rb);
        pc_os_set_reset_exit(reset_exit);
        OSResetSystem(0, 0, 1);
        pc_os_set_reset_exit(NULL);
        ok &= check("OSResetSystem: priority order, 'not yet' passes repeated, then the final pass, then exit",
                    strcmp(s_reset_log, "babaBA") == 0 && s_reset_exited == 1);
        ok &= check("GBA ports report not ready (nothing connected)", GBAGetStatus(0, &status) == 1 && status == 0);
    }

    printf("result: %s\n", ok ? "ALL PASSED" : "SOME FAILED");
    return ok ? 0 : 1;
}
