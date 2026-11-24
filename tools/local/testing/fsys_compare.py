"""Archive loads, console vs PC (2026-10-25). For each archive id's n-th load in a stretch: the label it first
appears in an FSYS slot and the label it first reaches 1000 (loaded), on the console (dolphin_playrec.py's fsys probe,
fsys.csv) and on the PC (ORRE_FSYS_LOG=1's [fsys] lines), labels counted from each side's room entry. One line per
load, with the difference in the "loaded" label; a disc-timing change shows at once which loads it helped or hurt.

    python -I tools/local/testing/fsys_compare.py <fsys.csv> <pc log> <dolphin entry frame> <pc entry frame + 2>
                                                  <from> <to>

The PC's entry is its "[slice] entered" frame + 2 (the same alignment as the owners and sfx logs).

--retrace (2026-10-25): use the [fsys] line's retrace column instead of its game-frame column, and give the PC
entry as a retrace (the room's disc anchor: the console's entry frame + the "PC's clock is the recording's +N" offset
of the trace log). The game-frame count falls behind the retraces by every retrace the game waited, so after a long
wait the frame column puts every PC event early (P05's street battle: 12 behind; it looked like the battle started 13
labels early when it was in step).
"""
import re
import sys
from collections import defaultdict


def loads(rows):
    """rows: (label, {id: status}) in order -> {(id, n): [first label, loaded label]}"""
    out, count, prev = {}, defaultdict(int), {}
    for t, st in rows:
        for i, s in st.items():
            if i not in prev:
                count[i] += 1
                out[(i, count[i])] = [t, None]
            if s == 1000 and prev.get(i) != 1000 and out[(i, count[i])][1] is None:
                out[(i, count[i])][1] = t
        prev = st
    return out


def parse(slots):
    """"id:status:refs:name ..." -> {"id:name": status}"""
    st = {}
    for p in slots.split():
        a = p.split(":")
        if len(a) >= 3:
            st[a[0] + ":" + a[3] if len(a) > 3 else a[0]] = int(a[1])
    return st


def main():
    use_retrace = "--retrace" in sys.argv
    args = [a for a in sys.argv[1:] if a != "--retrace"]
    fs, log = args[0], args[1]
    d0, p0, lo, hi = map(int, args[2:6])
    d = []
    with open(fs) as f:
        for line in f:
            if line.startswith("#") or line.startswith("frame"):
                continue
            parts = line.rstrip("\n").split(",", 2)
            t = int(parts[0]) - d0
            if lo <= t <= hi:
                d.append((t, parse(parts[2] if len(parts) > 2 else "")))
    p = []
    for line in open(log, errors="replace"):
        m = re.match(r"\[fsys\] (\d+),(\d+),\d+,(.*)", line)
        if m:
            t = int(m.group(2 if use_retrace else 1)) - p0
            if lo <= t <= hi:
                p.append((t, parse(m.group(3))))
    a, b = loads(d), loads(p)
    fmt = lambda v: "-" if v is None else str(v)
    for k in sorted(set(a) | set(b), key=lambda k: (a.get(k, b.get(k))[0])):
        x, y = a.get(k), b.get(k)
        diff = (y[1] - x[1]) if x and y and x[1] is not None and y[1] is not None else None
        print(f"{k[0]:40s} #{k[1]}  console {fmt(x and x[0]):>6} -> {fmt(x and x[1]):>6}   PC {fmt(y and y[0]):>6} -> "
              f"{fmt(y and y[1]):>6}   loaded {'' if diff is None else '%+d' % diff}")


if __name__ == "__main__":
    main()
