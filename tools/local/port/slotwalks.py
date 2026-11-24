"""Catch native pointer reads of GameCube-layout linked lists and window slots in the PC build (OrrePort).

Runtime records that keep their GameCube layout on the PC (window records, sprite lists, menu argument blocks, ...)
hold pointers in 32-bit slots (`PTR32_FIELD`, `PTR32_AT_T`, include/pc/ptr32.h). A native read of such a slot,
`*(T**)(p + 0x10)`, loads 8 bytes on the PC: the link plus its neighbour (windowCloseMain, winSpriteRelease,
windowSearchIDInline, the sprite walks in cursor_bios.c / win_sequence.c / window_candidate_801046C8.c and the shop's
list menu were all this bug, 2026-10-16). In live PC code (not the `#else` branch of `#ifdef TARGET_PC`) of the units
the PC build compiles, this flags:
  1. list-walk steps through native pointers: `x = *(T**)(x + off)` and `x = *(T**)x`;
  2. native pointer reads at the window record's slot offsets (list head +0x0C of winSysWork, links +0x10 / +0x14,
     sprite heads +0x1C / +0x20, the menu argument +0x60) in window, sprite, cursor and menu files.
Reviewed exceptions (not GameCube-layout records) are listed in ALLOW by file and line text. Exit 1 if anything
unreviewed is found, so the gate catches new cases.
Usage (repo root): python -I tools/local/port/slotwalks.py [--all-units]
"""
import re
import sys
from pathlib import Path

ALLOW = {
    # the hand-built MWCC variadic reader: its argument area is not a GameCube-layout record
    ("src/game/gs_range_800C45A0.c", "address = *(char**)address;"),
}

PTR = r"\*\s*\(\s*(?:const\s+)?(?:struct\s+)?\w+\s*\*\s*\*\s*\)"
WALK = re.compile(r"\b(\w+)\s*=\s*" + PTR + r"\s*\(\s*(?:\(\s*u8\s*\*\s*\)\s*)?\1\s*(?:\+\s*(?:0x[0-9A-Fa-f]+|\d+))?\s*\)")
WALK0 = re.compile(r"\b(\w+)\s*=\s*" + PTR + r"\s*\1\b")
SLOT = re.compile(PTR + r"\s*\(\s*(?:\(\s*u8\s*\*\s*\)\s*)?\w+\s*\+\s*0x(?:0?[cC]|10|14|1[cC]|20|60)\s*\)")
SLOT_FILES = re.compile(r"window|win_|cursor|menu")


def live_lines(text):
    """(line number, line) outside the #else part of `#ifdef TARGET_PC` / `#if defined(TARGET_PC)` blocks"""
    state = []
    for n, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if s.startswith("#if"):
            state.append("pc" if "TARGET_PC" in s and not s.startswith("#ifndef") and "!defined(TARGET_PC)" not in s
                         else "other")
        elif s.startswith("#else") and state:
            state[-1] = "gc" if state[-1] == "pc" else state[-1]
        elif s.startswith("#endif") and state:
            state.pop()
        elif "gc" not in state:
            yield n, line


def main():
    compiled_file = Path("build-pc/firstlink/compiled.txt")
    compiled = None
    if "--all-units" not in sys.argv and compiled_file.exists():
        compiled = set(compiled_file.read_text(encoding="utf-8").split())
    files = sorted(Path("src/game").rglob("*.c")) + sorted(Path("include/game").rglob("*.h"))
    hits = []
    for p in files:
        path = p.as_posix()
        if compiled is not None and path.endswith(".c") and path not in compiled:
            continue
        text = p.read_text(errors="replace")
        for n, line in live_lines(text):
            if (path, line.strip()) in ALLOW:
                continue
            found = WALK.search(line) or WALK0.search(line)
            if not found and SLOT_FILES.search(p.name):
                found = SLOT.search(line)
            if found:
                hits.append(f"{path}:{n}: {line.strip()[:120]}")
    for h in hits:
        print(h)
    print(f"{len(hits)} native reads of GameCube-layout list links / window slots"
          + ("" if compiled is not None else " (all units)"))
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main())
