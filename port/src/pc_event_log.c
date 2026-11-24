/*
 * pc_event_log.c: the event log, lines in the program's one log (snatcherLeo, 2026-11-10: "Just own logger lines in the
 * pre-existing logger, so it can help with log zip files"), for the post-game sweep (every move, ability, item, weather
 * and species) and the story-flag catalogue, and for testers' report zips:
 *   [btext] every line the battle's text box shows, as text ("Golem used Earthquake!")
 *   [battle] each turn's Pokémon on the field, each move / item action as it is chosen (the existing battle log,
 *            switched on with this one; fight_floor_bios.c, fight_out_pokemon.c, fight_floor.c)
 *   [flag]   every story flag as it changes (frame, room, old -> new)
 *
 * Observe only: the game calls these through hook points (docs/port/hook-points.md: OrreHook_BattleMessage,
 * OrreHook_MessageMeasured, OrreHook_FlagChanged) that change nothing it does; a recorded sitting replays with the
 * same random numbers with the log on or off. The battle text comes from the game's own measuring pass for the message
 * (GSmsgGetRect, which the battle text box runs once as it opens to size itself), so the names, moves and numbers are
 * already filled in and the log adds no work of its own to the game.
 *
 * On by default; settings.ini `event_log = 0` (or ORRE_EVENT_LOG=0) turns it off. Platform layer only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pc_settings.h"

typedef unsigned int u32;
typedef unsigned short u16;
typedef int s32;

extern volatile unsigned int pc_vi_heartbeat; /* pc_vi.c: the game frame count */

/* the game's own lookups for names (prototypes.h) */
extern u32 wazaGetStatus(void* ptr, u16 dataId, u16 status, u32 index);
extern void* GSmsgGetGSchar(u32 key);

static int s_on = -1;
static u32 s_room = 0xFFFFFFFFu;

int pc_event_log_on(void) {
    if (s_on < 0) {
        const char* env = getenv("ORRE_EVENT_LOG");
        s_on = env != NULL ? atoi(env) != 0 : pc_settings_int("event_log", 1) != 0;
        printf("[events] %s (battle text, battle actions, story-flag changes; settings.ini event_log)\n",
               s_on ? "logging" : "off");
    }
    return s_on;
}

/* the battle log's switch (the game's ORRE_BATTLE_LOG blocks): on with the event log, or ORRE_BATTLE_LOG for the
 * debugging lines too */
int pc_battle_log_on(void) { return getenv("ORRE_BATTLE_LOG") != NULL || pc_event_log_on(); }

void pc_event_log_room(u32 room) { s_room = room; }

/* one UTF-16 code unit as UTF-8 (the game's text is Unicode: 'A' is 0x0041, 'é' 0x00E9) */
static void put_utf8(char* out, size_t* n, size_t cap, unsigned c) {
    char b[4];
    size_t len;
    if (c < 0x80) {
        b[0] = (char)c;
        len = 1;
    } else if (c < 0x800) {
        b[0] = (char)(0xC0 | (c >> 6));
        b[1] = (char)(0x80 | (c & 0x3F));
        len = 2;
    } else {
        b[0] = (char)(0xE0 | (c >> 12));
        b[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        b[2] = (char)(0x80 | (c & 0x3F));
        len = 3;
    }
    if (*n + len < cap) {
        memcpy(out + *n, b, len);
        *n += len;
    }
}

/* text as the log shows it: runs of spaces (and the control codes' line breaks, sent as 0x20) become one space */
static void to_text(const u16* chars, u32 count, char* out, size_t cap) {
    size_t n = 0;
    int space = 1;
    u32 i;
    for (i = 0; i < count; i++) {
        unsigned c = chars[i];
        if (c == 0x20 || c == 0x0A || c == 0x3000) {
            if (!space) {
                put_utf8(out, &n, cap, ' ');
            }
            space = 1;
            continue;
        }
        if (c < 0x20 || c == 0x300C || c == 0x300D) {
            continue; /* not printed: the game's own line-indent markers */
        }
        put_utf8(out, &n, cap, c);
        space = 0;
    }
    while (n > 0 && out[n - 1] == ' ') {
        n--;
    }
    out[n] = '\0';
}

/* a name from the message banks (big-endian UTF-16 in memory; 0xFFFF starts a control code, skipped) */
static void bank_text(u32 key, char* out, size_t cap) {
    const unsigned char* p = (const unsigned char*)GSmsgGetGSchar(key);
    u16 buf[64];
    u32 n = 0;
    out[0] = '\0';
    if (p == NULL || key == 0) {
        return;
    }
    while (n < 64) {
        const unsigned c = (unsigned)p[0] << 8 | p[1];
        if (c == 0) {
            break;
        }
        if (c == 0xFFFF) {
            p += 3; /* the escape and its control byte */
            continue;
        }
        buf[n++] = (u16)c;
        p += 2;
    }
    to_text(buf, n, out, cap);
}

/* the same reading for other OrrePort parts that need a game message as plain text (2026-11-14: the SAVE FILES
 * list's place names, through src/game/pc_game_ui.c). Short names only (64 characters): control codes are skipped by
 * their escape and code byte, which is enough for names without arguments. Goes through the game's own lookup, so the
 * chosen language's text comes back. All platforms. */
void pc_msg_text_utf8(u32 key, char* out, size_t cap) {
    if (out == NULL || cap == 0) {
        return;
    }
    bank_text(key, out, cap);
}

/* a move's name, for the battle log's move lines (wazaGetStatus status 1: the name's message key) */
const char* pc_event_log_move_name(u32 move) {
    static char name[64];
    if (move == 0) {
        return "-";
    }
    bank_text(wazaGetStatus(0, (u16)move, 1, 0), name, sizeof(name));
    return name[0] != '\0' ? name : "?";
}

/* ---- battle text ---- */
static int s_pending = 0;
static u32 s_pending_key = 0;

/* OrreHook_BattleMessage: winMsgOpenFight / winMsgOpenFightNoWait were asked for this message */
static void battle_message(u32 key) {
    if (s_pending) {
        printf("[btext] (message 0x%X opened, no text measured)\n", (unsigned)s_pending_key);
    }
    s_pending = 1;
    s_pending_key = key;
}

/* ---- dialogue outside battle (2026-11-10, snatcherLeo: "record every line of dialogue") ----
 * The field text box (window 0x40) and the menu message box (window 0x10, item results and the like) hold their
 * message key in their work area's first word (pc_debug_game.c, the [msg] log). A measurement of that same key is the
 * box's text, with the names filled in; one line per new key in each box (the same line twice in a row shows once). */
static const u32 s_dlg_windows[2] = {0x40, 0x10};
static u32 s_dlg_key[2];       /* the key each box shows now (0: closed) */
static int s_dlg_done[2];      /* its line was printed */
static u32 s_dlg_frame[2];     /* the frame it opened */

/* a box's key: its work area's first word, 0 when the box is closed */
static u32 dialog_box_key(int i) {
    extern void* windowSearchID();
    extern void* windowGetFreeWork(void* window);
    void* window = windowSearchID(s_dlg_windows[i]);
    void* work = window != NULL ? windowGetFreeWork(window) : NULL;
    return work != NULL ? *(u32*)work : 0u;
}

/* the speaker the game named for the next field message (OrreHook_CharacterTalk) */
static u32 s_spk_key = 0;
static char s_speaker[160];

static void dialog_print(int i, const char* text, const char* how) {
    const int named = s_spk_key != 0 && s_spk_key == s_dlg_key[i];
    printf("[text] %s   (message 0x%X, box 0x%X, frame %u, room 0x%X%s%s%s)\n", text, (unsigned)s_dlg_key[i],
           (unsigned)s_dlg_windows[i], (unsigned)s_dlg_frame[i], (unsigned)s_room, how, named ? ", speaker " : "",
           named ? s_speaker : "");
    s_dlg_done[i] = 1;
}

/* forward: the bank's text, for a name */
static void bank_text(u32 key, char* out, size_t cap);

/* OrreHook_CharacterTalk (people.c std054_characterTalk): a person is about to say message `message` */
static void character_talk(u32 group, u32 id, u32 message, u32 name_record, u32 name_message) {
    char name[64];
    name[0] = '\0';
    if (name_message != 0) {
        bank_text(name_message, name, sizeof(name));
    }
    s_spk_key = message;
    if (name_record == 0xFFFFFFFFu) {
        snprintf(s_speaker, sizeof(s_speaker), "people group %u index %u (no name record)", (unsigned)group,
                 (unsigned)id);
    } else {
        snprintf(s_speaker, sizeof(s_speaker), "%s (name record %u, people group %u index %u)",
                 name[0] != '\0' ? name : "?", (unsigned)name_record, (unsigned)group, (unsigned)id);
    }
}

/* the measured text (names filled in), when the game measures the open box's message */
static void dialog_measured(u32 key, const u16* chars, u32 count) {
    char text[512];
    int i;
    for (i = 0; i < 2; i++) {
        if (s_dlg_key[i] == key && key != 0 && !s_dlg_done[i]) {
            to_text(chars, count, text, sizeof(text));
            dialog_print(i, text, "");
            return;
        }
    }
}

/* the message's text from the bank, for a box whose text was never measured (the control codes skipped, so a name
 * the game fills in is missing; marked "from the bank") */
static void bank_text_long(u32 key, char* out, size_t cap) {
    const unsigned char* p = (const unsigned char*)GSmsgGetGSchar(key);
    u16 buf[400];
    u32 n = 0;
    out[0] = '\0';
    if (p == NULL || key == 0) {
        return;
    }
    while (n < 390) {
        const unsigned c = (unsigned)p[0] << 8 | p[1];
        if (c == 0) {
            break;
        }
        if (c == 0xFFFF) {
            /* a control code (messages.md): its byte, then the argument bytes its handler reads (msgctrl.c) */
            const unsigned code = p[2];
            const char* insert = NULL;
            p += 3;
            switch (code) {
            case 0x00: case 0x01: case 0x02: case 0x03:
                buf[n++] = 0x20; /* line and page breaks */
                break;
            case 0x08:
                p += 4; /* Color: R, G, B, A */
                break;
            case 0x07: case 0x09: case 0x38: case 0x52: case 0x53: case 0x54: case 0x5B: case 0x5C:
                p += 1; /* Font, Wait, Palette, Shadow, Align, TalkSE, LineSpace, BaseLineBias */
                break;
            case 0x2B: insert = "<player>"; break;
            case 0x2C: insert = "<partner>"; break;
            case 0x59: insert = "<speaker>"; break;
            default:
                if (code >= 0x0D && code <= 0x51 && code != 0x38 && code != 0x3D && code != 0x3E) {
                    insert = "<name>"; /* a name or number the game fills in */
                }
                break;
            }
            while (insert != NULL && *insert != '\0' && n < 398) {
                buf[n++] = (u16)(unsigned char)*insert++;
            }
            continue;
        }
        buf[n++] = (u16)c;
        p += 2;
    }
    to_text(buf, n, out, cap);
}

/* once a frame (pc_debug_game.c): a box showing a new key starts a line; a key left unprinted gets the bank's text */
void pc_event_log_dialog_frame(void) {
    char text[1024];
    int i;
    if (s_on != 1) {
        return; /* off, or not set up yet (pc_event_log_install decides) */
    }
    for (i = 0; i < 2; i++) {
        u32 key = dialog_box_key(i);
        if (key == s_dlg_key[i]) {
            continue;
        }
        if (s_dlg_key[i] != 0 && !s_dlg_done[i]) {
            bank_text_long(s_dlg_key[i], text, sizeof(text));
            dialog_print(i, text, ", from the bank");
        }
        s_dlg_key[i] = key;
        s_dlg_done[i] = 0;
        s_dlg_frame[i] = (u32)pc_vi_heartbeat;
    }
}

/* ---- who the player talks to (OrreHook_TalkStarted: hero_move.c, the A press's target) ---- */
static void talk_started(u32 group, u32 id) {
    static u32 last_group = 0xFFFFFFFFu, last_id, last_frame;
    if (group == last_group && id == last_id && (u32)pc_vi_heartbeat - last_frame < 120) {
        last_frame = (u32)pc_vi_heartbeat; /* the same A press held or repeated (a sign, a closed door): once */
        return;
    }
    last_group = group;
    last_id = id;
    last_frame = (u32)pc_vi_heartbeat;
    printf("[talk] WES talks to people group %u index %u (frame %u, room 0x%X)\n", (unsigned)group, (unsigned)id,
           (unsigned)pc_vi_heartbeat, (unsigned)s_room);
}

/* OrreHook_MessageMeasured: the game measured a message's expanded text (GSmsgGetRect); the battle box does this once
 * as it opens */
static void message_measured(u32 key, const u16* chars, u32 count) {
    char text[512];
    if (!s_pending || key != s_pending_key) {
        dialog_measured(key, chars, count);
        return; /* other measurements (the battle HUD's labels, menus): not battle text */
    }
    s_pending = 0;
    to_text(chars, count, text, sizeof(text));
    printf("[btext] %s   (message 0x%X, frame %u)\n", text, (unsigned)key, (unsigned)pc_vi_heartbeat);
}

/* ---- story flags ---- */
/* *Changed 2026-11-20 (autosave):* chained. The autosave (pc_autosave.cpp) also watches flag changes ("no story flag
 * changed since the room was entered") and registers first, at start-up; this log is attached later
 * (pc_game_main.c), so it keeps the earlier listener and calls it. All platforms */
static void (*s_prev_flag_changed)(s32 id, u32 old_value, u32 new_value);
static void flag_changed(s32 id, u32 old_value, u32 new_value) {
    if (s_prev_flag_changed != NULL) s_prev_flag_changed(id, old_value, new_value);
    printf("[flag] %d: %u -> %u (frame %u, room 0x%X)\n", (int)id, (unsigned)old_value, (unsigned)new_value,
           (unsigned)pc_vi_heartbeat, (unsigned)s_room);
}

/* pc_game_main.c: attach the hooks when the log is on */
extern void (*OrreHook_BattleMessage)(u32 message);
extern void (*OrreHook_MessageMeasured)(u32 key, const u16* chars, u32 count);
extern void (*OrreHook_FlagChanged)(s32 flagId, u32 oldValue, u32 newValue);
extern void (*OrreHook_TalkStarted)(u32 group, u32 id);
extern void (*OrreHook_CharacterTalk)(u32 group, u32 id, u32 message, u32 nameRecord, u32 nameMessage);

void pc_event_log_install(void) {
    if (!pc_event_log_on()) {
        return;
    }
    OrreHook_BattleMessage = battle_message;
    OrreHook_MessageMeasured = message_measured;
    if (OrreHook_FlagChanged != flag_changed) s_prev_flag_changed = OrreHook_FlagChanged; /* the autosave's (above) */
    OrreHook_FlagChanged = flag_changed;
    OrreHook_TalkStarted = talk_started;
    OrreHook_CharacterTalk = character_talk;
}
