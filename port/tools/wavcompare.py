"""port/tools/wavcompare.py (B, 2026-10-17): compare two audio dumps of the same stretch of the game, e.g. the port's
ORRE_DUMP_AUDIO file against Dolphin's DSP audio dump (gamecube-accuracy.md entry 10, the golden slice).

Usage: python -I port/tools/wavcompare.py port.wav dolphin.wav [--window-ms 50]

Both files are 16-bit PCM WAV (any rate, mono or stereo). The two dumps start at different moments, so the tool first
lines them up: it compares their loudness envelopes (RMS per window) at every offset within +-30 s and takes the best
match. Then it reports, over the overlapping part: the offset, the level difference (dB, RMS), how closely the
envelopes follow each other (correlation, 1.0 = same shape), peaks and full-scale samples on each side, and the
windows where the loudness differs most (where to listen). Read-only; prints a report.
Limits: a loudness comparison, not sample by sample (the two mixers resample and pan differently); a stretch where
one side plays a sound and the other does not shows up as a large local difference, which is the point.
"""
import argparse
import array
import math
import sys
import wave


def load(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            sys.exit(f"{path}: only 16-bit PCM is supported (sample width {w.getsampwidth()})")
        rate, channels, frames = w.getframerate(), w.getnchannels(), w.getnframes()
        data = array.array("h", w.readframes(frames))
    if sys.byteorder != "little":
        data.byteswap()
    return rate, channels, data


def envelope(rate, channels, data, window_ms):
    """RMS per window over all channels, plus the peak and the full-scale count of the whole file."""
    step = max(1, rate * window_ms // 1000) * channels
    env = []
    for i in range(0, len(data) - step + 1, step):
        seg = data[i : i + step]
        env.append(math.sqrt(sum(x * x for x in seg) / len(seg)))
    peak = max((abs(x) for x in data), default=0)
    clipped = sum(1 for x in data if x >= 32767 or x <= -32768)
    return env, peak, clipped


def corr(a, b):
    n = len(a)
    if n < 2:
        return 0.0
    ma, mb = sum(a) / n, sum(b) / n
    va = sum((x - ma) ** 2 for x in a)
    vb = sum((y - mb) ** 2 for y in b)
    if va == 0 or vb == 0:
        return 0.0
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / math.sqrt(va * vb)


def db(x):
    return 20 * math.log10(x) if x > 0 else float("-inf")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("first")
    ap.add_argument("second")
    ap.add_argument("--window-ms", type=int, default=50)
    ap.add_argument("--max-offset-s", type=float, default=30.0)
    ap.add_argument("--chunk-s", type=float, default=0.0,
                    help="also align each chunk of the first file on its own (load times differ, so one offset drifts)")
    args = ap.parse_args()

    files = []
    for path in (args.first, args.second):
        rate, channels, data = load(path)
        env, peak, clipped = envelope(rate, channels, data, args.window_ms)
        files.append((path, rate, channels, len(data) // channels / rate, env, peak, clipped, len(data)))
        print(f"{path}: {rate} Hz, {channels} ch, {len(data) // channels / rate:.1f} s, peak {peak}, "
              f"full-scale samples {clipped} ({100.0 * clipped / max(1, len(data)):.4f}%)")

    a, b = files[0][4], files[1][4]
    max_shift = int(args.max_offset_s * 1000 / args.window_ms)
    best = (-2.0, 0)
    for shift in range(-max_shift, max_shift + 1):  # positive: the second file starts later in the game
        if shift >= 0:
            x, y = a[shift:], b
        else:
            x, y = a, b[-shift:]
        n = min(len(x), len(y))
        if n < 20:
            continue
        c = corr(x[:n], y[:n])
        if c > best[0]:
            best = (c, shift)
    c, shift = best
    x, y = (a[shift:], b) if shift >= 0 else (a, b[-shift:])
    n = min(len(x), len(y))
    x, y = x[:n], y[:n]
    print(f"\nalignment: the second file matches the first {shift * args.window_ms / 1000:+.2f} s in; "
          f"envelope correlation {c:.3f} over {n * args.window_ms / 1000:.1f} s")
    rms_a = math.sqrt(sum(v * v for v in x) / n) if n else 0
    rms_b = math.sqrt(sum(v * v for v in y) / n) if n else 0
    print(f"level: first {db(rms_a / 32768):.1f} dBFS, second {db(rms_b / 32768):.1f} dBFS, "
          f"difference {db(rms_a / 32768) - db(rms_b / 32768):+.1f} dB (RMS of the overlap)")
    diffs = sorted(((abs(db(max(p, 1) / 32768) - db(max(q, 1) / 32768)), i) for i, (p, q) in enumerate(zip(x, y))),
                   reverse=True)
    print("largest local differences (time in the first file, dB first / second):")
    for d, i in diffs[:8]:
        t = (i + max(shift, 0)) * args.window_ms / 1000
        print(f"  {t:7.2f} s: {db(max(x[i], 1) / 32768):6.1f} / {db(max(y[i], 1) / 32768):6.1f} (difference {d:.1f} dB)")

    if args.chunk_s > 0:
        chunk_report(files, args, shift)


def chunk_report(files, args, global_shift):
    """Each chunk of the first file aligned on its own near the global offset: level and full-scale samples per
    chunk on both sides (full-scale counts come from the raw samples of the matched stretch)."""
    (_, rate_a, ch_a, _, a, _, _, _), (_, rate_b, ch_b, _, b, _, _, _) = files
    raw_a = load(args.first)[2]
    raw_b = load(args.second)[2]
    win = args.window_ms / 1000
    size = int(args.chunk_s / win)
    search = int(1.0 / win)  # +-1 s around the global offset (load-time drift between the two runs)
    # the global shift is in "the first file's index minus the second's" form: a[i] matches b[i - global_shift]
    global_shift = -global_shift
    print(f"\nper {args.chunk_s:.0f} s chunk of the first file (own alignment within +-1 s):")
    print("   first at  | second at | corr  | RMS dB first / second (diff) | full-scale first / second")
    for start in range(0, len(a) - size + 1, size):
        seg = a[start : start + size]
        best = (-2.0, None)
        for off in range(start + global_shift - search, start + global_shift + search + 1):
            if off < 0 or off + size > len(b):
                continue
            c = corr(seg, b[off : off + size])
            if c > best[0]:
                best = (c, off)
        c, off = best
        if off is None:
            continue
        other = b[off : off + size]
        ra = math.sqrt(sum(v * v for v in seg) / size)
        rb = math.sqrt(sum(v * v for v in other) / size)
        sa = raw_a[int(start * win * rate_a) * ch_a : int((start + size) * win * rate_a) * ch_a]
        sb = raw_b[int(off * win * rate_b) * ch_b : int((off + size) * win * rate_b) * ch_b]
        fa = sum(1 for v in sa if v >= 32767 or v <= -32768)
        fb = sum(1 for v in sb if v >= 32767 or v <= -32768)
        print(f"  {start * win:7.1f} s  | {off * win:7.1f} s | {c:5.2f} | {db(ra / 32768):6.1f} / {db(rb / 32768):6.1f} "
              f"({db(ra / 32768) - db(rb / 32768):+5.1f}) | {fa:5d} / {fb:5d}")


if __name__ == "__main__":
    main()
