"""Hands-on check of Felk's Dolphin scripting fork (docs/testing-plan.md, "Felk fork: interface check"), 2026-10-15.

Runs inside the fork: Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_felk_check.py (set FELK_OUT to an output
folder first; the fork in portable mode, single-core CPU, fixed RTC, code breakpoints in User/GameSettings/GC6E01.ini as
"$800ff58c nl" / "$800e0c78 nl" = log only). Logs one JSON line per event: room changes (memory watch on lbl_80478B18
and a code breakpoint on floorRequestChange with its argument from r3), the RNG seeding call (fn_800E0C78), the RNG
state every 30 frames, and a PNG screenshot every 600 frames; presses Down / A / Up / A / Start on a fixed cycle, which
gets from power-on through the menus and the opening movie into the first room. Our code, not the fork's.
"""
import json, zlib, struct, os, time
from dolphin import event, memory, controller, registers, emulation

OUT = os.path.join(os.environ.get("FELK_OUT", "."), "run.jsonl")
SHOTS = os.path.join(os.environ.get("FELK_OUT", "."), "shots")
os.makedirs(SHOTS, exist_ok=True)
f = open(OUT, "w", buffering=1)
def log(**kw):
    f.write(json.dumps(kw) + "\n")

ROOM = 0x80478B18      # requested room id (floorRequestChange writes it)
RNGPTR = 0x80478C94    # pointer to the shared RNG state
frame = 0
t0 = time.time()
log(ev="start")

try:
    memory.add_memcheck(ROOM)
    log(ev="memcheck_added", addr=hex(ROOM))
except Exception as e:
    log(ev="memcheck_error", err=repr(e))

def on_mem(is_write, addr, value):
    log(ev="mem", frame=frame, write=is_write, addr=hex(addr), value=value)
event.on_memorybreakpoint(on_mem)

OURS = {0x800FF58C: "floorRequestChange", 0x800E0C78: "rng_seed"}
seen = set()
def on_code(addr):
    if addr not in OURS or (frame, addr) in seen:   # the event also fires for unrelated instructions, and twice per hit
        return
    seen.add((frame, addr))
    try:
        r3 = registers.read_gpr(3)
    except Exception as e:
        r3 = repr(e)
    log(ev="code", frame=frame, addr=hex(addr), r3=r3)
event.on_codebreakpoint(on_code)

last = {"rng": None, "room": None}
def on_frame():
    global frame
    frame += 1
    try:
        p = memory.read_u32(RNGPTR)
        s = memory.read_u32(p) if 0x80000000 <= p < 0x81800000 else None
        room = memory.read_u32(ROOM)
    except Exception as e:
        log(ev="readerr", frame=frame, err=repr(e))
        return
    if frame % 30 == 0:
        log(ev="f", frame=frame, rngptr=hex(p), rng=s, room=room)
    # Inputs: nothing for the first 600 frames, then A every 40 frames and Start every 120 (3 frames each).
    if frame > 600:
        b = {}
        k = frame % 160
        if k < 3:
            b["Down"] = True
        elif 40 <= k < 43 or 120 <= k < 123:
            b["A"] = True
        elif 80 <= k < 83:
            b["Up"] = True
        if frame % 480 == 460:
            b["Start"] = True
        if b:
            controller.set_gc_buttons(0, b)
event.on_frameadvance(on_frame)

def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as g:
        g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))

last_shot = {"mark": 0}
def on_drawn(w, h, data):
    # the first frame drawn after each 600-frame mark (in the field the game draws every other emulated frame)
    if frame // 600 > last_shot["mark"]:
        last_shot["mark"] = frame // 600
        try:
            write_png(os.path.join(SHOTS, f"f{frame:06d}.png"), w, h, data)
            log(ev="shot", frame=frame, w=w, h=h, wall=round(time.time() - t0, 1))
        except Exception as e:
            log(ev="shot_error", err=repr(e))
event.on_framedrawn(on_drawn)
