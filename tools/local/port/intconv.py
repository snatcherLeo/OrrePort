"""Integer <-> pointer conversions the PC build accepts silently (OrrePort; merge gate step, 2026-10-24).

The PC build compiles with -w, so passing a 32-bit integer where a pointer is expected (or the reverse) compiles,
links and boots. Below 4 GB on Windows it usually works; with ORRE_PTR32_ENCODED (macOS) or a debug check it doesn't,
and a signed int holding a pointer sign-extends. The Count-Sheep merge brought 64 new such sites, all invisible.

This compiles every unit firstlink.py compiles (syntax only, firstlink's flags without -w, plus -Wint-conversion) and
writes one line per distinct warning: "file<TAB>kind<TAB>message" (line numbers and argument positions left out, so
two trees compare). kind: p<-i (pointer made from an integer) or i<-p (integer made from a pointer).
Fix sites with ptr32wrap.py (PC_PTR / PC_PTR32 wrappers, GameCube-neutral), the rest by hand.

Usage (repo root, MSYS2 mingw64 on PATH; through tools/local/saferun.py):
  python -I tools/local/port/intconv.py <out file> [jobs=8]
  then, for a merge: run it on the old and the new tree and compare the two sorted files (comm -13 old new).
"""
import concurrent.futures as cf
import importlib.util
import re
import subprocess
import sys

spec = importlib.util.spec_from_file_location("fl", "port/tools/firstlink.py")
fl = importlib.util.module_from_spec(spec)
saved, sys.argv = sys.argv, ["firstlink.py"]
spec.loader.exec_module(fl)
sys.argv = saved
out = sys.argv[1]
jobs = int(sys.argv[2]) if len(sys.argv) > 2 else 8
units = [u for u in fl.units() if u[0] not in fl.SDK_UNITS]
W = re.compile(r"^(?:\./)?(src/[^:]+):\d+:\d+: warning: (.*?) \[-Wint-conversion\]")


def run(u):
    src, defs, lang = u
    cmd = (["gcc"] + [f for f in fl.FLAGS if f not in ("-c", "-w")] + ["-fsyntax-only", "-Wint-conversion", "-x",
           lang, "-std=gnu11" if lang == "c" else "-std=gnu++17"] + [f"-D{d}" for d in defs] + [src])
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    res = set()
    for line in r.stderr.splitlines():
        m = W.match(line)
        if not m:
            continue
        msg = m.group(2)
        kind = "p<-i" if "pointer from integer" in msg else "i<-p" if "integer from pointer" in msg else "other"
        res.add((m.group(1), kind, re.sub(r"argument \d+ of", "argument of", msg)))
    return res


found = set()
with cf.ThreadPoolExecutor(jobs) as ex:
    for s in ex.map(run, units):
        found |= s
with open(out, "w", encoding="utf-8") as f:
    for a, k, m in sorted(found):
        f.write(f"{a}\t{k}\t{m}\n")
print(f"{len(found)} distinct int-conversion warnings -> {out}")
