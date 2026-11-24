"""Which argument registers does a function really read? (OrrePort; leftover-calls.md)

For each function name, scans its original assembly (build/GC6E01/asm) from the entry in order and reports the
integer argument registers r3-r10 and float registers f1-f8 that are read before anything writes them: those are
the parameters the function takes (or passes on). A straight-line scan up to the first branch target or call
(conservative: registers read only after a call are not counted, since the call may have set them). Compare with
the C prototype: fewer parameters in C than registers read = a definition that drops parameters (callers' values
are real); more = parameters the function never reads (callers' values are unused). Read-only.

Usage (repo root): python -I tools/local/port/argregs.py NAME [NAME ...]
"""
import re
import sys
from pathlib import Path

ARGS = [f"r{i}" for i in range(3, 11)] + [f"f{i}" for i in range(1, 9)]


def body(name):
    for p in Path("build/GC6E01/asm").rglob("*.s"):
        t = p.read_text(encoding="utf-8", errors="replace")
        i = t.find(f".fn {name},")
        if i >= 0:
            j = t.find(".endfn", i)
            return [re.sub(r"^/\* [0-9A-F]+ [0-9A-F]+  (?:[0-9A-F]{2} ){4}\*/\s*", "", l).strip()
                    for l in t[i:j].splitlines()[1:]]
    return None


def reads(lines):
    """(read, passed): argument registers read before any write in a linear scan of the whole body, and those still
    unwritten when the first call is made (handed to the callee untouched: the function may pass its own parameter
    through, as _soundMemFree does). Linear order is an approximation across branches; check by hand when it matters."""
    written, read, passed, seen_call = set(), [], [], False
    for l in lines:
        if not l or l.startswith((".L", "#", "/*")):
            continue
        op, _, rest = l.partition(" ")
        regs = [x.strip() for x in re.split(r",", rest)]
        flat = []
        for x in regs:
            m = re.match(r"^-?\w*\((r\d+)\)$", x)
            flat.append(m.group(1) if m else x)
        if op in ("bl", "bctrl", "blrl"):
            if not seen_call:
                passed = [r for r in ARGS if r not in written and r.startswith(("r", "f"))]
                seen_call = True
            # after a call r3-r12 / f0-f13 hold the callee's results or garbage
            written |= set(ARGS)
            continue
        if op.startswith("b"):
            continue
        stores = op.startswith(("st", "cmp")) or op in ("mtlr", "mtctr", "mtspr")
        dst = None if stores else (flat[0] if flat else None)
        srcs = flat if stores else flat[1:]
        for r in srcs:
            if r in ARGS and r not in written and r not in read:
                read.append(r)
        if dst in ARGS:
            written.add(dst)
    return read, passed


def main():
    for name in sys.argv[1:]:
        b = body(name)
        if b is None:
            print(f"{name}: no assembly found")
            continue
        rd, ps = reads(b)
        ps = [r for r in ps if r not in rd and r in ("r3", "r4", "r5", "r6", "f1", "f2")]
        print(f"{name}: reads {', '.join(rd) or 'none'}" + (f"; hands {', '.join(ps)} untouched to its first call (may pass a parameter through)" if ps else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
