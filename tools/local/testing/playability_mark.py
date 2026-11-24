"""Mark playability rows covered by a set of verified replays (2026-11-01; testing-plan.md 1a).

    python -I tools/local/testing/playability_mark.py <disc image> --evidence "<what proved it>" SITTING...
        [--steered SITTING...] [--apply]

SITTING is a recorded sitting folder (`_playthrough/sections/<section>/<sitting>`) whose PC replay reached its end with
the console's room entries, battles and story flags (match-ledger.md). From each sitting's console recording it takes:
- rooms and menu rooms entered (the `flags_entry_NNN_<room id>.json` names) -> `room:` / `menu:` rows
  "matches Dolphin" (entered in the same order with the same story flags, the standard of the golden-slice rows);
- battles started (`events.jsonl` "battle" lines) -> `battle:` rows "matches Dolphin" (the replay's battles ran as on
  the console: same ids, messages, results), or "works" when the battle appears only in `--steered` sittings (replays
  that needed logged resyncs);
- world items whose collected flag a sitting set (its last flag dump against its first) -> `item:` rows "works"
  (picked up on the PC; the bag itself is not compared row by row);
- exits and lifts (room-event rows "exit to X" / "lift to X"): a room change A -> X in the entry sequence marks A's
  exit or lift to X "matches Dolphin" when A has only one such row (likely the one used: doors name no destination,
  and the walk-through plane behind a door is the exit row itself); several rows to the same room stay untouched.
- with --pc-runs (2026-11-02, the reference playthrough Part 1 item 6): room-script functions (script group 0x0100, the current
  room's script) that the console started in a room (the `scripts` probe: the sitting's newest `replay_N/scripts.csv`,
  converter test passes with a `movie.txt` skipped) AND the PC's final run of the sitting started there too (its
  `[script] start` lines, rooms from its `[slice] entered` lines; paired by function order, a pair only when its
  rooms agree) -> `script:` rows of that function, and `talk:` / `event:` rows whose script is that function when no
  other person or room event in the room uses it and a message window opened within 30 frames after the console's
  start; "matches Dolphin" from strict sittings, "works" from steered ones. Gift rows are not marked from a start (a
  gift function runs on every visit and checks its "received" flag; the review, 2026-11-02). Prints per sitting how much of the console's
  start order the PC's run repeats (the order check; the console probe misses starts that end inside one frame).
Only rows still "not tested" (or empty) change, plus rows this tool marked earlier with the same --evidence text,
which are recomputed (and reset to "not tested" if no longer proved); any other recorded status is never overwritten. Without --apply it prints
what would change. Afterwards rerun tools/local/playability.py to refresh the summary page (statuses are kept by id).
"""
import argparse, bisect, collections, csv, datetime, glob, json, os, re, struct, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.dirname(HERE))
from colo.disc import Disc  # noqa: E402
from colo.tables import common_rel, table_rows  # noqa: E402
from colo.savefile import flag_layout  # noqa: E402

CSV_PATH = os.path.join(REPO, "docs", "testing", "playability.csv")
TABLES = os.path.join(REPO, "extracted", "data_tables")
ENTRY = re.compile(r"flags_entry_\d+_([0-9a-f]+)\.json$")


def set_flags(words, layout):
    out = set()
    for i, (cls, w, pos) in enumerate(layout):
        if pos is None or cls >= len(words):
            continue
        buf, wi, bp = words[cls], pos >> 5, pos & 31
        if wi >= len(buf):
            continue
        v = buf[wi] >> bp
        if w > 1 and bp and wi + 1 < len(buf):
            v |= buf[wi + 1] << (32 - bp)
        if v & ((1 << w) - 1):
            out.add(i)
    return out


def console_starts(sit):
    """([(room id, function, talked)], replay) of the room-script (group 0x0100) starts in the sitting's newest scripts
    replay, or (None, None). talked: a field message window opened within 30 frames after the start (the replay's own
    msgwin events; the review, 2026-11-02: a start alone doesn't show that a talk happened)"""
    reps = [d for d in glob.glob(os.path.join(sit, "replay_*")) if os.path.exists(os.path.join(d, "scripts.csv"))
            and not os.path.exists(os.path.join(d, "movie.txt"))]  # movie.txt: a converter test pass
    if not reps:
        return None, None
    rep = max(reps, key=lambda d: int(d.rsplit("_", 1)[1]))
    wins = []
    ev = os.path.join(rep, "replay_events.jsonl")
    if os.path.exists(ev):
        for line in open(ev, encoding="utf-8"):
            if '"ev": "msgwin"' in line:
                e = json.loads(line)
                if e.get("type", -1) >= 0:
                    wins.append(e["f"])
    wins.sort()
    out = []
    for r in csv.DictReader(l for l in open(os.path.join(rep, "scripts.csv"), encoding="utf-8") if not l.startswith("#")):
        if r["bank"] == "0x100":
            f = int(r["frame"])
            i = bisect.bisect_left(wins, f)
            out.append((int(r["room"]), int(r["func"]), i < len(wins) and wins[i] - f <= 30))
    return out, os.path.basename(rep)


def pc_starts(run):
    """[(room id, function, next room)] of the room-script starts in a PC run's log ([slice] entered gives the room;
    next room: the one the next [slice] entered names, or None)"""
    room, out, pending = None, [], []
    for line in open(os.path.join(run, "run.log"), encoding="utf-8", errors="replace"):
        m = re.match(r"\[slice\] entered 0x([0-9A-Fa-f]+)", line)
        if m:
            room = int(m.group(1), 16)
            for k in pending:
                out[k] = out[k][:2] + (room,)
            pending = []
            continue
        m = re.match(r"\[script\] start 0x0100([0-9A-Fa-f]{4})", line)
        if m and room is not None:
            pending.append(len(out))
            out.append((room, int(m.group(1), 16), None))
    return out


BAG_POCKETS = (0x14, 0x2B, 0x10, 0x40, 0x2E, 3)  # save section 2's item pockets after the party (save-data.md)


def bag(gci):
    """{item id: count} from a save file's bag (section 2 at block +0x70: name 0x2C, rnd 4, party 6 x 0x138, then the
    pockets, each slot {u16 id, u16 count}), or None"""
    from colo.savefile import current_block
    try:
        block = current_block(open(gci, "rb").read())
    except (OSError, ValueError):
        return None
    if block is None:
        return None
    out, o = collections.Counter(), 0x70 + 0x30 + 6 * 0x138
    for n in BAG_POCKETS:
        for i in range(n):
            item, count = struct.unpack(">HH", block[o + 4 * i:o + 4 * i + 4])
            if item:
                out[item] += count
        o += 4 * n
    return out


def gifts_received(sit, run):
    """item ids the sitting gained (the console's end save holds more than its start save) that the PC's end save holds
    as many of: the gift rows' evidence (the review, 2026-11-02: a gift function's start alone isn't the gift)"""
    first = lambda pattern: next(iter(sorted(glob.glob(pattern))), None)
    paths = (first(os.path.join(sit, "card_start", "*.gci")), first(os.path.join(sit, "card_end", "*.gci")),
             first(os.path.join(run, "user", "USA", "Card A", "*.gci")))
    if None in paths:
        return set()
    start, end, pc = (bag(p) for p in paths)
    if None in (start, end, pc):
        return set()
    return {i for i, n in end.items() if n > start.get(i, 0) and pc.get(i, 0) >= n}


def room_scripts(a, archive, steered, want, note):
    """the room-script rows (--pc-runs): see the module doc. Returns the count of (room, function) pairs proved."""
    import difflib
    runs = {}
    for run in a.pc_runs:
        env = open(os.path.join(run, "check.env"), encoding="utf-8").read()
        m = re.search(r"^# sitting: (.+)$", env, re.M)
        if m:
            runs[os.path.normcase(os.path.abspath(m.group(1).strip()))] = run
    proved = collections.defaultdict(set)  # (archive, function) -> sitting names
    strict, talked = set(), set()
    gained = {}  # sitting name -> items received (gifts_received)
    for sit in a.sittings:
        key = os.path.normcase(os.path.abspath(sit))
        con, rep = console_starts(sit)
        if con is None or key not in runs:
            print(f"  {os.path.basename(sit)}: {'no scripts replay' if con is None else 'no PC run given'}, skipped")
            continue
        pc = pc_starts(runs[key])
        # the order check: the console's starts paired with the PC's by their function order (the probe can miss a
        # start that ends inside one frame, so the PC may have more). A pair counts when its rooms agree: the same
        # room; the console's unset (0xFFFFFFFF in menu rooms and loads: the PC's room then); or the PC's next room
        # entry naming the console's room (a new room's first start, logged on the PC just before its "[slice]
        # entered"; 2026-11-02, the 16 sittings: mostly function 3, the room's setup). Others are counted, not used
        # (the review: a lone common function can pair across places that don't belong together)
        sm = difflib.SequenceMatcher(None, [c[1] for c in con], [p[1] for p in pc], autojunk=False)
        both, first, odd = set(), 0, 0
        for b in sm.get_matching_blocks():
            for k in range(b.size):
                (crid, fn, talk), (prid, _, nxt) = con[b.a + k], pc[b.b + k]
                if crid == prid or crid == 0xFFFFFFFF:
                    rid = prid
                elif crid == nxt:
                    rid, first = crid, first + 1
                else:
                    odd += 1
                    continue
                both.add((rid, fn))
                if talk:
                    talked.add((archive.get(rid), fn))
        same = sum(b.size for b in sm.get_matching_blocks())
        print(f"  {os.path.basename(sit)} ({rep}): console {len(con)} room-script starts, PC {len(pc)}; {same} in the "
              f"same order ({100 * same // max(1, len(con))}%; {first} a new room's first start, {odd} rooms disagree)")
        gained[os.path.basename(sit)] = gifts_received(sit, runs[key])
        for rid, fn in both:
            if rid in archive:
                proved[(archive[rid], fn)].add(os.path.basename(sit))
                if key not in steered:
                    strict.add((archive[rid], fn))
    # who uses each room function: people (talk scripts) and room events; a talk / event row is marked only when its
    # function is used by nothing else in the room
    users = collections.defaultdict(list)
    for r in csv.DictReader(open(os.path.join(TABLES, "npc_placements_decoded.csv"), encoding="utf-8")):
        if r["talk_script"].startswith("0x0100"):
            users[(r["room"], int(r["talk_script"][6:], 16))].append(f"talk:{r['room']}:{r['slot']}")
    for r in csv.DictReader(open(os.path.join(TABLES, "events_decoded.csv"), encoding="utf-8")):
        if r["script_id"] == "0x0100":
            users[(r["room"], int(r["function"]))].append(f"event:{r['room']}:k{r['kind']}:{r['event_id']}")
    with open(CSV_PATH, encoding="utf-8", newline="") as f:
        ids = [r["id"] for r in csv.DictReader(f)]
    # gift rows are not marked from a start alone: a gift function usually checks an "already received" flag first and
    # runs on every visit (the review, 2026-11-02). A gift row needs its function started in a sitting AND its item
    # received in that sitting (more in the console's end save than its start save, and as many in the PC's end save)
    for (room, fn), sits in proved.items():
        for i in ids:
            m = re.match(rf"gift:{re.escape(room)}:{fn}:item(\d+)$", i)
            if m and any(int(m.group(1)) in gained.get(s, ()) for s in sits) and want.get(i) != "matches Dolphin":
                got = sorted(s for s in sits if int(m.group(1)) in gained.get(s, ()))
                want[i] = "matches Dolphin" if (room, fn) in strict else "works"
                note[i] = (f"; gift function 0x0100{fn:04X} started on the console and the PC, and the item received "
                           f"(the end saves' bags, {', '.join(got)})")
    for (room, fn), sits in proved.items():
        st = "matches Dolphin" if (room, fn) in strict else "works"
        rows = [i for i in ids if i == f"script:{room}:{fn}"]
        what = "the function started"
        if len(users[(room, fn)]) == 1 and (room, fn) in talked:
            rows += users[(room, fn)]  # the one person or room event using it, a message window opening right after
            what = "the function started and a message window opened within 30 frames"
        for i in rows:
            if want.get(i) != "matches Dolphin":  # a stronger status from another rule stays
                want[i] = st
                note[i] = (f"; room script 0x0100{fn:04X}: {what} on the console (scripts probe), started on the PC "
                           f"in the same order ({', '.join(sorted(sits))})")
    return len(proved)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("disc")
    ap.add_argument("sittings", nargs="+")
    ap.add_argument("--evidence", required=True)
    ap.add_argument("--steered", nargs="*", default=[],
                    help="sittings whose replay needed logged resyncs: a battle seen only in these is 'works', not "
                         "'matches Dolphin' (the replay was steered through it; the review, 2026-11-01)")
    ap.add_argument("--pc-runs", nargs="*", default=[],
                    help="the PC's final run folders of these sittings (check.env '# sitting:' names the sitting): "
                         "turns on the room-script rows")
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()

    layout = list(flag_layout(table_rows(common_rel(Disc(a.disc)), 0x111e04, 0x14a6c0, 8)))
    steered = {os.path.normcase(os.path.abspath(s)) for s in a.steered}
    rooms, flags, moves = set(), set(), set()
    battles = collections.defaultdict(set)  # battle id -> sitting names
    strict_battles = set()
    for sit in a.sittings:
        seq, dumps = [], sorted(glob.glob(os.path.join(sit, "flags_entry_*.json")))
        for f in dumps:
            seq.append(int(ENTRY.search(f).group(1), 16))
        if dumps:
            # flags this sitting set: the last dump's minus the first's (the first holds what the start card had)
            flags |= set_flags(json.load(open(dumps[-1])), layout) - set_flags(json.load(open(dumps[0])), layout)
        rooms |= set(seq)
        moves |= set(zip(seq, seq[1:]))
        ev = os.path.join(sit, "events.jsonl")
        is_strict = os.path.normcase(os.path.abspath(sit)) not in steered
        if os.path.exists(ev):
            for line in open(ev, encoding="utf-8"):
                if '"ev": "battle"' in line:
                    b = json.loads(line)["id"]
                    battles[b].add(os.path.basename(sit))
                    if is_strict:
                        strict_battles.add(b)
    room_rows = list(csv.DictReader(open(os.path.join(TABLES, "rooms_decoded.csv"), encoding="utf-8")))
    archive = {int(r["room_id"]): r["archive"] for r in room_rows}
    dup = [n for n, k in collections.Counter(r["archive"] for r in room_rows).items() if k > 1]
    if dup:  # two room ids on one archive would merge into one row key (the review)
        sys.exit(f"rooms_decoded.csv: archives shared by several room ids: {dup}")
    items = {r["index"] for r in csv.DictReader(open(os.path.join(TABLES, "treasure_decoded.csv"), encoding="utf-8"))
             if int(r["flag"]) in flags}
    want, note = {}, {}
    for rid in rooms:
        if rid in archive:
            want[f"room:{archive[rid]}"] = want[f"menu:{archive[rid]}"] = "matches Dolphin"
    for b, sits in battles.items():
        if b in strict_battles:
            want[f"battle:{b}"] = "matches Dolphin"
        else:
            want[f"battle:{b}"] = "works"
            note[f"battle:{b}"] = f"; seen only in sittings that needed logged resyncs ({', '.join(sorted(sits))})"
    for i in items:
        want[f"item:{i}"] = "works"
    nscripts = room_scripts(a, archive, steered, want, note) if a.pc_runs else 0

    build = subprocess.run(["git", "rev-parse", "--short", "HEAD"], capture_output=True, text=True, cwd=REPO).stdout.strip()
    today = datetime.date.today().isoformat()
    with open(CSV_PATH, encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        cols, rows = reader.fieldnames, list(reader)
    walked = {(archive.get(x), archive.get(y)) for x, y in moves}
    exits = collections.defaultdict(list)
    for r in rows:
        m = re.search(r" to (\S+) \(entry", r["description"])
        if r["category"] == "room event" and r["group"] in ("exit", "lift") and m:
            exits[(r["id"].split(":")[1], m.group(1))].append(r["id"])
    for pair, ids in exits.items():
        if pair in walked and len(ids) == 1:
            want[ids[0]] = "matches Dolphin"
    changed = collections.Counter()
    mine = f"automatic: {a.evidence}"
    for r in rows:
        st = want.get(r["id"])
        ours = r["verified_by"].startswith(mine)  # a row this tool marked with the same evidence: recomputed
        if st and (r["status"] in ("", "not tested") or ours):
            new = dict(status=st, verified_by=mine + note.get(r["id"], ""))
            if any(r[k] != v for k, v in new.items()):
                r.update(new, last_build=build, date=today)
                changed[(r["category"], st)] += 1
        elif not st and ours:
            r.update(status="not tested", verified_by="", last_build="", date="")
            changed[(r["category"], "not tested")] += 1
    print(f"from {len(a.sittings)} sittings ({len(steered)} steered): {len(rooms)} room ids, {len(battles)} battles "
          f"({len(strict_battles)} in strict sittings), {len(items)} world items, {nscripts} room-script functions")
    for (cat, st), n in sorted(changed.items()):
        print(f"  {cat}: {n} -> {st}")
    if a.apply:
        with open(CSV_PATH, "w", encoding="utf-8", newline="") as f:
            w = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
            w.writeheader()
            w.writerows(rows)
        print(f"written: {CSV_PATH}")
    else:
        print("dry run (--apply writes the CSV)")


if __name__ == "__main__":
    main()
