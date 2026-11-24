"""List native reads of data that is big-endian on the PC (OrrePort byte-order check).

A data symbol declared with a DISC_STRUCT type anywhere (`extern MenuItem lbl_802EF0A8[]`) stays big-endian on the PC
(pcdata.py treats DISC-typed data as settled and copies the bytes). Any other file that reads the same symbol through a
plain cast (`*(s16*)(lbl_802EF0A8 + 4)`, `((s16*)lbl_x)[i]`, a non-DISC struct view) reads it in the wrong byte order.
This lists such reads, by symbol and file. Reads already wrapped in DISC_* macros are skipped.
Usage (repo root): python -I tools/local/port/discmixed.py [--all]   (default: only 16- / 32-bit reads)
"""
import re
import sys
from collections import defaultdict
from pathlib import Path

files = sorted(Path("src/game").rglob("*.c")) + sorted(Path("src/game").rglob("*.cpp")) + sorted(Path("include").rglob("*.h"))
texts = {f: f.read_text(encoding="utf-8", errors="replace") for f in files}

disc_types = set()
for t in texts.values():
    for m in re.finditer(r"\bstruct\s+DISC_STRUCT\s+(\w+)?\s*\{", t):
        if m.group(1):
            disc_types.add(m.group(1))
        tail = t[m.end():]
        depth, i = 1, 0
        while depth and i < len(tail):
            depth += (tail[i] == "{") - (tail[i] == "}")
            i += 1
        mm = re.match(r"\s*(\w+)\s*;", tail[i:])
        if mm:
            disc_types.add(mm.group(1))

disc_syms = defaultdict(set)
for f, t in texts.items():
    for m in re.finditer(r"\bextern\s+(?:const\s+)?(?:struct\s+)?(\w+)\s*\*?\s*(lbl_[0-9A-Fa-f]{8})\b", t):
        if m.group(1) in disc_types:
            disc_syms[m.group(2)].add(f)

wide = r"(?:s16|u16|s32|u32|f32)" if "--all" not in sys.argv else r"\w+"
hits = defaultdict(list)
for f, t in texts.items():
    for sym in disc_syms:
        if sym not in t:
            continue
        for ln, line in enumerate(t.splitlines(), 1):
            if sym not in line or "extern" in line or line.strip().startswith(("/*", "*", "//")):
                continue
            for m in re.finditer(r"\*\s*\(\s*" + wide + r"\s*\*\s*\)\s*\(\s*(?:\(u8\s*\*\)\s*)?" + sym + r"\b|\(\s*\(\s*" + wide
                                 + r"\s*\*\s*\)\s*\(?\s*" + sym + r"\b", line):
                pre = line[:m.start()]
                if re.search(r"DISC_\w*\($", pre.rstrip()):
                    continue
                hits[sym].append(f"{f.as_posix()}:{ln}: {line.strip()[:120]}")
for sym in sorted(hits, key=lambda s: -len(hits[s])):
    print(f"{sym}: {len(hits[sym])} native reads (DISC-typed in {', '.join(sorted(x.name for x in disc_syms[sym]))})")
    for h in hits[sym][:6]:
        print("   " + h)
print(f"{sum(len(v) for v in hits.values())} reads, {len(hits)} symbols; {len(disc_syms)} DISC-typed symbols checked")
