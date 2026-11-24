"""Scripted walk, part 2: Outskirt Stand after Willie -> world map -> Phenac City (route steps 7-8) (testing lane).

Runs inside Felk's Dolphin fork, like dolphin_walk.py (same position readout and stick learning):

    set POS_OUT=<folder holding after_willie_talk.sav>
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_phenac.py

Phases (keyed to the room id, lbl_80478B18):
- room 1 (after Willie's talk): B closes his last line (it ignores A; verified 2026-10-16), then Wes walks south across
  the town's edge trigger (collision event 0, z = 96.2) -> the world map (room 910, 0x38E);
- world map: LEFT toward Phenac City (west of Outskirt Stand; the cursor starts on the current place, docs/engine/menus.md),
  then A (and A again for a confirmation, if any); the bike ride plays;
- Phenac City (room 3): A every 40 frames through the arrival and Folly's scenes; battles (any other room after it)
  get A presses too.
States: worldmap_open, m1_arrival, folly_start, after_folly (+ .png). Screenshots every 60 frames (walk2_shots/) and one JSON
line per event (walk2.jsonl); the pad is logged as in dolphin_walk.py (walk2_pad.jsonl) for a later PC replay.
"""
import json, math, os, struct, time, zlib
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", ".")
SHOTS = os.path.join(OUT, "walk2_shots"); os.makedirs(SHOTS, exist_ok=True)
log_file = open(os.path.join(OUT, "walk2.jsonl"), "w", buffering=1)
pad_log = open(os.path.join(OUT, "walk2_pad.jsonl"), "w", buffering=1)
def log(**kw): log_file.write(json.dumps(kw) + "\n")

ROOM = 0x80478B18
PEOPLE_PTR, PEOPLE_MAX, ENTRY = 0x8047B200, 0x8047B1F8, 0xDC
WORLD_MAP, PHENAC = 910, 3
frame = 0
st = {"loaded": False, "room": None, "first": {}, "visits": {}, "calib": [], "M": None, "phase": "close_talk",
      "last": None, "battle_room": None, "last_pad": None}

def u32(a): return memory.read_u32(a)

def player_pos():
    try:
        base, n = u32(PEOPLE_PTR), u32(PEOPLE_MAX)
        if not (0x80000000 <= base < 0x81800000) or n > 512: return None
        for i in range(n):
            e = base + i * ENTRY
            if memory.read_u8(e) and u32(e + 0x28) == 0 and u32(e + 0x2C) == 100:
                m = u32(e + 0x08)
                if 0x80000000 <= m < 0x81800000:
                    return (memory.read_f32(m + 0x18), memory.read_f32(m + 0x1C), memory.read_f32(m + 0x20))
    except Exception:
        return None
    return None

def stick(sx, sy):
    return {"StickX": int(128 + 127 * max(-1, min(1, sx))), "StickY": int(128 + 127 * max(-1, min(1, sy)))}

pending = []
def save(name):
    try:
        savestate.save_to_file(os.path.join(OUT, name + ".sav")); log(ev="state", spot=name, frame=frame)
    except Exception as e:
        log(ev="state_error", spot=name, err=repr(e))
    pending.append(name); st["visits"][name] = frame

def steer(x, z, tx, tz):
    a, b, c, d = st["M"]; det = a * d - b * c; dx, dz = tx - x, tz - z
    if abs(det) < 1e-6: return {}
    sx, sy = (d * dx - b * dz) / det, (-c * dx + a * dz) / det
    n = math.hypot(sx, sy) or 1
    return stick(sx / n, sy / n)

def on_frame():
    global frame
    frame += 1
    if not st["loaded"]:
        st["loaded"] = True
        savestate.load_from_file(os.path.join(OUT, "after_willie_talk.sav")); log(ev="loaded"); return
    room = u32(ROOM)
    st["first"].setdefault(room, frame)
    if room != st["room"]:
        log(ev="room", frame=frame, room=room, pos=player_pos()); st["room"] = room
        if room not in (1, WORLD_MAP, PHENAC, 0xFFFFFFFF) and "m1_arrival" in st["visits"] and st["battle_room"] is None:
            st["battle_room"] = room
    rel = frame - st["first"][room]
    if frame % 60 == 0:
        pending.append("f%06d" % frame); log(ev="pos", frame=frame, room=room, pos=player_pos(), phase=st["phase"])
    b = {}
    pos = player_pos()
    if room == 0xFFFFFFFF and st["battle_room"] is not None:
        # after a battle the room id reads "none" (as after Willie's): the trainer's lines; A advances them, B every
        # fourth press in case a line ignores A (Willie's last one does)
        brel = frame - st["visits"].setdefault("battle_over", frame)
        if brel % 40 < 3: b["B" if (brel // 40) % 4 == 3 else "A"] = True
        if brel == 2400 and "after_folly" not in st["visits"]: save("after_folly")
    elif room in (1, 0xFFFFFFFF) and "worldmap_open" not in st["visits"]:
        if st["phase"] == "close_talk":
            if frame < 900:
                if frame % 40 < 3: b["B"] = True          # Willie's last line closes with B
            else:
                st["phase"] = "calib_up"; st["calib"] = []
        elif st["phase"] in ("calib_up", "calib_right") and pos:
            c = st["calib"]; c.append((pos[0], pos[2]))
            if len(c) <= 30:
                b.update(stick(0, 1) if st["phase"] == "calib_up" else stick(1, 0))
            else:
                dx, dz = c[-1][0] - c[5][0], c[-1][1] - c[5][1]; c.clear()
                if math.hypot(dx, dz) < 0.5:
                    b["B"] = (frame % 40 < 3)             # still in a text box
                else:
                    st[st["phase"]] = (dx, dz)
                    if st["phase"] == "calib_up":
                        st["phase"] = "calib_right"
                    else:
                        (ux, uz), (rx, rz) = st["calib_up"], st["calib_right"]
                        st["M"] = (rx, ux, rz, uz); st["phase"] = "walk_south"
                        log(ev="calibrated", room=room, up=(ux, uz), right=(rx, rz))
        elif st["phase"] == "walk_south" and pos:
            b.update(steer(pos[0], pos[2], pos[0], 120.0))   # straight to +z, across the edge at z = 96.2
    elif room == WORLD_MAP:
        if rel == 240 and "worldmap_open" not in st["visits"]:
            save("worldmap_open")
        if 300 <= rel < 304: b["Left"] = True
        if 420 <= rel < 424: b["A"] = True
        if 600 <= rel < 604: b["A"] = True                # a confirmation, if the map asks
    elif room == PHENAC:
        if rel == 30 and "m1_arrival" not in st["visits"]:
            save("m1_arrival")
        if rel % 40 < 3: b["A"] = True
    elif st["battle_room"] is not None and room == st["battle_room"]:
        if rel == 1500 and "folly_start" not in st["visits"]: save("folly_start")
        if rel % 30 < 3: b["A"] = True
    key = json.dumps(b, sort_keys=True)
    if key != st["last_pad"]:
        st["last_pad"] = key
        pad_log.write(json.dumps({"frame": frame, "room": room, "pad": b}) + chr(10))
    if b:
        controller.set_gc_buttons(0, b)
    if "after_folly" in st["visits"] and not pending and frame > st["visits"]["after_folly"] + 60:
        log(ev="exit", frame=frame); log_file.close(); os._exit(0)
    if frame > 60 * 60 * 9:
        log(ev="timeout", frame=frame, visits=st["visits"]); log_file.close(); os._exit(1)
try:  # runtime id log (idlog.py): battles and rooms reached; set TESTING_DIR to this folder (the fork's import path)
    import sys
    sys.path.insert(0, os.environ.get("TESTING_DIR", ""))
    import idlog
except Exception as e:
    open(os.path.join(OUT, "idlog_error.txt"), "w").write(repr(e) + "\n" + repr(sys.path))
    event.on_frameadvance(on_frame)
else:
    idlog.install(event, on_frame, OUT, run="phenac")

def on_drawn(w, h, data):
    while pending:
        name = pending.pop()
        raw = b"".join(b"\x00" + data[y * w * 3:(y + 1) * w * 3] for y in range(h))
        ch = lambda t, d: struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
        path = os.path.join(SHOTS if name.startswith("f") else OUT, name + ".png")
        open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                               + ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))
event.on_framedrawn(on_drawn)
