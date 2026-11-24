"""Playability inventory (testing lane): one row per playable thing, the definition of done for roadmap step 2
(docs/testing-plan.md section 1a).

    python -I tools/local/playability.py [--extracted DIR]

Reads the decoded tables and script listings in extracted/ (git-ignored, regenerated from the disc by
tools/local/extract.py) and docs/testing/route.json, and writes:
- docs/testing/playability.csv: one row per playable thing: ids and our own descriptions only (no game text);
- docs/testing/playability.md: the summary page (counts per category and status, per group);
- extracted/testing/playability_text.csv: the local view with the game's own names (trainers, NPCs, items, shops);
  never committed.
snatcherLeo's own rows live in docs/testing/playability-extra.csv (created empty with the header if missing, never
overwritten); they are merged in. The status, verified-by, last-build and date columns of an existing
playability.csv are kept by id, so regenerating never loses test results.

Categories: game modes, rooms, menu screens, room events (hero-event table: exits, doors, signs, PCs, TV),
talk events (NPCs with a talk script), script events (other room-script functions that start a battle, warp, open a
menu or shop, give something, send mail, or move the camera), battles, shops, gifts, world items, side features.
The facts come from the decoded tables (events_decoded, npc_placements_decoded, battles_decoded, ...) and the script
listings (calls found in each function, followed one level into the functions it calls). Labels: rows from tables
are verified data; what a script function "does" is read from its calls (likely: a call inside a branch may not run).
"""
import csv, json, os, re, sys
from collections import Counter, defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
TESTING = os.path.join(REPO, "docs", "testing")
OUT_CSV = os.path.join(TESTING, "playability.csv")
OUT_MD = os.path.join(TESTING, "playability.md")
EXTRA_CSV = os.path.join(TESTING, "playability-extra.csv")
MANUAL_CSV = os.path.join(TESTING, "playability-manual.csv")  # the hand cross-check, kept by hand
RUNTIME_CSV = os.path.join(TESTING, "runtime-ids.csv")  # ids seen in real runs (testing/idlog_apply.py)
UNUSED_CSV = os.path.join(TESTING, "unused-ids.csv")  # ids resolved as unused from the data, with evidence (by hand)
ROUTE = os.path.join(TESTING, "route.json")

COLUMNS = ["id", "category", "group", "description", "source", "reach", "system", "status", "verified_by",
           "last_build", "date"]
KEPT = ["status", "verified_by", "last_build", "date"]
# "later": real content planned for after step 2 (region-only, bonus disc); doesn't block step 2 (testing-plan.md 1a)
UNTRACED = "untraced"  # fixed reach marker: the data names no way in (see build())
STATUSES = ["not tested", "works", "matches Dolphin", "waived", "later"]
CATEGORIES = ["game mode", "room", "room (pushed)", "menu screen", "room event", "talk event", "script event", "battle",
              "shop", "gift", "world item", "side feature", "rule", "console behaviour", "region only",
              "debug / test only"]


def find_extracted():
    if "--extracted" in sys.argv:
        return sys.argv[sys.argv.index("--extracted") + 1]
    for p in (os.path.join(REPO, "extracted"), os.path.join(REPO, "..", "pkmn_colosseum", "extracted")):
        if os.path.isdir(os.path.join(p, "data_tables")):
            return os.path.normpath(p)
    sys.exit("extracted/ not found (pass --extracted DIR; regenerate with tools/local/extract.py)")


EX = find_extracted()


def table(name):
    with open(os.path.join(EX, "data_tables", name), encoding="utf-8") as f:
        return list(csv.DictReader(f))


# --- coverage-checklist systems (docs/engine/index.md, first column, short form) ---
SYS = dict(battle="Battles / encounters", arena="Battle arenas", trainer_where="Trainers: where and when you meet them",
           room="Maps / floors", npc="NPCs", script="Scripts", events="Events", cutscene="Cutscenes", shop="Shops",
           world_item="Items placed in the world", colo="Colosseum challenges", trade="Trading and Pokémon exchange",
           save="Save file", gba="GBA link transfer format", movie="Movies (THP)", menus="Menus",
           shadow="Shadow Pokémon", ereader="e-Reader / bonus content", pc="PC box, bag, mail", items="Items",
           flags="Story flags and variables", text="Text")

# --- battle kinds (battle-engine.md) -> (group, our description) ---
BATTLE_KIND = {1: ("story", "story boss battle on the map"), 2: ("story", "story trainer battle"),
               3: ("story Colosseum", "story Colosseum round"), 5: ("story Colosseum", "story Colosseum final round"),
               4: ("test", "test battle (debug)"), 6: ("Colosseum", "Phenac Colosseum challenge round"),
               7: ("Colosseum", "Phenac Colosseum challenge final round"), 8: ("Mt. Battle", "Mt. Battle round (1-99)"),
               9: ("Mt. Battle", "Mt. Battle 100th battle"), 10: ("Card e", "Card e matrix battle"),
               17: ("Card e", "Card e special battle"), 11: ("Card e", "Card e consecutive battle"),
               12: ("Battle Mode", "Battle Mode quick battle"), 13: ("Battle Mode", "Battle Mode single Colosseum"),
               14: ("Battle Mode", "Battle Mode single Colosseum (final)"), 15: ("Battle Mode", "Battle Mode 100 wins"),
               18: ("Battle Mode", "Battle Mode 100 wins (final)"), 16: ("Battle Mode", "link battle")}

# --- the common script's functions (common-script.md) ---
COMMON_FN = {3: "minimal main loop", 4: "exit", 5: "door", 6: "animated warp (lift)", 7: "animated warp arrival",
             8: "fade in", 9: "fade out", 10: "treasure show / hide", 11: "sign", 12: "special warp to outside Realgam Tower",
             13: "PC box", 14: "person helper", 15: "short pause", 16: "uses a Cologne", 17: "evolution check",
             18: "sends mail 10", 19: "TV news", 21: "story mail scheduler"}
EVENT_KIND = {1: "walk-through plane", 2: "door plane", 3: "examine plane", 4: "event kind 4"}

# Floor-table entries outside normal play (snatcherLeo, 2026-10-17): debug / test only, waived by default; anything later found
# reachable moves back. The first two are where the developer and key-disc builds boot (menu-rooms.md).
DEBUG_ROOMS = {"waza_viewer": "debug move viewer (developer build's first room)",
               "keydisc": "key-disc screen (the key-disc build's first room)",
               "": "empty floor-table entry"}
# The extra-disc screens left on the US main disc: real content the Bonus Content mod restores (design note 42), so
# status "later" (doesn't block step 2), not debug (snatcherLeo, 2026-10-17)
LATER_ROOMS = {a: "extra-disc screen left on the US main disc" for a in ("ex_title", "ex_top", "ex_coupon", "ex_shrine")}
# Battles that game code starts with a fixed number (verified from the code): {battle: (reach, default status)}.
# 561: script command 215 (std215_battleStartTrainerWithPrompt), used only by the Card e room's script, builds trainer
# 9 from a scanned card and starts 561; Card e is Japan-only, so "later" like C-RG01.
CODE_BATTLES = {561: ("Card e room M1_carde_1, script command 215 (opponent built from an e-Reader card)", "later")}
# Walkable places that use the menu classes (0xA0 / 0xC0) and are pushed like menus (snatcherLeo, 2026-10-17)
PUSHED_ROOMS = {"S2_hallway_1", "S2_snatchroom_1", "S2_bossroom_1", "S2_out_1", "S2_out_3", "M3_shrine_1F_rl",
                "D2_present_room"}

GIFT_POKEMON = {1: "starter gift 1 (Umbreon)", 2: "starter gift 2 (Espeon)", 3: "gift Pokémon (Plusle)",
                4: "gift Pokémon (Ho-Oh)", 5: "gift Pokémon (Pikachu)", 6: "gift Pokémon (Celebi)"}

# --- script listings: per function, the calls that make it a playable event ---
CALLS = [("battle", re.compile(r"\b(?:fieldStartEncount|fightStartEncount)\((\d+)")),
         ("link", re.compile(r"\bfloorLink(?:WithSE)?\((\d+)")),
         ("menu", re.compile(r"\bstd098_mapWarpToMenuRoom\((\d+)")),
         ("shop", re.compile(r"\bmenuShopOpen\((\d+)")),
         ("item", re.compile(r"\b(?:scriptGetItem|scriptAddItem)\((\d+)")),
         ("pokemon", re.compile(r"\bstd217_playerReceiveGiftPokemon\((\d+)")),
         ("mail", re.compile(r"\bmailMainSendByScrpt\((\d+)")),
         ("coupons", re.compile(r"\bscriptAddPokecoupon\(()")),
         ("trade", re.compile(r"\bstd198_menuEnterTradingMenu\(()")),
         ("carde", re.compile(r"\bstd202_warpToMenuRoom393\(()")),
         ("tv", re.compile(r"\bstd211_tvSetChannelByStory\(()")),
         ("camera", re.compile(r"\b(?:cameraPlayAnime|cameraMovePositionXYZ|cameraMoveRotationXYZ|cameraMoveTarget)\(()")),
         ("common", re.compile(r"\b0x0596([0-9a-f]{4})"))]
FUNC_HDR = re.compile(r"^func(\d+):")
FUNC_CALL = re.compile(r"\bfunc(\d+)\(")
# battles started from a variable: a local (filled by the caller, below) or a global
BATTLE_VAR = re.compile(r"\b(?:fieldStartEncount|fightStartEncount)\(([LG])(\d+)\b")
# a call with no arguments whose locals the caller fills: funcN(locals: a, b, c) (colo/script.py listing)
LOCALS_CALL = re.compile(r"\bfunc(\d+)\(locals: ([-\d, ]+)\)")
G_LITERAL = re.compile(r"^\s*G(\d+) = (\d+)\s*$")


def read_scripts(battle_ok=lambda arch, n: True):
    """{archive: {function: {kind: [args]}}} for every room script listing (battle-field scripts skipped).
    Battles started from a variable are followed two ways: a callee that starts battle L<k> gets it from the caller's
    k-th starting local (funcN(locals: ...)); a global G<k> that starts a battle takes every literal the script
    assigns to it, kept only when battle_ok(archive, number) (the battle is fought in this room)."""
    out = {}
    root = os.path.join(EX, "scripts")
    for area in sorted(os.listdir(root)):
        d = os.path.join(root, area)
        if not os.path.isdir(d) or area in ("common", "menus"):
            continue
        for fn in sorted(os.listdir(d)):
            if not fn.endswith(".txt") or "__" not in fn or "fight_common_scd" in fn:
                continue
            arch = fn.split("__")[0]
            funcs, cur, calls = {}, None, defaultdict(set)
            local_battles, global_battles = defaultdict(set), set()  # callee -> local index; global index
            local_calls, g_literals = [], []  # (caller, callee, values); (function, global, value)
            with open(os.path.join(d, fn), encoding="utf-8") as f:
                for line in f:
                    m = FUNC_HDR.match(line)
                    if m:
                        cur = int(m.group(1)); funcs[cur] = defaultdict(list); continue
                    if cur is None:
                        continue
                    code = line.split(";")[0]
                    for kind, rx in CALLS:
                        for a in rx.findall(code):
                            funcs[cur][kind].append(int(a) if a.isdigit() else a)
                    for c in FUNC_CALL.findall(code):
                        calls[cur].add(int(c))
                    for lg, k in BATTLE_VAR.findall(code):
                        (local_battles[cur].add(int(k)) if lg == "L" else global_battles.add(int(k)))
                    for c, vals in LOCALS_CALL.findall(code):
                        local_calls.append((cur, int(c), [int(v) for v in vals.split(",")]))
                    m = G_LITERAL.match(code)
                    if m:
                        g_literals.append((cur, int(m.group(1)), int(m.group(2))))
            for caller, callee, vals in local_calls:
                for k in local_battles.get(callee, ()):
                    if k < len(vals) and battle_ok(arch, vals[k]):
                        funcs[caller]["battle"].append(vals[k])
            # a global can reach the battle call through a local (Mt. Battle's valley: G170 = 204, later L0 = G170),
            # so any script that starts a battle from a variable takes its globals' literals, if fought in this room
            var_start = bool(global_battles or any(local_battles.values()))
            for fnum, g, v in g_literals:
                if (g in global_battles or var_start) and battle_ok(arch, v):
                    funcs[fnum]["battle"].append(v)
            # follow one level into called functions
            closed = {}
            for k, facts in funcs.items():
                merged = defaultdict(list, {a: list(b) for a, b in facts.items()})
                for c in calls[k]:
                    for a, b in funcs.get(c, {}).items():
                        merged[a] += b
                closed[k] = merged
            out[arch] = closed
    return out


def summarize(facts, rooms_by_id):
    """Our one-line description of what a script function does, from its calls."""
    parts = []
    if facts.get("battle"):
        parts.append("starts battle " + ", ".join(str(b) for b in sorted(set(facts["battle"]))))
    if facts.get("link"):
        parts.append("warps to " + ", ".join(rooms_by_id.get(r, f"room {r}") for r in sorted(set(facts["link"]))))
    if facts.get("menu"):
        parts.append("opens " + ", ".join(rooms_by_id.get(r, f"room {r}") for r in sorted(set(facts["menu"]))))
    if facts.get("shop"):
        parts.append("opens shop " + ", ".join(str(s) for s in sorted(set(facts["shop"]))))
    if facts.get("item"):
        parts.append(f"gives an item ({len(set(facts['item']))} kind(s))")
    if facts.get("pokemon"):
        parts.append("gives a Pokémon")
    if facts.get("coupons"):
        parts.append("gives Poké Coupons")
    if facts.get("mail"):
        parts.append("sends mail " + ", ".join(str(m) for m in sorted(set(facts["mail"]))))
    if facts.get("trade"):
        parts.append("opens trading")
    if facts.get("carde"):
        parts.append("opens the Card e menu")
    if facts.get("tv"):
        parts.append("sets the TV channel")
    if facts.get("camera"):
        parts.append("scripted camera scene")
    return "; ".join(parts)


def notable(facts):
    return any(facts.get(k) for k in ("battle", "link", "menu", "shop", "item", "pokemon", "coupons", "mail", "trade",
                                      "carde", "camera"))


def load_route():
    with open(ROUTE, encoding="utf-8") as f:
        r = json.load(f)
    room_step, battle_step, exits_to = {}, {}, defaultdict(list)
    for s in r["steps"]:
        arch = (s.get("room") or {}).get("archive")
        if arch:
            room_step.setdefault(arch, f"route step {s['step']}")
        for b in s.get("battle") or []:
            battle_step.setdefault(b["id"], f"route step {s['step']}")
    for p in r["postgame"]:
        room = p.get("room") if isinstance(p.get("room"), str) else (p.get("room") or {}).get("archive")
        if room:
            room_step.setdefault(room, f"postgame {p['id']}")
    for e in r["room_graph"]["exits"]:
        if e.get("to_archive"):
            exits_to[e["to_archive"]].append(e["from_archive"])
    return room_step, battle_step, exits_to


def build():
    rooms = table("rooms_decoded.csv")
    rooms_by_id = {int(r["room_id"]): r["archive"] for r in rooms}
    rooms_by_id.setdefault(910, "world_map")
    area_of = {r["archive"]: r["area"] for r in rooms}
    room_step, battle_step, exits_to = load_route()
    arena_room = {r["arena"]: r["room"] for r in table("arenas_decoded.csv")}
    battle_room = {int(b["battle"]): arena_room.get(b["arena"]) for b in table("battles_decoded.csv") if int(b["type"])}

    def battle_ok(arch, n):
        """A battle number found in a variable counts for this room only if the battle is fought here."""
        base = re.sub(r"_ev_\d+$", "", arch)
        return battle_room.get(n) in (arch, arch + "_bf", base, base + "_bf")
    scripts = read_scripts(battle_ok)
    rows, text = [], []

    def reach_room(arch):
        if arch in room_step:
            return room_step[arch]
        if arch in exits_to:
            return "exit from " + sorted(set(exits_to[arch]))[0]
        base = re.sub(r"(_ev_\d+|_bf)$", "", arch)
        if base != arch:
            return f"variant of {base} ({room_step.get(base, 'not on the route yet')})"
        return "not on the route yet (warp / checkpoint)"

    def add(id_, cat, group, desc, source, reach, system, default=""):
        rows.append(dict(id=id_, category=cat, group=group, description=desc, source=source, reach=reach,
                         system=system, default=default))

    # game modes (fixed list: new-game-flow.md, pause-menu.md, memory-card.md, gba-link.md, card-e.md, movies.md)
    for slug, group, desc, sysk in [
            ("boot-logos", "boot", "boot logos (Nintendo, Genius Sonority, Pokémon)", "movie"),
            ("title", "boot", "title screen and top menu", "menus"),
            ("attract-demo", "boot", "title screen left idle: the attract video", "movie"),
            ("new-game", "story", "new game: opening video, name entry, first room", "flags"),
            ("continue", "story", "continue from a save", "save"),
            ("memory-card-save", "memory card", "save the game (field and challenge saves)", "save"),
            ("memory-card-errors", "memory card", "no card / full card / damaged save handling", "save"),
            ("options", "options", "options screen", "menus"),
            ("gba-link", "GBA", "GBA link (trading and transfers)", "gba"),
            ("gba-upload", "GBA", "GBA upload features", "gba"),
            ("card-e", "Card e", "Card e menu and e-Reader battles", "ereader"),
            ("link-battle", "Battle Mode", "two-player link battle", "battle"),
            ("staff-roll", "story", "the ending and staff roll video", "movie")]:
        add(f"mode:{slug}", "game mode", group, desc, "docs (fixed list)", "title / top menu", SYS[sysk])
    for k, kind in enumerate(["Colosseum Battles", "Mt. Battle 100", "Gang Battle", "Battle Now"]):
        for r, rule in enumerate(["any level", "Lv50", "Lv100", "edited rule 1", "edited rule 2", "edited rule 3"]):
            add(f"mode:battle-mode:{k}:{r}", "game mode", "Battle Mode", f"Battle Mode: {kind}, rule {rule}",
                "pause-menu.md (save fields)", "top menu -> Battle Mode", SYS["battle"])

    # rooms and menu screens (floor table)
    for r in rooms:
        cls, arch = r["class"], r["archive"]
        if arch in DEBUG_ROOMS:
            rid = (f"menu:{arch}" if cls in ("0xa0", "0xc0") else f"room:{arch}") if arch else f"room:empty-{r['floor_index']}"
            add(rid, "debug / test only", "room",
                f"{DEBUG_ROOMS[arch]} {arch} (room id {r['room_id']}, class {cls})", "rooms_decoded.csv",
                "not reachable in normal play", SYS["room"], default="waived")
        elif arch in LATER_ROOMS:
            add(f"menu:{arch}", "menu screen", "bonus disc", f"{LATER_ROOMS[arch]} {arch} (room id {r['room_id']})",
                "rooms_decoded.csv", "not reachable on the US main disc alone", SYS["ereader"], default="later")
        elif arch in PUSHED_ROOMS:
            add(f"room:{arch}", "room (pushed)", r["area"],
                f"walkable room pushed like a menu (class {cls}) {arch} (room id {r['room_id']})", "rooms_decoded.csv",
                reach_room(arch), SYS["room"])
        elif cls in ("0xa0", "0xc0"):
            add(f"menu:{arch}", "menu screen", "title / top" if cls == "0xc0" else "menu",
                f"full-screen menu room {arch} (room id {r['room_id']})", "rooms_decoded.csv",
                reach_room(arch), SYS["menus"])
        else:
            kind = {"0x20": "field room", "0x40": "battle-field / arena room", "0x80": "move viewer (debug)",
                    "0x00": "room with class 0"}.get(cls, f"class {cls}")
            add(f"room:{arch}", "room", r["area"], f"{kind} {arch} (room id {r['room_id']})", "rooms_decoded.csv",
                reach_room(arch), SYS["room"])

    # room events (hero-event table)
    referenced = defaultdict(set)
    for e in table("events_decoded.csv"):
        arch, kind, sid, fn = e["room"], int(e["kind"]), e["script_id"], int(e["function"])
        a0, a1 = int(e["arg0"]), int(e["arg1"])
        what = EVENT_KIND.get(kind, f"event kind {kind}")
        if sid == "0x0596":
            name = COMMON_FN.get(fn, f"common function {fn}")
            if fn in (4, 6):
                name += f" to {rooms_by_id.get(a0, f'room {a0}')} (entry {a1})"
            elif fn == 5:
                name += f" {a0}"
            group = {4: "exit", 5: "door", 6: "lift", 11: "sign", 13: "PC", 19: "TV news"}.get(fn, "other")
        else:
            referenced[arch].add(fn)
            facts = scripts.get(arch, {}).get(fn, {})
            name = f"room script function {fn}" + (": " + summarize(facts, rooms_by_id) if notable(facts) else "")
            group = "room script"
        sysk = {"exit": "room", "door": "room", "lift": "room", "PC": "pc", "TV news": "events"}.get(group, "events")
        add(f"event:{arch}:k{kind}:{e['event_id']}", "room event", group, f"{what}: {name}", "events_decoded.csv",
            reach_room(arch), SYS[sysk])

    # talk events (NPC placements with a talk script)
    for p in table("npc_placements_decoded.csv"):
        ts = int(p["talk_script"], 16)
        if not ts:
            continue
        arch, group_id, fn = p["room"], ts >> 16, ts & 0xFFFF
        if group_id == 0x0100:
            referenced[arch].add(fn)
            facts = scripts.get(arch, {}).get(fn, {})
            what = summarize(facts, rooms_by_id) if notable(facts) else "talk"
        elif group_id == 0x0596:
            what = COMMON_FN.get(fn, f"common function {fn}")
        else:
            what = f"script {ts:#010x}"
        add(f"talk:{arch}:{p['slot']}", "talk event", area_of.get(arch, ""),
            f"person {p['slot']} (model {p['model']}): {what}", "npc_placements_decoded.csv", reach_room(arch),
            SYS["npc"])
        if p["name"]:
            text.append((f"talk:{arch}:{p['slot']}", p["name"]))

    # script events (other notable room-script functions) and the call sites that give things
    battle_callers, shop_callers, gifts = defaultdict(list), defaultdict(list), []
    for arch, funcs in scripts.items():
        for fn, facts in sorted(funcs.items()):
            for b in set(facts.get("battle", [])):
                battle_callers[b].append(f"{arch} function {fn}")
            for s in set(facts.get("shop", [])):
                shop_callers[s].append(f"{arch} function {fn}")
            for it in sorted(set(facts.get("item", []))):
                gifts.append((f"gift:{arch}:{fn}:item{it}", arch, f"item id {it} from {arch} function {fn}"))
            for k in sorted(set(facts.get("pokemon", []))):
                gifts.append((f"gift:{arch}:{fn}:pokemon{k}", arch,
                               f"{GIFT_POKEMON.get(k, f'gift Pokémon kind {k}')} from {arch} function {fn}"))
            if fn in referenced[arch] or not notable(facts):
                continue
            group = "scene" if facts.get("camera") else "script"
            add(f"script:{arch}:{fn}", "script event", group, f"{arch} function {fn}: " + summarize(facts, rooms_by_id),
                "script listing", reach_room(arch), SYS["cutscene" if group == "scene" else "script"])

    # battles
    for b in table("battles_decoded.csv"):
        n, kind = int(b["battle"]), int(b["type"])
        if not kind:
            continue
        group, kdesc = BATTLE_KIND.get(kind, ("other", f"battle kind {kind}"))
        sides = [s.split(":")[0].strip() for s in b["side_trainers"].split(";") if s.strip()]
        arena = b["arena"]
        desc = f"{kdesc}, arena {arena}, trainer ids {', '.join(sides) if sides else 'none'}"
        reach = battle_step.get(n) or (("started by " + battle_callers[n][0]) if battle_callers.get(n) else
                                       {"Battle Mode": "Battle Mode", "Card e": "Card e menu",
                                        "Mt. Battle": "Mt. Battle (challenge run)",
                                        "Colosseum": "Phenac Colosseum challenge",
                                        "story Colosseum": "story Colosseum challenge (rounds started by code)",
                                        "test": "not reachable (debug)"}.get(group, "not traced yet: no script "
                                                                             "starts it (likely game code, or unused)"))
        if n in CODE_BATTLES:
            reach, default = CODE_BATTLES[n]
            add(f"battle:{n}", "battle", "Card e", desc, "battles_decoded.csv / game code", reach, SYS["battle"],
                default=default)
        elif group == "test":
            add(f"battle:{n}", "debug / test only", "battle", desc, "battles_decoded.csv", reach, SYS["battle"],
                default="waived")
        else:
            add(f"battle:{n}", "battle", group, desc, "battles_decoded.csv", reach, SYS["battle"])
        if b["side_trainers"] or b["arena_name"]:
            text.append((f"battle:{n}", "; ".join(x for x in (b["arena_name"], b["side_trainers"]) if x)))

    # shops
    for s in table("shops_decoded.csv"):
        n, kind = int(s["shop"]), int(s["kind"])
        items_col = [c for c in s if c.startswith("items")][0]
        count = len([x for x in s[items_col].split(";") if x.strip()])
        kdesc = {0: "mart", 1: "vending machine"}.get(kind, "Poké Coupon exchange")
        callers = shop_callers.get(n, [])
        add(f"shop:{n}", "shop", kdesc, f"{kdesc}, {count} items" + (f", opened by {callers[0]}" if callers else ""),
            "shops_decoded.csv", reach_room(callers[0].split(" ")[0]) if callers else "not traced yet", SYS["shop"])
        text.append((f"shop:{n}", s[items_col]))

    # gifts: script call sites, then the gifts given by code (menu-rooms.md "Gift Pokémon")
    for gid, arch, desc in gifts:
        add(gid, "gift", "script", desc, "script listing", reach_room(arch), SYS["items"])
    for slug, desc, reach in [("starters", "the two starter Pokémon (new-game save)", "new game"),
                              ("ho-oh", "Ho-Oh after Mt. Battle in Battle Mode with all Shadow Pokémon purified",
                               "postgame, Battle Mode"),
                              ("pikachu", "Pikachu: the Japanese bonus disc's coupon desk (C-RG04); later, not US play",
                               "Japanese bonus disc"),
                              ("celebi", "Celebi: the Japanese bonus disc's shrine (C-RG02); later, not US play",
                               "Japanese bonus disc"),
                              ("agate-berries", "Agate Village's random berry gift (room gift table)",
                               reach_room("M3_out"))]:
        add(f"gift:code:{slug}", "gift", "code", desc, "menu-rooms.md / room gift table", reach, SYS["items"],
            default="later" if slug in ("pikachu", "celebi") else "")

    # world items (treasure table)
    for t in table("treasure_decoded.csv"):
        add(f"item:{t['index']}", "world item", area_of.get(t["room"], ""),
            f"treasure in {t['room']} (container kind {t['container_kind']}, quantity {t['quantity']}, "
            f"collected flag {t['flag']})", "treasure_decoded.csv", reach_room(t["room"]), SYS["world_item"])
        text.append((f"item:{t['index']}", t["item"]))

    # side features (fixed list) and every mail the scripts send
    for slug, desc, sysk in [("tv-news", "TV news in houses and the Outskirt Stand shop", "events"),
                             ("pda", "P★DA: mail, Strategy Memo, Pokémon list", "menus"),
                             ("strategy-memo", "Strategy Memo entries filled as Pokémon are seen and caught", "menus"),
                             ("purification", "purification at the Relic Stone", "shadow"),
                             ("heart-gauge", "Shadow Pokémon heart gauge and Hyper Mode", "shadow"),
                             ("snagging", "snagging Shadow Pokémon in battle", "shadow"),
                             ("day-care", "day care in Agate Village (reachable; C-ST21)", "events"),
                             ("trading", "trading (pokemonchange_menu)", "trade"),
                             ("evolution", "evolution scene", "events"),
                             ("cologne", "using a Cologne", "items"),
                             ("name-entry", "name entry (player, partner, nickname, box)", "menus"),
                             ("pc-box", "PC box (deposit, withdraw, move)", "pc"),
                             ("bag", "bag and items menu", "pc"),
                             ("party", "party menu and summary", "menus"),
                             ("pause-menu", "field pause menu", "menus"),
                             ("dna-key", "Shadow Pokémon Lab DNA key screen", "menus"),
                             ("mt-battle-rest", "Mt. Battle rest rooms (heal, save, PC)", "colo")]:
        add(f"feature:{slug}", "side feature", "feature", desc, "docs (fixed list)", "", SYS[sysk])
    mails = defaultdict(list)
    for arch, funcs in scripts.items():
        for fn, facts in funcs.items():
            for m in set(facts.get("mail", [])):
                mails[m].append(f"{arch} function {fn}")
    for m in (6, 8, 12, 13, 14, 15, 21, 22, 23, 24, 27, 28):
        mails[m].append("common script function 21")
    mails[10].append("common script function 18")
    for m in sorted(mails):
        add(f"feature:mail:{m}", "side feature", "mail", f"P★DA mail {m}, sent by {sorted(set(mails[m]))[0]}",
            "script listing / common-script.md", "", SYS["pc"])

    # Rows the data can't reach get the fixed marker "untraced" (snatcherLeo, 2026-10-17; testing-plan.md 1a): never assumed
    # unused, still counted for step 2. A row leaves it only on runtime evidence: confirmed reachable (then tested like
    # any row) or confirmed unused (then moved to debug / test only, with the evidence in the row). What we know goes
    # in the description.
    runtime = {}  # (kind, id) -> evidence, from runtime logs (tools/local/testing/idlog_apply.py)
    if os.path.exists(RUNTIME_CSV):
        with open(RUNTIME_CSV, encoding="utf-8", newline="") as f:
            for x in csv.DictReader(f):
                runtime.setdefault((x["kind"], int(x["id"])), f"{x['evidence']} ({x['date']})")
    room_id_of = {r["archive"]: int(r["room_id"]) for r in rooms}

    def seen_at_runtime(r):
        kind, _, rest = r["id"].partition(":")
        if kind == "battle" and rest.isdigit():
            return runtime.get(("battle", int(rest)))
        if kind in ("room", "menu") and rest in room_id_of:
            return runtime.get(("room", room_id_of[rest]))
        return None

    # Rows resolved as unused from the data (snatcherLeo, 2026-10-17: the 72 untraced story battles, trainer-encounters.md
    # "The 72, by family"): moved to debug / test only with their family and evidence, so they no longer count for
    # step 2. A runtime log that shows one started still wins (checked first).
    unused = {}  # (kind, id) -> (family, evidence)
    if os.path.exists(UNUSED_CSV):
        with open(UNUSED_CSV, encoding="utf-8", newline="") as f:
            for x in csv.DictReader(f):
                unused[(x["kind"], int(x["id"]))] = (x["family"], x["evidence"])

    for r in rows:
        ev = seen_at_runtime(r) if r["reach"].startswith(("not traced", "not on the route yet")) else None
        kind, _, rest = r["id"].partition(":")
        un = unused.get((kind, int(rest))) if rest.isdigit() and r["reach"].startswith("not traced") else None
        if ev:
            r["reach"] = f"runtime: {ev}"
        elif un:
            r["reach"] = f"unused: {un[0]}"
            r["category"] = "debug / test only"
            r["default"] = "waived"
            r["description"] += f"; resolved as unused from the data: {un[1]}"
        elif r["reach"].startswith("not traced"):
            r["reach"] = UNTRACED
            r["description"] += ("; named by no script and by no fixed number in the code (computed or unused: guess; "
                                 "a runtime battle log settles it)"
                                 if r["category"] == "battle" else "; no script opens it with a fixed number")
        elif r["reach"].startswith("not on the route yet"):
            r["reach"] = UNTRACED
            r["description"] += "; no route step or exit leads here"
    return rows, text


def merge(rows):
    old = {}
    if os.path.exists(OUT_CSV):
        with open(OUT_CSV, encoding="utf-8", newline="") as f:
            old = {r["id"]: r for r in csv.DictReader(f)}
    if not os.path.exists(EXTRA_CSV):
        with open(EXTRA_CSV, "w", encoding="utf-8", newline="") as f:
            csv.writer(f).writerow(COLUMNS)
    extra = []
    for path in (MANUAL_CSV, EXTRA_CSV):
        if os.path.exists(path):
            with open(path, encoding="utf-8", newline="") as f:
                extra += [dict(r, source=r.get("source") or os.path.basename(path))
                          for r in csv.DictReader(f) if r.get("id")]
    ids = {r["id"] for r in rows}
    dupes = [r["id"] for r in extra if r["id"] in ids]
    rows = rows + [r for r in extra if r["id"] not in ids]
    seen = Counter(r["id"] for r in rows)
    if any(n > 1 for n in seen.values()):
        sys.exit("duplicate ids: " + ", ".join(i for i, n in seen.items() if n > 1)[:500])
    for r in rows:
        prev = old.get(r["id"], {})
        for k in KEPT:
            r[k] = (prev.get(k) or r.get(k) or "").strip()
        # a row's default status (waived for debug / test only, later for bonus-disc content) replaces an untouched
        # "not tested"; a status someone recorded is kept
        if r.get("default") and r["status"] in ("", "not tested"):
            r["status"] = r["default"]
        r["status"] = r["status"] or "not tested"
    gone = sorted(set(old) - {r["id"] for r in rows})
    return rows, dupes, gone


def write(rows, text, dupes, gone):
    with open(OUT_CSV, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, COLUMNS, extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)
    local = os.path.join(EX, "testing")
    os.makedirs(local, exist_ok=True)
    with open(os.path.join(local, "playability_text.csv"), "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["id", "game_text"])
        w.writerows(text)

    by_cat = defaultdict(Counter)
    by_group = defaultdict(Counter)
    for r in rows:
        by_cat[r["category"]][r["status"]] += 1
        by_group[r["category"]][r["group"]] += 1
    cats = [c for c in CATEGORIES if c in by_cat] + sorted(set(by_cat) - set(CATEGORIES))
    unknown = sorted({r["status"] for r in rows} - set(STATUSES))
    lines = ["# Playability inventory: summary", "",
             "Generated by `tools/local/playability.py` from the decoded tables in `extracted/` and `route.json`; do",
             "not edit by hand. The rows are in [playability.csv](playability.csv); snatcherLeo's own rows go in",
             "[playability-extra.csv](playability-extra.csv) (merged in, never overwritten); the hand cross-check's",
             "rows (rules and features the tables can't show, group \"manual\") are in",
             "[playability-manual.csv](playability-manual.csv). Status, verified-by,",
             "last-build and date are kept by id when the inventory is regenerated. The plan behind it:",
             "[testing-plan.md](../testing-plan.md) section 1a (roadmap step 2's definition of done).", "",
             f"**{len(rows)} rows.**", "",
             "| Category | Rows | " + " | ".join(STATUSES + unknown) + " |",
             "|---|---:|" + "---:|" * len(STATUSES + unknown)]
    for c in cats:
        lines.append(f"| {c} | {sum(by_cat[c].values())} | " +
                     " | ".join(str(by_cat[c].get(s, 0)) for s in STATUSES + unknown) + " |")
    tot = Counter()
    for c in cats:
        tot.update(by_cat[c])
    lines.append(f"| **total** | **{len(rows)}** | " + " | ".join(f"**{tot.get(s, 0)}**" for s in STATUSES + unknown) + " |")
    scope = [r for r in rows if r["status"] not in ("waived", "later")]
    done = [r for r in scope if r["status"] in ("works", "matches Dolphin")]
    later = [r for r in rows if r["status"] == "later"]
    debug = [r for r in rows if r["category"] == "debug / test only"]
    untraced = [r for r in rows if r["reach"] == UNTRACED]
    lines += ["", "## Step 2 scope", "",
              f"- **Counted for step 2:** {len(scope)} rows, {len(done)} done (works or matches Dolphin).",
              f"- **Later** (real content for after step 2; doesn't block it): {len(later)}: "
              + ", ".join(r["id"] for r in later) + ".",
              f"- **Debug / test only** (not reachable in normal play; waived by default; anything later found "
              f"reachable moves back): {len(debug)}: " + ", ".join(r["id"] for r in debug) + ".",
              f"- **Waived by snatcherLeo:** {sum(1 for r in rows if r['status'] == 'waived' and r not in debug)}.",
              f"- **Untraced** (the data names no way in; never assumed unused; still counted for step 2): "
              f"{len(untraced)}: " + ", ".join(f"{c} {n}" for c, n in Counter(r["category"] for r in untraced).most_common())
              + ". A row leaves untraced on runtime evidence (confirmed reachable: then tested like any row) or when it "
              "is confirmed unused (then debug / test only, with the evidence in the row; from the data too, listed in "
              "unused-ids.csv, snatcherLeo 2026-10-17)."]
    n_floor = len(table("rooms_decoded.csv"))
    fl = Counter(r["category"] for r in rows if r["source"] == "rooms_decoded.csv")
    fl_later = sum(1 for r in rows if r["source"] == "rooms_decoded.csv" and r["status"] == "later")
    lines += ["", f"**Floor table:** {n_floor} entries = {fl['room']} rooms + {fl['room (pushed)']} walkable rooms "
              f"pushed like menus + {fl['menu screen']} menu screens (of them {fl_later} later: the extra-disc screens) "
              f"+ {fl['debug / test only']} debug / test only. The menu classes (0xA0 / 0xC0) hold the pushed rooms and "
              "the menu screens."]
    lines += ["", "## Groups", "", "| Category | Groups (rows) |", "|---|---|"]
    for c in cats:
        lines.append(f"| {c} | " + ", ".join(f"{g or '(none)'} {n}" for g, n in sorted(by_group[c].items(),
                                                                                         key=lambda x: -x[1])) + " |")
    def reach_kind(s):
        if s.startswith(("route step", "postgame")): return "route step"
        if s == UNTRACED: return UNTRACED
        if s.startswith("runtime:"): return "runtime log"
        if s.startswith("variant of"): return "room variant"
        if s.startswith(("exit from", "started by", "story Colosseum")): return " ".join(s.split(" ")[:2])
        return s.split(" (")[0].split(",")[0]
    reach = Counter("manual row (reach in its own words)" if r["group"].startswith("manual") else reach_kind(r["reach"])
                    for r in rows)
    lines += ["", "## How rows are reached", "",
              "Rows with a route step come from `route.json` (the story route so far); the rest name the room exit,",
              "script function or menu that leads there; \"untraced\" = the data names no way in yet.", "",
              "| Reach | Rows |", "|---|---:|"]
    for k, n in reach.most_common():
        lines.append(f"| {k or '(none: fixed features)'} | {n} |")
    lines += ["", "## Notes", "",
              "- **Rooms and battles** are verified table rows (floor table, battle table). What a script function",
              "  does (\"starts battle 10; warps to ...\") is read from its calls, following one level into the functions",
              "  it calls: **likely** (a call inside a branch may not run every time).",
              "- **Script events** list only room-script functions that do something playable (battle, warp, menu, shop,",
              "  gift, mail, Card e, trading, or a scripted camera) and that no hero event or person already points at.",
              "  \"scene\" = it moves the camera (**likely** a cutscene).",
              "- **Game modes and side features** are fixed lists written from the docs; Battle Mode is listed as",
              "  4 kinds x 6 rules (pause-menu.md save fields).",
              "- **Gaps:** hero-event kind 4 is not named yet; the door table (doors_decoded.csv) is covered through",
              "  the door planes; menu-room scripts and battle-field scripts are not split into events yet.",
              f"- **Untraced story battles** ({sum(1 for r in untraced if r['category'] == 'battle')}): no",
              "  script holds their number. Battles started from a variable are followed: a helper's starting locals",
              "  (`funcN(locals: ...)`, e.g. Realgam Tower's Colosseum) and a global the script assigns a number to",
              "  (Mt. Battle's valley), kept only when the battle is fought in that room. Game code starting a fixed",
              "  number was searched too (it gave battle 561, the Card e card battle). The rest are untraced: computed",
              "  from unread data or unused (**guess**); a runtime log of started battles settles it (trainer-encounters.md).",
              f"- **Resolved as unused** ({sum(1 for r in rows if r['reach'].startswith('unused:'))}, snatcherLeo 2026-10-17):",
              "  rows listed in [unused-ids.csv](unused-ids.csv) with their family and evidence (the former untraced",
              "  battles: the unfinished postgame, the Lab's \"Shadow already snagged\" variants, Nore's middle pair;",
              "  trainer-encounters.md \"The 72, by family\"). They are debug / test only and waived; a runtime log showing",
              "  one started would move it back.",
              "- Game text (trainer, NPC, item and shop names) is only in the local view",
              "  `extracted/testing/playability_text.csv`, never in the committed files."]
    if dupes:
        lines += [f"- Extra rows ignored because a generated row has the same id: {', '.join(dupes)}."]
    if gone:
        lines += [f"- Rows from the previous inventory that are no longer generated ({len(gone)}): "
                  + ", ".join(gone[:40]) + (" ..." if len(gone) > 40 else "") + "."]
    with open(OUT_MD, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    return lines


if __name__ == "__main__":
    rows, text = build()
    rows, dupes, gone = merge(rows)
    lines = write(rows, text, dupes, gone)
    print("\n".join(lines[8:8 + 16]))
