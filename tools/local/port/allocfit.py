"""Find heap allocations sized in GameCube bytes for structs that are bigger on the PC (OrrePort).

The game allocates many records with a literal byte count (`_toolentryAlloc__FUl(0x40)`, `fn_800E2B00(0x8C, 0x20)`,
`memAlloc(0x8000)`) and then uses the memory through a struct. If that struct holds native pointers, it is bigger on
the PC and the record overruns its block (B's boot run 31: cacheInit; A: the move-sequence pool header). For each
function, this pairs a literal-size allocation with the struct types of the pointer locals in that function that
receive allocated memory (`T* x = ...alloc...` or `x = GSmemLock(handle)` after the allocation), and reports
those whose PC size times the apparent count exceeds the byte count. Heuristic: each hit needs a look.
Usage (repo root): python -I tools/local/port/allocfit.py
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.argv = sys.argv[:1]
import bssfit  # noqa: E402  (struct size calculator; its scan does nothing without a generated file)

ALLOC = re.compile(r"\b(_toolentryAlloc__FUl|fn_800E2B00|GSmemAlloc|memAlloc|GSmemAllocRaw|fsysAlloc|pdaAlloc|"
                   r"GSresAllocResourceAlign|windowAllocMemory|shopMemoryAlloc)\s*\(\s*(0x[0-9A-Fa-f]+|\d+)\s*[,)]")
FUNC = re.compile(r"^[A-Za-z_][\w \*]*\b(\w+)\s*\([^;]*\)\s*\{?\s*$")
for p in sorted(Path("src/game").rglob("*.c")):
    L = p.read_text(errors="replace").splitlines()
    i = 0
    while i < len(L):
        m = FUNC.match(L[i])
        if not m or L[i].rstrip().endswith(";"):
            i += 1
            continue
        j, depth, seen = i, 0, False
        while j < len(L):
            depth += L[j].count("{") - L[j].count("}")
            seen = seen or "{" in L[j]
            if seen and depth <= 0:
                break
            j += 1
        body = "\n".join(L[i:j + 1])
        allocs = [int(a.group(2), 0) for a in ALLOC.finditer(body)]
        if allocs:
            types = set(re.findall(r"\b(?:struct\s+)?([A-Z]\w+)\s*\*\s*\w+\s*(?:=|;)", body))
            for t in types:
                gc, pc = bssfit.size_of(t, False), bssfit.size_of(t, True)
                if not gc or not pc or gc[0] == pc[0]:
                    continue
                for n in allocs:
                    if n % gc[0] == 0 and n // gc[0] >= 1 and (n // gc[0]) * pc[0] > n:
                        print(f"{p.name}:{i + 1} {m.group(1)}: alloc 0x{n:X} = {n // gc[0]} x {t} "
                              f"(GameCube 0x{gc[0]:X}, PC 0x{pc[0]:X} each)")
        i = j + 1
