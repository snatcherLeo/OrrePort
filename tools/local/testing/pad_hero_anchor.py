"""Re-time a replay pad script's presses from the player's appearance in each room (2026-10-19).

    python -I tools/local/testing/pad_hero_anchor.py <in.pad> <replay folder> <out.pad> [--max-wait N]

Why: the pad script (dtm_to_pad.py) times each room's presses from the room's entry (the floor context switch). The
PC builds a room a few frames faster or slower than the console even with the recorded disc times, and in sitting 2's
Phenac the PC created Wes 8 frames later than the console: every press after that landed early, his walk drifted off
the recorded path and the A press for the first conversation missed. Counting from the player's appearance removes
the room-building time from the presses.

Input: the replay folder of a Dolphin replay with `--probe pos` (pos.csv: frame, room, x, y, z, ...; the probe) and
its replay_events.jsonl ("entered" events, the same moments as the PC's room hook). For each room entry where the
player's position becomes valid within --max-wait frames (default 300) of the entry, the first step at or after that
moment becomes "@0x1<room>+M" (M = frames after the appearance) and later steps keep their "+N" timing. The PC side
(pc_debug_game.c pc_hero_watch_frame -> pc_pad_script_hero_appeared) reports the same moment as entry 0x10000 + room.
Rooms with no appearance in that window (menus, the world map, battles) keep their entry timing.
"""
import csv
import json
import os
import re
import sys


def appearances(replay, max_wait):
    entries = [(e["f"], e["room"]) for e in map(json.loads, open(os.path.join(replay, "replay_events.jsonl"),
                                                                    encoding="utf-8")) if e.get("ev") == "entered"]
    valid = {}
    for r in csv.DictReader(open(os.path.join(replay, "pos.csv"), encoding="utf-8")):
        valid[int(r["frame"])] = r["x"] not in ("", "None")
    out = []  # (entry index, room, frames from the entry to the appearance) for each entry
    for i, (f, room) in enumerate(entries):
        end = entries[i + 1][0] if i + 1 < len(entries) else f + max_wait + 1
        seen_absent = not valid.get(f, False)
        hit = None
        for k in range(f, min(end, f + max_wait + 1)):
            if not valid.get(k, False):
                seen_absent = True
            elif seen_absent:
                hit = k - f
                break
        out.append((room, hit))
    return out


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    max_wait = 300
    if "--max-wait" in sys.argv:
        max_wait = int(sys.argv[sys.argv.index("--max-wait") + 1])
        args = [a for a in args if a != str(max_wait)]
    if len(args) != 3:
        raise SystemExit(__doc__)
    src, replay, dst = args
    apps = appearances(replay, max_wait)
    lines = open(src, encoding="utf-8").read().splitlines()
    out, entry, since, pending = [], -1, 0, None
    anchored = []
    for line in lines:
        m = re.match(r"@(0x[0-9A-Fa-f]+)\+(\d+):(.*)$", line)
        if m:
            entry += 1
            since = int(m.group(2))
            room, hit = apps[entry] if entry < len(apps) else (None, None)
            pending = (room, hit) if hit is not None and room == int(m.group(1), 16) else None
            out.append(line)
            continue
        m = re.match(r"\+(\d+):(.*)$", line)
        if m and entry >= 0:
            since += int(m.group(1))
            if pending is not None and since >= pending[1]:
                room, hit = pending
                out.append("@0x%X+%d:%s" % (0x10000 | room, since - hit, m.group(2)))
                anchored.append((hex(room), hit, since - hit))
                pending = None
                continue
        out.append(line)
    open(dst, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print("%s: %d room(s) re-timed from the player's appearance: %s" % (dst, len(anchored), ", ".join(
        "%s (appears at +%d, first press +%d after)" % a for a in anchored)))


if __name__ == "__main__":
    main()
