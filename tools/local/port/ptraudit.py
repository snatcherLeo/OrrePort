"""Integer-vs-pointer audit of the canonical PC prototypes (OrrePort stage 2).

Compiles every src/game and src/hsd unit with the PC compiler (-fpermissive, so int <-> pointer mix-ups are warnings
instead of errors) and collects, for each function in include/pc/prototypes.h:
  - arguments: "passing argument N of 'f' makes integer from pointer" (prototype says integer, caller passes a pointer)
    and "... makes pointer from integer" (the reverse);
  - return values: an assignment / initialisation / return warned as int <-> pointer whose right-hand side calls f.
Each finding must then be checked against the function's definition and the disc (what the target really passes).
Nothing is changed. Usage (repo root, MSYS2 mingw64 on PATH): python -I tools/local/port/ptraudit.py [--only DIR]
"""
import collections
import concurrent.futures as cf
import re
import subprocess
import sys
from pathlib import Path

only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
protos = {}
for line in Path("include/pc/prototypes.h").read_text(encoding="utf-8").splitlines():
    m = re.match(r"^(\w[\w\s\*]*?)\s*\b(\w+)\((.*)\);", line)
    if m:
        protos[m.group(2)] = line.strip()
cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
units = []
for c in cmds:
    m = re.search(r" -c (\S+) -o \S+", c)
    if "mwcceppc.exe" not in c or not m:
        continue
    src = m.group(1).replace("\\", "/")
    if src.startswith(("src/game", "src/hsd")) and (not only or src.startswith(only)):
        lang = "c++" if re.search(r"-lang[ =]c\+\+", c) or src.endswith((".cpp", ".cp")) else "c"
        units.append((src, re.findall(r"(?<=\s)-D(\S+)", c), lang))


def run(u):
    src, defs, lang = u
    cmd = ["gcc", "-fsyntax-only", "-fpermissive", "-fmax-errors=0", "-DTARGET_PC=1", "-include", "include/pc/prototypes.h",
           "-Iinclude", "-Ibuild/GC6E01/include", "-I.", "-Iextern/aurora/include", "-x", lang,
           "-std=gnu11" if lang == "c" else "-std=gnu++17", "-Wint-conversion", "-Wint-to-pointer-cast",
           "-Wpointer-to-int-cast"] + [f"-D{d}" for d in defs] + [src]
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace").stderr.splitlines()


args = collections.defaultdict(collections.Counter)   # name -> Counter((argN, direction))
rets = collections.defaultdict(collections.Counter)   # name -> Counter(direction)
where = collections.defaultdict(set)
argwhere = collections.defaultdict(set)  # (name, argN, direction) -> sites
seen = set()
lines_cache = {}
with cf.ThreadPoolExecutor(16) as ex:
    for out in ex.map(run, units):
        for l in out:
            m = re.match(r"^(.+?):(\d+):\d+: (?:warning|error): (.*)$", l)
            if not m or (m.group(1), m.group(2), m.group(3)) in seen:
                continue
            seen.add((m.group(1), m.group(2), m.group(3)))
            f, n, msg = m.group(1).replace("\\", "/").lstrip("./"), int(m.group(2)), m.group(3)
            d = "int<-ptr" if "integer from pointer" in msg or "pointer-to-int" in msg or "from pointer to integer" in msg else \
                "ptr<-int" if "pointer from integer" in msg or "int-to-pointer" in msg or "from integer to pointer" in msg else None
            if not d:
                continue
            a = re.search(r"passing argument (\d+) of '(\w+)'", msg)
            if a and a.group(2) in protos:
                args[a.group(2)][(int(a.group(1)), d)] += 1
                argwhere[(a.group(2), int(a.group(1)), d)].add(f"{f}:{n}")
                where[a.group(2)].add(f"{f}:{n}")
                continue
            if f not in lines_cache:
                try:
                    lines_cache[f] = Path(f).read_text(errors="replace").splitlines()
                except OSError:
                    lines_cache[f] = []
            text = lines_cache[f][n - 1] if 0 < n <= len(lines_cache[f]) else ""
            for name in re.findall(r"\b(\w+)\s*\(", text):
                if name in protos and not re.search(r"\(\s*\w[\w\s]*\*?\s*\)\s*" + name, text):
                    rets[name][d] += 1
                    where[name].add(f"{f}:{n}")
names = sorted(set(args) | set(rets), key=lambda k: -(sum(args[k].values()) + sum(rets[k].values())))
print(f"{len(units)} units; {len(names)} of {len(protos)} canonical prototypes have int/pointer mix-ups at call sites\n")
for k in names:
    print(protos[k])
    for (i, d), c in sorted(args[k].items()):
        print(f"    argument {i}: {d} x{c}   ({'prototype integer, caller pointer' if d == 'int<-ptr' else 'prototype pointer, caller integer'})"
              + ("   e.g. " + ", ".join(sorted(argwhere[(k, i, d)])[:3]) if d == "int<-ptr" else ""))
    for d, c in rets[k].items():
        print(f"    return value: {d} x{c}   ({'result stored as integer' if d == 'int<-ptr' else 'result stored as pointer'})")
    print("    e.g. " + ", ".join(sorted(where[k])[:3]))
