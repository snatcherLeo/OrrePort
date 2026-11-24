"""Level and clipping of a 16-bit PCM WAV (testing lane; gamecube-accuracy.md entry 10, the PC vs console mix).

    python -I tools/local/testing/wav_level.py <file.wav> [--from SEC] [--to SEC]

Prints the format, length, peak, the number of samples at full scale (|s| >= 32767), the overall RMS level in dBFS
and the loudest one-second window (RMS), for a Dolphin audio dump (dspdump.wav / dtkdump.wav) or the PC's
ORRE_DUMP_AUDIO file. Standard library only.
"""
import argparse, math, struct, wave


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--from", dest="t0", type=float, default=0.0)
    ap.add_argument("--to", dest="t1", type=float, default=None)
    a = ap.parse_args()
    with wave.open(a.wav, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        if width != 2:
            raise SystemExit(f"{a.wav}: {8 * width}-bit samples; only 16-bit PCM is supported")
        w.setpos(min(n, int(a.t0 * rate)))
        end = n if a.t1 is None else min(n, int(a.t1 * rate))
        data = w.readframes(max(0, end - w.tell()))
    s = struct.unpack(f"<{len(data) // 2}h", data)
    frames = len(s) // ch
    peak = max((abs(x) for x in s), default=0)
    full = sum(1 for x in s if x >= 32767 or x <= -32767)
    sq = sum(x * x for x in s)
    rms = math.sqrt(sq / len(s)) if s else 0.0
    db = lambda v: 20 * math.log10(v / 32768) if v > 0 else float("-inf")
    best, best_t = 0.0, 0
    step = rate * ch
    for i in range(0, len(s) - step + 1, step):
        r = math.sqrt(sum(x * x for x in s[i:i + step]) / step)
        if r > best:
            best, best_t = r, i // ch / rate + a.t0
    print(f"{a.wav}: {ch} ch, {rate} Hz, {frames / rate:.1f} s ({frames} frames, {len(s)} samples)")
    print(f"peak {peak} ({db(peak):.2f} dBFS); full-scale samples {full}; RMS {db(rms):.2f} dBFS; "
          f"loudest second at {best_t:.0f} s: {db(best):.2f} dBFS")


if __name__ == "__main__":
    main()
