"""Where do the PC's RNG draws first differ from Dolphin's? Compares draws per route step (testing lane; golden slice).

    python -I tools/local/testing/rng_steps_compare.py <Dolphin run folder> <PC run.log> [--from "seed 468546624"]

Both sides fire the same pad-script steps in the same order (dolphin_slice.py / pc_pad_script.c), so the steps are a
shared clock even though the frame numbers differ (the PC loads faster). From the step whose text contains --from on:
- Dolphin: slice.jsonl's "rng" events (SLICE_RNG_RANGE, the generator steps since the previous frame) summed between
  consecutive step / command events;
- PC: run.log's "[rngtrace] n caller state" lines (the project's ORRE_RNG_TRACE) counted between consecutive "[pad_script]
  frame N: step K" lines.
Prints the draws per step interval on both sides and stops at the first difference, listing that interval's PC callers
(addresses; decode with addr2line on the PC exe). Dolphin's count is per frame, so a draw in the frame a step fires may
fall on either side of the step: a difference of the same size in the next interval with the opposite sign is that,
not a real difference (the tool says so).
"""
import argparse, collections, json, os, re


def dolphin_intervals(folder, start):
    ev = [json.loads(l) for l in open(os.path.join(folder, "slice.jsonl"))]
    out, on, n, last, seeded = [], False, 0, None, None
    for e in ev:
        if e["ev"] in ("step", "command"):
            if on:
                out.append((last, n))
            if not on and start in e["step"]:
                on = True
            if on:
                n, last = 0, e["step"]
            if e["ev"] == "command" and "seed" in e["step"]:
                seeded = e["frame"]
        elif e["ev"] == "rng" and on and e.get("steps") not in (None, -1):
            if e["frame"] == seeded:
                continue   # the seed was written in this frame's callback: "steps" spans the write, not draws
            n += e["steps"]
    return out


def pc_intervals(log, start):
    out, on, n, last, callers = [], False, 0, None, collections.Counter()
    step_re = re.compile(r"\[pad_script\] frame (\d+): step (\d+) (.*)")
    for line in open(log, encoding="utf-8", errors="replace"):
        m = step_re.search(line)
        if m:
            if on:
                out.append((last, n, callers))
            if not on and start in line:
                on = True
            if on:
                n, last, callers = 0, m.group(3).strip(), collections.Counter()
        elif on and line.startswith("[rngtrace]"):
            parts = line.split()
            if len(parts) > 2 and parts[1] == "GSrand" and parts[2] == "from":
                # "[rngtrace] GSrand from <caller>": names GSrand's caller, not a draw (added to the project's trace later)
                callers["GSrand<-" + parts[3]] += 1
            elif len(parts) > 2 and parts[1].isdigit():
                n += 1
                callers[parts[2]] += 1
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dolphin")
    ap.add_argument("pclog")
    ap.add_argument("--from", dest="start", default="seed 468546624")
    ap.add_argument("--all", type=int, default=0, help="list this many intervals without stopping at a difference")
    a = ap.parse_args()
    d = dolphin_intervals(a.dolphin, a.start)
    p = pc_intervals(a.pclog, a.start)
    print(f"intervals: Dolphin {len(d)}, PC {len(p)} (from the step containing '{a.start}')")
    total_d = total_p = 0
    for i, ((dtext, dn), (ptext, pn, callers)) in enumerate(zip(d, p)):
        total_d += dn
        total_p += pn
        mark = "" if dn == pn else "   <-- differs"
        print(f"{i:3d}  Dolphin {dn:5d}  PC {pn:5d}  (totals {total_d} / {total_p})  after: {dtext}{mark}")
        if a.all:
            if i + 1 >= a.all:
                break
            if dn != pn:
                print("       PC callers: " + ", ".join(f"{c} {addr}" for addr, c in callers.most_common(6)))
            continue
        if dn != pn:
            if total_d == total_p:
                print("     totals equal again: a draw fell on the other side of a step (per-frame sampling), not a real difference")
                continue
            print("     PC callers in this interval:")
            for addr, c in callers.most_common(12):
                print(f"       {c:5d} {addr}")
            nxt = i + 1
            if nxt < min(len(d), len(p)) and d[nxt][1] - p[nxt][1] == pn - dn:
                print("     (the next interval differs by the opposite amount: likely per-frame sampling; continuing)")
                continue
            break


if __name__ == "__main__":
    main()
