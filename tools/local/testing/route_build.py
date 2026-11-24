"""Build docs/testing/route.json, the machine-readable story route (testing lane; with the project's docs/engine/story-route.md).

    python -I tools/local/testing/route_build.py [extracted_dir] [out.json]

The steps (order, flags, setter, room, trigger, needs, battle, note) are the project's part 1 of story-route.md, written here as
data; every id-based field is filled from the extracted tables (git-ignored, regenerated from the disc):
- flags: storage class, chain position (= story order, flags.md), jump_target = the flag's definition has an event
  (fn_8018FE30 jump target) -- from data_tables/flags_decoded.csv;
- rooms: id <-> archive -- rooms_decoded.csv;
- battles: arena and trainers -- battles_decoded.csv;
- the room graph: every walk-through exit (event kind 1 calling common function 4: target room, entry point) --
  events_decoded.csv.
The output holds only ids, names and short notes in our own words (no game text, no disc data beyond names and ids).
The "rooms" section carries the walkers' data (waypoints, input rules) used by tools/local/testing/dolphin_walk.py and,
later, the PC pad bot.
"""
import csv, json, os, sys

EX = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "..", "..", "..", "..",
                                                         "pkmn_colosseum", "extracted")
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "..", "..", "..", "docs",
                                                          "testing", "route.json")
T = os.path.join(EX, "data_tables")

def rows(name):
    return list(csv.DictReader(open(os.path.join(T, name), encoding="utf-8")))

FLAGS = {int(r["flag"]): r for r in rows("flags_decoded.csv")}
ROOMS = {r["archive"]: int(r["room_id"]) for r in rows("rooms_decoded.csv")}
ROOM_NAME = {v: k for k, v in ROOMS.items()}
BATTLES = {int(r["battle"]): r for r in rows("battles_decoded.csv")}
EVENTS = rows("events_decoded.csv")
ROOM_NAME.setdefault(910, "world_map")
ROOM_NAME.setdefault(0x3A0, "opening_demo")

# The steps come from the project's docs/engine/story-route.md (its step tables: # | flag(s) (chain pos.) | room, trigger | needs |
# battle | what you see), parsed here so route.json follows the page. Pass its path as argv[3]; default: the page in this
# checkout.
import re
SR = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(OUT), "..", "engine", "story-route.md")
SR_TEXT = open(SR, encoding="utf-8").read()

def nums(text):
    """Numbers in a cell, ranges expanded (3570-3572), bold marks dropped."""
    out = []
    for m in re.finditer(r"(\d+)(?:\s*-\s*(\d+))?", text.replace("*", "")):
        lo = int(m.group(1)); hi = int(m.group(2)) if m.group(2) else lo
        out += list(range(lo, hi + 1)) if 0 <= hi - lo < 64 else [lo]
    return out

def trigger_kind(t):
    t = t.lower()
    for key, kind in (("movie", "cutscene_end"), ("examine", "examine"), ("box", "enter_area"), ("talk", "talk"),
                      ("door", "door_or_exit"), ("leaving", "door_or_exit"), ("battle", "win_battle"),
                      ("main loop", "on_room_entry"), ("on arrival", "on_room_entry"), ("on entry", "on_room_entry"),
                      ("first entry", "on_room_entry"), ("code", "code")):
        if key in t:
            return kind
    return "other"

MAIN_PATH = set()
for m in re.finditer(r"main path[^\n]*?steps? ([0-9 ,\-]+)", SR_TEXT, re.I):
    MAIN_PATH.update(nums(m.group(1)))

# flags no script sets (debug-jump chapter markers): a step does not need them to count as complete
NO_SETTER = set()
_m = re.search(r"\*\*Nothing found \(\d+ flags\)\*\*:(.*?)Five of them", SR_TEXT, re.S)
if _m:
    NO_SETTER = set(nums(_m.group(1)))

STEPS = []
POSTGAME = []
for line in SR_TEXT.splitlines():
    mp = re.match(r"^\|\s*(P\d+)\s*\|(.*)$", line)
    if mp:
        cells = [c.strip() for c in mp.group(2).rstrip("|").split("|")]
        if len(cells) >= 5:
            fl = list(dict.fromkeys(nums(re.sub(r"\([^)]*\)", "", cells[0]))))
            rooms = re.findall(r"`([A-Z]\d_[A-Za-z0-9_]+)`", cells[1])
            POSTGAME.append({"id": mp.group(1), "flags": fl, "room": rooms[0] if rooms else None,
                             "trigger": re.sub(r"\*\*|`", "", cells[1]).strip(),
                             "battle_text": re.sub(r"\*\*|`", "", cells[3]).strip(),
                             "see": re.sub(r"\*\*|`", "", cells[4]).strip(), "optional": True})
        continue

    m = re.match(r"^\|\s*(\d+)\s*\|(.*)$", line)
    if not m:
        continue
    cells = [c.strip() for c in m.group(2).rstrip("|").split("|")]
    if len(cells) < 5:
        continue
    n = int(m.group(1)); fcell, rcell, need, bcell, see = cells[:5]
    flags = list(dict.fromkeys(nums(re.sub(r"\([^)]*\)", "", fcell).replace("jump target", ""))))
    rooms = re.findall(r"`([A-Z]\d_[A-Za-z0-9_]+|[a-z_]+_(?:demo|menu|map))`", rcell)
    clear = nums(" ".join(re.findall(r"not ([0-9, /and]+)", need)))
    setn = [f for f in nums(re.sub(r"not [0-9, /and]+", "", need)) if f > 50]
    bat = nums(re.sub(r"\([^)]*\)", "", bcell))
    clean = lambda t: re.sub(r"\*\*|`", "", t).strip()
    STEPS.append((n, flags, rooms[0] if rooms else None, clean(rcell), trigger_kind(rcell), (setn, clear), bat,
                  "optional" in see.lower() or "optional" in rcell.lower(), clean(see)))

# Steps a test run has reached, and how
REACHED = {n: "dolphin-walk 2026-10-16, flags read from the after_willie_menu state (dolphin_flags.py + flag_report.py): "
             "all of the step's flags set; furthest chain position 1678" for n in range(1, 7)}
REACHED.update({n: "dolphin_phenac 2026-10-16, flags read from the after_folly state: all of the step's flags set; furthest "
                   "chain position 1682" for n in (7, 8)})
REACHED.update({n: "dolphin_goto 2026-10-16 (path_plan.py route from the fountain; the plaza man's scene plays on the way "
                   "to the mayor's door): flags set in the mayor_house state" for n in (10, 11)})
REACHED[12] = ("dolphin_goto 2026-10-16: out of the mayor's house (around Rui, who stands behind Wes), path_plan route to "
               "the Stadium door, the reception desk talked to from right against the counter (z 18; from z 28 nothing "
               "happens): 998 set (desk state)")
REACHED[13] = ("dolphin_goto 2026-10-16 from the desk state: out of the Stadium (sidesteps around Rui), Wakin's scene on "
               "coming outside, battle in room 202 won with A: 999 2832 set (after_wakin state)")
REACHED[14] = ("dolphin_goto 2026-10-16: path_plan route from the Stadium to the mart's diagonal door, then to the clerk "
               "obasan_b1 at (-16, -50) (talk function 8): 2462 set (mart_talk state)")
REACHED[9] = ("dolphin_goto 2026-10-16 from after_folly: the sack opened in the project's box, Rui named RUI on the keyboard, YES to "
              "'stick with me' (UP + A; the box loops on No); flags 2454 996 997 2767 set, chain 1686 (rui_partner state)")

def flag_info(f):
    r = FLAGS.get(f)
    if r is None:
        return {"id": f}
    return {"id": f, "required": f not in NO_SETTER, "storage_class": int(r["storage_class"]), "chain_position": int(r["story_chain_position"]),
            "jump_target": r["event_index"] not in ("0", "")}

def battle_info(b):
    r = BATTLES.get(b, {})
    trainers = []
    for part in (r.get("side_trainers") or "").split(";"):
        part = part.strip()
        if ":" in part:
            tid, name = part.split(":", 1)
            if tid.strip() != "1":            # 1 = the player
                trainers.append({"id": int(tid), "name": name.strip()})
    return {"id": b, "arena": int(r["arena"]) if r.get("arena") else None, "trainers": trainers}

steps = []
for (n, flags, room, ref, kind, (need_set, need_clear), battle, optional, see) in STEPS:
    fi = [flag_info(f) for f in flags]
    steps.append({
        "step": n, "flags": fi,
        "chain_position": min((f["chain_position"] for f in fi if "chain_position" in f), default=None),
        "jump_target": any(f.get("jump_target") for f in fi),
        "room": {"id": ROOMS.get(room), "archive": room},
        "trigger": {"kind": kind, "ref": ref},
        "requires": {"flags_set": need_set, "flags_clear": need_clear},
        "battle": [battle_info(b) for b in battle] or None,
        "optional": optional, "main_path": (n in MAIN_PATH) if MAIN_PATH else not optional,
        "see": see, "verified": REACHED.get(n),
    })

# room graph: walk-through exits (kind 1, common script 0x0596 function 4: arg0 = target room, arg1 = entry point)
exits = []
for e in EVENTS:
    if e["kind"] == "1" and e["script_id"] == "0x0596" and e["function"] == "4":
        exits.append({"from": int(e["room_id"]), "from_archive": e["room"], "event": int(e["event_id"]),
                      "to": int(e["arg0"]), "to_archive": ROOM_NAME.get(int(e["arg0"])), "entry": int(e["arg1"])})

ROOMS_WALK = {
    "S1_out": {"waypoints_to_shop": [[-15.0, 14.0], [-15.0, -46.0]],
               "notes": "ramp foot, then up through the diner door (event 4 plane at z = -38.6)"},
    "S1_shop_1F": {"waypoints_out": [[-53.0, 42.0]], "notes": "exit plane at z = 30.4"},
}
INPUT_RULES = [
    {"where": "S1_out after battle 10", "rule": "Willie's last line closes with B, not A (verified, Dolphin probe)"},
    {"where": "top menu (room 929)", "rule": "the cursor starts on NEW GAME when the card holds a save (verified)"},
    {"where": "world map (room 910) from Outskirt Stand", "rule": "LEFT then A picks Phenac City (verified, Dolphin)"},
    {"where": "after a battle", "rule": "the room id reads 0xFFFFFFFF (none) until the next room change; the trainer's "
                                        "lines advance with A (verified, Willie and Folly)"},
    {"where": "M1_out, the sack (step 9)", "rule": "stand inside x -8..10, z 314..340, face the sack at (0, 313), A; "
                                                   "the naming screen opens about 1,430 frames after the first A"},
    {"where": "M1_out, Rui's 'stick with me' question (step 9)", "rule": "a yes / no that loops on No; the cursor starts on No: "
                                                                         "UP then A (verified: 996 / 997 set)"},
    {"where": "M1_stadium_1F reception (step 12)", "rule": "talk from right against the counter: (0, 18) facing (0, 0); "
                                                         "from z 28 the A press does nothing (verified)"},
    {"where": "after a scene with Rui", "rule": "she stands right behind Wes: start a walk with a sideways detour (verified)"},
    {"where": "after scenes that send P*DA mail (steps 64 -> 67: mail 0 sets 3531; step 122: mail 2 sets 2531; much of "
              "the postgame)", "rule": "open the P*DA and read the new mail: reading sets the gate flag (from the mail "
                                       "module)"},
    {"where": "naming keyboard", "rule": "the cursor starts on A; the blank gap between E/F (O/P, Y/Z) is a cursor column, so "
                                         "R = DOWN + 8 x RIGHT, U = DOWN + 10 x LEFT, I = 2 x UP + 9 x RIGHT; START = done "
                                         "(verified: RUI typed)"},
]

out = {"about": "Colosseum (USA) story route (the project's story-route.md steps, parsed; ids from the disc tables). Generated by"
                 " tools/local/testing/route_build.py.",
       "fields": "see docs/testing/README.md, 'route.json'",
       "steps": steps,
       "postgame": [dict(g, flags=[flag_info(f) for f in g["flags"]]) for g in POSTGAME],
       "room_graph": {"exits": exits},
       "rooms": ROOMS_WALK, "input_rules": INPUT_RULES}
json.dump(out, open(OUT, "w", encoding="utf-8"), indent=1, ensure_ascii=False)
print(OUT, len(steps), "steps,", len(exits), "exits")
