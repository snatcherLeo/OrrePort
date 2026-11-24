"""Infer the field widths of common_rel's data tables from the game code (OrrePort byte-order work, port-gaps 21).

On the PC the tables are converted to native order when the PC data is generated (the project notes "Decided while snatcherLeo was
away", 2026-10-15 night). That needs each record's field widths. They come from the code that reads the records:
  - the table's field getters: functions named like the table's `xxxGetPtr` getter with the `GetPtr` part dropped
    (`wazaDataBiosGetPtr` -> every `wazaDataBios*` function), reading through their pointer parameter;
  - locals assigned from the `GetPtr` call (`move = wazaDataBiosGetPtr(id);`) and reads through them in the same
    function, and direct reads `*(T*)(xxxGetPtr(id) + off)`.
Read forms: `*(T*)(p + C)`, `*(T*)((u8*)p + C)`, `*(T*)(&p[C])`, `p[C]` / `p[i + C]` (bytes).
Output (stdout, or --json FILE): per table, each offset's widths and where they were seen, the conflicts (one offset
read with two widths, or overlapping fields), and how much of the record is covered. Bytes never read stay as they
are (a byte needs no swap; an unread wider field is listed as a gap to check).
Usage (repo root): python -I tools/local/port/tablewidths.py [--json build-pc/table_widths.json] [--table NAME]
"""
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

# pointer global -> (short name, record size, GetPtr getter); from port-gaps 21 / game-data-tables.md
TABLES = {
    "lbl_80478F94": ("species", 0x11C, "pokemonDataBiosGetPtr"),
    "waza_data": ("moves", 0x38, "wazaDataBiosGetPtr"),
    "pokemon_seikaku_data": ("natures", 0x28, "pokemonSeikakuDataBiosGetPtr"),
    "pokemon_seikaku_rate_data": ("nature_rates", 0x2, "pokemonSeikakuRateDataBiosGetPtr"),
    "pokemon_dp_filter_data": ("dp_filter", 0x1, "pokemonDpFilterDataBiosGetPtr"),
    "lbl_80478F24": ("trainers", 0x34, "fightTrainerDataBiosGetPtr"),
    "lbl_80478F14": ("trainer_pokemon", 0x50, "fightTrainerPokemonDataBiosGetPtr"),
    "fight_trainer_kind_data": ("trainer_classes", 0xC, "fightTrainerKindDataBiosGetPtr"),
    "lbl_80478F34": ("trainer_ai", 0x28, "fightTrainerAiDataBiosGetPtr"),
    "fight_trainer_ai_value_addsub_data": ("trainer_ai_addsub", 0x14, "fightTrainerAiValueAddsubDataBiosGetPtr"),
    "fight_trainer_pokemon_part_data": ("trainer_pokemon_parts", 0x14, "fightTrainerPokemonPartDataBiosGetPtr"),
    "fight_encount_data": ("encounters", 0x38, "fightEncountDataBiosGetPtr"),
    "fight_kind_data": ("battle_kinds", 0x20, "fightKindDataBiosGetPtr"),
    "fight_type_data": ("battle_types", 0x8, "fightTypeDataBiosGetPtr"),
    "fight_side_data": ("battle_sides", 0x14, "fightSideDataBiosGetPtr"),
    "fight_floor_data": ("battle_floors", 0x18, "fightFloorDataBiosGetPtr"),
    "peopleInfoData": ("npc_info", 0x2C, "peopleInfoBiosGetPtr"),
    "lbl_80478E74": ("menu_faces", 0x8, "_menuFaceBiosGetPtr__FUs"),
    "menu_se": ("menu_se", 0xA, "menuSeBiosGetPtr"),
    "lbl_80478E0C": ("resources", 0x4, "tableResBiosGetResPtr"),
    "lbl_80363CE8": ("items", 0x28, "itemDataBiosGetPtr"),  # in main.dol .data (not common_rel)
    "lbl_802EF0A8": ("menu_items", 0x1C, "menuItemBiosGetPtr"),  # main.dol .data, big-endian views (menus.md)
    "_menu_sprite": ("menu_sprite", 0x18, "menuSpriteBiosGetPtr"),  # main.dol .data, big-endian views
}

TYPES = {"u8": 1, "s8": 1, "char": 1, "u16": 2, "s16": 2, "u32": 4, "s32": 4, "f32": 4, "int": 4, "float": 4,
         "unsigned int": 4, "BOOL": 4, "u64": 8, "s64": 8, "f64": 8, "double": 8}
FUNC = re.compile(r"^(?!(?:if|for|while|switch|return|else)\b)[A-Za-z_][\w \*]*?\b([A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*\{?\s*$")


def load_sources():
    for f in sorted(Path("src/game").rglob("*.c")) + sorted(Path("src/game").rglob("*.cpp")):
        yield f, f.read_text(encoding="utf-8", errors="replace").splitlines()


def functions(lines):
    """[(name, params, first line, last line)] by brace depth from each definition line."""
    out, i = [], 0
    while i < len(lines):
        m = FUNC.match(lines[i])
        if m and not lines[i].rstrip().endswith(";"):
            j, depth, seen = i, 0, False
            while j < len(lines):
                depth += lines[j].count("{") - lines[j].count("}")
                seen = seen or "{" in lines[j]
                if seen and depth <= 0:
                    break
                j += 1
            if seen:
                out.append((m.group(1), m.group(2), i, j))
                i = j + 1
                continue
        i += 1
    return out


def reads(text, var):
    """(offset, width, form) for reads through `var` in text."""
    v = re.escape(var)
    for m in re.finditer(r"\*\s*\(\s*(?:const\s+)?(" + "|".join(sorted(TYPES, key=len, reverse=True)) +
                         r")\s*\*\s*\)\s*\(\s*(?:\(\s*(?:const\s+)?u8\s*\*\s*\)\s*)?" + v + r"\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\)", text):
        yield int(m.group(2), 0), TYPES[m.group(1)], "cast"
    for m in re.finditer(r"\*\s*\(\s*(?:const\s+)?(" + "|".join(sorted(TYPES, key=len, reverse=True)) +
                         r")\s*\*\s*\)\s*\(\s*&\s*" + v + r"\s*\[\s*(0x[0-9A-Fa-f]+|\d+)\s*\]\s*\)", text):
        yield int(m.group(2), 0), TYPES[m.group(1)], "cast&[]"
    for m in re.finditer(r"(?<![\w.>])" + v + r"\s*\[\s*(?:[^\[\]]*?\+\s*)?(0x[0-9A-Fa-f]+|\d+)\s*\]", text):
        pre = text[max(0, m.start() - 3):m.start()]
        if "&" in pre:
            continue
        yield int(m.group(1), 0), 1, "byte[]"


def main():
    only = sys.argv[sys.argv.index("--table") + 1] if "--table" in sys.argv else None
    seen = {g: defaultdict(set) for g in TABLES}  # offset -> {(width, where)}
    prefixes = {}
    for g, (name, size, getter) in TABLES.items():
        base = getter.split("__")[0].lstrip("_")
        prefixes[g] = base[:-len("GetPtr")] if base.endswith("GetPtr") else base
    for f, lines in load_sources():
        for fname, params, a, b in functions(lines):
            body = "\n".join(lines[a:b + 1])
            for g, (name, size, getter) in TABLES.items():
                vars_ = set()
                bare = fname.lstrip("_")
                if bare.startswith(prefixes[g]) and fname != getter:
                    for pm in re.finditer(r"\*\s*([A-Za-z_]\w*)\s*(?:,|$)", params):
                        vars_.add(pm.group(1))
                        break
                for am in re.finditer(r"\b([A-Za-z_]\w*)\s*=\s*(?:\([^()]*\)\s*)?" + re.escape(getter) + r"\s*\(", body):
                    vars_.add(am.group(1))
                for v in vars_:
                    for off, w, form in reads(body, v):
                        if off < size:
                            seen[g][off].add((w, f"{f.name}:{fname}"))
                for dm in re.finditer(r"\*\s*\(\s*(" + "|".join(sorted(TYPES, key=len, reverse=True)) + r")\s*\*\s*\)\s*\(\s*(?:\(u8\s*\*\)\s*)?"
                                      + re.escape(getter) + r"\s*\([^;]*?\)\s*\+\s*(0x[0-9A-Fa-f]+|\d+)\s*\)", body):
                    off = int(dm.group(2), 0)
                    if off < size:
                        seen[g][off].add((TYPES[dm.group(1)], f"{f.name}:{fname}"))
    result = {}
    for g, (name, size, getter) in TABLES.items():
        if only and only != name:
            continue
        fields, conflicts = {}, []
        for off in sorted(seen[g]):
            widths = sorted({w for w, _ in seen[g][off]})
            if len(widths) > 1:
                conflicts.append(f"0x{off:X}: widths {widths}")
            fields[off] = max(widths)
        prev_end, prev = 0, None
        for off in sorted(fields):
            if prev is not None and off < prev_end and fields[prev] > 1:
                conflicts.append(f"0x{off:X} inside the {fields[prev]}-byte field at 0x{prev:X}")
            if off + fields[off] > prev_end:
                prev_end, prev = off + fields[off], off
        covered = sum(fields.values())
        result[name] = {"pointer": g, "record": size, "fields": {f"0x{o:X}": w for o, w in fields.items()},
                        "seen": {f"0x{o:X}": sorted({wh for _, wh in seen[g][o]})[:4] for o in fields},
                        "conflicts": conflicts, "bytes_covered": covered}
        print(f"{name} ({g}, record 0x{size:X}): {len(fields)} fields, {covered}/{size} bytes read, "
              f"{len(conflicts)} conflicts")
        print("   " + " ".join(f"{o:X}:{w}" for o, w in sorted(fields.items())))
        for c in conflicts:
            print("   CONFLICT", c)
    if "--json" in sys.argv:
        Path(sys.argv[sys.argv.index("--json") + 1]).write_text(json.dumps(result, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
