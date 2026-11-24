"""The RNG from power-on to the logo, frame by frame (testing lane; golden-slice M18 / D6).

Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set BOOTRNG_FRAMES=700
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_bootrng.py

Every frame reads the RNG state (*(u32*)*lbl_80478C94, rng.md) and writes bootrng.csv: frame, state, and how many
generator steps (state * 0x343FD + 0x269EC3) lead from the previous frame's state to this one (-1 = not reachable
within 50,000 steps: the seeding itself). The first state seen is the seed if seeding and the first draws fall in
different frames. Question it answers (2026-10-17): is Dolphin's seed 0xC1F12F2A, 1,022 draws before the
0xCC0C0B30 seen at the logo, as on the PC (memoInit's start-up draws)?
"""
import csv, os, time
from dolphin import event, memory

OUT = os.environ.get("POS_OUT", ".")
N = int(os.environ.get("BOOTRNG_FRAMES", "700"))
RNGPTR, MUL, ADD, MASK, LIMIT = 0x80478C94, 0x343FD, 0x269EC3, 0xFFFFFFFF, 50000
f = open(os.path.join(OUT, "bootrng.csv"), "w", newline="")
w = csv.writer(f)
w.writerow(["frame", "state", "steps"])
st = {"frame": 0, "prev": None}


def rng():
    p = memory.read_u32(RNGPTR)
    return memory.read_u32(p) if 0x80000000 <= p < 0x81800000 else None


def steps(a, b):
    n = 0
    while a != b:
        a = (a * MUL + ADD) & MASK
        n += 1
        if n > LIMIT:
            return -1
    return n


def on_frame():
    st["frame"] += 1
    s = rng()
    if s is not None and s != st["prev"]:
        w.writerow([st["frame"], "0x%08X" % s, steps(st["prev"], s) if st["prev"] is not None else ""])
        f.flush()
        st["prev"] = s
    if st["frame"] >= N:
        f.close()
        time.sleep(1)
        os._exit(0)


event.on_frameadvance(on_frame)
