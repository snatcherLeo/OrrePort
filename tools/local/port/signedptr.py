"""Find signed integers turned into pointers in the PC build (OrrePort; port-gaps.md 16).

A u32 holding a game address zero-extends to a valid 64-bit pointer; a signed s32 / int sign-extends any address at
0x80000000 or above (all of MEM1) into 0xFFFFFFFF8xxxxxxx. GCC's strict mode reports every integer -> pointer
conversion ("makes pointer from integer without a cast") and, for arguments, the argument's type in a following note;
for assignments and initialisations the type is in the error itself. This compiles every src/game unit in the PC
configuration without -fpermissive (pc_probe.py's flags), keeps those diagnostics with their source type, and lists
the signed ones (s8 / s16 / s32 / int / long / short / char / BOOL). Needs MSYS2 gcc on PATH.
Usage (repo root): python -I tools/local/port/signedptr.py [--only DIR]    -> build-pc/pc_signed_ptr.log
"""
import collections
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else "src/game"
cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout.splitlines()
units = []
for c in cmds:
    m = re.search(r" -c (\S+) -o \S+", c)
    if "mwcceppc.exe" not in c or not m:
        continue
    src = m.group(1).replace("\\", "/")
    if not src.startswith(only):
        continue
    lang = "c++" if re.search(r"-lang[ =]c\+\+", c) or src.endswith((".cpp", ".cp")) else "c"
    units.append((src, re.findall(r"(?<=\s)-D(\S+)", c), lang))
SIGNED = {"s8", "s16", "s32", "int", "long", "short", "char", "signed char", "BOOL", "long int", "short int"}


def run(u):
    src, defs, lang = u
    cmd = (["gcc", "-fsyntax-only", "-w", "-Werror=int-conversion", "-fmax-errors=0", "-DTARGET_PC=1",
            "-include", "port/src/sdk_pc_alias.h", "-include", "include/pc/game_pc_alias.h",
            "-include", "include/pc/prototypes.h", "-Iinclude", "-Ibuild/GC6E01/include", "-I.",
            "-Iextern/aurora/include", "-x", lang, "-std=gnu11" if lang == "c" else "-std=gnu++17"]
           + [f"-D{d}" for d in defs] + [src])
    lines = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stderr.splitlines()
    out = []
    for i, l in enumerate(lines):
        if "makes pointer from integer" not in l:
            continue
        loc = l.split(": error:")[0]
        ty = None
        m = re.search(r"from '([^']+)'", l)
        if m:
            ty = m.group(1)
        for n in lines[i + 1:i + 7]:
            mm = re.search(r"but argument is of type '([^']+)'", n)
            if mm:
                ty = mm.group(1)
                break
            # GCC's caret display labels the expression with its type: "      |     u32 {aka unsigned int}"
            mm = re.match(r"^\s+\|\s+([A-Za-z_][\w ]*?)(?: \{aka ([^}]+)\})?\s*$", n)
            if mm and not ty:
                ty = mm.group(2) or mm.group(1)
                break
        out.append((loc, ty or "?", l.split("error: ")[-1][:90]))
    return out


with ThreadPoolExecutor(16) as ex:
    res = [r for rs in ex.map(run, units) for r in rs]
seen = set()
by_ty = collections.Counter()
signed = []
for loc, ty, msg in res:
    if (loc, ty) in seen:
        continue
    seen.add((loc, ty))
    base = re.sub(r"\s*\{aka '([^']+)'\}", "", ty).strip()
    by_ty[base] += 1
    if base in SIGNED:
        signed.append(f"{loc}: {ty}: {msg}")
Path("build-pc").mkdir(exist_ok=True)
Path("build-pc/pc_signed_ptr.log").write_text("\n".join(signed) + "\n", encoding="utf-8")
print(f"{len(units)} units; {len(seen)} integer -> pointer conversions by source type: {dict(by_ty.most_common(8))}")
print(f"signed: {len(signed)} -> build-pc/pc_signed_ptr.log")
