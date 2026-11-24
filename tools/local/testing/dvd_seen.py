"""When the console's game first sees each disc read finish, per dvd.csv row (2026-10-21; the project's option (b) for
battle exactness: the PC hands a read's completion over at the start of the frame the console's game noticed it).

    python -I tools/local/testing/dvd_seen.py <replay folder> [--out FILE]

Reads from the replay folder (a `playrec.py replay N --probe dvd,frames,dvdseen` run). Use the replay whose dvd.csv
replay_check passes as ORRE_DVD_TRACE (the newest with one): the row index is the PC's trace entry index.
- dvd.csv: the recorded reads (start_frame, disc_offset, length, ...), one row each;
- dvdseen.csv: dolphin_playrec.py's dvdseen probe, per FSYS slot: the command block (offset, length, state) at the
  frame's end and `polled`, slot +0x54, the status this frame's poll (fsysUpdateTask) saw, about the read the slot
  held at the end of the previous frame. Rows come when anything changes and once more the frame after;
- replay_events.jsonl: the "entered" events, which split the frames into lines exactly as frames_to_pc.py does (line 0
  = boot, before the first entry; line k starts at the k-th entry).

Per slot, a read is seen in frame F when F's poll returned 0 (done) and the read was the slot's block at the end of
F - 1 (the latest row before F). Each dvd.csv row (in order) takes the first unused sighting with the same disc offset
and length at or after its start_frame. Output, one line per dvd.csv row that was seen: "<row> <line> <frame>", row =
the dvd.csv data row (0-based), frame = the recorder's frame labels since that line's first label (the entry is 0;
line 0 counts from the replay's first label). *Corrected 2026-10-21:* those labels count retraces (one per
frame-advance callback), not game frames, so in a 30 fps room the value is twice the game-frame count; the PC converts
it with the room's recorded frame lengths (pc_frame_times_frame_of_vi). The units stay retraces. Reads outside the FSYS slots (sound banks, other GSdvd reads) aren't seen by
this probe: they are left out and counted on stderr.
"""
import argparse
import csv
import json
import os
import sys
from collections import defaultdict


# fsysDVDReadCallback's status moves (fsys_file_r56_8017F108_suffix.c): a read issued in status s completes into CB[s]
CB = {1: 2, 3: 4, 101: 150, 200: 201, 303: 304, 400: 401}


def sightings(path, kinds=False):
    """[(frame, offset, length)] (with kinds=True: (frame, offset, length, kind)) for every read the game first saw
    done, in frame order. kind "poll": an armed slot (archive data set), whose fsysUpdateTask poll (+0x54) returned 0 in
    that frame for the read the slot held at the end of the previous frame (verified mechanism). kind "callback" (rows
    with the status column, 2026-10-21): an unarmed slot's read (header, table of contents), which completes through
    fsysDVDReadCallback: seen in the first frame whose end shows the handler acted, i.e. the slot holds another read or
    its status left both the issuing status and the callback's target (likely: a handler that acts without a visible
    change would make this late)."""
    rows = defaultdict(list)
    for r in csv.DictReader(open(path, encoding="utf-8")):
        if r["frame"][:1].isdigit():
            status = int(r["status"]) if r.get("status") not in (None, "") else None
            armed = int(r["armed"]) if r.get("armed") not in (None, "") else 1
            rows[int(r["slot"])].append((int(r["frame"]), int(r["offset"], 16), int(r["length"]), int(r["polled"]),
                                         status, armed))
    out = []
    for slot_rows in rows.values():
        held, held_seen = None, True     # the slot's block at the end of the previous row's frame
        held_armed, held_status = 1, None
        for f, off, length, polled, status, armed in slot_rows:
            if held is not None and not held_seen:
                if held_armed and armed and polled == 0:
                    out.append((f, held[0], held[1], "poll"))
                    held_seen = True
                elif not held_armed and held_status is not None and (
                        (off, length) != held or status not in (held_status, CB.get(held_status))):
                    out.append((f, held[0], held[1], "callback"))
                    held_seen = True
            if (off, length) != held:
                if held is not None and held[1] == 64 and off > held[0] + 0x40:
                    # a header followed directly by a read past the table of contents: the table read (archive
                    # +0x40 up to that read) was issued, finished and seen inside this one frame, between two frame
                    # ends (P01, Growl's wzx_nakigoe_attack: header at +0, body at +224, its 160-byte table never
                    # visible). Inferred (likely): seen in the frame its handler issued the next read
                    out.append((f, held[0] + 0x40, off - (held[0] + 0x40), "inferred"))
                held, held_seen, held_armed, held_status = (off, length), False, armed, status
    out.sort()
    return out if kinds else [x[:3] for x in out]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("replay")
    ap.add_argument("--out")
    ap.add_argument("--sightings", metavar="FILE",
                    help="also write every sighting, recorded or not (2026-10-21: reads the per-frame dvd capture "
                         "never recorded, matched on the PC by disc offset + length + order): '<offset hex> <length> "
                         "<line> <frame in line> <label> <kind> <dvd.csv row or ->'")
    a = ap.parse_args()
    reads = [r for r in csv.DictReader(open(os.path.join(a.replay, "dvd.csv"), encoding="utf-8"))]
    seen_k = sightings(os.path.join(a.replay, "dvdseen.csv"), kinds=True)
    seen = [x[:3] for x in seen_k]
    starts, first = [0], None
    for line in open(os.path.join(a.replay, "replay_events.jsonl"), encoding="utf-8"):
        e = json.loads(line)
        first = e["f"] if first is None else first
        if e.get("ev") == "entered":
            starts.append(e["f"])
    starts[0] = first or 0
    used = [None] * len(seen)
    out, missing = [], 0

    def place(f):
        line = max(k for k, s in enumerate(starts) if s <= f) if f >= starts[0] else 0
        return line, f - starts[line]

    for i, r in enumerate(reads):
        off, length, start = int(r["disc_offset"], 16), int(r["length"]), int(r["start_frame"])
        hit = next((k for k, (f, o, n) in enumerate(seen) if used[k] is None and f >= start and o == off
                    and n == length), None)
        if hit is None:
            missing += 1
            continue
        used[hit] = i
        line, rel = place(seen[hit][0])
        out.append(f"{i} {line} {rel}")
    kinds = {k: sum(1 for x in seen_k if x[3] == k) for k in ("poll", "callback", "inferred")}
    head = (f"# dvd_seen.py {os.path.basename(os.path.normpath(a.replay))}: <dvd.csv row> <line> <frame in line> "
            f"(frames are recorder labels: retraces); {len(out)} of {len(reads)} recorded reads seen, {missing} not "
            f"seen; {sum(1 for u in used if u is None)} sightings matched no recorded read; sightings by the poll "
            f"{kinds['poll']}, through the callback {kinds['callback']}, inferred table reads {kinds['inferred']}")
    if a.sightings:
        lines = [f"# dvd_seen.py {os.path.basename(os.path.normpath(a.replay))} sightings: <disc offset> <length> "
                 "<line> <frame in line> <label> <kind> <dvd.csv row or ->; frames are recorder labels (retraces)"]
        for (f, o, n, kind), row in zip(seen_k, used):
            line, rel = place(f)
            lines.append(f"0x{o:x} {n} {line} {rel} {f} {kind} {'-' if row is None else row}")
        open(a.sightings, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    text = "\n".join([head] + out) + "\n"
    if a.out:
        open(a.out, "w", encoding="utf-8").write(text)
    else:
        sys.stdout.write(text)
    print(head, file=sys.stderr)


if __name__ == "__main__":
    main()
