"""Cut a clip from a Dolphin audio dump by game frame (2026-11-07; P12 notes 7 and 8).

    python -I tools/local/testing/audio_clip.py <dspdump.wav> <from frame> <to frame> <out.wav> [--fps 59.94] [--offset F]

A replay run with PLAYREC_DUMPAUDIO=1 writes <replay>/user/Dump/Audio/dspdump.wav from boot, in emulated time; frame f
of the logger is f / fps seconds into it (minus --offset frames if the dump starts later than the logger's frame 0).
Prints the clip's peak and RMS loudness (dBFS) per channel, for comparing two clips' levels by number as well as by ear.
"""
import argparse
import array
import math
import wave

p = argparse.ArgumentParser()
p.add_argument("wav")
p.add_argument("start", type=int)
p.add_argument("end", type=int)
p.add_argument("out")
p.add_argument("--fps", type=float, default=60000 / 1001)
p.add_argument("--offset", type=int, default=0)
a = p.parse_args()

with wave.open(a.wav, "rb") as w:
    rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
    s0 = max(0, int((a.start - a.offset) / a.fps * rate))
    s1 = min(w.getnframes(), int((a.end - a.offset) / a.fps * rate))
    w.setpos(s0)
    data = w.readframes(max(0, s1 - s0))
with wave.open(a.out, "wb") as o:
    o.setnchannels(ch)
    o.setsampwidth(width)
    o.setframerate(rate)
    o.writeframes(data)
if width == 2 and data:
    x = array.array("h", data)
    for c in range(ch):
        v = x[c::ch]
        peak = max(abs(s) for s in v) or 1
        rms = math.sqrt(sum(s * s for s in v) / len(v)) or 1
        print("channel %d: peak %.1f dBFS, RMS %.1f dBFS" % (c, 20 * math.log10(peak / 32768), 20 * math.log10(rms / 32768)))
print("%s: %.2f s at %d Hz, frames %d-%d" % (a.out, (s1 - s0) / rate, rate, a.start, a.end))
