"""Functions the PC build calls without any declaration in scope (OrrePort; port-gaps.md 16).

An implicitly declared function returns `int` in C. With -fpermissive (the PC build's mode) GCC only warns, and
pc_probe.py hides warnings, so a call to a float- or pointer-returning function compiles silently and gets garbage on
x64 (found by A 2026-10-15: gs_math_*.c hide their SDK declarations on the PC but never include Aurora's mtx.h, so
PSVECDotProduct & co. returned int). This runs every src/game, src/hsd and src/musyx unit in the PC configuration
(pc_probe --alias --aurora --permissive flags) with -Wimplicit-function-declaration and lists each function by file.
Output: build-pc/pc_implicit_decls.txt. Read-only; needs MSYS2 gcc on PATH.

Usage (repo root): python -I tools/local/port/implicitdecl.py [--only PREFIX]
"""
import collections
import concurrent.futures as cf
import re
import subprocess
import sys
from pathlib import Path

only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
jobs = []
for c in cmds:
    if "mwcceppc" not in c:
        continue
    m = re.search(r" -c (\S+) -o \S+", c)
    if not m:
        continue
    src = m.group(1).replace("\\", "/")
    if not src.startswith(("src/game/", "src/hsd/", "src/musyx/")) or (only and not src.startswith(only)):
        continue
    defs = re.findall(r"(?<=\s)-D(\S+)", c)
    lang = "c++" if re.search(r"-lang[ =]c\+\+", c) or src.endswith((".cpp", ".cp")) else "c"
    jobs.append((src, ["gcc", "-fsyntax-only", "-fpermissive", "-Wimplicit-function-declaration", "-fmax-errors=0",
                       "-DTARGET_PC=1", "-include", "port/src/sdk_pc_alias.h", "-include", "include/pc/game_pc_alias.h",
                       "-include", "include/pc/prototypes.h", "-Iinclude", "-Ibuild/GC6E01/include", "-I.",
                       "-Iextern/aurora/include", "-x", lang, "-std=gnu11" if lang == "c" else "-std=gnu++17"]
                 + [f"-D{d}" for d in defs] + [src]))

pat = re.compile(r"^([^:]+):(\d+):\d+: (?:warning|error): implicit declaration of function .(\w+).")


def run(job):
    src, cmd = job
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    return [(m.group(1).replace("\\", "/"), int(m.group(2)), m.group(3))
            for m in (pat.match(l) for l in r.stderr.splitlines()) if m]


hits = set()
with cf.ThreadPoolExecutor(12) as ex:
    for res in ex.map(run, jobs):
        hits.update(res)
by_fn = collections.defaultdict(set)
for f, line, fn in hits:
    by_fn[fn].add(f"{f}:{line}")
out = [f"{fn}\t{len(sites)}\t{' '.join(sorted(sites)[:6])}" for fn, sites in sorted(by_fn.items(), key=lambda kv: -len(kv[1]))]
Path("build-pc").mkdir(exist_ok=True)
Path("build-pc/pc_implicit_decls.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
print(f"{len(jobs)} units; {len(by_fn)} functions implicitly declared at {len(hits)} sites -> build-pc/pc_implicit_decls.txt")
