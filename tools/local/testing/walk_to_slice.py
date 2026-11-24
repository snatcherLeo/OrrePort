"""Turn a recorded Dolphin walk into the next part of a golden-slice pad script (testing lane).

    python -I tools/local/testing/walk_to_slice.py <walk folder> <out.pad> --after 60 [--stop-room 0xC8]

The walk folder holds dolphin_walk.py's walk_pad.jsonl (one line per pad change: frame, pad) and idlog's ids.jsonl
("entered" lines: rooms entered, PC-style, with the same frame count). The walk started from a state the previous
slice part saved (dolphin_slice.py, SLICE_SAVE) --after frames after its last step fired (SLICE_END), so walk frame k
is that many frames, plus k - 1, after the previous part's last step.

Output steps follow B's pad-script rules (port/src/pc_pad_script.c), as the PC and dolphin_slice.py read them: the
first step after a room entry is "@R+N" (N frames after R is entered), every other step "+N" after the previous one.
With --stop-room R, the walk is cut where room R is entered (e.g. 0xC8, Willie's arena), so the battle's own steps
(the seed at the waiting command menu and the recorded presses) can follow. Pad values: Dolphin's StickX / StickY
become SX= / SY= (same 0-255 values), buttons by name; an empty pad is "-".
"""
import argparse, json, os

NAMES = {"A": "A", "B": "B", "X": "X", "Y": "Y", "Z": "Z", "L": "L", "R": "R", "Start": "START",
         "Up": "UP", "Down": "DOWN", "Left": "LEFT", "Right": "RIGHT"}


LATCHED_Y = False   # --latched-y: the recording's Y is the game's stored (flipped) copy; flip it to the raw convention


def what(pad):
    toks = []
    for k, v in pad.items():
        if k == "StickX":
            if int(v) != 128:
                toks.append(f"SX={int(v)}")
        elif k == "StickY":
            y = (256 - int(v)) & 255 if LATCHED_Y else int(v)
            if y != 128:
                toks.append(f"SY={y}")
        elif v and k in NAMES:
            toks.append(NAMES[k])
    return "+".join(toks) if toks else "-"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("walk")
    ap.add_argument("out")
    ap.add_argument("--after", type=int, required=True, help="frames from the previous part's last step to the saved state")
    ap.add_argument("--stop-room", type=lambda s: int(s, 0))
    ap.add_argument("--prefix", help="a pad file to put first (the previous slice parts)")
    ap.add_argument("--suffix", help="a pad file to put last (e.g. the battle steps)")
    ap.add_argument("--latched-y", action="store_true",
                    help="the recording's StickY is the game's flipped stored copy (dolphin_record.py before the "
                         "2026-10-17 fix, e.g. rec_2): flip it to the raw controller convention the pad script uses")
    a = ap.parse_args()
    global LATCHED_Y
    LATCHED_Y = a.latched_y
    pads = [json.loads(l) for l in open(os.path.join(a.walk, "walk_pad.jsonl"))]
    entries = [json.loads(l) for l in open(os.path.join(a.walk, "ids.jsonl"))]
    # only the walk's own entries (the folder's ids.jsonl may also hold the previous slice part's, in its own frames)
    entries = [(e["id"], e["frame"]) for e in entries if e.get("kind") == "entered" and e.get("run") == "walk"]
    stop = next((f for r, f in entries if r == a.stop_room), None) if a.stop_room is not None else None

    def slice_frame(k):          # frames after the previous part's last step
        return a.after + (k - 1)

    lines = [f"# slice part from {os.path.basename(os.path.normpath(a.walk))} (walk_to_slice.py; --after {a.after})"]
    prev = 0                     # the previous step, in slice frames (the previous part's last step = 0)
    used_entry = None
    last_what = None
    for p in pads:
        k = p["frame"]
        if stop is not None and k >= stop:
            break
        w = what(p["pad"])
        if w == last_what:
            continue
        s = slice_frame(k)
        ent = [(r, slice_frame(f)) for r, f in entries if prev < slice_frame(f) <= s]
        if ent and ent[-1] != used_entry:
            room, e = ent[-1]
            lines.append(f"@0x{room:X}+{s - e}:{w}")
            used_entry = ent[-1]
        else:
            lines.append(f"+{s - prev}:{w}")
        prev, last_what = s, w
    if stop is not None:
        lines.append(f"# cut here: room 0x{a.stop_room:X} entered (walk frame {stop}); the battle steps follow")
        if last_what and last_what != "-":
            # release whatever the walk held at the cut, so it isn't held into the next part. Plain "+3", and the next
            # part times its first step from it ("+N", not "@R+N"): the room entry may land before or after the walk's
            # last press depending on load timing, and "@R+N" only matches an entry at or after the previous step
            # (an "@R+0" release has the same problem on a side that enters the room earlier; 2026-10-17)
            lines.append("+3:-")
    text = "\n".join(lines) + "\n"
    if a.prefix:
        text = open(a.prefix).read().rstrip("\n") + "\n" + text
    if a.suffix:
        text = text + open(a.suffix).read()
    open(a.out, "w").write(text)
    print(a.out, len(lines) - 1, "steps")


if __name__ == "__main__":
    main()
