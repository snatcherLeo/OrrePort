"""The console's logic passes at fades -> the PC replay's frame-times file (2026-10-26; the project's candidate 2, option a).

    python -I tools/local/testing/frames_logic.py <in.frames> <frames replay dir> <fade replay dir> <out.frames>

Why: on the console the fade-in into some rooms (P01: 0x3A1, 0x390, 0x38E, 0x391; P02 / P12: 0x38F, 0x39D) holds its
first step or two. The steps probe (P01 replay_38) shows the frame step stays one retrace and drawing stays on
schedule, but the logic pass that advances the fade (mainRetraceLogic -> menuDaemon -> fadeDaemon) skips a retrace and
later runs twice in one. The PC replay ran those fades straight through, so every room after was 1-2 retraces early
(P02: -2 per world-map visit). gamecube-accuracy.md, "Logic passes at fade-ins".

How: a running fade adds (frame step / 60) per logic pass, and the step is the previous frame's length (the logic pass
runs before the frame's retrace wait), so within a fade each console frame's logic passes are its elapsed advance
divided by the previous frame's length (fade.csv, frames.csv; any replay of the sitting, they are label for label the
same). The frames file (frames_to_pc.py, one entry per console frame) is rewritten per logic pass where that count
isn't 1: a frame with no pass hands its retraces to the next pass, written `1*<len>/<step>` (longer, same step), and a
frame with two passes gets an extra `1*0/<step>` (no wait). port/src/pc_frame_times.c reads the step and registers
OrreHook_FrameStep / OrreHook_FrameSkipWait only when one is present. Labelled verified where checked: P01's 0x3A1
fade-in (hold at +1, +2, double at +4); the rule for other fades is likely.
"""
import bisect
import csv
import json
import os
import sys


def read_frames(path):
    """.frames -> header lines, [(room text, [(len, step or 0) per frame])]"""
    head, lines = [], []
    for ln in open(path, encoding="utf-8"):
        t = ln.split()
        if not t or t[0].startswith("#"):
            head.append(ln.rstrip("\n"))
            continue
        frames = []
        for tok in t[1:]:
            c, _, rest = tok.partition("*")
            n, _, s = rest.partition("/")
            frames += [(int(n), int(s) if s else 0)] * int(c)
        lines.append((t[0], frames))
    return head, lines


def rle(frames):
    out, prev, count = [], None, 0
    for f in frames + [None]:
        if f == prev:
            count += 1
            continue
        if prev is not None:
            out.append(f"{count}*{prev[0]}" + (f"/{prev[1]}" if prev[1] else ""))
        prev, count = f, 1
    return " ".join(out)


def main():
    listing = "--list" in sys.argv  # print every console frame whose logic passes aren't 1 (label, length, passes)
    args = [x for x in sys.argv[1:] if x != "--list"]
    if len(args) != 4:
        raise SystemExit(__doc__)
    src, frep, fade_rep, dst = args
    head, lines = read_frames(src)
    rows = [(int(r["frame"]), int(r["frameRetraces"])) for r in csv.DictReader(open(os.path.join(frep, "frames.csv")))]
    starts = [e["f"] for e in map(json.loads, open(os.path.join(frep, "replay_events.jsonl"), encoding="utf-8"))
              if e.get("ev") == "entered"]
    # the same split as frames_to_pc.py: a boot line, then one line per "entered" event
    per_line, k = [[]], -1
    for f, n in rows:
        while k + 1 < len(starts) and f >= starts[k + 1]:
            k += 1
            per_line.append([])
        per_line[-1].append((f, n))
    if [len(x) for x in per_line] != [len(fr) for _, fr in lines] or \
            any(n != fl[0] for x, (_, fr) in zip(per_line, lines) for (_, n), fl in zip(x, fr)):
        print(f"[frames_logic] {src} doesn't line up with {frep}/frames.csv: copied unchanged")
        open(dst, "w", encoding="utf-8").write(open(src, encoding="utf-8").read())
        return
    fade = [(int(r["frame"]), int(r["mode"]), int(r["pending"]), int(r["type"]), float(r["duration"]),
             float(r["elapsed"])) for r in csv.DictReader(open(os.path.join(fade_rep, "fade.csv")))
            if r["frame"][:1].isdigit()]
    flabels = [x[0] for x in fade]

    def state(label):
        i = bisect.bisect_right(flabels, label) - 1
        return fade[i] if i >= 0 else None

    def passes(fp, f, n):
        """logic passes in the console frame (fp, f] of n retraces, or None when no fade tells"""
        a, b = state(fp), state(f)
        if a is None or b is None or not (a[2] == 1 and b[2] == 1 and a[1] in (1, 2, 3) and a[3] == b[3]):
            return None
        if b[5] >= b[4] or b[5] < a[5]:
            return None  # the fade's end (clamped) or a restart
        i, j = bisect.bisect_right(flabels, fp), bisect.bisect_right(flabels, f)
        if any(fade[m][2] != 1 or fade[m][5] < fade[m - 1][5] for m in range(max(i, 1), j)):
            return None
        adv = round((b[5] - a[5]) * 60)
        return adv // n if n and adv % n == 0 else None

    out, total = head + [], {"held": 0, "double": 0}
    prev_label, prev_n = None, None
    for (room, fr), rws in zip(lines, per_line):
        new, carry, held, dbl = [], 0, 0, 0
        for (f, n), (ln, st) in zip(rws, fr):
            # fadeDaemon runs in the logic pass, before this frame's retrace wait, so its step is the previous frame's
            # length (P02's world map: a 2-retrace frame after a 1-retrace one advances the fade by 1/60, verified)
            p = passes(prev_label, f, prev_n) if prev_label is not None else None
            if listing and p not in (None, 1):
                print(f"  {room} label {f}: length {n} (step {prev_n}), {p} logic passes")
            prev_label, prev_n = f, n
            if p is None or p == 1:
                new.append((ln + carry, st or (n if carry else 0)) if carry else (ln, st))
                carry = 0
            elif p == 0:
                carry += ln
                held += 1
            else:
                new.append((ln + carry, n))
                new += [(0, n)] * (p - 1)
                carry = 0
                dbl += p - 1
        if carry:
            if new:  # a hold at the line's end: the last pass takes the retraces, keeping its own step
                new[-1] = (new[-1][0] + carry, new[-1][1] or new[-1][0])
            else:
                new.append((carry, 0))
        total["held"] += held
        total["double"] += dbl
        if held or dbl:
            print(f"[frames_logic] {room}: {held} held, {dbl} extra logic passes")
        out.append(f"{room} {rle(new)}")
    open(dst, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(f"[frames_logic] {dst}: {total['held']} held and {total['double']} extra logic passes at fades")


if __name__ == "__main__":
    main()
