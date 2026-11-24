/*
 * pc_os_alarm.c: the SDK's alarms, which Aurora declares but does not define (docs/port/sdk-gaps.md). Apart from
 * pc_os_sdk.c because it needs Aurora's clock (OSGetTime, in Aurora's core), which the audio tests do not link.
 *
 * On the GameCube an alarm fires its handler from the decrementer interrupt at a given time (OSGetTime ticks,
 * 40.5 MHz). Our game code only uses them on the GBA boot path (skipped for the prototype); SDK-side callers (DVD, SI)
 * are replaced by Aurora. On the PC the queue is checked once per field, at VIWaitForRetrace (pc_vi.c), with interrupts
 * disabled, on the game thread: an alarm fires at most about 16.7 ms late, which the game's uses tolerate (they are
 * delays, not clocks). Same queue layout as the SDK (fire-time ordered, prev / next links in the alarm itself).
 */
#include <dolphin/os.h>

static OSAlarm* s_alarm_head;
static OSTime s_alarm_run_now; /* while pc_os_run_alarms runs: its "now"; else 0 */

void OSInitAlarm(void) {}

void OSCreateAlarm(OSAlarm* alarm) {
    alarm->handler = NULL;
    alarm->tag = 0;
}

static void alarm_unlink(OSAlarm* alarm) {
    if (alarm->prev != NULL) {
        alarm->prev->next = alarm->next;
    } else if (s_alarm_head == alarm) {
        s_alarm_head = alarm->next;
    }
    if (alarm->next != NULL) {
        alarm->next->prev = alarm->prev;
    }
    alarm->prev = alarm->next = NULL;
}

static void alarm_insert(OSAlarm* alarm, OSTime fire) {
    OSAlarm** link = &s_alarm_head;
    OSAlarm* prev = NULL;
    alarm->fire = fire;
    while (*link != NULL && (*link)->fire <= fire) { /* equal times fire in the order they were set */
        prev = *link;
        link = &(*link)->next;
    }
    alarm->prev = prev;
    alarm->next = *link;
    if (*link != NULL) {
        (*link)->prev = alarm;
    }
    *link = alarm;
}

static void alarm_set(OSAlarm* alarm, OSTime fire, OSTime period, OSTime start, OSAlarmHandler handler) {
    BOOL enabled = OSDisableInterrupts();
    if (alarm->handler != NULL) {
        alarm_unlink(alarm); /* the SDK requires a cancelled alarm; re-setting a live one just moves it */
    }
    alarm->handler = handler;
    alarm->period = period;
    alarm->start = start;
    if (s_alarm_run_now != 0 && fire <= s_alarm_run_now) {
        fire = s_alarm_run_now + 1; /* set by a handler and already due: fires at the next check, so a handler that
                                       re-arms itself cannot keep the game thread in here forever */
    }
    alarm_insert(alarm, fire);
    OSRestoreInterrupts(enabled);
}

void OSSetAlarm(OSAlarm* alarm, OSTime tick, OSAlarmHandler handler) {
    alarm_set(alarm, OSGetTime() + tick, 0, 0, handler);
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler) { alarm_set(alarm, time, 0, 0, handler); }

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
    OSTime now = OSGetTime();
    OSTime fire = start;
    if (period > 0 && fire < now) {
        fire += ((now - start) / period + 1) * period; /* next period boundary after now, as the SDK does */
    }
    alarm_set(alarm, fire, period, start, handler);
}

void OSCancelAlarm(OSAlarm* alarm) {
    BOOL enabled = OSDisableInterrupts();
    if (alarm->handler != NULL) {
        alarm_unlink(alarm);
        alarm->handler = NULL;
    }
    OSRestoreInterrupts(enabled);
}

void OSSetAlarmTag(OSAlarm* alarm, u32 tag) { alarm->tag = tag; }

void OSCancelAlarms(u32 tag) {
    BOOL enabled = OSDisableInterrupts();
    OSAlarm* alarm = s_alarm_head;
    if (tag != 0) {
        while (alarm != NULL) {
            OSAlarm* next = alarm->next;
            if (alarm->tag == tag) {
                OSCancelAlarm(alarm);
            }
            alarm = next;
        }
    }
    OSRestoreInterrupts(enabled);
}

BOOL OSCheckAlarmQueue(void) {
    OSAlarm* alarm;
    for (alarm = s_alarm_head; alarm != NULL; alarm = alarm->next) {
        if (alarm->handler == NULL || (alarm->next != NULL && alarm->next->prev != alarm) ||
            (alarm->next != NULL && alarm->next->fire < alarm->fire)) {
            return FALSE;
        }
    }
    return TRUE;
}

/* Called by VIWaitForRetrace with interrupts disabled: fire every alarm that is due, earliest first. A handler may set
 * or cancel alarms (including its own); periodic alarms are re-queued before their handler runs, as in the SDK. */
void pc_os_run_alarms(void) {
    OSTime now = OSGetTime();
    s_alarm_run_now = now;
    while (s_alarm_head != NULL && s_alarm_head->fire <= now) {
        OSAlarm* alarm = s_alarm_head;
        OSAlarmHandler handler = alarm->handler;
        alarm_unlink(alarm);
        if (alarm->period > 0) {
            OSTime fire = alarm->fire + alarm->period;
            if (fire <= now) {
                fire += ((now - fire) / alarm->period + 1) * alarm->period;
            }
            alarm_insert(alarm, fire);
        } else {
            alarm->handler = NULL;
        }
        handler(alarm, NULL);
    }
    s_alarm_run_now = 0;
}
