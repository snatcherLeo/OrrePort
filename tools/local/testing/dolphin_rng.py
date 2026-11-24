"""Count the RNG calls per frame in a room, standing still and walking (testing lane; for the project's battle / RNG pages).

Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set RNG_STATE=<state in it>   set RNG_FRAMES=300   set RNG_WALK=SX,SY (default 128,255)
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_rng.py

Loads the state, waits 30 frames, then RNG_FRAMES frames with no input ("still") and RNG_FRAMES frames holding the stick
("walk"), and writes <state>.rng.csv: phase, frame, calls this frame, RNG state. Calls are counted by stepping the
game's generator (rng.md: state = state * 0x343FD + 0x269EC3, *lbl_80478C94) from last frame's state to this frame's,
so every call is counted exactly; a state not reached within 50,000 steps is logged as -1 (a reseed or a direct
write). The fork reports a code breakpoint only on its first hit, so callers can't be listed per call here.
"""
import csv, os
from dolphin import controller, event, memory, savestate

OUT = os.environ.get("POS_OUT", ".")
STATE = os.environ["RNG_STATE"]
N = int(os.environ.get("RNG_FRAMES", "300"))
SX, SY = (int(v) for v in os.environ.get("RNG_WALK", "128,255").split(","))
RNGPTR, MUL, ADD, MASK, LIMIT = 0x80478C94, 0x343FD, 0x269EC3, 0xFFFFFFFF, 50000


def rng():
    p = memory.read_u32(RNGPTR)
    return memory.read_u32(p) if 0x80000000 <= p < 0x81800000 else None


def steps(a, b):
    """How many generator steps lead from state a to state b (-1 if not within LIMIT)."""
    if a is None or b is None:
        return -1
    n, s = 0, a
    while s != b:
        s = (s * MUL + ADD) & MASK
        n += 1
        if n > LIMIT:
            return -1
    return n


frame = 0
rows = []
prev = None


def on_frame():
    global frame, prev
    frame += 1
    if frame == 1:
        savestate.load_from_file(os.path.join(OUT, STATE))
        return
    if frame < 30:
        prev = rng()
        return
    t = frame - 30
    phase = "still" if t < N else "walk" if t < 2 * N else None
    if phase is None:
        with open(os.path.join(OUT, STATE + ".rng.csv"), "w", newline="") as f:
            w = csv.writer(f)
            w.writerow(["phase", "frame", "calls", "state"])
            w.writerows(rows)
        os._exit(0)
    if phase == "walk":
        # only the off-centre axes, as raw fork values: Felk's fork turns an explicit 128 into full tilt and an
        # axis left out into centre (measured, dolphin_stickcal.py); the walk only needs to move, not a set heading
        controller.set_gc_buttons(0, {k: v for k, v in (("StickX", SX), ("StickY", SY)) if v != 128})
    cur = rng()
    rows.append([phase, t, steps(prev, cur), cur])
    prev = cur


event.on_frameadvance(on_frame)
