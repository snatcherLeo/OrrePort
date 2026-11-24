"""Walk to a point, face a target, press buttons (testing lane). Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>  GOTO_STATE=<state in it>  GOTO_TO=x,z  GOTO_FACE=x,z  GOTO_PRESS="A:40:1200,UP@300"
    set GOTO_SAVE=<name>@<frames after the presses start>   (optional: save a state then)
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_goto.py

Loads the state; learns the stick-to-world map (push up, then right; A presses while Wes can't move, for text boxes);
walks through the GOTO_TO points (one point or path_plan.py's list; closed loop on the player's position, people table; see dolphin_walk.py); turns toward GOTO_FACE with a
short stick push; then runs GOTO_PRESS: "BUTTON:every:for" repeats a button every N frames for M frames, "BUTTON@N"
presses once N frames after the presses start; "BUTTON:every:for@start" starts the repetition at frame `start`. Screenshots every 30 frames (goto_shots/), events in goto.jsonl.
"""
import json, math, os, struct, zlib
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", "."); STATE = os.environ["GOTO_STATE"]
# GOTO_TO: one point "x,z" or a list "x,z;x,z;..." (path_plan.py's output); FACE defaults to the last point
# or GOTO_PLAN="<collision obj>|<npc placements csv>|<room archive>|gx,gz[|bx,bz]": plan from Wes's real position once
# it is readable (path_plan.plan), then (optionally) a last point bx,bz beyond a door
PLAN = os.environ.get("GOTO_PLAN")
WAY = [tuple(map(float, p.split(","))) for p in (os.environ.get("GOTO_TO") or "0,0").split(";") if p.strip()]
TO = WAY[-1]; FACE = tuple(map(float, os.environ["GOTO_FACE"].split(","))) if os.environ.get("GOTO_FACE") else TO
PRESS = os.environ.get("GOTO_PRESS", "A:40:900")
SAVE = os.environ.get("GOTO_SAVE")
PUSH = int(os.environ.get("GOTO_PUSH", "6"))
ROOM_SAVE = os.environ.get("GOTO_ROOM_SAVE")   # "<name>@<frames>": save that long after the room id changes, then stop
ROOM_A = os.environ.get("GOTO_ROOM_A", "1") == "1"   # press A every 40 frames while waiting after the room change   # frames to keep pushing toward FACE after arriving (e.g. 120: through a door)
SHOTS = os.path.join(OUT, "goto_shots"); os.makedirs(SHOTS, exist_ok=True)
lf = open(os.path.join(OUT, "goto.jsonl"), "w", buffering=1)
def log(**kw): lf.write(json.dumps(kw) + "\n")
PEOPLE_PTR, PEOPLE_MAX, ENTRY, ROOM = 0x8047B200, 0x8047B1F8, 0xDC, 0x80478B18
frame = 0; st = {"phase": "load", "calib": [], "t0": None}; pending = []

def u32(a): return memory.read_u32(a)
def pos():
    try:
        base, n = u32(PEOPLE_PTR), u32(PEOPLE_MAX)
        for i in range(min(n, 512)):
            e = base + i * ENTRY
            if memory.read_u8(e) and u32(e + 0x28) == 0 and u32(e + 0x2C) == 100:
                m = u32(e + 0x08)
                return (memory.read_f32(m + 0x18), memory.read_f32(m + 0x20)) if 0x80000000 <= m < 0x81800000 else None
    except Exception:
        return None
CAM = {"ptr": None}
def camera_map():
    """Stick-to-world map from the live render camera (GSres group 0 id 0: eye +0x70, interest +0x100;
    dolphin_camera.py): stick up = away from the camera on the ground, stick right = that turned 90 degrees.
    Returns (rx, fx, rz, fz) so that world = (rx*sx + fx*sy, rz*sx + fz*sy), or None."""
    try:
        if CAM["ptr"] is None:
            tab, n = u32(0x8047AC5C), u32(0x8047AC60)
            for i in range(min(n, 4096)):
                e = tab + 0x14 * i
                if u32(e + 4) and u32(e + 8) == 0 and u32(e + 0xC) == 0:
                    CAM["ptr"] = u32(e + 4); break
        c = CAM["ptr"]
        if not c: return None
        ex, ez = memory.read_f32(c + 0x70), memory.read_f32(c + 0x78)
        ix, iz = memory.read_f32(c + 0x100), memory.read_f32(c + 0x108)
        fx, fz = ix - ex, iz - ez; n = math.hypot(fx, fz)
        if n < 1e-3: return None
        fx, fz = fx / n, fz / n
        # Measured in Dolphin (2026-10-16): StickY 255 moves Wes toward the camera (-forward) and StickX 255 the mirror of
        # (-fz, fx); so with our stick(sx, sy): world = sx * (fz, -fx) + sy * (-fx, -fz)
        return (fz, -fx, -fx, -fz)
    except Exception:
        return None

def live_people():
    """Positions of the visible people in the room now (people table: active, +0x20 visible, model position), without
    the player (group 0, index 100) and the partner (index 101)."""
    out = []
    try:
        base, n = u32(PEOPLE_PTR), u32(PEOPLE_MAX)
        for i in range(min(n, 512)):
            e = base + i * ENTRY
            if not memory.read_u8(e) or not memory.read_u8(e + 0x20):
                continue
            if u32(e + 0x28) == 0 and u32(e + 0x2C) in (100, 101):
                continue
            m = u32(e + 8)
            if 0x80000000 <= m < 0x81800000:
                out.append((memory.read_f32(m + 0x18), memory.read_f32(m + 0x20)))
    except Exception:
        pass
    return out

def stick(sx, sy): return {"StickX": int(128 + 127 * max(-1, min(1, sx))), "StickY": int(128 + 127 * max(-1, min(1, sy)))}
def toward(p, t):
    a, b, c, d = st["M"]; det = a * d - b * c; dx, dz = t[0] - p[0], t[1] - p[1]
    sx, sy = (d * dx - b * dz) / det, (-c * dx + a * dz) / det; n = math.hypot(sx, sy) or 1
    return stick(sx / n, sy / n)

presses = []
for item in PRESS.split(","):
    item = item.strip()
    if ":" in item:                       # BUTTON:every:for[@start]
        spec, _, start = item.partition("@")
        btn, every, dur = spec.split(":"); presses.append(("every", btn, int(every), int(dur), int(start or 0)))
    elif "@" in item:                     # BUTTON@at
        btn, at = item.split("@"); presses.append(("once", btn, int(at)))
BTN = {"A": "A", "B": "B", "UP": "Up", "DOWN": "Down", "LEFT": "Left", "RIGHT": "Right", "START": "Start", "X": "X", "Y": "Y"}

def on_frame():
    global frame
    frame += 1
    if frame == 1:
        savestate.load_from_file(os.path.join(OUT, STATE)); log(ev="loaded", state=STATE); st["phase"] = "calib_up"; return
    p = pos(); b = {}
    room = u32(ROOM)
    if ROOM_SAVE:
        # leg mode: when the room changes, save <name> after <frames> (pressing A meanwhile) and stop
        st.setdefault("room0", room)
        if room != st["room0"]:
            rs = st.setdefault("room_changed", frame); name, after = ROOM_SAVE.split("@")
            if frame - rs < int(after):
                if (frame - rs) % 40 < 3 and ROOM_A: controller.set_gc_buttons(0, {"A": True})
                return
            if frame - rs == int(after):
                savestate.save_to_file(os.path.join(OUT, name + ".sav")); pending.append(name)
                log(ev="state", spot=name, frame=frame, room=room); return
            if frame - rs > int(after) + 180:
                log(ev="exit", frame=frame); lf.close(); os._exit(0)
            return
    if frame % 30 == 0:
        pending.append(frame); log(ev="pos", frame=frame, room=u32(ROOM), pos=p, phase=st["phase"])
    ph = st["phase"]
    cm = camera_map()
    if cm and ph in ("calib_up", "calib_right"):
        st["M"] = cm; st["phase"] = ph = "walk"; log(ev="camera_map", frame=frame, M=cm)
    elif cm and ph in ("walk", "face"):
        st["M"] = cm                              # the field camera turns as Wes moves: follow it every frame
    if ph in ("calib_up", "calib_right") and p:
        c = st["calib"]; c.append(p)
        if len(c) <= 20:
            b.update(stick(0, 1) if ph == "calib_up" else stick(1, 0))
        else:
            dx, dz = c[-1][0] - c[3][0], c[-1][1] - c[3][1]; c.clear()
            if math.hypot(dx, dz) < 0.3:
                if frame % 40 < 3: b["A"] = True
            else:
                st[ph] = (dx, dz)
                if ph == "calib_up": st["phase"] = "calib_right"
                else:
                    (ux, uz), (rx, rz) = st["calib_up"], st["calib_right"]; st["M"] = (rx, ux, rz, uz)
                    st["phase"] = "walk"; log(ev="calibrated", up=(ux, uz), right=(rx, rz), frame=frame)
    elif ph == "walk" and p and PLAN and not st.get("planned"):
        parts = PLAN.split("|"); goal = tuple(map(float, parts[3].split(",")))
        try:
            import sys as _s
            _s.path.insert(0, os.environ.get("GOTO_TOOLDIR") or os.path.dirname(os.path.abspath(__file__)))
            import path_plan
            way = path_plan.plan(parts[0], p, goal, npcs=parts[1] or None, room=parts[2] or None,
                                 people=live_people() or None, margin=int(os.environ.get("GOTO_MARGIN", "2")),
                                 npc_radius=float(os.environ.get("GOTO_NPC_RADIUS", "5"))) or [goal]
        except Exception as e:                    # never let a planning error stop the frame callback
            log(ev="plan_error", err=repr(e)); way = [goal]
        if len(parts) > 4: way = way[:-1] + [tuple(map(float, parts[4].split(",")))]
        WAY[:] = way; st["planned"] = True; log(ev="planned", frame=frame, start=p, way=way, people=live_people())
    elif ph == "walk" and p:
        k = st.setdefault("wp", 0); tgt = WAY[k]
        last = st.get("lastp"); st["lastp"] = p
        moving = last is not None and math.hypot(p[0] - last[0], p[1] - last[1]) > 0.05
        st["still"] = 0 if moving else st.get("still", 0) + 1
        # progress-based stuck test: Wes can jitter in place against an obstacle, so "not moving" isn't enough
        dist = math.hypot(tgt[0] - p[0], tgt[1] - p[1])
        if st.get("best_wp") != k or dist < st.get("best", 1e9) - 1.0:
            st["best_wp"] = k; st["best"] = dist; st["best_frame"] = frame
        no_progress = frame - st.get("best_frame", frame) > 60
        side = st.get("side_until", 0)
        if st["still"] > 20 and frame % 40 < 3:
            b["B"] = True                         # a text box may still be open: B closes it (A would re-talk)
        if frame < side:                          # stuck recovery: step sideways, then resume
            b.update(st["side_pad"])
        elif (st["still"] > 40 or no_progress) and (k < len(WAY) - 1 or (dist > 10 and st.get("tries", 0) < 6)):
            tries = st["tries"] = st.get("tries", 0) + 1
            if tries > 6 and k < len(WAY) - 1:   # give up on this waypoint
                st["wp"] = k + 1; st["tries"] = 0; log(ev="skip_waypoint", n=k, frame=frame, pos=p)
            else:
                dx, dz = tgt[0] - p[0], tgt[1] - p[1]; sgn = 1 if tries % 2 else -1
                st["side_pad"] = toward(p, (p[0] - sgn * dz, p[1] + sgn * dx))
                st["side_until"] = frame + 20; st["still"] = 0; st["best_frame"] = frame + 20; log(ev="sidestep", n=k, frame=frame, pos=p, tries=tries)
        elif math.hypot(tgt[0] - p[0], tgt[1] - p[1]) < 2.5 and k < len(WAY) - 1:
            st["tries"] = 0
            st["wp"] = k + 1; log(ev="waypoint", n=k + 1, frame=frame, pos=p)
        elif math.hypot(tgt[0] - p[0], tgt[1] - p[1]) < 2.5 or (k == len(WAY) - 1 and (st["still"] > 60 or no_progress)):
            # arrived, or stopped on the way to the last point (a closed door, a wall, or a scene took over)
            st["phase"] = "face"; st["face_from"] = frame; log(ev="arrived", frame=frame, pos=p, stuck=st["still"] > 60)
        elif frame >= side:
            b.update(toward(p, tgt))
    elif ph == "face" and p:
        if frame - st["face_from"] < PUSH: b.update(toward(p, FACE))
        else: st["phase"] = "press"; st["t0"] = frame; log(ev="press_start", frame=frame, pos=p)
    elif ph == "press":
        rel = frame - st["t0"]; done = True
        for pr in presses:
            if pr[0] == "once":
                if pr[2] <= rel < pr[2] + 4: b[BTN[pr[1]]] = True
                if rel < pr[2] + 4: done = False
            else:
                start = pr[4]
                if rel < start + pr[3]:
                    done = False
                    if rel >= start and (rel - start) % pr[2] < 3: b[BTN[pr[1]]] = True
        if SAVE:
            name, at = SAVE.split("@")
            if rel <= int(at) + 180: done = False  # wait for the save point, then 3 s: Dolphin writes the state
                                                   # asynchronously (a .tmp renamed when done); exiting at once leaves
                                                   # only the .tmp (seen 2026-10-16)
            if rel == int(at):
                savestate.save_to_file(os.path.join(OUT, name + ".sav")); pending.append(name); log(ev="state", spot=name, frame=frame)
        if done and not pending and rel > 30:
            log(ev="exit", frame=frame); lf.close(); os._exit(0)
    if b: controller.set_gc_buttons(0, b)
    if frame > 60 * 60 * 6:
        log(ev="timeout", frame=frame, phase=st["phase"]); lf.close(); os._exit(1)
try:  # runtime id log (idlog.py): battles and rooms reached; set TESTING_DIR to this folder (the fork's import path)
    import sys
    sys.path.insert(0, os.environ.get("TESTING_DIR", ""))
    import idlog
except Exception as e:
    open(os.path.join(OUT, "idlog_error.txt"), "w").write(repr(e) + "\n" + repr(sys.path))
    event.on_frameadvance(on_frame)
else:
    idlog.install(event, on_frame, OUT, run="goto:" + STATE)

def on_drawn(w, h, d):
    while pending:
        n = pending.pop()
        raw = b"".join(b"\x00" + d[y * w * 3:(y + 1) * w * 3] for y in range(h))
        ch = lambda t, x: struct.pack(">I", len(x)) + t + x + struct.pack(">I", zlib.crc32(t + x) & 0xFFFFFFFF)
        path = os.path.join(OUT, n + ".png") if isinstance(n, str) else os.path.join(SHOTS, "g%06d.png" % n)
        open(path, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                               + ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))
event.on_framedrawn(on_drawn)
