"""Scripted walk for the visual-check sessions: Outskirt Stand -> shop -> back out -> Willie's battle (testing lane).

Runs **inside** Felk's Dolphin scripting fork (separate GPL program, never linked into OrrePort), like
dolphin_positions.py:

    set POS_OUT=<session folder outside the repo>   (must hold s1_out_arrival.sav from dolphin_positions.py)
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_walk.py

Loads s1_out_arrival.sav, then steers Wes with the control stick, closed loop: his position is read every frame from
the people table (pointer lbl_8047B200, 0xDC-byte entries: +0x00 active, +0x08 model, +0x28 group, +0x2C index; the
player is group 0, index 100; the model's position is GSmodel +0x18, three floats) (people.h, gs_model_struct.h).
The stick-to-world map is learned per room by pushing up, then right, and measuring the motion. Waypoints come from
the room data (docs/engine/rooms.md): Outskirt Stand's ramp and shop door (collision event 4, z = -38.6), the shop's
exit plane (z = 30.4). Whenever Wes doesn't move (a scene or a dialogue), A is pressed every 40 frames.
Saves willie_start / willie_end / after_willie_talk / after_willie_menu states + screenshots; one JSON line per event in run.jsonl.
"""
import json, math, os, struct, time, zlib
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", ".")
SHOTS = os.path.join(OUT, "walk_shots")
os.makedirs(SHOTS, exist_ok=True)
log_file = open(os.path.join(OUT, "walk.jsonl"), "w", buffering=1)
pad_log = open(os.path.join(OUT, "walk_pad.jsonl"), "w", buffering=1)
def log(**kw):
    log_file.write(json.dumps(kw) + "\n")

ROOM = 0x80478B18
PEOPLE_PTR, PEOPLE_MAX, ENTRY = 0x8047B200, 0x8047B1F8, 0xDC
# per room: waypoints (x, z) in order; reaching one = within 4 units
ROUTE = {
    1: [(-15.0, 14.0), (-15.0, -46.0)],      # Outskirt Stand: ramp foot, then up through the shop door
    2: [(-53.0, 42.0)],                      # shop: back out through the exit plane
}

frame = 0
state = {"loaded": False, "room": None, "phase": "calib_up", "calib": [], "M": None, "wp": 0, "last": None,
         "still": 0, "visits": {}, "battle_room": None, "battle_from": None}
t0 = time.time()

def u32(a): return memory.read_u32(a)
def f32(a): return memory.read_f32(a)

def player_pos():
    try:
        base, n = u32(PEOPLE_PTR), u32(PEOPLE_MAX)
        if not (0x80000000 <= base < 0x81800000) or n > 512:
            return None
        for i in range(n):
            e = base + i * ENTRY
            if memory.read_u8(e) and u32(e + 0x28) == 0 and u32(e + 0x2C) == 100:
                m = u32(e + 0x08)
                if 0x80000000 <= m < 0x81800000:
                    return (f32(m + 0x18), f32(m + 0x1C), f32(m + 0x20))
    except Exception:
        return None
    return None

def stick(sx, sy):
    """sx, sy in -1..1 -> GameCube stick bytes (128 = centre)."""
    return {"StickX": int(128 + 127 * max(-1, min(1, sx))), "StickY": int(128 + 127 * max(-1, min(1, sy)))}

def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as g:
        g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

pending = []
def save(name):
    try:
        savestate.save_to_file(os.path.join(OUT, name + ".sav"))
        log(ev="state", spot=name, frame=frame)
    except Exception as e:
        log(ev="state_error", spot=name, err=repr(e))
    pending.append(name)
    state["visits"][name] = frame

def on_frame():
    global frame
    frame += 1
    if not state["loaded"]:
        state["loaded"] = True
        try:
            savestate.load_from_file(os.path.join(OUT, "s1_out_arrival.sav"))
            log(ev="loaded")
        except Exception as e:
            log(ev="load_error", err=repr(e))
        return
    room = u32(ROOM)
    state.setdefault("room_first", {}).setdefault(room, frame)
    if room != state["room"]:
        log(ev="room", frame=frame, room=room, pos=player_pos())
        state["room"] = room
        if room in ROUTE:
            state.update(phase="calib_up", calib=[], M=None, wp=0, still=0)
        elif state["battle_room"] is None and "left_shop" in state["visits"]:
            state["battle_room"], state["battle_from"] = room, frame
    if frame % 120 == 0:
        pending.append("f%06d" % frame)
        log(ev="pos", frame=frame, room=room, pos=player_pos(), phase=state["phase"])
    pos = player_pos()
    b = {}
    if room == 2 and "in_shop" not in state["visits"]:
        state["visits"]["in_shop"] = frame
    if room == 1 and "in_shop" in state["visits"] and "left_shop" not in state["visits"]:
        state["visits"]["left_shop"] = frame
        state["phase"] = "talk"                  # Willie's scene: no walking, A through the dialogue
    # battle: save states at fixed delays after the battle room loads (A selects FIGHT and the first move)
    if state["battle_room"] is not None and room == state["battle_room"]:
        rel = frame - state["battle_from"]
        dense = os.environ.get("WALK_DENSE")      # e.g. "200-1500:10": battle-relative frames to screenshot densely
        if dense:
            span, step = dense.split(":"); lo, hi = map(int, span.split("-"))
            if lo <= rel <= hi and rel % int(step) == 0:
                pending.append("f%06d_b%05d" % (frame, rel))
        if rel == 1500 and "willie_start" not in state["visits"]:
            save("willie_start")                 # both sides sent out, the first command menu
        if rel == 4050 and "willie_end" not in state["visits"]:
            save("willie_end")                   # just before the last hit (2026-10-16 run: the last Zigzagoon
                                                 # faints between rel 4128 and 4368, one hit)
        if rel % 30 < 3:
            b["A"] = True
    elif state["battle_room"] is not None and room != state["battle_room"]:
        # back in the field after the battle
        rel = frame - state["visits"].setdefault("after_battle", frame)
        if rel == 1200 and "after_willie_talk" not in state["visits"]:
            save("after_willie_talk")            # Willie's after-battle dialogue (name-prefixed lines)
        if rel < 1200 and rel % 40 < 3:
            b["A"] = True                        # through Willie's dialogue up to its last line
        if 1200 <= rel < 2000 and rel % 40 < 3:
            b["B"] = True                        # the last line ("PHENAC CITY's out west ...") closes with B, not A
                                                 # (verified by a probe from after_willie_talk, 2026-10-16)
        if 2200 <= rel < 2204:
            b["Start"] = True                    # the pause menu (POKeMON / P*DA / ITEMS / EXIT)
        if rel == 2500 and "after_willie_menu" not in state["visits"]:
            save("after_willie_menu")
        if rel > 2700 and not pending:
            log(ev="exit", frame=frame)
            log_file.close()
            os._exit(0)
    elif room in ROUTE and state["phase"] != "talk" and pos is not None:
        x, z = pos[0], pos[2]
        last = state["last"]
        moved = last is not None and math.hypot(x - last[0], z - last[1]) > 0.05
        state["last"] = (x, z)
        ph = state["phase"]
        if ph in ("calib_up", "calib_right"):
            c = state["calib"]
            c.append((x, z))
            if len(c) <= 30:
                b.update(stick(0, 1) if ph == "calib_up" else stick(1, 0))
            else:
                dx, dz = c[-1][0] - c[5][0], c[-1][1] - c[5][1]
                if math.hypot(dx, dz) < 0.5:
                    b["A"] = (frame % 40 < 3)    # not moving: a scene or text box is up
                    c.clear()
                else:
                    state[ph] = (dx, dz)
                    c.clear()
                    if ph == "calib_up":
                        state["phase"] = "calib_right"
                    else:
                        (ux, uz), (rx, rz) = state["calib_up"], state["calib_right"]
                        state["M"] = (rx, ux, rz, uz)       # world = M . (sx, sy)
                        state["phase"] = "walk"
                        log(ev="calibrated", room=room, up=(ux, uz), right=(rx, rz))
        elif ph == "walk":
            wps = ROUTE[room]
            if state["wp"] < len(wps):
                tx, tz = wps[state["wp"]]
                dx, dz = tx - x, tz - z
                if math.hypot(dx, dz) < 4:
                    state["wp"] += 1
                    log(ev="waypoint", frame=frame, room=room, n=state["wp"], pos=(x, z))
                else:
                    a, bb, c, d = state["M"]
                    det = a * d - bb * c
                    if abs(det) > 1e-6:
                        sx, sy = (d * dx - bb * dz) / det, (-c * dx + a * dz) / det
                        n = math.hypot(sx, sy) or 1
                        b.update(stick(sx / n, sy / n))
                if not moved:
                    state["still"] += 1
                    if state["still"] > 20 and state["still"] % 40 < 3:
                        b["A"] = True            # blocked by a text box
                else:
                    state["still"] = 0
            else:
                # past the last waypoint: keep pushing the same way until the room changes
                a, bb, c, d = state["M"]
                tx, tz = wps[-1]
                b.update(stick(0, 0))
    elif state["phase"] == "talk" or room not in ROUTE:
        if frame % 40 < 3:
            b["A"] = True
    # record the pad (whole state, every change) with the room and the frame since this room was first seen, so the
    # walk can be replayed on the PC as a room-keyed pad script (pad_to_script.py)
    key = json.dumps(b, sort_keys=True)
    if key != state.get("last_pad"):
        state["last_pad"] = key
        pad_log.write(json.dumps({"frame": frame, "room": room, "since_room": frame - state["room_first"].get(room, frame),
                                  "pad": b}) + chr(10))
    if b:
        try:
            controller.set_gc_buttons(0, b)
        except Exception as e:
            if not state.get("pad_error"):
                state["pad_error"] = True
                log(ev="pad_error", frame=frame, keys=sorted(b), err=repr(e))
    if frame > 60 * 60 * 10:
        log(ev="timeout", frame=frame, visits=state["visits"])
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
    idlog.install(event, on_frame, OUT, run="walk")

def on_drawn(w, h, data):
    while pending:
        name = pending.pop()
        folder = SHOTS if name.startswith("f") else OUT
        try:
            write_png(os.path.join(folder, name + ".png"), w, h, data)
        except Exception as e:
            log(ev="shot_error", spot=name, err=repr(e))
event.on_framedrawn(on_drawn)
