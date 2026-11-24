"""2026-11-10: what the event log (port/src/pc_event_log.c, the hub branch) saw, against the game's full lists:
the post-game sweep's coverage (run3-sweep-plan.md, step 0) and the story-flag catalogue's raw list.

    python -I tools/local/port/event_coverage.py <log> [<log> ...] [--flags OUT.tsv] [--missing]

Reads the logs' `[battle] action move` lines (species, move, effect id), `[battle] action item` lines, `[battle] mon`
lines (the species on the field, both sides) and `[btext]` lines (the battle text: abilities and held items show up
there by name), and the `[flag]` lines. The full lists come from the repo's own name headers
(include/game/constants/moves.h, abilities.h, species.h, items.h), so no game data is read. Prints the counts;
`--missing` lists what is still missing; `--flags` writes every flag change in log order (log, frame, room, flag,
old, new) as a tab-separated file.
"""
import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HEADERS = ROOT / "include" / "game" / "constants"


def read_header(name, prefix, skip=()):
    """{number: NAME} from a constants header's #defines (the disc's own names)"""
    out = {}
    for line in (HEADERS / name).read_text(encoding="utf-8").splitlines():
        m = re.match(r"#define\s+" + prefix + r"([A-Z0-9_]+)\s+(\d+)", line)
        if m and m.group(1) not in skip and not m.group(1).startswith("UNUSED") and not m.group(1).startswith("PLACEHOLDER"):
            out[int(m.group(2))] = m.group(1)
    return out


MOVES = read_header("moves.h", "MOVE_", skip=("NONE", "COUNT"))
ABILITIES = read_header("abilities.h", "ABILITY_", skip=("NONE", "COUNT"))
SPECIES = read_header("species.h", "SPECIES_", skip=("NONE", "COUNT", "EGG"))
ITEMS = read_header("items.h", "ITEM_", skip=("NONE", "COUNT"))


def spoken(name):
    """a header name as the battle text prints it: DOWN_ST_KEY -> DOWN ST KEY"""
    return name.replace("_", " ")


def norm(text):
    """a name compared loosely: letters and digits only, one space between words (DOUBLE-EDGE = DOUBLE_EDGE)"""
    return " ".join(re.sub(r"[^A-Z0-9]+", " ", text.upper()).split())


MOVE_BY_NAME = {norm(name): n for n, name in MOVES.items()}
MOVE_LINE = re.compile(r"^\[battle\] action move: species (\d+) uses move (\d+) .*? effect (\d+) ")
ITEM_LINE = re.compile(r"^\[battle\] action item: species (\d+) uses item (\d+) ")
MON_LINE = re.compile(r"^\[battle\] mon (\d+) hp=\S+ types \S+(?: ability (\d+) item (\d+))?")
USED_TEXT = re.compile(r" USED (.+?)!")
FLAG_LINE = re.compile(r"^\[flag\] (-?\d+): (\d+) -> (\d+) \(frame (\d+), room 0x([0-9A-Fa-f]+)\)")
TEXT_LINE = re.compile(r"^\[btext\] (.*?)\s{3}\(message")
# 2026-11-10: the whole team at a battle's first turn, the field / menu text with its speaker, the battle number
TEAM_LINE = re.compile(r"^\[battle\] team (?:trainer )?\d+:(.*)$")
TEAM_MON = re.compile(r"(\d+) lv (\d+) ability (\d+) item (\d+) moves (\d+) (\d+) (\d+) (\d+)")
DIALOG_LINE = re.compile(r"^\[text\] (.*?)\s{3}\(message 0x([0-9A-Fa-f]+), box 0x([0-9A-Fa-f]+), frame (\d+), "
                         r"room 0x([0-9A-Fa-f]+)(, from the bank)?(?:, speaker (.*))?\)$")
SPEAKER_NAME = re.compile(r"^(.*?) \(name record (\d+), people group (\d+) index (\d+)\)$")
BATTLE_START = re.compile(r"^\[battle\] start (\d+)")

# Rooms by place, for speakers the game leaves nameless (its speech-bubble icon, U+2031): "NPC in <place> (<room>)".
# The room ids' archive names come from route.json's room graph; the prefixes' places from fsys-catalog.md's table.
PLACES = {"S1": "Outskirt Stand", "M1": "Phenac City", "M2": "Pyrite Town", "M3": "Agate Village", "M4": "The Under",
          "D1": "Shadow Pokémon Lab", "D2": "Mt. Battle", "D4": "Realgam Tower", "S2": "Team Snagem Hideout",
          "T1": "a Colosseum arena"}


def room_archives():
    """{room id: archive name} from docs/testing/route.json's room graph"""
    import json
    out = {}
    try:
        graph = json.loads((ROOT / "docs" / "testing" / "route.json").read_text(encoding="utf-8"))["room_graph"]
    except (OSError, KeyError, ValueError):
        return out
    for e in graph.get("exits", []):
        out.setdefault(e["from"], e.get("from_archive", ""))
        out.setdefault(e["to"], e.get("to_archive", ""))
    return out


ROOMS = room_archives()


def room_names():
    """{room id: (ROOM_ name, place comment)} from include/game/constants/rooms.h (all 196 rooms, menus included)"""
    out = {}
    for line in (HEADERS / "rooms.h").read_text(encoding="utf-8").splitlines():
        m = re.match(r"#define\s+ROOM_(\w+)\s+(\d+)\s*(?:/\*\s*(.*?)\s*\*/)?", line)
        if m:
            out[int(m.group(2))] = (m.group(1), m.group(3) or "")
    return out


ROOM_NAMES = room_names()


def place_of(room_hex):
    """'Pyrite Town (M2_police_1F)' for a room id (rooms.h first, then route.json's room graph), or 'room 0x10'"""
    rid = int(room_hex, 16)
    if rid in ROOM_NAMES:
        name, place = ROOM_NAMES[rid]
        return f"{place} ({name})" if place else name.lower()
    archive = ROOMS.get(rid, "")
    if not archive:
        return f"room 0x{room_hex.upper()}"
    return f"{PLACES.get(archive[:2], archive[:2])} ({archive})"


LINE_NAME = re.compile(r"^([A-Z][A-Z0-9 .'\-é]{1,20}): ")


def speaker_label(speaker, room_hex, text=""):
    """the speaker as the dialogue list shows it: the game's name record when it has one, else the name the line
    itself starts with ("JOHNSON: ..."), else 'NPC G-I in <place>' (the game's nameless speech-bubble speakers)"""
    if not speaker:
        return ""
    if m := SPEAKER_NAME.match(speaker):
        name, record, group, index = m.groups()
        if name not in ("?", "") and record != "0":
            return f"{name} (people {group}-{index})"
        if n := LINE_NAME.match(text):
            return f"{n.group(1)} (people {group}-{index}, named in the line)"
        return f"NPC {group}-{index} in {place_of(room_hex)}"
    if m := re.match(r"people group (\d+) index (\d+)", speaker):
        return f"NPC {m.group(1)}-{m.group(2)} in {place_of(room_hex)}"
    return speaker


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--flags", help="write the flag changes, in log order, to this .tsv")
    ap.add_argument("--missing", action="store_true", help="list what is still missing")
    ap.add_argument("--dialogue", help="write every dialogue line (log, frame, room, message, speaker, text) to this .tsv")
    ap.add_argument("--teams", help="write every battle's teams (log, battle, trainer slot, species, level, ability, "
                                    "item, moves) to this .tsv")
    a = ap.parse_args()

    moves, effects, items_used, species, texts = {}, {}, {}, set(), []
    abilities_on_field, items_held = set(), set()
    flag_rows = []
    dialog_rows, speakers, team_rows = [], set(), []
    team_species, team_abilities, team_items, team_moves = set(), set(), set(), set()
    battle = "-"
    for log in a.logs:
        for line in Path(log).read_text(encoding="utf-8", errors="replace").splitlines():
            if m := BATTLE_START.match(line):
                battle = m.group(1)
            elif m := TEAM_LINE.match(line):
                order = line.split("team ", 1)[1].split(":", 1)[0].replace("trainer ", "")
                slot = "player" if order == "0" else f"opponent {order}"
                for mon in TEAM_MON.finditer(m.group(1)):
                    sp, lv, ab, it, *mv = (int(x) for x in mon.groups())
                    team_species.add(sp)
                    team_abilities.add(ab)
                    if it:
                        team_items.add(it)
                    team_moves.update(x for x in mv if x)
                    team_rows.append((Path(log).name, battle, slot, SPECIES.get(sp, str(sp)), str(lv),
                                      ABILITIES.get(ab, str(ab)),
                                      ITEMS.get(it, str(it)) if it else "-",
                                      ", ".join(MOVES.get(x, str(x)) for x in mv if x)))
            elif m := DIALOG_LINE.match(line):
                text, key, box, frame, room, bank, speaker = m.groups()
                if speaker and (s := SPEAKER_NAME.match(speaker)) and s.group(1) != "?":
                    speakers.add(s.group(1))
                dialog_rows.append((Path(log).name, frame, place_of(room), "0x" + key.upper(),
                                    speaker_label(speaker, room, text), text))
            elif m := MOVE_LINE.match(line):
                user, move, effect = (int(x) for x in m.groups())
                moves[move] = moves.get(move, 0) + 1
                effects.setdefault(effect, set()).add(move)
                species.add(user)
            elif m := ITEM_LINE.match(line):
                user, item = int(m.group(1)), int(m.group(2))
                items_used[item] = items_used.get(item, 0) + 1
                species.add(user)
            elif m := MON_LINE.match(line):
                species.add(int(m.group(1)))
                if m.group(2) is not None:
                    abilities_on_field.add(int(m.group(2)))
                    if int(m.group(3)) != 0:
                        items_held.add(int(m.group(3)))
            elif m := FLAG_LINE.match(line):
                flag, old, new, frame, room = m.groups()
                flag_rows.append((Path(log).name, frame, "0x" + room.upper(), flag, old, new))
            elif m := TEXT_LINE.match(line):
                texts.append(m.group(1).upper())

    text = "\n".join(texts)
    # moves that ran, from the battle text too ("X used Y!": also the moves another move called, Metronome's pick)
    moves_in_text = set()
    for t in texts:
        for m in USED_TEXT.finditer(" " + t):
            if (n := MOVE_BY_NAME.get(norm(m.group(1)))) is not None:
                moves_in_text.add(n)
    abilities_named = {n for n, name in ABILITIES.items() if spoken(name) in text}
    items_named = {n for n, name in ITEMS.items() if f"ITS {spoken(name)}" in text or f"{spoken(name)}!" in text}
    species = species | team_species
    abilities_on_field = abilities_on_field | team_abilities
    items_held = items_held | team_items
    species_seen = species & set(SPECIES)
    moves_seen = set(moves) | moves_in_text

    def line(label, seen, full, note=""):
        print(f"{label:<20} {len(seen & set(full)):>4} of {len(full):>4}{note}")

    print(f"logs: {len(a.logs)}; battle text lines: {len(texts)}; flag changes: {len(flag_rows)}")
    line("moves used", moves_seen, MOVES, f"  ({len(set(moves))} chosen, {len(moves_in_text)} named in text)")
    line("abilities named", abilities_named, ABILITIES, "  (its name in a battle line: it visibly acted)")
    line("abilities on field", abilities_on_field, ABILITIES, "  (a battler had it; it may have acted silently)")
    line("species on field", species_seen, SPECIES)
    print(f"{'move effects':<20} {len(effects):>4} of  214 (Gen 3's effect ids 0-213)")
    print(f"{'items used':<20} {len(items_used):>4} kinds from the bag in battle")
    print(f"{'items held':<20} {len(items_held):>4} kinds held by a battler")
    print(f"{'items named':<20} {len(items_named):>4} kinds named in battle text (taking effect, or used)")
    line("moves known", team_moves, MOVES, "  (on a team at a battle's start, used or not)")
    print(f"{'team lines':<20} {len(team_rows):>4} Pokémon over the battles' teams")
    print(f"{'dialogue lines':<20} {len(dialog_rows):>4} ({len({r[3] for r in dialog_rows})} distinct messages; "
          f"{sum(1 for r in dialog_rows if r[4])} with the game's speaker; {len(speakers)} named speakers)")

    if a.missing:
        def missing(label, full, seen):
            rest = [f"{n} {full[n]}" for n in sorted(full) if n not in seen]
            print(f"\nmissing {label} ({len(rest)}):")
            print("  " + ", ".join(rest))
        missing("moves", MOVES, moves_seen)
        missing("abilities (never named in a battle line)", ABILITIES, abilities_named)
        missing("species", SPECIES, species_seen)
        print(f"\nmove effects seen (effect: moves): " +
              "; ".join(f"{e}: {', '.join(MOVES.get(m, str(m)) for m in sorted(ms))}" for e, ms in sorted(effects.items())))

    if a.flags:
        with open(a.flags, "w", encoding="utf-8", newline="") as f:
            f.write("log\tframe\troom\tflag\told\tnew\n")
            for row in flag_rows:
                f.write("\t".join(row) + "\n")
        print(f"\nflag changes written: {a.flags} ({len(flag_rows)} rows)")
    # a person named in any line (by the game's record or by the line's own NAME:) names their nameless lines in the
    # same place too ("SHERLES (people 46-1, named elsewhere)"): people numbers are per room, so the place is part of
    # the match
    known = {}
    for r in dialog_rows:
        if (m := re.match(r"^(.+?) \(people (\d+-\d+)", r[4])) and not r[4].startswith("NPC "):
            known.setdefault((r[2], m.group(2)), m.group(1))
    for i, r in enumerate(dialog_rows):
        if (m := re.match(r"^NPC (\d+-\d+) in ", r[4])) and (r[2], m.group(1)) in known:
            dialog_rows[i] = r[:4] + (f"{known[(r[2], m.group(1))]} (people {m.group(1)}, named elsewhere)",) + r[5:]
    # the bank's "<speaker>" placeholder (the game's Npc insert, code 0x59) is the line's own speaker: fill it in
    for i, r in enumerate(dialog_rows):
        if "<speaker>" in r[5] and (m := re.match(r"^(.+?) \(people ", r[4])) and not r[4].startswith("NPC "):
            dialog_rows[i] = r[:5] + (r[5].replace("<speaker>", m.group(1)),)
    for path, header, rows in ((a.dialogue, "log\tframe\troom\tmessage\tspeaker\ttext", dialog_rows),
                               (a.teams, "log\tbattle\ttrainer\tspecies\tlevel\tability\titem\tmoves", team_rows)):
        if path:
            with open(path, "w", encoding="utf-8", newline="") as f:
                f.write(header + "\n")
                for row in rows:
                    f.write("\t".join(r.replace("\t", " ") for r in row) + "\n")
            print(f"written: {path} ({len(rows)} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
