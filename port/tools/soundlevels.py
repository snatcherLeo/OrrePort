"""port/tools/soundlevels.py (B, 2026-10-18): per-sound loudness, the port against Dolphin, in dB.

Usage: python -I port/tools/soundlevels.py PC.wav PC_LOG DOLPHIN.wav [--window-s 0.6] [--max-offset-s 20]
                                          [--min-gap-s 0.15] [--csv out.csv]

PC.wav is the port's ORRE_DUMP_AUDIO file of a run with ORRE_SOUND_LOG=1, PC_LOG that run's log (it holds
"[audio] dump starts at mixer t=..." and one "[sndfx] t=... fx=N vol=V pan=P ..." line per effect the game started).
DOLPHIN.wav is Dolphin's DSP audio dump of the same route (ideally DSP LLE, the exact mode).

How it measures (read-only, prints a report):
- Lines the two dumps up by their loudness envelopes (10 ms RMS) over +-max-offset, then refines the offset for each
  sound locally (+-120 ms), since the two runs drift by a few frames.
- For each effect start: the power in the window after it (window-s) minus the power just before it (the 300 ms
  before, i.e. the music and anything already playing), so the number is the sound's own contribution. Reported in
  dBFS (0 = full scale; -20 is a tenth of full-scale amplitude). Also the window's peak.
- "diff" = PC minus Dolphin: negative means the PC plays that sound quieter. A difference above about 1 dB is audible
  for a loud sound; under 0.5 dB is measurement noise.
- Per effect id, the mean difference over its plays, then the stretches with no effect starting (mostly music).
Limits: overlapping sounds share a window (marked "overlap"); a sound much quieter than the music under it gives an
unreliable contribution (marked "weak").
"""
import argparse
import array
import math
import re
import sys
import wave


def load(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            sys.exit(f"{path}: only 16-bit PCM is supported")
        rate, channels, frames = w.getframerate(), w.getnchannels(), w.getnframes()
        data = array.array("h", w.readframes(frames))
    if sys.byteorder != "little":
        data.byteswap()
    # per-frame power (mean of the channels' squares), as a prefix sum for fast window sums
    prefix = array.array("d", [0.0])
    total = 0.0
    n = len(data) // channels
    for i in range(n):
        s = 0.0
        for c in range(channels):
            x = data[i * channels + c]
            s += x * x
        total += s / channels
        prefix.append(total)
    return {"rate": rate, "channels": channels, "data": data, "prefix": prefix, "frames": n, "path": path}


def window_power(f, start_s, length_s):
    rate = f["rate"]
    a = max(0, int(round(start_s * rate)))
    b = min(f["frames"], int(round((start_s + length_s) * rate)))
    if b <= a:
        return None
    return (f["prefix"][b] - f["prefix"][a]) / (b - a)


def window_peak(f, start_s, length_s):
    rate, ch, data = f["rate"], f["channels"], f["data"]
    a = max(0, int(start_s * rate)) * ch
    b = min(f["frames"], int((start_s + length_s) * rate)) * ch
    return max((abs(x) for x in data[a:b]), default=0)


def dbfs_power(p):
    return 10 * math.log10(p / (32768.0 * 32768.0)) if p and p > 0 else float("-inf")


def envelope(f, step_s):
    rate = f["rate"]
    step = max(1, int(rate * step_s))
    pre = f["prefix"]
    return [math.sqrt(max(0.0, (pre[i + step] - pre[i]) / step)) for i in range(0, f["frames"] - step, step)]


def correlation(xa, xb):
    ma, mb = sum(xa) / len(xa), sum(xb) / len(xb)
    num = sum((x - ma) * (y - mb) for x, y in zip(xa, xb))
    da = math.sqrt(sum((x - ma) ** 2 for x in xa))
    db_ = math.sqrt(sum((y - mb) ** 2 for y in xb))
    return num / (da * db_) if da > 0 and db_ > 0 else -1.0


def best_offset(ea, eb, max_shift):
    """the shift k (b index = a index + k) with the highest correlation of the two whole envelopes"""
    best, best_k = -2.0, 0
    for k in range(-max_shift, max_shift + 1):
        i0, i1 = max(0, -k), min(len(ea), len(eb) - k)
        if i1 - i0 < 20:
            continue
        c = correlation(ea[i0:i1], eb[i0 + k : i1 + k])
        if c > best:
            best, best_k = c, k
    return best_k, best


def local_shift(ea, eb, a, b, k, radius):
    """around a[a:b], the extra shift j (|j| <= radius) on top of the global k that lines the stretch up best"""
    best, best_j = -2.0, 0
    for j in range(-radius, radius + 1):
        i0, i1 = max(a, -(k + j)), min(b, len(ea), len(eb) - (k + j))
        if i1 - i0 < 20:
            continue
        c = correlation(ea[i0:i1], eb[i0 + k + j : i1 + k + j])
        if c > best:
            best, best_j = c, j
    return best_j


def parse_log(path):
    start = None
    events = []
    rx = re.compile(r"\[sndfx\] t=([\d.]+) fx=(\d+) vol=(\d+) pan=(\d+) studio=(\d+) vid=0x([0-9A-Fa-f]+)")
    with open(path, encoding="utf-8", errors="replace") as fh:
        for line in fh:
            if start is None:
                m = re.search(r"\[audio\] dump starts at mixer t=([\d.]+)", line)
                if m:
                    start = float(m.group(1))
            m = rx.search(line)
            if m and m.group(6).upper() != "FFFFFFFF":
                events.append({"t": float(m.group(1)), "fx": int(m.group(2)), "vol": int(m.group(3)),
                               "pan": int(m.group(4))})
    if start is None:
        sys.exit(f"{path}: no '[audio] dump starts at mixer t=' line (run with ORRE_DUMP_AUDIO and ORRE_SOUND_LOG)")
    for e in events:
        e["t"] -= start
    return events


def contribution(f, t, win):
    """the sound's own power: the window after t minus the 300 ms before it; with the before and after levels"""
    after = window_power(f, t, win)
    before = window_power(f, t - 0.35, 0.3)
    if after is None or before is None:
        return None, None, None
    return max(after - before, 0.0), after, before


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pc_wav")
    ap.add_argument("pc_log")
    ap.add_argument("dolphin_wav")
    ap.add_argument("--window-s", type=float, default=0.6)
    ap.add_argument("--max-offset-s", type=float, default=20.0)
    ap.add_argument("--min-gap-s", type=float, default=0.15, help="effects closer than this count as one")
    ap.add_argument("--csv")
    ap.add_argument("--offset-guess-s", type=float, default=0.0,
                    help="expected Dolphin time minus PC time; the search covers this +- max-offset")
    ap.add_argument("--from-s", type=float, default=0.0, help="only effects from this PC dump time on")
    ap.add_argument("--to-s", type=float, default=1e9, help="only effects up to this PC dump time")
    args = ap.parse_args()

    pc, dol = load(args.pc_wav), load(args.dolphin_wav)
    events = [e for e in parse_log(args.pc_log) if args.from_s <= e["t"] <= args.to_s]
    if not events:
        sys.exit("no effect starts in the log")
    # coarse (100 ms) over the whole +-max-offset, then fine (10 ms) within +-0.3 s of it: a whole-file search at
    # 10 ms takes far too long in plain Python for dumps of several minutes
    # Only the measured stretch (--from-s .. --to-s, plus 2 s each side) is lined up: two runs' room loads differ, so
    # the offset drifts over a whole slice. Shifts searched: guess +- max-offset (--offset-guess-s: Dolphin - PC)
    def align(step, lo_s, hi_s, center_k, radius):
        ea, eb = envelope(pc, step), envelope(dol, step)
        a, b = max(0, int(lo_s / step)), min(len(ea), int(hi_s / step))
        best_c, best_k = -2.0, center_k
        for k in range(center_k - radius, center_k + radius + 1):
            i0, i1 = max(a, -k), min(b, len(eb) - k)
            if i1 - i0 < 20:
                continue
            cc = correlation(ea[i0:i1], eb[i0 + k : i1 + k])
            if cc > best_c:
                best_c, best_k = cc, k
        return best_k, best_c, ea, eb

    first = min(e["t"] for e in events)
    last = max(e["t"] for e in events)
    lo_s, hi_s = max(0.0, first - 2.0), last + args.window_s + 2.0
    coarse = 0.1
    kc, _, _, _ = align(coarse, lo_s, hi_s, int(round(args.offset_guess_s / coarse)), int(args.max_offset_s / coarse))
    step = 0.01
    k, c, epc, edol = align(step, lo_s, hi_s, int(round(kc * coarse / step)), 30)
    offset = k * step
    print(f"PC {args.pc_wav}: {pc['frames'] / pc['rate']:.1f} s at {pc['rate']} Hz; Dolphin {args.dolphin_wav}: "
          f"{dol['frames'] / dol['rate']:.1f} s at {dol['rate']} Hz")
    print(f"alignment: Dolphin time = PC time {offset:+.2f} s (envelope correlation {c:.3f})")
    if c < 0.5:
        print("WARNING: weak alignment: the two dumps may not cover the same stretch")

    rows = []
    for i, e in enumerate(events):
        t = e["t"]
        overlap = any(abs(o["t"] - t) < args.min_gap_s and o is not e for o in events) or any(
            0 < o["t"] - t < args.window_s for o in events
        )
        # local refinement around this sound: +-120 ms on a 1.5 s stretch
        lk = local_shift(epc, edol, int(max(0.0, t - 0.5) / step), int((t + 1.0) / step), k, 12)
        td = t + offset + lk * step
        p_pc, after_pc, before_pc = contribution(pc, t, args.window_s)
        p_dol, after_dol, before_dol = contribution(dol, td, args.window_s)
        if p_pc is None or p_dol is None:
            continue
        weak = p_pc < 0.25 * before_pc or p_dol < 0.25 * before_dol
        row = {
            "t": t, "fx": e["fx"], "vol": e["vol"], "pan": e["pan"],
            "pc_db": dbfs_power(p_pc), "dol_db": dbfs_power(p_dol),
            "pc_peak": window_peak(pc, t, args.window_s), "dol_peak": window_peak(dol, td, args.window_s),
            "flags": ",".join(x for x, on in (("overlap", overlap), ("weak", weak)) if on),
        }
        row["diff"] = row["pc_db"] - row["dol_db"]
        rows.append(row)

    print()
    print(f"{'time':>8} {'fx':>5} {'vol':>4} {'pan':>4} {'PC dB':>7} {'Dol dB':>7} {'diff':>6} {'PC pk':>6} {'Dol pk':>6} flags")
    for r in rows:
        print(f"{r['t']:8.2f} {r['fx']:5d} {r['vol']:4d} {r['pan']:4d} {r['pc_db']:7.1f} {r['dol_db']:7.1f} "
              f"{r['diff']:+6.1f} {r['pc_peak']:6d} {r['dol_peak']:6d} {r['flags']}")

    print("\nper effect id (clean plays only: no overlap, not weak):")
    by = {}
    for r in rows:
        if not r["flags"] and math.isfinite(r["diff"]):
            by.setdefault(r["fx"], []).append(r["diff"])
    for fx in sorted(by):
        d = by[fx]
        print(f"  fx {fx:5d}: {len(d)} play(s), PC {sum(d) / len(d):+.1f} dB vs Dolphin (range {min(d):+.1f} .. {max(d):+.1f})")

    # stretches with no effect in the window: mostly music
    quiet = []
    times = sorted(e["t"] for e in events)
    span = min(hi_s, pc["frames"] / pc["rate"], dol["frames"] / dol["rate"] - offset)
    s = max(lo_s, 1.0)
    while s + 2.0 < span:
        if not any(s - 0.5 < t < s + 2.5 for t in times):
            p1, p2 = window_power(pc, s, 2.0), window_power(dol, s + offset, 2.0)
            if p1 and p2:
                quiet.append(dbfs_power(p1) - dbfs_power(p2))
        s += 2.0
    if quiet:
        quiet.sort()
        print(f"\nstretches without effects (2 s each, mostly music): {len(quiet)}, PC {sum(quiet) / len(quiet):+.1f} dB "
              f"vs Dolphin (median {quiet[len(quiet) // 2]:+.1f}, range {quiet[0]:+.1f} .. {quiet[-1]:+.1f})")

    if args.csv:
        with open(args.csv, "w", encoding="utf-8") as fh:
            fh.write("time_s,fx,vol,pan,pc_db,dolphin_db,diff_db,pc_peak,dolphin_peak,flags\n")
            for r in rows:
                fh.write(f"{r['t']:.3f},{r['fx']},{r['vol']},{r['pan']},{r['pc_db']:.2f},{r['dol_db']:.2f},"
                         f"{r['diff']:.2f},{r['pc_peak']},{r['dol_peak']},{r['flags']}\n")


if __name__ == "__main__":
    main()
