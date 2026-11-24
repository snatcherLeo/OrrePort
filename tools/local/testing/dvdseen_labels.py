"""Add the absolute frame label to the dvdseen map (2026-10-20; option (b), replay disc completions by the frame
the console's game saw them).

    python -I tools/local/testing/dvdseen_labels.py <replay folder> [--out FILE]

dvd_seen.py writes "<dvd.csv row> <line> <offset>", the offset in probe labels (retraces) since that line's room
entry (replay_events "entered"; line 0 counts from the replay's first event). The PC's room entries can sit up to ~100
frames off Dolphin's room-change moment, so the PC uses the label on the recording's own clock instead, which the disc
trace already maps to the PC's retraces. Output: "<row> <line> <offset> <label>", label = the line's start + offset;
the header line is kept. pc_dvd.c's ORRE_DVD_FRAMES reads the fourth column when it is there.
"""
import argparse, json, os, sys


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("replay")
    ap.add_argument("--out")
    a = ap.parse_args()
    starts, first = [0], None
    with open(os.path.join(a.replay, "replay_events.jsonl"), encoding="utf-8") as f:
        for line in f:
            e = json.loads(line)
            first = e["f"] if first is None else first
            if e.get("ev") == "entered":
                starts.append(e["f"])
    starts[0] = first or 0
    out = []
    with open(os.path.join(a.replay, "dvdseen.map"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#"):
                out.append(line.rstrip("\n") + " (+ the label: dvdseen_labels.py)")
                continue
            p = line.split()
            if len(p) >= 3:
                out.append(f"{p[0]} {p[1]} {p[2]} {starts[int(p[1])] + int(p[2])}")
    text = "\n".join(out) + "\n"
    if a.out:
        with open(a.out, "w", encoding="utf-8") as f:
            f.write(text)
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
