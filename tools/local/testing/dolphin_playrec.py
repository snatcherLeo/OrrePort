"""The playthrough recorder, Dolphin side (2026-10-18). Runs inside Felk's Dolphin scripting fork; started by
tools/local/testing/playrec.py, never by hand (the launcher sets the environment below and copies the settings and the
memory card into the sitting folder first).

Two modes (PLAYREC_MODE):
- **record**: snatcherLeo plays; nothing is written to the pad. Every frame the game's own copy of controller 1 (InputPad
  slot 0's latched PADStatus, lbl_80401C10 + 0x18: buttons, both sticks, both triggers, analog A / B) is logged on
  every change (pad.jsonl), so the stretch can be replayed exactly. events.jsonl gets what the game did (below);
  states/ gets a save state at the first entry of every room, at every battle start, every 5 minutes and at each mark;
  blocks/ gets the live save block (all 17 sections: party, PC boxes, bag, money, flags, Shadow state) at every room
  entry, zlib-compressed, decodable with tools/local/colo/savefile.py's readers.
- **replay**: the same start (state or power-on with the copied card), then the recorded pad is sent back frame by
  frame; every frame the pad the game actually read is compared with the recording (replay_check.jsonl: the first
  mismatches), and the same events.jsonl is written, so playrec.py check can compare the two event logs.

- **watch**: log only (replay_events.jsonl), touching nothing: for checking a Dolphin movie (.dtm) played back with
  `-m`, the exact replay of a sitting recorded with Dolphin's own Movie -> Start Recording (it stores the controller
  at every poll; this script's own pad log keeps one value per frame, which proved not enough: sitting 1, 2026-10-18).

Button combos (record mode; the game sees them too, which is harmless and replays the same):
- L + R + Z: **mark** (a numbered event, a save state; "something odd here"), with an on-screen message.
- L + R + START: **end the sitting** (an end state, then Dolphin closes).

Events (one JSON line each, "f" = frame since the script started): room changes and entries, battles (idlog.py's
ids.jsonl as well), story-flag bits set or cleared, message windows opening / closing (type, gWinMsgCurrentType), the
two music / sound slots (lbl_8047B0A8 / lbl_8047B0AC, set by soundPlay), the battle result, the Pokémon on the field
(species, HP, moves, PP) with HP and PP changes (a PP drop = the move used), the player's position, the RNG state, and
marks. Addresses: idlog.py, message-windows.md, global-state.md, the scratchpad logger of 2026-10-18 (live.py).
"""
import json, os, sys, time, zlib
from dolphin import controller, event, memory, savestate
try:
    from dolphin import gui
except Exception:
    gui = None

OUT = os.environ["PLAYREC_OUT"]
MODE = os.environ.get("PLAYREC_MODE", "record")
START_STATE = os.environ.get("PLAYREC_STATE", "")
PAD_IN = os.environ.get("PLAYREC_PAD", "")             # replay: the recorded pad.jsonl
SHIFT = int(os.environ.get("PLAYREC_SHIFT", "1"))      # replay: send frame f's pad this many frames early
PROBES = set(filter(None, os.environ.get("PLAYREC_PROBES", "").split(",")))   # watch mode extras: camera, rng
WATCH_FRAMES = int(os.environ.get("PLAYREC_FRAMES", "0"))  # watch mode: stop after this many frames (0 = never)
DRIVE = MODE == "record" and bool(PAD_IN)             # test only: a scripted "player" (PLAYREC_PAD) while recording
PERIODIC = int(os.environ.get("PLAYREC_PERIODIC", str(5 * 60 * 60)))   # frames between periodic states
sys.path.insert(0, os.environ["TESTING_DIR"])           # set by playrec.py (the fork defines no __file__)
import idlog

for d in ("states", "blocks"):
    os.makedirs(os.path.join(OUT, d), exist_ok=True)
ev_log = open(os.path.join(OUT, "events.jsonl" if MODE == "record" else "replay_events.jsonl"), "w", buffering=1)
pad_log = open(os.path.join(OUT, "pad.jsonl"), "w", buffering=1) if MODE == "record" else None
chk_log = open(os.path.join(OUT, "replay_check.jsonl"), "w", buffering=1) if MODE == "replay" else None

LATCHED = 0x80401C10 + 0x18
ROOM = 0x80478B18
WINMSG_TYPE, WINMSG_ERROR = 0x80478B30, 0x8047AD10
SOUND_A, SOUND_B = 0x8047B0A8, 0x8047B0AC
FIGHT_RESULT = 0x8047B5D8
FIGHT_FLOOR = 0x8046DD90
SAVE_PTR, BLOCK_SIZE = 0x8047ADB8, 0x1DFD0
PEOPLE_PTR, PEOPLE_MAX, ENTRY = 0x8047B200, 0x8047B1F8, 0xDC
# the npc probe's window (PLAYREC_NPC="room,group,frames[,visit]"; default Phenac 0x3, group 31, 800 frames, the
# first visit)
_npc = [int(v, 0) for v in os.environ.get("PLAYREC_NPC", "0x3,31,800").split(",")]
NPC_ROOM, NPC_GROUP, NPC_FRAMES = _npc[:3]
# PLAYREC_FADETASK=lo-hi: the fadetask probe's label window (2026-10-26; run_probes)
_ft = os.environ.get("PLAYREC_FADETASK", "")
FADETASK = tuple(int(x) for x in _ft.split("-")) if "-" in _ft else None
# PLAYREC_RNGCALL=lo-hi: the rngcall probe's label window (2026-10-26; run_probes arms its watch only there)
_rc = os.environ.get("PLAYREC_RNGCALL", "")
# several windows "a-b,c-d" (2026-10-27, flagged for D: every battle of a sitting in one replay, the field
# between them left out)
RNGCALL = [tuple(int(x) for x in w.split("-")) for w in _rc.split(",") if "-" in w] or None
# PLAYREC_WATCH=addr,...: more memory watches inside that window, logged to fadewatch.csv (2026-10-26)
WATCH_ADDRS = [int(x, 16) for x in os.environ.get("PLAYREC_WATCH", "").split(",") if x]
# PLAYREC_PEEK=lo-hi:addr,...: the 32-bit words at these addresses once per label inside the window, to peek.csv
# (2026-10-27, flagged for D: the idle battle camera's flags, timer and owner in P08's cup battle)
_pk = os.environ.get("PLAYREC_PEEK", "")
PEEK = (tuple(int(x) for x in _pk.split(":")[0].split("-")),
        [int(x, 16) for x in _pk.split(":")[1].split(",") if x]) if ":" in _pk else None
NPC_VISIT = _npc[3] if len(_npc) > 3 else 1
NPC_FROM = int(os.environ.get("PLAYREC_NPC_FROM", "0") or 0)   # start the npc probe at this frame instead
# the owners probe's window (PLAYREC_OWNERS="room,frames[,visit]"; default the battle room 0xCA, 2000 frames, the
# first visit; 2026-10-25: P03's peon battle is room 0xCC's second visit; visit 0 = every visit)
_own = [int(v, 0) for v in os.environ.get("PLAYREC_OWNERS", "0xCA,2000").split(",")]
OWN_ROOM, OWN_FRAMES = _own[:2]
OWN_VISIT = _own[2] if len(_own) > 2 else 1
RNG_PTR = 0x80478C94
FIGHT_THREAD = 0x8047B5D0   # lbl_8047B5D0: the fight thread's GSthread, set in _fightInitialize, 0 at the end
FIGHT_FLAG = 0x8047B5DA     # lbl_8047B5DA: 1 from _fightInitialize's start, 0 in fn_801EF5C0 at the battle's end

BTN = {"Left": 0x0001, "Right": 0x0002, "Down": 0x0004, "Up": 0x0008, "Z": 0x0010, "R": 0x0020, "L": 0x0040,
       "A": 0x0100, "B": 0x0200, "X": 0x0400, "Y": 0x0800, "Start": 0x1000}
MARK, END = 0x0010 | 0x0020 | 0x0040, 0x1000 | 0x0020 | 0x0040
# value to send so the game reads a given main-stick value (the fork's measured mapping, dolphin_slice.py; the table
# is against the latched copy, the same convention recorded here)
STICK_X_SEND = {(128 * (1 + (v & 1)) - v) & 255: v for v in range(256)}
STICK_Y_SEND = {(v if v & 1 else (v + 128) & 255): v for v in range(256)}

frame = 0
t0 = time.time()
st = {"started": False, "pad": None, "combo": 0, "marks": 0, "end_at": None, "rooms_seen": set(), "entries": 0,
      "winmsg": None, "snd": None, "result": None, "flags": None, "field": None, "last_state": 0, "replay": {},
      "replay_last": 0, "mismatch": 0, "battle": None}


def w(ev, **kw):
    kw = dict(f=frame, ev=ev, **kw)
    kw["t"] = round(time.time() - t0, 1)
    ev_log.write(json.dumps(kw) + "\n")
    if LIVEPAD:
        livepad_event(ev, kw)


# PLAYREC_LIVEPAD=<steps.json> (2026-11-02; the reference playthrough's Dolphin side, the closed loop agreed with Main): the PC's pad,
# fed live. livepad_steps.py ties every PC pad change to the last PC event before it (room entered, message window
# opening / closing, battle message) with its delay; here each step fires that many frames after Dolphin shows the
# same event, so the presses follow Dolphin's own timing (P01's precomputed movies didn't converge: Dolphin is ready
# for a press later than the PC after a window opens, likely). Dolphin's events are matched to the PC's in order, with
# a lookahead of 3 (a PC event Dolphin doesn't show is skipped; an extra Dolphin event is ignored). When the next PC
# event is overdue (its PC distance after the last one, plus 20 frames), the group's last new press is sent again
# for 3 frames, every 30 frames, up to 10 times: a press that came too early and was ignored. livepad.csv logs
# every match, skip and re-press.
LIVEPAD = os.environ.get("PLAYREC_LIVEPAD", "")
LP = {}
if LIVEPAD:
    PROBES.add("menu")    # the live pad's menu events come from the menu probe (a run without it stalls at the boot)
    _lp = json.load(open(LIVEPAD))
    LP["anchors"] = [tuple(a) for a in _lp["anchors"]]
    LP["by"] = {}
    for k, d, h in _lp["steps"]:
        LP["by"].setdefault(k, []).append((d, bytes.fromhex(h)))
    # *Added 2026-11-05 (P07 replay_2 / 3):* in the bag (menu 89) the console's item list moves about 3 frames after
    # a D-pad press where the PC's moves after 2 (P06's fast scroll: PC +2 on every tap, console +5 / +6; the other
    # menus show no such lag in the same feed, so likely the game, not the feed: to settle on a clean movie replay).
    # Taps still land, but a held direction's auto-repeat fits one step fewer: P07's 32-frame Down (PC 105,002-105,034)
    # gave the PC 5 steps (to Awakening, then Up to Parlyz Heal) and the console 4 (Up to Ice Heal: "It's meaningless
    # to use that item", the stall at 105,263). So in a bag group, from the release of a D-pad hold of 16 frames or
    # more, the steps go PLAYREC_LIVEPAD_BAGHOLD frames later (default 3; 0 turns it off); logged once per group
    # (the battle bag only: menu 89 is the field bag too, and field-bag holds already replay right, e.g. P06 at 2,454
    # and P07 at 87,105. A group counts as in battle when the last of menus 241 (the battle command menu) and 65 (the
    # pause menu) before it is 241; the room event 911 comes before both bags, so it can't tell them apart)
    bag_extra = int(os.environ.get("PLAYREC_LIVEPAD_BAGHOLD", "3"))
    LP["bagshift"] = {}
    _last, _in_battle = None, set()
    for _k, _a in enumerate(LP["anchors"]):
        if _a[0] == "menu" and str(_a[1]).split(":")[0] in ("241", "65"):
            _last = str(_a[1]).split(":")[0]
        if _last == "241":
            _in_battle.add(_k)
    # (the loop variable must not be called st: that is the script's global state dict, and rebinding it here at
    # module level broke every later frame silently; P07 run 4 and P06 run 8 ran with no input, 2026-11-05)
    for k, _steps in LP["by"].items():
        if bag_extra <= 0 or k not in _in_battle or LP["anchors"][k][0] != "menu" \
                or not str(LP["anchors"][k][1]).startswith("89:"):
            continue
        held_from, shift, out = {}, 0, []
        for d, raw in _steps:
            dpad = raw[1] & 0x0F
            for bit in (1, 2, 4, 8):
                if dpad & bit and bit not in held_from:
                    held_from[bit] = d
                elif not dpad & bit and bit in held_from:
                    if not shift and d - held_from[bit] >= 16:
                        shift = bag_extra
                        LP["bagshift"][k] = (d, bag_extra)
                    del held_from[bit]
            out.append((d + shift, raw))
        LP["by"][k] = out
    if LP["bagshift"]:
        print("livepad: bag holds released later: %s" % ", ".join(
            "group %d (PC %d) from +%d by %d" % (k, LP["anchors"][k][2], d, n) for k, (d, n) in
            sorted(LP["bagshift"].items())), flush=True)
    LP["boot"] = [(g, bytes.fromhex(h)) for g, h in _lp["boot"]]
    LP["path"] = {}
    for k, d, x, z, rot in _lp.get("path", []):
        LP["path"].setdefault(k, []).append((d, x, z, rot))
    LP["k"], LP["at"], LP["i"], LP["pad"] = -1, 0, 0, bytes(10)
    LP["retry"] = 0
LP_HDR = "frame,what,index,kind,key,pc_frame,detail"
LP_LAG = int(os.environ.get("PLAYREC_LIVEPAD_LAG", "-1"))


def livepad_event(ev, kw):
    """a Dolphin event: if it is the next PC anchor (or one of the 3 after), that anchor's steps start now"""
    if ev in ("entered", "reentered"):
        key = ("room", kw.get("room"))
    elif ev == "msgwin":
        key = ("window", kw.get("type"))
    elif ev == "battle_msg":
        key = ("battle message", kw.get("id"))
    elif ev == "menu":
        key = ("menu", kw.get("key"))
    else:
        return
    an = LP["anchors"]
    # an anchor skipped at the last match with the same PC frame as it, now seen: the two sides logged one frame's
    # events in another order (P01 live run 4: the PC logged menu 64 opening then window 3, Dolphin the other way, and
    # the menu event matched the NEXT "64:p2", 1,800 frames on, skipping that window's presses); it counts as that
    # anchor and moves nothing on
    for s in LP.get("pending", []):
        if (an[s][0], an[s][1]) == key:
            LP["pending"].remove(s)
            probe_out("livepad.csv", LP_HDR).write("%d,late,%d,%s,%s,%d,\n" % (frame, s, an[s][0], an[s][1], an[s][2]))
            return
    for j in range(LP["k"] + 1, min(LP["k"] + 9, len(an))):   # lookahead 8 (menu changes can differ in detail)
        if (an[j][0], an[j][1]) == key:
            for s in range(LP["k"] + 1, j):
                probe_out("livepad.csv", LP_HDR).write("%d,skip,%d,%s,%s,%d,\n" % (frame, s, an[s][0], an[s][1],
                                                                                 an[s][2]))
            # the steps of the anchors skipped over still come first (their last state carries)
            for s in range(LP["k"] + 1, j):
                for _, raw in LP["by"].get(s, []):
                    LP["pad"] = raw
            # (earlier pending anchors of the same PC frame stay pending: P07 replay_6, 2026-11-05, matched 241:p2 and
            # then 243:p2 of one frame's group, and the second match dropped the 75:c0 / 75:p1 the first had left
            # pending, so the console's menu 75 matched the PC's NEXT visit, 9,600 frames on, and stalled there)
            LP["pending"] = [s for s in LP.get("pending", []) if an[s][2] == an[j][2]] + \
                [s for s in range(LP["k"] + 1, j) if an[s][2] == an[j][2]]
            LP["k"], LP["at"], LP["i"], LP["retry"] = j, frame, 0, 0
            # after a cursor change, the rest of the stick hold goes neutral until the PC's stick returns to centre:
            # on the PC it moved the cursor no further (else another event), while in Dolphin the cursor can get
            # there earlier in the hold, and the rest of it moves on (P01 run 2: menu 99 reached 6, then went to 1)
            LP["calm"] = an[j][0] == "menu" and ":c" in str(an[j][1])
            probe_out("livepad.csv", LP_HDR).write("%d,match,%d,%s,%s,%d,\n" % (frame, j, an[j][0], an[j][1], an[j][2]))
            return
    probe_out("livepad.csv", LP_HDR).write("%d,extra,,%s,%s,,\n" % (frame, key[0], key[1]))


def livepad_steer(k):
    """on a walk, the pad with its stick turned towards where the PC's Wes was 10 frames on (None: no steering).
    Only while the PC's Wes moved there (a known stick-to-world turn) and Dolphin's is more than 1.5 units off; the
    stick keeps the PC's strength (livepad_steps.py "path")"""
    import math
    raw = LP["pad"]
    sx, sy = (raw[2] ^ 0x80) - 0x80, -((raw[3] ^ 0x80) - 0x80)   # back to the PC's own sign
    mag = math.hypot(sx, sy)
    path = LP["path"].get(k)
    if mag < 20 or not path:
        return None
    # *Changed 2026-11-02 (P01 run 8):* steering 10 frames ahead and replacing the stick's direction ran away (off 17,
    # then 30 units). Now the target is the PC's position at the same moment, steering starts only past 3 units, and
    # the PC's own stick is turned by at most 30 degrees towards it: a nudge, not a takeover
    # *Changed 2026-11-03 (P02: one 950-frame run ending 16 units off its talk):* follow the PC's line, not its clock.
    # The nearest PC path sample within 60 frames of now is where Dolphin's Wes is on the PC's line; the target is 2
    # samples (20 frames) further along. Only while Dolphin's Wes is moving (P01's runaways came from the two standing
    # still for different times) and more than 1.5 units off the line
    t = frame - LP["at"]
    cur = player_pos()
    if not cur:
        return None
    last = st.get("lp_lastpos")
    st["lp_lastpos"] = cur
    if not last or math.hypot(cur[0] - last[0], cur[2] - last[2]) < 0.05:
        return None
    near = [i for i, p in enumerate(path) if abs(p[0] - t) <= 60]
    if not near:
        return None
    i = min(near, key=lambda j: math.hypot(path[j][1] - cur[0], path[j][2] - cur[2]))
    off = math.hypot(path[i][1] - cur[0], path[i][2] - cur[2])
    tgt = path[min(i + 2, len(path) - 1)]
    rot = tgt[3] if tgt[3] is not None else path[i][3]
    if off < 1.5 or rot is None:
        return None
    dx, dz = tgt[1] - cur[0], tgt[2] - cur[2]
    want = math.atan2(dz, dx) - rot
    have = math.atan2(sy, sx)
    turn = (want - have + math.pi) % (2 * math.pi) - math.pi
    ang = have + max(-math.pi / 6, min(math.pi / 6, turn))
    nx, ny = int(round(mag * math.cos(ang))), int(round(mag * math.sin(ang)))
    nx, ny = max(-127, min(127, nx)), max(-127, min(127, ny))
    if frame % 30 == 0:
        probe_out("livepad.csv", LP_HDR).write("%d,steer,%d,,,%d,off %.1f\n" % (frame, k, LP["anchors"][k][2],
                                                                            math.hypot(dx, dz)))
    return raw[:2] + bytes([nx & 255, (-ny) & 255]) + raw[4:]


LP_STALL = int(os.environ.get("PLAYREC_LIVEPAD_STALL", "1800"))
LP_DIAG = ([int(x) for x in os.environ["PLAYREC_INJECT_DIAG"].split("-", 1)]
           if os.environ.get("PLAYREC_INJECT_DIAG") else [-1, -1])   # from-to: log every draw there (diagnostic)


def lp_tilted(raw):
    """the main stick pushed past 20 (a resting stick reads 1 or 2, not 0: P02 had no exact-0 release at all)"""
    import math
    return math.hypot((raw[2] ^ 0x80) - 0x80, (raw[3] ^ 0x80) - 0x80) >= 20


def livepad_hold(k, delay, raw):
    """True to keep the stick held one more frame instead of applying this step, which lets the stick go: Wes walks
    the PC's line but a few frames later (P02: about 6 units behind along the path, 16 short at the talk at the PC's
    10,920, which then never opened), so the release waits while Dolphin's Wes is more than 2 units short of where the
    PC's stood once he stopped and is still closing in, for at most 60 frames. The PC's own stick direction is kept
    (unlike steering, which made P01 worse)"""
    import math
    # a stick release, or (*added 2026-11-03, P02*) a new button press while the stick is still pushed: P02's
    # 950-frame run ends with A while running, and Dolphin's Wes is on the PC's line but behind along it (the
    # path-following steer never fired: off the line by under 1.5), so the A wait too until he's there
    newpress = (raw[0] << 8 | raw[1]) & ~(LP["pad"][0] << 8 | LP["pad"][1])
    if not lp_tilted(LP["pad"]) or (lp_tilted(raw) and not newpress):
        return False
    after_at = 0 if newpress else 10              # a press: where the PC's Wes was then; a release: once stopped
    if os.environ.get("PLAYREC_LIVEPAD_HOLD", "1") != "1":
        return False
    path = LP["path"].get(k)
    if not path:
        return False
    after = [p for p in path if p[0] >= delay + after_at]
    if not after:
        return False
    tx, tz = after[0][1], after[0][2]
    pos = player_pos()
    if not pos:
        return False
    d = math.hypot(tx - pos[0], tz - pos[2])
    key = (k, delay)
    h = st.setdefault("lp_hold", {}).setdefault(key, {"n": 0, "best": d})
    if d <= 2 or h["n"] >= 60 or (h["n"] >= 10 and d > h["best"] - 0.05):
        if h["n"]:
            probe_out("livepad.csv", LP_HDR).write("%d,held,%d,,,%d,%d frames longer; %.1f units left\n" % (
                frame, k, LP["anchors"][k][2], h["n"], d))
        # *Added 2026-11-03 (P03):* PLAYREC_LIVEPAD_SNAP=<PC frame> moves Wes onto the PC's spot when the hold gives
        # up more than 2 units off, from that PC frame on: a last resort to reach a later scene (P03's battle, for the
        # Pursuit shots) when the walks drift 8-16 units. It changes the console's state, so a snapped run is a way to
        # get somewhere, not a comparison of the field; every snap is logged ("snap" rows)
        # *Fixed 2026-11-05 (P07 replay_1):* only a hold that gives up off the PC's spot snaps; one that ends on it
        # (within 2 units) leaves Wes on his walk. P07's hold ended on the spot and still snapped him to the door target
        # 16.4 units back
        if d > 2:
            # *Fixed 2026-11-05 (P06 replay_7):* the door target only when it lies ahead of this point of the walk (its
            # sample at or after this step's delay); otherwise the hold's own target. P06's walk to room 15 (PC 48,907)
            # gave up at delay 123 (5.1 units off, the PC had turned) and was snapped back 62.6 units to the walk's
            # first sample, then stalled
            back = lp_room_back(k, delay, pos)
            # *Fixed 2026-11-05 (P06 replay_11):* the door target only for a hold near the walk's end (within 10
            # units of the hold's own target). P06's 870-frame walk to room 23 (PC 80,832) gave up mid-walk 9.7 units
            # off; the snap put Wes 84 units on at the door, and the walk's remaining steps marched him 70 units away
            if back and math.hypot(back[0] - tx, back[1] - tz) > 10:
                back = None
            tx, tz = back or (tx, tz)
            lp_snap(k, tx, tz, math.hypot(tx - pos[0], tz - pos[2]), 0.5 if back else 2)
        return False
    h["n"] += 1
    h["best"] = min(h["best"], d)
    return True


def lp_room_back(k, now=None, pos=None):
    """PLAYREC_LIVEPAD_SNAP_BACK=<PC frames>: when the next event is a room entry, the snap target is the PC's spot that
    many frames before the entry (None: not set, or no such sample; with <now>, a delay in the group, also None when
    the target's sample is earlier than that: the hold's snap never sends Wes back along his walk).
    *Added 2026-11-04 (P02 run 17):* the last spot before room 3's exit (PC 32,052) was already past the door: the PC's
    Wes touched the door at z -11.5 (touch event 3, its script about 50 frames), then crossed the pass line at z -14.4
    to -15.9, which requested room 11. Snapped to -17.3, Dolphin's Wes touched and crossed nothing, and the push walked
    him on to -27.9. Snapping further back lets him walk through the door's own triggers.
    *Changed 2026-11-04 (P06 replay_3):* when the room entry is closer than <back> frames to the last event, the
    earliest sample after that event is used instead of none. P06's room 0x17 exit came 97 frames after a talk closed;
    with no sample 130 frames back the snap fell back to the PC's last spot, (1.52, -41.06), where the door's script had
    already moved Wes after the request fired at about (3.3, -47.3), so Dolphin's Wes stood past the trigger"""
    back = int(os.environ.get("PLAYREC_LIVEPAD_SNAP_BACK", "0"))
    an = LP["anchors"]
    if back <= 0 or k + 1 >= len(an) or an[k + 1][0] != "room":
        return None
    path = [p for p in LP["path"].get(k, []) if p[0] <= an[k + 1][2] - an[k][2]]
    # *Changed 2026-11-04 (P06 replay_5):* a door whose script moves Wes when the request fires shows as a jump in the
    # PC's path (room 0x17's exit: 6.5 units between two 10-frame samples, a walk covers about 2). The trigger is where
    # he stood before the jump, so the snap goes one sample further back and the push walks him through it. replay_5
    # snapped to the anchor's first spot, (21.0, -62.5), and the push walked him into a wall
    # *Changed 2026-11-05 (P07 replay_1):* a jump is also at least 2.5 times the step before it. Running covers about
    # 7.2 units per sample, so P07's walk to the PC (room 0x391, PC 38,774) read as a jump and Dolphin's Wes, on the
    # PC's path, was snapped 16.4 units back and stalled; P06's door jump was 6.5 after a step of 2.1
    import math
    step = [math.hypot(path[i][1] - path[i - 1][1], path[i][2] - path[i - 1][2]) if i else 0.0
            for i in range(len(path))]
    # *Fixed 2026-11-05 (P06 replay_7):* the first step has none before it to compare with, so it is never a jump (it
    # made every walk with no door jump snap to its first sample)
    # *Fixed 2026-11-05 (P06 replay_9):* with <pos> (Dolphin's Wes), "behind" is measured from his nearest sample on the
    # PC's line, not from the step's delay, and a target behind him gives that nearest sample instead (back on the
    # line, never back along it). P06's walk to room 17 (PC 70,360) gave up with Wes 11 units short of the exit, at
    # the line's sample 259; the 130-frames-back target was sample 179, 57.8 units behind him, and from there the push
    # walked him into a wall
    near = None
    if pos is not None and path:
        near = min(range(len(path)), key=lambda j: math.hypot(path[j][1] - pos[0], path[j][2] - pos[2]))

    def ahead(i):
        if near is not None:
            return path[i] if i >= near else path[near]
        return path[i] if now is None or path[i][0] >= now else None

    for i in range(len(path) - 1, 1, -1):
        if step[i] > 4 and step[i] > 2.5 * step[i - 1]:
            at = ahead(max(i - 2, 0))
            return (at[1], at[2]) if at else None
    idx = [j for j, p in enumerate(path) if p[0] <= an[k + 1][2] - an[k][2] - back][-1:] or ([0] if path else [])
    at = ahead(idx[0]) if idx else None
    return (at[1], at[2]) if at else None


def lp_snap(k, tx, tz, d, min_d=2):
    """PLAYREC_LIVEPAD_SNAP: Wes moved to (tx, tz) when he is more than min_d units off (2 by default), from that PC
    frame on. Written like the game's own peopleSetPositionByIndex (0x8018C0A8): the model (GSmodelSetPosition: +0x18
    and its joint's translation, joint +0x38) and the entry's copy at +0x9C (peopleSetTransform). The model alone was
    undone the next frame (P03 run 4; likely from the entry's copy).
    *Changed 2026-11-05 (P06 replay_6):* a door's snap target (lp_room_back) uses 0.5 units. Room 0x17's exit: the PC's
    Wes slid along the wall at z -48.0 to x 5.26 and through the opening at about x 5; Dolphin's reached the same wall
    later and further right and stopped at x 6.71, 1.45 units from the target, so the 2-unit limit never snapped him
    and the straight-up push held him against the door frame for 1,800 frames (verified in pos.csv; no person
    involved)"""
    snap_from = int(os.environ.get("PLAYREC_LIVEPAD_SNAP", "-1"))
    em = player_entry()
    if d > min_d and 0 <= snap_from <= LP["anchors"][k][2] and em is not None:
        e, m = em
        j = u32(m + 0x08)
        for a in [m + 0x18, e + 0x9C] + ([j + 0x38] if ram(j) else []):
            memory.write_f32(a, tx)
            memory.write_f32(a + 8, tz)
        probe_out("livepad.csv", LP_HDR).write("%d,snap,%d,,,%d,moved %.1f units to %.2f %.2f\n" % (
            frame, k, LP["anchors"][k][2], d, tx, tz))


def livepad_now():
    """the pad to send this frame"""
    k = LP["k"]
    # the stall watchdog (snatcherLeo, 2026-11-03: "You need to be able to see if something gets stuck without me being
    # there"): no PC event matched for LP_STALL frames past the next one's due time ends the replay, with the reason
    # in livepad.csv and replay.json, so a stuck run finishes by itself and its runner is told
    an = LP["anchors"]
    if k + 1 < len(an):
        due = (LP["at"] if k >= 0 else 0) + (an[k + 1][2] - (an[k][2] if k >= 0 else 0))
        # *Added 2026-11-03 (P03 run 8):* with PLAYREC_LIVEPAD_SNAP, a talk still not open 300 frames past due (after
        # the re-press snap) is skipped: the pad goes on from that talk's window close, as if it had been seen. P03's
        # room 0x14 talk (script 0x0596000B, PC 23,428) never opened with Wes on the PC's exact spot and facing: the
        # person stands elsewhere on the console (the field drift). A skipped talk is logged ("skiptalk")
        snap_from = int(os.environ.get("PLAYREC_LIVEPAD_SNAP", "-1"))
        if (k >= 0 and 0 <= snap_from <= an[k + 1][2] and an[k + 1][:2] == ("menu", "64:p2")
                and frame > due + 300):
            j = next((j for j in range(k + 2, min(k + 40, len(an))) if an[j][:2] == ("window", -1)), None)
            if j is not None:
                probe_out("livepad.csv", LP_HDR).write("%d,skiptalk,%d,%s,%s,%d,to %d (PC %d)\n" % (
                    frame, k + 1, an[k + 1][0], an[k + 1][1], an[k + 1][2], j, an[j][2]))
                LP["k"], LP["at"], LP["i"], LP["retry"], LP["pending"] = j, frame, 0, 0, []
                LP["pad"] = bytes(10)
                return LP["pad"]
        if frame > due + LP_STALL and not st.get("lp_stalled"):
            st["lp_stalled"] = True
            probe_out("livepad.csv", LP_HDR).write("%d,stalled,%d,%s,%s,%d,no match for %d frames past due\n" % (
                frame, k + 1, an[k + 1][0], an[k + 1][1], an[k + 1][2], frame - due))
            finish("livepad stalled at PC event %d (%s %s, PC frame %d)" % (k + 1, an[k + 1][0], an[k + 1][1],
                                                                         an[k + 1][2]))
    if k < 0:
        for g, raw in LP["boot"]:
            if frame >= g + 137:          # before the first event: the PC's frame + P01 pass 1's boot offset
                LP["pad"] = raw
        return LP["pad"]
    steps = LP["by"].get(k, [])
    # (an event is seen in this frame's callback and a pad set here is read in the next frame: LAG -1 by default)
    while LP["i"] < len(steps) and frame >= LP["at"] + steps[LP["i"]][0] + LP_LAG:
        raw = steps[LP["i"]][1]
        if livepad_hold(k, steps[LP["i"]][0], raw):
            break                                 # the stick stays held this frame (the walk isn't there yet)
        if (raw[0] << 8 | raw[1]) & ~(LP["pad"][0] << 8 | LP["pad"][1]):
            LP["lastpress"] = bytes(raw[:2]) + bytes(8)       # the buttons only, sticks centred
        if LP.get("calm"):
            if not lp_tilted(raw) and not raw[1] & 0x0F:
                LP["calm"] = False                # the PC's stick is back at centre (within 20), its D-pad let go
            else:
                raw = raw[:1] + bytes([raw[1] & 0xF0, 0, 0]) + raw[4:]
        LP["pad"] = raw
        LP["i"] += 1
    # *Changed 2026-11-04 (P02 run 19):* the calm also lets go of the D-pad. P02's field menu opened on row 3 in
    # Dolphin (row 2 on the PC; it remembers its last row), the PC's held Up went 3 -> 2 -> 1 (the PC's event), and its
    # auto-repeat went on to row 0 before the release step, so A opened room 0x38F instead of the P★DA
    if LP.get("calm") and (LP["pad"][2] or LP["pad"][3] or LP["pad"][1] & 0x0F):
        LP["pad"] = LP["pad"][:1] + bytes([LP["pad"][1] & 0xF0, 0, 0]) + LP["pad"][4:]
    # steering is opt-in (PLAYREC_LIVEPAD_STEER=1): both versions made P01 worse (runs 8 and 9: Wes ended 17-30 units
    # off and missed doors run 6 found unsteered); the PC's position at a given delay isn't Dolphin's target when the
    # two stand still for different times after a menu
    # *Added 2026-11-03 (P03):* PLAYREC_LIVEPAD_STEER_FROM=<PC frame> steers only from that PC event on: P03's walk
    # after the talk at the PC's 21,726 drifts 5 units left in Dolphin and misses the next talk, while steering the
    # whole sitting stalls it in the third room
    steer_from = int(os.environ.get("PLAYREC_LIVEPAD_STEER_FROM", "-1"))
    steer = livepad_steer(k) if (os.environ.get("PLAYREC_LIVEPAD_STEER") == "1"
                                 or 0 <= steer_from <= an[k][2]) else None
    if steer is not None:
        return steer
    an = LP["anchors"]
    if k + 1 < len(an) and LP["i"] >= len(steps):
        due = LP["at"] + (an[k + 1][2] - an[k][2]) + 20
        late = frame - due
        # *Changed 2026-11-03 (P02):* when the next event is a menu cursor change, the retry replays the group's last
        # burst of input as recorded (from its last change away from neutral to the end): P02's box cursor took a
        # stick tap 8 frames after reaching 0 on the PC and ignored it in Dolphin, and re-pressing A could only make
        # things worse. Other overdue events still re-press the last button
        burst = []
        if an[k + 1][0] == "menu" and ":c" in str(an[k + 1][1]):
            b0 = None
            for j, (d, raw) in enumerate(steps):
                neutral = raw[0] == 0 and raw[1] == 0 and raw[2] == 0 and raw[3] == 0
                if not neutral and (j == 0 or b0 is None or steps[j - 1][1][:4] == bytes(4)):
                    b0 = j
            if b0 is not None:
                burst = [(d - steps[b0][0], raw) for d, raw in steps[b0:]]
        if burst and late >= 0 and LP["retry"] < 10:
            t = late % 30
            if t == 0:
                LP["retry"] += 1
                probe_out("livepad.csv", LP_HDR).write("%d,reburst,%d,%s,%s,%d,%d steps\n" % (
                    frame, k + 1, an[k + 1][0], an[k + 1][1], an[k + 1][2], len(burst)))
            cur = None
            for d, raw in burst:
                if t >= d:
                    cur = raw
            if cur is not None and t <= burst[-1][0] + 1:
                return cur
            return bytes(10)
        if late >= 0 and LP["retry"] < 10 * 3:
            # the next event is overdue: press the group's last new button again (3 frames)
            press = None
            prev = 0
            for _, raw in steps:
                b = raw[0] << 8 | raw[1]
                if b & ~prev:
                    press = raw
                prev = b
            # a group with no press of its own: the last press fired before it (P01 live run 5, battle 14: the PC's
            # window after message 30005 went on 4 frames later on an A still held, Dolphin's waited)
            press = press or LP.get("lastpress")
            # *Added 2026-11-03 (P03 run 5):* with PLAYREC_LIVEPAD_SNAP, the first re-press for a talk first moves Wes
            # to the PC's last spot before it: P03's third talk (PC 23,428) had Wes stopped 6.6 units short, blocked,
            # with no stick release or running press for the hold (and its snap) to act on
            if press is not None and late == 0 and LP.get("path", {}).get(k):
                # (where the PC's Wes stood when the talk opened: the first sample at or after it; the last one before
                # it, 8 frames earlier, was 1.9 units short and the talk didn't open, run 6)
                at = [p for p in LP["path"][k] if p[0] >= an[k + 1][2] - an[k][2]]
                j = k + 1                     # (later samples are filed under the last event at the talk's PC frame:
                while j + 1 < len(an) and an[j + 1][2] == an[k + 1][2]:   # P03's menu and window both at 23,428)
                    j += 1
                at = at or LP["path"].get(j, [])[:1] or LP["path"][k][-1:]
                if an[k + 1][0] == "room":
                    # *Fixed 2026-11-04 (P05 run 1):* a room entry's later samples are the next room's coordinates
                    # (P05's snap moved Wes 57 units to (55.9, 76.2), the PC was at (-0.9, 40.4) by the door): the last
                    # sample before the entry instead
                    at = [p for p in LP["path"][k] if p[0] <= an[k + 1][2] - an[k][2]][-1:]
                    # (with Wes's position: this re-press snap, not the hold's, sent P06 replay_10's Wes 57.8 units
                    # back at room 15's exit, 2026-11-05; the hold's fix alone didn't reach it)
                    back = lp_room_back(k, None, player_pos())
                    at = [(0, back[0], back[1])] if back else at
                else:
                    back = None
                pos = player_pos()
                if at and pos:
                    import math
                    tx, tz = at[0][1], at[0][2]
                    lp_snap(k, tx, tz, math.hypot(tx - pos[0], tz - pos[2]), 0.5 if back else 2)
            snap_from = int(os.environ.get("PLAYREC_LIVEPAD_SNAP", "-1"))
            if an[k + 1][:2] == ("menu", "64:p2") and 0 <= snap_from <= an[k + 1][2] and late < 4:
                # *Added 2026-11-05 (P08 replay_2):* an overdue talk in snap mode first turns Wes: 4 frames of the
                # group's last tilted stick, no buttons, then the re-presses. P08's roadblock talk (PC 42,968): the PC
                # swept the stick from (-43,-127) to (-125,-52) in 9 frames, turning Wes on the spot, then pressed A;
                # Dolphin's Wes ran those steps 3 units further on, the snap put him back facing the old way, and ten
                # A presses opened nothing (PC frames, run.log [pad_script]; facing a likely cause, not measured)
                turn = next((raw for _, raw in reversed(steps) if lp_tilted(raw)), None)
                if turn is not None:
                    if late == 0:
                        probe_out("livepad.csv", LP_HDR).write("%d,turn,%d,%s,%s,%d,%s\n" % (
                            frame, k + 1, an[k + 1][0], an[k + 1][1], an[k + 1][2], turn.hex()))
                    return bytes(2) + turn[2:]
            if an[k + 1][0] == "room" and 0 <= snap_from <= an[k + 1][2] and late < 600:
                # *Added 2026-11-04 (P05 run 2):* an overdue room entry in snap mode keeps pushing the group's last
                # tilted stick (after the re-press snap put Wes at the PC's spot by the door): P05's PC pushed (0xC0,
                # 0x77) into the door for 23 frames before the room changed, and the 3-frame re-press of an older A
                # press only nudged Dolphin's Wes along the wall
                push = next((raw for _, raw in reversed(steps) if lp_tilted(raw)), None)
                if push is not None:
                    if late == 0:
                        probe_out("livepad.csv", LP_HDR).write("%d,push,%d,%s,%s,%d,%s\n" % (
                            frame, k + 1, an[k + 1][0], an[k + 1][1], an[k + 1][2], push.hex()))
                    return bytes(2) + push[2:]
            if press is not None and late % 30 >= 3:
                # *Added 2026-11-03 (P03 run 9):* between re-presses the pressed buttons are let go. The PC released A
                # only after the next event (menu 75 p2 -> p3 at 28,380), so in Dolphin A stayed held from the group's
                # press on, and every re-press landed on a held A (no new press, ten times)
                b = press[0] << 8 | press[1]
                held = (LP["pad"][0] << 8 | LP["pad"][1]) & ~b
                return bytes([held >> 8, held & 255]) + LP["pad"][2:]
            if press is not None:
                LP["retry"] += 1
                if late % 30 == 0:
                    probe_out("livepad.csv", LP_HDR).write("%d,repress,%d,%s,%s,%d,%s\n" % (
                        frame, k + 1, an[k + 1][0], an[k + 1][1], an[k + 1][2], press.hex()))
                return press
    return LP["pad"]


def osd(text, color=None, ms=3000):
    try:
        if gui:
            if color is None:
                gui.add_osd_message(text, ms)
            else:
                try:
                    gui.add_osd_message(text, ms, color)
                except TypeError:                     # a build whose call takes no colour
                    gui.add_osd_message(text, ms)
    except Exception:
        pass


def ram(a):
    return 0x80000000 <= a < 0x81800000


HEAP_EVERY = int(os.environ.get("PLAYREC_HEAP_EVERY", "600"))   # the heap probe's sampling step (frames)


def u8(a): return memory.read_u8(a)
def u16(a): return memory.read_u16(a)
def u32(a): return memory.read_u32(a)


def s8(v):
    return v - 256 if v > 127 else v


def read_pad_bytes():
    return bytes(u8(LATCHED + k) for k in range(10))


def save_state(name, why):
    try:
        savestate.save_to_file(os.path.join(OUT, "states", name + ".sav"))
        w("state", name=name, why=why)
    except Exception as e:
        w("state_error", name=name, err=repr(e))
    st["last_state"] = frame


def movie_check():
    """Run 2's movie start and check (guide part 3, 2026-10-19). (1) When the Genius Sonority logo's room (0x39A)
    is first entered, write OUT/start_movie.flag: the launcher (playrec.py) presses Dolphin's "Start Recording"
    hotkey then, if the recording profile binds one (a movie started during the boot froze Dolphin once, sitting 5).
    (2) From 300 frames after the logo, every 1,800 frames until confirmed: save a check state; while a movie records,
    Dolphin writes the movie so far beside every state (<state>.sav.dtm). Found: "Movie OK"; not found: a red
    "NO MOVIE" message (on screen and as a no_movie event), repeated until the movie starts or the sitting ends."""
    if st.get("movie_ok"):
        return
    if st.get("logo_at") is None:
        if u32(ROOM) == 0x39A:
            st["logo_at"] = frame
            try:
                open(os.path.join(OUT, "start_movie.flag"), "w").write(str(frame))
            except Exception as e:
                w("flag_error", err=repr(e))
        return
    since = frame - st["logo_at"]
    pending = st.get("movie_probe")
    if pending and frame >= pending[1]:
        dtm = os.path.join(OUT, "states", pending[0] + ".sav.dtm")
        st["movie_probe"] = None
        if os.path.exists(dtm) and os.path.getsize(dtm) > 0x100:
            st["movie_ok"] = True
            w("movie_ok", state=pending[0])
            osd("Movie OK")
        else:
            w("no_movie", state=pending[0])
            osd("NO MOVIE: press L+R+START and start again", color=0xFFFF4040, ms=15000)
    elif not pending and since >= 300 and (since - 300) % 1800 == 0:
        name = "movie_check_%03d" % ((since - 300) // 1800 + 1)
        save_state(name, "movie check")
        st["movie_probe"] = (name, frame + 30)       # Dolphin writes the state (and the movie) a moment later


# Message placeholder slots -> the global that holds each (msgctrlSetValue, msgctrl_exact_80132A38.c; verified). The
# battle code stores string pointers in these (names via GSmsgGetGSchar, nicknames via pokemonGetStatus 0x77).
PLACEHOLDER = {0x0D: 0x8047ADC8, 0x0E: 0x8047ADCC, 0x0F: 0x8047ADD4, 0x10: 0x8047ADD8, 0x11: 0x8047ADDC,
               0x12: 0x8047ADE0, 0x13: 0x8047ADE4, 0x14: 0x8047ADE8, 0x15: 0x8047ADEC, 0x16: 0x8047ADF0,
               0x17: 0x8047ADF4, 0x1A: 0x8047AE00, 0x1B: 0x8047AE04, 0x1C: 0x8047AE08, 0x1F: 0x8047AE14,
               0x20: 0x8047AE18, 0x21: 0x8047AE1C, 0x22: 0x8047AE20, 0x23: 0x8047AE24, 0x28: 0x8047AE38,
               0x29: 0x8047AE3C, 0x41: 0x8047ADD0}


def game_string(p, limit=40):
    """A game string (16-bit characters, zero-terminated) at p; control codes (0xFFxx) are skipped."""
    out = []
    for k in range(limit):
        c = u16(p + 2 * k)
        if c == 0:
            break
        if c < 0xFF00 and c >= 0x20:
            out.append(chr(c))
    return "".join(out)


def placeholder_names():
    """{slot: text} for the placeholder slots that hold a readable string right now (read when a battle message
    shows, so the report can fill the line's <0F>, <28>, ...). Values that aren't pointers to text are left out."""
    got = {}
    for slot, addr in PLACEHOLDER.items():
        try:
            v = u32(addr)
            if ram(v):
                if 0x8000 <= u16(v) < 0x8180 and ram(u32(v)):
                    # not text but a field Pokémon record (its first word is a pointer; slots 0x0F / 0x10, verified on
                    # sitting 6): its nickname, record +4 -> battle record, +4 -> the Pokémon copy, +0x2E
                    fp = u32(v + 4)
                    s = game_string(fp + 4 + 0x2E, 11) if ram(fp) else ""
                else:
                    s = game_string(v)
                if s.strip():
                    got["%02X" % slot] = s
        except Exception:
            pass
    # the move's attacker and target: battle floor +0xA474 / +0xA478, field Pokémon records (floor status 0x36 / 0x42,
    # fightFloorBiosGetAttackPokemonPtr / ...DefensePokemonPtr; verified from the getters): their nicknames
    for key, off in (("ATK", 0xA474), ("DEF", 0xA478)):
        try:
            op = u32(FIGHT_FLOOR + off)
            fp = u32(op + 4) if ram(op) else 0
            if ram(fp):
                s = game_string(fp + 4 + 0x2E, 11)
                if s.strip():
                    got[key] = s
        except Exception:
            pass
    # number slots (msgctrlSetValue stores these as plain values): 0x2F u32 (EXP amounts, ...), 0x2D / 0x2E u16
    for slot, addr, size in ((0x2F, 0x8047AE54, 4), (0x2D, 0x8047AE50, 2), (0x2E, 0x8047AE52, 2)):
        try:
            got["%02X" % slot] = str(u32(addr) if size == 4 else u16(addr))
        except Exception:
            pass
    return got


def dump_block(tag):
    try:
        base = u32(SAVE_PTR)
        if not ram(base):
            return
        # the fork has no bulk read: whole words, big-endian as in memory (BLOCK_SIZE is a multiple of 4)
        data = b"".join(u32(base + o).to_bytes(4, "big") for o in range(0, BLOCK_SIZE, 4))
        name = "%04d_%s.bin.z" % (st["entries"], tag)
        open(os.path.join(OUT, "blocks", name), "wb").write(zlib.compress(data, 6))
        w("block", file=name, base=hex(base))
    except Exception as e:
        w("block_error", err=repr(e))


def player_entry():
    """the player's people entry (group 0, id 100) and its model (+0x18 / +0x1C / +0x20 are x / y / z), or None"""
    try:
        base, n = u32(PEOPLE_PTR), u32(PEOPLE_MAX)
        if not ram(base) or n > 512:
            return None
        for i in range(n):
            e = base + i * ENTRY
            if u8(e) and u32(e + 0x28) == 0 and u32(e + 0x2C) == 100:
                m = u32(e + 0x08)
                if ram(m):
                    return e, m
    except Exception:
        pass
    return None


def player_pos():
    em = player_entry()
    m = em[1] if em else None
    if m is None:
        return None
    return [round(memory.read_f32(m + o), 2) for o in (0x18, 0x1C, 0x20)]


def field_pokemon():
    """The Pokémon on the battle field (live.py's walk: gFightFloor +0xA4C4, 8 slots -> fight Pokémon + 4 = record):
    [slot, species, hp, max hp, [moves], [pp]] for each occupied slot."""
    out = []
    for i in range(8):
        op = u32(FIGHT_FLOOR + 0xA4C4 + 4 * i)
        fp = u32(op + 4) if ram(op) else 0
        if ram(fp):
            pk = fp + 4
            out.append([i, u16(pk), u16(pk + 0x8A), u16(pk + 0x8C), [u16(pk + 0x78 + 4 * k) for k in range(4)],
                        [u8(pk + 0x7A + 4 * k) for k in range(4)]])
    return out


def watch_game():
    """Polled state: message windows, sound slots, battle result, entries (states and blocks), field Pokémon."""
    t = s8(u8(WINMSG_TYPE))
    if t != st["winmsg"]:
        if st["winmsg"] is not None:
            w("msgwin", type=t, error=u8(WINMSG_ERROR))
        st["winmsg"] = t
    snd = (u32(SOUND_A), u32(SOUND_B))
    if snd != st["snd"]:
        w("sound", slots=list(snd))
        st["snd"] = snd
    res = u16(FIGHT_RESULT)
    if res != st["result"]:
        if st["result"] is not None:
            w("battle_result", value=res)
        st["result"] = res
    if len(idlog.entered) > st["entries"]:
        for room, ef in idlog.entered[st["entries"]:]:
            st["entries"] += 1
            p = u32(RNG_PTR)
            w("entered", room=room, n=st["entries"], rng=u32(p) if ram(p) else None, pos=player_pos())
            dump_block("room_%x" % room)
            if MODE == "record" and room not in st["rooms_seen"]:
                save_state("%04d_room_%x" % (st["entries"], room), "first entry")
            st["rooms_seen"].add(room)
    if len(idlog.reentered) > st.get("reentries", 0):
        # a room loaded again with no "entered" (a return from a battle room that reloads the field; idlog.py
        # FLOOR_STATE, 2026-10-29): its own event, so the "entered" numbering stays as before
        for room, ef in idlog.reentered[st.get("reentries", 0):]:
            st["reentries"] = st.get("reentries", 0) + 1
            p = u32(RNG_PTR)
            w("reentered", room=room, n=st["entries"], rng=u32(p) if ram(p) else None, pos=player_pos())
    battle = u16(FIGHT_FLOOR + 0x12)
    if battle != st["battle"]:
        if st["battle"] is not None and battle:
            w("battle", id=battle)
            if MODE == "record":
                save_state("%04d_battle_%d" % (st["entries"], battle), "battle start")
        st["battle"] = battle
    if frame % 10 == 0:
        try:
            fp = field_pokemon()
        except Exception:
            fp = []
        old = {r[0]: r for r in (st["field"] or [])}
        if fp and not st["field"]:
            w("field", mons=fp)
        # (Checked 2026-10-19: sitting 6's "Carvanha logged with Larvitar's moves" was right: the trainer's Carvanha
        # and Larvitar both know Thrash, Bite and Focus Energy, per the disc's trainer table. No logging bug.)
        for r in fp:
            o = old.get(r[0])
            if o and o[1] == r[1]:
                if o[2] != r[2]:
                    w("hp", slot=r[0], species=r[1], was=o[2], now=r[2])
                for k in range(4):
                    if r[5][k] < o[5][k] and r[4][k] == o[4][k]:
                        w("move_used", slot=r[0], species=r[1], move=r[4][k], pp=r[5][k])
            elif fp:
                w("switch", slot=r[0], species=r[1], hp=r[2], moves=r[4])
        st["field"] = fp
        # send-outs by battle record, not by slot (2026-10-19): the floor's slot array is re-sorted into turn order
        # each turn (likely), so a "switch" above can be two battlers trading slots. A battle record seen for the
        # first time in this battle is a real send-out. ("switch" stays as it was: old recordings compare against it.)
        recs = st.setdefault("battle_recs", {})
        if recs.get("battle") != st["battle"] or not fp:
            recs.clear()
            recs["battle"] = st["battle"]
        for r in fp:
            op = u32(FIGHT_FLOOR + 0xA4C4 + 4 * r[0])
            rec = u32(op + 4) if ram(op) else 0
            # keyed on (record, species): a replacement reuses the fainted Pokémon's battle record (verified on
            # sitting 6: Carvanha came in on Barboach's record), a slot swap keeps both
            if rec and (rec, r[1]) not in recs:
                recs[(rec, r[1])] = True
                w("send_out", slot=r[0], record=rec, species=r[1], hp=r[2], max_hp=r[3], moves=r[4])
    # battle message ids (the battle's message box: window 0x50, its id at node +0x9C; battle-turn-flow.md): the
    # report shows them as the game's own lines (item use, ball throws, break-outs, Call)
    try:
        node, pos, mid = u32(0x80404ACC + 0x0C), 0, None
        while ram(node) and pos < 64:
            if u32(node + 0x04) == 0x50:
                mid = u32(node + 0x9C)
                break
            node, pos = u32(node + 0x10), pos + 1
        if mid and mid != st.get("battle_msg"):
            w("battle_msg", id=mid, names=placeholder_names())
        st["battle_msg"] = mid
    except Exception:
        pass


def watch_flags():
    try:
        words = idlog.flag_words()
    except Exception as e:
        w("flag_error", err=repr(e))
        return
    prev = st["flags"]
    if prev is not None:
        for c, (old, new) in enumerate(zip(prev, words)):
            for k in range(min(len(old), len(new))):
                x = old[k] ^ new[k]
                if x:
                    for b in range(32):
                        if x >> b & 1:
                            w("flag", cls=c, word=k, bit=b, now=(new[k] >> b) & 1)
    st["flags"] = words


probe_files = {}


def probe_out(name, header):
    if name not in probe_files:
        f = open(os.path.join(OUT, name), "w", buffering=1)
        f.write(header + "\n")
        probe_files[name] = f
    return probe_files[name]


def find_cam():
    """The render camera record (the dolphin_camera.py layout, as the 2026-10-18 scratch logger used it): the first
    entry of the camera table (lbl_8047AC5C, count lbl_8047AC60, 0x14-byte entries) with a record and no parent."""
    tab, n = u32(0x8047AC5C), u32(0x8047AC60)
    if not ram(tab):
        return None
    for i in range(min(n, 4096)):
        e = tab + 0x14 * i
        d = u32(e + 4)
        if d and u32(e + 8) == 0 and u32(e + 0xC) == 0:
            return d
    return None


# The move pass's move writer (snatcherLeo approved 2026-10-19): PLAYREC_SETMOVE="when,target,index,slot,move[,pp]".
# when: "battle" (the first frame the target is on the field) or a frame number; target: "species" (index = a species
# id: the first Pokémon on the field with it; the move pass's choice, the same on both sides), "field" (that field
# slot's battle copy, PokemonBios at battle record +4: what the move command re-reads) or "party" (the player's
# party slot <index> in the save block: before a battle, so the battle copies it); slot 0-3; move id; pp (default:
# the move table's PP, entry +0x01; table pointer 0x80478DFC, 0x38 bytes per move: wazaDataBiosGetPtr). The slot's
# PokemonWaza at +0x78 + 4 * slot: u16 move, u8 PP, u8 PP-ups (set to 0). Writes once; logged to setmove.csv.
SETMOVE = os.environ.get("PLAYREC_SETMOVE", "")


def set_move():
    if st.get("setmove_done"):
        return
    try:
        p = SETMOVE.split(",")
        when, target, idx, slot, move = p[0], p[1], int(p[2], 0), int(p[3], 0), int(p[4], 0)
        if when != "battle" and frame < int(when, 0):
            return
        if target in ("field", "species"):
            # field: the floor's slot array index (re-sorted into turn order each turn, likely); species: the first
            # Pokémon on the field with that species id (unambiguous, and the same on the PC whatever its numbering)
            rec = 0
            for i in ([idx] if target == "field" else range(8)):
                op = u32(FIGHT_FLOOR + 0xA4C4 + 4 * i)
                fp = u32(op + 4) if ram(op) else 0
                if ram(fp) and (target == "field" or u16(fp + 4) == idx):
                    rec = fp + 4
                    break
            if not rec:
                return                                    # not on the field yet: try again next frame
        else:
            base = u32(SAVE_PTR)
            if not ram(base):
                return
            rec = base + 0x70 + 0x30 + 0x138 * idx        # HeroSaveData party (save-data.md)
        table = u32(0x80478DFC)
        pp = int(p[5], 0) if len(p) > 5 else (u8(table + 0x38 * move + 1) if ram(table) else 0)
        old = (u16(rec + 0x78 + 4 * slot), u8(rec + 0x7A + 4 * slot))
        memory.write_u16(rec + 0x78 + 4 * slot, move)
        memory.write_u8(rec + 0x7A + 4 * slot, pp)
        memory.write_u8(rec + 0x7B + 4 * slot, 0)
        probe_out("setmove.csv", "frame,target,index,slot,record,species,old_move,old_pp,move,pp,readback").write(
            "%d,%s,%d,%d,0x%08x,%d,%d,%d,%d,%d,%d\n" % (frame, target, idx, slot, rec, u16(rec), old[0], old[1], move,
                                                      pp, u16(rec + 0x78 + 4 * slot)))
        st["setmove_done"] = True
    except Exception as e:
        probe_out("setmove.csv", "frame,target,index,slot,record,species,old_move,old_pp,move,pp,readback").write(
            "# frame %d error %r\n" % (frame, e))
        st["setmove_done"] = True


def run_probes():
    """Replay-time extras (PLAYREC_PROBES): camera.csv in the PC's ORRE_CAMERA_LOG columns (eye, target, FOV, room),
    rng.csv (the shared random-number state every frame)."""
    if NW_FN is not None and not ROLLINJECT:
        try:
            nw_arm()
        except Exception as e:
            probe_out("natives.csv", NW_HDR).write("# frame %d arm error %r\n" % (frame, e))
    if "camera" in PROBES:
        try:
            if st.get("cam") is None or frame % 60 == 0:
                st["cam"] = find_cam()
            c = st["cam"]
            if c and ram(c):
                e = [memory.read_f32(c + 0x70 + 4 * k) for k in range(3)]
                i = [memory.read_f32(c + 0x100 + 4 * k) for k in range(3)]
                cobj = u32(c + 0x0C)
                fov = memory.read_f32(cobj + 0x40) if ram(cobj) else -1.0
                probe_out("camera.csv", "frame,eye_x,eye_y,eye_z,int_x,int_y,int_z,fov,room").write(
                    "%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d\n" % (frame, e[0], e[1], e[2], i[0], i[1], i[2], fov,
                                                                     u32(ROOM)))
        except Exception:
            pass
    if "vids" in PROBES:
        # MusyX's live voice ids (2026-10-25; P01's Folly battle: Uproar's move waits on sndFXCheck(vid), which
        # is "playing" while the vid is in this list): vidRoot (0x8047AFD4) -> VID_LIST {next +0, prev +4, vid +8,
        # root +0xC}; a row when the set changes: frame, retrace, vids added, vids removed (hex, space-separated). The
        # PC's ORRE_VID_LOG=1 prints the same row
        hdr = "frame,retrace,added,removed"
        try:
            node, live, n = u32(0x8047AFD4), set(), 0
            while ram(node) and n < 512:
                live.add(u32(node + 8))
                node = u32(node)
                n += 1
            old = st.get("vids") or set()
            if live != old:
                probe_out("vids.csv", hdr).write("%d,%d,%s,%s\n" % (
                    frame, u32(0x8047A84C), " ".join("%x" % v for v in sorted(live - old)),
                    " ".join("%x" % v for v in sorted(old - live))))
                st["vids"] = live
        except Exception as e:
            probe_out("vids.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "voices" in PROBES:
        # MusyX's hardware voices with their parameters (2026-10-27; the project's second-pass sound layer, to pair with the
        # PC's ORRE_SOUND_LOG "[sndvoice]" lines). The voice array is the pointer at 0x8047B024, 0xF4 bytes per voice
        # (hwBreak / hwSetVolume's matched code), u8 0x8047B05D of them (the count the hardware layer's own loop over
        # the array uses; 64 in Colosseum). Per voice: changed[0]
        # +0x24, pitch[0] +0x38, volL/R/S +0x4C, aux A L/R/S +0x52, aux B L/R/S +0x58, smp_id +0x70, SAMPLE_INFO +0x74
        # (length +0x84, loop +0x88, loopLength +0x8C, compType +0x90), state +0xEC (0 off, 1 starting, 2 playing),
        # studio +0xEF, lastUpdate.pitch +0xE4. A row when a voice turns on ("start"), off ("end"), or any logged value
        # changes ("set"): frame, retrace, what, voice, state, smp, studio, comp, len, loop, looplen, pitch (pitch[0],
        # hex, what the PC's line prints), pslot / pnow (hwSetPitch writes pitch[salTimeOffset], one of five sub-frame
        # slots, and keeps the slot in lastUpdate.pitch, 0xFF = none: so pitch[0] reads 0 for a voice whose pitch went
        # into a later slot, and pnow = pitch[pslot] is the voice's current pitch), vol L/R/S, aux A, aux B (signed,
        # 0x7FFF = full, as the PC prints them). Since 2026-10-28 (P08's play-length differences): adsr = the voice's
        # envelope (AdsrVars at +0xA4) as mode:state (+0xA4 / +0xA5; a change of state writes a row, so a release
        # shows as its own "set" row), and env = its current level (s32 +0xAC), read on every row but never itself
        # the reason for one (it changes every frame)
        hdr = "frame,retrace,what,voice,state,smp,studio,comp,len,loop,looplen,pitch,pslot,pnow,vol,auxA,auxB,adsr,env"
        try:
            base, nv = u32(0x8047B024), memory.read_u8(0x8047B05D)
            old = st.get("voices") or {}
            old_env = st.get("voices_env") or {}
            now, env = {}, {}
            if ram(base):
                for v in range(min(nv, 64)):
                    p = base + v * 0xF4
                    state = memory.read_u8(p + 0xEC)
                    if not state:
                        continue
                    s16 = lambda o: (memory.read_u16(p + o) ^ 0x8000) - 0x8000
                    now[v] = (state, memory.read_u16(p + 0x70), memory.read_u8(p + 0xEF), memory.read_u8(p + 0x90),
                              u32(p + 0x84), u32(p + 0x88), u32(p + 0x8C), u32(p + 0x38), memory.read_u8(p + 0xE4),
                              u32(p + 0x38 + 4 * memory.read_u8(p + 0xE4)) if memory.read_u8(p + 0xE4) < 5 else 0,
                              "%d/%d/%d" % (s16(0x4C), s16(0x4E), s16(0x50)),
                              "%d/%d/%d" % (s16(0x52), s16(0x54), s16(0x56)),
                              "%d/%d/%d" % (s16(0x58), s16(0x5A), s16(0x5C)),
                              "%d:%d" % (memory.read_u8(p + 0xA4), memory.read_u8(p + 0xA5)))
                    env[v] = memory.read_s32(p + 0xAC)
            out = None
            for v in sorted(set(old) | set(now)):
                a, b = old.get(v), now.get(v)
                if a == b:
                    continue
                what = "start" if a is None else "end" if b is None else "set"
                out = out or probe_out("voices.csv", hdr)
                if what == "set" and b[0] == 1 and (a[0] == 2 or a[1] != b[1]):
                    # a voice started again while still playing (MusyX reuses it: state 2 -> 1, P08's samples
                    # 625 / 627, 2026-10-28): an "end" row for the old sound, then a "start"
                    out.write("%d,%d,end,%d,0,%d,%d,%d,%d,%d,%d,%x,%d,%x,%s,%s,%s,%s,%d\n" % (
                        (frame, u32(0x8047A84C), v) + a[1:] + (old_env.get(v, 0),)))
                    what = "start"
                r = b or a
                out.write("%d,%d,%s,%d,%d,%d,%d,%d,%d,%d,%d,%x,%d,%x,%s,%s,%s,%s,%d\n" % (
                    (frame, u32(0x8047A84C), what, v, b[0] if b else 0) + r[1:]
                    + (env.get(v, old_env.get(v, 0)),)))
            st["voices"], st["voices_env"] = now, env
        except Exception as e:
            probe_out("voices.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "scripts" in PROBES:
        # every script function started (2026-11-01; the project's ask for the reference playthrough: talk / room-script / gift / shop rows of
        # playability.csv): cms_ScriptCreateThread gives each start the next handle key (the VM manager *0x80478B00:
        # u16 count +0, u16 lastKey +4, contexts +0xC; a context is 0x16C bytes: status +4, key +6, scriptId +8 =
        # script bank << 16 | function). Code breakpoints report only their first hit in this fork, so this polls once a
        # frame: when lastKey moves, each new key's context gives its script id. A key whose context is already gone
        # (started and finished, and its slot reused, inside one frame) is written with an empty script and missed=1, so
        # the count of starts is always complete. The PC logs the same starts as "[script] start 0x<id> from <caller>"
        hdr = "frame,room,key,script,bank,func,status,missed"
        try:
            pool = u32(0x80478B00)
            if ram(pool):
                last = u16(pool + 4)
                prev = st.get("vm_last")
                if prev is not None and last != prev:
                    count, ctxs = u16(pool), u32(pool + 0x0C)
                    by_key = {}
                    if ram(ctxs) and count <= 0x100:
                        for i in range(count):
                            # every slot, freed ones too: a script that ends inside the frame is marked free (status
                            # 0) by cms_ScriptExecute but keeps its key and id until the slot is reused (the first
                            # version read only live slots and missed 27 of sitting 2's 49 starts)
                            c = ctxs + i * 0x16C
                            by_key[u16(c + 6)] = (u32(c + 8), u8(c + 4))
                    k, out = prev, probe_out("scripts.csv", hdr)
                    while k != last:
                        k = (k + 1) & 0xFFFF
                        if k == 0:
                            continue  # the VM skips key 0
                        if k in by_key:
                            sid, status = by_key[k]
                            out.write("%d,%d,%d,0x%08X,0x%X,%d,%d,0\n" % (frame, u32(ROOM), k, sid, sid >> 16,
                                                                          sid & 0xFFFF, status))
                        else:
                            out.write("%d,%d,%d,,,,,1\n" % (frame, u32(ROOM), k))
                st["vm_last"] = last
        except Exception as e:
            probe_out("scripts.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "battlemode" in PROBES:
        # Battle Mode's kind and rule per battle (2026-11-02, for the reference playthrough's guide section 5: which of the 24 kind / rule
        # pairs Run 2's BM sittings used). Save section 14 at +0xE3E8 of the save image (save-data.md): +0x00 the kind
        # (0 Colosseum Battles, 1 Mt. Battle 100, 2 Gang Battle, 3 Battle Now; menuColosseumBattle stores it), +0x04 the
        # battle type, +0x08 the rule (0 any level, 1 Lv 50, 2 Lv 100, 3-5 the edited rules). A row whenever the three
        # words, the room or the battle id change; reads memory only.
        hdr = "frame,room,battle,kind,type,rule"
        try:
            base = u32(SAVE_PTR)
            if ram(base):
                s14 = base + 0xE3E8
                rec = (u32(ROOM), u16(FIGHT_FLOOR + 0x12), u32(s14), u32(s14 + 4), u32(s14 + 8))
                if rec != st.get("battlemode"):
                    st["battlemode"] = rec
                    probe_out("battlemode.csv", hdr).write("%d,%d,%d,%d,%d,%d\n" % ((frame,) + rec))
        except Exception as e:
            probe_out("battlemode.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "vipos" in PROBES:
        # where the game was when an interrupt came (2026-10-25; P01's Folly battle: one random draw went to another
        # caller inside a 30 fps frame, so where the console's frame logic stands at a refresh decides the order). The
        # SDK's __OSDispatchInterrupt records every interrupt above the memory ones: __OSLastInterrupt (u16 0x8047A718;
        # 24 = the video interface), __OSLastInterruptTime (u64 time base 0x8047A720, 40.5 MHz) and
        # __OSLastInterruptSrr0 (0x8047A714, the instruction it interrupted). Only reads memory, so it can't disturb the
        # replay (memory watches shift it; *corrected 2026-10-26 (verified):* code breakpoints do load from
        # GameSettings and fire, through the fork's Python event, first hit only, without disturbing the replay, but
        # they can't give a per-frame record). A row when
        # the record changes; a frame sees only the last interrupt before it, the retrace's when nothing came after
        hdr = "frame,retrace,interrupt,time,srr0"
        try:
            rec = (memory.read_u16(0x8047A718), u32(0x8047A720), u32(0x8047A724), u32(0x8047A714))
            if rec != st.get("vipos"):
                st["vipos"] = rec
                probe_out("vipos.csv", hdr).write("%d,%d,%d,%d,0x%08x\n" % (
                    frame, u32(0x8047A84C), rec[0], (rec[1] << 32) | rec[2], rec[3]))
        except Exception as e:
            probe_out("vipos.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "rng" in PROBES:
        p = u32(RNG_PTR)
        if ram(p):
            probe_out("rng.csv", "frame,rng,room").write("%d,%d,%d\n" % (frame, u32(p), u32(ROOM)))
    if "frames" in PROBES:
        # each game frame's length in retraces (2026-10-18; snatcherLeo's decision: replays play the console's frame
        # timing): the graphics state (pointer gsgfxState at 0x8047AA80; gs_gfx_layer.h) +0x50 lastRetrace changes once per game
        # frame (GSgfxWaitRetrace), +0x54 frameRetraces is that frame's length. Heavy scenes ask for 60 fps but take two
        # retraces on the console. One row per game frame: the frame-times file for the PC replay is made from it
        # (frames_to_pc.py).
        try:
            g = u32(0x8047AA80)
            # the state sits in the locked-cache scratch area (0xE0000000, 16 KB) once GSgfxInit has run ("state located
            # at E0000000h"), in main RAM only briefly at boot
            if ram(g) or 0xE0000000 <= g < 0xE0004000:
                last = u32(g + 0x50)
                if last != st.get("gfx_last"):
                    st["gfx_last"] = last
                    probe_out("frames.csv", "frame,room,frameRetraces").write(
                        "%d,%d,%d\n" % (frame, u32(ROOM), u32(g + 0x54)))
        except Exception:
            pass
    if "dvd" in PROBES:
        # the console's disc reads (2026-10-19; snatcherLeo's decision: PC replays use the recording's measured read times,
        # the project's ORRE_DVD_TRACE). Polled every frame from the SDK's DVD driver (dvd.h / DVD.c): executing_8047A7E8 = the
        # DVDCommandBlock in progress (DummyCommandBlock 0x803FC3A0 when idle); block +0x10 disc offset, +0x14 length.
        # One row per read when it stops executing. Frames are this replay's frames (one per VI field, the same unit
        # as the PC's retrace count), counted from the sitting's start state; room / room_frame let the PC side align
        # by room entry instead. Resolution: one frame (two short reads inside one frame merge into the later one).
        try:
            room = u32(ROOM)
            if room != st.get("dvd_room"):
                st["dvd_room"], st["dvd_room_at"] = room, frame
            ex = u32(0x8047A7E8)
            cur = (ex, u32(ex + 0x10), u32(ex + 0x14)) if ram(ex) and ex != 0x803FC3A0 else None
            last = st.get("dvd_exec")
            if cur != last:
                if last is not None:
                    start, sroom, sroom_frame = st["dvd_start"]
                    prev_end = st.get("dvd_prev_end")
                    seek = "" if prev_end is None else str(last[1] - prev_end)
                    probe_out("dvd.csv", "start_frame,end_frame,frames,disc_offset,length,seek_from_prev_end,room,"
                                         "room_frame").write("%d,%d,%d,0x%x,%d,%s,%d,%d\n" % (
                        start, frame, frame - start, last[1], last[2], seek, sroom, sroom_frame))
                    st["dvd_prev_end"] = last[1] + last[2]
                if cur is not None:
                    st["dvd_start"] = (frame, room, frame - st["dvd_room_at"])
                st["dvd_exec"] = cur
        except Exception:
            pass
    if "ptcl" in PROBES and PTCL_WIN:
        # the particle probe (2026-11-03, port-gaps 56: P03's Pursuit streaks, bank 2, don't show on the PC):
        # every live HSD_Particle (psstructs.h, 0x94 bytes) on the active lists, activeParticle (0x40 bytes; every word
        # is walked as a list head, particles seen twice counted once), one row per particle per frame inside the
        # window. PLAYREC_PTCL_PC=<from>-<to> is in PC frames (the live pad's clock, only while the next event isn't
        # overdue), PLAYREC_PTCL=<from>-<to> in this file's frames
        wins, by_pc = PTCL_WIN
        pc = None
        if LIVEPAD and LP.get("k", -1) >= 0:
            pc = LP["anchors"][LP["k"]][2] + (frame - LP["at"])
            nxt = LP["anchors"][LP["k"] + 1][2] if LP["k"] + 1 < len(LP["anchors"]) else pc
            if pc > nxt:
                pc = None
        t = pc if by_pc else frame
        if t is not None and any(lo <= t <= hi for lo, hi in wins):
            out = probe_out("ptcl.csv", "frame,pc,list,addr,kind,bank,texGroup,poseNum,palNum,life,x,y,z,size,rotate,"
                            "primCol,envCol,matRGB,matA,ambRGB,ambA,aCmpMode,aCmp1,aCmp2,trail,gen,appsrt")
            seen = set()
            try:
                for li in range(16):
                    p, n = u32(0x80452788 + 4 * li), 0
                    while ram(p) and p not in seen and n < 5000:
                        seen.add(p)
                        n += 1
                        f = [memory.read_f32(p + o) for o in (0x40, 0x44, 0x48, 0x4C, 0x50, 0x88)]
                        out.write("%d,%s,%d,%08x,%08x,%d,%d,%d,%d,%d,%.3f,%.3f,%.3f,%.4f,%.3f,%08x,%08x,%d,%d,%d,%d,"
                                  "%d,%d,%d,%.3f,%08x,%08x\n" % (
                                      frame, "" if pc is None else pc, li, p, u32(p + 4), u8(p + 8), u8(p + 9),
                                      u8(p + 10), u8(p + 11), u16(p + 0x2A), f[0], f[1], f[2], f[3], f[4],
                                      u32(p + 0x12), u32(p + 0x16), u8(p + 0x80), u8(p + 0x81), u8(p + 0x82),
                                      u8(p + 0x83), u8(p + 0x56), u8(p + 0x57), u8(p + 0x58), f[5], u32(p + 0x8C),
                                      u32(p + 0x90)))
                        p = u32(p)
                out.write("#%d,%s,count %d\n" % (frame, "" if pc is None else pc, len(seen)))
            except Exception as e:
                out.write("#%d,error %s\n" % (frame, e))
    if PTCLZ_WIN:
        # the 0x88 leftover probe (2026-11-03, port-gaps 56): a memory watch on every live particle's pos.z
        # (+0x48) inside the window (PC frames). psInterpretParticle0's command 0x88 ("add to position") adds a local
        # Vec at sp+0x3C / 0x40 / 0x44 whose z is only set when the command gives it; the store's callback comes before
        # the store lands, so the old z, the Vec (r1 + 0x3C..0x44), r26 (the command) and r31 (the particle) are read
        # there (ptclz_hit). Slots are reused, so every particle seen is watched, not only bank 2. Code breakpoints
        # don't fire in this fork (only on compiles), hence the watch
        pc = None
        if PTCLZ_BY_FRAME:
            pc = frame
        elif LIVEPAD and LP.get("k", -1) >= 0:
            pc = LP["anchors"][LP["k"]][2] + (frame - LP["at"])
        lo, hi = PTCLZ_WIN
        if pc is not None and lo <= pc <= hi and PTCLS_ADDRS and not PTCLS_ON:
            for a in PTCLS_ADDRS:
                memory.add_memcheck(a)
            PTCLS_ON.append(1)
        elif PTCLS_ON and (pc is None or pc > hi):
            for a in PTCLS_ADDRS:
                memory.remove_memcheck(a)
            PTCLS_ON.clear()
        if pc is not None and lo <= pc <= hi and not PTCLZ_BY_FRAME \
                and os.environ.get("PLAYREC_PTCLZ_PARTICLES", "1") == "1":
            for li in range(16):
                p, n = u32(0x80452788 + 4 * li), 0
                while ram(p) and n < 5000:
                    n += 1
                    if p + 0x48 not in PTCLZ_ADDRS:
                        PTCLZ_ADDRS.add(p + 0x48)
                        memory.add_memcheck(p + 0x48)
                    p = u32(p)
        elif PTCLZ_ADDRS and (pc is None or pc > hi):
            for a in PTCLZ_ADDRS:
                memory.remove_memcheck(a)
            PTCLZ_ADDRS.clear()
    if "pos" in PROBES:
        # the player's position every frame (2026-10-19: where the PC's Wes first leaves the recording's path in
        # Phenac), full precision, from the same record as player_pos(); with the graphics state's lastRetrace (+0x50,
        # changes once per game frame) and frameRetraces (+0x54) so the 30 fps phase shows: a row whose last_retrace
        # didn't change is a retrace with no new game frame
        try:
            xyz = None
            base, n = u32(PEOPLE_PTR), u32(PEOPLE_MAX)
            if ram(base) and n <= 512:
                for i in range(n):
                    e = base + i * ENTRY
                    if u8(e) and u32(e + 0x28) == 0 and u32(e + 0x2C) == 100:
                        m = u32(e + 0x08)
                        if ram(m):
                            # the record is a GSmodel (gs_model_struct.h): +0x18 position, +0x24 rotation (the vector
                            # GSmodelGetRotation / heroMoveGetHeroRot copy out; x, y, z angles)
                            xyz = [memory.read_f32(m + o) for o in (0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C)]
                        break
            g = u32(0x8047AA80)
            gl = (u32(g + 0x50), u32(g + 0x54)) if ram(g) or 0xE0000000 <= g < 0xE0004000 else ("", "")
            probe_out("pos.csv", "frame,room,x,y,z,last_retrace,frame_retraces,rot_x,rot_y,rot_z").write(
                "%d,%d,%s,%s,%s,%s\n" % (frame, u32(ROOM), ",".join(repr(v) for v in xyz[:3]) if xyz else ",,", gl[0],
                                         gl[1], ",".join(repr(v) for v in xyz[3:]) if xyz else ",,"))
        except Exception as e:
            probe_out("pos.csv", "frame,room,x,y,z,last_retrace,frame_retraces,rot_x,rot_y,rot_z").write(
                "# frame %d error %r\n" % (frame, e))
    if "input" in PROBES:
        # the stick the game uses, every frame (2026-10-19: Wes's turn speed comes from it; hero_move reads
        # |x| / 28, |y| / 28 after GSinputRead's filter and inputClampSticks). InputPad array 0x80401C10, 4 slots of
        # 0x6C; the slot whose id (+0x00) is 1: +0x08 outputMode, +0x10 smoothFrames (u8), +0x14 smoothMode,
        # latched PADStatus +0x18 (stick x / y s8 at +0x1A / +0x1B), current +0x24 (+0x26 / +0x27), filtered f32
        # +0x48 / +0x4C, out s8 +0x58 / +0x59. Same fields as the PC's ORRE_EVENT_LOG=2 "[ev] ... stick" lines.
        # latched_buttons / current_buttons (2026-10-21): the u16 button words of the latched (+0x18) and current
        # (+0x24) PADStatus. Sitting 9's Phenac talk: presses taken from the movie's polls reached the 30 fps scene a
        # game frame apart from the console's, and a text box's key wait (msgctrlKeyEnd) only takes a press on its frame
        hdr = ("frame,room,slot,latched_x,latched_y,current_x,current_y,filtered_x,filtered_y,out_x,out_y,"
               "smooth_frames,smooth_mode,output_mode,latched_buttons,current_buttons")
        try:
            # local names must not shadow the module's s8(value): a local s8 makes every s8 in run_probes local
            # (the outpoke / battlemsg probes then failed with UnboundLocalError, 2026-10-19)
            s8_at = lambda a: (u8(a) ^ 0x80) - 0x80
            s32_at = lambda a: (u32(a) ^ 0x80000000) - 0x80000000
            row = None
            for k in range(4):
                b = 0x80401C10 + 0x6C * k
                if u32(b) == 1:
                    row = "%d,%d,%d,%d,%d,%d,%d,%r,%r,%d,%d,%d,%d,%d,%d,%d" % (
                        frame, u32(ROOM), k, s8_at(b + 0x1A), s8_at(b + 0x1B), s8_at(b + 0x26), s8_at(b + 0x27),
                        memory.read_f32(b + 0x48), memory.read_f32(b + 0x4C), s8_at(b + 0x58), s8_at(b + 0x59),
                        u8(b + 0x10), s32_at(b + 0x14), u32(b + 0x08), u16(b + 0x18), u16(b + 0x24))
                    break
            probe_out("input.csv", hdr).write((row or "%d,%d,,,,,,,,,,,,,," % (frame, u32(ROOM))) + "\n")
        except Exception as e:
            probe_out("input.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "npc" in PROBES:
        # Phenac's wandering NPCs every frame (2026-10-19: one NPC's arrival draw in peopleMoveWander comes ~8
        # fields earlier on the PC). People table (pointer 0x8047B200, count 0x8047B1F8, 0xDC per entry; people.h
        # PeopleEntry): entries with groupId (+0x28) == NPC_GROUP, from the first frame the room is NPC_ROOM, for
        # NPC_FRAMES frames. +0x00 active, +0x2C index, +0x54 state, +0x55 subState, +0x58 moveSpeed, +0x5C walk target
        # (x, y, z), +0x74 / +0x7C home x / z, +0x80 radius, +0x84 pause timer, +0x40 facing target; position from the
        # model (+0x08) record's +0x18, as the pos probe reads Wes.
        # PLAYREC_NPC group -1: every entry (2026-10-20: P02's Drig walk, which NPC stood in Wes's way)
        hdr = "frame,room_frame,slot,active,index,state,sub,move,tx,ty,tz,hx,hz,radius,pause,yaw,x,y,z,group"
        try:
            room = u32(ROOM)
            # the visit (2026-10-25: P02's walk to Kaib, battle 21, is Phenac's second visit): count the
            # entries into NPC_ROOM and start at the NPC_VISIT-th
            if room == NPC_ROOM and st.get("npc_in") is not True:
                st["npc_visits"] = st.get("npc_visits", 0) + 1
            st["npc_in"] = room == NPC_ROOM
            if room == NPC_ROOM and st.get("npc_from") is None and st.get("npc_visits", 0) >= NPC_VISIT:
                st["npc_from"] = frame
            # PLAYREC_NPC_FROM=<frame> (2026-11-02): start at that frame whatever the room (the reference playthrough P01: after the
            # battle the room variable stays unset, and it is unset during menus and loads too, so a visit count
            # can't find the stretch)
            if NPC_FROM and st.get("npc_from") is None and frame >= NPC_FROM:
                st["npc_from"] = frame
            t0 = st.get("npc_from")
            if t0 is not None and frame - t0 < NPC_FRAMES:
                base, n = u32(PEOPLE_PTR), (u32(PEOPLE_MAX) ^ 0x80000000) - 0x80000000
                if ram(base) and 0 < n <= 512:
                    f32 = memory.read_f32
                    for i in range(n):
                        e = base + i * ENTRY
                        if NPC_GROUP >= 0 and u32(e + 0x28) != NPC_GROUP:
                            continue
                        m = u32(e + 0x08)
                        xyz = ",".join(repr(f32(m + o)) for o in (0x18, 0x1C, 0x20)) if ram(m) else ",,"
                        probe_out("npc.csv", hdr).write("%d,%d,%d,%d,%d,%d,%d,%r,%r,%r,%r,%r,%r,%r,%r,%r,%s,%d\n" % (
                            frame, frame - t0, i, u8(e), u32(e + 0x2C), u8(e + 0x54), u8(e + 0x55), f32(e + 0x58),
                            f32(e + 0x5C), f32(e + 0x60), f32(e + 0x64), f32(e + 0x74), f32(e + 0x7C), f32(e + 0x80),
                            f32(e + 0x84), f32(e + 0x40), xyz, u32(e + 0x28)))
        except Exception as e:
            probe_out("npc.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "owners" in PROBES:
        # the battle's waza-sequence owner pool every frame (2026-10-19: eye-blink draws split at battle +1808;
        # wazaSequenceOwnerUpdateBlink draws once per visible Pokémon per frame while its blink state is idle). Pool
        # pointer 0x80467CC0, u16 count 0x80467CC4, 0x8C per entry; active entries (+0x74): +0x32 index, +0x16
        # motionBusy, +0x18 flags, +0x19 blink state, +0x20 blink timer (u16), +0x1C blink texanim index (s16), +0x75
        # animationActive, +0x24 model. From the first frame the room is OWN_ROOM, for OWN_FRAMES frames.
        # + the running sequence (2026-10-25): +0x6C (wazaSequenceOwnerIsPlaying), its time counter (+0x00,
        # compared with wazaSequenceSysGetWazaTime by wazaSequenceSysIsAllEnd) and its +0x14 / +0x15 bytes; the PC's
        # ORRE_OWNERS_LOG prints the same columns
        # + wazaSequenceSysGetWazaTime's inputs: kind (+0x0C), field_10, animationMode (+0x2E) and the timing record
        # (sequence +0x3C owner -> +0x2C table + kind * 0xD4) s32s at +0x0C / +0x10 / +0x14 / +0x18
        hdr = ("frame,room_frame,slot,index,motion_busy,flags,blink_state,blink_timer,blink_texanim,anim_active,model,"
               "playing,seq_time,seq_14,seq_15,kind,f10,mode,t0c,t10,t14,t18")
        try:
            in_room = u32(ROOM) == OWN_ROOM
            if in_room and st.get("own_in") is not True:
                st["own_visits"] = st.get("own_visits", 0) + 1  # the visit (as the npc probe's)
                if OWN_VISIT == 0:
                    # visit 0 = every visit, OWN_FRAMES from each entry (2026-10-26: P12's ten battles are ten
                    # visits to room 0xD9, one replay for all of them)
                    st["own_from"] = frame
            st["own_in"] = in_room
            if in_room and st.get("own_from") is None and st.get("own_visits", 0) >= OWN_VISIT:
                st["own_from"] = frame
            t0 = st.get("own_from")
            if t0 is not None and frame - t0 < OWN_FRAMES:
                base = u32(0x80467CC0)
                n = memory.read_u16(0x80467CC4)
                if ram(base) and n <= 256:
                    for i in range(n):
                        e = base + i * 0x8C
                        if not u8(e + 0x74):
                            continue
                        tex = memory.read_u16(e + 0x1C)
                        sq = u32(e + 0x6C)
                        playing = 1 if sq else 0
                        sq_time = memory.read_s32(sq) if ram(sq) else 0
                        sq_14 = u8(sq + 0x14) if ram(sq) else 0
                        sq_15 = memory.read_s8(sq + 0x15) if ram(sq) else 0
                        kind = memory.read_s32(sq + 0x0C) if ram(sq) else 0
                        f10 = memory.read_s32(sq + 0x10) if ram(sq) else 0
                        mode = memory.read_u16(sq + 0x2E) if ram(sq) else 0
                        own = u32(sq + 0x3C) if ram(sq) else 0
                        tab = u32(own + 0x2C) if ram(own) else 0
                        tm = tab + kind * 0xD4 if ram(tab) else 0
                        tvals = [memory.read_s32(tm + o) if ram(tm) else 0 for o in (0x0C, 0x10, 0x14, 0x18)]
                        probe_out("owners.csv", hdr).write(
                            "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,0x%08x,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n" % (
                                frame, frame - t0, i, memory.read_u16(e + 0x32), u8(e + 0x16), u8(e + 0x18),
                                u8(e + 0x19), memory.read_u16(e + 0x20), tex - 0x10000 if tex > 0x7FFF else tex,
                                u8(e + 0x75), u32(e + 0x24), playing, sq_time, sq_14, sq_15, kind, f10, mode, *tvals))
        except Exception as e:
            probe_out("owners.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "windows" in PROBES:
        # when each menu window becomes ready for input (2026-10-19: replays time menu presses from the moment a
        # menu opens; snatcherLeo's decision). MenuSystem 0x80404ACC, open-window list head u32 at +0x0C; each node
        # (tagWINDOW_WORK): phase s8 +0x01, id u32 +0x04, next u32 +0x10. Ready = phase becomes 2 (menuDaemon 1 -> 2:
        # opening done, buttons read from then on). One row per change into or out of phase 2, keyed by node address
        # and id (a reopened window counts again). Frames are this replay's, the same base as replay_events.jsonl.
        hdr = "frame,event,id,node,position,phase,room"
        try:
            node, pos, seen = u32(0x80404ACC + 0x0C), 0, {}
            while ram(node) and pos < 64:
                ph = (u8(node + 0x01) ^ 0x80) - 0x80
                seen[(node, u32(node + 0x04))] = (pos, ph)
                node, pos = u32(node + 0x10), pos + 1
            prev = st.get("win_prev", {})
            out = probe_out("windows.csv", hdr)
            for key, (p, ph) in seen.items():
                was = prev.get(key, (None, None))[1]
                if ph == 2 and was != 2:
                    out.write("%d,ready,0x%x,0x%08x,%d,%d,%d\n" % (frame, key[1], key[0], p, ph, u32(ROOM)))
                elif ph != 2 and was == 2:
                    out.write("%d,left_ready,0x%x,0x%08x,%d,%d,%d\n" % (frame, key[1], key[0], p, ph, u32(ROOM)))
            for key, (p, ph) in prev.items():
                if key not in seen and ph == 2:
                    out.write("%d,closed,0x%x,0x%08x,%d,,%d\n" % (frame, key[1], key[0], p, u32(ROOM)))
            st["win_prev"] = seen
        except Exception as e:
            probe_out("windows.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "outpoke" in PROBES:
        # the battlers' state for the move pass (snatcherLeo approved 2026-10-19; offsets verified from the getters, see
        # battle-turn-flow.md "Field Pokémon record"): FIGHT_FLOOR +0xA4C4 = 8 field-slot pointers; field record
        # +0x004 battle record, +0x15C 52 condition slots, +0x604..+0x60A stat stages (Atk, Def, SpA, SpD, Spe, accuracy,
        # evasion; 6 = neutral, likely); battle record +0x004 the Pokémon copy (PokemonBios: species +0x00, HP +0x8A,
        # max HP +0x8C, condition +0x64), +0x13C one condition slot. A condition slot (16 bytes): u16 id, u16 sub id,
        # s8 turns left, u8, u8 stack count, pad, u32 value, u16. One row per field slot whenever anything changes.
        hdr = "frame,slot,species,hp,max_hp,stages,conditions"
        try:
            out = probe_out("outpoke.csv", hdr)
            prev = st.setdefault("outpoke", {})

            def conds(base, n, tag):
                got = []
                for k in range(n):
                    c = base + 0x10 * k
                    cid = u16(c)
                    if cid:
                        got.append("%s:%x/%x/t%d/n%d/v%x" % (tag, cid, u16(c + 2), s8(u8(c + 4)), u8(c + 6), u32(c + 8)))
                return got

            for i in range(8):
                op = u32(FIGHT_FLOOR + 0xA4C4 + 4 * i)
                if not ram(op):
                    if prev.get(i) is not None:
                        out.write("%d,%d,,,,,gone\n" % (frame, i))
                        prev[i] = None
                    continue
                fp = u32(op + 4)
                pk = fp + 4 if ram(fp) else 0
                stages = " ".join(str(s8(u8(op + 0x604 + k))) for k in range(7))
                cl = conds(op + 0x15C, 0x34, "out")
                if pk:
                    cl += conds(fp + 0x13C, 1, "fight") + conds(pk + 0x64, 1, "mon")
                row = (u16(pk) if pk else -1, u16(pk + 0x8A) if pk else -1, u16(pk + 0x8C) if pk else -1, stages,
                       " ".join(cl))
                if row != prev.get(i):
                    prev[i] = row
                    out.write("%d,%d,%d,%d,%d,%s,%s\n" % ((frame, i) + row))
        except Exception as e:
            probe_out("outpoke.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "battlemsg" in PROBES:
        # battle messages in order (move pass): the battle message box is the window with menu id 0x50
        # (winMsgOpenFight, message-windows.md); its first open parameter (node +0x60) and its work area's first word
        # (node +0x9C: windowGetFreeWork, winMsgCtrl copies the parameter there) hold the message id passed to
        # winMsgOpenFight. One row whenever the id changes, a new box opens, or the box closes.
        hdr = "frame,event,message_id,param0,node,phase"
        try:
            node, pos, cur = u32(0x80404ACC + 0x0C), 0, None
            while ram(node) and pos < 64:
                if u32(node + 0x04) == 0x50:
                    cur = (node, u32(node + 0x9C), u32(node + 0x60), s8(u8(node + 0x01)))
                    break
                node, pos = u32(node + 0x10), pos + 1
            last = st.get("bmsg")
            out = probe_out("battlemsg.csv", hdr)
            if cur and (not last or cur[0] != last[0] or cur[1] != last[1]):
                out.write("%d,shown,0x%x,0x%x,0x%08x,%d\n" % (frame, cur[1], cur[2], cur[0], cur[3]))
            elif not cur and last:
                out.write("%d,closed,0x%x,0x%x,0x%08x,\n" % (frame, last[1], last[2], last[0]))
            st["bmsg"] = cur
        except Exception as e:
            probe_out("battlemsg.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "camparams" in PROBES:
        # the move camera's computed parameters (2026-10-21; snatcherLeo: battle cameras differ): lbl_804673A0, 13 words
        # (wazaSequenceCamera.c WazaSequenceCameraParams: motion s32, then size, height, rotationMin / Max / Base,
        # heightMin / Max, distanceMin / Max, near / middle / far lengths), a row when any changes. The PC's
        # ORRE_CAMPARAMS_LOG=1 prints the same row
        hdr = "frame,room,motion,size,height,rot_min,rot_max,rot_base,h_min,h_max,d_min,d_max,near,mid,far"
        try:
            words = [u32(0x804673A0 + 4 * k) for k in range(13)]
            if words != st.get("camp"):
                import struct as _st
                vals = [str((words[0] ^ 0x80000000) - 0x80000000)] + \
                       ["%.4f" % _st.unpack(">f", w_.to_bytes(4, "big"))[0] for w_ in words[1:]]
                probe_out("camparams.csv", hdr).write("%d,%d,%s\n" % (frame, u32(ROOM), ",".join(vals)))
                st["camp"] = words
        except Exception as e:
            probe_out("camparams.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "fieldmsg" in PROBES:
        # field text boxes, page by page (2026-10-21, sitting 9's Phenac arrival: the PC closed the first
        # conversation 27 frames late). The field message window is menu 0x40 (message-windows.md); its +0x01 phase,
        # +0x98 "finished" and +0x99 bytes, and the GSmsg entry its work area's first word names (the entry table
        # _pMw (0x80478B08): u16 count, +0x20 the 0x68-byte entries, key at +0x1C; GSmsgIsCheck). One row whenever any of
        # it changes; the PC's ORRE_MSG_LOG prints the same columns (pc_debug_game.c)
        hdr = "frame,phase,finished,b99,key,entry"
        try:
            node, pos, row = u32(0x80404ACC + 0x0C), 0, None
            while ram(node) and pos < 64:
                # PLAYREC_MSGWINDOW=<id> (2026-10-29): another message window than the field box 0x40 (P09's
                # menu message 0x10); the PC's ORRE_MSG_WINDOW is the same
                if u32(node + 0x04) == int(os.environ.get("PLAYREC_MSGWINDOW", "0x40"), 0):
                    key = u32(node + 0x9C)
                    entry = ""
                    head = u32(0x80478B08)
                    if ram(head):
                        base = u32(head + 0x20)
                        for i in range(min(u16(head), 64) if ram(base) else 0):
                            e = base + i * 0x68
                            if u8(e) and u32(e + 0x1C) == key:
                                entry = "".join("%08x" % u32(e + o) for o in range(0, 0x68, 4))
                                break
                    row = "%d,%d,%d,0x%x,%s" % (s8(u8(node + 0x01)), u8(node + 0x98), u8(node + 0x99), key, entry)
                    break
                node, pos = u32(node + 0x10), pos + 1
            if row != st.get("fmsg"):
                probe_out("fieldmsg.csv", hdr).write("%d,%s\n" % (frame, row if row else ",,,,"))
                st["fmsg"] = row
        except Exception as e:
            probe_out("fieldmsg.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "fade" in PROBES:
        # the screen fade (2026-10-20: Wes's name screen leaves one frame early on the PC, right after a 0.5 s
        # fade to black): FadeState _fadeWork (0x80466E30, fade.c): +0x00 mode, +0x01 pending, +0x02 type, +0x04 duration,
        # +0x08 elapsed; a row whenever any changes. The PC's ORRE_FADE_LOG=1 prints the same row
        hdr = "frame,mode,pending,type,duration,elapsed"
        try:
            row = "%d,%d,%d,%r,%r" % (u8(0x80466E30), u8(0x80466E31), memory.read_u16(0x80466E32),
                                      memory.read_f32(0x80466E34), memory.read_f32(0x80466E38))
            if row != st.get("fade"):
                probe_out("fade.csv", hdr).write("%d,%s\n" % (frame, row))
                st["fade"] = row
        except Exception as e:
            probe_out("fade.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "heromove" in PROBES:
        # heroMoveMain's early-return inputs, once per frame (2026-11-06; the project's P10 note 4 part 2: the PC starts a
        # talk from an A press inside the cup script's heroMoveMain loop, the console doesn't). Pad 1's slot in the
        # InputPad table lbl_80401C10 (0x6C each; id +0x00, current.button u16 +0x24, prevButton u32 +0x30), so
        # held & changed & 0x1C00 is the talk test; HeroMoveWork HeroMove (0x80426BD0; leader +0x000, lockFrame +0x188,
        # autoEvent[5] +0x18C, eventValue[3] +0x410); menuIsCheck(0xCA) by the window list (0x80404ACC +0x0C, next
        # +0x10, id +0x04); GSscene_GetMode = the byte behind the pointer at 0x80478C40. A row whenever any changes
        # latched = the sampling callback's copy (+0x18 button), to tell a frozen current from a held pad
        # cb = inputPadCallback's call counter lbl_8047AC48 (one per PAD sampling callback), logged every frame
        hdr = "frame,held,prev,talk_test,lockFrame,leader,autoEvent0,autoEvent1,eventValues,menuCA,sceneMode,latched,cb"
        try:
            held = prev = latched = 0
            for i in range(4):
                p = 0x80401C10 + 0x6C * i
                if u32(p) == 1:
                    held, prev, latched = memory.read_u16(p + 0x24), u32(p + 0x30), memory.read_u16(p + 0x18)
                    break
            hm = 0x80426BD0
            node, pos, ca = u32(0x80404ACC + 0x0C), 0, 0
            while ram(node) and pos < 64:
                if u32(node + 0x04) == 0xCA:
                    ca = 1
                node, pos = u32(node + 0x10), pos + 1
            sp = u32(0x80478C40)
            row = "0x%04x,0x%04x,0x%x,%d,%d,0x%08x,0x%x,%d/%d/%d,%d,%s,0x%04x" % (
                held, prev, held & (held ^ prev) & 0x1C00, u32(hm + 0x188), u32(hm), u32(hm + 0x18C),
                u32(hm + 0x190), u32(hm + 0x410), u32(hm + 0x414), u32(hm + 0x418), ca,
                u8(sp) if ram(sp) else "-", latched)
            win = os.environ.get("PLAYREC_HEROMOVE_EVERY", "")      # "<from>-<to>": a row every frame there
            every = False
            if win:
                lo, _, hi = win.partition("-")
                every = int(lo) <= frame <= int(hi)
            if row != st.get("heromove") or every:
                probe_out("heromove.csv", hdr).write("%d,%s,%d\n" % (frame, row, u32(0x8047AC48)))
                st["heromove"] = row
        except Exception as e:
            probe_out("heromove.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "cursoritem" in PROBES:
        # what windowGetCursorToItem would return, once per frame (2026-11-06; the project's port-gaps 73, the summary's
        # move-switch highlight): for each open window whose id is in PLAYREC_CURSORWIN (default 0x53,0x57,0x58), the
        # window's cursor (+0x94 x, +0x95 y, s8) and the walk: menu data _menu_data + id * 0x1C, its +0x04 s16 first
        # item index; items lbl_802EF0A8 + index * 0x1C (byte 0 bit 7 selectable, bit 6 last; +0x0C s16 cursorSprite;
        # +0x18 s16 next). The selectable item whose rank equals cursor y, else NULL. A row whenever any changes
        hdr = "frame,window,node,cursor_x,cursor_y,item,cursorSprite,items"
        try:
            want = [int(x, 0) for x in os.environ.get("PLAYREC_CURSORWIN", "0x53,0x57,0x58").split(",")]
            node, pos, rows = u32(0x80404ACC + 0x0C), 0, []
            while ram(node) and pos < 64:
                wid = u32(node + 0x04)
                if wid in want:
                    cy = (u8(node + 0x95) ^ 0x80) - 0x80
                    cx = (u8(node + 0x94) ^ 0x80) - 0x80
                    idx = (memory.read_u16(0x802E2DB8 + wid * 0x1C + 0x04) ^ 0x8000) - 0x8000
                    found, rank, walk, n = 0, 0, [], 0
                    while 0 <= idx < u32(0x80478968) and n < 64:
                        it = 0x802EF0A8 + idx * 0x1C
                        b0 = u8(it)
                        cs = (memory.read_u16(it + 0x0C) ^ 0x8000) - 0x8000
                        walk.append("%d%s%s:%d" % (idx, "s" if b0 & 0x80 else "", "L" if b0 & 0x40 else "", cs))
                        if b0 & 0x80:
                            if rank == cy and not found:
                                found = it
                            rank += 1
                        if b0 & 0x40:
                            break
                        idx = (memory.read_u16(it + 0x18) ^ 0x8000) - 0x8000
                        n += 1
                    fcs = (memory.read_u16(found + 0x0C) ^ 0x8000) - 0x8000 if found else ""
                    rows.append("0x%x,0x%08x,%d,%d,0x%08x,%s,%s" % (wid, node, cx, cy, found, fcs, " ".join(walk)))
                node, pos = u32(node + 0x10), pos + 1
            row = "|".join(rows)
            if row != st.get("curitem"):
                out = probe_out("cursoritem.csv", hdr)
                for r in rows or [",,,,,,"]:
                    out.write("%d,%s\n" % (frame, r))
                st["curitem"] = row
        except Exception as e:
            probe_out("cursoritem.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "aifield" in PROBES:
        # the field order the battle AI walks (2026-10-26; the project's P12 battle 207 question): fightFloorLoopValid-
        # FightOutPokemon's flag-0 walk takes side -> trainer -> field index through fightFloorBiosGetFightSidePtr
        # (floor +0x14 + side * 0x5230), fightSideBiosGetFightTrainerPtr (+0x64 + trainer * 0x28E4) and
        # fightTrainer_GetFightoutPokemonPtr (+0x1314 + index * 0x6E0): records inside the floor, no pointers to follow.
        # Per (side, trainer, index): the field record's address, its battle record (+0x04) and that record's species
        # (the Pokémon copy inline at +0x04, species first; *fixed 2026-10-26*, first read as a pointer, so P12
        # replay_3's species column is 0: name the records from its send_out events); then the 8 field-slot pointers
        # (floor +0xA4C4) to check the addresses. Verified on P12 replay_3 (matched the recording): battle 207's walk
        # was [Noctowl …852, Gengar …192] while the slot array held Gengar first.
        # A row whenever anything changes, while a battle is set up (floor +0x12 nonzero). Reads only
        hdr = "frame,battle,walk,slots"
        try:
            if u16(FIGHT_FLOOR + 0x12):
                walk = []
                for s in range(2):
                    for t in range(2):
                        for i in range(2):
                            a = FIGHT_FLOOR + 0x14 + s * 0x5230 + 0x64 + t * 0x28E4 + 0x1314 + i * 0x6E0
                            rec = u32(a + 4)
                            sp = u16(rec + 4) if ram(rec) else 0  # the Pokémon copy is inline at +0x04
                            walk.append("%d%d%d:%08x/%08x/%d" % (s, t, i, a, rec, sp))
                slots = " ".join("%08x" % u32(FIGHT_FLOOR + 0xA4C4 + 4 * k) for k in range(8))
                row = "%d,%s,%s" % (u16(FIGHT_FLOOR + 0x12), " ".join(walk), slots)
                if row != st.get("aifield"):
                    probe_out("aifield.csv", hdr).write("%d,%s\n" % (frame, row))
                    st["aifield"] = row
        except Exception as e:
            probe_out("aifield.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "aiaction" in PROBES:
        # each field Pokémon's queued move (2026-10-26; the project's P12 battle 207 question: which target id the AI
        # queued, and what it resolved to): the move slot (FightWaza) at field record +0x49C, filled by fightWazaCreate:
        # +0x00 u8 move index (fightWazaBiosSetWazaBanme), +0x02 u16 original move (SetMotoWazaDataId), +0x04 u16 move
        # in use (SetUseWazaDataId), +0x06 u16 target id (SetTargetDataId); records in the aifield walk order
        # (side, trainer, index). Then the floor's attacker / defender (+0xA474 / +0xA478, field record pointers).
        # A row whenever anything changes, while a battle is set up. Reads only
        hdr = "frame,battle,moves,atk,def"
        try:
            if u16(FIGHT_FLOOR + 0x12):
                mv = []
                for s in range(2):
                    for t in range(2):
                        for i in range(2):
                            a = FIGHT_FLOOR + 0x14 + s * 0x5230 + 0x64 + t * 0x28E4 + 0x1314 + i * 0x6E0
                            w = a + 0x49C
                            # the battle record's moves and PP (record +0x04 copy: move u16 at +0x78 + 4k, PP u8 at
                            # +0x7A + 4k) and its one condition slot id (+0x13C), for the AI's usable-move count
                            rec = u32(a + 4)
                            pp = ".".join("%d:%d" % (u16(rec + 4 + 0x78 + 4 * k), u8(rec + 4 + 0x7A + 4 * k))
                                          for k in range(4)) if ram(rec) else "-"
                            cond = u16(rec + 0x13C) if ram(rec) else 0
                            mv.append("%d%d%d:%08x/b%d/m%d/u%d/t%x/pp%s/c%x" % (
                                s, t, i, a, u8(w), u16(w + 2), u16(w + 4), u16(w + 6), pp, cond))
                row = "%d,%s,%08x,%08x" % (u16(FIGHT_FLOOR + 0x12), " ".join(mv), u32(FIGHT_FLOOR + 0xA474),
                                           u32(FIGHT_FLOOR + 0xA478))
                if row != st.get("aiaction"):
                    probe_out("aiaction.csv", hdr).write("%d,%s\n" % (frame, row))
                    st["aiaction"] = row
        except Exception as e:
            probe_out("aiaction.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "steps" in PROBES:
        # the frame step behind each fade step (2026-10-26; the project's candidate 2): fadeDaemon advances elapsed by
        # GSgetFrameStep() / fps, and GSgetFrameStep is the graphics state's frameRetraces (+0x54), set in
        # GSgfxWaitRetrace from its own retrace counter (+0x4C, counted in HSD's post-retrace callback) minus +0x50.
        # One row per label (reads only, no watch): retraceCount, lastRetrace, frameRetraces, minFrameRetraces,
        # waitRetrace, the fade's elapsed. A frame whose step is 0 leaves +0x50 unchanged, so frames.csv can't see it
        hdr = "frame,retraceCount,lastRetrace,frameRetraces,minFrameRetraces,waitRetrace,elapsed"
        try:
            g = u32(0x8047AA80)
            if ram(g) or 0xE0000000 <= g < 0xE0004000:
                probe_out("steps.csv", hdr).write("%d,%d,%d,%d,%d,%d,%r\n" % (
                    frame, u32(g + 0x4C), u32(g + 0x50), u32(g + 0x54), u32(g + 0x58), u8(g + 0x5C),
                    memory.read_f32(0x80466E38)))
        except Exception as e:
            probe_out("steps.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "win" in PROBES:
        # every open menu window and its phase (2026-10-20; Wes's name screen: the name list's close animation
        # ends a frame sooner on the PC): the window list (winSysWork 0x80404ACC +0x0C, next +0x10, phase +0x01, id +0x04) as
        # "id:phase", a row whenever it changes. The PC's ORRE_WIN_LOG=1 prints the same row (pc_debug_game.c)
        hdr = "frame,windows"
        try:
            node, n, parts = u32(0x80404ACC + 0x0C), 0, []
            while ram(node) and n < 32:
                parts.append("%x:%d" % (u32(node + 4), s8(u8(node + 1))))
                node, n = u32(node + 0x10), n + 1
            row = " ".join(parts)
            if row != st.get("win"):
                probe_out("win.csv", hdr).write("%d,%s\n" % (frame, row))
                st["win"] = row
        except Exception as e:
            probe_out("win.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "floor" in PROBES:
        # the room manager's state (lbl_8047ACD8), the current room context's resource step (lbl_8047ACC8 -> +0x10
        # isActive, fn_80100B24's phases 1-6) and the fade's pending flag (0x80466E31), a row whenever one changes
        # (2026-10-25; P01's Folly battle: the room change after the battle starts a frame later on the PC). The
        # PC's ORRE_FLOOR_LOG=1 prints the same row (pc_debug_game.c)
        hdr = "frame,state,step,fade"
        try:
            ctx = u32(0x8047ACC8)
            step = u32(ctx + 0x10) if ram(ctx) else -1
            if step != -1 and step >= 0x80000000:
                step -= 1 << 32
            row = "%d,%d,%d" % (u32(0x8047ACD8), step, u8(0x80466E31))
            if row != st.get("floor"):
                probe_out("floor.csv", hdr).write("%d,%s\n" % (frame, row))
                st["floor"] = row
        except Exception as e:
            probe_out("floor.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "sfx" in PROBES:
        # every sound start and stop, battle cries and move sounds included (2026-10-20; snatcherLeo: "sound stuff
        # included"): the sound table (snd_data -> 12-byte GSsndEntry, count *snd_data_number) has a work pointer at
        # +0x08 while a sound plays, and the work's +0x08 is the MusyX handle of its latest start. A row whenever the
        # set changes: "started" = ids whose work appeared or whose handle changed, "stopped" = ids whose work went.
        # The PC's ORRE_SFX_LOG=1 prints the same row (pc_debug_game.c); sound_compare.py --sfx compares them
        hdr = "frame,started,stopped"
        try:
            n, table, live = u32(u32(0x80478FA8)), u32(0x80478FAC), {}
            if ram(table) and 0 < n < 4096:
                try:
                    raw = memory.read_bytes(table, n * 12)
                    works = [int.from_bytes(raw[i * 12 + 8:i * 12 + 12], "big") for i in range(n)]
                except Exception:
                    works = [u32(table + i * 12 + 8) for i in range(n)]
                for i, w in enumerate(works):
                    if ram(w):
                        live[i] = u32(w + 8)
            old = st.get("sfx") or {}
            started = [i for i, h in live.items() if old.get(i) != h]
            stopped = [i for i in old if i not in live]
            if started or stopped:
                probe_out("sfx.csv", hdr).write("%d,%s,%s\n" % (frame, " ".join(map(str, started)),
                                                               " ".join(map(str, stopped))))
            if os.environ.get("PLAYREC_SFX_HANDLES") == "1":
                # (2026-11-03, the project's port-gaps 53: music ids 1094 / 1173 / 1118 show as started at N and N+1 on
                # the console and once on the PC) the handle before and after for every start
                for i in started:
                    probe_out("sfxhandles.csv", "frame,id,old_handle,new_handle").write(
                        "%d,%d,%s,%08x\n" % (frame, i, "%08x" % old[i] if i in old else "-", live[i]))
            st["sfx"] = live
        except Exception as e:
            probe_out("sfx.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "dvdx" in PROBES and not st.get("dvdx_on"):
        # every disc read at the retrace it started and ended (2026-10-21): a memory watch on the SDK DVD
        # driver's command-in-progress pointer (executing_8047A7E8) fires on each write, and the console's own retrace
        # counter (retraceCount 0x8047A84C, vi.c) says when. The per-frame dvd probe sees a 30 fps scene's reads only at
        # its polls, two retraces apart, which hid the one retrace that put Growl's effect a game frame late on the PC.
        # Same columns as dvd.csv (frames = this replay's frame labels: the retrace count plus the label offset measured
        # at the watch's first frame), plus the exact retraceCount at start and end.
        # **The watch disturbs the replay** (verified 2026-10-21, snatcherLeo saw it: Folly's Whismur used other moves): with
        # it on, P01's RNG left the clean replay's at frame 24825 (Folly's battle start). Use its rows only before the
        # first RNG difference against a clean replay of the same sitting, and never as the sitting's reference replay
        # for anything else
        st["dvdx_on"] = True
        st["dvdx_label"] = frame - u32(0x8047A84C)
        st["dvdx_room"], st["dvdx_room_at"] = u32(ROOM), frame
        try:
            memory.add_memcheck(0x8047A7E8)
        except Exception as e:
            probe_out("dvdx.csv", "start_frame,end_frame,frames,disc_offset,length,seek_from_prev_end,room,room_frame,"
                                  "start_retrace,end_retrace").write("# add_memcheck failed: %r\n" % (e,))
    if "dvdx" in PROBES and u32(ROOM) != st.get("dvdx_room"):
        st["dvdx_room"], st["dvdx_room_at"] = u32(ROOM), frame
    if "fsys" in PROBES:
        # the archive cache (2026-10-21, P01's Willie battle: the console's send-out found wzx_monsterball_out
        # loaded and read nothing, the PC loaded it, and the battle's timing parted there): every FSYS slot in use
        # (manager lbl_80453FEC +0x00 slot count, slot array *lbl_8047B1B4, 0x140 bytes each; fsys.h FSYSSlot:
        # +0x48 status, +0x60 references, +0xF8 archive id, +0x70 file name), a row whenever the set changes, as
        # "id:status:refs:name" separated by spaces. The PC's ORRE_FSYS_LOG=1 prints the same row
        hdr = "frame,room,slots"
        try:
            n, base, parts = u32(0x80453FEC), u32(0x8047B1B4), []
            if ram(base) and 0 < n <= 256:
                for i in range(n):
                    s = base + i * 0x140
                    status = (u32(s + 0x48) ^ 0x80000000) - 0x80000000
                    if status == 0:
                        continue
                    name = bytes(memory.read_u8(s + 0x70 + k) for k in range(0x40)).split(b"\0")[0].decode("ascii", "replace")
                    parts.append("%x:%d:%d:%s" % (u32(s + 0xF8), status, (u32(s + 0x60) ^ 0x80000000) - 0x80000000,
                                                  name.rsplit("/", 1)[-1]))
            row = " ".join(parts)
            if row != st.get("fsys"):
                probe_out("fsys.csv", hdr).write("%d,%d,%s\n" % (frame, u32(ROOM), row))
                st["fsys"] = row
        except Exception as e:
            probe_out("fsys.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "party" in PROBES:
        # the party's species in slot order (2026-10-25; P05's box reorder before the first street battle): the
        # save work block *0x8047ADB8, the hero record +0x70, its six party records +0x30 (0x138 bytes each, species
        # u16 first; saveedit.py's PARTY / REC); a row whenever the order changes. The PC's ORRE_PARTY_LOG=1 prints
        # the same row
        hdr = "frame,room,party"
        try:
            base = u32(0x8047ADB8)
            if ram(base):
                row = " ".join(str(u16(base + 0xA0 + 0x138 * i)) for i in range(6))
                if row != st.get("party"):
                    probe_out("party.csv", hdr).write("%d,%d,%s\n" % (frame, u32(ROOM), row))
                    st["party"] = row
        except Exception as e:
            probe_out("party.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "heap" in PROBES and frame % HEAP_EVERY == 0:
        # the SDK heaps' use (2026-11-07; P12 crashed when heap 0, HSD_MemAlloc's 3,628,544 bytes, ran out in a
        # Realgam Tower battle on the PC): the heap table *HeapArray (0x8047A6E8) holds OSHeapDesc {s32 size, cell* free, cell*
        # allocated} (0x0C each; count lbl_8047A6EC); each cell {prev, next, s32 size} with size counting its 0x20-byte
        # header (OSAllocFromHeap rounds every request up so). One row per heap every HEAP_EVERY frames: the bytes and
        # cells in use, the free bytes, cells and largest free cell. heap_sizes.csv: heap 0's in-use cells by size
        # ("size:count", most bytes first) every 18,000 frames and whenever heap 0's use reaches a new high
        hdr = "frame,room,heap,size,used,cells,free,free_cells,largest_free"
        try:
            tab, n = u32(0x8047A6E8), u32(0x8047A6EC)
            if ram(tab):
                for h in range(min(n, 8)):
                    d = tab + 0x0C * h
                    size = u32(d)
                    if size & 0x80000000:          # an unused slot (size -1)
                        continue
                    sums = []
                    by = {}
                    for head in (u32(d + 8), u32(d + 4)):
                        tot, cnt, big, c = 0, 0, 0, head
                        while ram(c) and cnt < 50000:
                            s = u32(c + 8)
                            tot += s
                            cnt += 1
                            big = max(big, s)
                            if h == 0 and head == u32(d + 8):
                                by[s] = by.get(s, 0) + 1
                            c = u32(c + 4)
                        sums.append((tot, cnt, big))
                    (used, cells, _), (free, fcells, big) = sums
                    probe_out("heap.csv", hdr).write("%d,%d,%d,%d,%d,%d,%d,%d,%d\n" % (
                        frame, u32(ROOM), h, size, used, cells, free, fcells, big))
                    if h == 0 and (used > st.get("heap0_hi", 0) or frame % 18000 == 0):
                        why = "high" if used > st.get("heap0_hi", 0) else "every18k"
                        st["heap0_hi"] = max(used, st.get("heap0_hi", 0))
                        row = " ".join("%d:%d" % (s, k) for s, k in sorted(by.items(), key=lambda x: -x[0] * x[1]))
                        probe_out("heap_sizes.csv", "frame,room,why,used,sizes").write(
                            "%d,%d,%s,%d,%s\n" % (frame, u32(ROOM), why, used, row))
        except Exception as e:
            probe_out("heap.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "tms" in PROBES:
        # the TM pocket in slot order (2026-10-29; P09's TM list after Miror B.: row 0 held another TM on the PC):
        # the save work block *0x8047ADB8, the hero record +0x70, its pockets after the six party records (+0x780):
        # normal 0x14, key 0x2B, balls 0x10 entries of {u16 item, u16 count}, then the TMs (0x40 entries, block +0x92C;
        # saveedit.py's pocket layout); "item:count" per used slot, a row whenever it changes. The PC's ORRE_PARTY_LOG=2
        # prints the same row ("[tms]")
        hdr = "frame,room,tms"
        try:
            base = u32(0x8047ADB8)
            if ram(base):
                # field by field: this Dolphin build's memory module has no read_bytes (the first version logged that
                # error every frame)
                cells = [(u16(base + 0x92C + 4 * i), u16(base + 0x92C + 4 * i + 2)) for i in range(0x40)]
                row = " ".join("%d:%d" % c for c in cells if c[0])
                if row != st.get("tms"):
                    probe_out("tms.csv", hdr).write("%d,%d,%s\n" % (frame, u32(ROOM), row))
                    st["tms"] = row
        except Exception as e:
            probe_out("tms.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "cache" in PROBES:
        # the fsys ARAM cache's nodes (2026-10-25; P05's first street battle: the portrait archive's table of
        # contents cached on the PC, read from the disc on the console): lbl_80454038 -> 0x400 nodes of 0x20 bytes
        # (gs_range_8017FA5C_shared.h GsRangeCacheNode: +0x00 data (ARAM address), +0x0C size, +0x10 file handle,
        # +0x14 / +0x18 keys, +0x1C active), a row of the active ones whenever the set changes, as
        # "node:handle:key1:key2:size:aram" separated by spaces. The PC's ORRE_CACHE_LOG=1 prints the same row
        hdr = "frame,room,nodes"
        try:
            base, parts = u32(0x80454038), []
            if ram(base):
                for i in range(0x400):
                    n = base + i * 0x20
                    if u32(n + 0x1C) == 0:
                        continue
                    parts.append("%d:%x:%x:%x:%x:%x" % (i, u32(n + 0x10), u32(n + 0x14), u32(n + 0x18), u32(n + 0x0C),
                                                        u32(n)))
            row = " ".join(parts)
            if row != st.get("cache"):
                probe_out("cache.csv", hdr).write("%d,%d,%s\n" % (frame, u32(ROOM), row))
                st["cache"] = row
        except Exception as e:
            probe_out("cache.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "dvdseen" in PROBES:
        # which frame's poll first sees each FSYS read done (2026-10-21; the project's option (b): the PC hands a read over
        # at the start of the frame the console's game first noticed it). fsysUpdateTask runs once a frame and stores
        # each slot's DVDGetCommandBlockStatus in slot +0x54 (fsysUpdateSlot; 0 = done, 1 = busy) before the slot's
        # handler may start its next read. So at the frame's end, +0x54 is what this frame's poll saw, about the read
        # the slot held at the end of the previous frame. Per slot in use (+0x40 archive data, +0x68 GSDVDWork): the
        # command block at work +0x04 (+0x0C state, +0x10 disc offset, +0x14 length) and +0x54; a row when any of them
        # changes, and once more the frame after a change (a read that starts and ends between two polls changes
        # nothing else). dvd_seen.py turns the rows into "<dvd.csv row> <line> <frame>". No breakpoints (they report
        # only a first hit) and no memory watch: per-frame reads only.
        # *Updated 2026-10-21:* the header (64 bytes) and table-of-contents reads come before the slot's archive data
        # is set, so the poll skips them; they complete through fsysDVDReadCallback, which moves the slot's status
        # (+0x48: 1 -> 2, 3 -> 4, 101 -> 150, 200 -> 201, 303 -> 304, 400 -> 401) at the drive's interrupt, and the next
        # fsysUpdateTask runs the new status's handler. So every slot with a work pointer is logged now (not only armed
        # ones), with its status and whether the archive data is set ("armed": the poll's +0x54 is current only then).
        hdr = "frame,slot,offset,length,state,polled,status,armed"
        try:
            n, base = u32(0x80453FEC), u32(0x8047B1B4)
            prev = st.setdefault("dvdseen", {})
            again = st.setdefault("dvdseen_again", set())
            if ram(base) and 0 < n <= 256:
                for i in range(n):
                    s = base + i * 0x140
                    work = u32(s + 0x68)
                    status = (u32(s + 0x48) ^ 0x80000000) - 0x80000000
                    if not ram(work) or (status == 0 and i not in prev):
                        continue
                    cb = work + 4
                    cur = (u32(cb + 0x10), u32(cb + 0x14), (u32(cb + 0x0C) ^ 0x80000000) - 0x80000000,
                           (u32(s + 0x54) ^ 0x80000000) - 0x80000000, status, 1 if u32(s + 0x40) else 0)
                    changed = prev.get(i) != cur
                    if changed or i in again:
                        probe_out("dvdseen.csv", hdr).write("%d,%d,0x%x,%d,%d,%d,%d,%d\n" % ((frame, i) + cur))
                    if changed:
                        again.add(i)
                    else:
                        again.discard(i)
                    prev[i] = cur
        except Exception as e:
            probe_out("dvdseen.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    if "rngcall" in PROBES and RNGCALL and not FADETASK:
        # arm the RNG-state watch inside the window only (a watch left on parts a long replay; see the passes probe)
        inside = any(lo <= frame <= hi for lo, hi in RNGCALL)
        if inside and not st.get("rc_armed"):
            st["rc_addr"] = u32(RNG_PTR)
            memory.add_memcheck(st["rc_addr"])
            memory.add_memcheck(0x8047AC00)  # the running GS thread (GSthreadGetCurrentThread): each switch
            st["rc_armed"], st["rc_last"], st["rc_thread"] = True, u32(st["rc_addr"]), u32(0x8047AC00)
        elif not inside and st.get("rc_armed"):
            memory.remove_memcheck(st["rc_addr"])
            memory.remove_memcheck(0x8047AC00)
            st["rc_armed"] = False
        st["rc_frame"] = frame
    if ROLLINJECT:
        # battle N starts at _fightInitialize (2026-11-02, from P25's live run: the opponent's team is made there,
        # 958 creation draws before GSthreadCreate, the handle still 0; the PC tags them serial + 1). Its first store
        # is lbl_8047B5DA = 1 (after the debug-menu check); fn_801EF5C0 stores 0 at the battle's end. A watch on that
        # flag is on for the whole replay (written twice per battle; on_inject starts the battle at its store of 1);
        # the RNG watch is on only inside battles and goes off once the flag is 0 again
        if not st.get("ij_flagwatch"):
            memory.add_memcheck(FIGHT_FLAG)
            st["ij_flagwatch"] = True
        # particle draws ran free this frame (FREEPTCL, on_inject): the next recorded entry's state goes back now
        if st.get("ij_dirty") and st.get("ij_phase") is None and st.get("ij_armed"):
            inject_next()
            st["ij_dirty"] = False
        # (also once the battle's recorded sequence is used up: nothing more to inject, and the watch slows the replay;
        # P25 live4 logged 146k other draws after its battle with the watch still on)
        if FIELD and st.get("entries", 0) > 0 and (field_due() or in_battle()):
            # with a field list the RNG watch is on near each field draw (field_due) and in battles
            if not st.get("ij_armed"):
                st["ij_addr"] = u32(RNG_PTR)
                st["ij_phase"] = None
                memory.add_memcheck(st["ij_addr"])
                st["ij_armed"] = True
                inject_next()
        elif FIELD:
            if st.get("ij_armed") and st.get("ij_phase") is None:
                memory.remove_memcheck(st["ij_addr"])
                st["ij_armed"] = False
        elif st.get("ij_armed") and (not memory.read_u8(FIGHT_FLAG)
                                     or st.get("ij_i", 0) >= len(INJ.get(st.get("ij_battle"), []))):
            memory.remove_memcheck(st["ij_addr"])
            st["ij_armed"] = False
    if RNGTEST and frame >= RNGTEST and not st.get("rt_armed") and ram(u32(RNG_PTR)):
        st["rt_addr"] = u32(RNG_PTR)    # the injection test (on_rngtest below)
        memory.add_memcheck(st["rt_addr"])
        st["rt_armed"] = True
    if "passes" in PROBES and not FADETASK:
        # the passes probe (on_pass_write below): close the previous frame's count, write it when it isn't 1
        try:
            if not st.get("pass_watch"):
                memory.add_memcheck(0x8047AC94)
                st["pass_watch"] = True
                st["pass_count"] = 0
            else:
                n = st.get("pass_count", 0)
                if n != 1:
                    probe_out("passes.csv", "frame,passes").write("%d,%d\n" % (frame - 1, n))
                st["pass_count"] = 0
        except Exception as e:
            probe_out("passes.csv", "frame,passes").write("# frame %d error %r\n" % (frame, e))
    if FADETASK and FADETASK[0] <= frame <= FADETASK[1]:
        # PLAYREC_FADETASK=lo-hi (2026-10-26; the project's moment (A): the console's fade-in holds a step for a frame).
        # Every label in the window: the fade state (fade.c FadeState at 0x80466E30: +0 mode, +1 pending, +2 type,
        # +4 duration, +8 elapsed), the previous frame's length (the gfx state *0x8047AA80 +0x54 frameRetraces, which
        # fadeDaemon's step uses through GSgetFrameStep), and every gapp task of priority 0xFE (mainRetraceLogic, which
        # runs fadeDaemon; the list at *0x8047AC98, GSgappTask: +0x04 next, +0x08 state, +0x0C priority, +0x0D blocked,
        # +0x14 callback). The memory watch on elapsed (below) adds a row per write, to tell a skipped step from a
        # fadeSet that zeroed elapsed in the same frame.
        hdr = "frame,kind,mode,pending,type,duration,elapsed,frame_retraces,task_state,task_blocked,task_callback"
        try:
            f32 = memory.read_f32
            g = u32(0x8047AA80)
            fr = u32(g + 0x54) if ram(g) else -1
            tasks, t, guard = [], u32(0x8047AC98), 0
            while ram(t) and guard < 64:
                if u8(t + 0x0C) == 0xFE:
                    tasks.append((u32(t + 0x08), u8(t + 0x0D), u32(t + 0x14)))
                t, guard = u32(t + 0x04), guard + 1
            ts = tasks[0] if tasks else ("", "", 0)
            probe_out("fadetask.csv", hdr).write("%d,label,%d,%d,%d,%r,%r,%d,%s,%s,0x%08x\n" % (
                frame, u8(0x80466E30), u8(0x80466E31), u16(0x80466E32), f32(0x80466E34), f32(0x80466E38), fr,
                ts[0], ts[1], ts[2]))
            if not st.get("fadetask_watch"):
                for addr in [0x80466E38] + WATCH_ADDRS:
                    memory.add_memcheck(addr)
                st["fadetask_watch"] = True
        except Exception as e:
            probe_out("fadetask.csv", hdr).write("# frame %d error %r\n" % (frame, e))
    elif FADETASK and frame > FADETASK[1] and st.get("fadetask_watch"):
        for addr in [0x80466E38] + WATCH_ADDRS:
            try:
                memory.remove_memcheck(addr)
            except Exception:
                pass
        st["fadetask_watch"] = False
    if SETMOVE:
        set_move()
    if PEEK and PEEK[0][0] <= frame <= PEEK[0][1]:
        try:
            probe_out("peek.csv", "frame," + ",".join("%08x" % a for a in PEEK[1])).write(
                "%d,%s\n" % (frame, ",".join("%08x" % u32(a) for a in PEEK[1])))
        except Exception as e:
            probe_out("peek.csv", "").write("# frame %d error %r\n" % (frame, e))
    if "card" in PROBES:
        # the console's memory-card operations (2026-10-19; B's card-timing model for S02's Continue screen). Polled
        # every frame from the SDK's card control blocks (card_dsp_private.h: __CARDBlock[2] at 0x803FC620, 0x110 bytes per slot):
        # +0x04 result is -1 (BUSY) while a CARD call holds the slot (__CARDGetControlBlock sets it, the end of the
        # call or its async completion clears it). One row per busy stretch: start / end frame, the callbacks held
        # (+0xD0 apiCallback = the caller's completion callback, +0xD4 xferCallback = the SDK's own step; resolve the
        # addresses with symbols.txt), the EXI commands seen (+0x94 cmd[0]: 0x52 read, 0xF2 write, 0xF1 erase sector,
        # 0x83 / 0x89 status), the card byte addresses touched (+0xB0) and the final result. Resolution: one frame, so
        # a call that starts and ends inside one frame shows only if a frame boundary falls inside it.
        try:
            room = u32(ROOM)
            if room != st.get("card_room"):
                st["card_room"], st["card_room_at"] = room, frame
            for chan in (0, 1):
                cb = 0x803FC620 + 0x110 * chan
                busy = u32(cb + 0x04) == 0xFFFFFFFF
                op = st.get(("card", chan))
                if busy:
                    if op is None:
                        op = st[("card", chan)] = {"start": frame, "room": room,
                                                   "room_frame": frame - st["card_room_at"], "api": set(), "xfer": set(),
                                                   "cmds": set(), "lo": None, "hi": None}
                    op["api"].add(u32(cb + 0xD0))
                    op["xfer"].add(u32(cb + 0xD4))
                    op["cmds"].add(memory.read_u8(cb + 0x94))
                    a = u32(cb + 0xB0)
                    op["lo"] = a if op["lo"] is None else min(op["lo"], a)
                    op["hi"] = a if op["hi"] is None else max(op["hi"], a)
                elif op is not None:
                    hexes = lambda s: " ".join("%x" % v for v in sorted(s))
                    probe_out("card.csv", "chan,start_frame,end_frame,frames,api_callbacks,xfer_callbacks,exi_cmds,"
                                          "addr_lo,addr_hi,result,attached,room,room_frame").write(
                        "%d,%d,%d,%d,%s,%s,%s,0x%x,0x%x,%d,%d,%d,%d\n" % (
                            chan, op["start"], frame, frame - op["start"], hexes(op["api"]), hexes(op["xfer"]),
                            hexes(op["cmds"]), op["lo"], op["hi"], (u32(cb + 0x04) ^ 0x80000000) - 0x80000000, u32(cb), op["room"],
                            op["room_frame"]))
                    st[("card", chan)] = None
            # the game's own card task (memcard.c, addresses from B, 2026-10-19): a per-frame state machine that starts
            # an async CARD call per state and polls CARDGetResultCode until it stops returning BUSY. Task pointer
            # 0x8047B3D4 (0 = none); +0x00 kind, +0x0C state, +0x10 resume state, +0x28 card result, +0x34 random
            # delay. One row whenever any of these (or slot A's SDK result) changes.
            p = u32(0x8047B3D4)
            s32 = lambda a: (u32(a) ^ 0x80000000) - 0x80000000
            row = (s32(p), s32(p + 0x0C), s32(p + 0x10), s32(p + 0x28), s32(p + 0x34)) if ram(p) else (None,) * 5
            row = (p,) + row + (s32(0x803FC624),)
            if row != st.get("memcard_row"):
                st["memcard_row"] = row
                f = lambda v: "" if v is None else str(v)
                probe_out("memcard.csv", "frame,task,task_kind,state,resume_state,card_result,random_delay,sdk_result,"
                                         "room,room_frame").write("%d,0x%x,%s,%s,%s,%s,%s,%d,%d,%d\n" % (
                    frame, p, f(row[1]), f(row[2]), f(row[3]), f(row[4]), f(row[5]), row[6], room,
                    frame - st["card_room_at"]))
        except Exception as e:
            probe_out("card.csv", "chan,start_frame,end_frame,frames,api_callbacks,xfer_callbacks,exi_cmds,addr_lo,"
                                  "addr_hi,result,attached,room,room_frame").write("# frame %d error %r\n" % (frame, e))
    if "gfxdump" in PROBES:
        # layout check for the frames probe: the graphics state's first 0x70 bytes every 60 frames
        try:
            if frame % 60 == 0:
                g = u32(0x8047AA80)
                words = " ".join("%08x" % u32(g + 4 * k) for k in range(0x1C)) if ram(g) else "-"
                probe_out("gfxdump.csv", "frame,room,ptr,words").write("%d,%d,%08x,%s\n" % (frame, u32(ROOM), g,
                                                                                          words))
        except Exception as e:
            probe_out("gfxdump.csv", "frame,room,ptr,words").write("%d,error,%r\n" % (frame, e))
    if "menu" in PROBES:
        # the open menu windows (2026-10-18, for the PC's stuck PC-menu cursor): the window list starts at the
        # menu system winSysWork (0x80404ACC) +0x0C; each window (menu_r50_80102014_prefix.c tagWINDOW_WORK): +0x01 phase,
        # +0x04 id, +0x10 next, +0x19 cursorCount, +0x1A cursorMode, +0x1B cursorFlags, +0x94/+0x95 cursor base /
        # offset; plus the menu's table entry (_menu_data + id * 0x1C), whose callback slots the game fills at run
        # time. A row only when something changed.
        try:
            rows, node, guard = [], u32(0x80404ACC + 0x0C), 0
            while ram(node) and guard < 64:
                wid = u32(node + 4)
                ent = 0x802E2DB8 + wid * 0x1C
                cbs = [u32(ent + 0x0C + 4 * k) for k in range(4)]
                # last column: the window's byte +0x09 (its draw priority per 2026-11-05, for the box
                # party-column brackets the PC doesn't show)
                rows.append("%d,%d,%d,%d,%d,%d,%d,%s,%s,%d" % (
                    wid, s8(u8(node + 1)), s8(u8(node + 0x19)), u8(node + 0x1A), u8(node + 0x1B),
                    s8(u8(node + 0x94)), s8(u8(node + 0x95)),
                    "".join("%02x" % u8(ent + k) for k in range(0x0C)), " ".join("%08x" % c for c in cbs),
                    u8(node + 0x09)))
                node = u32(node + 0x10)
                guard += 1
            if LIVEPAD:
                # menu events for the live pad (2026-11-02, P01: a stick move timed from the room entry came
                # before Dolphin's menu was ready and the cursor ended elsewhere): a window's phase, and its cursor
                # (base + offset, when it has more than one choice), each when it changes; the same as
                # livepad_steps.py reads from the PC's [menu] lines
                now = {}
                for r in rows:
                    f = r.split(",")
                    wid, ph, cnt, base, off = int(f[0]), int(f[1]), int(f[2]), int(f[5]), int(f[6])
                    now[(wid, "p")] = ph
                    if cnt > 1:
                        now[(wid, "c")] = base + off
                was = st.get("lp_menu", {})
                for (wid, what), v in sorted(now.items()):
                    if was.get((wid, what)) != v:
                        livepad_event("menu", {"key": "%d:%s%d" % (wid, what, v)})
                st["lp_menu"] = now
            key = "|".join(rows)
            if key != st.get("menu"):
                st["menu"] = key
                out = probe_out("menu.csv", "frame,room,id,phase,cursorCount,cursorMode,cursorFlags,base,offset,"
                                            "table_head,callbacks,prio")
                for r in rows:
                    out.write("%d,%d,%s\n" % (frame, u32(ROOM), r))
        except Exception:
            pass


def pad_dict(raw):
    """set_gc_buttons dict for 10 latched PADStatus bytes."""
    b = (raw[0] << 8) | raw[1]
    d = {k: True for k, m in BTN.items() if b & m}
    x, y = (s8(raw[2]) + 128) & 255, (s8(raw[3]) + 128) & 255
    if x != 128:
        d["StickX"] = STICK_X_SEND[x]
    if y != 128:
        d["StickY"] = STICK_Y_SEND[y]
    cx, cy = (s8(raw[4]) + 128) & 255, (s8(raw[5]) + 128) & 255
    # the C-stick goes through the same scramble as the main stick (all 256 values per axis measured with
    # dolphin_stickcal.py STICKCAL_CSTICK=1, 2026-10-18; an axis sent as 128 arrives as full tilt, so centred is omitted)
    if cx != 128:
        d["CStickX"] = STICK_X_SEND[cx]
    if cy != 128:
        d["CStickY"] = STICK_Y_SEND[cy]
    if raw[6]:
        d["TriggerLeft"] = raw[6]
    if raw[7]:
        d["TriggerRight"] = raw[7]
    if raw[8]:
        d["AnalogA"] = raw[8]
    if raw[9]:
        d["AnalogB"] = raw[9]
    return d


def load_replay():
    seq = {}
    for line in open(PAD_IN):
        r = json.loads(line)
        seq[r["f"]] = bytes.fromhex(r["p"])
    st["replay"] = seq
    st["replay_frames"] = sorted(seq)
    st["replay_last"] = max(seq) if seq else 0
    st["replay_end"] = json.load(open(os.path.join(os.path.dirname(PAD_IN), "sitting.json"))).get("last_frame") \
        if os.path.exists(os.path.join(os.path.dirname(PAD_IN), "sitting.json")) else None


def recorded_at(f):
    """The recorded pad in force at frame f (the last change at or before f)."""
    import bisect
    fr = st["replay_frames"]
    i = bisect.bisect_right(fr, f) - 1
    return st["replay"][fr[i]] if i >= 0 else bytes(10)


def finish(reason):
    w("end", reason=reason, frames=frame)
    summary = {"last_frame": frame, "reason": reason, "mode": MODE, "marks": st["marks"], "entries": st["entries"],
               "seconds": round(time.time() - t0, 1)}
    if st.get("ij_stats"):
        # *Added 2026-11-04 (after P06's particle fault):* whether this run is usable for effect pictures at a
        # glance: draws the injector watched that weren't recorded kinds ("other"), how many of them loaded the same
        # state as the draw before (a long run of those is the old fault), the longest such run, and the free misses
        s = st["ij_stats"]
        summary["inject"] = dict(s)
        line = ("particle draws: %d (free %d), repeats of one state: %d (longest run %d), free misses: %d; "
                "other draws: %d, repeats %d (longest run %d)" % (
                    s["other"], s["free"], s["repeats"], s["longest"], s["misses"], s["rest"], s["rest_repeats"],
                    s["rest_longest"]))
        summary["inject_line"] = line
        try:
            probe_out("inject.csv", IJ_HDR).write("# summary: %s\n" % line)
        except Exception:
            pass
        print("[inject] " + line)
    json.dump(summary, open(os.path.join(OUT, "sitting.json" if MODE == "record" else "replay.json"), "w"))
    ev_log.flush()
    if pad_log:
        pad_log.flush()
    if chk_log:
        chk_log.write(json.dumps({"summary": True, "mismatched_frames": st["mismatch"], "frames": frame}) + "\n")
        chk_log.flush()
    time.sleep(2)
    os._exit(0)


def on_frame():
    global frame
    frame += 1
    if frame % 300 == 1:
        # the heartbeat for playrec.py's no-progress watchdog (2026-11-05, after two runs ran 50 and 12 minutes
        # with a dead script and nothing ended them): first thing in the frame hook, so a script that never loaded, a
        # frame hook that dies, or an emulator that stops advancing all stop it
        try:
            with open(os.path.join(OUT, "heartbeat.txt"), "w") as hb:
                hb.write("%d %.0f\n" % (frame, time.time()))
        except Exception:
            pass
    if not st["started"]:
        st["started"] = True
        if START_STATE:
            try:
                savestate.load_from_file(START_STATE)
                w("loaded", state=os.path.basename(START_STATE))
            except Exception as e:
                w("load_error", err=repr(e))
        else:
            w("power_on")
        if MODE == "replay" or DRIVE:
            load_replay()
            w("replay_start", changes=len(st["replay"]), last=st["replay_last"])
            if DRIVE:
                st["end_at"] = st["replay_last"] + 300
        else:
            osd("Recording. L+R+Z = mark, L+R+START = end the sitting")
        return
    if MODE == "record":
        movie_check()
    if MODE == "replay" or DRIVE:
        nxt = recorded_at(frame + SHIFT)
        try:
            controller.set_gc_buttons(0, pad_dict(nxt))
        except Exception as e:
            if not st.get("set_err"):
                st["set_err"] = True
                w("set_error", err=repr(e))
    elif LIVEPAD:
        try:
            controller.set_gc_buttons(0, pad_dict(livepad_now()))
        except Exception as e:
            if not st.get("set_err"):
                st["set_err"] = True
                w("set_error", err=repr(e))
    try:
        raw = read_pad_bytes()
    except Exception:
        raw = None
    if raw is not None:
        if MODE == "record":
            if raw != st["pad"]:
                pad_log.write(json.dumps({"f": frame, "p": raw.hex()}) + "\n")
                b = (raw[0] << 8) | raw[1]
                for combo, kind in ((MARK, "mark"), (END, "end")):
                    held = (b & combo) == combo
                    was = st["pad"] is not None and ((st["pad"][0] << 8 | st["pad"][1]) & combo) == combo
                    if held and not was:
                        if kind == "mark":
                            st["marks"] += 1
                            w("mark", n=st["marks"], room=u32(ROOM), pos=player_pos())
                            save_state("mark_%03d" % st["marks"], "mark")
                            osd("Mark %d saved" % st["marks"])
                        elif st["end_at"] is None:
                            save_state("end", "end of sitting")
                            osd("Sitting ended: saving, Dolphin closes in 3 s")
                            st["end_at"] = frame + 180
                st["pad"] = raw
        elif MODE == "replay":
            want = recorded_at(frame)
            if raw != want:
                st["mismatch"] += 1
                if st["mismatch"] <= 200:
                    chk_log.write(json.dumps({"f": frame, "want": want.hex(), "got": raw.hex()}) + "\n")
    try:
        watch_game()
    except Exception as e:
        if not st.get("watch_err"):
            st["watch_err"] = True
            w("watch_error", err=repr(e))
    if PROBES or SETMOVE or ROLLINJECT or RNGTEST:
        run_probes()
    if frame % 30 == 0:
        watch_flags()
    if frame % 60 == 0:
        p = u32(RNG_PTR)
        w("tick", room=u32(ROOM), pos=player_pos(), rng=u32(p) if ram(p) else None)
    if MODE == "record" and frame - st["last_state"] >= PERIODIC:
        save_state("t%07d" % frame, "periodic")
    if st["end_at"] is not None and frame >= st["end_at"]:
        finish("end combo")
    if MODE == "watch" and WATCH_FRAMES and frame >= WATCH_FRAMES:
        finish("watch complete")
    if MODE == "replay" and st.get("replay") is not None and frame > 1 \
            and frame >= (st.get("replay_end") or st["replay_last"] + 600):
        finish("replay complete")


idlog.install(event, on_frame, OUT, run="playrec_" + MODE)

# PLAYREC_SHOTS=<frame>[,<frame>...] or <from>-<to>/<step> (2026-10-21): screenshots at those frames (the frame
# counter the probes use) into shots/f<frame>.png, to compare with the PC's ORRE_DUMP_FRAMES (framediff.py); snatcherLeo:
# the zone transitions (Wes's bike ride on the world map) look off on the PC
SHOTS = set()
for part in filter(None, os.environ.get("PLAYREC_SHOTS", "").split(",")):
    if "-" in part:
        span, _, step = part.partition("/")
        lo, hi = (int(x) for x in span.split("-"))
        SHOTS.update(range(lo, hi + 1, int(step or 1)))
    else:
        SHOTS.add(int(part))
# PLAYREC_SHOTS_PC (2026-11-03; the same syntax, in PC frames, with the live pad): a shot whenever the PC frame
# that the current frame stands for (the last matched PC event's frame plus the frames since Dolphin saw it) is in the
# set, into shots/f<frame>_pc<PC frame>.png. the project asked for the console's frames of P03's Pursuit, placed by its
# battle message; the Dolphin frame isn't known before the run
SHOTS_PC = set()
for part in filter(None, os.environ.get("PLAYREC_SHOTS_PC", "").split(",")):
    if "-" in part:
        span, _, step = part.partition("/")
        lo, hi = (int(x) for x in span.split("-"))
        SHOTS_PC.update(range(lo, hi + 1, int(step or 1)))
    else:
        SHOTS_PC.add(int(part))
# the ptcl probe's windows ([(lo, hi), ...], in PC frames?): PLAYREC_PTCL_PC=<from>-<to>[,<from>-<to>...] or
# PLAYREC_PTCL=<from>-<to>[,...] (*Changed 2026-11-04, P06:* several windows, for the snag and Poison Sting in one run)
_pw = os.environ.get("PLAYREC_PTCL_PC") or os.environ.get("PLAYREC_PTCL")
PTCL_WIN = ([tuple(int(x) for x in w.split("-", 1)) for w in _pw.split(",") if w],
            bool(os.environ.get("PLAYREC_PTCL_PC"))) if _pw else None
# the 0x88 leftover probe's window, PLAYREC_PTCLZ_PC=<from>-<to> (PC frames; needs the live pad and ROLLINJECT, whose
# memory-breakpoint handler passes these watches on to ptclz_hit)
_pz = os.environ.get("PLAYREC_PTCLZ_PC") or os.environ.get("PLAYREC_PTCLZ")
PTCLZ_WIN = tuple(int(x) for x in _pz.split("-", 1)) if _pz else None
# *Added 2026-11-04 (the clean re-measure):* PLAYREC_PTCLZ=<from>-<to> is the same window in this file's frames,
# for a plain movie replay (no live pad, no injection): only the stack-slot watches run then, not the particle-z ones
PTCLZ_BY_FRAME = bool(os.environ.get("PLAYREC_PTCLZ")) and not os.environ.get("PLAYREC_PTCLZ_PC")
PTCLZ_ADDRS = set()
# PLAYREC_PTCLZ_STACK=<sp>[,<sp>...] (hex): watch sp + 0x3C / 0x40 / 0x44 (the 0x88 local Vec) inside the same window,
# loads and stores, with r1 / r26 / r31 (ptcls_hit). Run 12's particle-z watch caught no 0x88 add (the add likely runs
# when a particle is first interpreted, in a slot not yet watched); these two stack frames wrote the particle z there.
# PLAYREC_PTCLZ_SLOTS=<hex>,<hex>... (added 2026-11-04): other offsets, e.g. all four commands' Vecs (from the
# original assembly: 0x80 at sp+0x48 / 4C / 50, 0x88 at 3C / 40 / 44, 0x90 at 30 / 34 / 38, 0x98 at 24 / 28 / 2C;
# psInterpretParticle0's frame was sp 0x8048E1D0 in P03 replay_13, r26 = the command byte)
PTCLS_SLOTS = [int(s, 16) for s in os.environ.get("PLAYREC_PTCLZ_SLOTS", "3C,40,44").split(",") if s]
PTCLS_ADDRS = {int(s, 16) + o: o for s in filter(None, os.environ.get("PLAYREC_PTCLZ_STACK", "").split(","))
               for o in PTCLS_SLOTS}
PTCLS_ON = []


def ptcls_hit(is_write, addr, value):
    import struct
    from dolphin import registers
    sp, r26, r31 = registers.read_gpr(1), registers.read_gpr(26), registers.read_gpr(31)
    cur = memory.read_f32(addr)
    v = struct.unpack(">f", struct.pack(">I", value & 0xFFFFFFFF))[0] if isinstance(value, int) else float(value)
    pinfo = (u32(r31 + 4), u8(r31 + 8)) if ram(r31) else (0, 255)
    pc = LP["anchors"][LP["k"]][2] + (frame - LP["at"]) if LP.get("k", -1) >= 0 else -1
    probe_out("ptcls.csv", "frame,pc,addr,slot,rw,value,mem_before,r1,r26,r31,r31_kind,r31_bank").write(
        "%d,%d,%08x,%x,%s,%.4f,%.4f,%08x,%08x,%08x,%08x,%d\n" % (
            frame, pc, addr, PTCLS_ADDRS[addr], "w" if is_write is True else "r", v, cur, sp, r26, r31, pinfo[0],
            pinfo[1]))


def ptclz_hit(is_write, addr, value):
    """a watched particle z: on a store, log the old z, the stored value, psInterpretParticle0's local Vec at
    r1 + 0x3C / 0x40 / 0x44, r26 / r31, and whether old + Vec.z == the value (the 0x88 add) (port-gaps 56)"""
    if is_write is not True:
        return
    import struct
    from dolphin import registers
    pp = addr - 0x48
    sp = registers.read_gpr(1)
    vec = [memory.read_f32(sp + o) if ram(sp) else float("nan") for o in (0x3C, 0x40, 0x44)]
    old = memory.read_f32(addr)
    new = struct.unpack(">f", struct.pack(">I", value & 0xFFFFFFFF))[0] if isinstance(value, int) else float(value)
    is_add = abs(old + vec[2] - new) < 1e-3 * max(1.0, abs(new))
    pc = LP["anchors"][LP["k"]][2] + (frame - LP["at"]) if LP.get("k", -1) >= 0 else -1
    probe_out("ptclz.csv", "frame,pc,pp,kind,bank,texGroup,old_z,new_z,vec_x,vec_y,vec_z,is_add,r1,r26,r31,gen").write(
        "%d,%d,%08x,%08x,%d,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%08x,%08x,%08x,%08x\n" % (
            frame, pc, pp, u32(pp + 4), u8(pp + 8), u8(pp + 9), old, new, vec[0], vec[1], vec[2], is_add, sp,
            registers.read_gpr(26), registers.read_gpr(31), u32(pp + 0x8C)))
if "rngcall" in PROBES and RNGCALL and not FADETASK and "passes" not in PROBES:
    # the rngcall probe (2026-10-26; the project's P12 battle 207 question: which code takes each random draw on the
    # console). A write watch on the RNG state word, armed only in PLAYREC_RNGCALL's label window. HSD_Rand / HSD_Randf
    # are leaf functions, so at the write r1 is their caller's frame: [r1] is the back chain and [[r1] + 4] the
    # caller's saved return address (into the caller's caller); two more levels follow the chain. The PC's trace logs
    # HSD_Rand's own return address, one level below the first column here. rngcall.csv: label, new state, r1,
    # three return addresses (map them with symbols.txt)
    from dolphin import registers

    def on_rng_write(*_args):
        try:
            a = st.get("rc_addr")
            if not st.get("rc_armed") or a is None:
                return
            v, th = u32(a), u32(0x8047AC00)
            if th != st.get("rc_thread"):
                # a GS thread switch (the second watch): logged as state "switch" with the new thread
                st["rc_thread"] = th
                v = None
            elif v == st.get("rc_last"):
                return  # the fork reports each write twice
            else:
                st["rc_last"] = v
            r1 = registers.read_gpr(1)
            chain, p = [], r1
            for _ in range(3):
                p = u32(p) if ram(p) else 0
                chain.append(u32(p + 4) if ram(p) else 0)
            # the gapp task being dispatched (lbl_8047AC94, GSgappTask +0x0C priority; 0x100 = none: a draw outside
            # the task loop, e.g. in a GS thread resumed between tasks)
            # as "index:priority" like the PC's [rngtrace] (pc_gapp_current_task: index = (task - the task array at
            # _gappStructList) / 0x18 + 1, as the callback receives it; 0:0 outside a dispatch), and the running GS thread
            # (_activeThread, GSthreadGetCurrentThread)
            task = u32(0x8047AC94)
            tid = "%d:%x" % ((task - u32(0x8047AC7C)) // 0x18 + 1, u8(task + 0x0C)) if ram(task) else "0:0"
            probe_out("rngcall.csv", "frame,state,r1,ret1,ret2,ret3,task,thread").write(
                "%d,%s,%08x,%s,%s,%08x\n" % (st.get("rc_frame", -1), "switch" if v is None else "%08x" % v, r1,
                                             ",".join("%08x" % c for c in chain), tid, th))
        except Exception as e:
            probe_out("rngcall.csv", "frame,state,r1,ret1,ret2,ret3,task,thread").write("# error %r\n" % (e,))
    event.on_memorybreakpoint(on_rng_write)

RNGTEST = int(os.environ.get("PLAYREC_RNGTEST", "0") or 0)
RNGTEST_MODE = os.environ.get("PLAYREC_RNGTEST_MODE", "")   # "store": test 3, r0 written at a store callback
if RNGTEST:
    # PLAYREC_RNGTEST=<frame> (2026-11-02, the project's battle-roll injection question): can a watch callback set the RNG
    # state before a draw uses it? From that frame a watch on the state word logs its first 16 callbacks (the fork's
    # arguments, the word now); at the first callback it writes K. HSD_Rand computes state * 214013 + 2531011, so if the
    # next state written is K's successor, a write in the callback lands before the draw reads (injection works). A
    # test only: the replay parts after it.
    RNG_K = 0x12345678

    def on_rngtest(*args):
        try:
            n = st.get("rt_n", 0)
            if not st.get("rt_armed") or n >= 16:
                return
            st["rt_n"] = n + 1
            a = st["rt_addr"]
            v = u32(a)
            wrote = ""
            # *Updated 2026-11-02 (first run, P04 replay_5):* a read callback comes after the load and a write callback
            # before the store lands, so K is written at the first read that follows a write (a draw's own return
            # load): the next draw should then compute from K
            if RNGTEST_MODE == "store":
                # test 3 (2026-11-02): at a store callback, before the store lands, set r0 (HSD_Rand / HSD_Randf store
                # r0 and load it back into r0 for the return value) to K. If the store and the return load then
                # show K, a draw can be given any state while it runs: no need to guess the next draw's list
                if args and args[0] is True and not st.get("rt_wrote"):
                    from dolphin import registers
                    before = registers.read_gpr(0)
                    registers.write_gpr(0, RNG_K)
                    st["rt_wrote"] = True
                    wrote = "r0 %08x -> K, now %08x" % (before, registers.read_gpr(0))
            elif args and args[0] is True:
                st["rt_after_write"] = True
            elif st.get("rt_after_write") and not st.get("rt_wrote"):
                memory.write_u32(a, RNG_K)
                st["rt_wrote"] = True
                wrote = "wrote K"
            probe_out("rngtest.csv", "n,frame,args,word,note,successor_of_K").write("%d,%d,%s,%08x,%s,%08x\n" % (
                n, frame, repr(args).replace(",", ";"), v, wrote, (RNG_K * 214013 + 2531011) & 0xFFFFFFFF))
        except Exception as e:
            probe_out("rngtest.csv", "n,frame,args,word,note,successor_of_K").write("# error %r\n" % (e,))
    event.on_memorybreakpoint(on_rngtest)

ROLLINJECT = os.environ.get("PLAYREC_ROLLINJECT", "").removeprefix("@file:")
INJ = {}   # (list, battle N) -> its states in order: ("battle", N) and ("create", N)
IJ_HDR = "frame,battle,kind,due_list,used_index,loaded,stored,next_injected,thread,ret1,ret2,ret3"
if ROLLINJECT:
    # PLAYREC_ROLLINJECT=<the PC's ORRE_ROLL_RECORD file> (2026-11-02, the project's design): the PC's own rolls given to
    # the console, so a the reference playthrough sitting's battles can be compared without frame-for-frame RNG parity. The record has one
    # line per PC draw, "<list> <battle N> <index> <state before the draw> <frame>"; two lists are used here: "battle"
    # (the battle thread's draws, not the camera shot code, not animation particles) and "create" (draws inside
    # pokemonCreate / pokemonCreateRndFit: the opponents' teams; N = the battle they belong to, 0 outside one).
    # How (PLAYREC_RNGTEST, P04, 2026-11-02, verified): the fork's watch callback gets (is_write, address, value),
    # comes after a load and before a store lands, and HSD_Rand logs load, store, return load. A state written at a
    # draw's return load is what the next draw loads (test 2); a draw can't be changed while it runs (test 1: a write
    # at its load is too late; test 3: the register view at a store callback isn't the game's, r0 read 0 and writing
    # it changed nothing; code breakpoints fire on compiles, not every hit). So after every draw the state is set to
    # the next recorded draw's state.
    # *Changed 2026-11-02 (P25 live4, replay_9):* one sequence per battle, all four recorded kinds (create, battle,
    # camera, idle) in the record's own order, which is the PC's draw order (frames never go back; 1,285 of 1,359
    # neighbours chain state-after to state-before). A recorded-kind draw moves the sequence on; any other draw
    # (blinks, particles, other threads) used the entry and gets it again. Before, with only the create and battle
    # lists, a camera draw got the same state again after every try, and a camera routine that redraws until it gets
    # a usable number never finished (frame 6,514, the run killed at its time limit). inject.csv logs every draw.
    from dolphin import registers
    RECORDED = ("create", "battle", "camera", "idle")
    FIELD = []   # the field list (2026-11-03, 20d261f0d): every draw inside peopleMoveWander, sitting-wide
    FIELD_PC = []   # each field draw's PC frame (the record's 5th column)
    for line in open(ROLLINJECT, encoding="utf-8"):
        p = line.split()
        if len(p) >= 4 and not line.startswith("#") and p[0] in RECORDED and int(p[1]) > 0:
            INJ.setdefault(int(p[1]), []).append((p[0], int(p[3], 16)))
        elif len(p) >= 5 and not line.startswith("#") and p[0] == "field":
            FIELD.append(int(p[3], 16))
            FIELD_PC.append(int(p[4]))

    def _wander(v):   # peopleMoveWander (its GSrandf direction) and fn_800E0BA0 (the pause after arriving)
        # (+ 0x8018199C, the return address of the call into peopleMoveWander: the pause draws' saved return
        # addresses skip a level and start there, so they matched neither range; P02, 2026-11-03: Dolphin's field
        # list fell one entry behind at its first pause pair, frame 2,859)
        return 0x80184D80 <= v < 0x8018524C or 0x800E0BA0 <= v < 0x800E0BE4 or v == 0x8018199C

    def field_due():
        """a field draw is near: the live pad's PC position (its last matched event plus the frames since) is within
        300 frames before the next field entry's PC frame, or past it. Outside that, Dolphin's own numbers stay (P01:
        injecting from the first room on changed the opening's draws, stalled at the PC's 2,377 and ran 6 fps)"""
        fi = st.get("ij_fi", 0)
        if fi >= len(FIELD):
            return False
        if not LIVEPAD or LP.get("k", -1) < 0:
            return True
        pcnow = LP["anchors"][LP["k"]][2] + (frame - LP["at"])
        # an entry more than 900 frames overdue is given up (Dolphin never made that draw, or made it unwatched):
        # the list moves on to the first entry still due, so a missed one can't hold the rest
        while fi < len(FIELD) and pcnow > FIELD_PC[fi] + 900:
            fi += 1
        if fi != st.get("ij_fi", 0):
            probe_out("inject.csv", IJ_HDR).write("# frame %d: field entries %d-%d given up (overdue)\n" % (
                frame, st.get("ij_fi", 0), fi - 1))
            st["ij_fi"] = fi
        return fi < len(FIELD) and FIELD_PC[fi] - 300 <= pcnow

    def in_battle():
        """a battle's recorded sequence is still being used"""
        return st.get("ij_battle") and st.get("ij_i", 0) < len(INJ.get(st["ij_battle"], [])) \
            and memory.read_u8(FIGHT_FLAG)

    def inject_start(value):
        """battle N begins (_fightInitialize stored the fight flag): its sequence starts at 0, the RNG watch goes on"""
        st["ij_battle"] = st.get("ij_battle", 0) + 1
        st["ij_i"] = 0
        st["ij_phase"] = None
        st["ij_addr"] = u32(RNG_PTR)
        inject_next()
        if not st.get("ij_armed"):
            memory.add_memcheck(st["ij_addr"])
            st["ij_armed"] = True
        seq = INJ.get(st["ij_battle"], [])
        probe_out("inject.csv", IJ_HDR).write("# frame %d: battle %d starts (flag store %x), %d recorded draws: %s\n" % (
            frame, st["ij_battle"], value, len(seq),
            ", ".join("%s %d" % (k, sum(1 for x in seq if x[0] == k)) for k in RECORDED)))

    def _create(v):   # as the PC's create wrappers (2026-11-02): pokemonCreate / pokemonCreateRndFit, and
        # everything inside fightTrainerCreateFightTrainerDataIdToHero (the trainer's heroCreate draws, then the team).
        # Seen through the draw's three saved return addresses: a draw nested deeper inside the trainer creation
        # would be missed (inject.csv would show it as "other" in the burst)
        # (+ fightTrainerSortFightTrainerDataIdToHeroTemotiPokemon, 0x801F99C8-0x801F9CBC: the team's order, 6 draws right
        # after the creation in P25's replay_8; asked of Main the same day)
        return 0x801240C4 <= v < 0x801248C4 or 0x801F99C8 <= v < 0x801F9F78

    def due():
        """the next recorded draw's kind: the battle's sequence inside a battle, else the field list ("-": none)"""
        if in_battle():
            return INJ[st["ij_battle"]][st["ij_i"]][0]
        return "field" if st.get("ij_fi", 0) < len(FIELD) else "-"

    def inject_next():
        """set the state to the next recorded draw's; returns it (or None when nothing is due)"""
        if in_battle():
            s = INJ[st["ij_battle"]][st["ij_i"]][1]
        elif field_due():
            s = FIELD[st.get("ij_fi", 0)]
        else:
            return None
        memory.write_u32(st["ij_addr"], s)
        return s

    def _camera(v):   # roll_resync_spec.py's camera ranges (the shot code)
        return 0x801D2B4C <= v < 0x801D2C6C or 0x801D30BC <= v < 0x801D7230

    def _idle(v):     # roll_resync_spec.py's idle camera: battleCameraStartRandom and its return into fn_801EF488
        return 0x801C2D80 <= v < 0x801C2F00 or 0x801D2C74 <= v < 0x801D2D28 or v == 0x801EF49C

    def _anim(v):     # gsParticlePtclHook: animation particles, in no list
        return 0x80119BD0 <= v < 0x80119D90

    # the particle code's draw sites (return addresses of the "other" draws in P06 replay_3's inject.csv, each seen
    # thousands of times, none ever followed by a recorded draw in the same frame); see the FREEPTCL note below
    FREE_PTCL = {0x80118818, 0x80169D98, 0x801740F8, 0x8016FE98, 0x8016F474, 0x80170F30, 0x801703A8, 0x80170760,
                 0x801703C0}
    FREEPTCL = os.environ.get("PLAYREC_INJECT_FREEPTCL", "1") == "1"

    def on_inject(*args):
        try:
            if not args:
                return
            if args[1] in PTCLZ_ADDRS:            # the 0x88 leftover probe's watches (PLAYREC_PTCLZ_PC)
                ptclz_hit(*args[:3])
                return
            if PTCLS_ON and args[1] in PTCLS_ADDRS:   # and its stack-slot watches (PLAYREC_PTCLZ_STACK)
                ptcls_hit(*args[:3])
                return
            if args[1] != FIGHT_FLAG and args[1] != (FIGHT_FLAG & ~3) and not st.get("ij_armed"):
                return
            if args[1] in (FIGHT_FLAG, FIGHT_FLAG & ~3):
                # the fight flag stored (the fork may report the byte or its word): 1 = _fightInitialize begins, the
                # next battle (back-to-back battles store it again)
                # not before the first room entry: the boot's memory clearing stores to that word too (the reference playthrough P01's
                # pass 1: a false battle 1 at frame 447, "flag store 80005b00", shifted every battle's number)
                if args[0] is True and args[2] and st.get("entries", 0) > 0:
                    inject_start(args[2])
                return
            if args[1] != st.get("ij_addr"):
                return
            phase = st.get("ij_phase")
            if args[0] is not True and phase is None:
                # a draw's load: classify it (HSD_Rand is a leaf, so r1 is its caller's frame, as in rngcall)
                chain, p = [], registers.read_gpr(1)
                for _ in range(3):
                    p = u32(p) if ram(p) else 0
                    chain.append(u32(p + 4) if ram(p) else 0)
                # the link register, when the fork can read it: the return address into the roll's own caller (the
                # saved ones start a level higher; P02's movers all showed 0x8018199C, peopleMain)
                if not st.get("lp_regs_logged"):
                    st["lp_regs_logged"] = True
                    probe_out("inject.csv", IJ_HDR).write("# registers: %s\n" % " ".join(
                        n for n in dir(registers) if not n.startswith("_")))
                lr = 0
                for fn_name in ("read_lr", "read_LR"):
                    if hasattr(registers, fn_name):
                        try:
                            lr = getattr(registers, fn_name)()
                        except Exception:
                            pass
                        break
                if lr:
                    chain = [lr] + chain
                # the battle thread: the running GS thread is the fight thread's handle (pc_debug_game.c's own test)
                th = u32(0x8047AC00)
                onb = th and th == u32(FIGHT_THREAD)
                kind = ("create" if any(_create(c) for c in chain) else "field" if any(_wander(c) for c in chain)
                        else "camera" if any(_camera(c) for c in chain)
                        else "anim" if any(_anim(c) for c in chain)
                        else "idle" if not onb and any(_idle(c) for c in chain)
                        else "battle" if onb else "other")
                st["ij_kind"], st["ij_loaded"], st["ij_chain"], st["ij_th"] = kind, args[2], chain, th
                st["ij_phase"] = "loaded"
            elif args[0] is True and phase == "loaded":
                st["ij_stored"], st["ij_phase"] = args[2], "stored"
            elif args[0] is not True and phase == "stored":
                # the return load: the draw is done. A draw of the due list's kind used its entry: move on. Then set
                # the state for the next draw
                # (due_list = the kind the record expected here; a recorded-kind draw of another kind is logged with it,
                # so a sequence that drifts shows at the first such row)
                kind, n, was = st["ij_kind"], st.get("ij_battle", 0), due()
                if kind == "field":
                    # a wander draw takes the next field entry wherever it comes (the list is sitting-wide)
                    used = st.get("ij_fi", 0) if st.get("ij_fi", 0) < len(FIELD) else -1
                    st["ij_fi"] = st.get("ij_fi", 0) + 1
                else:
                    used = st["ij_i"] if kind in RECORDED and was not in ("-", "field") else -1
                    if used >= 0:
                        st["ij_i"] += 1
                # *Added 2026-11-04 (P06, snatcherLeo: "Dolphin test I was viewing had some odd... particles for various
                # moves"; verified):* a particle draw left the state at the next recorded entry, so every particle
                # after it loaded the same number (replay_3's snag burst: 1,522 draws, all state 818b02af, the
                # spray's streaks stacked into one comet line; 933k of the run's 935k battle draws were such repeats).
                # Particle draws (FREE_PTCL callers) now keep the game's own sequence; the recorded state goes back at
                # the next frame (run_probes) or before the next non-particle draw's load can't be caught, so a
                # recorded draw right after a particle draw in the same frame would load the wrong state: none in
                # replay_3's log (the 193 recorded draws after an "other" draw in the same frame all followed
                # 801df2b0 / 801db0d0, not particle code). A miss is logged ("free miss"). PLAYREC_INJECT_FREEPTCL=0:
                # the old behaviour
                ss = st.setdefault("ij_stats", {"other": 0, "free": 0, "repeats": 0, "longest": 0, "misses": 0,
                                                "rest": 0, "rest_repeats": 0, "rest_longest": 0})
                isptcl = kind == "other" and any(c in FREE_PTCL for c in st["ij_chain"][:2])
                rep = st["ij_loaded"] == st.get("ij_prev_loaded")
                # particle draws (the effect pictures) and the remaining "other" draws (801df2b0 / 801db0d0 and the
                # like, which still get the recorded state by design) are counted apart: a particle draw loading the
                # state of the draw before never happens on its own (the generator never repeats a state), so its
                # repeat count should be 0
                if isptcl:
                    ss["other"] += 1
                    st["ij_run"] = st.get("ij_run", 1) + 1 if rep and st.get("ij_prev_ptcl") else 1
                    ss["repeats"] += st["ij_run"] > 1
                    ss["longest"] = max(ss["longest"], st["ij_run"])
                elif kind == "other":
                    ss["rest"] += 1
                    st["ij_rrun"] = st.get("ij_rrun", 1) + 1 if rep and st.get("ij_prev_rest") else 1
                    ss["rest_repeats"] += st["ij_rrun"] > 1
                    ss["rest_longest"] = max(ss["rest_longest"], st["ij_rrun"])
                st["ij_prev_loaded"], st["ij_prev_ptcl"] = st["ij_loaded"], isptcl
                st["ij_prev_rest"] = kind == "other" and not isptcl
                if isptcl and FREEPTCL:
                    nxt = None
                    st["ij_dirty"] = True
                    ss["free"] += 1
                else:
                    if st.get("ij_dirty") and kind != "other":
                        probe_out("inject.csv", IJ_HDR).write("# frame %d: free miss, a %s draw loaded %08x after "
                                                              "particle draws\n" % (frame, kind, st["ij_loaded"]))
                        ss["misses"] += 1
                    st["ij_dirty"] = False
                    nxt = inject_next()
                # outside battles only the wander draws are logged (the watch is on all sitting with a field list;
                # every blink and particle would be millions of rows)
                if kind == "field" or in_battle() or was not in ("field", "-") \
                        or LP_DIAG[0] <= frame <= LP_DIAG[1]:   # PLAYREC_INJECT_DIAG=from-to: every draw (diagnostic)
                    probe_out("inject.csv", IJ_HDR).write("%d,%d,%s,%s,%d,%08x,%08x,%s,%08x,%s\n" % (
                        frame, n, kind, was, used, st["ij_loaded"], st["ij_stored"],
                        "%08x" % nxt if nxt is not None else "-", st["ij_th"],
                        ",".join("%08x" % c for c in st["ij_chain"])))
                st["ij_phase"] = None
        except Exception as e:
            probe_out("inject.csv", IJ_HDR).write("# frame %d error %r\n" % (frame, e))
    event.on_memorybreakpoint(on_inject)

# PLAYREC_NATIVEWATCH=<hex native address>@<from>-<to> (2026-11-05, port-gaps 65: what the console's lift
# script gets from floorEventElevatorMain, 0x801C9B6C): inside the window (this file's frames) a write watch on
# lbl_8047AC38, where cms_op37_callStandard stores each native's function pointer just before calling it. When the
# stored pointer is the watched native, the VM context is found among r31..r24 (a candidate whose stack depth +0x28 and
# frame +0x1C are within 0..0x40 and whose ip +0x14 is in RAM), the call's first stack values are logged (native
# index, then the arguments), and the result slot ctx + 0x6C + 4 * (frame - 1) is watched: its next write is the
# native's return value (the r3 the script reads). natives.csv: frame, ctx, the arguments, the result. Clean movie
# replays only (no ROLLINJECT): a watch on that busy word could disturb timing, so it is on only inside the window
# *Changed 2026-11-05 (the lift script's natives after the second elevator call):* <native> may be 0 = every
# native, and an optional third part @<ctx hex> keeps only that VM context; each pending call is tracked by its
# result slot (a native that waits, like a sleep, yields while other scripts call theirs), and the row gives the
# call frame, the return frame (when the result is stored) and the native's function pointer
NW = os.environ.get("PLAYREC_NATIVEWATCH", "")
_nw = NW.split("@") if NW else []
NW_FN = int(_nw[0], 16) if NW else None
NW_WIN = tuple(int(x) for x in _nw[1].split("-")) if NW else None
NW_CTX = int(_nw[2], 16) if len(_nw) > 2 else None
NW_PTR = 0x8047AC38
NW_HDR = "frame,ctx,reg,frame_index,stack_depth,args,result,return_frame,native_ptr"


def nw_arm():
    """run_probes: the watch on lbl_8047AC38 only inside the window"""
    on = NW_WIN[0] <= frame <= NW_WIN[1]
    if on and not st.get("nw_on"):
        memory.add_memcheck(NW_PTR)
        st["nw_on"] = True
    elif not on and st.get("nw_on"):
        memory.remove_memcheck(NW_PTR)
        for slot in list(st.get("nw_pend", {})):
            memory.remove_memcheck(slot)
        st["nw_pend"] = {}
        st["nw_on"] = False


def nw_hit(is_write, addr, value):
    from dolphin import registers
    pend = st.setdefault("nw_pend", {})
    if addr == NW_PTR and is_write is True and (NW_FN == 0 or value == NW_FN):
        for rn in range(31, 23, -1):
            c = registers.read_gpr(rn)
            if not ram(c):
                continue
            depth, fi = u32(c + 0x28), u32(c + 0x1C)
            if 0 < depth <= 0x40 and 1 <= fi < depth and ram(u32(c + 0x14)):
                if NW_CTX is not None and c != NW_CTX:
                    return
                # up to 8 words (2026-11-06: std058's bounds box has 6 arguments after the native index)
                args = [u32(c + 0x6C + 4 * (fi + i)) for i in range(min(8, depth - fi))]
                slot = c + 0x6C + 4 * (fi - 1)
                if slot not in pend:
                    memory.add_memcheck(slot)
                pend[slot] = (frame, c, rn, fi, depth, args, value)
                return
        probe_out("natives.csv", NW_HDR).write("%d,,,,,no VM context in r24-r31,,,%08x\n" % (frame, value))
    elif addr in pend and is_write is True:
        f0, c, rn, fi, depth, args, fn = pend.pop(addr)
        probe_out("natives.csv", NW_HDR).write("%d,%08x,r%d,%d,%d,%s,%d,%d,%08x\n" % (
            f0, c, rn, fi, depth, " ".join("%x" % a for a in args), value if value < 0x80000000 else value - 2 ** 32,
            frame, fn))
        memory.remove_memcheck(addr)


if NW_FN is not None and not ROLLINJECT:
    def on_nw(*args):
        try:
            if args:
                nw_hit(*args[:3])
        except Exception as e:
            probe_out("natives.csv", NW_HDR).write("# frame %d error %r\n" % (frame, e))
    event.on_memorybreakpoint(on_nw)

if PTCLS_ADDRS and not ROLLINJECT:
    # *Added 2026-11-04 (the clean re-measure of the 0x80 / 0x88 / 0x90 / 0x98 leftovers):* without injection
    # nothing else hands the stack-slot watches on, so they get their own handler (a plain movie replay: the console's
    # own random numbers, no live pad)
    def on_ptcls(*args):
        try:
            if args and PTCLS_ON and args[1] in PTCLS_ADDRS:
                ptcls_hit(*args[:3])
        except Exception as e:
            probe_out("ptcls.csv", "").write("# frame %d error %r\n" % (frame, e))
    event.on_memorybreakpoint(on_ptcls)

if "passes" in PROBES and not FADETASK and "dvdx" not in PROBES:
    # the passes probe (2026-10-26; the project's moment (A)): how many GSgappUpdate passes ran in each logger frame. A
    # watch on lbl_8047AC94 (0x8047AC94, the task being dispatched, written before each callback and cleared after the
    # pass): a pass starts when it becomes the list head (*0x8047AC98). passes.csv gets a row only for a frame whose
    # count isn't 1 (frame, passes); run_probes writes the previous frame's row at each frame advance.
    # Result (2026-10-26, verified on P01 replay_37 / P02 replay_13): frames.csv and the pass count agree on the
    # number of game frames (the running difference stays within -1..+2 and returns to 0); they differ only in which
    # refresh a frame is credited to, by one. So frames.csv does not miss frames, and P01 label 1784 is such a one-
    # refresh offset, not a long frame. The watch DISTURBS long replays: RNG left every other replay at P01 label 24825
    # and P02 label 42397, so use it only as a diagnostic, never in a replay whose trace is used later.
    PASS_ADDR, HEAD_ADDR = 0x8047AC94, 0x8047AC98

    def on_pass_write(*_args):
        try:
            v = u32(PASS_ADDR)
            if v != st.get("pass_last"):
                st["pass_last"] = v
                if v and v == u32(HEAD_ADDR):
                    st["pass_count"] = st.get("pass_count", 0) + 1
        except Exception:
            pass
    event.on_memorybreakpoint(on_pass_write)

if FADETASK and "dvdx" not in PROBES:
    def on_fade_write(*args):
        # a write to a watched address inside the PLAYREC_FADETASK window (the value as the fork reports it: read
        # here, before or after the write lands; two writes in one label, one of them 0, = fadeSet in that frame).
        # With PLAYREC_WATCH (2026-10-26: the gapp dispatch, lbl_8047AC94 = the task now being dispatched,
        # _gappListTop = the list head, lbl_8047AC9C = the pending list), a "watch" row per hit: the fork's callback
        # arguments and every watched word, and for a task pointer its priority (+0x0C) and callback (+0x14)
        try:
            probe_out("fadetask.csv", "").write("%d,write,,,,,%r,,,,\n" % (frame, memory.read_f32(0x80466E38)))
            if WATCH_ADDRS:
                parts = []
                for addr in WATCH_ADDRS:
                    v = u32(addr)
                    extra = "/p%02x/cb%08x" % (u8(v + 0x0C), u32(v + 0x14)) if ram(v) else ""
                    parts.append("%08x=%08x%s" % (addr, v, extra))
                probe_out("fadewatch.csv", "frame,args,words").write("%d,%s,%s\n" % (
                    frame, repr(args).replace(",", ";"), " ".join(parts)))
        except Exception as e:
            probe_out("fadetask.csv", "").write("# write error %r\n" % (e,))
    event.on_memorybreakpoint(on_fade_write)

if "dvdx" in PROBES:
    def on_dvdx(*_args):
        # a write to executing_8047A7E8 (the dvdx probe above): the read in progress changed
        try:
            ex = memory.read_u32(0x8047A7E8)
            rc = memory.read_u32(0x8047A84C)
            cur = (ex, memory.read_u32(ex + 0x10), memory.read_u32(ex + 0x14)) \
                if 0x80000000 <= ex < 0x81800000 and ex != 0x803FC3A0 else None
            last = st.get("dvdx_exec")
            if cur == last:
                return
            if last is not None:
                s_rc, s_room, s_room_frame = st["dvdx_start"]
                lab = st.get("dvdx_label", 0)
                prev_end = st.get("dvdx_prev_end")
                probe_out("dvdx.csv", "start_frame,end_frame,frames,disc_offset,length,seek_from_prev_end,room,"
                                      "room_frame,start_retrace,end_retrace").write("%d,%d,%d,0x%x,%d,%s,%d,%d,%d,%d\n" % (
                    s_rc + lab, rc + lab, rc - s_rc, last[1], last[2], "" if prev_end is None else str(last[1] - prev_end),
                    s_room, s_room_frame, s_rc, rc))
                st["dvdx_prev_end"] = last[1] + last[2]
            if cur is not None:
                room = memory.read_u32(ROOM)
                st["dvdx_start"] = (rc, room, rc + st.get("dvdx_label", 0) - st.get("dvdx_room_at", 0))
            st["dvdx_exec"] = cur
        except Exception as e:
            probe_out("dvdx.csv", "").write("# error %r\n" % (e,))
    event.on_memorybreakpoint(on_dvdx)

# Code breakpoints (2026-10-21). playrec.py writes PLAYREC_BREAKS into the replay's GameSettings/GC6E01.ini as
# "$addr nl". In a -b run Dolphin logs no hit itself; the fork's event.on_codebreakpoint fires instead, plus a flood
# of unrelated addresses (~185k events in 6,000 boot frames), so the handler drops anything not in BREAKS first.
# One "break" event per address and frame goes to replay_events.jsonl ({"f", "ev": "break", "addr"}).
# **Only the first hit is reliable** (verified 2026-10-21 on boots with the JIT and the cached interpreter alike:
# fsysUpdateTask, called every frame, reported once, at frame 134; floorRequestChange reported again after room loads,
# which flush the code cache): a "break" event says the code ran at least once by then, not how often. Breakpoints
# leave the replay undisturbed (P02 with 0x801DEDE4: RNG identical to replay_1 on all 83,127 frames, verified).
BREAKS = set()
for part in filter(None, os.environ.get("PLAYREC_BREAKS", "").split(",")):
    BREAKS.add(int(part, 16))
if BREAKS:
    def on_break(addr):
        if addr not in BREAKS:
            return
        if st.setdefault("break_seen", {}).get(addr) != frame:
            st["break_seen"][addr] = frame
            w("break", addr="0x%08x" % addr)
    event.on_codebreakpoint(on_break)

if SHOTS or SHOTS_PC:
    os.makedirs(os.path.join(OUT, "shots"), exist_ok=True)

    def on_drawn_shot(w_, h_, data):
        name = "f%06d.png" % frame if frame in SHOTS else None
        if SHOTS_PC and LIVEPAD and LP.get("k", -1) >= 0:
            pc = LP["anchors"][LP["k"]][2] + (frame - LP["at"])
            nxt = LP["anchors"][LP["k"] + 1][2] if LP["k"] + 1 < len(LP["anchors"]) else pc
            # only while the next PC event isn't overdue (P03 run 9: a stall before Pursuit ran the PC clock on into
            # the shot window and took 76 shots of a stuck battle)
            if pc in SHOTS_PC and pc <= nxt and pc not in st.setdefault("shot_pc", set()):
                st["shot_pc"].add(pc)
                name = "f%06d_pc%06d.png" % (frame, pc)
        if name:
            raw = b"".join(b"\x00" + data[y * w_ * 3:(y + 1) * w_ * 3] for y in range(h_))

            def chunk(t, d):
                c = len(d).to_bytes(4, "big") + t + d
                return c + (zlib.crc32(t + d) & 0xFFFFFFFF).to_bytes(4, "big")
            with open(os.path.join(OUT, "shots", name), "wb") as g:
                g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", w_.to_bytes(4, "big") + h_.to_bytes(4, "big") +
                                                     bytes([8, 2, 0, 0, 0]))
                        + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))
    event.on_framedrawn(on_drawn_shot)
