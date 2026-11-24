"""Generate MusyX's initialised tables for the PC build, in native byte order (OrrePort).

The GameCube build defines these tables in the game's data units as big-endian byte arrays (for example
`u8 lbl_803692C8[388] = { ... };` in src/game/data/data_80363CA8.c), but our MusyX runtime reads them as u16 / s16 /
f32. On a little-endian PC the bytes must be turned into typed values first. This script reads the byte arrays from
those committed data units and writes typed C definitions into the build folder (untracked); every output file starts
with the generated-data marker line, so the git hooks never let it be committed (snatcherLeo's conditions for generated game
data, the project notes section 6).

Usage (repo root): python -I port/tools/musyx_tables.py <output.c>
"""
import re
import struct
import sys
from pathlib import Path

MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"  # built at run time so this script itself doesn't carry the marker

# (symbol, data unit, element type, struct format, MusyX name: guess from our source comments)
TABLES = [
    ("lbl_803692C8", "src/game/data/data_80363CA8.c", "u16", ">H", "dspAttenuationTab"),
    ("lbl_8036944C", "src/game/data/data_80363CA8.c", "u8", ">B", "dspScale2IndexTab"),
    ("lbl_8036984C", "src/game/data/data_80363CA8.c", "f32", ">f", "dspDLSVolTab"),
    ("lbl_80369D20", "src/game/data/data_80369D20.c", "s16", ">h", "sndSintab (quarter sine, 1024 entries)"),
]


def byte_array(path, symbol):
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    m = re.search(r"\b" + symbol + r"\s*\[\s*(\d+)\s*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not m:
        sys.exit(f"{symbol}: byte array not found in {path}")
    values = [int(v, 0) for v in re.findall(r"0x[0-9A-Fa-f]+|\d+", re.sub(r"/\*.*?\*/", "", m.group(2), flags=re.S))]
    if len(values) != int(m.group(1)):
        sys.exit(f"{symbol}: {len(values)} bytes found, {m.group(1)} declared")
    return bytes(values)


# Constants that the data units already define as typed C (copied as they are; no byte order to fix)
TYPED = [
    ("lbl_8047D3B0", "src/game/data/sdata2_8047D3B0.c"),
    ("lbl_8047D3C0", "src/game/data/sdata2_8047D3C0.c"),
]


def typed_definition(path, symbol):
    text = Path(path).read_text(encoding="utf-8", errors="replace")
    m = re.search(r"const\s+(f32|f64|u32|s32)\s+" + symbol + r"\s*(\[\s*\d+\s*\])?\s*=\s*([^;]+);", text)
    if not m:
        sys.exit(f"{symbol}: typed definition not found in {path}")
    return f"const {m.group(1)} {symbol}{m.group(2) or ''} = {m.group(3).strip()};"


def main():
    out = [f"/* {MARKER}: MusyX tables in native byte order, made by port/tools/musyx_tables.py from the game's data",
           " * units. Build output only: never commit (the git hooks refuse this marker). */",
           '#include "dolphin/types.h"', ""]
    for symbol, path, ctype, fmt, name in TABLES:
        data = byte_array(path, symbol)
        size = struct.calcsize(fmt)
        if len(data) % size:
            sys.exit(f"{symbol}: {len(data)} bytes is not a whole number of {ctype}")
        values = [struct.unpack_from(fmt, data, i)[0] for i in range(0, len(data), size)]
        if ctype == "f32":
            items = [repr(float(v)) + "f" if v == v else "__builtin_nanf(\"\")" for v in values]
        else:
            items = [str(v) for v in values]
        out.append(f"/* {name} */")
        out.append(f"{ctype} {symbol}[{len(values)}] = {{")
        for i in range(0, len(items), 12):
            out.append("    " + ", ".join(items[i:i + 12]) + ",")
        out.append("};")
        out.append("")
    # TYPED constants are not copied: the game's own data units define them (sdata2_8047D3B0.c / _D3C0.c); the
    # standalone audio tests compile those two files (port/CMakeLists.txt, ORRE_MUSYX_GAME_DATA)
    out.append("")
    Path(sys.argv[1]).parent.mkdir(parents=True, exist_ok=True)
    Path(sys.argv[1]).write_text("\n".join(out), encoding="utf-8")


if __name__ == "__main__":
    main()
