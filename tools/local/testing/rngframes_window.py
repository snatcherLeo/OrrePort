"""Keep the per-frame random-state resync only inside chosen stretches of chosen room visits (2026-10-27).

    python -I tools/local/testing/rngframes_window.py <run>/sitting.rngframes ROOM:VISIT[@FROM[-TO]] [...]

`replay_check.py --rng-frames all` writes the console's random state for every game frame (rngframes_to_pc.py);
forcing it through a whole battle can put a roll on the wrong draw where the PC's frame splits its work differently
(P01's Folly battle: one turn went another way). This rewrites the file in place: every frame outside the listed
stretches becomes `-` (the PC leaves its own state there, port/src/pc_frame_times.c). FROM / TO count game frames
from the visit's entry (the file's own items). Use it on a `--prepare-only` folder (with `--rng-frames all`), then
run the folder as pad_resync.py describes. Example (P01): `0xCA:1@5500 0x390:2` forces only Folly's room after her
battle and the second name screen.
"""
import sys


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    path = sys.argv[1]
    keep = []
    for spec in sys.argv[2:]:
        where, _, span = spec.partition("@")
        room, _, visit = where.partition(":")
        lo, _, hi = span.partition("-")
        keep.append((int(room, 16), int(visit or 1), int(lo or 0), int(hi) if hi else 10 ** 9))
    lines = open(path, encoding="utf-8").read().splitlines()
    seen, out, kept = {}, [], 0
    for line in lines:
        if not line.startswith("0x"):
            out.append(line)
            continue
        head, *states = line.split()
        room = int(head, 16)
        seen[room] = seen.get(room, 0) + 1
        spans = [(lo, hi) for r, v, lo, hi in keep if r == room and v == seen[room]]
        new = []
        for i, s in enumerate(states):
            on = any(lo <= i <= hi for lo, hi in spans)
            new.append(s if on else "-")
            kept += on and s != "-"
        out.append(" ".join([head] + new))
    open(path, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print(f"{path}: {kept} frame states kept")


if __name__ == "__main__":
    main()
