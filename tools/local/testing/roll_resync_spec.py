"""Write ORRE_DEBUG_AT lines that put the console's random state and blink timers in place just before every battle
roll of one battle (2026-10-27; the second pass's battle-roll resync).

    python -I tools/local/testing/roll_resync_spec.py <replay> --shift N [--from L] [--to L] > <out>.at
    run with ORRE_DEBUG_AT=@file:<out>.at (port/src/pc_debug.c)

Why (match ledger, P08, battle 101): between the battle-menu resyncs the owners' idle-blink checks
(`wazaSequenceOwnerUpdateBlink`, one draw per idle owner whose blink timer is >= 10) drift by a draw inside one 30 fps
frame; one blink then lands elsewhere, the per-frame draw count parts, and the next accuracy or AI roll takes another
number (Gengar's Hypnosis: draw 270 on the PC, 281 on the console). Setting the console's state and timers right before
the roll's frame gives the roll the console's number; it worked by hand on that roll.

Inputs from one Dolphin replay folder: `rngcall.csv` (`PLAYREC_RNGCALL=lo-hi playrec.py replay N --probe rngcall`;
every draw with its new state and three return addresses) and `owners.csv` (`PLAYREC_OWNERS=room,frames,visit`, the
blink timers per label) over the battle. A frame is a roll frame when one of its draws comes from battle logic (a
caller named fight*, ws*, _fight*, pokemon*, kaisuu*); the line pair goes to PC frame (roll label + shift): the state
after the last draw of an earlier label, and the timers of the label before the roll's. `--shift`: PC frame minus
console label, one frame early (P08 battle 101: 620, from its hand-tested roll at label 10295 -> PC frame 10915).
"""
import argparse
import bisect
import csv
import os
import re

LOGIC = re.compile(r"^(fight|ws|_fight|pokemon|kaisuu|_ws)")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("replay")
    ap.add_argument("--shift", type=int, default=0)
    ap.add_argument("--draws", type=int, metavar="BATTLE",
                    help="instead: the per-draw file for ORRE_ROLL_SEEDS, as battle BATTLE (the Nth battle since "
                         "power-on): the console's state before every draw its battle thread made in the window")
    ap.add_argument("--all-battles", action="store_true",
                    help="the per-draw file for every battle in the replay's replay_events.jsonl, battle 1 = the "
                         "first start message (30317) there (capture them with PLAYREC_RNGCALL=a-b,c-d,... one "
                         "window per battle, each from its start to its end message; a short one is warned about)")
    ap.add_argument("--from", dest="lo", type=int, default=0)
    ap.add_argument("--to", dest="hi", type=int, default=10 ** 9)
    ap.add_argument("--symbols", default=os.path.join(os.path.dirname(__file__), "..", "..", "..", "config", "GC6E01",
                                                      "symbols.txt"))
    a = ap.parse_args()
    syms = []
    for line in open(a.symbols, encoding="utf-8", errors="replace"):
        m = re.match(r"(\S+) = \.text:0x([0-9A-Fa-f]+);", line)
        if m:
            syms.append((int(m.group(2), 16), m.group(1)))
    syms.sort()
    keys = [k for k, _ in syms]

    def name(addr):
        if not addr:
            return ""
        i = bisect.bisect_right(keys, int(addr, 16)) - 1
        return syms[i][1] if i >= 0 else ""

    draws = []
    # Labels where a run of battle-logic draws starts. A thread's run can cross a label (P08 battle 101: the trainer
    # AI draws for both foes at 10215 and goes on into 10216 with no switch between), so a label whose logic draws
    # only continue the run before it gets no line of its own: its seed would overwrite the state the PC's AI is
    # still drawing from (the PC runs the whole pick in one frame).
    starts = set()
    prev_logic = False
    rows = []   # every draw in order: (label, new state, thread, logic?)
    with open(os.path.join(a.replay, "rngcall.csv"), newline="") as f:
        for r in csv.reader(f):
            if not r or not r[0].isdigit():
                continue
            if r[1] == "switch":
                prev_logic = False
                continue
            callers = [name(x) for x in r[3:6]]
            logic = any(LOGIC.match(c) for c in callers)
            if logic and not prev_logic:
                starts.add(int(r[0]))
            prev_logic = logic
            draws.append((int(r[0]), r[1], callers))
            rows.append((int(r[0]), r[1], r[7] if len(r) > 7 else "", logic,
                         [int(x, 16) for x in r[3:6] if x]))
    if a.all_battles:
        import json
        import sys
        # Battles keyed by their start message (battle_msg 30317), not the "battle" event (2026-11-01, the project's
        # port-gaps 49): in P09 the console ran the battles at 200,473 and 218,656 (30317 frames) back to back with
        # one "battle" event and one end message (30316) for both, so the event list had 15 battles for 16 and the
        # PC's 16th went unseeded. The message comes ~28 frames after the event, so the window opens 90 frames before
        # it (the event's 60 plus that). Replays with no battle messages fall back to the events.
        starts, ev_starts, ends = [], [], []
        for line in open(os.path.join(a.replay, "replay_events.jsonl"), encoding="utf-8"):
            e = json.loads(line)
            if e.get("ev") == "battle":
                ev_starts.append(e["f"])
            elif e.get("ev") == "battle_msg" and e.get("id") == 30317:
                starts.append(e["f"])
            elif e.get("ev") == "battle_msg" and e.get("id") == 30316:
                ends.append(e["f"])
        lead = 90
        if not starts:
            starts, lead = ev_starts, 60
            print("# no battle start messages (30317) in the replay: battles from its 'battle' events",
                  file=sys.stderr)
        elif len(starts) != len(ev_starts):
            print(f"# {len(starts)} battle start messages, {len(ev_starts)} 'battle' events: keyed by the messages",
                  file=sys.stderr)
        labels = sorted(r[0] for r in rows)
        for k, f0 in enumerate(starts):
            # ends before the next battle's window opens (2026-11-02: ending at the next start - 1 overlapped the next
            # window's lead, so a battle's opening draws also landed at the end of the one before)
            hi = starts[k + 1] - lead - 1 if k + 1 < len(starts) else 10 ** 9
            # the capture window must span the battle, from its start message to its end message (port-gaps 49:
            # P17b, P18, P15, P24 and P23 had windows shorter than the battle); a back-to-back battle ends at the next
            # start or at the shared end message
            end = next((f for f in ends if f0 < f <= hi), None)
            i, j = bisect.bisect_left(labels, f0 - lead), bisect.bisect_right(labels, hi) - 1
            got = labels[i:j + 1]
            if not got:
                print(f"# battle {k + 1} (start {f0}): no draws captured in labels {f0 - lead}-{hi}", file=sys.stderr)
            elif got[0] > f0 + 30 or (end is not None and got[-1] < end - 30):
                print(f"# battle {k + 1} (start {f0}, end {end}): the capture covers labels {got[0]}-{got[-1]} only; "
                      f"recapture with a window of {f0 - lead}-{end if end is not None else hi}", file=sys.stderr)
            per_battle(a.replay, rows, draws, k + 1, f0 - lead, hi)
        return
    if a.draws is not None:
        per_battle(a.replay, rows, draws, a.draws, a.lo, a.hi)
        return
    timers = {}
    with open(os.path.join(a.replay, "owners.csv"), newline="") as f:
        for r in csv.DictReader(f):
            if r["blink_texanim"] not in ("", "-1"):
                timers.setdefault(int(r["frame"]), {})[int(r["slot"])] = int(r["blink_timer"])
    roll_labels = sorted(lab for lab in starts if a.lo <= lab <= a.hi)
    out = 0
    for lab in roll_labels:
        before = [d for d in draws if d[0] < lab]
        if not before or (lab - 1) not in timers:
            continue
        state = before[-1][1]
        t = timers[lab - 1]
        frame = lab + a.shift
        callers = sorted({c for l2, _, cs in draws if l2 == lab for c in cs if LOGIC.match(c)})
        print(f"{frame} seed 0x{state.upper()}  # console label {lab}: {', '.join(callers)}")
        print(f"{frame} blink " + " ".join(f"{s}:{v}" for s, v in sorted(t.items())))
        out += 1
    import sys
    print(f"# {out} roll frames", file=sys.stderr)


def per_battle(replay, rows, draws, battle, lo, hi):
    """Print one battle's "battle N" and "idle N" lists for ORRE_ROLL_SEEDS (console labels lo-hi)."""
    import collections
    import sys
    # The battle thread: the thread its logic draws run on (P08: 8049ebac; the main logic thread, 8049eb40, makes
    # the blinks, particles and camera draws), picked by the move-script and AI callers only: the trainer Pokémon are
    # made on the main thread before the battle thread starts (P07: 114 draws in pokemonCreateRndFit, more than the
    # battle thread's own). Each of its draws gets the state the draw before it left.
    turn = re.compile(r"^(fightCheckAccuracy|fightTrainerAi|fightApplyDamageVariance|WS_|ws|_ws)")
    thr = collections.Counter(t for (lab, _, cs), (_, _, t, _, _) in zip(draws, rows)
                              if lo <= lab <= hi and any(turn.match(c) for c in cs)).most_common(1)
    if not thr:
        print(f"# battle {battle}: no battle-logic draws in labels {lo}-{hi}", file=sys.stderr)
        return
    thr = thr[0][0]

    # The move camera's draws on the battle thread (a return address in the camera code, 0x801D2C6C-0x801D7230) go
    # to their own "camera N" list (2026-10-28): some procedural shots take another number of draws on the PC (P07's
    # battle 171), and kept apart they can't shift the rolls' list
    # (the shot code only: wazaSequenceCameraGetPattern and battleCameraStartWaza onwards; the same file's per-frame
    # fn_801D2D28 also draws, called from the blink update, and is no part of a shot: 2026-10-28, it sat in the
    # camera list and shifted it by one)
    def camera(rets):
        return any(0x801D2B4C <= v < 0x801D2C6C or 0x801D30BC <= v < 0x801D7230 for v in rets)

    # The idle camera's own draws (battleCameraStartRandom's 50/50 and target picks, on another thread): a return
    # address in battleCameraStartRandom or the return into fn_801EF488 after its call (0x801EF49C; the probe reads
    # saved return addresses, so a draw made straight from battleCameraStartRandom shows its caller); the shot it
    # starts (fn_801D2C74 -> battleCameraStartWaza) is camera code
    def idle(rets):
        return any(0x801C2D80 <= v < 0x801C2F00 or 0x801D2C74 <= v < 0x801D2D28 or v == 0x801EF49C for v in rets)

    # A model animation's particle (HSD_JObjAnimCallback -> gsParticlePtclHook -> gsParticleCreate) draws on whichever
    # thread animates the model: P24's battle 4 had one on the console's battle thread, the PC's ran elsewhere, and the
    # battle list shifted by one (a critical hit lost two turns later; port-gaps 49, 2026-11-02). In no list; the
    # PC leaves the same draws out (pc_anim_particle_depth, src/game/pc_debug_game.c).
    def anim(rets):
        return any(0x80119BD0 <= v < 0x80119D90 for v in rets)

    # three lists (2026-10-28): the battle thread's rolls; the idle camera's own rolls (its 50/50 and target picks);
    # every camera shot's draws, a move's or an idle shot's, in time order (their counts can differ on the PC)
    kinds = (("battle", lambda t, rets: t == thr and not camera(rets) and not anim(rets)),
             ("camera", lambda t, rets: camera(rets)),
             ("idle", lambda t, rets: t != thr and idle(rets) and not camera(rets)))
    print(f"# console battle thread {thr}, labels {lo}-{hi} of {replay}")
    counts = []
    for kind, keep in kinds:
        print(f"{kind} {battle}")
        k = 0
        for i in range(1, len(rows)):
            lab, _, t, _, rets = rows[i]
            if lo <= lab <= hi and keep(t, rets):
                print(f"{rows[i - 1][1].upper()}  # label {lab}")
                k += 1
        counts.append(k)
    n, c, m = counts
    print(f"# battle {battle}: {n} battle-roll draws, {c} move-camera draws, {m} idle-camera draws", file=sys.stderr)


if __name__ == "__main__":
    main()
