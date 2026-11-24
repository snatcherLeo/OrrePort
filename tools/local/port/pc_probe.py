"""OrrePort stage 2 probe: syntax-check every compiled unit of the game with the PC compiler (MSYS2 GCC).

For each MWCC compile in the GameCube build (`ninja -t commands all_source`), take its source file, its -D defines and
its language (-lang c / c++), and run `gcc -fsyntax-only` with TARGET_PC=1 and the repo's include paths. Prints a
summary of error kinds and the files with the most errors; writes the full log to build-pc/pc_probe.log.
Nothing is changed. Usage (repo root, MSYS2 mingw64 on PATH): python -I tools/local/port/pc_probe.py [jobs] [--only DIR] [--aurora]
"""
import collections
import concurrent.futures as cf
import os
import re
from pathlib import Path
import subprocess
import sys

jobs = int(next((a for a in sys.argv[1:] if a.isdigit()), "16"))
only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
PERMISSIVE = "--permissive" in sys.argv  # GCC 14 -fpermissive: old-C problems become warnings
ALIAS = "--alias" in sys.argv  # also force-include include/pc/game_pc_alias.h (address names -> real names, as the PC link will)
FLOAT_ARGS = "--float-args" in sys.argv  # also log float -> integer conversions of call arguments (port-gaps.md 11: an int / float parameter-order swap always produces one)
AURORA = "--aurora" in sys.argv  # Aurora's SDK headers after ours: our headers forward to them under TARGET_PC; also force-includes include/pc/prototypes.h.
# The PC build's own configuration is --alias --aurora --permissive (Corrected 2026-10-15: before this date --alias did
# nothing without --aurora, an operator-precedence slip)
cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
units = []
for c in cmds:
    if "mwcceppc.exe" not in c:
        continue
    m = re.search(r" -c (\S+) -o \S+", c)
    if not m:
        continue
    src = m.group(1).replace("\\", "/")
    if not src.startswith("src/") or (only and not src.startswith(only)):
        continue
    defs = re.findall(r"(?<=\s)-D(\S+)", c)
    lang = "c++" if re.search(r"-lang[ =]c\+\+", c) or src.endswith((".cpp", ".cp")) else "c"
    units.append((src, defs, lang))


def run(u):
    src, defs, lang = u
    cmd = ["gcc", "-fsyntax-only"] + (["-Wfloat-conversion"] if FLOAT_ARGS else ["-w"]) + ["-fmax-errors=0", "-DTARGET_PC=1"] + (["-fpermissive"] if PERMISSIVE else []) + (["-include", "port/src/sdk_pc_alias.h", "-include", "include/pc/game_pc_alias.h"] if ALIAS else []) + (["-include", "include/pc/prototypes.h"] if AURORA else []) + ["-Iinclude", "-Ibuild/GC6E01/include", "-I."] + (["-Iextern/aurora/include"] if AURORA else []) + [
           "-x", lang, "-std=gnu11" if lang == "c" else "-std=gnu++17"] + [f"-D{d}" for d in defs] + [src]
    r = subprocess.run(cmd, capture_output=True, text=True, errors="replace")
    lines = r.stderr.splitlines()
    errs = []
    for i, l in enumerate(lines):
        if ": error: " not in l:
            continue
        # an error inside a macro (an alias `#define fn_X name`) is reported at the #define; GCC's following
        # "in expansion of macro" notes give the use site: append the last one as [at file:line]
        at = None
        for n in lines[i + 1:i + 8]:
            if ": error: " in n:
                break
            m = re.match(r"^(.+?):(\d+):\d+: note: in expansion of macro", n)
            if m:
                at = f"{m.group(1)}:{m.group(2)}"
        errs.append(l + (f" [at {at}]" if at else ""))
    if FLOAT_ARGS:
        # keep the float -> integer conversions that sit inside a call's argument list (the column is inside an
        # unclosed `name(` on that line): an assignment converts the same way on the GameCube, an argument may not
        for l in lines:
            m = re.match(r"^(.+?):(\d+):(\d+): warning: conversion from '(float|double|f32|f64)'.*\[-Wfloat-conversion\]", l)
            if not m:
                continue
            try:
                text = Path(m.group(1)).read_text(encoding="utf-8", errors="replace").splitlines()[int(m.group(2)) - 1]
            except (OSError, IndexError):
                continue
            before = text[:int(m.group(3)) - 1]
            depth, call = 0, False
            for k in range(len(before) - 1, -1, -1):
                ch = before[k]
                if ch == ")":
                    depth += 1
                elif ch == "(":
                    if depth == 0:
                        call = bool(re.search(r"\w\s*$", before[:k]))
                        break
                    depth -= 1
            if call:
                errs.append("FLOATARG " + l + " :: " + text.strip()[:120])
    return src, errs


kinds = collections.Counter()
per_file = collections.Counter()
total = 0
os.makedirs("build-pc", exist_ok=True)
floatlog = []
with open("build-pc/pc_probe.log", "w", encoding="utf-8") as log, cf.ThreadPoolExecutor(jobs) as ex:
    for src, errs in ex.map(run, units):
        total += 1
        for e in errs:
            if e.startswith("FLOATARG "):
                floatlog.append(f"{src}: {e[9:]}")
                continue
            log.write(f"{src}: {e}\n")
            k = re.sub(r"'[^']*'", "X", re.sub(r" \[at [^\]]*\]$", "", e.split(": error: ", 1)[1]))
            k = re.sub(r"\d+", "N", k)
            kinds[k] += 1
        if errs:
            per_file[src] += len(errs)
clean = total - len(per_file)
print(f"{total} units; {clean} compile cleanly; {len(per_file)} with errors; {sum(per_file.values())} errors in all")
print("\nError kinds:")
for k, n in kinds.most_common(25):
    print(f"  {n:6}  {k[:110]}")
print("\nFiles with the most errors:")
for f, n in per_file.most_common(15):
    print(f"  {n:6}  {f}")

if FLOAT_ARGS:
    Path("build-pc/pc_float_args.log").write_text("\n".join(sorted(set(floatlog))) + "\n", encoding="utf-8")
    print(f"{len(set(floatlog))} float -> integer argument conversions -> build-pc/pc_float_args.log")
