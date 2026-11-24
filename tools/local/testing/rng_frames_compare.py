"""PC vs Dolphin RNG draws frame by frame, from a shared anchor (testing lane; golden slice).

    python -I tools/local/testing/rng_frames_compare.py <Dolphin run folder> <PC run.log> [--anchor "seed 468546624"]
                                                         [--frames 600]

The anchor is a pad-script step both sides fire (default: the battle seed). Frame numbers differ between the sides
(the PC loads faster), so frames are counted from the anchor's frame on each side.
- Dolphin: slice.jsonl's "rng" events (dolphin_slice.py with SLICE_RNG_RANGE covering the stretch): draws per frame.
- PC: run.log with ORRE_RNG_TRACE=1 from a build that prints "[rngtrace] frame N" (2026-10-18): draw lines
  "[rngtrace] <n> <addr> <state>" counted per frame; the caller lines "[rngtrace] GSrand / randf / randpm from <addr>"
  name who called the wrappers.
Prints both sides' draws per frame from the anchor and stops at the first frame whose count differs, listing that
frame's PC draws and callers (decode with addr2line on the PC exe). The anchor frame itself is skipped on both sides
(a seed written in it makes Dolphin's step count meaningless there).
"""
import argparse, collections, json, os, re


def dolphin_frames(folder, anchor):
    ev = [json.loads(l) for l in open(os.path.join(folder, "slice.jsonl"))]
    start = next(e["frame"] for e in ev if e["ev"] in ("step", "command") and anchor in e["step"])
    per = {}
    for e in ev:
        if e["ev"] == "rng" and e["frame"] > start and e.get("steps") not in (None, -1):
            per[e["frame"] - start] = e["steps"]
    return start, per


def pc_frames(log, anchor):
    frame_re = re.compile(r"\[rngtrace\] frame (\d+)")
    step_re = re.compile(r"\[pad_script\] frame (\d+): step \d+ (.*)")
    start, cur = None, None
    per = collections.Counter()
    detail = collections.defaultdict(list)
    for line in open(log, encoding="utf-8", errors="replace"):
        if start is None:
            m = step_re.search(line)
            if m and anchor in m.group(2):
                start = int(m.group(1))
            continue
        m = frame_re.search(line)
        if m:
            cur = int(m.group(1)) - start
            continue
        if line.startswith("[rngtrace]") and cur is not None and cur > 0:
            parts = line.split()
            if len(parts) > 3 and parts[2] == "from":
                detail[cur].append(f"{parts[1]} from {parts[3]}")
            elif len(parts) > 2 and parts[1].isdigit():
                per[cur] += 1
                detail[cur].append(f"draw at {parts[2]} -> {parts[3] if len(parts) > 3 else ''}")
    return start, per, detail


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dolphin")
    ap.add_argument("pclog")
    ap.add_argument("--anchor", default="seed 468546624")
    ap.add_argument("--frames", type=int, default=600)
    a = ap.parse_args()
    ds, d = dolphin_frames(a.dolphin, a.anchor)
    ps, p, detail = pc_frames(a.pclog, a.anchor)
    if ps is None:
        print("anchor not found in the PC log")
        return
    print(f"anchor '{a.anchor}': Dolphin frame {ds}, PC frame {ps}")
    td = tp = 0
    for f in range(1, a.frames + 1):
        dn, pn = d.get(f, 0), p.get(f, 0)
        td += dn
        tp += pn
        if dn != pn:
            print(f"FIRST DIFFERENCE at anchor + {f} (Dolphin frame {ds + f}, PC frame {ps + f}): "
                  f"Dolphin {dn} draws, PC {pn} (totals {td} / {tp})")
            for line in detail.get(f, [])[:40]:
                print("   ", line)
            prev = [x for x in range(max(1, f - 3), f)]
            print("   previous frames, Dolphin / PC:", [(x, d.get(x, 0), p.get(x, 0)) for x in prev])
            return
    print(f"no difference in {a.frames} frames (totals {td} / {tp})")


if __name__ == "__main__":
    main()
