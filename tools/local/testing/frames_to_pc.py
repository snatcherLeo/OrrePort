"""A Dolphin replay's frame lengths -> the PC replay's frame-times file (2026-10-18).

    python -I tools/local/testing/frames_to_pc.py <replay folder>/frames.csv <out.frames>

Why: some scenes ask for 60 fps but the console takes two retraces per frame there (gamecube-accuracy.md row 4b), so
code that runs once per update (eye-blink timers, NPC wandering, their random draws) runs half as often as on the PC,
and a recorded movie drifts. snatcherLeo's decision (2026-10-18): replay mode plays the console's frame timing exactly.

Input: `frames.csv` from `playrec.py replay N --probe frames` (one row per game frame: frame, room, frameRetraces).
Output, one line per room entry, in order: `0x<room> <run> <run> ...`, each run `count*retraces` (e.g. `120*1 300*2`).
The PC (port/src/pc_frame_times.c, ORRE_REPLAY_FRAMES=@file:<out.frames>) starts each line at its own entry of that
room, so its faster loads don't shift the timing, and makes each game frame last at least that many retraces.
"""
import json
import os
import sys


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    rows = [l.strip().split(",") for l in open(sys.argv[1]) if l[:1].isdigit()]
    # Lines split where the PC's own line starts: the replay's room entries (replay_events.jsonl "entered", the same
    # moment as the PC's room hook: the floor context switch, idlog.py). Corrected 2026-10-19: splitting where the room
    # variable changed put each load's no-room stretch (0xFFFFFFFF) on a line of its own that the PC never uses, so the
    # PC started a room's line up to a whole load early and ran its tail at the game's own (60 fps) timing; in sitting 2
    # Phenac's arrival then ran on the wrong pacing (Wes walked further and set story flag 512, which Dolphin never did).
    events = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), "replay_events.jsonl")
    starts = []
    if os.path.exists(events):
        for line in open(events, encoding="utf-8"):
            e = json.loads(line)
            if e.get("ev") == "entered":
                starts.append((e["f"], e["room"]))
    entries = []
    if starts:
        k = -1
        entries.append((0xFFFFFFFF, []))  # before the first entry (boot): the PC never uses this line
        for f, _r, n in rows:
            f, n = int(f), int(n)
            while k + 1 < len(starts) and f >= starts[k + 1][0]:
                k += 1
                entries.append((starts[k][1], []))
            entries[-1][1].append(n)
    else:  # no events file: the old split by the room variable
        room = None
        for _f, r, n in rows:
            r, n = int(r), int(n)
            if r != room:
                entries.append((r, []))
                room = r
            entries[-1][1].append(n)
    out = ["# frame lengths in retraces per game frame, one line per room entry (frames_to_pc.py from " +
           sys.argv[1].replace("\\", "/").split("/")[-2] + ")"]
    for r, ns in entries:
        runs, prev, count = [], None, 0
        for n in ns + [None]:
            if n == prev:
                count += 1
                continue
            if prev is not None:
                runs.append(f"{count}*{prev}")
            prev, count = n, 1
        out.append(f"0x{r & 0xFFFFFFFF:X} " + " ".join(runs))
    open(sys.argv[2], "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(f"{sys.argv[2]}: {len(entries)} room entries, {sum(len(ns) for _, ns in entries)} frames")


if __name__ == "__main__":
    main()
