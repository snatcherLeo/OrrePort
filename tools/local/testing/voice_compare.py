"""Compare MusyX's hardware voices between a Dolphin replay and a PC run (2026-10-28; the project's second-pass sound
layer, snatcherLeo: audio checked closely).

    python -I tools/local/testing/voice_compare.py <Dolphin replay_K folder> <PC run log> [--list N] [--fields ...]

Dolphin: the `voices` probe (voices.csv: a row when a voice starts, ends or changes; "frame,retrace,what,voice,...").
PC: ORRE_SOUND_LOG=1, "[sndvoice] t=SECONDS voice=V smp=S ... pitch=0x.. vol=L/R/S auxA=L/R/S auxB=L/R/S" at each
voice start and "[sndvoice] t=SECONDS end voice=V smp=S" at each end.

Clocks: in a replay the PC renders the mix on the game thread once per retrace, so its mixer time times 59.94 runs in
step with the retraces, from an unknown start. That start is the commonest gap (Dolphin retrace minus PC retrace)
between starts of the same sample, re-estimated every --window retraces (a load that takes longer on one side moves
it; each move is listed). Each Dolphin start then pairs with the PC start of the same sample nearest the local gap,
within --tol retraces (ties broken by the fewest differing parameters). *Corrected 2026-10-28:* the first version
aligned the whole lists in order (difflib) and fitted the clock to that; over a long run it paired unrelated starts
(1.5% aligned, a slope of 10), so pairing is now by time.

For every aligned start it compares the start parameters (sample format, length, loop, voice number, studio,
volumes, aux sends, and the pitch where both sides read the same slot 0) and prints the counts and the first
differences of each field. Pitch: the PC's line prints pitch slot 0 and so does the probe's `pitch` column; the
console's current pitch (`pnow`) can sit in another slot, so a slot-0 pitch of 0 on either side isn't compared.
Since 2026-10-28 the PC line also ends in "pslot=N pnow=0x.." (pitch[lastUpdate.pitch]); when every PC start
has it, `pnow` (the current pitch, whatever slot it sits in) is compared on every pair too.
"""
import argparse, bisect, collections, csv, re, statistics

# retraces per PC mixer second, the first guess (the mix is rendered once per retrace in a replay); the real ratio is
# then fitted from the pairs: P08's partial capture drifted -1 retrace per ~1,000 at 59.94 and -2 at 60 (2026-10-28)
RATE = 59.94

PC_START = re.compile(r"^\[sndvoice\] t=([\d.]+) voice=(\d+) smp=(\d+) studio=(\d+) comp=(\d+) len=(\d+) "
                      r"loop=(\d+)\+(\d+) pitch=0x([0-9A-Fa-f]+) itd=\d+ vol=(\S+) auxA=(\S+) auxB=(\S+)")
PC_PNOW = re.compile(r" pslot=(-?\d+) pnow=0x([0-9A-Fa-f]+)")   # added by 2026-10-28 (pitch[lastUpdate.pitch])
# added by 2026-10-29: a playing voice's main / aux A volume changed (hwSetVolume, PC only)
PC_SET = re.compile(r"^\[sndvoice\] t=([\d.]+) set voice=(\d+) vol=(\S+) auxA=(\S+)")
# added by 2026-10-29: a voice's key-off (adsrRelease)
PC_REL = re.compile(r"^\[sndvoice\] t=([\d.]+) release voice=(\d+)")
PC_END = re.compile(r"^\[sndvoice\] t=([\d.]+) end voice=(\d+) smp=(\d+)")
FIELDS = ("voice", "studio", "comp", "len", "loop", "looplen", "pitch", "vol", "auxA", "auxB")


def load_dolphin(folder):
    starts, ends, starting, last_state, quiet, cur = [], [], {}, {}, {}, {}
    with open(folder.rstrip("/\\") + "/voices.csv", encoding="utf-8") as f:
        for r in csv.DictReader(f):
            if not r["frame"][:1].isdigit():
                continue
            prev = last_state.get(r["voice"])
            last_state[r["voice"]] = r["state"]
            if r["what"] == "set" and r["state"] == "1" and prev == "2":
                # a voice started again while still playing (state 2 -> 1; captures before 2026-10-28 log it as
                # "set"): the old sound ends and a new one starts here
                ends.append(dict(t=int(r["retrace"]), voice=int(r["voice"]), smp=int(r["smp"])))
                r = dict(r, what="start")
            if r["what"] == "set" and r.get("adsr", "").endswith(":4") and r["voice"] in cur \
                    and cur[r["voice"]].get("rel") is None:
                # the release (key-off): the envelope's state 4 (ADSR_STATE_RELEASE; captures from 2026-10-28 on)
                cur[r["voice"]]["rel"] = int(r["retrace"])
            if r["what"] != "start" and r["voice"] in quiet:
                # vol_on: the first row after a silent start with a main volume (None if the voice ends first)
                if r["what"] == "end":
                    quiet.pop(r["voice"])
                elif r["vol"] != "0/0/0":
                    quiet.pop(r["voice"])["vol_on"] = int(r["retrace"])
            if r["what"] == "set" and r["state"] == "2" and r["voice"] in starting:
                # a voice seen first while starting (state 1, volume often still 0) gets its parameters from the row
                # where it plays: the PC prints its line when the voice starts playing (P08: samples 731-733 start
                # at volume 0 and get it one label later)
                s = starting.pop(r["voice"])
                s.update(pitch=int(r["pitch"], 16), pnow=int(r["pnow"], 16), vol=r["vol"], auxA=r["auxA"],
                         auxB=r["auxB"], voice=int(r["voice"]))
                continue
            starting.pop(r["voice"], None)
            if r["what"] == "start":
                starts.append(dict(t=int(r["retrace"]), smp=int(r["smp"]), voice=int(r["voice"]),
                                   studio=int(r["studio"]), comp=int(r["comp"]), len=int(r["len"]),
                                   loop=int(r["loop"]), looplen=int(r["looplen"]), pitch=int(r["pitch"], 16),
                                   pnow=int(r["pnow"], 16),
                                   vol=r["vol"], auxA=r["auxA"], auxB=r["auxB"], frame=int(r["frame"])))
                if r["state"] == "1":
                    starting[r["voice"]] = starts[-1]
                starts[-1]["rel"] = int(r["retrace"]) if r.get("adsr", "").endswith(":4") else None
                cur[r["voice"]] = starts[-1]
                quiet.pop(r["voice"], None)
                if r["vol"] == "0/0/0":
                    starts[-1]["vol_on"] = None
                    quiet[r["voice"]] = starts[-1]
                else:
                    starts[-1]["vol_on"] = int(r["retrace"])
            elif r["what"] == "end":
                ends.append(dict(t=int(r["retrace"]), voice=int(r["voice"]), smp=int(r["smp"])))
    return starts, ends


def load_pc(log):
    starts, ends, sets, rels = [], [], [], collections.defaultdict(list)
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.startswith("[sndvoice]"):
                continue
            m = PC_START.match(line)
            if m:
                g = m.groups()
                starts.append(dict(t=float(g[0]), voice=int(g[1]), smp=int(g[2]), studio=int(g[3]), comp=int(g[4]),
                                   len=int(g[5]), loop=int(g[6]), looplen=int(g[7]), pitch=int(g[8], 16),
                                   vol=g[9], auxA=g[10], auxB=g[11]))
                q = PC_PNOW.search(line)
                if q:
                    starts[-1]["pnow"] = int(q.group(2), 16)
                continue
            m = PC_END.match(line)
            if m:
                ends.append(dict(t=float(m.group(1)), voice=int(m.group(2)), smp=int(m.group(3))))
                continue
            m = PC_SET.match(line)
            if m:
                sets.append((int(m.group(2)), float(m.group(1)), m.group(3)))
                continue
            m = PC_REL.match(line)
            if m:
                rels[int(m.group(2))].append(float(m.group(1)))
    # the mix runs ahead in blocks, so lines can come slightly out of time order: sort, ties by sample
    starts.sort(key=lambda s: (s["t"], s["smp"]))
    if rels:
        # rel: the voice's first release line at or after its start, before the voice's next start
        nxt = {}
        for s in reversed(starts):
            until = nxt.get(s["voice"], float("inf"))
            nxt[s["voice"]] = s["t"]
            v = rels.get(s["voice"], [])
            i = bisect.bisect_left(v, s["t"])
            s["rel"] = v[i] if i < len(v) and v[i] < until else None
    if sets:
        # vol_on: when the voice first has a main volume (its start line, or the first "set" line after it with a
        # non-zero volume, before the voice's next start); None = never while it played
        nxt = {}
        per_voice = collections.defaultdict(list)
        for v, t, vol in sets:
            per_voice[v].append((t, vol))
        for v in per_voice.values():
            v.sort()
        for s in reversed(starts):
            s["until"] = nxt.get(s["voice"], float("inf"))
            nxt[s["voice"]] = s["t"]
        for s in starts:
            if s["vol"] != "0/0/0":
                s["vol_on"] = s["t"]
                continue
            v = per_voice.get(s["voice"], [])
            # from 1/60 s before the start line: the volume often lands in the same mixer pass but prints 0-0.2
            # retraces before it (P11, 2026-10-29)
            i = bisect.bisect_left(v, (s["t"] - 1 / 60, ""))
            s["vol_on"] = None
            while i < len(v) and v[i][0] < s["until"]:
                if v[i][1] != "0/0/0":
                    s["vol_on"] = v[i][0]
                    break
                i += 1
    return starts, ends


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dolphin")
    ap.add_argument("pc_log")
    ap.add_argument("--list", type=int, default=5, help="examples shown per differing field (default 5)")
    ap.add_argument("--steps", type=int, default=15, help="gap changes listed (default 15)")
    ap.add_argument("--window", type=int, default=1000, help="retraces per gap estimate (default 1000)")
    ap.add_argument("--search", type=int, default=600, help="how far the gap may move between windows (default 600)")
    ap.add_argument("--tol", type=int, default=4, help="retraces a pair may be off the local gap (default 4)")
    a = ap.parse_args()
    ds, de = load_dolphin(a.dolphin)
    ps, pe = load_pc(a.pc_log)
    print(f"Dolphin: {len(ds)} voice starts, {len(de)} ends; PC: {len(ps)} starts, {len(pe)} ends")
    if not ds or not ps:
        return
    # PC time -> retraces at a rate fitted from a first pairing (see RATE), from an unknown start; the start (and any
    # move from loads) is found as the commonest gap between same-sample starts, per window
    by_smp = collections.defaultdict(list)
    for p in ps:
        by_smp[p["smp"]].append(p)
    for v in by_smp.values():
        v.sort(key=lambda p: p["t"])
    rate = RATE
    for attempt in range(2):
        pairs, used, offs, off = pair_all(a, ds, by_smp, rate)
        if attempt == 0 and len(pairs) > 50:
            n = len(pairs)
            mt = sum(p["t"] for _, p, _ in pairs) / n
            mr = sum(d["t"] for d, _, _ in pairs) / n
            rate = sum((p["t"] - mt) * (d["t"] - mr) for d, p, _ in pairs) / \
                sum((p["t"] - mt) ** 2 for _, p, _ in pairs)
    print(f"clock: PC retrace = t * {rate:.4f} + gap (fitted from a first pairing at {RATE}; the gap at the start "
          f"{offs[0][2] - offs[0][1] if offs else off})")
    first_t, last_t = ds[0]["t"], ds[-1]["t"]
    span_pc = [p for v in by_smp.values() for p in v if first_t - 600 <= p["r"] + off <= last_t + 600]
    print(f"paired on the sample id within {a.tol} retraces of the local gap: {len(pairs)} of {len(ds)} Dolphin "
          f"starts ({100 * len(pairs) / len(ds):.1f}%); PC starts in the same stretch not paired: "
          f"{sum(id(p) not in used for p in span_pc)}")
    paired = {id(d) for d, _, _ in pairs}
    missing = [d for d in ds if id(d) not in paired]
    for d in missing[:a.list]:
        print(f"  Dolphin only: frame {d['frame']} retrace {d['t']} smp {d['smp']} voice {d['voice']}")
    _report(a, ds, de, ps, pe, pairs, offs, rate)


def pair_all(a, ds, by_smp, rate):
    """(pairs [(dolphin start, PC start, retraces off the local gap)], ids of the PC starts used, gap moves, last gap)"""
    for v in by_smp.values():
        for p in v:
            p["r"] = p["t"] * rate
    keys = {s: [p["r"] for p in v] for s, v in by_smp.items()}

    def peak(dwin, lo, hi):
        """the commonest Dolphin-minus-PC gap (1-retrace bins) between same-sample starts, within lo..hi"""
        hist = collections.Counter()
        for d in dwin:
            ks = keys.get(d["smp"], [])
            for k in range(bisect.bisect_left(ks, d["t"] - hi), bisect.bisect_right(ks, d["t"] - lo)):
                hist[round(d["t"] - ks[k])] += 1
        if not hist:
            return None, 0
        # smooth over +-1 so a gap split between two bins still wins
        best = max(hist, key=lambda g: hist[g - 1] + hist[g] + hist[g + 1])
        return best, hist[best - 1] + hist[best] + hist[best + 1]

    off, _ = peak(ds[:400], -10 ** 7, 10 ** 7)
    pairs, used, offs, k = [], set(), [], 0
    while k < len(ds):
        win = [d for d in ds[k:] if d["t"] < ds[k]["t"] + a.window]
        g, n = peak(win, off - a.search, off + a.search)
        if g is not None and n >= max(3, len(win) // 10):
            if g != off:
                offs.append((win[0], g - off, g))
            off = g
        for d in win:
            best = None
            cand, ks = by_smp.get(d["smp"], []), keys.get(d["smp"], [])
            for i in range(bisect.bisect_left(ks, d["t"] - off - a.tol - 1), bisect.bisect_right(ks, d["t"] - off + a.tol + 1)):
                p = cand[i]
                dt = d["t"] - p["r"] - off
                if abs(dt) <= a.tol and id(p) not in used:
                    # whole retraces first, then the fewest differing parameters (several copies of one sample
                    # often start on one retrace), then the sub-retrace time
                    score = (round(abs(dt)), sum(d[f] != p.get(f) for f in ("vol", "auxA", "pnow", "voice")), abs(dt))
                    if best is None or score < best[0]:
                        best = (score, p, dt)
            if best:
                used.add(id(best[1]))
                pairs.append((d, best[1], best[2]))
        k += len(win)
    return pairs, used, offs, off


def _report(a, ds, de, ps, pe, pairs, offs, rate):
    diffs = [dt for _, _, dt in pairs]
    print(f"timing within the window (Dolphin minus PC, retraces): exact (|x| < 0.5) {sum(abs(x) < 0.5 for x in diffs)},"
          f" within 1: {sum(abs(x) <= 1 for x in diffs)}, within 2: {sum(abs(x) <= 2 for x in diffs)} of {len(diffs)}")
    print(f"the gap changed {len(offs)} times (each a stretch where the PC's sounds start earlier or later than the "
          f"console's, as from a load):")
    for d, step, g in offs[:a.steps]:
        print(f"  from Dolphin frame {d['frame']} (retrace {d['t']}): {step:+d} (now {g})")
    pairs = [(d, p) for d, p, _ in pairs]
    print("start parameters on the aligned pairs:")
    has_pnow = all("pnow" in p for _, p in pairs)
    for fld in FIELDS + (("pnow",) if has_pnow else ()):
        cmp = [(d, p) for d, p in pairs if fld != "pitch" or (d["pitch"] and p["pitch"])]
        bad = [(d, p) for d, p in cmp if d[fld] != p[fld]]
        print(f"  {fld:8s} {len(cmp) - len(bad)} of {len(cmp)} equal" + (" (pairs where both slot-0 pitches are set)"
                                                                          if fld == "pitch" else ""))
        for d, p in bad[:a.list if bad else 0]:
            show = (lambda v: f"0x{v:X}") if fld in ("pitch", "pnow") else str
            print(f"    frame {d['frame']} smp {d['smp']}: Dolphin {show(d[fld])}, PC {show(p[fld])}")
    # how long each paired voice played: its end on the same voice number after its start, on each side
    def durations(starts, ends, key):
        # the end is the voice's next end or next start, whichever comes first (a reused voice may log no end)
        by_voice = collections.defaultdict(list)
        for e in ends:
            by_voice[e["voice"]].append(e["t"])
        for s in starts:
            by_voice[s["voice"]].append(s["t"] + 1e-6)
        for v in by_voice.values():
            v.sort()
        out = {}
        for s in starts:
            # strictly after the start's own entry; an end on the start's own retrace closes the sound before it
            v = by_voice[s["voice"]]
            i = bisect.bisect_right(v, s["t"] + 1e-6)
            if i < len(v):
                out[id(s)] = v[i] - s["t"]
        return out
    dd_ = durations(ds, de, "t")
    pd_ = durations(ps, pe, "t")
    lens = [(d, dd_[id(d)], pd_[id(p)] * rate) for d, p in pairs if id(d) in dd_ and id(p) in pd_]
    if lens:
        dl = [x - y for _, x, y in lens]
        print(f"play length (start to end, Dolphin minus PC, retraces) on {len(lens)} pairs: median "
              f"{statistics.median(dl):+.2f}, within 2: {sum(abs(x) <= 2 for x in dl)}, longer by more than 10 on "
              f"Dolphin: {sum(x > 10 for x in dl)}, on the PC: {sum(x < -10 for x in dl)}")
        for d, x, y in sorted(lens, key=lambda r: -abs(r[1] - r[2]))[:a.list]:
            print(f"    frame {d['frame']} smp {d['smp']} voice {d['voice']}: Dolphin {x} retraces, PC {y:.1f}")
    # silent starts: how long until the voice first has a main volume (needs the PC's "set" lines, 2026-10-29)
    if any("vol_on" in p for _, p in pairs):
        rows = []
        for d, p in pairs:
            if d.get("vol_on") == d["t"] and p.get("vol_on") == p["t"]:
                continue
            dc = None if d.get("vol_on") is None else d["vol_on"] - d["t"]
            pc = None if p.get("vol_on") is None else (p["vol_on"] - p["t"]) * rate
            rows.append((d, dc, pc))
        print(f"pairs where a side starts at volume 0: {len(rows)} (delay to the first main volume, retraces)")
        by = collections.defaultdict(list)
        for d, dc, pc in rows:
            by[d["smp"]].append((dc, pc))
        for smp, xs in sorted(by.items(), key=lambda kv: -len(kv[1]))[:a.list * 2]:
            med = lambda v: f"{statistics.median(v):.1f}" if v else "-"
            dn = [x for x, _ in xs if x is not None]
            pn = [y for _, y in xs if y is not None]
            print(f"  smp {smp}: {len(xs)} pairs; Dolphin median {med(dn)}, never {len(xs) - len(dn)}; "
                  f"PC median {med(pn)}, never {len(xs) - len(pn)}")
    # key-off: start to release on each side (needs the PC's "release" lines and a capture with the adsr column)
    if any("rel" in p for _, p in pairs) and any(d.get("rel") is not None for d, _ in pairs):
        by = collections.defaultdict(list)
        for d, p in pairs:
            dc = None if d.get("rel") is None else d["rel"] - d["t"]
            pc = None if p.get("rel") is None else (p["rel"] - p["t"]) * rate
            by[d["smp"]].append((dc, pc))
        both = [(x, y) for xs in by.values() for x, y in xs if x is not None and y is not None]
        print(f"start to release (key-off), retraces: both sides released on {len(both)} pairs, within 2 of each "
              f"other {sum(abs(x - y) <= 2 for x, y in both)}; released on one side only: "
              f"{sum(1 for xs in by.values() for x, y in xs if (x is None) != (y is None))}")
        worst = sorted(by.items(), key=lambda kv: -sum(1 for x, y in kv[1] if x is not None and y is not None
                                                      and abs(x - y) > 2 or (x is None) != (y is None)))
        for smp, xs in worst[:a.list * 2]:
            bad = sum(1 for x, y in xs if x is not None and y is not None and abs(x - y) > 2 or (x is None) != (y is None))
            if not bad:
                break
            dn = [x for x, _ in xs if x is not None]
            pn = [y for _, y in xs if y is not None]
            med = lambda v: f"{statistics.median(v):.1f}" if v else "-"
            print(f"  smp {smp}: {len(xs)} pairs, {bad} differ; Dolphin median {med(dn)} (never {len(xs) - len(dn)}), "
                  f"PC median {med(pn)} (never {len(xs) - len(pn)})")


if __name__ == "__main__":
    main()
