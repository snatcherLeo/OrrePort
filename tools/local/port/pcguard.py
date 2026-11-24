"""Hide local re-definitions of SDK names from the PC build (OrrePort stage 2).

Our reconstructed headers and sources sometimes re-declare SDK types and functions locally (typedef struct ...
GXTexObj; typedef int GXTevOp; void GXLoadTexObj(...);). On the GameCube they must stay exactly as they are; on the PC,
Aurora's SDK headers provide the real ones and the local copies conflict. This wraps each such declaration in
`#ifndef TARGET_PC ... #endif`, which leaves the GameCube build's preprocessed source identical.

Usage: python -I tools/local/port/pcguard.py <file> <name> [<name> ...]
  Wraps: single-line declarations mentioning a name as the declared identifier (typedef ... NAME; / prototypes
  ... NAME(...);), and multi-line `typedef struct|union|enum { ... } NAME;` blocks. Already-guarded lines are skipped.
"""
import re
import sys

CR, LF = chr(13), chr(10)
path, names = sys.argv[1], set(sys.argv[2:])
raw = open(path, "rb").read().decode("utf-8", errors="surrogateescape")
nl = CR + LF if (CR + LF) in raw else LF
L = raw.replace(CR + LF, LF).split(LF)
wrap = []  # (start, end) inclusive line ranges
i = 0
while i < len(L):
    line = L[i]
    # multi-line typedef block ending in `} NAME;`
    m = re.match(r"^\s*\}\s*(\w+)\s*;", line)
    if m and m.group(1) in names:
        j = i
        while j >= 0 and not re.match(r"^\s*typedef\s+(struct|union|enum)\b", L[j]):
            j -= 1
        if j >= 0:
            wrap.append((j, i))
        i += 1
        continue
    # single-line typedef / prototype declaring NAME: a type word must come before the name, so an (indented) call
    # statement such as `    GXInitLightSpot(&obj, ...);` is never taken for a declaration (2026-10-15)
    m = re.match(r"^\s*(?:extern\s+)?(?:typedef\s+)?(?:(?:const|unsigned|signed|struct|union|enum|static|inline|volatile)\s+)*"
                 r"(?!return\b)\w+[\s\*]+(?:\w+[\s\*]+)*?(\w+)\s*(?:\([^;{}]*\))?\s*;\s*(?:/\*.*\*/|//.*)?$", line)
    if m and m.group(1) in names and not line.lstrip().startswith(("return", "#")):
        wrap.append((i, i))
    i += 1
# drop ranges already inside a TARGET_PC guard
out, done = [], 0
guarded = set()
for s, e in wrap:
    if s > 0 and L[s - 1].strip().startswith("#ifndef TARGET_PC"):  # also the commented forms the port tools write
        continue
    guarded.update(range(s, e + 1))
starts = {s for s, e in wrap if s in guarded}
ends = {e for s, e in wrap if s in guarded}
for k, line in enumerate(L):
    if k in starts:
        out.append("#ifndef TARGET_PC /* the PC build takes this SDK name from Aurora */")
    out.append(line)
    if k in ends:
        out.append("#endif")
        done += 1
open(path, "wb").write(nl.join(out).encode("utf-8", errors="surrogateescape"))
print(f"{path}: {done} declarations guarded")
