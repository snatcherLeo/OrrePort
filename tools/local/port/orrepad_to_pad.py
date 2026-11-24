r"""Turn a sitting's input recording (.orrepad, docs/port/pad-recording.md) into a pad script the PC build replays
(ORRE_PAD_SCRIPT), for the reference playthrough's crash recovery (Part 1 item 3, 2026-11-01).

    python -I tools/local/port/orrepad_to_pad.py <recording.orrepad> <out.pad> [--until-frame F] [--info]

Each record becomes a step "F:STATE" at the game frame F it was first read in (absolute frames, the pad script's
count); when several records fall in one game frame the last one is kept (the state the frame ends with). Controller
1 only: pad scripts drive port 1 (Colosseum's story mode reads only it; Battle Mode's other players are not replayed).
Analog A / B pressure is not in the pad script's vocabulary and is left out (the buttons carry A and B).

--until-frame F: steps only up to game frame F. The replay then hands the controller back to the player 10 frames after
its last step (pc_pad_script.c's hand-over), which is the crash-recovery use: replay to just before the crash, then
play on. --info prints the header and a summary only.
"""
import argparse
import struct
import sys

BUTTONS = (  # dolphin/pad.h bits, in the pad script's names
    (0x0100, "A"), (0x0200, "B"), (0x0400, "X"), (0x0800, "Y"), (0x0010, "Z"), (0x0040, "L"), (0x0020, "R"),
    (0x1000, "START"), (0x0008, "UP"), (0x0004, "DOWN"), (0x0001, "LEFT"), (0x0002, "RIGHT"),
)


def read_recording(path, with_epoch=False):
    data = open(path, "rb").read()
    if data[:8] not in (b"ORREPAD1", b"ORREPAD2"):
        sys.exit(f"{path}: not an .orrepad file")
    head, size = struct.unpack_from("<II", data, 8)
    epoch = struct.unpack_from("<Q", data, 16)[0] if data[:8] == b"ORREPAD2" else 0  # the replay clock's start
    stamp = data[(24 if data[:8] == b"ORREPAD2" else 16):head].split(b"\0", 1)[0].decode("utf-8", "replace")
    if size < 60:
        sys.exit(f"{path}: record size {size}, from before the game-frame column (2026-11-01); record again")
    records = []
    for off in range(head, len(data) - size + 1, size):
        read, retrace, frame = struct.unpack_from("<III", data, off)
        button, sx, sy, cx, cy, lt, rt = struct.unpack_from("<HbbbbBB", data, off + 12)
        err = struct.unpack_from("<b", data, off + 12 + 10)[0]
        records.append((read, retrace, frame, button, sx, sy, cx, cy, lt, rt, err))
    return (stamp, records, epoch) if with_epoch else (stamp, records)


def state(button, sx, sy, cx, cy, lt, rt):
    tokens = [name for bit, name in BUTTONS if button & bit]
    for name, v in (("SX", sx), ("SY", sy), ("CX", cx), ("CY", cy)):
        if v != 0:
            tokens.append(f"{name}={v + 128}")
    for name, v in (("LT", lt), ("RT", rt)):
        if v != 0:
            tokens.append(f"{name}={v}")
    return "+".join(tokens) if tokens else "-"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("recording")
    ap.add_argument("out", nargs="?")
    ap.add_argument("--until-frame", type=int, help="steps only up to this game frame (then the player takes over)")
    ap.add_argument("--info", action="store_true", help="print the header and a summary only")
    a = ap.parse_args()

    stamp, records, epoch = read_recording(a.recording, with_epoch=True)
    last = records[-1] if records else None
    print(f"{a.recording}: build '{stamp}', {len(records)} records"
          + (f", last at read {last[0]}, game frame {last[2]}" if last else "")
          + (f"; replay with ORRE_VIRTUAL_EPOCH={epoch} (the clock's start)" if epoch else "; no replay-clock start"))
    if a.info:
        return
    if a.out is None:
        sys.exit("give an output .pad file (or --info)")
    by_frame = {}
    for r in records:
        if r[10] != 0:  # port 1 not connected yet (err): leave the pad centred
            continue
        if a.until_frame is not None and r[2] > a.until_frame:
            break
        by_frame[r[2]] = state(*r[3:10])  # the last record of a frame wins
    lines = [f"# from {a.recording} (build '{stamp}') by tools/local/port/orrepad_to_pad.py"
             + (f", up to game frame {a.until_frame}" if a.until_frame is not None else "")]
    if epoch:
        lines.append(f"# replay with ORRE_VIRTUAL_EPOCH={epoch} and the sitting's starting save (its launch backup)")
    prev = None
    for frame in sorted(by_frame):
        if by_frame[frame] != prev:
            lines.append(f"{frame}:{by_frame[frame]}")
            prev = by_frame[frame]
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    print(f"{a.out}: {len(lines) - 1} steps")


if __name__ == "__main__":
    main()
