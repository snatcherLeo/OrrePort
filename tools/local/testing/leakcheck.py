"""2026-11-05 (port-gaps 67; snatcherLeo: "check for CPU and RAM leaks and stuff like that too"); moved from the
session's scratch folder into the repo 2026-11-06. The verdict on one leakwatch.ps1 sample file.

FAIL when the game's own GPU memory passes 3 GB or its RAM (private bytes) 4 GB, or when GPU, RAM or handles grow by
more than 25% from the first third of the run to the last third (a steady climb). The growth test skips the first
60 s (boot and the first room's loads read as 27-85% growth on 60 s replays, 2026-11-05) and needs 12 samples after
it (about 3 minutes at the default 10 s interval); the caps apply to every sample. Exit code 1 on FAIL.
    python -I tools/local/testing/leakcheck.py <samples.txt>"""
import sys

GPU_CAP_MIB = 3072
RAM_CAP_MIB = 4096
GROWTH_LIMIT = 0.25
SKIP_S = 60
MIN_STEADY = 12


def main(path):
    rows = []
    with open(path, encoding="ascii", errors="replace") as f:
        for line in f:
            p = line.split()
            if not p or p[0].startswith("#") or len(p) < 8 or p[1] == "-1":
                continue
            rows.append([float(x) for x in p[:8]])
    if len(rows) < 2:
        print(f"leakcheck: only {len(rows)} samples: too short to judge")
        return 0
    t, gpu, card, priv, ws, cpu, handles, threads = zip(*rows)
    steady = [r for r in rows if r[0] >= SKIP_S]
    judge_growth = len(steady) >= MIN_STEADY
    third = max(len(steady) // 3, 1)

    def grow(col):
        first = max(col[:third])
        last = max(col[-third:])
        return first, last, (last - first) / first if first else 0.0

    bad = []
    for k, name, col, cap in ((1, "GPU", gpu, GPU_CAP_MIB), (3, "RAM", priv, RAM_CAP_MIB), (6, "handles", handles, None)):
        peak = max(col)
        if cap and peak > cap:
            bad.append(f"{name} peak {peak:.0f} MiB over {cap}")
        if judge_growth:
            first, last, g = grow([r[k] for r in steady])
            if g > GROWTH_LIMIT:
                bad.append(f"{name} grew {g * 100:.0f}% after start-up ({first:.0f} -> {last:.0f})")
    note = "" if judge_growth else " (growth not judged: under 3 minutes)"
    print(f"leakcheck: {len(rows)} samples over {t[-1]:.0f} s; GPU peak {max(gpu):.0f} MiB, RAM peak {max(priv):.0f} MiB, "
          f"handles peak {max(handles):.0f}, CPU {cpu[-1]:.0f} s: " + ("FAIL: " + "; ".join(bad) if bad else "PASS") + note)
    return 1 if bad else 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    sys.exit(main(sys.argv[1]))
