"""Per-sitting lists for snatcherLeo's sitting PDFs (2026-10-20; docs/testing/playthrough-guide.md part 8).

    python -I tools/local/testing/sitting_lists.py [--root <decomp>/_playthrough]

Writes ROOT/sitting_lists/<code>.md, one per sitting code (P01-P25 with P14 / P15 / P16 and P17a / P17b apart, POST
for the postgame, EVO, SYS, S16), and ROOT/sitting_lists/README.md (counts and the coverage check). Like the section
checklists they are built from the disc's decoded tables, so they stay outside git.

Each list holds that sitting's share of: the story steps (story-route.md), every trainer battle (story, optional and
off-route ones, with the party and any Shadow Pokémon), the named people, the rooms, every item (treasure boxes,
sparkles on the ground, the Ein File books, and items people give through the room scripts), Pokémon gifts and the
shops with their buy lists. Every entry lands in exactly one sitting: the coverage check at the end lists anything the
rules below leave unplaced.

Where an entry goes (the tables below; "likely" unless the story route names it):
- story steps: STEP_SITTING (the guide's sitting table);
- rooms: ROOM_SITTING, else their area's AREA_SITTING. A room counts in the sitting where it is explored; rooms a
  story step passes through earlier are also named there ("passed through");
- people, items in the world, gifts and shops: their room's sitting; an item with an appear flag that a story step
  sets goes to that step's sitting if later; GIFT_SITTING / SHOP_SITTING override by hand;
- battles: BATTLE_SITTING by hand, then by kind (cups, the Mt. Battle challenge, Card e), then the first story step
  naming the battle, then the postgame groups, then where the trainer stands (a named person whose name is the
  trainer's last word), then the battle's arena room.
"""
import argparse, csv, glob, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.path.insert(0, HERE)                                   # python -I leaves the script's own folder out
from playrec import DEFAULT_ROOT                            # noqa: E402
from section_checklist import REPO, TABLES, rows, story_steps   # noqa: E402

SCRIPTS = os.path.join(os.path.dirname(TABLES), "scripts")

ORDER = ["P01", "P02", "P03", "P04", "P05", "P06", "P07", "P08", "P09", "P10", "P11", "P12", "P13", "P14", "P15",
         "P16", "P17a", "P17b", "P18", "P19", "P20", "P21", "P22", "P23", "P24", "P25", "POST", "EVO", "SYS", "S16"]
TITLE = {"P01": "New Game to Phenac", "P02": "Phenac City", "P03": "Miror B.'s peons to Pyrite",
         "P04": "the Phenac Stadium cup", "P05": "Pyrite part 1", "P06": "Pyrite part 1 continued",
         "P07": "the Pyrite Colosseum cup", "P08": "the building", "P09": "Pyrite Cave and Miror B.",
         "P10": "after Miror B.", "P11": "Agate Village", "P12": "Mt. Battle: Dakim", "P13": "the Time Flute",
         "P14": "the Mt. Battle challenge, areas 1-3", "P15": "the Mt. Battle challenge, areas 4-7",
         "P16": "the Mt. Battle challenge, areas 8-10", "P17a": "Agate afterwards", "P17b": "to The Under",
         "P18": "The Under", "P19": "Venus", "P20": "the subway and the Shadow Liner",
         "P21": "the Shadow Pokémon Lab, part 1", "P22": "the Shadow Pokémon Lab, part 2",
         "P23": "Realgam Tower's domes", "P24": "the tower", "P25": "the end", "POST": "the postgame (P26 onward)",
         "EVO": "the evolution sitting", "SYS": "the systems sitting", "S16": "GBA link, the bonus disc, the e-Reader"}

# story steps -> sitting (the guide's sitting table, part 8)
STEP_SITTING = [(1, 9, "P01"), (10, 15, "P02"), (16, 16, "P04"), (17, 17, "P02"), (18, 23, "P03"), (24, 32, "P05"),
                (33, 40, "P06"), (41, 41, "P07"), (42, 44, "P08"), (45, 47, "P09"), (48, 54, "P10"),
                (55, 64, "P11"), (65, 70, "P12"), (71, 76, "P13"), (77, 77, "P14"), (78, 78, "P17a"),
                (79, 83, "P17b"), (84, 94, "P18"), (95, 95, "P19"), (96, 99, "P20"), (100, 104, "P21"),
                (105, 115, "P22"), (116, 123, "P23"), (124, 130, "P24"), (131, 133, "P25")]
AREA_SITTING = {"S1": "P01", "M1": "P02", "M2": "P05", "M3": "P11", "D2": "P12", "M4": "P18", "D1": "P21",
                "D4": "P23", "S2": "POST", "T1": "POST"}
ROOM_SITTING = {
    "M1_water_colo": "P04", "M1_gym_1F": "P10", "M1_gym_1F_bf": "P10", "M1_gym_B1": "P10", "M1_carde_1": "S16",
    "M2_enter_1F": "P06", "M2_enter_1F_2": "P06", "M2_windmill_1F": "P06", "M2_windmill_2F": "P06",
    "M2_earth_colo": "P07", "M2_building_1F": "P08", "M2_building_1F_bf": "P08", "M2_building_2F": "P08",
    "M2_building_3F": "P08", "M2_building_4F": "P08", "M2_uranai_1F": "P10",
    "M2_cave_1F_1": "P09", "M2_cave_1F_2": "P09", "M2_cave_2F_1": "P09", "M2_cave_2F_2": "P09", "M2_cave_3F_1": "P09",
    "M2_cave_3F_1_bf": "P09", "M2_cave_3F_2": "P09",
    "D2_present_room": "POST",
    "D2_valley_1": "P14", "D2_valley_1_bf": "P14", "D2_valley_2": "P14", "D2_valley_3": "P14", "D2_rest_1": "P14",
    "D2_rest_2": "P14", "D2_rest_3": "P14",
    "D2_magma_1": "P15", "D2_magma_1_bf": "P15", "D2_magma_2": "P15", "D2_magma_3": "P15", "D2_rest_4": "P15",
    "D2_rest_5": "P15", "D2_rest_6": "P15", "D2_cloud_1": "P15", "D2_rest_7": "P15",
    "D2_cloud_1_bf": "P16", "D2_cloud_2": "P16", "D2_cloud_3": "P16", "D2_cloud_4": "P16", "D2_rest_8": "P16",
    "D2_rest_9": "P16", "D2_crater_colo": "P16", "D2_crater_colo_bf": "P16",
    "M4_enter_1F": "P17b", "M4_subway_B1": "P20", "M4_subway_B2_1": "P20", "M4_subway_B2_1_bf": "P20",
    "M4_subway_B2_2": "P20", "M4_subway_B3": "P20", "M4_subway_B3_ev_1": "P20", "M4_subway_B3_ev_2": "P20",
    "M4_train_1F": "P20", "M4_cylinder_colo": "P21", "M4_bottom_colo": "POST", "M4_enter_B1": "POST",
    "D1_labo_1F": "P22", "D1_labo_B1": "P22", "D1_labo_B1_bf": "P22", "D1_labo_B2": "P22", "D1_labo_B3": "P22",
    "D4_tower_1F_2": "P24", "D4_tower_1F_2_bf": "P24", "D4_tower_1F_3": "P24", "D4_tower_1F_3_bf": "P24",
    "D4_tower_2F": "P24", "D4_tower_2F_bf": "P24", "D4_tower_2F_2": "P24", "D4_tower_2F_2_bf": "P24",
    "D4_casino_colo": "P24", "D4_casino_colo_bf": "P24"}
# (room, item id or "var") -> sitting, for items people give whose room is explored earlier or later
GIFT_SITTING = {("S1_shop_1F", 4): "P02", ("M1_stadium_1F", "var"): "P04", ("M2_enter_1F", 294): "P07",
                ("M2_enter_1F", "var"): "P07", ("M2_police_1F", 501): "P17b", ("M3_houseC_1F", 1): "P21",
                ("M3_out", 542): "P17a", ("M4_enter_1F", "var"): "P21", ("M4_out", 505): "P21",
                ("D2_pc_1F", 537): "P13", ("M2_guild_1F_1", "Plusle"): "P10"}
GIFT_NOTE = {("M1_out", 329): "who gives it and when: not traced", ("S1_shop_1F", 4): "story step 15",
             ("M1_stadium_1F", "var"): "the cup's prize (chosen by a script variable)",
             ("M2_enter_1F", "var"): "a cup prize (chosen by a script variable)",
             ("M4_enter_1F", "var"): "The Under Colosseum cup's prize (chosen by a script variable)",
             ("M1_gym_1F", 180): "the Pre Gym (when: not traced)", ("M1_gym_1F", 315): "the Pre Gym (when: not traced)"}
TREASURE_NOTE = {153: "verified from the police station's room script: the first spot shows while the windmill is "
                      "broken, from the gear going missing until the building (P08), and the second only after Miror "
                      "B. is beaten; pick it up in P06, the second spot is only a later chance",
                 2541: "the random DNA layout decides which spot is used", 2542: "the random DNA layout decides which "
                 "spot is used", 2543: "the random DNA layout decides which spot is used"}
# a story step whose battles come from script globals (the story route can't name them): its arena places them
STEP_ARENA = {"D4_casino_colo_bf": (129, "Nascour's four-trainer show battle in the Realgam Tower Colosseum (the "
                                         "step's battle ids come from script globals; placed by its arena)")}
SHOP_SITTING = {("D2_pc_1F", 13): "P16", ("S1_shop_1F", 1): "P02"}
SHOP_NOTE = {("S1_shop_1F", 1): "opened right after the clerk finds the Poké Balls (story step 15)",
             ("S1_shop_1F", "var"): "the clerk's everyday shop; the script works out its number (not traced). The "
                                    "disc's lists 0, 1 and 2 differ in the balls on sale (0: none, 1: Poké and Great "
                                    "Balls, 2: more kinds), so it likely grows with the story",
             ("D2_pc_1F", 13): "the Poké Coupon exchange (prices in coupons)"}
# battle -> (sitting, note) by hand: the guide's choices where the data alone would place them elsewhere
BATTLE_SITTING = {13: ("P10", "Willie's rematch (the guide's P10 side content)"),
                  530: ("POST", "Willie's later rematch (postgame)"),
                  531: ("P03", "story step 18's retry: only if Makuhita wasn't snagged"),
                  532: ("POST", "the Mystery Troop rematch in Phenac (postgame)"),
                  533: ("POST", "the Mystery Troop rematch in Phenac (postgame)"),
                  534: ("POST", "the Mystery Troop rematch in Phenac (postgame)")}
# the deliberate miss (guide P23): the Troop member met in P03 stands in Realgam Tower with his starter; optional, talk
# to him; only the member met in Phenac appears (verified, story route), with the starter or without it once snagged
for _n in (28, 29, 31, 32, 34, 35):
    BATTLE_SITTING[_n] = ("P23", "the deliberate miss: talk to him on purpose (optional); only the Troop member you "
                                 "met in P03 is there")
# a trainer record whose party pointer is Willie's first party (238), other than Willie's own (trainer 10): 34 records
# share it, so their party in the table is a placeholder (verified: the same pointer); whether they are unused or get
# their real party some other way is not checked
PLACEHOLDER_PARTY, WILLIE = "238", 10


def step_sitting(n):
    return next((s for lo, hi, s in STEP_SITTING if lo <= n <= hi), None)


def base_room(r):
    return r[:-3] if r.endswith("_bf") else r


def room_sitting(r):
    return ROOM_SITTING.get(r) or ROOM_SITTING.get(base_room(r)) or AREA_SITTING.get(r[:2])


def later(a, b):
    return max(a, b, key=ORDER.index)


def postgame_groups():
    """story-route.md's postgame groups (rows "| P<n> |"): [(group, what)]."""
    out = []
    for line in open(os.path.join(REPO, "docs", "engine", "story-route.md"), encoding="utf-8"):
        m = re.match(r"\| (P\d+) \|", line)
        if m:
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            out.append((m.group(1), re.sub(r"\s+", " ", cells[-1])[:220]))
    return out


def postgame_battles():
    """Battle numbers named in story-route.md's postgame groups (rows "| P<n> |")."""
    out = set()
    for line in open(os.path.join(REPO, "docs", "engine", "story-route.md"), encoding="utf-8"):
        if re.match(r"\| P\d+ \|", line):
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            cell = re.sub(r"\([^)]*\)", "", cells[4]) if len(cells) > 4 else ""
            for lo, hi in re.findall(r"(?<![\w(])(\d{1,3})(?:\s*-\s*(\d{1,3}))?(?![\w)])", cell):
                lo, hi = int(lo), int(hi) if hi else int(lo)
                if 0 < lo <= hi < 1000 and hi - lo < 60:
                    out.update(range(lo, hi + 1))
    return out


def script_calls():
    """Items and Pokémon people give, and the shops, from the decoded room scripts: [(kind, room, value, count)]."""
    out = []
    for p in sorted(glob.glob(os.path.join(SCRIPTS, "*", "*__*.txt"))):
        room = os.path.basename(p).split("__")[0]
        seen = set()
        for line in open(p, encoding="utf-8", errors="replace"):
            for m in re.finditer(r"scriptGetItem\((\w+), (\d+)\)", line):
                key = ("gift", m.group(1))
                if key not in seen:
                    seen.add(key)
                    v = int(m.group(1)) if m.group(1).isdigit() else "var"
                    out.append(("gift", room, v, int(m.group(2))))
            for m in re.finditer(r"playerReceiveGiftPokemon\((\d+)\)", line):
                out.append(("pokemon", room, "Plusle" if m.group(1) == "3" else f"gift {m.group(1)}", 1))
            for m in re.finditer(r"menuShopOpen\((\w+)\)", line):
                key = ("shop", m.group(1))
                if key not in seen:
                    seen.add(key)
                    out.append(("shop", room, int(m.group(1)) if m.group(1).isdigit() else "var", 0))
    return out


def grouped(battles):
    """One tick box per trainer (or trainer pair), its battle records underneath, in battle order."""
    groups = {}
    for num, key, line in sorted(battles):
        groups.setdefault(key, []).append(line)
    out = []
    for key, recs in groups.items():
        if len(recs) == 1:
            out.append(f"- [ ] {key}: {recs[0]}")
        else:
            out.append(f"- [ ] {key}: {len(recs)} battle records (you meet one or more of them):")
            out += [f"    - {r}" for r in recs]
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=DEFAULT_ROOT)
    a = ap.parse_args()
    out_dir = os.path.join(a.root, "sitting_lists")
    os.makedirs(out_dir, exist_ok=True)
    L = {k: {"steps": [], "battles": [], "placeholder": [], "people": [], "rooms": [], "passed": [], "items": [], "gifts": [],
             "shops": [], "notes": []} for k in ORDER}
    unplaced = []

    # story steps
    steps = story_steps()
    flag_step = {}
    for s in steps:
        s["sitting"] = step_sitting(s["step"]) if not s["postgame"] else "POST"
        if s["sitting"] is None:
            unplaced.append(f"story step {s['step']}")
            continue
        L[s["sitting"]]["steps"].append(s)
        for f in s["flags"]:
            flag_step.setdefault(f, s["sitting"])

    # rooms
    room_rows = [r for r in rows("rooms_decoded.csv") if r["archive"] and r["area"].strip()]
    room_of = {}
    for r in room_rows:
        sit = room_sitting(r["archive"])
        if sit is None:
            unplaced.append(f"room {r['archive']}")
            continue
        room_of[r["archive"]] = sit
        L[sit]["rooms"].append(f"{r['archive']} (room {r['room_id']}, {r['area']})")
    for s in steps:
        for r in s["rooms"]:
            if r in room_of and s.get("sitting") and ORDER.index(s["sitting"]) < ORDER.index(room_of[r]):
                note = f"{r} (explored in {room_of[r]}; story step {s['step']})"
                if note.split(" (")[0] not in [x.split(" (")[0] for x in L[s["sitting"]]["passed"]]:
                    L[s["sitting"]]["passed"].append(note)

    # people (named)
    seen = set()
    for p in rows("npc_placements_decoded.csv"):
        nm = p["name"].strip()
        if not nm or (p["room"], nm) in seen:
            continue
        seen.add((p["room"], nm))
        sit = room_of.get(p["room"])
        if sit is None:
            unplaced.append(f"person {nm} in {p['room']}")
            continue
        L[sit]["people"].append(f"{nm} in {p['room']}")

    # items in the world (treasure: look 1 box, 2 sparkle on the ground, 3 book)
    raw = {r["index"]: r for r in csv.DictReader(open(os.path.join(TABLES, "treasure.csv"), encoding="utf-8"))}
    looks = {1: "treasure box", 2: "on the ground (sparkle)", 3: "book (Ein File)"}
    tre = []
    for t in rows("treasure_decoded.csv"):
        sit = room_of.get(t["room"])
        if sit is None:
            unplaced.append(f"item {t['item']} in {t['room']}")
            continue
        appear = int(raw[t["index"]]["w08"], 16) >> 16 if t["index"] in raw else 0   # +0x08, a halfword
        why = ""
        if appear and appear in flag_step:
            if ORDER.index(flag_step[appear]) > ORDER.index(sit):
                sit, why = flag_step[appear], f"; appears once story flag {appear} is set"
            else:
                why = f"; appears once story flag {appear} is set"
        elif appear:
            why = f"; appears once flag {appear} is set (when: not traced)"
        look = looks.get(int(t["container_kind"]) >> 5, "?")
        qty = f" x{t['quantity']}" if t["quantity"] not in ("", "1") else ""
        item = t["item"].strip() or "a DNA sample (random: see the notes)"
        tre.append(dict(sit=sit, flag=int(t["flag"]), item=f"{item}{qty}", where=f"{look} in {t['room']}", why=why))
    # records sharing a picked-up flag are one item with several possible spots: one line, in the earliest sitting
    by_flag = {}
    for e in tre:
        by_flag.setdefault(e["flag"], []).append(e)
    item_records = 0
    for flag, es in by_flag.items():
        item_records += len(es)
        if len(es) == 1:
            e = es[0]
            L[e["sit"]]["items"].append(f"{e['item']}: {e['where']} (picked-up flag {flag}{e['why']})")
            continue
        first = min(es, key=lambda e: ORDER.index(e["sit"]))
        spots = "; ".join(f"{e['where']}{e['why'].replace('; ', ', ')}" for e in es)
        note = TREASURE_NOTE.get(flag, "picking it up in one spot removes the others")
        L[first["sit"]]["items"].append(f"{first['item']}: one item, {len(es)} possible spots sharing picked-up flag "
                                        f"{flag} ({note}): {spots}")

    # items people give, Pokémon gifts, shops (room scripts)
    names = {int(r["item_id"]): r["name"] for r in rows("items_decoded.csv")}
    shops = {int(r["shop"]): r for r in rows("shops_decoded.csv")}
    for kind, room, v, n in script_calls():
        if kind in ("gift", "pokemon"):
            sit = GIFT_SITTING.get((room, v)) or room_of.get(room)
            if sit is None:
                unplaced.append(f"gift {v} in {room}")
                continue
            what = names.get(v, "?") if isinstance(v, int) else ("an item chosen by the script" if v == "var" else v)
            note = GIFT_NOTE.get((room, v), "")
            label = "Pokémon gift" if kind == "pokemon" else "given by a person or event"
            qty = f" x{n}" if n > 1 else ""
            L[sit]["gifts"].append(f"{what}{qty}: {label} in {room}" + (f" ({note})" if note else ""))
        else:
            sit = SHOP_SITTING.get((room, v)) or room_of.get(room)
            if sit is None:
                unplaced.append(f"shop {v} in {room}")
                continue
            note = SHOP_NOTE.get((room, v), "")
            if v == "var":
                L[sit]["shops"].append(f"{room}: {note or 'a shop chosen by the script'}")
            else:
                s = shops.get(v, {})
                L[sit]["shops"].append(f"{room}: shop {v}" + (f" ({note})" if note else "")
                                       + f": {s.get('items (price; coupons at the exchanges)', '?')}")

    # battles and trainers
    trainers = {int(r["trainer"]): r for r in rows("trainers_decoded.csv")}
    shadow_of = {}
    for sh in rows("shadow_pokemon_decoded.csv"):
        for x in sh["trainers"].split(";"):
            k = x.split(":")[0].strip()
            if k.isdigit() and sh["species"].strip():
                shadow_of.setdefault(int(k), []).append(sh["species"].strip())
    step_of_battle = {}
    for s in steps:
        if not s.get("sitting"):
            continue
        for b in sorted(set(s["battles"]) | set(s["all_battles"])):
            step_of_battle.setdefault(b, s)
    post = postgame_battles()
    arena_room = {int(r["arena"]): r["room"] for r in rows("arenas_decoded.csv")}
    npc_rooms = {}
    for p in rows("npc_placements_decoded.csv"):
        if p["name"].strip():
            npc_rooms.setdefault(p["name"].strip().upper(), []).append(p["room"])
    battles = rows("battles_decoded.csv")
    mtb = sorted(int(b["battle"]) for b in battles if b["type"] == "8")
    side = {int(b["battle"]): [int(x.split(":")[0]) for x in b["side_trainers"].split(";")
                               if x.split(":")[0].strip().isdigit() and int(x.split(":")[0]) > 1] for b in battles}

    def top_level(num):
        lv = [int(m) for t in side.get(num, []) for m in re.findall(r" L(\d+)", trainers.get(t, {}).get("party", ""))]
        return max(lv, default=0)

    # a trainer's other records (same name, a battle no step names) go with the story battle that names the trainer,
    # or to the postgame when they are 10+ levels higher
    story_name = {}
    for b in sorted(battles, key=lambda b: int(b["battle"])):
        if int(b["battle"]) in step_of_battle:
            for x in b["side_trainers"].split(";"):
                k, _, nm = x.partition(":")
                if k.strip().isdigit() and int(k) > 1 and nm.strip():
                    story_name.setdefault(nm.strip(), (step_of_battle[int(b["battle"])]["sitting"], int(b["battle"])))
    test_count = 0
    for b in battles:
        num, kind = int(b["battle"]), int(b["type"] or 0)
        ids = []
        for x in b["side_trainers"].split(";"):
            k, _, nm = x.partition(":")
            if k.strip().isdigit() and int(k) > 1:
                ids.append((int(k), nm.strip()))
        if not ids:
            continue
        if kind in (4, 12):
            test_count += 1
            continue
        aroom = arena_room.get(int(b["arena"] or 0), "")
        sit, how = None, ""
        if num in BATTLE_SITTING:
            sit, how = BATTLE_SITTING[num]
        elif kind in (6, 7):
            sit, how = "P04", "the Phenac Stadium cup" + (" (a final-round record)" if kind == 7 else "")
        elif kind in (3, 5):
            cup = {"M2_earth_colo": ("P07", "the Pyrite Colosseum cup"),
                   "M4_cylinder_colo": ("P21", "The Under Colosseum cup"),
                   "M4_bottom_colo": ("POST", "the Orre Colosseum (postgame)")}.get(aroom)
            if cup:
                sit, how = cup[0], cup[1] + (" (a cup boss record)" if kind == 5 else "")
        elif kind == 8:
            area = mtb.index(num) // 10 + 1
            sit = "P14" if area <= 3 else "P15" if area <= 7 else "P16"
            how = (f"the Mt. Battle challenge, area {area} (10 records per area, in order; their arenas match: "
                   f"grass 1-3, magma 4-6, cloud 7-10)")
        elif kind == 9:
            sit, how = "P16", "the Mt. Battle challenge's last battle"
        elif kind in (10, 11, 17):
            sit, how = "S16", "Card e (the e-Reader battles)"
        if sit is None and num in step_of_battle:
            s = step_of_battle[num]
            sit = s["sitting"]
            how = f"story step {s['step']}" + (" (optional)" if s["optional"] else " (story)")
        if sit is None and aroom in STEP_ARENA:
            n, what = STEP_ARENA[aroom]
            s = next(st for st in steps if st["step"] == n)
            sit, how = s["sitting"], f"story step {n} (story): {what}"
        if sit is None and num in post:
            sit, how = "POST", "a postgame group (story route)"
        if sit is None:
            for tid, nm in ids:
                if nm in story_name:
                    sit, bn = story_name[nm]
                    if top_level(num) >= top_level(bn) + 10:
                        sit = "POST"
                        how = (f"a later record of the trainer in story battle {bn}, about {top_level(num)} levels "
                               f"high: likely a postgame rematch or second chance (not checked)")
                    else:
                        how = (f"another record of the trainer in story battle {bn}: likely a variant the game picks "
                               f"by story stage, or the battle once its Shadow Pokémon is snagged (not checked)")
                    break
        if sit is None:
            for tid, nm in ids:
                cand = npc_rooms.get(nm.split(" ")[-1].upper(), []) if nm else []
                same = [r for r in cand if r[:2] == aroom[:2]] or cand
                for r in same:
                    if r in room_of:
                        sit, how = room_of[r], f"not on the story route: optional or skippable (likely); stands in {r}"
                        break
                if sit:
                    break
        if sit is None and base_room(aroom) in room_of and not aroom.startswith("M2_earth"):
            sit = room_of[base_room(aroom)]
            how = f"not on the story route: optional or skippable (likely); fought in {aroom}"
        who, key = [], " + ".join(nm or "?" for _, nm in ids)
        for tid, nm in ids:
            t = trainers.get(tid, {})
            party = t.get("party", "")
            sh = shadow_of.get(tid, [])
            who.append((f"{nm or '?'}, " if len(ids) > 1 else "") + f"trainer {tid}" + (f": {party}" if party else "")
                       + (f" **Shadow: {', '.join(sh)}**" if sh else ""))
        line = f"battle {num} (" + "; ".join(who) + ")" + (f" [{how}]" if how else "")
        if sit is None:
            unplaced.append(f"{key}: {line}")
            continue
        placeholder = all(trainers.get(tid, {}).get("first_trainer_pokemon") == PLACEHOLDER_PARTY and tid != WILLIE
                          for tid, _ in ids)
        L[sit]["placeholder" if placeholder else "battles"].append((num, key, line))

    L["S16"]["notes"] += ["GBA link (Dolphin's built-in mGBA: check)", "the bonus disc",
                          "the e-Reader (Card e) features: not reachable in the US game (battle-engine.md)"]
    L["EVO"]["notes"] += ["after Ho-Oh; start with one Pokémon in the party; save edit E-EVO adds two Wurmple, Nincada "
                          "and Feebas (playthrough-guide.md part 7, 'Evolution: every method')",
                          "a Rare Candy on each Wurmple: Silcoon and Cascoon",
                          "a Rare Candy on Nincada (free party slot, a Poké Ball in the bag): Ninjask and Shedinja; note "
                          "whether a Poké Ball was used up",
                          "a Rare Candy on Feebas (Beauty 170): Milotic",
                          "no trainers, people, rooms or items of its own"]
    L["SYS"]["notes"] += ["the Options menu: every setting changed once and back (text speed, sound, rumble)",
                          "the title screen's own menus, and its attract video (wait at the title)",
                          "no trainers, people, rooms or items of its own: the systems sitting only uses menus"]
    L["P22"]["notes"] += ["the DNA samples: when the Maingate Key opens the Lab, the game draws three of nine samples "
                          "at random and fills one of four layouts of three spots; each of the three DNA lines below "
                          "lists its four possible spots, and the drawn layout decides which one holds it (treasure.md)"]
    L["POST"]["notes"] += [f"postgame group {g} (story route): {what}" for g, what in postgame_groups()]
    L["POST"]["notes"] += ["**read every new mail**: mails 7 and 9 are required for the postgame story",
                           "last: Ho-Oh, from Battle Mode's Mt. Battle (100 trainers, continues allowed: the gift "
                           "comes with the 100th win, guide part 12 decision 4, corrected 2026-10-20) on the story "
                           "card; before starting, check the registered team: six Pokémon, no two holding the same "
                           "item"]

    # write
    index = ["# Per-sitting lists (generated by sitting_lists.py; not committed: built from the disc)", "",
             "| Sitting | Steps | Battles | Placeholder battles | People | Rooms | Items | Gifts | Shops |",
             "|---|---|---|---|---|---|---|---|---|"]
    for k in ORDER:
        d = L[k]
        lines = [f"# {k}: {TITLE[k]}", "",
                 "Everything the game data places in this stretch: tick each as you go. Generated from the decoded "
                 "tables and the story route; where an entry goes is **likely** unless the story route names it.", ""]
        if d["notes"]:
            lines += ["## Notes", ""] + [f"- [ ] {x}" for x in d["notes"]] + [""]
        lines += ["## Story steps", ""]
        lines += [f"- [ ] step {s['step']}{' *(optional)*' if s['optional'] else ''}: {s['what']}" for s in d["steps"]] \
            or ["- (none)"]
        lines += ["", "## Trainer battles (story, optional and off-route; party; Shadow Pokémon in bold)", ""]
        lines += grouped(d["battles"]) or ["- (none)"]
        if d["placeholder"]:
            lines += ["", "Battle records here whose party in the data is a copy of Willie's first party (two Lv 24 "
                      "Zigzagoon): likely placeholders, unused or given their real party some other way (not "
                      "checked). Fight any you meet and note the real party:", ""]
            lines += [x.replace(": ZIGZAGOON L24; ZIGZAGOON L24", "") for x in grouped(d["placeholder"])]
        lines += ["", "## Named people (talk to each once)", ""]
        lines += [f"- [ ] {x}" for x in d["people"]] or ["- (none)"]
        lines += ["", "## Rooms (enter each once)", ""]
        lines += [f"- [ ] {x}" for x in d["rooms"]] or ["- (none)"]
        if d["passed"]:
            lines += ["", "Also passed through here (their full lists are in the sitting named):", ""]
            lines += [f"- {x}" for x in d["passed"]]
        lines += ["", "## Items in the world (boxes, on the ground, books)", ""]
        lines += [f"- [ ] {x}" for x in d["items"]] or ["- (none)"]
        lines += ["", "## Items and Pokémon people give", ""]
        lines += [f"- [ ] {x}" for x in d["gifts"]] or ["- (none)"]
        lines += ["", "## Shops (open each buy list once)", ""]
        lines += [f"- [ ] {x}" for x in d["shops"]] or ["- (none)"]
        open(os.path.join(out_dir, f"{k}.md"), "w", encoding="utf-8").write("\n".join(lines) + "\n")
        index.append(f"| {k} | {len(d['steps'])} | {len(d['battles'])} | {len(d['placeholder'])} | {len(d['people'])} | {len(d['rooms'])} | "
                     f"{len(d['items'])} | {len(d['gifts'])} | {len(d['shops'])} |")
    tot = lambda key: sum(len(L[k][key]) for k in ORDER)
    index += ["", f"Totals: {tot('steps')} story steps, {tot('battles')} battles (+ {tot('placeholder')} with a "
                  f"placeholder party), {tot('people')} named people, "
                  f"{tot('rooms')} rooms, {tot('items')} items in the world, {tot('gifts')} gifts, {tot('shops')} "
                  f"shop entries. Left out on purpose: {test_count} developer test battles.",
              "", f"Unplaced (needs a rule): {len(unplaced)}"] + [f"- {x}" for x in unplaced]
    # coverage: every source entry is in exactly one list
    src = {"rooms": len(room_rows), "people": len(seen), "items": len(rows("treasure_decoded.csv")),
           "gifts": sum(1 for c in script_calls() if c[0] != "shop"), "shops": sum(1 for c in script_calls()
                                                                              if c[0] == "shop"),
           "battles": sum(1 for b in battles if side.get(int(b["battle"]))) - test_count}
    got = {"rooms": tot("rooms"), "people": tot("people"), "items": item_records, "gifts": tot("gifts"),
           "shops": tot("shops"), "battles": tot("battles") + tot("placeholder")}
    index += ["", "Coverage (each source entry in exactly one list): " + ", ".join(
        f"{k} {got[k]}/{src[k]}{'' if got[k] == src[k] else ' MISMATCH'}" for k in src)]
    open(os.path.join(out_dir, "README.md"), "w", encoding="utf-8").write("\n".join(index) + "\n")
    print("\n".join(index))


if __name__ == "__main__":
    main()
