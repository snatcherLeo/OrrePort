"""Where each console random state lies on the PC's path (2026-10-25). For every state Dolphin samples (rng.csv,
one per label) in a stretch: the PC draw that holds it, that draw's PC label (and its offset from the console's), and
the draws the PC made since the previous sampled state, counted by caller. A constant offset with the same draw
counts means the sequence matches; a jump at one label with the counts restoring after is a sampling artefact (the
console's sample fell further into a frame), not a split.

    python -I tools/local/testing/rng_drawlead.py <rng.csv> <pc log> <dolphin entry frame> <pc entry frame> <from> <to>

The PC log needs ORRE_RNG_TRACE=<from>-<to> (PC frames) over the stretch. The trace stops after 200,000 draws (a
battle with particles makes ~500 a frame, so about 400 frames): "not on the PC path" from one label on, right after the
last traced draw, is the cap, not a split; narrow the range.
"""
import csv
import re
import sys


def main():
    rng, log = sys.argv[1], sys.argv[2]
    d0, p0, lo, hi = map(int, sys.argv[3:7])
    d = []
    with open(rng) as f:
        for r in csv.DictReader(f):
            t = int(r["frame"]) - d0
            if lo <= t <= hi:
                d.append((t, int(r["rng"])))
    draws, cur = [], None  # (draw number, caller, state, PC label)
    for line in open(log, errors="replace"):
        m = re.match(r"\[rngtrace\] frame \d+ retrace (\d+)", line)
        if m:
            cur = int(m.group(1)) - p0
            continue
        m = re.match(r"\[rngtrace\] (\d+) (\S+) ([0-9A-F]{8})", line)
        if m and cur is not None:
            draws.append((int(m.group(1)), m.group(2)[-6:], int(m.group(3), 16), cur))
    where = {}
    for i, (_, _, s, _) in enumerate(draws):
        where.setdefault(s, i)
    prev_i, prev_state = None, None
    for t, s in d:
        if s == prev_state:
            continue
        i = where.get(s)
        if i is None:
            print(f"{t:5d} {s:08X} not on the PC path")
            prev_state = s
            continue
        between = draws[prev_i + 1:i + 1] if prev_i is not None else []
        callers = {}
        for _, c, _, _ in between:
            callers[c] = callers.get(c, 0) + 1
        print(f"{t:5d} {s:08X} PC draw {draws[i][0]} at PC label {draws[i][3]} ({draws[i][3] - t:+d})  "
              f"{len(between)} draws: {callers}")
        prev_i, prev_state = i, s


if __name__ == "__main__":
    main()
