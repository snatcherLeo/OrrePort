"""List zero-argument calls to functions whose definition takes parameters (OrrePort, 2026-10-16).

The GameCube passes whatever the argument registers hold (e.g. a just-returned value still in r3); on the PC such a
call passes garbage. Found the hard way: floorReadColPostFunc called fn_8010CFE4() while the GameCube passed the
collision file in r3 (assembly). Only calls through an empty-parameter extern visible to the PC build are listed
(declarations inside `#ifndef TARGET_PC` are replaced by canonical prototypes there). Each hit needs the assembly:
real value if used, any value if provably unused (snatcherLeo's rule: never default to 0).
Usage (repo root): python -I tools/local/port/zeroargs.py
"""
import re
from pathlib import Path


def live_text(text):
    """The lines the PC build sees (drops `#ifndef TARGET_PC` blocks and the `#else` of `#ifdef TARGET_PC`)."""
    out, state = [], []
    for line in text.splitlines():
        s = line.strip()
        if s.startswith("#if"):
            if s.startswith("#ifndef TARGET_PC"):
                state.append("gc")
            elif "TARGET_PC" in s and not s.startswith("#ifndef"):
                state.append("pc")
            else:
                state.append("other")
            out.append("")
            continue
        if s.startswith("#else") and state:
            state[-1] = {"gc": "pc", "pc": "gc"}.get(state[-1], state[-1])
            out.append("")
            continue
        if s.startswith("#endif") and state:
            state.pop()
            out.append("")
            continue
        out.append("" if "gc" in state else line)
    return "\n".join(out)


units = [l.strip() for l in open("build-pc/firstlink/compiled.txt") if l.startswith("src/game")]
defs = {}
for p in Path("src/game").rglob("*.c"):
    for m in re.finditer(r"^[A-Za-z_][\w \*]*?\b(\w+)\s*\(([^()]*)\)\s*\{", p.read_text(errors="replace"), re.M):
        params = m.group(2).strip()
        if params and params != "void":
            defs.setdefault(m.group(1), params)
hits = []
for f in units:
    t = live_text(Path(f).read_text(errors="replace"))
    lines = t.splitlines()
    # empty `()` and `(void)` declarations alike (scriptExecTask's `extern s32 fn_800F036C(void);`, 2026-10-16)
    for name in set(re.findall(r"extern[^;(]*\b(\w+)\s*\(\s*(?:void)?\s*\)\s*;", t)):
        if name not in defs:
            continue
        for m in re.finditer(r"(?<![\w.>])" + re.escape(name) + r"\s*\(\s*\)", t):
            n = t.count("\n", 0, m.start())
            if "extern" not in lines[n]:
                hits.append(f"{f}:{n + 1}: {name}() but defined ({defs[name][:60]})")
print("\n".join(hits))
print(len(hits), "zero-argument calls to functions that take parameters")
