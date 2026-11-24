"""Compare a PC replay's memory card with the console's end-of-sitting card, section by section (2026-10-27).

    python -I tools/local/testing/savecmp.py <pc card .gci> <console card_end .gci> [--bytes N]

The second pass's last check for each sitting: the save the PC made at the sitting's end against the one snatcherLeo's
console made (`card_end/` in the sitting folder). A PC card in the PC's byte order is converted to the GameCube's
first (colo/savepc.py; the replays' cards were already in the GameCube's order on 2026-10-27), then both cards' current
save blocks (the valid slot with the highest save counter) are compared
byte for byte and the differences listed per save section (docs/engine/save-data.md), with the story flags that
differ by id. Read-only; prints only offsets, counts and flag ids, no save contents beyond the differing bytes'
values (--bytes N shows up to N differing bytes per section, default 8).
"""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "colo"))
import savefile  # noqa: E402
import savepc  # noqa: E402

# (start, name) in block order; each section runs to the next start (docs/engine/save-data.md, the section table)
SECTIONS = [
    (0x0, "1 bookkeeping"), (0x70, "2 player"), (0xB88, "3 PC box"), (0x7D20, "10 mail"), (0x8168, "11 daycare"),
    (0x82A8, "12 Strategy Memo"), (0x9A1C, "13 Card-e"), (0xE3E8, "14 Battle Mode state"), (0x1B014, "15 Shadow"),
    (0x1BDBC, "4 flags class 1"), (0x1BDDC, "5 flags class 2"), (0x1BE5C, "6 flags class 3"),
    (0x1C45C, "16 ribbons"),
]


def section_of(off):
    name = SECTIONS[0][1]
    for start, n in SECTIONS:
        if off >= start:
            name = n
    return name


def flag_words(block):
    """{(class, word index): value} for the three flag buffers."""
    out = {}
    for cls, (o, n) in savefile.FLAG_SECTIONS.items():
        for i, v in enumerate(struct.unpack(f">{n}I", block[o:o + 4 * n])):
            out[(cls, i)] = v
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pc")
    ap.add_argument("console")
    ap.add_argument("--bytes", type=int, default=8)
    a = ap.parse_args()

    pc_gc = Path(a.pc).read_bytes()
    if savefile.current_block(pc_gc) is None:  # not valid in the GameCube's order: a PC-order card, convert it
        pc_gc, _ = savepc.convert(pc_gc, "pc", "gc")
        print("PC card: converted from the PC's byte order")
    else:
        print("PC card: already in the GameCube's byte order")
    pc_slots = savefile.slots(pc_gc)
    con_slots = savefile.slots(Path(a.console).read_bytes())
    for side, sl in (("PC", pc_slots), ("console", con_slots)):
        print(f"{side}: " + ", ".join(f"slot {s['index']} counter {s['counter']} {'valid' if s['valid'] else '-'}"
                                      for s in sl))
    pc = savefile.current_block(pc_gc)
    con = savefile.current_block(Path(a.console).read_bytes())
    if pc is None or con is None:
        raise SystemExit("no valid slot on " + ("the PC card" if pc is None else "the console card"))

    diffs = [o for o in range(len(con)) if pc[o] != con[o]]
    print(f"save block: {len(con)} bytes, {len(diffs)} differ" + (" (identical)" if not diffs else ""))
    # bytes the game is known to read: the traced and code-known fields (savepc's table) and the kept text ranges.
    # A differing byte outside all of them is likely padding or leftover memory, which no game code reads back
    known = bytearray(len(con))
    for off, w in savepc.load_fields().items():
        known[off:off + w] = b"\x01" * w
    for off, w in savepc.load_keep():
        known[off:off + w] = b"\x01" * w
    per = {}
    for o in diffs:
        per.setdefault(section_of(o), []).append(o)
    for _, name in SECTIONS:
        if name not in per:
            continue
        offs = per[name]
        field = [o for o in offs if known[o]]
        shown = ", ".join(f"+0x{o:05X} {con[o]:02X}->{pc[o]:02X}" for o in field[:a.bytes])
        print(f"  {name}: {len(offs)} bytes differ, {len(field)} in known fields"
              + (f" (console->PC): {shown}{' ...' if len(field) > a.bytes else ''}" if field else ""))

    fp, fc = flag_words(pc), flag_words(con)
    bad = [(k, fc[k], fp[k]) for k in fc if fc[k] != fp[k]]
    if bad:
        print(f"story-flag words that differ: {len(bad)}")
        for (cls, i), c, p in bad[:20]:
            bits = [b for b in range(32) if (c ^ p) >> b & 1]
            print(f"  class {cls} word {i}: bits {bits} (console {c:08X}, PC {p:08X})")
    else:
        print("story flags: identical")


if __name__ == "__main__":
    main()
