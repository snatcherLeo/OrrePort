"""Play a PC pad script in Dolphin from power-on, with the logs the golden-slice comparison needs (testing lane).

Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set SLICE_PAD=<pad script file>   set TESTING_DIR=<repo>/tools/local/testing
    set SLICE_END=600      (frames to keep running after the last step; default 600)
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_slice.py

The pad script is the project's ORRE_PAD_SCRIPT format (port/src/pc_pad_script.c), read the same way, so ONE file drives the PC
and Dolphin:
- steps WHEN:WHAT, separated by commas or new lines, '#' starts a comment;
- WHEN: N = at frame N (counted from power-on here; the PC counts VIWaitForRetrace calls), +N = N frames after the
  previous step fired, @R+N = N frames after room R is entered (the first entry at or after the previous step);
- WHAT: '-' or buttons joined by '+' (A B X Y Z L R START UP DOWN LEFT RIGHT), SX= / SY= (main stick, 0-255);
  "!seed N" sets the RNG state (as the PC's debug command does: *lbl_80478C94 = N); other "!" commands are skipped
  (logged: Dolphin has no debug toolkit).
"Room entered" is detected the way the PC reports it (idlog.py: the floor context switching to a new room; not the
boot room, not a return from a menu), so @R+N steps fire at the same game moment on both sides as long as both load
the room in the same number of frames.

Writes in POS_OUT: ids.jsonl (idlog: battles, room requests, and every room entry with frame, RNG state and the story
flags' CRC; the full flags beside it as flags_entry_NNN_<room>.json), slice.jsonl (each step as it fires), and a
screenshot at each room entry (shots/). Compare with a PC run using slice_compare.py.
"""
import json, os, sys, time, zlib

OUT = os.environ.get("POS_OUT", ".")
PAD = os.environ["SLICE_PAD"]
END = int(os.environ.get("SLICE_END", "600"))
STOP_FRAME = int(os.environ.get("SLICE_STOP_FRAME", "0"))
SAVE = os.environ.get("SLICE_SAVE")   # optional: save a state at the end (the next part's starting point)
sys.path.insert(0, os.environ.get("TESTING_DIR", ""))
from dolphin import controller, event, memory, savestate
import idlog

# SLICE_SHOT_RANGE=lo-hi:step: screenshots every step frames from frame lo to hi (e.g. a transition, for the project's checks)
_r = os.environ.get("SLICE_SHOT_RANGE")
SHOT_RANGE = None
if _r:
    _span, _step = _r.split(":")
    SHOT_RANGE = (int(_span.split("-")[0]), int(_span.split("-")[1]), int(_step))
# SLICE_RNG_RANGE=lo-hi: log the RNG state every frame from lo to hi, with the generator steps since the previous frame
# (draws per frame; to compare with the PC's ORRE_RNG_TRACE around a reseed)
_rr = os.environ.get("SLICE_RNG_RANGE")
RNG_RANGE = tuple(int(v) for v in _rr.split("-")) if _rr else None


def _steps(a, b, limit=50000):
    if a is None or b is None:
        return None
    n = 0
    while a != b:
        a = (a * 0x343FD + 0x269EC3) & 0xFFFFFFFF
        n += 1
        if n > limit:
            return -1
    return n


SHOTS = os.path.join(OUT, "shots")
os.makedirs(SHOTS, exist_ok=True)
log_f = open(os.path.join(OUT, "slice.jsonl"), "w", buffering=1)
idlog.open_log(OUT, run="slice:" + os.path.basename(PAD))


def log(**kw):
    log_f.write(json.dumps(kw) + "\n")


BUTTONS = {"A": "A", "B": "B", "X": "X", "Y": "Y", "Z": "Z", "L": "L", "R": "R", "START": "Start",
           "UP": "Up", "DOWN": "Down", "LEFT": "Left", "RIGHT": "Right"}


# value to send so the game reads a given stick value (inverse of the fork's measured mapping; see parse())
STICK_X_SEND = {(128 * (1 + (v & 1)) - v) & 255: v for v in range(256)}
STICK_Y_SEND = {(v if v & 1 else (v + 128) & 255): v for v in range(256)}


def parse(text):
    steps = []
    # comments first, per line (they may contain commas), then the comma-separated steps
    lines = [part for raw in text.splitlines() for part in raw.split("#")[0].split(",")]
    for raw in lines:
        line = raw.strip()
        if not line:
            continue
        when, what = line.split(":", 1)
        when = when.strip()
        if when.startswith("@"):
            r, n = when[1:].split("+")
            step = {"when": "room", "room": int(r, 0), "n": int(n)}
        elif when.startswith("+"):
            step = {"when": "after", "n": int(when[1:])}
        else:
            step = {"when": "frame", "n": int(when)}
        what = what.strip()
        if what.startswith("!"):
            step["command"] = what[1:].strip()
        else:
            pad = {}
            if what != "-":
                for tok in what.split("+"):
                    tok = tok.strip().upper()
                    if tok.startswith("SX="):
                        pad["StickX"] = int(tok[3:])
                    elif tok.startswith("SY="):
                        pad["StickY"] = int(tok[3:])
                    elif tok in BUTTONS:
                        pad[BUTTONS[tok]] = True
                # Pad-script stick values are what the game reads (PADStatus + 128, as on the PC and as
                # dolphin_record.py records them). The fork does not pass StickX / StickY through unchanged: measured
                # for all 256 values per axis (dolphin_stickcal.py, 2026-10-17), the game reads
                # X: (128 * (1 + (v & 1)) - v) & 255 and Y: v if v is odd, else (v + 128) & 255; an axis left out of
                # the dict arrives centred. So a centred axis is left out, and the others are sent pre-mapped.
                # *Corrected 2026-10-17:* the table above was measured against the game's stored copy (`latched`), in
                # which the game has flipped Y (input.c: `s->stickY = -s->stickY`); pad-script values are the raw
                # controller's (as on the PC), so Y is flipped to the stored convention before the lookup.
                if "StickY" in pad:
                    pad["StickY"] = (256 - pad["StickY"]) & 255
                for axis, table in (("StickX", STICK_X_SEND), ("StickY", STICK_Y_SEND)):
                    if axis in pad:
                        if pad[axis] == 128:
                            del pad[axis]
                        else:
                            pad[axis] = table[pad[axis]]
            step["pad"] = pad
        step["text"] = line
        steps.append(step)
    return steps


try:
    steps = parse(open(PAD).read())
except Exception:
    import traceback
    open(os.path.join(OUT, "slice_error.txt"), "w").write(traceback.format_exc())
    raise
log(ev="start", pad=PAD, steps=len(steps))
frame = 0
st = {"next": 0, "prev_fired": 0, "pad": {}, "done_frame": None}
pending_shots = []


def due(step):
    """The frame this step fires at, or None while it waits for a room entry."""
    if step["when"] == "frame":
        return step["n"]
    if step["when"] == "after":
        return st["prev_fired"] + step["n"]
    for room, f in idlog.entered:
        if room == step["room"] and f >= st["prev_fired"]:
            return f + step["n"]
    return None


# Battles during the slice, in battle_compare.py's reference format (battle/ref.jsonl: start, turn, hp), so the same run
# is the battle reference too. Memory as dolphin_battle_ref.py (gFightFloor; field slot -> +4 -> +4 = Pokemon record).
FLOOR = 0x8046DD90
BATTLE_DIR = os.path.join(OUT, "battle")
bst = {"enc": None, "turn": None, "hp": {}, "f": None}


def _ram(a):
    return 0x80000000 <= a < 0x81800000


def _mon(op):
    if not _ram(op):
        return None
    fp = memory.read_u32(op + 4)
    if not _ram(fp):
        return None
    pk = fp + 4
    return {"record": hex(pk), "species": memory.read_u16(pk), "hp": memory.read_u16(pk + 0x8A),
            "max_hp": memory.read_u16(pk + 0x8C)}


def _rng():
    p = memory.read_u32(0x80478C94)
    return memory.read_u32(p) if _ram(p) else None


def _blog(**kw):
    if bst["f"] is None:
        os.makedirs(BATTLE_DIR, exist_ok=True)
        bst["f"] = open(os.path.join(BATTLE_DIR, "ref.jsonl"), "w", buffering=1)
    bst["f"].write(json.dumps(kw) + "\n")


def battle_frame():
    enc = memory.read_u16(FLOOR + 0x12)
    if bst["enc"] is None:           # the value at power-on (or a leftover) is not a battle start
        bst["enc"] = enc
        return
    field = [_mon(memory.read_u32(FLOOR + 0xA4C4 + 4 * i)) for i in range(8)]
    live = [m for m in field if m]
    if enc != bst["enc"] and enc and live:
        bst["enc"], bst["turn"] = enc, None
        _blog(ev="start", frame=frame, battle=enc, rng=_rng(), field=field)
    elif enc != bst["enc"]:
        bst["enc"] = enc
    if not enc or not live:
        return
    turn = memory.read_u16(FLOOR + 0x10)
    if turn != bst["turn"]:
        bst["turn"] = turn
        _blog(ev="turn", frame=frame, turn=turn, rng=_rng(), turn_random=memory.read_u16(FLOOR + 0xA4E4), field=field)
    # each action: the attacker / target field slots (gFightFloor +0xA474 / +0xA478) and the appointed move (+0xA4BA),
    # logged when they change, so a PC run's choices and damage can be compared action by action
    act = (memory.read_u32(FLOOR + 0xA474), memory.read_u32(FLOOR + 0xA478), memory.read_u16(FLOOR + 0xA4BA))
    if act != bst.get("act"):
        bst["act"] = act
        att, tgt = _mon(act[0]), _mon(act[1])
        _blog(ev="action", frame=frame, turn=turn, attacker=att and att["species"], target=tgt and tgt["species"],
              attacker_slot=hex(act[0]), target_slot=hex(act[1]), move=act[2], rng=_rng())
    for m in live:
        k = m["record"]
        if bst["hp"].get(k) is not None and m["hp"] != bst["hp"][k]:
            _blog(ev="hp", frame=frame, turn=turn, species=m["species"], before=bst["hp"][k], after=m["hp"], rng=_rng())
        bst["hp"][k] = m["hp"]


PEOPLE_PTR, PEOPLE_MAX, ENTRY = 0x8047B200, 0x8047B1F8, 0xDC


def player_pos():
    """Wes's position (as dolphin_walk.py reads it: people table entry group 0, index 100; model +0x18), or None."""
    base, n = memory.read_u32(PEOPLE_PTR), memory.read_u32(PEOPLE_MAX)
    if not _ram(base) or n > 512:
        return None
    for i in range(n):
        e = base + i * ENTRY
        if memory.read_u8(e) and memory.read_u32(e + 0x28) == 0 and memory.read_u32(e + 0x2C) == 100:
            m = memory.read_u32(e + 0x08)
            if _ram(m):
                return (memory.read_f32(m + 0x18), memory.read_f32(m + 0x1C), memory.read_f32(m + 0x20))
    return None


# SLICE_VOICE_RANGE=lo-hi: log the MusyX DSP voices that are playing, frame by frame, to voices.jsonl (B1, the PC vs
# console sound levels). Layout from B (hw_dspctrl.h, computed by hand, likely): the voice array's base is the u32 at
# dspVoice (0x8047B024), the count the u8 at 0x8047B05D, 0xF4 bytes per voice; +0x4C..0x5C the nine volumes (main L/R/S, aux A
# L/R/S, aux B L/R/S), +0x70 sample id, +0xAC the envelope's current level (s32), +0xD0 / +0xD2 the ITD shifts, +0xEC
# state (0 = off), +0xEF studio, +0xF0 flags; +0x00 points to the DSP parameter block, whose mix volumes start at +0x12.
# A voice's line is written only when one of its values changes, so the file stays small.
_vr = os.environ.get("SLICE_VOICE_RANGE")
VOICE_RANGE = tuple(int(v) for v in _vr.split("-")) if _vr else None
voice_f = open(os.path.join(OUT, "voices.jsonl"), "w", buffering=1) if VOICE_RANGE else None
voice_last = {}


def log_voices():
    base, count = memory.read_u32(0x8047B024), memory.read_u8(0x8047B05D)
    if not _ram(base) or count == 0 or count > 128:
        return
    for v in range(count):
        a = base + v * 0xF4
        state = memory.read_u8(a + 0xEC)
        if state == 0:
            if voice_last.pop(v, None) is not None:
                voice_f.write(json.dumps({"frame": frame, "voice": v, "state": 0}) + "\n")
            continue
        vols = [memory.read_u16(a + 0x4C + 2 * i) for i in range(9)]
        rec = {"state": state, "studio": memory.read_u8(a + 0xEF), "flags": memory.read_u32(a + 0xF0),
               "smp_id": memory.read_u16(a + 0x70), "vol": vols,
               "adsr": memory.read_u32(a + 0xAC), "itd": [memory.read_u16(a + 0xD0), memory.read_u16(a + 0xD2)]}
        pb = memory.read_u32(a)
        if _ram(pb):
            rec["pb_mix"] = [memory.read_u16(pb + 0x12 + 2 * i) for i in range(12)]
        key = json.dumps(rec, sort_keys=True)
        if voice_last.get(v) != key:
            voice_last[v] = key
            voice_f.write(json.dumps(dict(rec, frame=frame, voice=v)) + "\n")


# SLICE_MUSIC_MUTE=1: mute the music the game's own way every frame, as sndMasterVolume(0, 0, music=1, fx=0) does (B1,
# measuring effects and cries without the music; the PC's ORRE_MUSIC_MUTE=1 does the same). Layout from B (synth.c,
# verified there): 32 MusyX master faders of 0x30 bytes at 0x80434E64 (f32 volume +0, target +4, start +8, time +0xC);
# a voice's fader factor is fader[group].pauseVol * fader[group].volume * fader[fx ? 22 : 21].volume, so fader 21 is
# the master of all music and 22 of all effects. sndMasterVolume(0, 0, 1, 0) sets fader 21 alone; MusyX re-applies
# it to the playing voices by itself (it compares against lastVolFaderScale every update).
# *Corrected 2026-10-18:* the first version zeroed every type-0 fader and set the fading mask; that muted only some
# music groups (3-6 dB in battle, none on the field; verified) and is gone.
MUSIC_MUTE = os.environ.get("SLICE_MUSIC_MUTE") == "1"
FADER_MUSIC, FADER_FX = 0x80434E64 + 21 * 0x30, 0x80434E64 + 22 * 0x30
F32_ZERO = 0x00000000


def mute_music():
    # *Corrected 2026-10-18 (second time):* without the fading-mask bit the playing music voices never picked up the new
    # factor (the field stayed at full level; verified against the PC's mute). sndMasterVolume's own path sets fader
    # 21's volume / target, deltaTime 1.0, and ORs (1 << 21) into the mask at 0x8047AF40 (B, synth.c, likely).
    for off in (0x00, 0x04, 0x08, 0x0C):
        memory.write_u32(FADER_MUSIC + off, F32_ZERO)
    memory.write_u32(FADER_MUSIC + 0x10, 0x3F800000)
    memory.write_u32(0x8047AF40, memory.read_u32(0x8047AF40) | 0x00200000)
    if frame in (300, 22200):   # read back: fader 21 should be 0.0, fader 22 (effects) untouched
        log(ev="faders", frame=frame, music=hex(memory.read_u32(FADER_MUSIC)), fx=hex(memory.read_u32(FADER_FX)))


# SLICE_PORTRAIT_RANGE=lo-hi: log the battle portrait slots per frame (the project's M38: how long the console's send-out
# waits for each lead's portrait, fn_8010AE2C's "state == 2" loop). Layout from Main (verified there): *(u32*)
# 0x8047AD4C = the slot array, *(s32*)0x8047AD48 = the slot count; 16-byte slots: +0 data pointer, +4 u16 key, +6 u8
# state (2 = ready), +7 s8 index. A line per slot whenever its (key, state, data) changes, to portraits.jsonl.
_pr = os.environ.get("SLICE_PORTRAIT_RANGE")
PORTRAIT_RANGE = tuple(int(v) for v in _pr.split("-")) if _pr else None
portrait_f = open(os.path.join(OUT, "portraits.jsonl"), "w", buffering=1) if PORTRAIT_RANGE else None
portrait_last = {}


def log_portraits():
    base, n = memory.read_u32(0x8047AD4C), memory.read_u32(0x8047AD48)
    if not _ram(base) or n > 64:
        return
    for i in range(n):
        a = base + 16 * i
        rec = (memory.read_u16(a + 4), memory.read_u8(a + 6), memory.read_u32(a))
        if rec != portrait_last.get(i):
            portrait_last[i] = rec
            if rec[0] or i in portrait_last:
                portrait_f.write(json.dumps({"frame": frame, "slot": i, "key": rec[0], "state": rec[1],
                                             "data": hex(rec[2])}) + "\n")


# SLICE_DVD_RANGE=lo-hi: the disc reads, polled every frame (console disc timing, measured behaviour only).
# The SDK's DVD driver (dvd.h / DVD.c / DVDQueue.c): executing_8047A7E8 = the DVDCommandBlock being executed
# (DummyCommandBlock 0x803FC3A0 when idle); WaitingQueue 0x803FC3F8 = 4 priority lists of {next, prev} (empty = points
# to itself). Block: +0x08 command, +0x0C state, +0x10 disc offset, +0x14 length, +0x18 destination, +0x20 bytes
# transferred. Per read (block address + offset + length) dvd.jsonl gets "queued" (first frame seen waiting), "start"
# (first frame executing), "end" (the first frame it's no longer executing, with the last bytes transferred). Frame
# resolution only: reads shorter than a frame that start and end between two polls are missed (the log says how many
# reads were seen; compare with the request count from a code-side log if that matters).
_dr = os.environ.get("SLICE_DVD_RANGE")
DVD_RANGE = tuple(int(v) for v in _dr.split("-")) if _dr else None
dvd_f = open(os.path.join(OUT, "dvd.jsonl"), "w", buffering=1) if DVD_RANGE else None
DVD_EXEC, DVD_DUMMY, DVD_QUEUE = 0x8047A7E8, 0x803FC3A0, 0x803FC3F8
dvd_seen = {"queued": set(), "exec": None}


def _dvd_block(b):
    return {"block": hex(b), "command": memory.read_u32(b + 0x08), "offset": hex(memory.read_u32(b + 0x10)),
            "length": memory.read_u32(b + 0x14), "addr": hex(memory.read_u32(b + 0x18))}


def _dvd_key(b):
    return (b, memory.read_u32(b + 0x10), memory.read_u32(b + 0x14))


def log_dvd():
    for prio in range(4):
        head = DVD_QUEUE + 8 * prio
        node, guard = memory.read_u32(head), 0
        while _ram(node) and node != head and guard < 64:
            key = _dvd_key(node)
            if key not in dvd_seen["queued"]:
                dvd_seen["queued"].add(key)
                dvd_f.write(json.dumps(dict(_dvd_block(node), ev="queued", frame=frame, prio=prio)) + "\n")
            node, guard = memory.read_u32(node), guard + 1
    ex = memory.read_u32(DVD_EXEC)
    cur = _dvd_key(ex) if _ram(ex) and ex != DVD_DUMMY else None
    last = dvd_seen["exec"]
    if cur != last:
        if last is not None:
            dvd_f.write(json.dumps({"ev": "end", "frame": frame, "block": hex(last[0]), "offset": hex(last[1]),
                                    "length": last[2]}) + "\n")
        if cur is not None:
            dvd_f.write(json.dumps(dict(_dvd_block(ex), ev="start", frame=frame,
                                        state=memory.read_u32(ex + 0x0C))) + "\n")
        dvd_seen["exec"] = cur


# SLICE_FADER_DIAG=lo-hi[,lo-hi...]: every frame in those windows, BEFORE the mute's write (B1 diagnosis), to
# faderdiag.jsonl: fader 21's five f32s (+0..+0x10, raw hex), seqId u32 +0x28, seqMode u8 +0x2C, type u8 +0x2D; the
# fading mask (u32 0x8047AF40); fader 22's volume; and up to 4 playing voices (state != 0): volL / volR, the envelope
# level, pb_mix vL / vR.
_fd = os.environ.get("SLICE_FADER_DIAG")
FADER_DIAG = [tuple(int(v) for v in w.split("-")) for w in _fd.split(",")] if _fd else None
fdiag_f = open(os.path.join(OUT, "faderdiag.jsonl"), "w", buffering=1) if FADER_DIAG else None


def log_fader_diag():
    f21 = 0x80434E64 + 21 * 0x30
    rec = {"frame": frame, "f21": [hex(memory.read_u32(f21 + o)) for o in (0, 4, 8, 0xC, 0x10)],
           "seqId": hex(memory.read_u32(f21 + 0x28)), "seqMode": memory.read_u8(f21 + 0x2C),
           "type": memory.read_u8(f21 + 0x2D), "mask": hex(memory.read_u32(0x8047AF40)),
           "f22": hex(memory.read_u32(0x80434E64 + 22 * 0x30))}
    base, count = memory.read_u32(0x8047B024), memory.read_u8(0x8047B05D)
    voices = []
    if _ram(base) and 0 < count <= 128:
        for v in range(count):
            a = base + v * 0xF4
            if memory.read_u8(a + 0xEC) == 0:
                continue
            pb = memory.read_u32(a)
            voices.append({"v": v, "smp": memory.read_u16(a + 0x70), "volL": memory.read_u16(a + 0x4C),
                           "volR": memory.read_u16(a + 0x4E), "adsr": memory.read_u32(a + 0xAC),
                           "pb": [memory.read_u16(pb + 0x12), memory.read_u16(pb + 0x16)] if _ram(pb) else None})
            if len(voices) == 4:
                break
    rec["voices"] = voices
    fdiag_f.write(json.dumps(rec) + "\n")


def run_command(cmd):
    parts = cmd.split()
    if parts and parts[0] == "seed" and len(parts) > 1:
        p = memory.read_u32(0x80478C94)
        if 0x80000000 <= p < 0x81800000:
            memory.write_u32(p, int(parts[1], 0) & 0xFFFFFFFF)
            return "ok"
        return "refused (no RNG yet)"
    return "skipped (no debug toolkit in Dolphin)"


def on_frame():
    global frame
    frame += 1
    n_entries = len(idlog.entered)
    idlog.on_frame(frame)
    battle_frame()
    if len(idlog.entered) > n_entries:
        room, f = idlog.entered[-1]
        pending_shots.append(f"{frame:06d}_room_{room:x}")
    while st["next"] < len(steps):
        step = steps[st["next"]]
        t = due(step)
        if t is None or frame < t:
            break
        if "command" in step:
            log(ev="command", frame=frame, step=step["text"], result=run_command(step["command"]))
        else:
            st["pad"] = step["pad"]
            log(ev="step", frame=frame, step=step["text"])
        st["prev_fired"] = frame
        st["next"] += 1
    if st["pad"]:
        controller.set_gc_buttons(0, st["pad"])
    if FADER_DIAG and any(lo <= frame <= hi for lo, hi in FADER_DIAG):
        log_fader_diag()      # before the mute's write, so it shows what the game left there
    if MUSIC_MUTE:
        mute_music()
    if PORTRAIT_RANGE and PORTRAIT_RANGE[0] <= frame <= PORTRAIT_RANGE[1]:
        log_portraits()
    if DVD_RANGE and DVD_RANGE[0] <= frame <= DVD_RANGE[1]:
        log_dvd()
    if VOICE_RANGE and VOICE_RANGE[0] <= frame <= VOICE_RANGE[1]:
        log_voices()
    if RNG_RANGE and RNG_RANGE[0] <= frame <= RNG_RANGE[1]:
        s = _rng()
        log(ev="rng", frame=frame, state=s, steps=_steps(st.get("rng_prev"), s))
        st["rng_prev"] = s
    if SHOT_RANGE:
        lo, hi, step = SHOT_RANGE
        if frame == lo - 1 and not st.get("drawn_on"):
            st["drawn_on"] = True
            event.on_framedrawn(on_drawn)          # only for the window: a drawn-frame callback slows emulation
        if lo <= frame <= hi and (frame - lo) % step == 0:
            pending_shots.append(f"{frame:06d}")
        if frame == hi + 2:
            try:
                event.on_framedrawn(None)
            except Exception as e:
                log(ev="shots_off_error", err=repr(e))
    if frame % 30 == 0:
        p = player_pos()
        if p:
            # with the stick the game read (InputPad slot 0's latched PADStatus, as dolphin_record.py records it), to
            # check the fork's StickX / StickY reach the game unchanged
            sx = memory.read_u8(0x80401C2A)
            sy = memory.read_u8(0x80401C2B)
            log(ev="pos", frame=frame, pos=[round(v, 2) for v in p],
                stick=[(sx - 256 if sx > 127 else sx) + 128, (sy - 256 if sy > 127 else sy) + 128])
    if STOP_FRAME and frame >= STOP_FRAME:      # SLICE_STOP_FRAME=N: end the run at frame N (a partial replay)
        log(ev="end", frame=frame, entries=len(idlog.entered), stopped=True)
        log_f.flush()
        time.sleep(1)
        os._exit(0)
    if st["next"] >= len(steps):
        if st["done_frame"] is None:
            st["done_frame"] = frame
            log(ev="last_step", frame=frame)
        elif frame - st["done_frame"] >= END:
            if SAVE and not st.get("saved"):
                # SLICE_SAVE=<name>: save a state where the pad ends, as the start of the next slice part's recording
                # (Dolphin writes states asynchronously: wait before exiting)
                savestate.save_to_file(os.path.join(OUT, SAVE))
                st["saved"] = frame
                log(ev="saved", frame=frame, state=SAVE)
                return
            if st.get("saved") and frame - st["saved"] < 180:
                return
            log(ev="end", frame=frame, entries=len(idlog.entered))
            log_f.flush()
            time.sleep(3 if st.get("saved") else 1)
            os._exit(0)


def write_png(path, w, h, rgb):
    import struct
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as g:
        g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def on_drawn(w, h, data):
    while pending_shots:
        try:
            write_png(os.path.join(SHOTS, pending_shots.pop() + ".png"), w, h, data)
        except Exception as e:
            log(ev="shot_error", err=repr(e))


def on_code(addr):
    """SLICE_BREAKS=1: log each code breakpoint hit (the addresses in the fork's User/GameSettings/GC6E01.ini) with the
    frame and the RNG state, e.g. where a burst of draws starts relative to that frame's state. The fork reports only
    a breakpoint's first hit (and some hits twice); registers are only right with -C Dolphin.Core.CPUCore=5."""
    key = (addr, frame)
    if key in st.setdefault("breaks", set()):
        return
    st["breaks"].add(key)
    log(ev="break", addr=hex(addr), frame=frame, rng=_rng())


event.on_frameadvance(on_frame)
if os.environ.get("SLICE_BREAKS") == "1":
    event.on_codebreakpoint(on_code)
# Screenshots only on request (SLICE_SHOTS=1): with a frame-drawn callback registered, the fork hands every drawn frame
# to Python, which slowed a slice run to about 3-4 frames per second (verified 2026-10-17; 20 minutes for ~4,500 frames).
if os.environ.get("SLICE_SHOTS") == "1":
    event.on_framedrawn(on_drawn)
