"""The playthrough's section checklists (2026-10-18; docs/testing/playthrough-guide.md section 5).

    python -I tools/local/testing/section_checklist.py <disc image> [--root <decomp>/_playthrough]

Writes ROOT/checklists/SNN_<name>.md (one per section) and ROOT/checklists/README.md (the overview with counts). They
are generated from game data, so they stay outside git, like extracted/; rerun the tool after every sitting: rows are
ticked from all sittings' logs. Sources:
- the story steps, their flags, rooms and battles: docs/engine/story-route.md (the step tables; the section's step
  range below, parts 4-5 split by the rooms the steps name);
- items in the world: extracted/data_tables/treasure_decoded.csv (room, item, the "picked up" flag);
- people: npc_placements_decoded.csv (named people per room: talk to each once);
- Shadow Pokémon: shadow_pokemon_decoded.csv (their trainers), placed in the section whose steps name the trainer;
- rooms: rooms_decoded.csv; a room belongs to the first section whose steps name it, else to its area's section.
Ticks: a step when all its flags are set; a battle when it was fought; an item when its flag is set; a room when it
was entered; people can't be ticked from the log yet (marked "by hand").
"""
import argparse, csv, glob, json, os, re, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)                                   # python -I leaves the script's own folder out
TABLES = os.path.join(REPO, "extracted", "data_tables")
if not os.path.isdir(TABLES):   # a lane worktree has no extracted/: the main checkout's (pkmn_colosseum)
    TABLES = os.path.join(REPO, "..", "pkmn_colosseum", "extracted", "data_tables")
from playrec import SECTIONS, DEFAULT_ROOT, all_sittings   # noqa: E402

STEP_RANGES = [("S01", 1, 9), ("S02", 10, 23), ("S03", 24, 40), ("S04", 41, 44), ("S05", 45, 53), ("S06", 54, 54),
               ("S07", 55, 64), ("S08", 65, 77), ("S09", 78, 83)]
AREA_SECTION = {"Outskirt Stand": "S01", "Phenac City": "S02", "Pyrite Town": "S03", "Agate Village": "S07",
                "Mt Battle": "S08", "The Under": "S10", "Shadow Pokemon Lab": "S12", "Realgam Tower": "S13",
                "Team Snagem Hideout": "S15", "Colosseum Arena T1": "S15"}   # the hideout is postgame (story-route)
# where the data alone gives the wrong section: trainer id -> (section, why)
TRAINER_OVERRIDE = {472: ("S15", "the shady guy outside the diner is **Fein impersonating Wes**, a postgame event "
                                 "(Shadow Togetic Lv 20, battle 474; likely, story-route.md postgame groups)"),
                    378: ("S15", "Deep King Agnol (Shadow Shuckle Lv 45) is fought in the **Orre Colosseum** in The "
                                 "Under's basement, postgame (battles 380 / 565; likely, story-route.md)")}
EXTRA = {"S02": ["**Planned losses** with only Sentret in the party (2-3 times, an ordinary trainer; blackout: half the "
                 "money, healed, back to the return point): playthrough-guide.md section 6"],
         "S09": ["Get the **Cologne Case** officially (story step 78), save at a PC, end the sitting: **save edit E2** "
                 "(x99 Vivid Scent)"],
         "S15": ["(story-route.md \"The postgame\", groups, likely all optional) Folly and Trudly at Phenac's mayor's "
                 "house (battles 16, 18 / 19); Reath and Ferma in Pyrite Cave (battles 125-131)",
                 "the **Team Snagem hideout** (S2_building_*, S2_out_2): Snagem members, the Mystery Troop again "
                 "(527-529), **Gonzap** (428 / 429), the **D-DISK** (treasure)",
                 "the Shadow Pokémon Lab's peons again, with the Mystery Troop's later records (562-564)",
                 "the **Orre Colosseum** in The Under's basement (M4_enter_B1): the admins and Deep King Agnol "
                 "(380 / 565); TM prizes TM02, TM12, TM44, TM48",
                 "Outskirt Stand: Fein impersonating Wes (battle 474: Shadow Togetic) and the TV reporter scene; "
                 "Willie's later rematches",
                 "**read every new mail** in the P★DA after each scene that sends one (the postgame mail chain gates "
                 "later events)",
                 "the Ho-Oh gift (unused-content.md, menu-rooms.md)",
                 "purify the rest (optional): Relic Stone, walking, scents"],
         "S16": ["GBA link (Dolphin's built-in mGBA: check)",
                 "the trades with the linked GBA game in Phenac's Pokémon Center basement",
                 "Battle Mode with GBA Pokémon (snatcherLeo, 2026-10-20): Colosseum Battle -> Solo Battle -> Register "
                 "Pokémon -> **GBA Pokémon** (the team from the linked GBA save), then fight **one Colosseum Battle** "
                 "with that team",
                 "the bonus disc", "the e-Reader (Card-e) features"]}


# Battle mode (section BM; snatcherLeo, 2026-10-19; playthrough-guide.md part 8 "BM1-BM5" and part 11 item 4):
# 3-5 full battles with teams Main picks, each with a short turn checklist; no story around them.
BM_BATTLES = ["BM1", "BM2", "BM3", "BM4", "BM5"]
BM_FIRST = [
    "team sources (**verified** 2026-10-19 in Dolphin, new-game-flow.md): Colosseum Battle -> Solo / Gang -> "
    "Register Pokémon, from the Story Mode save on card A or a linked GBA game, before any battle; Battle Now: "
    "single / double, difficulty, then a ready-made team (no save needed). For teams Main picks: register the edited "
    "story save's party",
    "Battle Mode opens from the title menu with no story progress (**verified**: Battle Now ran with no story save)",
    "the teams Main picks: ordinary, legal movesets (the story save's movesets are edited); written into a save copy "
    "(the project's save edits, never snatcherLeo's card)"]
BM_COVER = [
    "two-turn and multi-turn moves (Fly, Solar Beam, Outrage-type moves, Bide)",
    "weather and field effects over several turns (Rain Dance, Sandstorm, Reflect, Spikes)",
    "switching, and abilities that act on entry (Intimidate, Drizzle)",
    "fainting and the replacement",
    "doubles: target choice, moves that hit both sides, the partner's turn",
    "the opponent's own choices over a long battle",
    "status over several turns (sleep counters, poison and burn damage, confusion)"]


def battle_mode_checklist(root, out_dir, name):
    """BM's checklist: the battles in recording order, each ticked when a BM sitting with a battle result exists
    (one battle per sitting), and what the battles must cover between them. Returns the battles done."""
    sits = sorted(glob.glob(os.path.join(root, "sections", f"BM_{name}", "*", "events.jsonl")))
    done = []
    for p in sits:
        evs = [json.loads(l) for l in open(p, encoding="utf-8") if l.strip()]
        res = [e for e in evs if e.get("ev") == "battle_result"]
        bat = [e for e in evs if e.get("ev") == "battle"]
        if res:
            done.append((os.path.basename(os.path.dirname(p)), bat[0].get("id") if bat else "?", res[-1].get("value")))
    lines = [f"# BM {name.replace('_', ' ')}", "",
             "Battles with no story around them: a different result on the PC can only be battle logic. Recordable "
             "at any time. Ticks for the battles come from the BM sittings' logs (one battle per sitting); the rest "
             "is ticked by hand.", "", "## First (before BM1)", ""]
    lines += [f"- [ ] {x}" for x in BM_FIRST]
    lines += ["", "## Battles (team and turn checklist: Main writes each before it is recorded)", ""]
    for i, b in enumerate(BM_BATTLES):
        if i < len(done):
            sit, bid, res = done[i]
            lines.append(f"- [x] {b}: sitting {sit}, battle {bid}, result {res}")
        else:
            lines.append(f"- [ ] {b}: team: (to fill in) / turns: (to fill in)" +("  *(optional: 3-5 battles)*" if i >= 3 else ""))
    lines += ["", "## What the battles must cover between them (tick when a battle's turn checklist includes it)", ""]
    lines += [f"- [ ] {x}" for x in BM_COVER]
    lines += ["", "Single moves are covered by the automated move pass (every move once, Dolphin vs PC), "
              "not here; what it can't cover goes into these battles' turn checklists."]
    open(os.path.join(out_dir, f"BM_{name}.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
    return min(len(done), len(BM_BATTLES))


def rows(name):
    p = os.path.join(TABLES, name)
    if not os.path.exists(p):
        sys.exit(f"missing {p}: run the extraction first (tools/local/extract.py)")
    return list(csv.DictReader(open(p, encoding="utf-8", errors="replace")))


def story_steps():
    out, part, postgame = [], None, False
    for line in open(os.path.join(REPO, "docs", "engine", "story-route.md"), encoding="utf-8"):
        if line.startswith("## Part"):
            part = line[8:9]
        if line.startswith("### ") and "postgame" in line.lower():
            postgame = True
        m = re.match(r"\| (\d+) \|", line)
        if not m or part is None:
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        flags_cell = cells[1].split("(")[0]
        flags = [int(x) for x in re.findall(r"\b(\d{1,4})\b", flags_cell)]
        bcell = cells[4] if len(cells) > 4 else ""
        battles = [int(x) for x in re.findall(r"\*\*(\d+)\*\*\s*:", bcell)]
        # every battle number in the cell, ranges too ("battles 59-94", "Calda 53-55"); "(trainer 51-53)" left out
        named = []
        for lo, hi in re.findall(r"(?<![\w(])(\d{1,3})(?:\s*-\s*(\d{1,3}))?(?![\w)])", re.sub(r"\([^)]*\)", "", bcell)):
            lo, hi = int(lo), int(hi) if hi else int(lo)
            if 0 < lo <= hi < 1000 and hi - lo < 60:
                named += range(lo, hi + 1)
        trainers = [int(x) for x in re.findall(r"\((?:trainer|record)s? (\d+)", line)]
        rooms = sorted(set(re.findall(r"`([A-Z]\d_[A-Za-z0-9_]+)`", line)))
        what = re.sub(r"\s+", " ", cells[-1])[:220]
        out.append(dict(step=int(m.group(1)), part=part, flags=flags, battles=battles, all_battles=sorted(set(named)),
                        trainers=trainers,
                        rooms=rooms, what=what, optional="optional" in line.lower(), postgame=postgame))
    return out


def assign_sections(steps):
    """Section of each step: fixed ranges for parts 1-3, then parts 4-5 by the rooms they name."""
    state = {"4": "S10", "5": "S13"}
    for s in steps:
        rng = next((sec for sec, lo, hi in STEP_RANGES if lo <= s["step"] <= hi), None)
        if rng:
            s["section"] = rng
            continue
        if s["part"] == "4":
            if any("subway" in r.lower() or "liner" in r.lower() for r in s["rooms"]) and state["4"] == "S10":
                state["4"] = "S11"
            if any(r.startswith("D1_") for r in s["rooms"]):
                state["4"] = "S12"
            s["section"] = state["4"] if s["step"] > 83 else "S09"
        elif s["part"] == "5":
            if s["postgame"]:
                state["5"] = "S15"
            elif state["5"] == "S13" and s["rooms"] and not any("dome" in r for r in s["rooms"]) \
                    and any(st["section"] == "S13" and any("dome" in r for r in st["rooms"]) for st in steps
                            if "section" in st):
                state["5"] = "S14"
            s["section"] = state["5"]
        else:
            s["section"] = "S09"
    return steps


def flag_layout(disc_path):
    from colo.disc import Disc
    from colo.tables import common_rel, table_rows
    from colo import savefile
    rel = common_rel(Disc(disc_path))
    lay = savefile.flag_layout(table_rows(rel, 0x111e04, 0x14a6c0, 8))
    pos = {}
    for i, (c, w, p) in enumerate(lay):
        if p is not None:
            for k in range(w):
                pos[(c, p + k)] = i
    return pos


def progress(root, pos):
    """Everything the sittings reached: flags set (ids), battles, rooms entered (ids)."""
    flags, battles, rooms = set(), set(), set()
    for n, folder in all_sittings(root):
        p = os.path.join(folder, "events.jsonl")
        if not os.path.exists(p):
            continue
        for line in open(p):
            e = json.loads(line)
            if e["ev"] == "flag" and e["now"]:
                fid = pos.get((e["cls"], e["word"] * 32 + e["bit"]))
                if fid is not None:
                    flags.add(fid)
            elif e["ev"] == "battle":
                battles.add(e["id"])
            elif e["ev"] == "entered":
                rooms.add(e["room"])
        # flags already set before the sitting (a loaded save): the flag buffers at the first room entry
        for fe in sorted(glob.glob(os.path.join(folder, "flags_entry_*.json")))[:1] + \
                sorted(glob.glob(os.path.join(folder, "flags_entry_*.json")))[-1:]:
            words = json.load(open(fe))
            for c, buf in enumerate(words):
                for k, wv in enumerate(buf):
                    for b in range(32):
                        if wv >> b & 1:
                            fid = pos.get((c, k * 32 + b))
                            if fid is not None:
                                flags.add(fid)
    return flags, battles, rooms


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("disc")
    ap.add_argument("--root", default=DEFAULT_ROOT)
    a = ap.parse_args()
    steps = assign_sections(story_steps())
    room_rows = rows("rooms_decoded.csv")
    room_id = {r["archive"]: int(r["room_id"]) for r in room_rows if r["archive"]}
    # a room's section: the later of its area's section and the first step naming it (Phenac's square is named by
    # S01's arrival steps but belongs to S02; Pyrite's cave rooms are named first in S05, after Pyrite's S03)
    order = list(SECTIONS)
    named = {}
    for s in steps:
        for r in s["rooms"]:
            named.setdefault(r, s["section"])
    area_of = {r["archive"]: r["area"] for r in room_rows if r["archive"]}
    room_sec = {}
    for r in set(named) | set(area_of):
        cands = [x for x in (named.get(r), AREA_SECTION.get(area_of.get(r, ""))) if x]
        if cands:
            room_sec[r] = max(cands, key=order.index)
    # trainers per battle (battles_decoded side_trainers "id: name; ..."), so a battle named in a step places its
    # trainers, and through them their Shadow Pokémon, in that step's section
    battle_trainers = {int(b["battle"]): [int(x.split(":")[0]) for x in b["side_trainers"].split(";")
                                          if x.strip() and x.split(":")[0].strip().isdigit()]
                       for b in rows("battles_decoded.csv")}
    trainer_sec = {}
    for s in steps:
        for t in s["trainers"]:
            trainer_sec.setdefault(t, s["section"])
        for b in s["all_battles"]:
            for t in battle_trainers.get(b, []):
                if t > 1:                                  # 1 = the player's own record in every battle
                    trainer_sec.setdefault(t, s["section"])
    # trainers no story step names: placed by where they fight (a battle's arena naming a room, "(S1_out)") or where
    # they stand (a named person whose name is the trainer's last word), in that room's section
    by_name = {}
    for p in rows("npc_placements_decoded.csv"):
        if p["name"].strip():
            by_name.setdefault(p["name"].strip().upper(), p["room"])
    tr_where = {}
    for b in rows("battles_decoded.csv"):
        for x in b["side_trainers"].split(";"):
            k, _, nm = x.partition(":")
            if not k.strip().isdigit() or int(k) <= 1 or int(k) in trainer_sec:
                continue
            m = re.search(r"\(([A-Z]\d_[A-Za-z0-9_]+)\)", b["arena_name"])
            room = by_name.get(nm.strip().split(" ")[-1].upper()) if nm.strip() else None
            room = room or (m.group(1) if m else None)
            if room and room in room_sec:
                tr_where.setdefault(int(k), (room_sec[room], room))
    for t, (sec, room) in tr_where.items():
        trainer_sec[t] = sec
    for t, (sec, why) in TRAINER_OVERRIDE.items():
        trainer_sec[t] = sec
    pos = flag_layout(a.disc)
    flags, battles, rooms = progress(a.root, pos)
    treasure = rows("treasure_decoded.csv")
    people = rows("npc_placements_decoded.csv")
    shadows = rows("shadow_pokemon_decoded.csv")
    out_dir = os.path.join(a.root, "checklists")
    os.makedirs(out_dir, exist_ok=True)
    tick = lambda ok: "[x]" if ok else "[ ]"
    overview = ["# Section checklists (generated by section_checklist.py; not committed: built from the disc)", "",
                "| Section | Story steps | Battles | Items | Rooms | People (by hand) | Shadow Pokémon |", "|---|---|---|---|---|---|---|"]
    unplaced_shadows = []
    for sec, name in SECTIONS.items():
        if sec == "TEST":
            continue
        if sec == "BM":
            bm_done = battle_mode_checklist(a.root, out_dir, name)
            overview.append(f"| BM {name} | - | {bm_done}/{len(BM_BATTLES)} | - | - | - | - |")
            continue
        st = [s for s in steps if s["section"] == sec]
        lines = [f"# {sec} {name.replace('_', ' ')}", "",
                 "Generated from game data and the story-route page; ticks come from the sittings' logs.", ""]
        for x in EXTRA.get(sec, []):
            lines.append(f"- [ ] {x}")
        lines += ["", "## Story steps (story-route.md)", ""]
        done_steps = 0
        for s in st:
            ok = bool(s["flags"]) and all(f in flags for f in s["flags"])
            done_steps += ok
            opt = " *(optional)*" if s["optional"] else ""
            lines.append(f"- {tick(ok)} step {s['step']}{opt}: {s['what']}  (flags {', '.join(map(str, s['flags'])) or '-'})")
        bt = sorted({b for s in st for b in s["battles"]})
        lines += ["", "## Battles named in these steps", ""]
        lines += [f"- {tick(b in battles)} battle {b}" for b in bt] or ["- (none named)"]
        my_rooms = sorted(r for r, s2 in room_sec.items() if s2 == sec)
        lines += ["", "## Rooms (enter each at least once)", ""]
        lines += [f"- {tick(room_id.get(r) in rooms)} {r} (room {room_id.get(r, '?')})" for r in my_rooms] or ["- (none)"]
        items = [t for t in treasure if t["room"] in my_rooms]
        lines += ["", "## Items in the world (pick up each)", ""]
        got_items = 0
        for t in items:
            ok = int(t["flag"]) in flags
            got_items += ok
            lines.append(f"- {tick(ok)} {t['item']} x{t['quantity']} in {t['room']} (flag {t['flag']})")
        if not items:
            lines.append("- (none)")
        ppl = [p for p in people if p["room"] in my_rooms and p["name"].strip()]
        lines += ["", "## People to talk to (named; tick by hand)", ""]
        lines += [f"- [ ] {p['name']} in {p['room']}" for p in ppl] or ["- (no named people)"]
        shops = [r for r in my_rooms if "shop" in r.lower()]
        lines += ["", "## Shops (open each buy list once)", ""]
        lines += [f"- [ ] {r}" for r in shops] or ["- (none)"]
        sh_here = []
        for sh in shadows:
            ids = [int(x.split(":")[0]) for x in sh["trainers"].split(";") if x.strip() and x.split(":")[0].strip().isdigit()]
            secs = sorted((trainer_sec[i] for i in ids if i > 1 and i in trainer_sec), key=lambda x: (x == "S15", x))
            if secs and secs[0] == sec:
                sh_here.append(sh)
        lines += ["", "## Shadow Pokémon met here (snag each)", ""]
        for t, (sec2, why) in TRAINER_OVERRIDE.items():
            if sec2 == sec:
                lines.append(f"- note: {why}")
        lines += [f"- [ ] #{s['shadow_id']} {s['species']} (Lv {s['levels']}; trainers {s['trainers']})" for s in sh_here] \
            or ["- (none placed in this section by the story steps; see the README's unplaced list)"]
        open(os.path.join(out_dir, f"{sec}_{name}.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
        overview.append(f"| {sec} {name} | {done_steps}/{len(st)} | {sum(b in battles for b in bt)}/{len(bt)} | "
                        f"{got_items}/{len(items)} | {sum(room_id.get(r) in rooms for r in my_rooms)}/{len(my_rooms)} | "
                        f"{len(ppl)} | {len(sh_here)} |")
    placed = {s["shadow_id"] for s in shadows
              if any(int(x.split(':')[0]) > 1 and int(x.split(':')[0]) in trainer_sec for x in s["trainers"].split(";")
                     if x.strip() and x.split(':')[0].strip().isdigit())}
    unplaced_shadows = [f"#{s['shadow_id']} {s['species']}" for s in shadows
                        if s["shadow_id"] not in placed and s["species"].strip()]
    overview += ["", f"Shadow Pokémon not yet placed in a section (their trainers aren't named in the story steps): "
                 f"{len(unplaced_shadows)}: " + ", ".join(unplaced_shadows[:60])]
    open(os.path.join(out_dir, "README.md"), "w", encoding="utf-8").write("\n".join(overview) + "\n")
    print("\n".join(overview))


if __name__ == "__main__":
    main()
