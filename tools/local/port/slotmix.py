"""Find offsets used as 32-bit slots in one place and as native pointers in another (OrrePort).

Runtime records kept in their GameCube layout on the PC hold pointers in 32-bit slots, accessed with
PTR32_AT / PTR32_AT_T / PTR32_AT_SET_T (include/pc/ptr32.h). A read of the same base name and offset through a plain
pointer cast (`*(void**)(work + 0x24)`) elsewhere loads 8 bytes on the PC: the wrong value, or the slot plus its
neighbour. This lists such reads, keyed by (base variable name, offset). Base names repeat across unrelated records,
so each hit needs a look (A's sequence +0x24 and the PDA's menu model were real; floor_data's `ptr` was not).
Lines inside `#else` branches of `#ifdef TARGET_PC` blocks (GameCube-only code) are skipped.
Usage (repo root): python -I tools/local/port/slotmix.py
"""
import re
from collections import defaultdict
from pathlib import Path

SLOT = re.compile(r"PTR32_AT(?:_SET)?(?:_T)?\(\s*(?:[\w ]+\s*,\s*)?(?:\(u8\s*\*\)\s*)?(\w+)\s*,\s*(0x[0-9A-Fa-f]+)\s*[,)]")
RAW = re.compile(r"\*\s*\(\s*(?:struct\s+)?\w+\s*\*\*\s*\)\s*\(\s*(?:\(u8\s*\*\)\s*)?(\w+)\s*\+\s*(0x[0-9A-Fa-f]+)\s*\)")


def live_lines(text):
    """(line number, line) outside the #else part of `#ifdef TARGET_PC` blocks (one level is enough here)."""
    state = []
    for n, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if s.startswith("#if"):
            state.append("pc" if "TARGET_PC" in s and not s.startswith("#ifndef") else "other")
        elif s.startswith("#else") and state:
            state[-1] = "gc" if state[-1] == "pc" else state[-1]
        elif s.startswith("#endif") and state:
            state.pop()
        elif "gc" not in state:
            yield n, line


srcs = {p: p.read_text(errors="replace") for p in Path("src/game").rglob("*.c")}
slots = defaultdict(set)
for p, t in srcs.items():
    for m in SLOT.finditer(t):
        slots[(m.group(1), int(m.group(2), 16))].add(p.name)
hits = defaultdict(list)
for p, t in srcs.items():
    for n, line in live_lines(t):
        for m in RAW.finditer(line):
            k = (m.group(1), int(m.group(2), 16))
            if k in slots:
                hits[k].append(f"{p.name}:{n}")
for k, v in sorted(hits.items(), key=lambda kv: -len(kv[1])):
    print(f"{k[0]}+0x{k[1]:X}: {len(v)} native reads, e.g. {', '.join(v[:3])}; 32-bit slot in {', '.join(sorted(slots[k])[:2])}")
