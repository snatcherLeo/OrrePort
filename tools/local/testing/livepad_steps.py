"""Turn a PC recording into event-tied steps for Dolphin's live pad (2026-11-02; the reference playthrough's Dolphin side, the closed loop).

    python -I tools/local/testing/livepad_steps.py <recording.orrepad> <pc run.log> <out.json>

Every change of the PC's port-1 pad is tied to the last PC event before it (a room entered, a message window opening or
closing, a battle message: the PC log's [slice] entered, [ev] msgwin and [bmsg] lines) with its delay in game frames
after that event. dolphin_playrec.py (PLAYREC_LIVEPAD) then fires each step that many frames after Dolphin shows the
same event, so the presses follow what Dolphin does, not a precomputed movie: P01's movie passes didn't converge
because Dolphin is ready for a press later than the PC after a window opens (likely).

out.json: {"anchors": [[kind, key, pc frame], ...], "steps": [[anchor index, delay, pad hex], ...], "boot": [...]}
(boot: [pc frame, pad hex] for changes before the first event; pad hex = the 10 latched PADStatus bytes: buttons (2),
stick x, y, C-stick x, y (s8), triggers, analog A / B (0: not in the recording)).
"""
import bisect
import json
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port"))
from orrepad_to_pad import read_recording  # noqa: E402

ENTER = re.compile(r"\[slice\] entered 0x([0-9A-Fa-f]+) frame=(\d+)")
MSG = re.compile(r"\[ev\] frame=(\d+) msgwin type=(-?\d+)")


def main():
    rec, log, out = sys.argv[1:4]
    anchors, menu_was = [], {}
    for line in open(log, encoding="utf-8", errors="replace"):
        m = ENTER.search(line)
        if m:
            anchors.append(("room", int(m.group(1), 16), int(m.group(2))))
            continue
        m = MSG.search(line)
        if m:
            anchors.append(("window", int(m.group(2)), int(m.group(1))))
            continue
        if line.startswith("[bmsg] "):
            f, _, mid = line[7:].strip().split(",")[:3]
            anchors.append(("battle message", int(mid), int(f)))
            continue
        if line.startswith("[menu] "):
            # "[menu] frame,retrace,id:phase:count:mode:flags:base:offset ..." (ORRE_MENU_LOG): a window's phase and
            # its cursor (base + offset, when it has more than one choice), each when it changes; dolphin_playrec's
            # menu probe makes the same events (P01: a stick move timed from the room entry came before Dolphin's
            # menu was ready, likely, and the cursor ended elsewhere)
            parts = line[7:].strip().split(",", 2)
            f = int(parts[0])
            now = {}
            for ent in (parts[2].split() if len(parts) > 2 else []):
                v = [int(x) for x in ent.split(":")]
                now[(v[0], "p")] = v[1]
                if v[2] > 1:
                    now[(v[0], "c")] = v[5] + v[6]
            for (wid, what), val in sorted(now.items()):
                if menu_was.get((wid, what)) != val:
                    anchors.append(("menu", "%d:%s%d" % (wid, what, val), f))
            menu_was.clear()
            menu_was.update(now)
    anchors.sort(key=lambda a: a[2])
    frames = [a[2] for a in anchors]
    _, records, _ = read_recording(rec, with_epoch=True)
    state = {}
    for r in records:
        button, sx, sy, cx, cy, lt, rt, err = r[3:11]
        # the Y axes negated: dolphin_playrec's pad_dict takes the game's latched copy, whose Y runs the other way
        # from the PC's recorded PADStatus (P01's third live run: Wes walked -x / -z on the PC and -x / +z in Dolphin
        # from the same spot, likely this)
        neg = lambda v: max(-127, min(127, -v)) & 255
        raw = bytes(10) if err != 0 else bytes([button >> 8, button & 255, sx & 255, neg(sy), cx & 255, neg(cy),
                                                 lt, rt, 0, 0])
        state[r[2]] = raw            # the state each game frame ends with
    steps, boot, prev = [], [], None
    for g in sorted(state):
        if state[g] == prev:
            continue
        prev = state[g]
        # the last event strictly before this change: a change in an event's own frame is input the game read in
        # that frame, so it caused the event rather than answered it (P02: the stick push that moves the box cursor
        # to 1 has its 48 the frame before and its 94 / 127 in the cursor change's own frame; filed under the change,
        # Dolphin waited for the change those values cause). *Changed 2026-11-03*
        k = bisect.bisect_left(frames, g) - 1
        if k < 0:
            boot.append([g, state[g].hex()])
        else:
            steps.append([k, g - frames[k], state[g].hex()])
    # Wes's path (the PC log's "[ev] frame=N pos x y z", every 10 frames), each sample tied to its anchor like the
    # steps, with the turn from stick to world while he moves (the angle of his movement over the next 10 frames minus
    # the angle of the PC's stick then; the camera's doing). dolphin_playrec steers by it on walks: long walks drift
    # by a few units in Dolphin and miss the person talked to (P01 runs 6-7: the talk at the PC's 48,384 never came)
    import math
    pos = [(int(m.group(1)), float(m.group(2)), float(m.group(4))) for m in
           (re.match(r"\[ev\] frame=(\d+) pos (\S+) (\S+) (\S+)", line)
            for line in open(log, encoding="utf-8", errors="replace")) if m]
    frames_s = sorted(state)

    def stick_at(f):
        i = bisect.bisect_right(frames_s, f) - 1
        if i < 0:
            return 0, 0
        raw = state[frames_s[i]]
        return (raw[2] ^ 0x80) - 0x80, -((raw[3] ^ 0x80) - 0x80)   # the PC's own sx, sy (the steps hold -sy)

    path = []
    for (f, x, z), nxt in zip(pos, pos[1:] + [None]):
        k = bisect.bisect_right(frames, f) - 1
        if k < 0:
            continue
        rot = None
        if nxt is not None and nxt[0] - f <= 12:
            dx, dz = nxt[1] - x, nxt[2] - z
            sx, sy = stick_at(f + 5)
            if math.hypot(dx, dz) > 0.5 and math.hypot(sx, sy) > 20:
                rot = math.atan2(dz, dx) - math.atan2(sy, sx)
        path.append([k, f - frames[k], round(x, 3), round(z, 3), None if rot is None else round(rot, 4)])
    json.dump({"anchors": [list(a) for a in anchors], "steps": steps, "boot": boot, "path": path}, open(out, "w"))
    print(f"{out}: {len(anchors)} anchors ({sum(a[0] == 'room' for a in anchors)} rooms, "
          f"{sum(a[0] == 'window' for a in anchors)} windows, {sum(a[0] == 'battle message' for a in anchors)} battle "
          f"messages, {sum(a[0] == 'menu' for a in anchors)} menu changes), {len(steps)} steps, {len(boot)} before the "
          "first")


if __name__ == "__main__":
    main()
