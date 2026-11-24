"""Dolphin positions for the visual-check sessions (testing lane; docs/testing/visual-session.md).

Runs **inside** Felk's Dolphin scripting fork (a separate GPL program, never linked into OrrePort), like
`tools/local/port/dolphin_felk_check.py` (its setup: portable mode, single-core CPU, the pinned clock 0x386D4380):

    set POS_OUT=<session folder outside the repo>
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_positions.py

From power-on it waits for the title screen, then presses the input cycle of the hands-on runs into room 1, and at
each named spot saves a Dolphin state file and a screenshot into POS_OUT: `<spot>.sav` + `<spot>.png`. Spots are
either a frame number or "N frames after the room id first becomes R" (room id = lbl_80478B18, written by
floorRequestChange; verified by the hands-on runs in docs/testing-plan.md). One JSON line per event in POS_OUT/run.jsonl.
Our code; the fork's API names (savestate.save_to_file, event.on_frameadvance / on_framedrawn,
controller.set_gc_buttons, memory.read_u32) as in docs/testing-plan.md "Felk fork: interface check".
"""
import json, os, struct, time, zlib
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", ".")
os.makedirs(os.path.join(OUT, "shots"), exist_ok=True)
log_file = open(os.path.join(OUT, "run.jsonl"), "w", buffering=1)
def log(**kw):
    log_file.write(json.dumps(kw) + "\n")

ROOM = 0x80478B18

# Inputs, keyed to the room id (frames drift between runs, rooms don't):
# - none until the title screen is up (title_idle);
# - title (room 929 not yet seen): START every 240 frames, A every 240 (card messages);
# - top menu (room 929 = 0x3A1): a fixed list relative to its first frame: A on NEW GAME (the cursor starts there; verified by screenshot, 2026-10-16; Dolphin's portable card
#   holds a save, so CONTINUE comes first), A, then the PC route's yes / no (starts on NO: Up, A) and name entry
#   (Down to WES, A, A on "Is WES OK?");
# - after that, until room 1: A every 60 frames and Start every 480 (dialogue, the opening), never Down / Up (the
#   2026-10-16 run's Down / Up cycle wandered into OPTIONS and stayed there).
CYCLE_FROM = 2500
TOP_MENU = 929
TOP_MENU_PRESSES = {300: "A", 800: "Up", 900: "A", 1400: "Down", 1550: "A", 1900: "A"}
# Spots: (name, kind, value, delay). kind "frame": at that frame; kind "room": `delay` frames after the room id first
# becomes `value`. Screenshots every 600 frames as well (shots/), to pick more spots later.
SPOTS = [
    ("title_idle", "frame", 2400, 0),
    ("s1_out_arrival", "room", 1, 900),
]

frame = 0
room_seen = {}
done = set()
t0 = time.time()
log(ev="start", savestate_api=[n for n in dir(savestate) if not n.startswith("_")])

def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as g:
        g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

pending_shot = []
def save(name):
    path = os.path.join(OUT, name + ".sav")
    try:
        savestate.save_to_file(path)
        log(ev="state", spot=name, frame=frame, path=path, wall=round(time.time() - t0, 1))
    except Exception as e:
        log(ev="state_error", spot=name, frame=frame, err=repr(e))
    pending_shot.append(name)
    done.add(name)

def on_frame():
    global frame
    frame += 1
    try:
        room = memory.read_u32(ROOM)
    except Exception:
        room = None
    if room is not None and room not in room_seen:
        room_seen[room] = frame
        log(ev="room", frame=frame, room=room)
    for name, kind, value, delay in SPOTS:
        if name in done:
            continue
        if kind == "frame" and frame == value:
            save(name)
        elif kind == "room" and value in room_seen and frame == room_seen[value] + delay:
            save(name)
    b = {}
    if frame >= CYCLE_FROM and 1 not in room_seen:
        if TOP_MENU not in room_seen:
            if frame % 240 == 0:
                b["Start"] = True
            elif frame % 240 == 120:
                b["A"] = True
        else:
            rel = frame - room_seen[TOP_MENU]
            for at, button in TOP_MENU_PRESSES.items():
                if at <= rel < at + 4:
                    b[button] = True
            if rel > 2300:
                if rel % 60 < 3:
                    b["A"] = True
                if rel % 480 == 460:
                    b["Start"] = True
    if frame % 600 == 0 or (TOP_MENU in room_seen and 1 not in room_seen and frame % 150 == 0):
        pending_shot.append("shots/f%06d" % frame)
    if b:
        controller.set_gc_buttons(0, b)
    if len(done) == len(SPOTS) and not pending_shot:
        if "finished" not in room_seen:
            room_seen["finished"] = frame
            log(ev="all_spots_done", frame=frame)
        elif frame > room_seen["finished"] + 120:
            log(ev="exit", frame=frame)
            log_file.close()
            os._exit(0)   # the states are written; end Dolphin so the heavy-run lock is freed
    elif frame > 60 * 60 * 8:
        log(ev="timeout", frame=frame, missing=[n for n, *_ in SPOTS if n not in done])
        log_file.close()
        os._exit(1)
try:  # runtime id log (idlog.py): battles and rooms reached; set TESTING_DIR to this folder (the fork's import path)
    import sys
    sys.path.insert(0, os.environ.get("TESTING_DIR", ""))
    import idlog
except Exception as e:
    open(os.path.join(OUT, "idlog_error.txt"), "w").write(repr(e) + "\n" + repr(sys.path))
    event.on_frameadvance(on_frame)
else:
    idlog.install(event, on_frame, OUT, run="positions")

def on_drawn(w, h, data):
    while pending_shot:
        name = pending_shot.pop()
        try:
            write_png(os.path.join(OUT, name + ".png"), w, h, data)
            log(ev="shot", spot=name, frame=frame, w=w, h=h)
        except Exception as e:
            log(ev="shot_error", spot=name, err=repr(e))
event.on_framedrawn(on_drawn)
