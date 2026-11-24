"""A Dolphin replay's random state at the start of every game frame -> the PC replay's per-frame resync file (
2026-10-25).

    python -I tools/local/testing/rngframes_to_pc.py <replay folder> <out.rngframes>

Why: in a recorded battle the PC draws as many random numbers per frame as the console (verified on P05's battle 56),
but a frame of load timing before a turn, or a seed taken at the wrong point of a 30 fps frame, puts one roll on a
different number (an AI pick, a flinch, a confusion self-hit), and the battle parts. Resyncing at every frame start
gives every roll the console's number, so what still differs is behaviour, not timing: the mode for hunting port bugs
in long sittings (snatcherLeo's option C: resync at timing-only splits). Test aid only, like the frame-times file.

Input: the replay's `frames.csv` (frame = the label the game frame ends at, frameRetraces), `rng.csv` (the generator's
state at the end of each label) and `replay_events.jsonl` (the room entries). Output: one line per room entry, split
exactly as frames_to_pc.py splits the frame lengths, `0x<room> <state> <state> ...`: the state at the start of each
game frame of that entry (the state at the end of the frame before), in hex. The PC (port/src/pc_frame_times.c,
ORRE_REPLAY_RNG=@file:<out>) sets the generator to it at the start of each frame, in step with the frame lengths.
"""
import csv
import json
import os
import sys


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    rep, out_path = sys.argv[1], sys.argv[2]
    rows = [l.strip().split(",") for l in open(os.path.join(rep, "frames.csv")) if l[:1].isdigit()]
    rng = {}
    for r in csv.reader(open(os.path.join(rep, "rng.csv"))):
        if r and r[0].isdigit():
            rng[int(r[0])] = int(r[1])
    starts = []
    for line in open(os.path.join(rep, "replay_events.jsonl"), encoding="utf-8"):
        e = json.loads(line)
        if e.get("ev") == "entered":
            starts.append((e["f"], e["room"]))
    # the same split as frames_to_pc.py (a line per "entered" event; the boot stretch before the first is unused)
    entries, k, last = [(0xFFFFFFFF, [])], -1, None
    missing = 0
    for f, _r, n in rows:
        f, n = int(f), int(n)
        while k + 1 < len(starts) and f >= starts[k + 1][0]:
            k += 1
            entries.append((starts[k][1], []))
        s = rng.get(f - n)
        if s is None:
            missing += 1
            s = last
        entries[-1][1].append(s)
        last = rng.get(f, s)
    lines = ["# the random state at the start of each game frame, one line per room entry (rngframes_to_pc.py from " +
             os.path.basename(os.path.abspath(rep)) + ")"]
    for room, states in entries:
        lines.append(f"0x{room & 0xFFFFFFFF:X} " + " ".join("-" if s is None else f"{s:08X}" for s in states))
    open(out_path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print(f"{out_path}: {len(entries)} room entries, {sum(len(s) for _, s in entries)} frames, "
          f"{missing} without a recorded state")


if __name__ == "__main__":
    main()
