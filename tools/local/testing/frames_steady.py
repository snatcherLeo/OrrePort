"""Steady pacing for a PC replay's frame-times file: drop the console's load hitches (2026-10-19; the project's ask).

    python -I tools/local/testing/frames_steady.py <in.frames> <out.frames> [--max-frames N]

Why: a frame-times line (frames_to_pc.py) replays the console's frame lengths by frame count from the room entry. Most
of it is steady pacing (runs of 1 or 2 retraces per frame: 60 or 30 fps), but it also holds single long frames, e.g.
`1*6`, where the console stalled while something loaded (a menu, a model). Played by count, such a stall lands
wherever the PC happens to be at that frame number, not where the load is; and the PC's own load model (ORRE_DVD_TRACE,
ORRE_CARD_TRACE) already adds the real wait at the real moment. A guess for snatcherLeo's "kept pausing" in 0x391's party
menu (not checked by a run).

What it does: in each line, a run of at most N frames (default 1) whose frame length is longer than both neighbouring
runs' (a spike) takes the previous run's length (the next run's at the start of a line); then equal neighbouring runs
are merged. The number of game frames per line never changes, only how long those few frames last. Comment lines and
the room keys are kept. Prints each line's dropped retraces.
"""
import argparse


def parse(tok):
    c, n = tok.split("*")
    return [int(c), int(n)]


def steady(runs, max_frames):
    """runs: [[count, retraces], ...] -> (new runs, retraces removed)"""
    out = [list(r) for r in runs]
    removed = 0
    for i, (c, n) in enumerate(out):
        prev = out[i - 1][1] if i > 0 else None
        nxt = runs[i + 1][1] if i + 1 < len(runs) else None
        if c > max_frames or (prev is None and nxt is None):
            continue
        if (prev is None or n > prev) and (nxt is None or n > nxt):
            new = prev if prev is not None else nxt
            removed += c * (n - new)
            out[i][1] = new
    merged = []
    for c, n in out:
        if merged and merged[-1][1] == n:
            merged[-1][0] += c
        else:
            merged.append([c, n])
    return merged, removed


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--max-frames", type=int, default=1, help="longest run counted as a hitch (default 1 frame)")
    a = ap.parse_args()
    out = []
    for line in open(a.src, encoding="utf-8").read().splitlines():
        if not line.strip() or line.startswith("#"):
            out.append(line)
            continue
        key, *toks = line.split()
        runs, removed = steady([parse(t) for t in toks], a.max_frames)
        out.append(key + " " + " ".join(f"{c}*{n}" for c, n in runs))
        if removed:
            print(f"{key}: {removed} retraces of hitches dropped")
    out.insert(1 if out and out[0].startswith("#") else 0,
               f"# frames_steady.py: single long frames (load hitches, runs of at most {a.max_frames} frame(s)) dropped")
    open(a.dst, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(f"{a.dst}: written")


if __name__ == "__main__":
    main()
