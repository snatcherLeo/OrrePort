"""Make our SDK headers hand over to Aurora's under TARGET_PC (OrrePort stage 2).

For each of our include/<path> SDK headers, wraps the existing content in `#ifndef TARGET_PC ... #endif` and adds a
TARGET_PC branch that includes Aurora's matching header: the same path with #include_next when Aurora has one, or
Aurora's umbrella header otherwise. The GameCube build (TARGET_PC undefined) sees exactly the old content.
Idempotent: headers that already have the TARGET_PC branch are skipped.

Usage: python -I tools/local/port/forward_headers.py
"""
from pathlib import Path

CR, LF = chr(13), chr(10)
# our header -> Aurora header (None = same path via #include_next)
MAP = {
    "dolphin/gx/GX.h": "dolphin/gx.h",
    "dolphin/gx/GXVert.h": None,
    "dolphin/mtx.h": None,
    "dolphin/os/OSInterrupt.h": None,
    "dolphin/os/OSCache.h": None,
    "dolphin/os/OSThread.h": None,
    "dolphin/os/OSModule.h": None,
    "dolphin/os/OSContext.h": None,
    "dolphin/os/OS.h": "dolphin/os.h",
    "dolphin/os/OSClock.h": "dolphin/os.h",
    "dolphin/vi/VI.h": "dolphin/vi.h",
    "dolphin/ai/AI.h": "dolphin/ai.h",
}
# every other header of ours under include/dolphin that Aurora also has (same path): Aurora's own headers include
# their sub-headers by these paths, and with include/ first they would otherwise pick up ours
for _p in Path("include/dolphin").rglob("*.h"):
    _rel = _p.relative_to("include").as_posix()
    if _rel not in MAP and Path("extern/aurora/include", _rel).exists():
        MAP[_rel] = None
MARK = "OrrePort (PC build): this SDK header hands over to Aurora's"
for ours, theirs in MAP.items():
    p = Path("include") / ours
    raw = p.read_bytes().decode("utf-8", errors="surrogateescape")
    if MARK in raw:
        print(f"{ours}: already forwarded")
        continue
    nl = CR + LF if (CR + LF) in raw else LF
    body = raw.replace(CR + LF, LF).rstrip(LF)
    inc = f"#include_next <{ours}>" if theirs is None else f"#include <{theirs}>"
    out = (f"#ifdef TARGET_PC\n/* {MARK} ({theirs or ours}); docs/port/stage2-plan.md. */\n{inc}\n"
           f"#else /* GameCube build: unchanged */\n{body}\n#endif /* TARGET_PC */\n")
    p.write_bytes(out.replace(LF, nl).encode("utf-8", errors="surrogateescape"))
    print(f"{ours} -> {theirs or '(include_next)'}")
