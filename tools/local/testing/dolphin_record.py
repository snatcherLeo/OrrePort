"""Record a hand-played stretch of the golden slice in Dolphin, as a pad log walk_to_slice.py turns into a pad script
(testing lane).

Runs inside Felk's Dolphin fork, with the player at the controls (the fork's keyboard mapping: arrows = main stick,
X = A, Z = B, Enter = START; the Dolphin window needs focus):

    set POS_OUT=<folder holding the start state>   set REC_STATE=s1_out_arrival.sav   set TESTING_DIR=<this folder>
    set REC_STOP_ROOM=0xC8   (stop this many frames after that room is entered: REC_STOP_AFTER, default 120)
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_record.py

Frame 1 loads the start state (the one dolphin_slice.py saved with SLICE_SAVE); from frame 2 on, the pad the player
holds is read every frame and written on every change to walk_pad.jsonl (frame, room, since_room, pad: the same lines
dolphin_walk.py writes, so walk_to_slice.py reads both). Nothing is ever written to the pad: the player's input goes
to the game untouched. idlog (run="walk") logs the rooms entered with the RNG state and the story flags' CRC; walk.jsonl
gets Wes's position every 30 frames (to find where a replay leaves the recorded path). At the stop, a state is saved
(rec_end.sav) and Dolphin exits.
"""
import json, os, sys, time
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", ".")
STATE = os.environ.get("REC_STATE", "s1_out_arrival.sav")
STOP_ROOM = int(os.environ.get("REC_STOP_ROOM", "0xC8"), 0)
STOP_AFTER = int(os.environ.get("REC_STOP_AFTER", "120"))
sys.path.insert(0, os.environ.get("TESTING_DIR", ""))
import idlog

walk_log = open(os.path.join(OUT, "walk.jsonl"), "w", buffering=1)
pad_log = open(os.path.join(OUT, "walk_pad.jsonl"), "w", buffering=1)
ROOM = 0x80478B18
PEOPLE_PTR, PEOPLE_MAX, ENTRY = 0x8047B200, 0x8047B1F8, 0xDC
KEEP = ("A", "B", "X", "Y", "Z", "L", "R", "Start", "Up", "Down", "Left", "Right")
frame = 0
st = {"loaded": False, "last": None, "room_first": {}, "stop_at": None}


def log(**kw):
    walk_log.write(json.dumps(kw) + "\n")


def _ram(a):
    return 0x80000000 <= a < 0x81800000


def player_pos():
    base, n = memory.read_u32(PEOPLE_PTR), memory.read_u32(PEOPLE_MAX)
    if not _ram(base) or n > 512:
        return None
    for i in range(n):
        e = base + i * ENTRY
        if memory.read_u8(e) and memory.read_u32(e + 0x28) == 0 and memory.read_u32(e + 0x2C) == 100:
            m = memory.read_u32(e + 0x08)
            if _ram(m):
                return [round(memory.read_f32(m + o), 2) for o in (0x18, 0x1C, 0x20)]
    return None


# The game's own copy of controller 1 as PADRead returned it: InputPad slot 0 (lbl_80401C10, input.h) +0x18 "latched"
# PADStatus (+0 buttons, +2 stick X, +3 stick Y, signed, 0 = centre). The fork's get_gc_buttons reported the stick as
# 0 / 0 all through a hand-played walk while Wes moved (verified 2026-10-17), so the stick is read here instead.
LATCHED = 0x80401C10 + 0x18


def _s8(v):
    return v - 256 if v > 127 else v


def read_pad():
    """The held pad as dolphin_walk.py logs it: pressed buttons, and both stick axes (0-255, 128 = centre, the raw
    controller convention: SY > 128 is up) when the stick is off centre.
    *Corrected 2026-10-17:* the game flips Y when it stores the reading (input.c: `s->stickY = -s->stickY` before the
    copy into `latched`), so Y is flipped back here; recordings made before this fix (rec_2) have Y reversed and need
    walk_to_slice.py --latched-y."""
    raw = controller.get_gc_buttons(0)
    pad = {k: True for k in KEEP if raw.get(k)}
    sx = _s8(memory.read_u8(LATCHED + 2)) + 128
    sy = (256 - (_s8(memory.read_u8(LATCHED + 3)) + 128)) & 255
    if sx != 128 or sy != 128:
        pad["StickX"], pad["StickY"] = sx, sy
    return pad


def on_frame():
    global frame
    frame += 1
    if not st["loaded"]:
        st["loaded"] = True
        try:
            savestate.load_from_file(os.path.join(OUT, STATE))
            log(ev="loaded", state=STATE)
        except Exception as e:
            log(ev="load_error", err=repr(e))
        return
    room = memory.read_u32(ROOM)
    st["room_first"].setdefault(room, frame)
    try:
        pad = read_pad()
    except Exception as e:
        if not st.get("pad_error"):
            st["pad_error"] = True
            log(ev="pad_error", frame=frame, err=repr(e))
        pad = {}
    key = json.dumps(pad, sort_keys=True)
    if key != st["last"]:
        st["last"] = key
        pad_log.write(json.dumps({"frame": frame, "room": room, "since_room": frame - st["room_first"][room],
                                  "pad": pad}) + "\n")
    if frame % 30 == 0:
        log(ev="pos", frame=frame, room=room, pos=player_pos())
    if st["stop_at"] is None:
        for r, f in idlog.entered:
            if r == STOP_ROOM:
                st["stop_at"] = f + STOP_AFTER
                log(ev="stop_room", frame=f, room=r)
    elif frame == st["stop_at"]:
        savestate.save_to_file(os.path.join(OUT, "rec_end.sav"))
        log(ev="saved", frame=frame, state="rec_end.sav")
    elif frame >= st["stop_at"] + 180:      # states are written asynchronously: wait before exiting
        log(ev="end", frame=frame)
        walk_log.flush()
        pad_log.flush()
        time.sleep(2)
        os._exit(0)


idlog.install(event, on_frame, OUT, run="walk")
