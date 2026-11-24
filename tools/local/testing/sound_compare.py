"""Compare the sound cues between a Dolphin replay and a PC run (2026-10-20; snatcherLeo: "sound stuff included").

    python -I tools/local/testing/sound_compare.py <Dolphin replay_K folder> <PC run log> [--all]

Both sides log the two sound slots soundPlay keeps whenever either changes: lbl_8047B0A8, the last sound effect it
started, and lbl_8047B0AC, the current music. Dolphin: the "sound" event in replay_events.jsonl (slots [effect, music],
"f" = the probe's frame label, a retrace count). PC: ORRE_SOUND_LOG=1, "[sound_slots] frame,retrace,effect,music".
Effects a battle starts straight through MusyX don't pass through soundPlay and aren't in these slots.

The two change lists are aligned in order (difflib) on the (effect, music) pair. It prints how many changes match in
order, the first place they part, and for the matched ones the retrace offset PC minus Dolphin: a steady offset means
the cues come at the same moments relative to each other; a jump between neighbours is a timing difference.

--sfx: every sound start instead, battle cries and move sounds included (the sound table's work pointers): Dolphin's
`sfx` probe (sfx.csv: frame,started,stopped) against the PC's ORRE_SFX_LOG=1 ("[sfx] frame,retrace,started,stopped").
One item per started sound id, aligned the same way.
"""
import argparse, csv, difflib, json, re

ROW = re.compile(r"^\[sound_slots\] (\d+),(\d+),(\d+),(\d+)")
SFX = re.compile(r"^\[sfx\] (\d+),(\d+),([\d ]*),([\d ]*)$")


def load_dolphin_sfx(folder):
    out = []
    with open(folder.rstrip("/\\") + "/sfx.csv", encoding="utf-8") as f:
        for r in csv.reader(f):
            if not r or r[0] == "frame" or r[0].startswith("#"):
                continue
            out += [(int(r[0]), (int(i),)) for i in r[1].split()]
    # the probe sees some starts twice on consecutive labels (the sound's work appears, then its handle changes:
    # P03's music 1094, P04's sound 7, 2026-10-27): count such a pair once. *Why (2026-11-03, verified with
    # PLAYREC_SFX_HANDLES on the reference playthrough P01):* a start sets the work with the placeholder handle 0xFFFFFFFF at frame N
    # and the real MusyX handle at N+1, and the probe's "handle changed" rule counts both. Matched per sound id now
    # (the last start of that id one frame earlier), so another sound's row in between doesn't break the pair
    deduped, last = [], {}
    for frame, ids in out:
        if last.get(ids) == frame - 1:
            last[ids] = frame
            continue
        last[ids] = frame
        deduped.append((frame, ids))
    return deduped


def load_pc_sfx(log):
    out = []
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = SFX.match(line.rstrip("\r\n"))
            if m:
                out += [(int(m.group(2)), (int(i),)) for i in m.group(3).split()]
    return out


def load_dolphin(folder):
    out = []
    with open(folder.rstrip("/\\") + "/replay_events.jsonl", encoding="utf-8") as f:
        for line in f:
            e = json.loads(line)
            if e.get("ev") == "sound":
                out.append((int(e["f"]), tuple(e["slots"])))
    return out


def load_pc(log):
    out = []
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = ROW.match(line)
            if m:
                out.append((int(m.group(2)), (int(m.group(3)), int(m.group(4)))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dolphin")
    ap.add_argument("pc_log")
    ap.add_argument("--all", action="store_true", help="list every change on both sides")
    ap.add_argument("--sfx", action="store_true", help="every sound start (sfx probe / ORRE_SFX_LOG)")
    a = ap.parse_args()
    if a.sfx:
        d, p = load_dolphin_sfx(a.dolphin), load_pc_sfx(a.pc_log)
    else:
        d, p = load_dolphin(a.dolphin), load_pc(a.pc_log)
    # the first row on each side is the start-up state (0, 0); the PC prints it at its first frame
    sm = difflib.SequenceMatcher(a=[v for _, v in d], b=[v for _, v in p], autojunk=False)
    matched = sum(b.size for b in sm.get_matching_blocks())
    print(f"changes: Dolphin {len(d)}, PC {len(p)}, matched in order {matched}")
    prev = None
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            for k in range(i2 - i1):
                fd, v = d[i1 + k]
                fp = p[j1 + k][0]
                off = fp - fd
                jump = "" if prev is None or off == prev else f"  (offset {off - prev:+d} vs previous)"
                if a.all or jump:
                    what = f"sound {v[0]}" if a.sfx else f"effect {v[0]} music {v[1]}"
                    print(f"  = D {fd:6d}  PC {fp:6d}  offset {off:+d}  {what}{jump}")
                prev = off
        else:
            print(f"  {tag}: Dolphin {[(f, v) for f, v in d[i1:i2]]}  PC {[(f, v) for f, v in p[j1:j2]]}")


if __name__ == "__main__":
    main()
