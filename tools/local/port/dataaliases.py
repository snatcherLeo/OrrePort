"""Data names the C uses that symbols.txt doesn't define on their own (OrrePort; first link).

The decompiled C sometimes names a word inside a larger symbol (`lbl_8047B66C` is the second word of the 8-byte
`lbl_8047B668`). On the GameCube the linker map resolves both; the PC link knows only symbols that exist, so these
need aliases (`name = symbol + offset`, e.g. a linker `--defsym` or a generated `#define`). Names outside any symbol
(low-memory addresses such as 0x80003FF0, or references only in dead code) are listed for a manual look. Output:
build-pc/pc_data_aliases.txt. Read-only.

Usage (repo root): python -I tools/local/port/dataaliases.py
"""
import bisect
import re
from pathlib import Path


def main():
    syms, ranges = {}, []
    for line in open("config/GC6E01/symbols.txt", encoding="utf-8"):
        m = re.match(r"(\S+) = \.(\w+):0x([0-9A-F]+);.*?size:0x([0-9A-F]+)", line)
        if m:
            a = int(m.group(3), 16)
            syms[m.group(1)] = a
            ranges.append((a, int(m.group(4), 16), m.group(1)))
    ranges.sort()
    starts = [r[0] for r in ranges]
    used = set()
    for p in list(Path("src").rglob("*.c")) + list(Path("src").rglob("*.cpp")):
        used |= set(re.findall(r"\blbl_([0-9A-F]{8})\b", p.read_text(encoding="utf-8", errors="replace")))
    out = []
    for h in sorted(used):
        name = "lbl_" + h
        if name in syms:
            continue
        a = int(h, 16)
        i = bisect.bisect_right(starts, a) - 1
        if i >= 0 and ranges[i][0] < a < ranges[i][0] + ranges[i][1]:
            out.append(f"{name} = {ranges[i][2]} + 0x{a - ranges[i][0]:X}")
        else:
            out.append(f"{name} : not inside any symbol")
    Path("build-pc").mkdir(exist_ok=True)
    Path("build-pc/pc_data_aliases.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
    inside = sum(1 for x in out if " = " in x)
    print(f"{len(out)} names: {inside} inside a larger symbol (aliasable), {len(out) - inside} outside any symbol")


if __name__ == "__main__":
    main()
