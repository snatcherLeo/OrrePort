"""Suggest pcdata_types.txt overrides for raw data the code reads only through struct casts (OrrePort).

pcdata.py leaves a data symbol raw big-endian when its declarations don't prove a layout (`extern u8 x[]` only, or a
conflict). If the code casts it to a numbers-only struct (`((PokemonGrowData*)pokemon_grow_data)[idx]`) or a scalar (`(u16*)lbl_x`), that struct is
its layout, and an override `lbl_x Struct[]` lets pcdata convert it per field. This lists such symbols from the
generator's unsure list that have no override yet, with the struct, its record size and whether the symbol's size
fits (whole records, or a tail shorter than one record).
Check each suggestion (DISC_STRUCT-read tables stay raw by design; zero-filled data needs nothing) before adding it.
Usage (repo root): python -I tools/local/port/castoverrides.py [UNSURE_DATA.txt]
  (default: build-pc/generated/unsure_data.txt)
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
sys.argv = sys.argv[:2]
import pcdata  # noqa: E402

src = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("build-pc/generated/unsure_data.txt")
structs = pcdata.scalar_struct_layouts()
unsure = {}
for line in src.read_text(errors="replace").splitlines()[1:]:
    parts = line.split("\t")
    if len(parts) >= 3:
        unsure[parts[0]] = (int(parts[1], 16) if parts[1].startswith("0x") else 0, parts[2])
overridden = {l.split()[0] for l in Path("tools/local/port/pcdata_types.txt").read_text(encoding="utf-8").splitlines()
              if l.strip() and not l.startswith("#")}
hits = {}
for p in Path("src/game").rglob("*.c"):
    t = p.read_text(errors="replace")
    for m in re.finditer(r"\(\s*(?:struct\s+)?(\w+)\s*\*\s*\)\s*\(?\s*(?:PC_PTR\()?\s*&?\s*(lbl_[0-9A-F]{8})\b", t):
        st, sym = m.group(1), m.group(2)
        if st in structs and sym in unsure and sym not in overridden:
            hits.setdefault(sym, set()).add((st, p.name))
scalar_hits = {}
for p in Path("src/game").rglob("*.c"):
    t = p.read_text(errors="replace")
    for m in re.finditer(r"\(\s*(u16|s16|u32|s32|f32)\s*\*\s*\)\s*\(?\s*&?\s*(lbl_[0-9A-F]{8})(?!\s*\+)", t):
        ty, sym = m.group(1), m.group(2)
        if sym in unsure and sym not in overridden and "byte" in unsure[sym][1]:
            scalar_hits.setdefault(sym, set()).add((ty, p.name))
for sym, v in sorted(scalar_hits.items()):
    tys = sorted({t for t, _ in v})
    print(f"{sym} {tys[0]}[]	# 0x{unsure[sym][0]:X} bytes, read through ({'/'.join(tys)}*) casts; {', '.join(sorted({f for _, f in v})[:2])}"
          + ("  (CONFLICTING WIDTHS)" if len({pcdata.SCALARS[t][0] for t in tys}) > 1 else ""))
for sym, v in sorted(hits.items()):
    size, reason = unsure[sym]
    for st in sorted({s for s, _ in v}):
        rec = sum(pcdata.SCALARS[f][0] for f in structs[st])
        fits = size and (size % rec == 0 or (size > rec and size % rec < rec))
        files = ", ".join(sorted({f for s, f in v if s == st})[:2])
        print(f"{sym} {st}[]\t# 0x{size:X} bytes, record 0x{rec:X}{'' if fits else ' (DOES NOT FIT)'}; {reason[:60]}; {files}")
