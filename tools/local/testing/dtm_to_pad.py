"""Turn a sitting's Dolphin movie into a PC pad script (2026-10-18; the playthrough → PC replay path).

    python -I tools/local/testing/dtm_to_pad.py <sitting folder> <out.pad> [--poll first|second] [--report]

A sitting folder (playrec.py) holds `sitting.dtm` (Dolphin's movie: every controller poll, exact), `events.jsonl` (the
recorder's log: room entries by frame since power-on) and `states/*.sav.dtm` (the movie's copy at each save state).
The output is the PC's ORRE_PAD_SCRIPT format (port/src/pc_pad_script.c), keyed to room entries so the PC's own load
times don't shift the inputs:
- the first step after a room entry is `@R+N` (N frames after that entry), later steps `+N` after the previous one
  (the script rule: `@R+N` waits for the first entry of R at or after the previous step);
- before the first room entry, and during stretches the recorder logs as no room, steps are absolute frames / `+N`.

Facts this relies on (verified from sitting 002, 2026-10-18):
- The movie starts when the player picks Movie → Start Recording, a few seconds after power-on: record 0 is at
  power-on frame ~657 (every save state agrees within 2 frames). Frames before that are taken as a released pad
  (the recorder's own pad log shows nothing pressed there).
- Dolphin records each poll, usually two per frame but not always (2,495 records by frame 1,248). Each state's movie
  copy gives (frames, records) checkpoints; between them the poll index is interpolated.
- The game keeps only the latest poll (input.c `inputPadCallback` overwrites the latch), and the PC polls once per
  frame. --poll picks which of a frame's polls stands for the frame (default: the second, the one read last).
Record layout (Dolphin's GameCube ControllerState, 8 bytes): byte 0 bits Start A B X Y Z Up Down, byte 1 bits Left
Right L R (then disc / reset / connected / origin), then trigger L, trigger R, stick X, stick Y, C-stick X, C-stick Y
(0x80 = centre, Y above 0x80 = up: the pad script's own convention).
"""
import argparse
import bisect
import glob
import json
import math
import os
import re
import statistics
import struct

BITS0 = ["START", "A", "B", "X", "Y", "Z", "UP", "DOWN"]
BITS1 = ["LEFT", "RIGHT", "L", "R"]
NO_ROOM = 0xFFFFFFFF
# the rooms the game pushes like menus (docs/engine/menu-rooms.md): never where the player reappears after a battle
MENU_ROOMS = {0x385, 0x386, 0x387, 0x38F, 0x390, 0x391, 0x392, 0x393, 0x394, 0x397, 0x39D, 0x3E7, 0x8F}


def read_dtm(path):
    d = open(path, "rb").read()
    if d[:4] != b"DTM\x1a":
        raise SystemExit(f"{path}: not a Dolphin movie")
    vi, inputs = struct.unpack_from("<QQ", d, 13)
    recs = [d[0x100 + 8 * i:0x108 + 8 * i] for i in range((len(d) - 0x100) // 8)]
    return {"game": d[4:10].decode("ascii", "replace"), "from_state": d[12], "vi": vi, "inputs": inputs,
            "recs": recs}


def what(rec):
    b0, b1, lt, rt, sx, sy, cx, cy = rec
    toks = [n for i, n in enumerate(BITS0) if b0 >> i & 1] + [n for i, n in enumerate(BITS1) if b1 >> i & 1]
    for name, v in (("SX", sx), ("SY", sy), ("CX", cx), ("CY", cy)):
        if v != 0x80:
            toks.append(f"{name}={v}")
    for name, v in (("LT", lt), ("RT", rt)):
        if v:
            toks.append(f"{name}={v}")
    return "+".join(toks) if toks else "-"


def f32(v):
    """round to single precision (the game's stick filter runs in f32: fsubs / fdivs / fadds)"""
    return struct.unpack("<f", struct.pack("<f", v))[0]


def clamp_stick(x, y, radius=56):
    """input.c InputClampStick: the +-10 dead zone, then the circle limit (integer maths, the length truncated)"""
    x = 0 if -10 < x < 10 else x - 10 if x > 0 else x + 10
    y = 0 if -10 < y < 10 else y - 10 if y > 0 else y + 10
    sq = x * x + y * y
    if sq > radius * radius:
        n = int(f32(math.sqrt(sq)))
        x = abs(x) * radius // n * (1 if x >= 0 else -1)  # the project's / truncates towards zero
        y = abs(y) * radius // n * (1 if y >= 0 else -1)
    return x, y


def smooth_stick(st, x, y, frames, mode):
    """input.c inputSmoothStick on the main stick; st = [lastX, lastY, stepX, stepY, posX, posY, smoothCount]"""
    lx, ly, sx, sy, px, py, n = st
    if lx < x - 2 or lx > x + 2 or ly < y - 2 or ly > y + 2:
        d = f32(frames) if mode == 0 else f32(frames * 0.25) if mode == 1 else 0.0
        if d:
            sx, sy = f32(f32(x - px) / d), f32(f32(y - py) / d)
        n = 0
    lx, ly = x, y
    px, py = f32(px + sx), f32(py + sy)
    if mode == 1:
        n = (n + 1) & 0xFF
        if n < frames:
            sx, sy = f32(sx * 0.5), f32(sy * 0.5)
            if abs(sx) < 1.0:
                sx = 1.0 if sx > 0 else -1.0
            if abs(sy) < 1.0:
                sy = 1.0 if sx > 0 else -1.0  # retail tests stepX here (input.c)
    if (sx < 0 and px < lx) or (sx >= 0 and px > lx):
        px = float(lx)
    if (sy < 0 and py < ly) or (sy >= 0 and py > ly):
        py = float(ly)
    return [lx, ly, sx, sy, px, py, n]


def game_reads(replay_dir, log=None):
    """Replay frame -> the main stick (x, y) the game's GSinputRead took on that frame, from a Dolphin replay's input
    probe (input.csv) and the frames it ran a game frame on (pos.csv's frame_retraces: 2 = no game frame).

    Main / side session, 2026-10-19 (S02's Phenac walk, verified): the probe samples `latched` at a fixed point of the
    frame, and the console's sampling callback keeps overwriting it, so the probe's value is not always the one the game
    read. In Phenac the read took the previous row's latch on 330 game frames, the same row's on 40, and a poll the
    probe never saw on 15 (e.g. (71,-27) between (70,-29) and (71,-25)). A fixed lag gets the second and third kinds
    wrong: at +370 from Wes's appearance the PC read (90,26) where the console had read (90,25), his step came out
    0.003 units long, and a turn later he slipped past the obstacle that stops him in Dolphin.
    So: replay input.c's stick filter and clamp (single precision) and take, per game frame, the candidate (the previous
    row's latch, this row's, or the stick the filter's move implies) that reproduces the probe's `current`, `filtered`
    and `out` exactly. Where none does (a GSinputResetLeftSmoothing after the read, a replay started from a save state,
    a frame with no read, the filter's hidden step lost after a reset), keep one that gives `current` and `out`, else
    one that gives `current` (the read itself, clamped), else the previous row's latch (the old fixed lag), and
    resynchronise the filter from the probe. Sitting 2: 8,874 / 67 / 46 exact (previous / this / unseen), 29
    resynchronised, 24 unresolved; sitting 4: 11,597 / 469 / 55, 427, 22.
    Returns (reads, counts); `log` (a list) collects the frames that weren't reproduced exactly."""
    import csv
    retr = {}
    pos_csv = os.path.join(replay_dir, "pos.csv")
    if os.path.exists(pos_csv):
        retr = {int(r["frame"]): r["frame_retraces"] for r in csv.DictReader(open(pos_csv, encoding="utf-8"))}
    st, prev, reads = [0, 0, 0.0, 0.0, 0.0, 0.0, 0], None, {}
    counts = {"previous latch": 0, "this latch": 0, "unseen poll": 0, "resynchronised": 0, "unresolved": 0}
    for r in csv.DictReader(open(os.path.join(replay_dir, "input.csv"), encoding="utf-8")):
        if r["slot"] != "0":
            continue
        f = int(r["frame"])
        lat = (int(r["latched_x"]), int(r["latched_y"]))
        cur = (int(r["current_x"]), int(r["current_y"]))
        fil = (float(r["filtered_x"]), float(r["filtered_y"]))
        out = (int(r["out_x"]), int(r["out_y"]))
        seen = (cur, fil, out)
        plat = prev[1] if prev is not None else lat
        if retr.get(f) == "2" and prev is not None and seen == prev[0]:
            prev = (seen, lat)
            continue
        prev = (seen, lat)
        frames, mode, omode = int(r["smooth_frames"]), int(r["smooth_mode"]), int(r["output_mode"])
        cands = [plat, lat]
        d = frames if mode == 0 else frames * 0.25
        if d and fil != (st[4], st[5]):
            cands.append((round(st[4] + (fil[0] - st[4]) * d), round(st[5] + (fil[1] - st[5]) * d)))
            cands.append((round(fil[0]), round(fil[1])))
        tried = []
        for c in dict.fromkeys(cands):
            ns = smooth_stick(st, c[0], c[1], frames, mode)
            q = (int(ns[4]), int(ns[5]))  # f32 -> s8 (truncation)
            ccur, cout = (clamp_stick(*c), clamp_stick(*q)) if omode == 2 else (c, q)
            tried.append((c, ns, ccur == cur and cout == out, (ns[4], ns[5]) == fil, ccur == cur))
        exact = [t for t in tried if t[2] and t[3]]
        # `current` is the read itself (clamped), whatever the filter's hidden state: second choice after current + out
        loose = [t for t in tried if t[2]] or [t for t in tried if t[4]]
        if exact:
            c, st = exact[0][0], exact[0][1]
            if c == plat:
                counts["previous latch"] += 1
            elif c == lat:
                counts["this latch"] += 1
            else:
                counts["unseen poll"] += 1
        else:
            c, ns = (loose[0][0], loose[0][1]) if loose else (plat, smooth_stick(st, plat[0], plat[1], frames, mode))
            counts["resynchronised" if loose else "unresolved"] += 1
            if log is not None:
                log.append((f, "resynchronised" if loose else "unresolved", c, lat, cur, fil, out))
            reset = fil[0].is_integer() and fil[1].is_integer() and fil != (ns[4], ns[5])
            st = [int(fil[0]) if reset else c[0], int(fil[1]) if reset else c[1], ns[2], ns[3], fil[0], fil[1], ns[6]]
        reads[f] = c
    return reads, counts


# Frames the save states' "f" lags the movie's own frame count (sitting 2: states 657, stick match 665). Used only when
# no input probe exists to match against; one sitting measured, so a likely value, not a settled one.
STATE_CORRECTION = 8


def newest_input_probe(sitting):
    reps = sorted(glob.glob(os.path.join(sitting, "replay_*", "input.csv")),
                  key=lambda p: int(os.path.basename(os.path.dirname(p)).rsplit("_", 1)[1]))
    return reps[-1] if reps else None


def match_offset(probe, sitting, events, rec_for_frame, guess, span=30):
    """The power-on frame of movie record 0 that makes the movie's stick equal the game's latched stick (InputPad
    +0x1A / +0x1B in input.csv) on the most frames where the stick is moving. Replay frames map to recording frames by
    the first shared room entry. Returns (offset, rate, frames compared, stick Y sign) or None."""
    rep_events = os.path.join(os.path.dirname(probe), "replay_events.jsonl")
    if not os.path.exists(rep_events):
        return None
    # align on the first replay entry that matches a recorded one by room and RNG state (as playrec.py's
    # check_aligned): a movie that starts from a save state has no replay entry for the rooms before it
    rec_entries = [e for e in events if e.get("ev") == "entered"]
    pair = None
    for e in map(json.loads, open(rep_events, encoding="utf-8")):
        if e.get("ev") == "entered":
            pair = next(((r, e) for r in rec_entries if r["room"] == e["room"] and r.get("rng") == e.get("rng")), None)
            if pair:
                break
    if not pair:
        return None
    shift = pair[0]["f"] - pair[1]["f"]              # recording frame = replay frame + shift
    rows = []
    for line in open(probe, encoding="utf-8"):
        p = line.strip().split(",")
        if not p[0].isdigit() or len(p) < 5 or p[3] == "":
            continue
        lx, ly = int(p[3]), int(p[4])
        if lx or ly:                                  # moving-stick frames only: a centred stick matches anywhere
            rows.append((int(p[0]) + shift, lx, ly))
    cache = {}

    def stick(m):
        if m not in cache:
            r = rec_for_frame(m)
            cache[m] = (r[4] - 0x80, r[5] - 0x80)
        return cache[m]

    best = None
    for off in range(guess - span, guess + span + 1):
        for sign in (1, -1):
            n = hit = 0
            for f, lx, ly in rows:
                m = f - off
                if m < 0:
                    continue
                n += 1
                sx, sy = stick(m)
                hit += sx == lx and sign * sy == ly
            if n and (best is None or hit / n > best[1]):
                best = (off, hit / n, n, sign)
    return best


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("sitting")
    ap.add_argument("out")
    ap.add_argument("--poll", choices=("first", "second"), default="second")
    ap.add_argument("--report", action="store_true", help="print the offset and checkpoint details")
    ap.add_argument("--reseed", action="store_true",
                    help="at every room entry, set Dolphin's RNG state there (a '!seed' step; the PC needs ORRE_DEBUG=1)")
    ap.add_argument("--reseed-noop", action="store_true",
                    help="with --reseed: the same room-entry steps and press timing, but they only re-state the pad "
                         "instead of setting the random state (2026-10-20: measures the port's own random state "
                         "without changing how the presses are timed)")
    ap.add_argument("--offset", type=int, default=None,
                    help="power-on frame of record 0 (default: matched against an input probe, see --input-probe). "
                         "Corrected 2026-10-19: the save states' estimate (657 for sitting 2) is 8 frames early; matching "
                         "the movie's stick against the game's own pad latch per frame (the input probe, "
                         "replay_13/input.csv) gives 665 (92%% of frames identical with the second poll)")
    ap.add_argument("--input-probe", help="a replay's input.csv (playrec.py replay N --probe input) to place the movie "
                                          "by matching its stick against the game's latched stick (default: the "
                                          "newest replay_K/input.csv of the sitting)")
    ap.add_argument("--latched", metavar="REPLAY_DIR",
                    help="take the main stick from a Dolphin replay's input probe (replay_<k>/input.csv: the stick the "
                         "game latched each frame) instead of the movie's polls. 2026-10-19: picking one of a "
                         "frame's two polls by interpolating between the save states' checkpoints chose the wrong poll "
                         "on about 8%% of frames (S02's Phenac: (50,50) where the console latched (53,49)), and the small "
                         "differences added up to Wes standing 10 units from the third conversation")
    ap.add_argument("--latched-lag", type=int, default=1,
                    help="frames the PC's room clock runs behind the replay's (Phenac: the player appears 1 frame "
                         "later from the room hook than from the recorder's entry); with 1, the stick set at frame f is "
                         "the one the console latched at f (the PC latches it a frame later, at the same room time)")
    ap.add_argument("--movie-buttons", action="store_true",
                    help="with --latched: keep the movie's buttons even when the input probe has the game's own "
                         "(current_buttons, 2026-10-21). By default the buttons each game frame read are placed the "
                         "same way as the stick reads: sitting 9's 30 fps Phenac talk took the movie's presses a game "
                         "frame apart from the console's, and a text box's key wait takes a press only on its frame")
    ap.add_argument("--latched-probe", action="store_true",
                    help="with --latched: take the probe's latched column as it is (before 2026-10-19's fix) instead of "
                         "the stick each game frame actually read, reconstructed from the probe's current / filtered / "
                         "out (game_reads: the probe's latch is sometimes a poll newer than the one the game read)")
    ap.add_argument("--windows", metavar="REPLAY_DIR",
                    help="time presses from menu openings (2026-10-19; snatcherLeo: 'time battle and menu inputs from "
                         "when each menu opens'): REPLAY_DIR/windows.csv (the probe: the frame each window reaches "
                         "phase 2, i.e. starts reading buttons). Every opening becomes an anchor step "
                         "'@0x2<id>+0:<current pad>' (the PC's OrreHook_WindowReady), in the recording's order, and the "
                         "presses after it count from it")
    ap.add_argument("--window-frameat", metavar="REPLAY_DIR",
                    help="with --windows: after each anchored window opening a '+0:!frameat K' step, K = the console's "
                         "game frame there counted in its room's line of the frame-times file (REPLAY_DIR/frames.csv "
                         "and replay_events.jsonl, the replay frames_to_pc.py made that file from), so the PC's "
                         "recorded frame lengths follow the windows instead of the time since the room entry ("
                         "2026-10-28: P07's box visit drifted 123 retraces from its entry and lost a key repeat)")
    ap.add_argument("--press-frameat", metavar="ROOMS", default="",
                    help="with --window-frameat: also a '+0:!frameat K' after every press step in these rooms (hex, "
                         "comma-separated), K = the console's frame the press is read in, for room visits whose presses "
                         "count from the entry (no window anchors; P09's and P10's box 0x391: anchoring its windows made "
                         "the presses wait for openings the PC makes differently). Not in field rooms: in P10's walks it "
                         "moved Wes off the console's route. *Tried 2026-10-29, not recommended:* in room 0x391 alone "
                         "it still parted P10 at its first box visit and didn't fix P09's box (the grid then opened 2 "
                         "frames later after the press than on the console); kept as an experiment, off by default")
    ap.add_argument("--cursor-sync", metavar="ROOMS", default="",
                    help="with --windows: after each new A press in these rooms' visits (hex, comma-separated) a "
                         "'+0:!cursor ID:BASE:POS ...' step with the console's cursor of every open window that has one, "
                         "from REPLAY_DIR/menu.csv just before the press (2026-10-29: P09's box and Y-menu cursors "
                         "ended one entry apart when a held key's repeat landed a frame apart; a replay resync)")
    ap.add_argument("--window-seed", action="store_true",
                    help="with --windows: at each anchored window opening also set Dolphin's random state there (a "
                         "'!seed' step from REPLAY_DIR/rng.csv, the same replay's per-frame state; needs ORRE_DEBUG=1). "
                         "A test aid like --reseed (2026-10-19): S02's battle drifts a draw or two between menus "
                         "(a send-out a frame late shifts one Pokémon's blink timer), and the AI picks targets with the "
                         "next draws; this keeps each turn's decisions on Dolphin's numbers while that is fixed")
    ap.add_argument("--no-battle-window-seed", action="store_true",
                    help="with --window-seed: no seeds at the battle windows themselves, only at the player's "
                         "reappearance after each battle (2026-10-21: with the disc reads and ARAM copies timed "
                         "like the console, P01's battles run draw for draw with Dolphin's, and a seed taken a frame "
                         "off moved the PC's state back or ahead and parted the camera's random shots)")
    ap.add_argument("--window-seed-rooms", default="",
                    help="with --window-seed and --no-battle-window-seed: battle-window seeds after all in these room "
                         "visits, from FROM recorded frames after the entry: ROOM@FROM,... (hex room). The resync at a "
                         "timing-only split (snatcherLeo's option 2026-10-25): from the split on, each battle menu starts "
                         "from Dolphin's random state, so the check goes on past a frame of load timing")
    ap.add_argument("--window-seed-skip-ids", default="",
                    help="window ids (hex, comma list) whose openings never get a seed, e.g. 0x50, the battle message "
                         "box: it opens inside move animations, where a seed taken at the end of a particle-burst frame "
                         "makes the PC repeat part of the burst (2026-10-25, P05)")
    ap.add_argument("--window-seed-burst", type=int, default=0,
                    help="resync seeds whose recorded frame drew more than N numbers take the state from before that "
                         "frame (the PC draws the burst itself after the seed); 0 = off")
    ap.add_argument("--window-seed-keep", type=int, default=0,
                    help="1: a seed that --window-seed-burst moved to the state before a burst also lists the state "
                         "after it ('seed X keep A'), and the PC keeps its own state when it is that one: it made the "
                         "burst already (a heavy logic pass that ran past the refresh, 2026-10-26, P12's battle "
                         "207: such a seed put the PC 192 draws back). N > 1 also lists Dolphin's states N-1 frames "
                         "around the seed, which leaves alone a PC that drifted by up to N-1 frames: P12's battle 204 "
                         "then lost a roll with N = 3 (the review warned of it). 0 = off")
    ap.add_argument("--window-seed-blink", default="",
                    help="a replay folder with owners.csv (the owners probe): each resync seed also sets the battle's "
                         "idle blink timers to the console's at that label (the 'blink SLOT:TIMER' debug command)")
    ap.add_argument("--window-seed-only-ids", default="",
                    help="window ids (hex, comma list): resync seeds only where one of these opens, e.g. 0x47, the "
                         "battle's command menu (the start of each command phase, 2026-10-25, P05: seeds at the "
                         "windows that open during move animations part the battle)")
    ap.add_argument("--window-rng", metavar="REPLAY_DIR",
                    help="where --window-seed reads rng.csv (another replay of the same sitting; default: --windows')")
    ap.add_argument("--window-seed-shift", type=int, default=0,
                    help="take the seed from rng.csv this many frames after the opening (calibration of the probe's "
                         "frame labels against the PC's boundary)")
    ap.add_argument("--burst-seed-max", type=int,
                    help="keep only the first N burst seeds (2026-10-21: P01's first is the title screen's, which "
                         "stands for the console seeding its generator from the clock at boot; the second is the New "
                         "Game burst's, which --entry-lag 1 on the name screen makes unnecessary)")
    ap.add_argument("--burst-seed", metavar="REPLAY_DIR",
                    help="before each burst of at least 1,000 draws in one frame that follows 20 or more quiet frames "
                         "(REPLAY_DIR/rng.csv), set Dolphin's state in the middle of the quiet stretch (a '!seed' step; "
                         "ORRE_DEBUG=1). A test aid like --reseed (2026-10-21, sitting 9): the New Game burst "
                         "(1,016 draws: the Strategy Memo, the trainer id, the starters) comes 31 frames after the name "
                         "screen's last draw; on the PC that screen stops drawing a frame early and the burst comes 2 "
                         "frames early, so it rolled other starters (Umbreon 86 / Espeon 71 max HP vs 88 / 75)")
    ap.add_argument("--hero", metavar="REPLAY_DIR",
                    help="after each battle, time the presses from the player's reappearance (REPLAY_DIR/pos.csv: the "
                         "first frame his position is valid again; the PC's anchor 0x1<room>, room = the one the battle "
                         "started from). 2026-10-19: a battle runs a different number of frames on the PC "
                         "(snatcherLeo's option A), and the walk after it was still timed from inside the battle")
    ap.add_argument("--hero-lag", type=int, default=0, help="frames added to a press timed from a reappearance")
    ap.add_argument("--fadein", metavar="REPLAY_DIR",
                    help="time each room visit's presses from its fade-in's end (REPLAY_DIR/fade.csv, the fade probe; "
                         "the PC's '@0x3<room>' anchor); rooms with no fade-in keep the other anchors (2026-10-26)")
    ap.add_argument("--fade-lag", type=int, default=0, help="frames added to a press timed from a fade-in's end")
    ap.add_argument("--entry-lag", type=int, default=0,
                    help="frames added to the first press after a room entry's seed step (P01's name screen: 1)")
    ap.add_argument("--entry-lag-rooms", help="only these rooms (hex, comma-separated) get --entry-lag")
    ap.add_argument("--entry-lag-nofade", type=int, default=0,
                    help="with --fadein: frames added to the first press of every non-battle room visit that has no "
                         "fade-in anchor (the PC marks an entry 2 refreshes early; P02's world map); 0 = off")
    ap.add_argument("--entry-lag-every", help="these rooms (hex, comma-separated) get --entry-lag on every visit, not "
                                              "only the first (2026-10-24: the pocket menu 0x38F, made for "
                                              "the PC's ORRE_DVD_SUBFIELD switch, removed 2026-11-01)")
    ap.add_argument("--battle-mash", action="store_true",
                    help="with --hero: tap A ('MASH' step) from the console's reappearance after each battle until the "
                         "PC's (2026-10-21, battle-tolerant replays: boxes the recording has no presses for)")
    ap.add_argument("--hero-rooms", action="store_true",
                    help="with --hero: also time every room's walk from the player's first appearance in it, not only "
                         "the walk after a battle. 2026-10-19, sitting 4: the player appears 1 or 2 frames later "
                         "after a door on the PC than in Dolphin, by room (Mayor's house +1, the room next to it +2), "
                         "and a walk timed from the room entry reached the next door early")
    ap.add_argument("--windows-everywhere", action="store_true",
                    help="anchor every window opening, not only those in battles")
    ap.add_argument("--windows-rooms", default="",
                    help="also anchor the window openings in these room visits (hex room, optionally ':N' for its Nth "
                         "visit only, '@FROM' for openings FROM recorded frames after the entry; comma list): from the "
                         "visit's entry (or FROM) to the next entry")
    ap.add_argument("--window-lag-rooms", default="",
                    help="per-room --window-lag for presses timed from battle windows: ROOM:LAG[@FROM],... (hex room; "
                         "its visits from entry, or FROM recorded frames after it, to the next entry); ROOM:VISIT:LAG"
                         "[@FROM] for one visit only (2026-10-26: P12's battle 207 is 0xD9:4:2, one game frame at "
                         "30 fps)")
    ap.add_argument("--press-lag", default="",
                    help="one press later, the rest on time: ROOM:VISIT@FRAME:LAG,... (hex room, its VISITth visit "
                         "from 1, the first button change at or after FRAME recorded frames into that visit comes LAG "
                         "frames later and the step after it LAG frames earlier; if that step's recorded gap is "
                         "shorter than LAG it lands on the lagged press's frame and the rest drifts by the difference). "
                         "2026-10-26: P12's battle 207 "
                         "needs only its turn-2 last A one 30 fps game frame later, 0xD9:4@4287:2 (--window-lag-rooms "
                         "lags the first press after each window, which moved that whole menu)")
    ap.add_argument("--windows-rooms-other", action="store_true",
                    help="with --windows-rooms: instead of the room's own windows, the others before the next entry "
                         "(a cutscene's, the probe's room column -1)")
    ap.add_argument("--fadein-hero-room", action="store_true",
                    help="with --fadein and --hero: a fade-in that follows a player reappearance in the same recorded "
                         "visit (after a battle, or a --hero-rooms one; the review 2026-10-30) takes that reappearance's "
                         "room (the recorder's missing re-entry of the field room)")
    ap.add_argument("--window-lag-ids", default="",
                    help="with --windows: ID:LAG,... (hex window ids): frames added to a press timed from an anchored "
                         "opening of one of these windows, wherever it opens (2026-10-31: the field message windows 0x40 / 0x41, "
                         "which the PC's anchor marks ready one frame before their first typing frame and the console's "
                         "probe on it; a whole room's lag, --window-lag-rooms 0x38F:1, broke P24's world-map menus)")
    ap.add_argument("--fadein-return-rooms", default="",
                    help="with --fadein: in a recorded visit of these menu rooms (hex, comma list), the field's own "
                         "fade-in when the player is back (Wes's position valid again; the recorder logs no entry for "
                         "the return) is an anchor too, in the field room entered last before the visit, and with --hero "
                         "so is the player's reappearance there (2026-10-31, sitting 40's purification chamber "
                         "0x385)")
    ap.add_argument("--windows-rooms-unroomed", action="store_true",
                    help="with --windows-rooms: the room's own windows and also those the probe logged with room -1 "
                         "inside the visit (a Y-menu reopening after a menu room, before the field is back)")
    ap.add_argument("--windows-rooms-lag", type=int, default=None,
                    help="with --windows-rooms: frames added to a press timed from those windows (default: --window-lag)")
    ap.add_argument("--window-lag", type=int, default=0,
                    help="frames added to a press timed from a window opening (calibration; 0: the press keeps its "
                         "recorded distance from the opening)")
    a = ap.parse_args()
    events = [json.loads(l) for l in open(os.path.join(a.sitting, "events.jsonl"), encoding="utf-8") if l.strip()]
    # A sitting without a Dolphin movie (sitting 1) has only the recorder's pad log: pad.jsonl, one line per change of
    # the 10 latched PADStatus bytes, in power-on frames. 2026-10-19: those are the values the game read (no "which
    # poll" guess), and a Dolphin replay of the sitting injects them at the latch, so that replay is the reference.
    padlog = None
    if not os.path.exists(os.path.join(a.sitting, "sitting.dtm")):
        padlog = [json.loads(l) for l in open(os.path.join(a.sitting, "pad.jsonl"), encoding="utf-8") if l.strip()]
        last = json.load(open(os.path.join(a.sitting, "sitting.json"), encoding="utf-8")).get("last_frame") or \
            max(p["f"] for p in padlog)
        movie = {"game": "", "from_state": 0, "vi": last + 1, "inputs": len(padlog), "recs": None}
    else:
        movie = read_dtm(os.path.join(a.sitting, "sitting.dtm"))
    import csv

    def replay_offset(replay_dir, what):
        """sitting frame = replay frame + this (from the room entries both logs have)"""
        rev = [json.loads(l) for l in open(os.path.join(replay_dir, "replay_events.jsonl"), encoding="utf-8")
               if l.strip()]
        rec = [e for e in events if e.get("ev") == "entered"]
        rep = [r for r in rev if r.get("ev") == "entered"]
        # pair each replay entry with the recorded one of the same room and random state (a movie from a save state
        # doesn't replay the entries before it: sitting 4 starts after the title rooms; the matcher does the same,
        # bc29a9fa7); the offset is the one most pairs agree on
        offs = [e["f"] - r["f"] for r in rep for e in rec if e["room"] == r["room"] and e.get("rng") == r.get("rng")]
        if not offs:
            raise SystemExit(f"{what}: no replay room entry matches a recorded one by room and random state")
        best = statistics.mode(offs)
        if offs.count(best) < 2 and len(rep) > 1:
            raise SystemExit(f"{what}: the replay's room entries don't line up with the sitting's ({offs[:9]})")
        return best

    latched, latched_off, read_counts = {}, None, None
    read_buttons = {}  # sitting frame -> PADStatus button word the console's game read (input probe, current_buttons)
    if a.latched:
        latched_off = replay_offset(a.latched, "--latched")
        for r in csv.DictReader(open(os.path.join(a.latched, "input.csv"), encoding="utf-8")):
            if r["slot"] == "0":
                latched[int(r["frame"]) + latched_off] = (int(r["latched_x"]), int(r["latched_y"]))
        probe_latched = dict(latched)  # the poll-pick calibration below votes with the probe's own column
        if not a.latched_probe:
            # the stick each game frame actually read (game_reads), placed one frame before that read: with
            # --latched-lag 1 the PC's read on frame g takes the value set on g - 1 (S02, verified per frame)
            read_log = [] if a.report else None
            reads, read_counts = game_reads(a.latched, read_log)
            for f, kind, c, lat, cur, fil, out in read_log or ():
                print(f"game read at replay frame {f} {kind}: took {c} (probe latch {lat}, current {cur}, "
                      f"filtered {fil}, out {out})")
            for g, c in reads.items():
                latched[g - 1 + latched_off] = c
            # hold each read until the next one (2026-10-25, P08's lobby walk): in a 30 fps scene the reads come
            # every other frame, and the frames between them kept the probe's own latched sample (or the movie's raw
            # poll), which is not what the game read. A PC whose game frames run a retrace out of phase with the
            # console's reads on exactly those frames: it took ((-14, 0) where the console's game read (-7, 0)), the
            # stick filter parted and Wes walked past the spot where he is blocked in Dolphin. With the frames in phase
            # the PC never reads there, so nothing changes for them
            rg = sorted(reads)
            for g, nxt in zip(rg, rg[1:]):
                for k in range(g, nxt - 1):
                    latched[k + latched_off] = reads[g]
        if not a.movie_buttons:
            # the buttons the game read on each frame (the probe's current PADStatus, +0x24), placed one frame before
            # that read like the stick (with --latched-lag 1 the PC's read on frame g takes the value set on g - 1)
            for r in csv.DictReader(open(os.path.join(a.latched, "input.csv"), encoding="utf-8")):
                if r["slot"] == "0" and r.get("current_buttons") not in (None, ""):
                    read_buttons[int(r["frame"]) - 1 + latched_off] = int(r["current_buttons"])
    # the recording's battles, (start, result) frames: room entries inside one are the bag / party menus (0x38F) the
    # player opened mid-battle, which a battle that runs differently on the PC may never open (snatcherLeo's option A;
    # 2026-10-21: sitting 11's PC won Miror B.'s peon one bag visit sooner and stood waiting for the second)
    battle_spans, bstart = [], None
    for e in events:
        if e.get("ev") == "battle" and bstart is None:
            bstart = e["f"]
        elif e.get("ev") == "battle_result" and bstart is not None:
            battle_spans.append((bstart, e["f"]))
            bstart = None
    if bstart is not None:
        battle_spans.append((bstart, 1 << 30))

    def in_battle_menu(ent):
        # only the menu rooms (bag / party 0x38F, the P*DA 0x392): the battle's own room always comes, and a fallback
        # there could fire before a PC that reaches it a little later than the console
        return ent[1] in (0x38F, 0x392) and any(s <= ent[0] <= e for s, e in battle_spans)

    fa_off, fa_rows, fa_entered, fa_rooms = 0, [], [], []  # --window-frameat: the replay's frame rows and entries
    if a.window_frameat:
        fa_off = replay_offset(a.window_frameat, "--window-frameat")
        fa_rows = [int(l.split(",", 1)[0]) for l in open(os.path.join(a.window_frameat, "frames.csv"),
                                                         encoding="utf-8") if l[:1].isdigit()]
        fa_ent = sorted((e["f"], e["room"]) for e in (json.loads(l) for l in open(
            os.path.join(a.window_frameat, "replay_events.jsonl"), encoding="utf-8") if l.strip())
            if e.get("ev") == "entered")
        fa_entered = [ef for ef, _ in fa_ent]
        fa_rooms = [rm for _, rm in fa_ent]
    # ROOM[:VISIT] (2026-10-29): one visit only (P09's 37th bag visit, 0x38F:37, where a press the PC read in a
    # 6-refresh frame moved the cursor 28 frames late; for every visit of 0x38F the option broke P10's walk)
    press_fa_rooms = {(int(x.partition(":")[0], 16), int(x.partition(":")[2]) if x.partition(":")[2] else None)
                      for x in a.press_frameat.split(",") if x}
    fa_visits = []
    for k, rm in enumerate(fa_rooms):
        fa_visits.append(sum(1 for r2 in fa_rooms[:k + 1] if r2 == rm))
    cs_all = a.cursor_sync == "all"  # every room (the Y-menu opens in the field rooms)
    cs_rooms = {0} if cs_all else {int(x, 16) for x in a.cursor_sync.split(",") if x}
    cs_frames, cs_rows, cs_ent = [], [], []  # --cursor-sync: menu.csv rows (sitting frames), the visits
    if cs_rooms:
        if not a.windows:
            raise SystemExit("--cursor-sync needs --windows (its replay's menu.csv)")
        cs_off = replay_offset(a.windows, "--cursor-sync")
        # menu.csv writes every open window whenever anything changes, so the last snapshot before a press is the
        # menus' state then, however old (its 8-frame limit missed P09's box slot choice: a snapshot 16 frames back)
        for r in csv.DictReader(open(os.path.join(a.windows, "menu.csv"), encoding="utf-8")):
            cs_rows.append((int(r["frame"]) + cs_off, int(r["id"]), int(r["base"]), int(r["offset"]),
                            r.get("phase") == "2" and int(r.get("cursorCount") or 0) > 0))
        cs_rows.sort(key=lambda x: x[0])
        cs_frames = [x[0] for x in cs_rows]
        cs_ent = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]

    def cursor_step(f, since):
        """'+0:!cursor ...' with the console's open cursors in the last menu.csv snapshot before sitting frame f, if
        it was taken after `since` (the visit's entry: a snapshot from before it may name windows long closed)"""
        i = bisect.bisect_left(cs_frames, f) - 1
        if i < 0 or cs_frames[i] < since:
            return None
        at = cs_frames[i]
        cur = {}
        while i >= 0 and cs_frames[i] == at:
            _, wid, base, off, live = cs_rows[i]
            if live:
                cur.setdefault(wid, (base, off))
            i -= 1
        if not cur:
            return None
        return "+0:!cursor " + " ".join(f"0x{w:X}:{b}:{o}" for w, (b, o) in sorted(cur.items()))
    windows = {}  # sitting frame -> [window id, ...] in the probe's list order
    room_window_frames = set()  # the openings anchored only by --windows-rooms (their presses get --windows-rooms-lag)
    window_lag_at = {}  # opening frame -> its --window-lag-rooms lag
    seed_spans = []     # (from, to, shift): --window-seed-rooms visits
    if a.windows:
        woff = replay_offset(a.windows, "--windows")
        # battles only (default): between the recorder's "battle" and "battle_result" events. Tried on all windows first
        # (2026-10-19, sitting 2): the title menu and Phenac's conversation windows open a few frames apart on the PC
        # (window 0x10 ten frames earlier), and re-timing the overworld presses from them broke a walk that matches
        # Dolphin exactly with room-entry timing; in the battle the anchors are what make the commands register
        spans, start = [], None
        for e in events:
            if e.get("ev") == "battle" and start is None:
                start = e["f"]
            elif e.get("ev") == "battle_result" and start is not None:
                spans.append((start, e["f"]))
                start = None
        if start is not None:
            spans.append((start, 1 << 30))
        room_spans = []  # (from, to, room): --windows-rooms visits
        lag_spans = []   # (from, to, lag): --window-lag-rooms visits (2026-10-25: P01's Folly battle room starts
        # on a 2-retrace frame and its presses read a retrace after the console's, Willie's are in step)
        if a.window_lag_rooms:
            # "@FROM" (same day): the whole visit moved Folly's earlier presses (+1434 on), which were in step
            # ROOM:VISIT:LAG[@FROM] (2026-10-26, the project's ask): only the room's VISITth visit (from 1), as
            # --windows-rooms / --window-seed-rooms take it; P12's battle 207 (0xD9's 4th visit) reads its command press
            # one 30 fps game frame early, so 0xD9:4:2 lags that visit alone
            lags = {}
            for x in a.window_lag_rooms.split(","):
                if x:
                    parts = x.split(":")
                    room, visit, rest = (parts[0], None, parts[1]) if len(parts) == 2 else (parts[0], int(parts[1]),
                                                                                            parts[2])
                    lag, _, start = rest.partition("@")
                    lags[(int(room, 16), visit)] = (int(lag), int(start) if start else 0)
            ent_l = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]
            seen = {}
            for i, (ef, room) in enumerate(ent_l):
                seen[room] = seen.get(room, 0) + 1
                got = lags.get((room, seen[room])) or lags.get((room, None))
                if got:
                    lag, start = got
                    lag_spans.append((ef + start, ent_l[i + 1][0] - 1 if i + 1 < len(ent_l) else 1 << 30, lag))
        if a.window_seed_rooms:
            # --window-seed-rooms (2026-10-25): the resync spans, ROOM[:VISIT]@FROM[~SHIFT]. ":VISIT" (the room's
            # Nth visit, from 1) and "~SHIFT" (this span's own --window-seed-shift) came later the same night: P05's
            # first street battle needs its seeds a label later (the AI decides a frame early on the PC), the next
            # battle's split is in another visit of another room
            # "-TO" (frames after the entry; several spans per visit allowed) came later still: P05's battle 53 needs
            # shift 1 for its first command phase and 0 inside the move animations (a seed taken at the end of a
            # particle-burst frame makes the PC repeat part of the burst)
            want = {}
            for x in a.window_seed_rooms.split(","):
                if x:
                    head, _, rest = x.partition("@")
                    span, _, shift = rest.partition("~")
                    start, _, stop = span.partition("-")
                    room, _, visit = head.partition(":")
                    want.setdefault((int(room, 16), int(visit) if visit else None), []).append(
                        (int(start) if start else 0, int(stop) if stop else None,
                         int(shift) if shift else a.window_seed_shift))
            ent_s = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]
            seen = {}
            for i, (ef, room) in enumerate(ent_s):
                seen[room] = seen.get(room, 0) + 1
                end = ent_s[i + 1][0] - 1 if i + 1 < len(ent_s) else 1 << 30
                for start, stop, shift in want.get((room, seen[room]), want.get((room, None), [])):
                    seed_spans.append((ef + start, min(end, ef + stop) if stop is not None else end, shift))
        if a.windows_rooms:
            # --windows-rooms (2026-10-25): P01's second name-screen visit runs its windows one frame apart from
            # the console's whichever lag its first press gets, so its presses count from the openings instead
            # ROOM[:VISIT][@FROM] (2026-10-26, P13: '@FROM' anchors only the openings FROM recorded frames
            # after the entry, so a visit whose early windows differ on the PC keeps its entry timing there)
            want = []
            for x in a.windows_rooms.split(","):
                if x:
                    head, _, frm = x.partition("@")
                    want.append((int(head.split(":")[0], 16), int(head.split(":")[1]) if ":" in head else None,
                                 int(frm) if frm else 1))
            ent = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]
            visits = {}
            for i, (ef, room) in enumerate(ent):
                visits[room] = visits.get(room, 0) + 1
                for wr, wv, frm in want:
                    if room == wr and (wv is None or wv == visits[room]):
                        # not the entry frame itself: windows opening with the room would anchor before its entry step
                        room_spans.append((ef + max(1, frm), ent[i + 1][0] - 1 if i + 1 < len(ent) else 1 << 30, room))
                        break
        for r in csv.DictReader(open(os.path.join(a.windows, "windows.csv"), encoding="utf-8")):
            if r.get("event", "ready") != "ready":
                continue
            f = int(r["frame"]) + woff
            # a room visit's own windows only (the probe's room column): after P01's second name screen the cutscene's
            # windows (room column -1, before the next entry) re-timed Wes's walk to the door and it missed it
            # --windows-rooms-unroomed (2026-10-29, P09): also the windows with room column -1, the gap after a
            # menu room closes and before the field is back; the Y-menu that reopens there opened 21-45 frames later on
            # the PC (the field reload), so the presses meant for it fell before it was ready
            own = lambda rm: r.get("room", "").isdigit() and (int(r["room"]) == rm or (
                a.windows_rooms_unroomed and int(r["room"]) == NO_ROOM))
            in_room = any(s <= f <= e and own(rm) != a.windows_rooms_other for s, e, rm in room_spans)
            if a.windows_everywhere or in_room or any(s <= f <= e for s, e in spans):
                windows.setdefault(f, []).append(int(r["id"], 0))
                if in_room and not any(s <= f <= e for s, e in spans):
                    room_window_frames.add(f)
                for s, e, lag in lag_spans:
                    if s <= f <= e:
                        window_lag_at[f] = lag
        if a.window_lag_ids:
            # --window-lag-ids: by window kind, not by room (a frame with several openings takes the largest lag)
            id_lags = {int(x.split(":")[0], 16): int(x.split(":")[1]) for x in a.window_lag_ids.split(",") if x}
            for f, ids in windows.items():
                got = [id_lags[w] for w in ids if w in id_lags]
                if got:
                    window_lag_at[f] = max(got + ([window_lag_at[f]] if f in window_lag_at else []))
    # Text-box anchors (presses timed from each conversation box opening / closing) were tried on 2026-10-19 for
    # sitting 4 and dropped: boxes open up to 3 frames apart on the PC, so the anchors moved S02's Phenac walk off its
    # path, and sitting 4's real cause was the poll pick (--latched's calibration below)
    heroes = {}  # sitting frame -> room: the player's first reappearance after each battle
    battle_heroes = set()  # the reappearances after battles (the others come from --hero-rooms)
    if a.hero:
        hoff = replay_offset(a.hero, "--hero")
        results = [e["f"] for e in events if e.get("ev") == "battle_result"]
        entries_seen = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]
        appear, appear_room, pv = [], {}, False
        for r in csv.DictReader(open(os.path.join(a.hero, "pos.csv"), encoding="utf-8")):
            v = r["x"] not in ("", "None")
            if v and not pv:
                appear.append(int(r["frame"]) + hoff)
                appear_room[appear[-1]] = int(r["room"]) if r.get("room", "").isdigit() else NO_ROOM
            pv = v
        for rf in results:
            after = [f for f in appear if f > rf]
            # not the bag / party visits inside the battle: sitting 11's Miror B. battle left 0x38F as "the room before
            # the battle room" and the reappearance anchor waited for Wes in the bag (2026-10-21)
            # the room entered last before the battle began, outside every battle: two battles in a row in the same
            # stadium room (sitting 11: 0xCC twice, the field room 0x5 between them never "entered" again) made "the
            # entry before the battle room" the first battle's room (2026-10-21)
            span = next((s for s, e in battle_spans if e == rf), None)
            if span is not None:
                # each battle's own room: the entry nearest its start (it can come a little before the recorder's
                # "battle" event)
                broom = {min(entries_seen, key=lambda e: abs(e[0] - s))[1] for s, _ in battle_spans}
                # never a menu room (menu-rooms.md: rooms pushed like menus, the bag / party 0x38F among them): Mt.
                # Battle's zone room is also its battle room, so "the field room before it" became the pocket-menu
                # visit and the MASH after every Mt. Battle fight ran to its 3,600-frame fallback, re-talking to the
                # trainer (P14's 0x4C after battle 220; the PC reports the zone's floor room 0x4C, 2026-10-26)
                field = [e for e in entries_seen if e[0] < span and e[1] not in broom and e[1] not in MENU_ROOMS
                         and not in_battle_menu(e) and not any(s <= e[0] <= t for s, t in battle_spans)]
                if not after or not field:
                    continue
                heroes[after[0]] = field[-1][1]
                continue
            battle_entry = [e for e in entries_seen if e[0] <= rf and not in_battle_menu(e)]
            if not after or len(battle_entry) < 2:
                continue
            heroes[after[0]] = battle_entry[-2][1]  # the room entered before the battle room
        battle_heroes = set(heroes)
        if a.hero_rooms:
            # the anchor's room is the game's current room id at the appearance (pos.csv's room column; the PC's
            # watch reports the same id), which can differ from the entered room: in sitting 2's opening scene (room
            # 0x38F) the PC reports 0x4 and the probe records none, so a scene like that keeps room-entry timing
            for i, (ef, room) in enumerate(entries_seen):
                nxt = entries_seen[i + 1][0] if i + 1 < len(entries_seen) else 1 << 30
                after = [f for f in appear if ef <= f < nxt]
                if after and appear_room[after[0]] == room != NO_ROOM and after[0] not in heroes:
                    heroes[after[0]] = room
    return_anchors = set()  # --fadein-return-rooms' reappearance and fade-in anchors (re-anchored with !frameat too)
    fadeins = {}  # sitting frame -> room: each recorded room visit's fade-in end (--fadein)
    fade_visit_start = {}  # the same frames -> the entry frame of their recorded visit (--fadein-hero-room)
    nofade_visits = set()  # entry frames of non-battle visits with no fade-in anchor (--entry-lag-nofade)
    if a.fadein:
        # snatcherLeo's step 3 (2026-10-26; gamecube-accuracy.md "Comparing with Dolphin"): the evidence table showed the
        # game reads each press at the same refresh after the fade-in's end on both sides, while the room change before
        # it is 1-2 refreshes shorter on the PC; so presses after a room's fade-in count from it. The fade probe logs a
        # row when the state changes; the end is pending 1 -> 0 with type 2, the first one in each visit
        foff = replay_offset(a.fadein, "--fadein")
        ends, pend = [], None
        for r in csv.DictReader(open(os.path.join(a.fadein, "fade.csv"), encoding="utf-8")):
            if not r["frame"].isdigit():
                continue
            p = int(r["pending"])
            if pend == 1 and p == 0 and int(r["type"]) == 2:
                ends.append(int(r["frame"]) + foff)
            pend = p
        visits = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]
        battle_starts = [e["f"] for e in events if e.get("ev") == "battle"]
        for i, (ef, room) in enumerate(visits):
            nxt = visits[i + 1][0] if i + 1 < len(visits) else 1 << 30
            if any(ef - 30 <= b < nxt for b in battle_starts):
                # a battle room keeps its window / reappearance timing (2026-10-26: P01's six extra open gaps
                # were in Willie's and Folly's battle rooms, where a fade-in anchor moved presses the windows timed)
                continue
            got = [f for f in ends if ef - 5 <= f < nxt]
            if got:
                fadeins[got[0]] = room
                fade_visit_start[got[0]] = ef
            else:
                nofade_visits.add(ef)
        if a.fadein_return_rooms:
            # --fadein-return-rooms (2026-10-31, sitting 40; the file, flagged to D): without it the walk after
            # the purification chamber counted from the chamber's last window across the field reload, which runs ~11
            # frames shorter on the PC (441 frames from the last window to Wes reappearing, the console's 452), so the
            # stick came ~13 frames late against Wes and each walk to the PC terminal ended somewhere else. The field's
            # fade-in ends 60 frames after the reappearance on both sides, so presses counted from it keep their place.
            # The chamber's own fade-in (the visit's first, already anchored above) has no valid position for Wes
            ret_rooms = {int(x, 16) for x in a.fadein_return_rooms.split(",") if x}
            valid, invalid = set(), set()  # frames whose row has the player / says "no player"
            pos_csv = os.path.join(a.fadein, "pos.csv")
            if not os.path.exists(pos_csv):
                raise SystemExit("--fadein-return-rooms needs the pos probe in the --fadein replay")
            for r in csv.DictReader(open(pos_csv, encoding="utf-8")):
                # the probe writes "# frame N error ..." lines when it hits an exception (the review)
                if r["frame"].isdigit():
                    (valid if r["x"] not in ("", "None") else invalid).add(int(r["frame"]) + foff)
            # the player's reappearance is an anchor too (0x1<field room>, as after a battle): the stick moves start
            # right after it, and an anchor only arms once the step before it has fired, so the fade-in's anchor alone
            # still fired late when that step (timed from the menu room's last window) came after the PC's fade-in
            # had ended (sitting 40, p2_S40i: 13 frames late)
            # only where the row before exists and says "no player" (the review): a frame missing from pos.csv is not a
            # reappearance, which would plant an anchor the PC never fires in the middle of a walk
            appears = sorted(f for f in valid if f - 1 in invalid)
            for i, (ef, room) in enumerate(visits):
                if room not in ret_rooms:
                    continue
                nxt = visits[i + 1][0] if i + 1 < len(visits) else 1 << 30
                field = [rm for _, rm in visits[:i] if rm not in MENU_ROOMS and rm != NO_ROOM]
                if not field:
                    continue
                for f in ends:
                    if ef <= f < nxt and f not in fadeins and f in valid:
                        fadeins[f] = field[-1]
                        fade_visit_start[f] = ef
                        return_anchors.add(f)
                if a.hero:
                    for f in appears:
                        if ef < f < nxt and f not in heroes:
                            heroes[f] = field[-1]
                            return_anchors.add(f)
        if a.fadein_hero_room:
            # --fadein-hero-room (2026-10-29, P13): a fade-in that follows the player's reappearance after a battle
            # takes that reappearance's room. The recorder logs no entry when a battle returns to a field room with the
            # same floor id (the "reentered", newer replays only), so the recorded visit is still the battle's menu room
            # 0x38F while the PC reports the field room: P13's fade-in anchor waited 71 frames for 0x3038F after battle
            # 203, the PC's fade-in had finished in 0x24, and every later press of the visit ran 60 frames late
            # only a reappearance inside the fade's own recorded visit (no entry between them): a menu room opened after
            # a battle keeps its own room (the first version also moved four menu-room fade-ins of P13)
            for f in list(fadeins):
                before = [h for h in heroes if fade_visit_start[f] <= h <= f and f - h < 600]
                if before and heroes[max(before)] not in (fadeins[f], NO_ROOM):
                    fadeins[f] = heroes[max(before)]
    window_rng = {}  # sitting frame -> Dolphin's random state at the end of that frame
    if a.windows and a.window_seed:
        rdir = a.window_rng or a.windows
        roff = replay_offset(rdir, "--window-rng")
        for r in csv.DictReader(open(os.path.join(rdir, "rng.csv"), encoding="utf-8")):
            window_rng[int(r["frame"]) + roff] = int(r["rng"])
    window_blink = {}  # sitting frame -> [(owner slot, blink timer)] of the owners idle there (--window-seed-blink)
    if a.window_seed_blink:
        # 2026-10-25 (P08's battle 101): a random seed doesn't carry the battle's blink timers, and one owner out
        # of phase changes how many numbers each frame draws (wazaSequenceOwnerUpdateBlink draws once per idle owner)
        # and with it the AI's picks; the owners probe has each owner's state and timer at every label
        woff = replay_offset(a.window_seed_blink, "--window-seed-blink")
        for r in csv.DictReader(open(os.path.join(a.window_seed_blink, "owners.csv"), encoding="utf-8")):
            if r["frame"].startswith("#") or r["blink_state"] != "0" or r["blink_texanim"] == "-1":
                continue
            window_blink.setdefault(int(r["frame"]) + woff, []).append((int(r["slot"]), int(r["blink_timer"])))
    burst_seeds = {}  # sitting frame -> Dolphin's state there (the middle of the quiet stretch before a burst)
    if a.burst_seed:
        boff = replay_offset(a.burst_seed, "--burst-seed")
        rrows = [(int(r["frame"]), int(r["rng"])) for r in
                 csv.DictReader(open(os.path.join(a.burst_seed, "rng.csv"), encoding="utf-8"))]

        def draws(s0, s1, limit=1200):  # LCG steps from s0 to s1 (rng.md), None past the limit
            for n in range(limit + 1):
                if s0 == s1:
                    return n
                s0 = (s0 * 0x343FD + 0x269EC3) & 0xFFFFFFFF
            return None

        quiet = 0
        for i in range(1, len(rrows)):
            if rrows[i][1] == rrows[i - 1][1]:
                quiet += 1
                continue
            if quiet >= 20 and draws(rrows[i - 1][1], rrows[i][1], 999) is None:
                mid = rrows[i - 1 - quiet // 2]
                if a.burst_seed_max is None or len(burst_seeds) < a.burst_seed_max:
                    burst_seeds[mid[0] + boff] = mid[1]
            quiet = 0
    state_frame = {e["name"]: e["f"] for e in events if e.get("ev") == "state"}
    # checkpoints: (movie frame, records so far) from each state's movie copy; the offset: power-on frame of record 0
    checks, offsets = [(0, 0)], []
    if padlog is None:
        for p in sorted(glob.glob(os.path.join(a.sitting, "states", "*.sav.dtm"))):
            s = read_dtm(p)
            name = os.path.basename(p)[:-len(".sav.dtm")]
            checks.append((s["vi"], s["inputs"]))
            if name in state_frame:
                offsets.append(state_frame[name] - s["vi"])
        checks.append((movie["vi"], movie["inputs"]))
        checks = sorted(set(checks))
        if not offsets:
            raise SystemExit("no save state matches the log: can't place the movie in time")
        state_offset = int(statistics.median(offsets))
    else:
        state_offset = 0  # pad.jsonl is already in power-on frames
        checks = [(0, 0), (1, 1)]
    cvi = [c[0] for c in checks]
    pad_frames = [p["f"] for p in padlog] if padlog else []

    def latch_to_rec(raw):
        """10 latched PADStatus bytes -> the movie's 8-byte ControllerState (what() reads that layout): buttons
        regrouped, main / C stick to 0..255 with 0x80 the centre and Y above 0x80 up (the latch keeps Y flipped:
        playrec.py's pc_step), triggers as they are"""
        b = (raw[0] << 8) | raw[1]
        bits0 = [0x1000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0010, 0x0008, 0x0004]  # START A B X Y Z UP DOWN
        bits1 = [0x0001, 0x0002, 0x0040, 0x0020]  # LEFT RIGHT L R
        b0 = sum(1 << i for i, m in enumerate(bits0) if b & m)
        b1 = sum(1 << i for i, m in enumerate(bits1) if b & m)
        s8 = lambda v: v - 256 if v > 127 else v
        sx, sy = (s8(raw[2]) + 128) & 255, (128 - s8(raw[3])) & 255
        cx, cy = (s8(raw[4]) + 128) & 255, (128 - s8(raw[5])) & 255
        return bytes([b0, b1, raw[6], raw[7], sx, sy, cx, cy])

    def poll_at(m):  # the index of the first poll of movie frame m (interpolated between checkpoints)
        i = max(0, min(bisect.bisect_right(cvi, m) - 1, len(checks) - 2))
        (v0, n0), (v1, n1) = checks[i], checks[i + 1]
        return n0 + round((m - v0) * (n1 - n0) / (v1 - v0)) if v1 > v0 else n0

    def pick(m):  # the poll index taken for movie frame m, before the calibration below
        lo, hi = poll_at(m), poll_at(m + 1)
        return lo if a.poll == "first" or hi <= lo else hi - 1

    # with --latched: shift each frame's pick by the poll count (-1, 0 or +1) that makes the movie's stick agree with
    # the stick the console latched, by majority over the frames within POLL_WINDOW. 2026-10-19, sitting 4: its
    # movie has one poll more than two per frame before the 4,444 checkpoint, so interpolating put the pick one poll
    # late from about movie frame 3,617 on (the game kept reading the same poll): the stick came from the probe and was
    # right, but 235 frames had a different A from the one the game read, and one of the Mayor's boxes took a press
    # more than in Dolphin. Per checkpoint interval was tried first: too coarse, the slip starts inside an interval
    POLL_WINDOW = 150
    poll_shift = []  # per movie frame; filled below, once record 0 is placed (rec_for_frame reads it as it grows)

    def rec_for_frame(m):
        if padlog is not None:
            i = bisect.bisect_right(pad_frames, m) - 1
            return latch_to_rec(bytes.fromhex(padlog[i]["p"])) if i >= 0 else bytes([0, 0, 0, 0, 128, 128, 128, 128])
        k = pick(m) + (poll_shift[m] if m < len(poll_shift) else 0)
        return movie["recs"][max(0, min(k, len(movie["recs"]) - 1))]

    # Where record 0 sits (2026-10-19, verified on sitting 2): the save states' "f" lags the movie's own frame
    # count, so the states' median (657) put every press 8 frames early; matching the movie's stick against the game's
    # latched stick in a replay's input probe gives 665 (second poll, 92.3% of frames identical). The match runs on the
    # uncalibrated poll pick (poll_shift still empty); the calibration below needs the offset it finds.
    if padlog is not None:
        offset, how = 0, "pad.jsonl is in power-on frames"
    else:
        offset, how = state_offset + STATE_CORRECTION, f"save states {state_offset} + {STATE_CORRECTION} (measured " \
                                                        f"on sitting 2; no input probe to match)"
        probe = a.input_probe or newest_input_probe(a.sitting)
        if a.offset is not None:
            offset, how = a.offset, "--offset"
        elif probe:
            best = match_offset(probe, a.sitting, events, rec_for_frame, state_offset)
            if best:
                offset, rate, n, sign = best
                how = f"matched against {probe}: {rate:.1%} of {n} moving-stick frames identical (stick Y sign " \
                      f"{sign:+d}; save states said {state_offset})"
    print(f"record 0 at power-on frame {offset}: {how}")

    if latched and padlog is None:
        recs, n = movie["recs"], movie["vi"]
        cum = {dk: [0] * (n + 1) for dk in (-1, 0, 1)}  # prefix sums of the votes for each shift
        for m in range(n):
            lat = probe_latched.get(offset + m + 1 - a.latched_lag)
            k = pick(m)
            vote = ()
            if lat is not None and 1 <= k < len(recs) - 1:
                near = [(recs[j][4], recs[j][5]) for j in (k - 1, k, k + 1)]
                if not near[0] == near[1] == near[2]:  # a steady stick says nothing about which poll the game read
                    want = ((128 + lat[0]) & 0xFF, (128 - lat[1]) & 0xFF)
                    vote = [dk for dk, s in zip((-1, 0, 1), near) if s == want]
            for dk in cum:
                cum[dk][m + 1] = cum[dk][m] + (dk in vote)
        for m in range(n):
            lo, hi = max(0, m - POLL_WINDOW), min(n, m + POLL_WINDOW + 1)
            v = {dk: cum[dk][hi] - cum[dk][lo] for dk in cum}
            poll_shift.append(max(v, key=lambda dk: (v[dk], dk == 0)))

    entries = [(e["f"], e["room"]) for e in events if e.get("ev") == "entered"]
    entry_lag_rooms = {int(x, 16) for x in a.entry_lag_rooms.split(",")} if a.entry_lag_rooms else set()
    entry_lagged = set()    # rooms whose first visit already got --entry-lag (only the first visit gets it)
    entry_lag_every = {int(x, 16) for x in a.entry_lag_every.split(",")} if a.entry_lag_every else set()
    entry_rng = {(e["f"], e["room"]): e.get("rng") for e in events if e.get("ev") == "entered"}
    press_lags = []      # [frame, lag, done]: --press-lag (2026-10-26)
    if a.press_lag:
        want_pl, seen_pl = {}, {}
        for x in a.press_lag.split(","):
            if x:
                head, _, rest = x.partition("@")
                room, _, visit = head.partition(":")
                at, _, lag = rest.partition(":")
                want_pl[(int(room, 16), int(visit))] = (int(at), int(lag))
        for ef, room in entries:
            seen_pl[room] = seen_pl.get(room, 0) + 1
            got = want_pl.get((room, seen_pl[room]))
            if got:
                press_lags.append([ef + got[0], got[1], False])
                want_pl.pop((room, seen_pl[room]))
        for (room, visit) in want_pl:
            print(f"warning: --press-lag {room:#x}:{visit}: the recording has no such visit, ignored")
    press_lag_back = 0   # frames the next relative step gives back after a lagged press
    lines = [f"# generated by tools/local/testing/dtm_to_pad.py from {os.path.basename(os.path.normpath(a.sitting))}"
             f"/{'pad.jsonl' if padlog is not None else 'sitting.dtm'} ({movie['vi']} frames, {movie['inputs']} "
             f"{'pad changes' if padlog is not None else 'polls'}, record 0 at power-on frame {offset},"
             f" poll {a.poll}{', reseeded at every room entry' if a.reseed else ''}"
             f"{f', stick from {os.path.basename(os.path.normpath(a.latched))}/input.csv (lag {a.latched_lag})' if a.latched else ''}"
             f"{f', presses timed from window openings ({sum(map(len, windows.values()))}, lag {a.window_lag})' if a.windows else ''})"]
    # the pad starts released: no leading "-" step. One at the movie's absolute start frame made the first room-keyed
    # step wait for a later entry on the PC, which reaches the title sooner (frame 637 vs Dolphin's 1,111): the script
    # then waited through the attract demo (2026-10-18, verified)
    # the 60 frames before each --fadein-return-rooms reappearance: stick-only steps there are dropped (the stick held at
    # the reappearance is written right after its anchor). The console's player tilts the stick a few frames before the
    # field is back, which moves nothing, but those steps count from the menu room's last window, so on the PC (whose
    # reload is ~11 frames shorter) they fired after the reappearance and held its anchor back 12 frames (sitting 40,
    # p2_S40k): an anchor arms only after the step before it
    pre_return = {g for h in return_anchors if h in heroes for g in range(h - 60, h)}

    def stick_only(w, before):
        keys = ("SX=", "SY=", "CX=", "CY=")
        return [t for t in w.split("+") if not t.startswith(keys) and t != "-"] == \
            [t for t in before.split("+") if not t.startswith(keys) and t != "-"]

    def return_frameat(f):
        # --window-frameat at a --fadein-return-rooms anchor, as at a window anchor (the frame after the event's, counted
        # from the line's first row): without it the field's frames after a menu room kept the frame lengths' phase from
        # the menu room's last window, which the PC's shorter reload had shifted, so 2-refresh frames fell inside the walk
        # (sitting 40, p2_S40j: the stick ramp took 34 frames on the PC against the console's 25)
        if not fa_rows:
            return []
        g = f - fa_off
        i = bisect.bisect_right(fa_entered, g) - 1
        if i < 0:
            return []
        return [f"+0:!frameat {bisect.bisect_right(fa_rows, g) - bisect.bisect_left(fa_rows, fa_entered[i])}"]

    prev, prev_f, steps, used = "-", None, 0, set()
    after_window = False  # the next relative step is the first after a window anchor
    room_window_next = False  # that anchor came from --windows-rooms only
    window_lag_next = None    # that anchor's --window-lag-rooms lag, if any
    hero_lag_next = None  # set when that anchor was a reappearance (--hero-lag instead of --window-lag)
    after_seed = False    # the last step written is a "@R+0:!seed" step
    for m in range(movie["vi"]):
        f = offset + m
        for wid in windows.get(f, ()):
            # an anchor step at each menu opening: re-states the pad (no change), so the presses after it count from
            # the opening. "@X+0" fires the frame after the opening on the PC (as after a room entry), hence the -1 on
            # the next step below
            # "~G": if the PC doesn't open this window, fire anyway G frames after the previous step (the recorded gap
            # plus 30; snatcherLeo's option A, battle-tolerant replays)
            gap = (f - prev_f if prev_f is not None else 0) + 30
            lines.append(f"@{hex(0x20000 | (wid & 0xFFFF))}+0~{gap}:{prev}")
            prev_f, after_window = f, True
            room_window_next = f in room_window_frames
            window_lag_next = window_lag_at.get(f)
        if windows.get(f) and fa_rows:
            # --window-frameat: the frame after the opening's (the anchor fires on the PC's next frame), counted from
            # the line's first row, the first at or after the room entry (frames_to_pc.py's split)
            g = f - fa_off
            i = bisect.bisect_right(fa_entered, g) - 1
            if i >= 0:
                lines.append(f"+0:!frameat {bisect.bisect_right(fa_rows, g) - bisect.bisect_left(fa_rows, fa_entered[i])}")
        if f in heroes:
            # the fallback: 60 frames past the recorded gap, but after a battle the player always comes back, and the
            # PC's battle can run far longer than the console's (sitting 9's Folly: ~1,470 frames; the 60 + gap
            # fallback fired ~1,000 frames before Wes reappeared, and the talk's presses ran from there, 2026-10-21)
            gap = (f - prev_f if prev_f is not None else 0) + (3600 if f in battle_heroes else 60)
            if f in battle_heroes and a.battle_mash and prev_f is not None:
                # battle-tolerant (snatcherLeo's option A): from the frame the console's player reappeared, tap A until the
                # PC's does, for boxes the PC shows that the recording has no presses for (a level-up after other
                # damage rolls); the reappearance anchor then counts its fallback from here
                lines.append(f"+{f - prev_f}:MASH")
                prev_f, gap = f, 3600
            lines.append(f"@{hex(0x10000 | heroes[f])}+0~{gap}:{prev}")
            prev_f, after_window = f, True
            hero_lag_next = a.hero_lag
            if f in return_anchors:
                lines.extend(return_frameat(f))
            if f in battle_heroes and window_rng.get(f + a.window_seed_shift) is not None:
                # the battle left the random numbers a little apart from Dolphin's (option A); Phenac's NPCs wander
                # with them, so the walk after the battle starts from Dolphin's state
                lines.append(f"+0:!seed 0x{window_rng[f + a.window_seed_shift]:08X}")
        if f in fadeins:
            # the room's fade-in finished: the presses after it count from here (fallback: the recorded gap + 60, as
            # for a reappearance in the field)
            gap = (f - prev_f if prev_f is not None else 0) + 60
            lines.append(f"@{hex(0x30000 | (fadeins[f] & 0xFFFF))}+0~{gap}:{prev}")
            prev_f, after_window = f, True
            hero_lag_next = a.fade_lag
            if f in return_anchors:
                lines.extend(return_frameat(f))
        span = next((sp for sp in seed_spans if sp[0] <= f <= sp[1]), None)
        resync = span is not None
        shift = span[2] if span is not None else a.window_seed_shift
        skip_ids = {int(x, 16) for x in a.window_seed_skip_ids.split(",") if x}
        only_ids = {int(x, 16) for x in a.window_seed_only_ids.split(",") if x}
        if windows.get(f) and (not a.no_battle_window_seed or resync) and \
                any(w not in skip_ids for w in windows[f]) and \
                (not only_ids or any(w in only_ids for w in windows[f])) and window_rng.get(f + shift) is not None:
            # once per frame with openings: Dolphin's state at the end of the opening's frame (the anchor step fires at
            # the PC's next frame boundary; --window-seed-shift, or the span's own "~SHIFT", calibrates the labels)
            state = window_rng[f + shift]
            # the state before the opening's frame (`prev_rng`; it was `prev`, which also holds the pad state written
            # last, so a seed step could leave a random state there for the next anchor's pad field, 2026-10-29)
            prev_rng = window_rng.get(f + shift - 1)
            keep_after = None
            if a.window_seed_burst and prev_rng is not None:
                # --window-seed-burst N: if that frame drew more than N numbers (a particle burst), seed the state from
                # before it: the PC makes the burst itself after the seed (P05: a seed at the end of a 95-draw burst
                # put the PC ~90 draws ahead and changed a Hyper Mode roll)
                s, n = prev_rng, 0
                while s != state and n <= a.window_seed_burst:
                    s = (s * 214013 + 2531011) & 0xFFFFFFFF
                    n += 1
                if s != state:
                    keep_after = state  # the state after the burst: a PC already there made the burst itself
                    state = prev_rng
            keep = []
            if a.window_seed_keep and prev_rng is not None and state == prev_rng and keep_after is not None:
                keep.append(keep_after)
            for k in range(-(a.window_seed_keep - 1), a.window_seed_keep) if a.window_seed_keep > 1 else ():
                v = window_rng.get(f + shift + k)
                if v is not None and v != state and v not in keep:
                    keep.append(v)
            # '/' between the states: a pad script ends a step at ','
            lines.append(f"+0:!seed 0x{state:08X}" + (" keep " + "/".join(f"0x{v:08X}" for v in keep) if keep else ""))
            if window_blink.get(f + shift):
                lines.append("+0:!blink " + " ".join(f"{s}:{t}" for s, t in window_blink[f + shift]))
        if f in burst_seeds and prev_f is not None:
            # a plain "+N" step: it fires N frames after the previous step, so the press after it keeps its distance
            lines.append(f"+{max(0, f - prev_f - (1 if after_seed else 0))}:!seed 0x{burst_seeds[f]:08X}")
            prev_f, after_seed = f, False
        rec = rec_for_frame(m)
        lat = latched.get(f + 1 - a.latched_lag)
        if lat is not None:
            # the stick the console's game latched (SX = 128 + x, SY = 128 - y: the latch's y points the other way)
            rec = rec[:4] + bytes([(128 + lat[0]) & 0xFF, (128 - lat[1]) & 0xFF]) + rec[6:]
        btn = read_buttons.get(f + 1 - a.latched_lag)
        if btn is not None:
            # the PADStatus button word regrouped into the movie's first two bytes (latch_to_rec's layout)
            b0 = sum(1 << i for i, m in enumerate((0x1000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0010, 0x0008, 0x0004))
                     if btn & m)
            b1 = sum(1 << i for i, m in enumerate((0x0001, 0x0002, 0x0040, 0x0020)) if btn & m)
            rec = bytes([b0, b1]) + rec[2:]
        w = what(rec)
        if a.reseed:
            # --reseed: at each room entry, Dolphin's RNG state there ("!seed", needs ORRE_DEBUG=1), so every room starts
            # in step with the recording and a drift can't carry over from one room into the next (heavy scenes run at
            # 30 fps on the console: gamecube-accuracy.md row 4b)
            for ent in entries:
                if ent[0] == f and ent not in used and ent[1] != NO_ROOM and entry_rng.get(ent) is not None:
                    # a menu opened mid-battle: fire anyway 60 frames past the recorded gap if the PC's battle never
                    # opens it ("~G", as the window anchors)
                    fb = f"~{(f - prev_f if prev_f is not None else 0) + 60}" if in_battle_menu(ent) else ""
                    lines.append(f"@{hex(ent[1])}+0{fb}:{prev}" if a.reseed_noop
                                 else f"@{hex(ent[1])}+0{fb}:!seed 0x{entry_rng[ent]:08X}")
                    used.add(ent)
                    prev_f = f
                    after_seed = True
        if w == prev:
            continue
        if f in pre_return and stick_only(w, prev):
            continue
        last = [e for e in entries if e[0] <= f]
        if prev_f is None and not last:
            when = str(f)
        elif last and last[-1][1] != NO_ROOM and last[-1] not in used and (prev_f is None or last[-1][0] > prev_f):
            when = f"@{hex(last[-1][1])}+{f - last[-1][0]}"
            if in_battle_menu(last[-1]):
                when += f"~{(f - prev_f if prev_f is not None else 0) + 60}"  # as the in-battle reseed above
            used.add(last[-1])
        elif after_window:
            # a window / reappearance anchor after the seed step takes over: the press counts from the anchor
            wlag = a.window_lag
            if window_lag_next is not None:
                wlag = window_lag_next
            if room_window_next and a.windows_rooms_lag is not None:
                wlag = a.windows_rooms_lag
            when = f"+{max(0, f - prev_f - 1 + (wlag if hero_lag_next is None else hero_lag_next))}"
            hero_lag_next = None
        elif after_seed:
            # the first press after a seed step: "@R+0" fires at entry + 1 (the entry happens mid-frame, after that
            # frame's step check), while "@R+N" with N >= 1 fires at entry + N, so "+N" after the seed would land one
            # frame late; N - 1 (never below 0) presses on the recording's frame (2026-10-19, verified from
            # pc_pad_script.c and the S02 log: Phenac's first press came at entry + 4 for the recording's entry + 3).
            # --entry-lag (2026-10-20): frames added back. Wes's name screen (P01 room 0x390) needs 1: with the -1
            # its presses came a frame early (the text box, list close and fades one frame ahead of the background
            # particles), so the New Game burst rolled other starters; with +1 the starters are the console's (88 / 75)
            # (2026-10-24) the first visit only: P01's second visit to 0x390 with the same +1 left the replay
            # stuck at frame 41,420 waiting for room 0x4 (with the first visit only it reaches all 16 rooms, verified)
            room = last[-1][1] if last else None
            lag = a.entry_lag if (not entry_lag_rooms and not entry_lag_every) or room in entry_lag_every or (
                room in entry_lag_rooms and room not in entry_lagged) else 0
            if lag and room in entry_lag_rooms:
                entry_lagged.add(room)
            if a.entry_lag_nofade and last and last[-1][0] in nofade_visits:
                # a visit with no fade-in to anchor to (2026-10-26): its presses count from the room entry, which
                # the PC's hook marks 2 refreshes before the recorder's "entered" (P02's fade-less world-map visits
                # each left 2 game frames early: 600 PC frames against the console's 602, every frame on time)
                lag = max(lag, a.entry_lag_nofade)
            when = f"+{max(0, f - prev_f - 1 + lag)}"
        else:
            when = f"+{f - prev_f}" if prev_f is not None else str(f)
        if press_lag_back:
            # the step after a --press-lag press keeps its recorded time (an anchor step re-times by itself)
            if when.startswith("+"):
                when = f"+{max(0, int(when[1:]) - press_lag_back)}"
            press_lag_back = 0
        for pl in press_lags:
            if not pl[2] and f >= pl[0]:
                pl[2] = True
                m_pl = re.match(r"^(.*?\+)(\d+)(.*)$", when)
                if m_pl:
                    when = f"{m_pl.group(1)}{int(m_pl.group(2)) + pl[1]}{m_pl.group(3)}"
                    press_lag_back = pl[1]
                else:
                    print(f"warning: --press-lag at recorded frame {pl[0]}: step '{when}' has no +N, not lagged")
        lines.append(f"{when}:{w}")
        if cs_rooms and "A" in w.split("+") and "A" not in prev.split("+"):
            # --cursor-sync: the console's cursors just before this A (same frame as the press, before the game reads it)
            j = bisect.bisect_right([ef for ef, _ in cs_ent], f) - 1
            if j >= 0 and (cs_all or cs_ent[j][1] in cs_rooms):
                step = cursor_step(f, cs_ent[j][0])
                if step:
                    lines.append(step)
        if fa_rows and press_fa_rooms:
            # --press-frameat: the frame lengths re-anchored at each press too (rooms whose presses count from the
            # entry, with no window anchors: P09's box visit), the index of the console's frame the press is read in
            # (the frame containing it; a window anchor's is the frame after the opening's, above). With the frame
            # after it, P09's box countdown crossed the console's 1-retrace frames a frame late and lost a repeat
            g = f - fa_off
            i = bisect.bisect_right(fa_entered, g) - 1
            if i >= 0 and ((fa_rooms[i], None) in press_fa_rooms or (fa_rooms[i], fa_visits[i]) in press_fa_rooms):
                k = bisect.bisect_right(fa_rows, g) - 1 - bisect.bisect_left(fa_rows, fa_entered[i])
                lines.append(f"+0:!frameat {max(0, k)}")
        prev, prev_f, steps, after_window, after_seed = w, f, steps + 1, False, False
    for pl in press_lags:
        if not pl[2]:
            print(f"warning: --press-lag at recorded frame {pl[0]}: no button change after it, not lagged")
    open(a.out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print(f"{a.out}: {steps} steps; record 0 at power-on frame {offset} (states: {sorted(offsets)})"
          f"{'; poll shifts (movie frames): ' + str({dk: poll_shift.count(dk) for dk in (-1, 0, 1)}) if poll_shift else ''}"
          f"{'; game reads (stick): ' + str(read_counts) if read_counts else ''}")
    if a.report:
        print("checkpoints (movie frame, polls):", checks)


if __name__ == "__main__":
    main()
