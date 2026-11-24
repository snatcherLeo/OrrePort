"""List what Felk's Dolphin fork exposes to scripts (2026-10-19: finding a way to log every disc read, not one per
frame). Writes api.txt in POS_OUT: every name in each dolphin module, with its docstring where there is one.

    set POS_OUT=<folder>   Dolphin.exe -b -e "<ISO>" --script dolphin_api_dump.py
"""
import os, sys, time
import dolphin
from dolphin import event, memory

OUT = os.environ.get("POS_OUT", ".")
lines = []
mods = {"dolphin": dolphin}
for name in ("event", "memory", "controller", "savestate", "registers", "gui", "debug", "emulation", "utils"):
    if hasattr(dolphin, name):        # the fork's modules are attributes of `dolphin`, not importable submodules
        mods[name] = getattr(dolphin, name)
    else:
        lines.append(f"[{name}] not present")
for name, m in mods.items():
    lines.append(f"== {name}")
    for attr in sorted(dir(m)):
        if attr.startswith("__"):
            continue
        doc = (getattr(getattr(m, attr), "__doc__", "") or "").strip().replace("\n", " ")[:300]
        lines.append(f"  {attr}: {doc}")
lines.append("python " + sys.version)
open(os.path.join(OUT, "api.txt"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
time.sleep(1)
os._exit(0)
