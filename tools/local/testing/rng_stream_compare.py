"""Where in a stretch do the PC's random draws drift from Dolphin's? Frame by frame, from the per-frame RNG states of a
playrec replay (2026-10-21; snatcherLeo: the battle cameras must be exactly the same, and their shots are random picks).

    python -I tools/local/testing/rng_stream_compare.py <Dolphin replay_K folder> <PC run log>
        --dolphin-from F --pc-from G [--frames N]

The game's generator is one linear sequence (HSD_Rand: state * 214013 + 2531011, src/hsd/random.c), so a state says
how far along the sequence the game is. Both sides are put on the same sequence (a room entry's seed in a replay), so
the only question per frame is how many draws each side has made: the drift.
- Dolphin: rng.csv (the probe `rng`: the state at each frame's end), from frame F;
- PC: the run log with ORRE_RNG_TRACE=<from>-<to> covering frame G on ("[rngtrace] frame N" lines, then one
  "[rngtrace] <n> <caller> <state>" line per draw, and the wrappers' "[rngtrace] GSrand from <addr>" caller lines).
Frames F and G are the same moment on the two sides (for example the battle room's first camera frame in both camera
logs). Positions count from Dolphin's state at frame F; the PC's states are found on the same sequence. Prints the draws
per frame on both sides and the running drift, stops after the first frames where the drift changes, and lists the PC
draws and callers of those frames (decode the addresses with addr2line on the PC exe).
"""
import argparse, csv, re

A, C, M = 214013, 2531011, 1 << 32
# newer builds add " retrace R": Dolphin's labels count retraces, and the game frame count falls behind them by every
# retrace the game spent waiting, so the PC side is keyed by the retrace when the log has it (--pc-from is then a
# retrace count too)
FRAME = re.compile(r"^\[rngtrace\] frame (\d+)(?: retrace (\d+))?")
DRAW = re.compile(r"^\[rngtrace\] (\d+) (\S+) ([0-9A-Fa-f]{8})")
CALLER = re.compile(r"^\[rngtrace\] (\S+) from (\S+)")


def steps(a, b, limit=200000):
    """Draws from state a to state b (None if not within limit)"""
    n, s = 0, a
    while s != b:
        s = (s * A + C) % M
        n += 1
        if n > limit:
            return None
    return n


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dolphin")
    ap.add_argument("pc_log")
    ap.add_argument("--dolphin-from", type=int, required=True)
    ap.add_argument("--pc-from", type=int, required=True)
    ap.add_argument("--frames", type=int, default=600)
    ap.add_argument("--after", type=int, default=3, help="frames listed after the first drift change")
    a = ap.parse_args()

    dstate = {}
    for r in csv.DictReader(open(a.dolphin.rstrip("/\\") + "/rng.csv", encoding="utf-8")):
        if r.get("rng", "").isdigit():
            dstate[int(r["frame"])] = int(r["rng"])
    anchor = dstate.get(a.dolphin_from)
    if anchor is None:
        raise SystemExit(f"no Dolphin RNG state at frame {a.dolphin_from}")

    pend, pdraws, frame = {}, {}, None  # PC: state at each frame's end, draws (caller, state) per frame
    callers = []
    for line in open(a.pc_log, encoding="utf-8", errors="replace"):
        m = FRAME.match(line)
        if m:
            frame = int(m.group(2) or m.group(1))
            continue
        m = DRAW.match(line)
        if m and frame is not None:
            st = int(m.group(3), 16)
            pdraws.setdefault(frame, []).append((callers[-1] if callers else "", m.group(2), st))
            callers = []
            pend[frame] = st
            continue
        m = CALLER.match(line)
        if m:
            callers.append(f"{m.group(1)} from {m.group(2)}")

    # the PC's state at the end of each frame from G on (a frame with no draws keeps the previous state)
    def pc_state_at(f):
        best = None
        for g in sorted(pend):
            if g <= f:
                best = pend[g]
        return best

    start_pc = pc_state_at(a.pc_from)
    if start_pc is None:
        raise SystemExit(f"no PC RNG trace at or before frame {a.pc_from} (ORRE_RNG_TRACE's range?)")
    base = steps(anchor, start_pc)
    back = None if base is not None else steps(start_pc, anchor)
    if base is None and back is None:
        raise SystemExit("the two sides are not on the same stretch of the sequence (a seed between them?)")
    print(f"start: Dolphin frame {a.dolphin_from} state {anchor:08X}; PC frame {a.pc_from} state {start_pc:08X} "
          f"({'+' + str(base) if base is not None else '-' + str(back)} draws from Dolphin's)")

    dpos, ppos = 0, base if base is not None else -back
    dprev, pprev = anchor, start_pc
    changed_at, last_drift = None, ppos - dpos
    print(" k  | Dolphin draws | PC draws | drift (PC minus Dolphin, draws)")
    for k in range(1, a.frames + 1):
        df, pf = a.dolphin_from + k, a.pc_from + k
        ds = dstate.get(df, dprev)
        n_d = steps(dprev, ds)
        ps = pend.get(pf, pprev)
        n_p = steps(pprev, ps)
        if n_d is None:
            print(f"{k:4d} | sequence jump on Dolphin: stopping")
            break
        seeded = ""
        if n_p is None:
            # the replay's "!seed" steps put Dolphin's state into the PC (battle window openings): find where the PC now
            # is on Dolphin's sequence
            fwd = steps(anchor, ps)
            if fwd is None:
                bwd = steps(ps, anchor)
                if bwd is None:
                    print(f"{k:4d} | the PC jumped off Dolphin's stretch of the sequence: stopping")
                    break
                fwd = -bwd
            n_p, ppos, seeded = fwd - ppos, fwd - (fwd - ppos), "   (PC seeded)"
        dpos, ppos, dprev, pprev = dpos + n_d, ppos + n_p, ds, ps
        drift = ppos - dpos
        mark = "" if drift == last_drift else "   <-- drift changes"
        if n_d or n_p or mark:
            # the PC's callers this frame, counted (decode with addr2line)
            who = {}
            for c, caller, st in pdraws.get(pf, []):
                w = c or caller
                who[w] = who.get(w, 0) + 1
            calls = ", ".join(f"{n}x {w}" for w, n in sorted(who.items(), key=lambda x: -x[1]))
            print(f"{k:4d} | {n_d:13d} | {n_p:8d} | {drift:+d}{mark}{seeded}{'   PC: ' + calls if calls else ''}")
        if drift != 0:
            changed_at = k if changed_at is None else changed_at
        else:
            changed_at = None  # back in step: a burst that only came a few frames apart
        last_drift = drift
        if changed_at is not None and k >= changed_at + a.after:
            print(f"the drift has lasted {a.after} frames (from k = {changed_at}): stopping")
            break


if __name__ == "__main__":
    main()
