"""2026-11-10: the sweep's loadouts (run3-sweep-plan.md, Part 1): pack what the event logs never saw into legal
teams of six for snatcherLeo's Mt. Battle climb.

    python -I tools/local/port/loadout_builder.py <event logs ...> --out loadouts.txt [--disc-root orig/GC6E01]

"Legal" (snatcherLeo, 2026-11-10: "Th eones they can legally learn"): what Colosseum's own data allows a species, read from
the disc: its level-up moves (species record +0xBA, 20 x {level, move}), its TM / HM compatibility (+0x34, one byte per
machine, with the machine table item_waza_machine_no in main.dol giving each machine's move) and its egg moves (+0x74,
8 moves; marked "egg", reachable by trading from a GBA game); abilities from its two ability slots (+0x32).
What's missing: moves never used, species never on the field, abilities never on the field, held items never held,
from the event logs (the same patterns as event_coverage.py).

Each loadout: six Pokemon at level 100, up to four missing moves each, a missing held item each when any are left.
Greedy: the species that covers the most still-missing moves goes first, with a bonus for a missing species, a missing
ability and single-learner moves. Prints each loadout and what can't be covered legally. Reads the extraction
(`extracted/data_tables/species.csv`, `items_decoded.csv`) and the disc's main.dol, so the output (game data) goes
outside git.
"""
import argparse
import csv
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import event_coverage as ec  # noqa: E402

ROOT = ec.ROOT
TM_TABLE = 0x80368018  # item_waza_machine_no (symbols.txt): 58 x {u32 is_hm, u32 move}
SKIP_MOVES = {"UNUSED_355", "SHADOW_LOCKED", "SHADOW_RUSH", "STRUGGLE"}  # Struggle: its own setup (no PP left)
# Moves no species learns in Colosseum's own data, legal in Gen 3 only on a Pokemon traded in from another game
# (snatcherLeo, 2026-11-10: "Yes, include them"): FireRed / LeafGreen's move tutor (the three starter moves), Emerald's
# breeding (Volt Tackle, a Pichu egg move), XD's move tutor (Nightmare). Each on one species that learned it there.
TRADED_IN = {"BLAST_BURN": "CHARIZARD", "HYDRO_CANNON": "BLASTOISE", "SOLID_PLANT": "VENUSAUR",
             "VOLT_TACKLE": "PIKACHU", "NIGHTMARE": "GENGAR"}


def dol_reader(path):
    d = Path(path).read_bytes()
    offs = struct.unpack(">18I", d[0:72])
    addrs = struct.unpack(">18I", d[72:144])
    sizes = struct.unpack(">18I", d[144:216])

    def rd(addr, n):
        for o, a, s in zip(offs, addrs, sizes):
            if a <= addr < a + s:
                return d[o + addr - a:o + addr - a + n]
        raise ValueError(hex(addr))
    return rd


def species_records():
    """{species: bytes of its 0x11C record} from the extraction's raw words"""
    out = {}
    for r in csv.DictReader(open(ROOT / "extracted" / "data_tables" / "species.csv", encoding="utf-8")):
        words = [k for k in r if k.startswith("w")]
        out[int(r["index"])] = b"".join(int(r[k], 16).to_bytes(4, "big") for k in words)
    return out


def learnset(rec, tm_moves):
    """(set of legal moves, set of egg-only moves, (ability 1, ability 2))"""
    moves = set()
    for i in range(20):
        lvl, mv = rec[0xBA + 4 * i], struct.unpack(">H", rec[0xBC + 4 * i:0xBE + 4 * i])[0]
        if mv:
            moves.add(mv)
    for m in range(58):
        if rec[0x34 + m]:
            moves.add(tm_moves[m])
    egg = {struct.unpack(">H", rec[0x74 + 2 * i:0x76 + 2 * i])[0] for i in range(8)} - {0}
    return moves | egg, egg - moves, (rec[0x32], rec[0x33])


def seen_from_logs(logs):
    moves, species, abilities, items = set(), set(), set(), set()
    for log in logs:
        for line in Path(log).read_text(encoding="utf-8", errors="replace").splitlines():
            if m := ec.MOVE_LINE.match(line):
                moves.add(int(m.group(2)))
                species.add(int(m.group(1)))
            elif m := ec.MON_LINE.match(line):
                species.add(int(m.group(1)))
                if m.group(2) is not None:
                    abilities.add(int(m.group(2)))
                    items.add(int(m.group(3)))
            elif m := ec.TEAM_LINE.match(line):
                for mon in ec.TEAM_MON.finditer(m.group(1)):
                    sp, lv, ab, it, *mv = (int(x) for x in mon.groups())
                    species.add(sp)
                    abilities.add(ab)
                    items.add(it)
            elif m := ec.TEXT_LINE.match(line):
                for u in ec.USED_TEXT.finditer(" " + m.group(1).upper()):
                    if (n := ec.MOVE_BY_NAME.get(ec.norm(u.group(1)))) is not None:
                        moves.add(n)
    return moves, species, abilities, items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs", nargs="+")
    ap.add_argument("--out", required=True)
    ap.add_argument("--disc-root", default=str(ROOT / "orig" / "GC6E01"))
    a = ap.parse_args()

    rd = dol_reader(Path(a.disc_root) / "sys" / "main.dol")
    raw = rd(TM_TABLE, 58 * 8)
    tm_moves = [struct.unpack(">II", raw[8 * i:8 * i + 8])[1] for i in range(58)]
    recs = species_records()
    seen_moves, seen_species, seen_abilities, seen_items = seen_from_logs(a.logs)

    wanted_moves = {n for n, name in ec.MOVES.items() if name not in SKIP_MOVES and n not in seen_moves}
    wanted_species = {n for n in ec.SPECIES if n not in seen_species}
    wanted_abilities = {n for n, name in ec.ABILITIES.items() if name != "CACOPHONY" and n not in seen_abilities}
    held = [int(r["item_id"]) for r in csv.DictReader(open(ROOT / "extracted" / "data_tables" / "items_decoded.csv",
                                                             encoding="utf-8"))
            if r["name"] and r["held_effect_id"] not in ("", "0")]
    wanted_items = [i for i in held if i not in seen_items]

    legal = {}
    for sp in ec.SPECIES:
        if sp in recs:
            legal[sp] = learnset(recs[sp], tm_moves)
    traded = {}  # (species, move) pairs legal only as a traded-in Pokemon
    by_name = {name: n for n, name in ec.SPECIES.items()}
    move_by_name = {name: n for n, name in ec.MOVES.items()}
    for move, species in TRADED_IN.items():
        sp, mv = by_name[species], move_by_name[move]
        legal[sp][0].add(mv)
        traded.setdefault(sp, set()).add(mv)
    learners = {}
    for sp, (mv, egg, ab) in legal.items():
        for m in mv:
            learners.setdefault(m, set()).add(sp)
    unlearnable = sorted(m for m in wanted_moves if not learners.get(m))

    lines = [f"missing: moves {len(wanted_moves)}, species {len(wanted_species)}, abilities {len(wanted_abilities)}, "
             f"held items {len(wanted_items)}",
             "no species can legally learn: " + (", ".join(ec.MOVES[m] for m in unlearnable) or "none"), ""]
    moves_left = set(wanted_moves) - set(unlearnable)
    species_left, abilities_left, items_left = set(wanted_species), set(wanted_abilities), list(wanted_items)
    teams = []
    while moves_left or abilities_left:
        team, used = [], set()
        for _ in range(6):
            best, best_score, best_moves = None, 0, []
            for sp, (mv, egg, ab) in legal.items():
                if sp in used:
                    continue
                can = sorted(mv & moves_left, key=lambda m: len(learners[m]))[:4]
                needs_ability = bool(set(ab) & abilities_left)
                if not can and not needs_ability:
                    continue  # a missing species alone is option A's job (Part 2), not a loadout slot
                score = sum(4 if len(learners[m]) == 1 else 1 for m in can)
                score += 1 if (sp in species_left and can) else 0  # tie-break: a new species, when it brings moves
                score += 5 if needs_ability else 0
                if score > best_score:
                    best, best_score, best_moves = sp, score, can
            if best is None:
                break
            used.add(best)
            mv, egg, ab = legal[best]
            ability = next((x for x in ab if x in abilities_left), ab[0])
            item = items_left.pop(0) if items_left else 0
            moves_left -= set(best_moves)
            species_left.discard(best)
            abilities_left.discard(ability)
            team.append([best, ability, item, list(best_moves), egg, ability in wanted_abilities])
        if not team:
            break
        teams.append(team)

    # packing pass: empty the last loadouts into free move slots of earlier Pokemon that can legally learn the moves
    # (and free item slots), so the climb fits Mt. Battle's areas; a member that carries a missing ability stays
    def compact():
        """drop any member whose moves (and item) all fit on other members; fewest moves first. True if one went"""
        members = sorted(((ti, m) for ti, t in enumerate(teams) for m in t), key=lambda x: len(x[1][3]))
        for ti, member in members:
            sp, ability, item, mvs, egg, needed_ability = member
            if needed_ability:
                continue
            others = [h for t in teams for h in t if h is not member]
            plan = []
            for m in mvs:
                host = next((h for h in others
                             if len(h[3]) + sum(1 for x in plan if x[0] is h) < 4 and m in legal[h[0]][0]), None)
                if host is None:
                    break
                plan.append((host, m))
            else:
                item_host = next((h for h in others if not h[2]), None) if item else None
                if item and item_host is None:
                    continue
                for host, m in plan:
                    host[3].append(m)
                if item:
                    item_host[2] = item
                teams[ti].remove(member)
                teams[:] = [t for t in teams if t]
                return True
        return False
    while sum(len(t) for t in teams) > 6 * (len(teams) - 1) and compact():
        pass
    # top up: a loadout short of six, merged into another with room
    while len(teams) > 1 and sum(len(t) for t in teams) <= 6 * (len(teams) - 1):
        small = min(range(len(teams)), key=lambda i: len(teams[i]))
        members = teams.pop(small)
        for member in members:
            dest = next(t for t in teams if len(t) < 6 and all(m[0] != member[0] for m in t))
            dest.append(member)
    loadout = len(teams)
    species_left = set(wanted_species) - {m[0] for t in teams for m in t}  # after the packing pass dropped members

    for n, team in enumerate(teams, 1):
        lines.append(f"Loadout {n}:")
        for sp, ability, item, mvs, egg, _ in team:
            names = [ec.MOVES[m] + (" (egg)" if m in egg else "") + (" (traded in)" if m in traded.get(sp, ()) else "")
                     for m in mvs]
            lines.append(f"  {ec.SPECIES[sp]:<12} ability {ec.ABILITIES.get(ability, ability):<14} item "
                         f"{ec.ITEMS.get(item, '-') if item else '-':<14} moves: {', '.join(names) or '(none left)'}")
        lines.append("")
    lines.append(f"loadouts: {loadout}; moves still missing after them: {len(moves_left)}; species still missing: "
                 f"{len(species_left)} (option A's model pass); held items still missing: {len(items_left)}")
    Path(a.out).write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines[:3] + lines[-1:]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
