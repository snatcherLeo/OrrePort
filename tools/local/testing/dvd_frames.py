"""Which game frame first saw each recorded disc read finish, from a Dolphin replay (2026-10-20; snatcherLeo's decision
(b): replay disc completions by game frame, not by retrace).

    python -I tools/local/testing/dvd_frames.py <dvd.csv> <frames replay folder> <fsys replay folder> <out.dvdframes>
        [--shift N]

Why: the console's processor takes time over each frame's work, so whether the game sees a finished read in this
frame or the next depends on how far that frame's work had got (the project notes, the exact-battle decision). Retrace times
can't carry that; the frame in which the game reacted can. The game's own reaction is visible in the archive loader:
a read's completion callback moves its slot to the next state, and the loader's next step moves it on again
(`fsys.csv`, the playrec probe `fsys`). The frame holding that second change is the first frame that saw the read.

Inputs (all from clean replays of the same sitting, so the frame labels agree): `dvd.csv` (the recorded reads the PC
replay matches, ORRE_DVD_TRACE), `frames.csv` + `replay_events.jsonl` (the game frames, split into room lines exactly
as frames_to_pc.py splits them, so line and frame numbers are the PC's), `fsys.csv`.
Output: one line per matched read, `<dvd.csv row index> <room line> <frame index in the line>` (0-based, the boot
line is line 0, as in the .frames file). --shift N moves every reaction N frames (calibration). Reads with no slot
change at their end (sound, not the archive loader) are left out: the PC keeps timing them by retrace.
"""
import argparse, bisect, csv, json, os


def frame_lines(folder):
    """[(start label of each game frame)] per room line, as frames_to_pc.py splits them"""
    rows = [l.strip().split(",") for l in open(os.path.join(folder, "frames.csv")) if l[:1].isdigit()]
    starts = []
    for line in open(os.path.join(folder, "replay_events.jsonl"), encoding="utf-8"):
        e = json.loads(line)
        if e.get("ev") == "entered":
            starts.append(e["f"])
    lines, k = [[]], -1
    for f, _r, _n in rows:
        f = int(f)
        while k + 1 < len(starts) and f >= starts[k + 1]:
            k += 1
            lines.append([])
        lines[-1].append(f)
    return lines


def slot_changes(folder):
    """archive name -> [(label, status)] in label order (every change of that slot)"""
    out, prev = {}, {}
    for line in open(os.path.join(folder, "fsys.csv"), encoding="utf-8"):
        p = line.rstrip("\n").split(",", 2)
        if len(p) != 3 or not p[0].isdigit():
            continue
        label, cur = int(p[0]), {}
        for part in p[2].split():
            bits = part.split(":", 3)
            if len(bits) == 4:
                cur[bits[3]] = int(bits[1])
        for name, st in cur.items():
            if prev.get(name) != st:
                out.setdefault(name, []).append((label, st))
        prev = cur
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dvd")
    ap.add_argument("frames_dir")
    ap.add_argument("fsys_dir")
    ap.add_argument("out")
    ap.add_argument("--shift", type=int, default=0)
    a = ap.parse_args()
    lines = frame_lines(a.frames_dir)
    flat = sorted((f, li, fi) for li, fs in enumerate(lines) for fi, f in enumerate(fs))
    labels = [x[0] for x in flat]
    changes = slot_changes(a.fsys_dir)
    # every slot change, by label, to find the slot a read's end moved
    by_label = {}
    for name, ch in changes.items():
        for i, (lab, st) in enumerate(ch):
            by_label.setdefault(lab, []).append((name, i))
    out, matched = [], 0
    rows = list(csv.DictReader(open(a.dvd, encoding="utf-8")))
    for idx, r in enumerate(rows):
        end = int(r["end_frame"])
        hit = None
        for d in (0, 1, -1, 2):  # the probe sees the callback's change at the read's end label, give or take its poll
            for name, i in by_label.get(end + d, []):
                if i + 1 < len(changes[name]):
                    hit = (name, i)
                    break
            if hit:
                break
        if not hit:
            continue
        name, i = hit
        react = changes[name][i + 1][0]
        k = bisect.bisect_right(labels, react) - 1
        if k < 0:
            continue
        _f, li, fi = flat[k]
        out.append(f"{idx} {li} {max(0, fi + a.shift)}  # {name} end {end} reaction {react}")
        matched += 1
    with open(a.out, "w", encoding="utf-8") as f:
        f.write(f"# dvd_frames.py: {matched} of {len(rows)} recorded reads with the frame that first saw them finish\n")
        f.write("\n".join(out) + "\n")
    print(f"{a.out}: {matched} of {len(rows)} recorded reads mapped to a reaction frame")


if __name__ == "__main__":
    main()
