"""PC replay vs Dolphin recording: where do they first differ? (testing lane; snatcherLeo's unattended-replays goal, the project notes
section 0 item 7; the project's request 2026-10-19).

    python -I tools/local/testing/replay_report.py <sitting folder> <PC run.log> [--dolphin-replay <replay_K folder>]
                                                   [--out report.md]

Inputs:
- the sitting folder (playrec.py): events.jsonl (room entries with the RNG state, battles and results, ...) and
  flags_entry_NNN_<room>.json (the four story-flag buffers at each room entry);
- the PC run's log, run with ORRE_SLICE_LOG=1 ("[slice] entered 0xR frame=F rng=0x.. flags=0x..") and ORRE_BATTLE_LOG=1
  ("[battle] start N", "[battle] end: ... result R ..."); with ORRE_RNG_TRACE=1 also the per-frame RNG;
- optional: a Dolphin replay folder of the same sitting with rng.csv (playrec.py replay N --probe rng) and
  replay_events.jsonl, for the per-frame RNG comparison.

Alignment, as playrec.py's check_aligned does: on the first room entry both sides share (same room and the same RNG
state; if no entry shares the RNG state, the first shared room, with a warning). Frames are then counted from that
entry on both sides.

Output: per sitting, the first difference (room order, the RNG at a room entry, the story flags at a room entry, a
battle's number or result), the frame drift at every entry (timing, reported separately: not a mismatch by itself),
the first frame whose RNG differs (when per-frame data exists on both sides), and a short summary. What isn't
compared because the PC doesn't log it yet (message windows, single flag bits, sound) is listed at the end.
"""
import argparse, csv, difflib, glob, json, os, re, zlib


def flags_crc(words):
    """CRC-32 of story-flag classes 0-3, each word big-endian (idlog.py; the PC prints the same)."""
    return zlib.crc32(b"".join(w.to_bytes(4, "big") for cls in words for w in cls)) & 0xFFFFFFFF


def recording_entries(sit):
    """The recording's room entries as the PC counts them (2026-10-29; the project's P09 stop at entry 68): load_dolphin's
    entries, plus the "reentered" loads (a battle room's return that reloads the field; idlog.py FLOOR_STATE). A
    recording made before that event existed can't have them, so they are taken from the newest replay folder that
    logged any, moved onto the recording's frames by the first room entry both share (same room and RNG state)."""
    de = load_dolphin(sit)[0]
    if not os.path.exists(os.path.join(sit, "events.jsonl")):
        return de
    if any(e["ev"] == "reentered" for e in map(json.loads, open(os.path.join(sit, "events.jsonl")))):
        return de
    reps = sorted((x for x in os.listdir(sit) if re.match(r"replay_\d+$", x)
                   and os.path.exists(os.path.join(sit, x, "replay_events.jsonl"))), key=lambda x: -int(x.split("_")[1]))
    for x in reps:
        ev = [json.loads(l) for l in open(os.path.join(sit, x, "replay_events.jsonl"), encoding="utf-8")]
        extra = [e for e in ev if e.get("ev") == "reentered"]
        if not extra:
            continue
        first = next((e for e in ev if e.get("ev") == "entered"), None)
        rec = next((d for d in de if first and d["room"] == first["room"] and d["rng"] == first["rng"]), None)
        if rec is None:
            continue
        off = rec["f"] - first["f"]
        add = [{"f": e["f"] + off, "room": e["room"], "rng": e["rng"], "crc": None, "from": x} for e in extra]
        return sorted(de + add, key=lambda e: e["f"])
    return de


def load_dolphin(sit):
    if not os.path.exists(os.path.join(sit, "events.jsonl")) and os.path.exists(os.path.join(sit, "ids.jsonl")):
        # a dolphin_slice.py run folder (the golden slice): ids.jsonl's "entered" lines carry the flag CRC already;
        # battles from battle/ref.jsonl's start event (no result logged there)
        ids = [json.loads(l) for l in open(os.path.join(sit, "ids.jsonl"))]
        entries = [{"f": e["frame"], "room": e["id"], "rng": e["rng"], "crc": e["flags_crc"]}
                   for e in ids if e.get("kind") == "entered"]
        battles = [{"f": e["frame"], "id": e["id"]} for e in ids if e.get("kind") == "battle"]
        return entries, battles, [], max((e["f"] for e in entries), default=0)
    ev = [json.loads(l) for l in open(os.path.join(sit, "events.jsonl"))]
    crc = {}
    for p in glob.glob(os.path.join(sit, "flags_entry_*.json")):
        n = int(os.path.basename(p).split("_")[2])
        crc[n] = flags_crc(json.load(open(p)))
    # "reentered" (2026-10-29): a room loaded again with no "entered" (a return from a battle room that reloads
    # the field), which the PC's hook counts as an entry; in captures from 2026-10-29 on (idlog.py FLOOR_STATE)
    entries = [{"f": e["f"], "room": e["room"], "rng": e["rng"],
                "crc": crc.get(e.get("n")) if e["ev"] == "entered" else None}
               for e in ev if e["ev"] in ("entered", "reentered")]
    battles = [{"f": e["f"], "id": e["id"]} for e in ev if e["ev"] == "battle"]
    results = [{"f": e["f"], "value": e["value"]} for e in ev if e["ev"] == "battle_result"]
    last = max((e["f"] for e in ev), default=0)
    MSG.clear()
    FLAG.clear()
    MSG["d"] = [(e["f"], (e["type"], e["error"])) for e in ev if e["ev"] == "msgwin"]
    FLAG["d"] = [(e["f"], (e["cls"], e["word"], e["bit"], e["now"])) for e in ev if e["ev"] == "flag"]
    return entries, battles, results, last


# message windows and story-flag bits, per side: lists of (frame, value); "d" = the recording, "p" = the PC
MSG, FLAG = {}, {}
EVMSG = re.compile(r"\[ev\] frame=(\d+) msgwin type=(-?\d+) error=(-?\d+)")
EVFLAG = re.compile(r"\[ev\] frame=(\d+) flag cls=(\d+) word=(\d+) bit=(\d+) now=(\d+)")
FLAG_TOL = 30   # dolphin_playrec.py polls the flags every 30 frames: their recorded frame is only that exact


ENTER = re.compile(r"\[slice\] entered 0x([0-9A-Fa-f]+) frame=(\d+) rng=0x([0-9A-Fa-f]+) flags=0x([0-9A-Fa-f]+)")
FRAME = re.compile(r"\[(?:rngtrace\] frame|pad_script\] frame|vi\] game frame) (\d+)")
BSTART = re.compile(r"\[battle\] start (\d+)")
BEND = re.compile(r"\[battle\] end:.*\bresult (\d+)")
DRAW = re.compile(r"\[rngtrace\] (\d+) (\S+) ([0-9A-Fa-f]{8})")


def load_pc(log):
    entries, battles, results, rng_frames = [], [], [], {}
    cur, last_state, traced = 0, None, False
    MSG["p"], FLAG["p"] = [], []
    for line in open(log, encoding="utf-8", errors="replace"):
        if line.startswith("[ev]"):
            m = EVMSG.search(line)
            if m:
                MSG["p"].append((int(m.group(1)), (int(m.group(2)), int(m.group(3)))))
                continue
            m = EVFLAG.search(line)
            if m:
                FLAG["p"].append((int(m.group(1)), tuple(int(m.group(k)) for k in range(2, 6))))
                continue
        m = ENTER.search(line)
        if m:
            entries.append({"f": int(m.group(2)), "room": int(m.group(1), 16), "rng": int(m.group(3), 16),
                            "crc": int(m.group(4), 16)})
            continue
        m = FRAME.search(line)
        if m:
            if line.startswith("[rngtrace] frame"):
                traced = True
                if last_state is not None:
                    rng_frames[cur] = last_state       # the state after the previous frame's last draw
            cur = int(m.group(1))
            continue
        m = DRAW.match(line)
        if m:
            last_state = int(m.group(3), 16)
            continue
        m = BSTART.search(line)
        if m:
            # two PC sites print the start (fight_candidate_8020DAD0.c and gs_range_801C766C.c; most battles pass both):
            # one battle unless an end came between
            if not (battles and battles[-1]["id"] == int(m.group(1))
                    and not any(r["f"] >= battles[-1]["f"] for r in results)):
                battles.append({"f": cur, "id": int(m.group(1))})
            continue
        m = BEND.search(line)
        if m:
            results.append({"f": cur, "value": int(m.group(1))})
    return entries, battles, results, (rng_frames if traced else None)


def load_dolphin_rng(rep):
    """rng.csv (frame, rng, room) of a Dolphin replay plus its replay_events.jsonl room entries."""
    rows = {}
    for line in list(open(os.path.join(rep, "rng.csv")))[1:]:
        f, r, _ = line.strip().split(",")
        rows[int(f)] = int(r)
    ev = [json.loads(l) for l in open(os.path.join(rep, "replay_events.jsonl"))]
    return rows, [{"f": e["f"], "room": e["room"], "rng": e["rng"]} for e in ev
                  if e["ev"] in ("entered", "reentered")]


def place(events, ents, stop):
    """(frame, value) events from the first room entry up to `stop`, as (frame, room index k, frames since entry k,
    value). `ents` = that side's paired room-entry frames."""
    out = []
    for f, v in events:
        if f < ents[0] or f >= stop:
            continue
        k = max(j for j, e in enumerate(ents) if e <= f)
        out.append((f, k, f - ents[k], v))
    return out


def on_rec_time(ev, dents):
    """A PC event's time on the recording's timeline: the recording's entry frame of the same room + the PC's frames
    since that entry (so room-entry drift doesn't count against the events after it)."""
    return dents[ev[1]] + ev[2]


def where(ev):
    return f"room entry {ev[1] + 1} +{ev[2]}"


def compare_msgwin(dl, pl, dents, tol=2):
    """Message windows in order over the whole compared stretch (a window near a room change may fall on either side
    of it, so they are not split by room). Returns (recording frame, text) of the first problem, or None."""
    for i in range(max(len(dl), len(pl))):
        if i >= len(pl):
            return dl[i][0], f"window {i + 1} (type {dl[i][3][0]}, recording {where(dl[i])}) is missing on the PC"
        if i >= len(dl):
            return on_rec_time(pl[i], dents), f"the PC has an extra window {i + 1} (type {pl[i][3][0]}, PC {where(pl[i])})"
        d, p = dl[i], pl[i]
        if d[3] != p[3]:
            return d[0], f"window {i + 1}: type / error {p[3]} on the PC vs {d[3]} (recording {where(d)}, PC {where(p)})"
        delta = on_rec_time(p, dents) - d[0]
        if abs(delta) > tol:
            return d[0], f"window {i + 1} (type {d[3][0]}) comes {'late' if delta > 0 else 'early'} on the PC: " \
                         f"PC {where(p)}, recording {where(d)} ({delta:+d} frames)"
    return None


def compare_flags(dl, pl, dents):
    """Story-flag bit changes as a set (the recording polls every FLAG_TOL frames, so the order inside one poll is
    lost): each recorded change matched to the PC's same change nearest in time; the PC's change must come at most
    FLAG_TOL frames before the poll that saw it (2 frames' slack). Returns (recording frame, text) of the earliest
    problem, or None."""
    left = list(pl)
    probs = []
    for d in dl:
        cand = [j for j, p in enumerate(left) if p[3] == d[3]]
        name = "flag cls {} word {} bit {} -> {}".format(*d[3])
        if not cand:
            probs.append((d[0], f"{name} (recording {where(d)}) is missing on the PC"))
            continue
        j = min(cand, key=lambda j: abs(on_rec_time(left[j], dents) - d[0]))
        p = left.pop(j)
        delta = on_rec_time(p, dents) - d[0]
        if delta > 2 or delta < -FLAG_TOL - 2:
            probs.append((d[0], f"{name} comes {'late' if delta > 0 else 'early'} on the PC: PC {where(p)}, "
                                f"recording poll {where(d)}"))
    for p in left:
        probs.append((on_rec_time(p, dents), "the PC has an extra flag change cls {} word {} bit {} -> {}".format(*p[3])
                      + f" at PC {where(p)}"))
    return min(probs) if probs else None


def load_frames(path):
    """A frame-times file (frames_to_pc.py): [(room, [(count, retraces), ...])] per line, line 0 = boot."""
    lines = []
    for raw in open(path, encoding="utf-8"):
        parts = raw.split()
        if not parts or parts[0].startswith("#"):
            continue
        runs = [tuple(int(x) for x in r.split("*")) for r in parts[1:]]
        lines.append((int(parts[0], 16), runs))
    return lines


def frame_len(runs, t):
    """The console's game-frame length (retraces) at retrace offset t into a line; the last run's past its end."""
    pos = 0
    for count, n in runs:
        pos += count * n
        if t < pos:
            return n
    return runs[-1][1] if runs else 1


def frames_shift(lines, de, di):
    """Which frame-times line holds recording entry K: line K + shift. The file starts at the replay's first entry
    (line 1); found by matching the room sequence from the aligned entry on."""
    rooms = [r for r, _ in lines]
    for s in range(-len(de), len(lines)):
        ks = range(di, min(len(de), di + 6))
        if all(0 <= k + s < len(rooms) and rooms[k + s] == de[k]["room"] for k in ks):
            return s
    return None


def pair_windows(dm, pm):
    """Windows paired room by room on their type sequence (so a window that one side lacks, as in a battle room whose
    turns went differently, doesn't shift every later pair). Returns [(d, p)] in recording order."""
    import difflib
    pairs = []
    for k in sorted({e[1] for e in dm} | {e[1] for e in pm}):
        dk = [e for e in dm if e[1] == k]
        pk = [e for e in pm if e[1] == k]
        sm = difflib.SequenceMatcher(None, [e[3] for e in dk], [e[3] for e in pk], autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == "equal":
                pairs += list(zip(dk[i1:i2], pk[j1:j2]))
    return pairs


def frames_lines(a):
    """The run's frame-times file: --frames, else sitting.frames next to the PC log (replay_check.py writes it there)."""
    path = a.frames or os.path.join(os.path.dirname(os.path.abspath(a.pclog)), "sitting.frames")
    return load_frames(path) if os.path.exists(path) else None


LATE_FADE = "fade set before the retrace task (long frame, tolerance rule)"


def late_fades(sit, de):
    """The console's fades set after the fade daemon's step in their refresh (2026-10-26; the project's choice (ii),
    gamecube-accuracy.md "Logic passes at fade-ins"): the fade probe's first row of the fade reads elapsed 0. The PC
    sets the same fades before its step, one refresh sooner each (P02's world map: two per visit). Recording frames of
    those starts, from the newest replay with fade.csv, placed by its room entries against the recording's"""
    reps = sorted((x for x in os.listdir(sit) if re.match(r"replay_\d+$", x)
                   and os.path.exists(os.path.join(sit, x, "fade.csv"))
                   and os.path.exists(os.path.join(sit, x, "replay_events.jsonl"))), key=lambda x: int(x.split("_")[1]))
    if not reps:
        return []
    rep = os.path.join(sit, reps[-1])
    rents = [(e["f"], e["room"]) for e in map(json.loads, open(os.path.join(rep, "replay_events.jsonl"),
                                                                 encoding="utf-8"))
             if e.get("ev") in ("entered", "reentered")]
    # pair by the room sequence: the replay's list can lack the recording's first (boot) entries (P02: one short, and
    # pairing by position put every late fade one room off)
    best = max(range(-4, 5), key=lambda s: sum(1 for i, (_, r) in enumerate(rents)
                                               if 0 <= i + s < len(de) and de[i + s]["room"] == r))
    offs = [de[i + best]["f"] - f for i, (f, r) in enumerate(rents) if 0 <= i + best < len(de) and de[i + best]["room"] == r]
    off = sorted(offs)[len(offs) // 2] if offs else 0
    out, prev = [], 0
    for r in csv.DictReader(open(os.path.join(rep, "fade.csv"))):
        if not r["frame"][:1].isdigit():
            continue
        p = int(r["pending"])
        if p == 1 and prev != 1 and float(r["elapsed"]) == 0.0:
            out.append(int(r["frame"]) + off)
        prev = p
    return out


def gap_section(say, rows, paired, dm, pm, dents, pents, d0, p0, de, di, lines, late=()):
    """snatcherLeo's tolerance rule (2026-10-26, docs/port/gamecube-accuracy.md "Comparing with Dolphin"): event times are
    compared as the gap from the previous matched event, not from the room entry (the two sides mark an entry a few
    refreshes apart), and a gap change bigger than one console game frame there (1 retrace at 60 fps, 2 at 30 fps; from
    the run's frame-times file) is "open" and goes in the sitting's ledger. Times are on the aligned timeline:
    (the room entry's frame from the alignment) + (the frames since that entry), on each side."""
    shift = frames_shift(lines, de, di) if lines else None

    def flen(k, t):
        if shift is None or not (0 <= di + k + shift < len(lines)):
            return 1
        return frame_len(lines[di + k + shift][1], t)

    say("\n## Gaps (tolerance rule)\n")
    say("Each event's time on the aligned timeline on both sides, PC minus console, and the change from the previous "
        "matched event (the gap change). **open** = a gap change bigger than one console game frame at that moment "
        f"({'from the run' + chr(39) + 's frame-times file' if shift is not None else 'no frame-times file found: 1 retrace assumed'}). "
        "Windows are paired room by room on their type sequence, so a window one side lacks doesn't shift the rest.\n")
    say("*Room-entry rows (2026-10-26, verified on P01's 15 room changes against the game's own fades):* the "
        "PC's room hook marks an entry 2 refreshes earlier than the recorder's \"entered\" event, so an entry's drift "
        "change mixes that constant marking offset with real time; between game events (fade-out end to the next fade "
        "change) the PC is 0 or +/-1 refresh off, mostly -1 (real; cause open). Read an entry row's **open** as \"look "
        "at the game events around this room change\" (the fades, or the room's first window below), not as a "
        "difference by itself.\n")
    if late:
        say(f"*Late-set fades (the project's choice (ii), 2026-10-26; gamecube-accuracy.md \"Logic passes at fade-ins\"):* "
            f"{len(late)} of the console's fades were set after the fade daemon's step in their refresh (a long frame), "
            "which the PC does one refresh sooner. A PC-earlier gap change of up to one refresh per such fade inside "
            f"the gap, beyond one game frame, reads \"{LATE_FADE}\" instead of **open**.\n")
    late_sorted = sorted(late)

    def verdict(ch, n, t0, t1):
        """'' / '**open**' / the late-fade label for a gap change ch over console time (t0, t1]"""
        if ch is None or abs(ch) <= n:
            return ""
        if ch < 0 and t0 is not None:
            cnt = sum(1 for f in late_sorted if t0 < f <= t1)
            if cnt and -ch <= n + cnt:
                return LATE_FADE
        return "**open**"

    say("| # | room | entry (rec / PC, from alignment) | drift | change | game frame | |")
    say("|---|---|---|---|---|---|---|")
    open_rooms = {}
    prev, prev_t = None, None
    for k, d, p, drift, _ in rows:
        n = flen(k, 0)
        ch = None if prev is None else drift - prev
        v = verdict(ch, n, prev_t, d["f"])
        if v == "**open**":
            open_rooms[k] = open_rooms.get(k, 0) + 1
        say(f"| {k + 1} | {d['room']:#x} | {d['f'] - d0['f']} / {p['f'] - p0['f']} | {drift:+d} | "
            f"{'' if ch is None else f'{ch:+d}'} | {n} | {v} |")
        prev, prev_t = drift, d["f"]
    say("\n| window | room | type | console (aligned) | PC (aligned) | PC minus console | gap change | game frame | |")
    say("|---|---|---|---|---|---|---|---|---|")
    prev, prev_t = None, None
    for i, (d, p) in enumerate(pair_windows(dm, pm)):
        k = d[1]
        dal = d[0] - d0["f"]
        pal = (pents[p[1]] - p0["f"]) + p[2]
        delta = pal - dal
        n = flen(k, d[2])
        ch = None if prev is None else delta - prev
        v = verdict(ch, n, prev_t, d[0])
        if v == "**open**":
            open_rooms[k] = open_rooms.get(k, 0) + 1
        say(f"| {i + 1} | {paired[k][1]['room']:#x} | {d[3][0]} | {dal} | {pal} | {delta:+d} | "
            f"{'' if ch is None else f'{ch:+d}'} | {n} | {v} |")
        prev, prev_t = delta, d[0]
    unpaired = len(dm) + len(pm) - 2 * len(pair_windows(dm, pm))
    total = sum(open_rooms.values())
    per = ", ".join(f"{paired[k][1]['room']:#x} (entry {k + 1}): {c}" for k, c in sorted(open_rooms.items())
                    if k < len(paired)) or "none"
    say(f"\nOpen gaps: {total} ({per}); windows left unpaired (one side only): {unpaired}.")
    GAPS["open"], GAPS["per"] = total, per


GAPS = {}


def compare_bmsg(sit, pclog, off, dlast, say):
    """Battle messages as id sequences: the recording's battle_msg events against the PC's "[bmsg] frame,retrace,id"
    lines (ORRE_BATTLE_LOG=1). Suggested by Main (2026-11-01, port-gaps 48): its scratch pass over Run 2's 36 final runs
    found Lightning Rod never redirecting on the PC, which the room and story checks missed. Returns the first
    difference as text, or None. PC messages past the recording's end (by the aligned frame) are left out."""
    ev = os.path.join(sit, "events.jsonl")
    con = []
    if os.path.exists(ev):
        for line in open(ev, encoding="utf-8"):
            if '"battle_msg"' in line:
                e = json.loads(line)
                con.append((e["f"], e["id"]))
    pc = []
    for line in open(pclog, encoding="utf-8", errors="replace"):
        if line.startswith("[bmsg] "):
            p = line[7:].strip().split(",")
            if len(p) >= 3 and p[0].isdigit() and int(p[0]) + off <= dlast:
                pc.append((int(p[0]), int(p[2])))
    say("\n## Battle messages\n")
    if not pc:
        say("Not compared: the PC log has no `[bmsg]` lines (run it with ORRE_BATTLE_LOG=1)." if con else
            "Neither side has battle messages.")
        return None
    a, b = [x[1] for x in con], [x[1] for x in pc]
    if a == b:
        say(f"All {len(a)} battle messages identical, in order.")
        return None
    ops = [o for o in difflib.SequenceMatcher(None, a, b, autojunk=False).get_opcodes() if o[0] != "equal"]
    say(f"Recording {len(a)}, PC {len(b)}; {len(ops)} differing run(s) (ids; frames are each side's own, the PC's "
        f"{off:+d} to put it on the recording's):\n")
    for tag, i1, i2, j1, j2 in ops[:12]:
        cf = con[i1][0] if i1 < len(con) else "end"
        pf = pc[j1][0] if j1 < len(pc) else "end"
        say(f"- {tag} at recording frame {cf} / PC frame {pf}: recording {a[i1:i2][:8]}, PC {b[j1:j2][:8]}")
    if len(ops) > 12:
        say(f"- ... {len(ops) - 12} more")
    tag, i1, i2, j1, j2 = ops[0]
    return (f"battle message {i1 + 1} (recording frame {con[i1][0] if i1 < len(con) else 'end'}): recording "
            f"{a[i1:i2][:4]}, PC {b[j1:j2][:4]}")


def align(d_entries, p_entries):
    for p in p_entries:
        for d in d_entries:
            if d["room"] == p["room"] and d["rng"] == p["rng"]:
                return d, p, ""
    for p in p_entries:
        for d in d_entries:
            if d["room"] == p["room"]:
                return d, p, " (no entry shares the RNG state: aligned on the first shared room only)"
    return None, None, ""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sitting")
    ap.add_argument("pclog")
    ap.add_argument("--dolphin-replay")
    ap.add_argument("--out")
    ap.add_argument("--frames", help="the run's frame-times file (default: sitting.frames next to the PC log)")
    a = ap.parse_args()
    out = []
    say = out.append
    de, db, dr, dlast = load_dolphin(a.sitting)
    de = recording_entries(a.sitting) if os.path.exists(os.path.join(a.sitting, "events.jsonl")) else de
    pe, pb, pr, prng = load_pc(a.pclog)
    say(f"# Replay report: {os.path.basename(os.path.normpath(a.sitting))}")
    say(f"Dolphin recording: {len(de)} room entries, {len(db)} battles; PC log: {len(pe)} room entries, "
        f"{len(pb)} battles.")
    d0, p0, note = align(de, pe)
    if d0 is None:
        say("**No shared room entry: can't align.** PC rooms: " + ", ".join(hex(e["room"]) for e in pe[:12]))
        print("\n".join(out))
        return
    off = d0["f"] - p0["f"]
    say(f"Aligned on room {d0['room']:#x}: recording frame {d0['f']}, PC frame {p0['f']}{note}. Frames below count "
        f"from that entry.")
    di = de.index(d0)
    pi = pe.index(p0)
    first = None
    rows = []
    for k, (d, p) in enumerate(zip(de[di:], pe[pi:])):
        drift = (p["f"] + off) - d["f"]
        what = []
        if d["room"] != p["room"]:
            what.append(f"room {p['room']:#x} instead of {d['room']:#x}")
        if d["rng"] != p["rng"]:
            what.append(f"RNG {p['rng']:#010x} instead of {d['rng']:#010x}")
        if d["crc"] is not None and d["crc"] != p["crc"]:
            what.append(f"story flags differ (CRC {p['crc']:#010x} vs {d['crc']:#010x})")
        rows.append((k, d, p, drift, what))
        if what and first is None:
            first = (f"room entry {k + 1} (recording frame {d['f']}, +{d['f'] - d0['f']} from the alignment): "
                     + "; ".join(what))
        if d["room"] != p["room"]:
            break
    say("\n## Room entries\n")
    say("| # | room (rec / PC) | frame from start (rec / PC) | drift | RNG | flags |")
    say("|---|---|---|---|---|---|")
    for k, d, p, drift, what in rows:
        say(f"| {k + 1} | {d['room']:#x} / {p['room']:#x} | {d['f'] - d0['f']} / {p['f'] - p0['f']} | {drift:+d} | "
            f"{'same' if d['rng'] == p['rng'] else 'differs'} | "
            f"{'n/a' if d['crc'] is None else ('same' if d['crc'] == p['crc'] else 'differs')} |")
    if len(de) - di != len(pe) - pi:
        say(f"\nEntries after the alignment: recording {len(de) - di}, PC {len(pe) - pi}.")
    say("\n## Battles\n")
    def result_of(bl, rl, k):
        # the battle's result: the first one logged after its start and before the next battle's (the recorder also
        # sees the result word cleared to 0 as a battle sets up, 2026-10-21: pairing by index put those 0s
        # against the PC's results and every battle after the first read "differs")
        lo = bl[k]["f"]
        hi = bl[k + 1]["f"] if k + 1 < len(bl) else 1 << 40
        got = [r["value"] for r in rl if lo <= r["f"] < hi and r["value"] != 0]
        return got[0] if got else None

    for k in range(max(len(db), len(pb))):
        d = db[k] if k < len(db) else None
        p = pb[k] if k < len(pb) else None
        dres = result_of(db, dr, k) if d else None
        pres = result_of(pb, pr, k) if p else None
        # a result missing on one side (not logged there) is unknown, not a difference
        ok = d and p and d["id"] == p["id"] and (dres is None or pres is None or dres == pres)
        say(f"- battle {k + 1}: recording {d and d['id']} (result {dres}), PC {p and p['id']} (result {pres})"
            f"{'' if ok else '  **differs**'}")
        if not ok and first is None:
            first = f"battle {k + 1}: recording {d and d['id']} / result {dres}, PC {p and p['id']} / result {pres}"
    bprob = compare_bmsg(a.sitting, a.pclog, off, dlast, say)
    if bprob and first is None:
        first = bprob
    if MSG.get("p") or FLAG.get("p"):
        say("\n## Message windows and story-flag bits, per room\n")
        say("Times are \"room entry N +frames since that entry\" on each side, so drift at room entries doesn't count "
            "against the events after it. Windows are compared in order over the whole stretch (a window near a room "
            "change may fall on either side of it); flag changes as a set, the recording seeing each up to "
            f"{FLAG_TOL} frames late (it polls).\n")
        paired = [r for r in rows if r[1]["room"] == r[2]["room"]]
        dents = [r[1]["f"] for r in paired]
        pents = [r[2]["f"] for r in paired]
        n = len(paired)
        dstop = de[di + n]["f"] if di + n < len(de) else dlast + 1
        pstop = pe[pi + n]["f"] if pi + n < len(pe) else 1 << 60
        dm, dfl = place(MSG.get("d", []), dents, dstop), place(FLAG.get("d", []), dents, dstop)
        # the PC side stops where the recording ends (the PC run may go on past it)
        pm = [e for e in place(MSG["p"], pents, pstop) if on_rec_time(e, dents) < dstop]
        pfl = [e for e in place(FLAG["p"], pents, pstop) if on_rec_time(e, dents) < dstop]
        say("| # | room | windows (rec / PC) | flag changes (rec / PC) |")
        say("|---|---|---|---|")
        for k, r in enumerate(paired):
            cnt = lambda l: sum(1 for e in l if e[1] == k)
            say(f"| {k + 1} | {r[1]['room']:#x} | {cnt(dm)} / {cnt(pm)} | {cnt(dfl)} / {cnt(pfl)} |")
        # every window side by side, to see where frames are gained or lost (until the first mismatch in kind)
        say("\n| window | type | recording | PC | PC minus recording |")
        say("|---|---|---|---|---|")
        for i, (d, p) in enumerate(zip(dm, pm)):
            if d[3] != p[3]:
                break
            say(f"| {i + 1} | {d[3][0]} | {where(d)} | {where(p)} | {on_rec_time(p, dents) - d[0]:+d} |")
        gap_section(say, rows, paired, dm, pm, dents, pents, d0, p0, de, di, frames_lines(a),
                    late_fades(a.sitting, de))
        mprob = compare_msgwin(dm, pm, dents)
        fprob = compare_flags(dfl, pfl, dents)
        say(f"\n- Message windows: {mprob[1] if mprob else 'all match'}.")
        say(f"- Story flags: {fprob[1] if fprob else 'all match'}.")
        evprob = min(x for x in (mprob, fprob) if x) if (mprob or fprob) else None
        if evprob:
            # earlier than the first room-entry difference? (that one is checked at the entry's frame)
            entry_f = next((r[1]["f"] for r in rows if r[4]), None)
            if first is None or (entry_f is not None and evprob[0] < entry_f):
                first = evprob[1]
    if a.dolphin_replay and prng:
        say("\n## RNG per frame\n")
        drows, rentries = load_dolphin_rng(a.dolphin_replay)
        r0 = next((e for e in rentries if e["room"] == p0["room"] and e["rng"] == p0["rng"]), None)
        if r0 is None:
            say("The Dolphin replay has no entry matching the alignment entry; skipped.")
        else:
            roff = r0["f"] - p0["f"]
            diff = None
            for pf in sorted(f for f in prng if f >= p0["f"]):
                df = pf + roff
                if df in drows and drows[df] != prng[pf]:
                    diff = (pf, df, prng[pf], drows[df])
                    break
            if diff:
                say(f"First frame whose RNG state differs: PC frame {diff[0]} (+{diff[0] - p0['f']} from the "
                    f"alignment), Dolphin replay frame {diff[1]}: PC {diff[2]:#010x}, Dolphin {diff[3]:#010x}. "
                    f"(Compare callers with rng_frames_compare.py from there.)")
                if first is None:
                    first = f"RNG at +{diff[0] - p0['f']} frames from the alignment"
            else:
                say("The RNG state matches on every frame both sides logged.")
    say("\n## Summary\n")
    drifts = [r[3] for r in rows]
    say(f"- First difference: {first or 'none in the compared events'}.")
    if drifts:
        say(f"- Timing drift at room entries: {min(drifts):+d} to {max(drifts):+d} frames (PC minus recording, after "
            f"alignment); not a mismatch by itself.")
    if "open" in GAPS:
        say(f"- Open gaps (tolerance rule, the ledger's list): {GAPS['open']} ({GAPS['per']}).")
    if MSG.get("p") or FLAG.get("p"):
        say("- Not compared here (the sound tools compare them): sound slots; not at all yet: per-move HP changes.")
    else:
        say("- Not compared: message windows and single story-flag bits (run the PC with ORRE_EVENT_LOG=1 to add "
            "them), sound slots, per-move HP changes.")
    text = "\n".join(out)
    print(text)
    if a.out:
        open(a.out, "w", encoding="utf-8").write(text + "\n")


if __name__ == "__main__":
    main()
