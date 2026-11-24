"""List the names one of our SDK headers defines that Aurora's matching headers don't (OrrePort stage 2).

When our header hands over to Aurora's under TARGET_PC, game code still uses names only our reconstructed header had
(typedefs such as Vec3, macros, prototypes of functions the SDK keeps under other names). This prints those names so
the TARGET_PC branch can add them as compatibility definitions.

Usage: python -I tools/local/port/header_extras.py include/dolphin/mtx.h
Reads our header's GameCube branch, and every header under extern/aurora/include/dolphin.
"""
import re
import sys
from pathlib import Path

ours = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
if "#else /* GameCube build: unchanged */" in ours:
    ours = ours.split("#else /* GameCube build: unchanged */", 1)[1]
aurora = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in Path("extern/aurora/include").rglob("*.h"))


def names(text):
    out = {}
    for m in re.finditer(r"typedef\s+(?:struct|union|enum)?\s*[\w\s\*]*?\b(\w+)\s*(?:\[[^\]]*\])*\s*;", text):
        out.setdefault(m.group(1), "typedef")
    for m in re.finditer(r"\}\s*(\w+)\s*;", text):
        out.setdefault(m.group(1), "typedef")
    for m in re.finditer(r"^\s*#\s*define\s+(\w+)", text, re.M):
        out.setdefault(m.group(1), "macro")
    for m in re.finditer(r"^[A-Za-z_][\w\s\*]*?\b(\w+)\s*\([^;{]*\)\s*;", text, re.M):
        out.setdefault(m.group(1), "function")
    return out


o, a = names(ours), names(aurora)
missing = {k: v for k, v in o.items() if k not in a and not re.match(r"^(DOLPHIN_|_?[A-Z_]+_H_?$)", k)}
for kind in ("typedef", "macro", "function"):
    ks = sorted(k for k, v in missing.items() if v == kind)
    print(f"{kind}s only in ours ({len(ks)}): {' '.join(ks)}")
