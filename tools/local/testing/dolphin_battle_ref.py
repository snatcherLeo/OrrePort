"""Dolphin reference run of a battle, per action and per turn (testing lane): the record the PC run is compared with.

Runs **inside** Felk's Dolphin scripting fork (separate GPL program, never linked into OrrePort):

    set POS_OUT=<folder with the start state>   set BATTLE_STATE=willie_start.sav   (default)
    set BATTLE_MOVES={"196": [93], "197": [0]}    (optional; species id -> move ids per turn, see below)
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_battle_ref.py

Loads the state (both sides out, the first command menu), plays the battle with the controller and writes, in
<POS_OUT>/battle_ref/:
- ref.jsonl: one JSON line per event: "start" (battle number, RNG state, every Pokémon on the field), "menu" (which
  menu opened, for which Pokémon), "choice" (the move slot chosen and the button presses), "action" (a move used:
  attacker, target, move id, RNG state), "hp" (an HP change: before, after, RNG state), "turn" (the turn counter moved:
  RNG state, turn random, every Pokémon's HP and PP, hash of the story flags and of the battle-state block), "end";
- turn_<NN>.bin: the raw battle-state block (gFightFloor, 0xA4E8 bytes) at each turn change, for a byte diff;
- flags_start.json / flags_end.json: the four story-flag buffers (decode with flag_report.py);
- choices.json: the menu choices per turn and Pokémon (command, move slot, target), so a PC run can repeat them;
- ids.jsonl: the runtime id log (idlog.py).

Moves: BATTLE_MOVES maps a species id to a list of move ids, one per turn (the last repeats); 0 = the first slot.
Default: Espeon (196) Confusion (93) every turn, Umbreon (197) its first move. If a move isn't in the Pokémon's set,
the first slot is used and the run logs it.

How it drives the menus: log-only code breakpoints (the fork's User/GameSettings/GC6E01.ini, "$<addr> nl") on the
command menu (fightMenuFightTrainerGcHeroOpenMenu, 0x8026316C), the move list (...OpenMenuSubWaza, 0x80264D58;
r4 = the battling Pokémon) and the target choice (...SubWazaSelectDefensePokemon, 0x8026503C). Command menu: A
(FIGHT is the default). Move list: the cursor starts on the slot this Pokémon chose last (status 0x101, likely; 0 at
the start), so the script presses Down / Up by the difference, then A. Target: A (the default target). If nothing
changes for BATTLE_IDLE_FRAMES (150), it presses A (battle messages such as "gained EXP" wait for A) and logs a nudge.
**Limit found 2026-10-17 (runs 1-4, verified):** in this fork each code breakpoint reports only its **first** hit
(with the JIT and with the cached interpreter alike), so the menu hooks steer only the first command menu, move list
and target choice of a run; after that the idle A presses answer the menus. That still plays the default plan exactly
(the move list keeps its cursor, and Confusion is Espeon's slot 0, Umbreon's plan is slot 0), and each turn's PP
snapshot shows which move each Pokémon used. A plan that needs other slots needs a memory signal for "menu open"
(open). Register values at a hit are only right with the cached interpreter (-C Dolphin.Core.CPUCore=5).

Memory (verified from the code: fight_floor_bios.c, fight_trainer_db_range_801FBD10.c, pokemon_data.h):
gFightFloor = 0x8046DD90: +0x10 turn counter (u16), +0x12 battle number (u16), +0xA474 acting Pokémon, +0xA478 target,
+0xA4BA the move named in the current message (u16), +0xA4C4 the 8 field slots (battling-Pokémon pointers), +0xA4E4
the turn random (u16). Battling Pokémon +0x04 = its Pokémon record: +0x000 species, +0x078 4 x {u16 move, u8 PP,
u8 PP-ups}, +0x08A HP, +0x08C max HP. RNG state: *(u32*)*lbl_80478C94. Story flags: lbl_80478EEC (dolphin_flags.py).
"""
import json, os, sys, time, traceback, zlib

OUT_BASE = os.environ.get("POS_OUT", ".")
STATE = os.environ.get("BATTLE_STATE", "willie_start.sav")
OUT = os.path.join(OUT_BASE, "battle_ref")
os.makedirs(OUT, exist_ok=True)
# the fork shows script errors only in its window: write them to a file, and make this folder importable
HERE = os.environ.get("TESTING_DIR") or os.environ.get("BATTLE_SCRIPT_DIR") or (os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else "")
if HERE:
    sys.path.insert(0, HERE)
try:
    from dolphin import controller, event, memory, registers, savestate
    import idlog
except Exception:
    open(os.path.join(OUT, "startup_error.txt"), "w").write(traceback.format_exc() + f"\nsys.path={sys.path}\n")
    raise
MOVES = {int(k): v for k, v in json.loads(os.environ.get("BATTLE_MOVES", '{"196": [93], "197": [0]}')).items()}

FLOOR, FLOOR_SIZE = 0x8046DD90, 0xA4E8
RNGPTR, FLAGS = 0x80478C94, 0x80478EEC
CMD_MENU, WAZA_MENU, TARGET_MENU = 0x8026316C, 0x80264D58, 0x8026503C
MENUS = {CMD_MENU: "command", WAZA_MENU: "moves", TARGET_MENU: "target"}
MAX_FRAMES = int(os.environ.get("BATTLE_MAX_FRAMES", "36000"))  # 10 minutes of game time
IDLE_FRAMES = int(os.environ.get("BATTLE_IDLE_FRAMES", "150"))  # nothing changed this long: press A (a message waits)

log_f = open(os.path.join(OUT, "ref.jsonl"), "w", buffering=1)
idlog.open_log(OUT, run="battle_ref:" + STATE)


def log(**kw):
    log_f.write(json.dumps(kw) + "\n")


def ram(a):
    return 0x80000000 <= a < 0x81800000


def u8(a): return memory.read_u8(a)
def u16(a): return memory.read_u16(a)
def u32(a): return memory.read_u32(a)


def rng():
    p = u32(RNGPTR)
    return u32(p) if ram(p) else None


def rng_steps(a, b, limit=50000):
    """Generator steps from state a to state b (rng.md: s = s * 0x343FD + 0x269EC3); -1 if not within limit."""
    if a is None or b is None:
        return 0
    n, s = 0, a
    while s != b:
        s = (s * 0x343FD + 0x269EC3) & 0xFFFFFFFF
        n += 1
        if n > limit:
            return -1
    return n


def mon(op):
    """One Pokémon on the field: field-slot (FightOutPokemon) pointer -> +0x04 its FightPokemon -> +0x04 its Pokémon
    record (fightOutPokemonGetPokemonPtr: status 0xD6 = lwz 4(r3), then 0xCC = +4; verified from main.dol)."""
    if not ram(op):
        return None
    fp = u32(op + 4)
    if not ram(fp):
        return None
    pk = fp + 4
    return {"ptr": hex(op), "record": hex(pk), "species": u16(pk), "hp": u16(pk + 0x8A), "max_hp": u16(pk + 0x8C),
            "moves": [[u16(pk + 0x78 + 4 * k), u8(pk + 0x7A + 4 * k)] for k in range(4)]}


def field():
    return [mon(u32(FLOOR + 0xA4C4 + 4 * i)) for i in range(8)]


def flags():
    out = {}
    for c in range(4):
        n, buf = u32(FLAGS + 8 * c), u32(FLAGS + 8 * c + 4)
        if buf and ram(buf) and 0 < n <= 0x1000:
            out[str(c)] = [u32(buf + 4 * i) for i in range(n)]
    return out


def flags_hash():
    return zlib.crc32(json.dumps(flags(), sort_keys=True).encode()) & 0xFFFFFFFF


def floor_bytes():
    return b"".join(u32(FLOOR + i).to_bytes(4, "big") for i in range(0, FLOOR_SIZE, 4))


frame = 0
st = {"started": False, "turn": None, "waza": None, "hp": {}, "queue": [], "cursor": {}, "turn_choices": {},
      "choices": [], "idle": 0, "ended": False, "end_frame": None, "pending_waza": None, "seen": set(),
      "mon_turns": {}, "menus_this_turn": 0}


_sr = os.environ.get("BATTLE_SHOT_RANGE")
SHOT_RANGE = (lambda s, st: (int(s.split("-")[0]), int(s.split("-")[1]), int(st)))(*_sr.split(":")) if _sr else None
SEED = os.environ.get("BATTLE_SEED")      # set the RNG state at the start (the PC does the same: !seed)
PAD = os.environ.get("BATTLE_PAD")        # replay a recorded press list instead of reacting to menus
st["presses"] = []


def press(buttons, wait_before=20, hold=3, gap=8):
    """Queue a sequence of single-button presses, starting wait_before frames from now (recorded for battle.pad).
    In replay mode (BATTLE_PAD) the script's own presses are off: only the file's presses happen."""
    if PAD:
        return
    t = frame + wait_before
    for b in buttons:
        st["queue"].append((t, t + hold, b))
        st["presses"].append((t, hold, b))
        t += hold + gap


def write_pad():
    """battle.pad: every press of this run as B's pad-script steps, relative to the start (the frame the seed is set),
    chained with "+N" so the PC can put it after its own "!seed" step."""
    t0 = st.get("seed_frame", 5)
    lines = ["# Willie battle presses from dolphin_battle_ref.py, relative to the seed step (each line: +frames:what)"]
    prev = t0
    for t, hold, b in sorted(st["presses"]):
        if t < prev:
            continue
        lines.append(f"+{t - prev}:{b.upper()}")
        lines.append(f"+{hold}:-")
        prev = t + hold
    open(os.path.join(OUT, "battle.pad"), "w").write("\n".join(lines) + "\n")


def load_pad(path, t0):
    """Read a battle.pad (only '+N:WHAT' steps; WHAT is a button or '-') into queue entries starting at frame t0."""
    t, cur = t0, None
    for line in open(path):
        line = line.split("#")[0].strip()
        if not line:
            continue
        when, what = line.split(":", 1)
        t += int(when.lstrip("+"))
        if cur:
            st["queue"].append((cur[0], t, cur[1]))
            cur = None
        if what != "-":
            cur = (t, {"START": "Start", "UP": "Up", "DOWN": "Down", "LEFT": "Left",
                       "RIGHT": "Right"}.get(what, what))
    if cur:
        st["queue"].append((cur[0], cur[0] + 3, cur[1]))


AI_PICK = 0x802397B8   # fightTrainerAiGetValueAryMaxBanme(s32* valueAry, u16 count, u8 useRandom); needs "$802397b8 nl"


def ai_scores():
    """the project's request: the first score array the trainer AI picks from (a breakpoint reports only its first hit in this
    fork, so this is one array: likely Willie's first Pokemon, turn 1). Registers are only right with the cached
    interpreter (-C Dolphin.Core.CPUCore=5)."""
    try:
        r3, r4, r5 = (registers.read_gpr(i) & 0xFFFFFFFF for i in (3, 4, 5))
    except Exception as e:
        log(ev="ai_scores", frame=frame, error=repr(e))
        return
    count = r4 & 0xFFFF
    vals = [memory.read_s32(r3 + 4 * i) for i in range(min(count, 32))] if ram(r3) else None
    lr = None
    for name in ("read_lr", "read_spr"):
        try:
            lr = getattr(registers, name)() if name == "read_lr" else registers.read_spr(8)
            break
        except Exception:
            continue
    log(ev="ai_scores", frame=frame, turn=st.get("turn"), array=hex(r3), count=count, use_random=r5 & 0xFF,
        values=vals, caller_lr=hex(lr) if isinstance(lr, int) else None)


def on_code(addr):
    idlog.on_code(addr, frame)
    if addr == AI_PICK and not st.get("ai_done"):
        st["ai_done"] = True
        ai_scores()
        return
    if addr not in MENUS or (frame, addr) in st["seen"]:
        return
    st["seen"].add((frame, addr))
    kind = MENUS[addr]
    try:
        r3, r4, r5 = (registers.read_gpr(i) & 0xFFFFFFFF for i in (3, 4, 5))
    except Exception:
        r3 = r4 = r5 = 0
    log(ev="hit", frame=frame, menu=kind, r3=hex(r3), r4=hex(r4), r5=hex(r5))
    shot(kind)
    st["expect"] = None
    if kind == "command":
        log(ev="menu", frame=frame, menu=kind)
        # the menu takes a while to accept input: A now, then again every 40 frames until the move list opens
        press(["A"], wait_before=30)
        st["expect"] = {"what": "moves", "next": frame + 70, "tries": 0}
    elif kind == "moves":
        m = mon(r4)
        if not (m and m["species"] in MOVES):
            # fallback (the breakpoint's registers may not be the function's arguments): the n-th move menu of the
            # turn belongs to the n-th Pokémon of the move plan
            k = st["menus_this_turn"]
            sp_guess = list(MOVES)[k] if k < len(MOVES) else None
            m = next((x for x in field() if x and x["species"] == sp_guess), m)
        st["menus_this_turn"] += 1
        sp = m["species"] if m else None
        n = st["mon_turns"].get(sp, 0)
        st["mon_turns"][sp] = n + 1
        plan = MOVES.get(sp, [0])
        want = plan[min(n, len(plan) - 1)] if plan else 0
        ids = [mv[0] for mv in m["moves"]] if m else []
        slot = ids.index(want) if want in ids else 0
        if want and want not in ids:
            log(ev="note", frame=frame, msg=f"move {want} not in species {sp}'s set {ids}; using slot 0")
        cur = st["cursor"].get(sp, 0)
        keys = ["Down"] * (slot - cur) if slot >= cur else ["Up"] * (cur - slot)
        st["cursor"][sp] = slot
        press(keys + ["A"])
        log(ev="menu", frame=frame, menu=kind, species=sp, moves=ids)
        log(ev="choice", frame=frame, species=sp, slot=slot, move=ids[slot] if slot < len(ids) else None,
            presses=keys + ["A"], cursor_from=cur)
        st["choices"].append({"turn": st["turn"], "species": sp, "command": "FIGHT", "slot": slot,
                              "move": ids[slot] if slot < len(ids) else None, "target": "default", "from": "menu"})
    elif kind == "target":
        log(ev="menu", frame=frame, menu=kind)
        press(["A"])


SHOTS = os.path.join(OUT, "shots")
os.makedirs(SHOTS, exist_ok=True)
pending_shots = []


def shot(name):
    """Screenshot of the next drawn frame (for seeing what the game waits on)."""
    pending_shots.append(f"{frame:06d}_{name}")


def write_png(path, w, h, rgb):
    import struct
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    with open(path, "wb") as g:
        g.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b""))


def on_drawn(w, h, data):
    while pending_shots:
        name = pending_shots.pop()
        try:
            write_png(os.path.join(SHOTS, name + ".png"), w, h, data)
        except Exception as e:
            log(ev="shot_error", name=name, err=repr(e))


def moves_used(fl, why, turn_used):
    """Which move each Pokémon used since the last snapshot, from its PP going down (works without the menu hooks).
    turn_used = the turn those moves belong to (the snapshot at the start of turn N closes turn N - 1)."""
    prev = st.get("pp", {})
    for m in fl:
        if not m:
            continue
        pp = [mv[1] for mv in m["moves"]]
        old = prev.get(m["record"])
        if old:
            for k, (a, b) in enumerate(zip(old, pp)):
                if b < a:
                    log(ev="used", frame=frame, turn=turn_used, species=m["species"], record=m["record"], slot=k,
                        move=m["moves"][k][0], pp_before=a, pp_after=b, seen_at=why)
                    st["choices"].append({"turn": turn_used, "species": m["species"], "slot": k,
                                          "move": m["moves"][k][0], "from": "PP change"})
        prev[m["record"]] = pp
    st["pp"] = prev


def snapshot_turn(turn):
    moves_used(field(), f"turn {turn} start", turn - 1)
    data = floor_bytes()
    with open(os.path.join(OUT, f"turn_{turn:02d}.bin"), "wb") as f:
        f.write(data)
    log(ev="turn", frame=frame, turn=turn, rng=rng(), turn_random=u16(FLOOR + 0xA4E4), field=field(),
        flags_crc=flags_hash(), floor_crc=zlib.crc32(data) & 0xFFFFFFFF)


def on_frame():
    global frame
    frame += 1
    if frame == 1:
        savestate.load_from_file(os.path.join(OUT_BASE, STATE))
        return
    if frame < 5:
        return
    idlog.on_frame(frame)
    # inputs
    held = {}
    for t0, t1, b in st["queue"]:
        if t0 <= frame < t1:
            held[b] = True
    st["queue"] = [q for q in st["queue"] if q[1] > frame]
    if held:
        controller.set_gc_buttons(0, held)
    if st["ended"]:
        if frame - st["end_frame"] > 240:
            json.dump(flags(), open(os.path.join(OUT, "flags_end.json"), "w"))
            log(ev="end", frame=frame, rng=rng())
            json.dump(st["choices"], open(os.path.join(OUT, "choices.json"), "w"), indent=1)
            if not PAD:
                write_pad()
            log_f.flush()
            time.sleep(1)
            os._exit(0)
        return
    enc, turn = u16(FLOOR + 0x12), u16(FLOOR + 0x10)
    fl = field()
    live = [m for m in fl if m]
    if not st["started"]:
        if enc and live:
            st["started"] = True
            st["turn"] = turn
            st["seed_frame"] = frame
            if SEED:
                p = u32(RNGPTR)
                if ram(p):
                    memory.write_u32(p, int(SEED, 0) & 0xFFFFFFFF)
                    log(ev="seed", frame=frame, value=int(SEED, 0))
            if PAD:
                load_pad(PAD, frame)
                log(ev="replay", frame=frame, pad=PAD, presses=len(st["queue"]))
            json.dump(flags(), open(os.path.join(OUT, "flags_start.json"), "w"))
            log(ev="start", frame=frame, battle=enc, turn=turn, rng=rng(), turn_random=u16(FLOOR + 0xA4E4),
                field=fl, state=STATE, moves_plan={str(k): v for k, v in MOVES.items()})
            snapshot_turn(turn)
            for i, m in enumerate(fl):
                if m:
                    st["hp"][i] = m["hp"]
            # the start state is saved with the first command menu already open: its breakpoint has passed
            log(ev="menu", frame=frame, menu="command", note="open in the start state")
            press(["A"], wait_before=30)
        elif frame > 1800:
            log(ev="error", frame=frame, msg="no battle found in the state after 1800 frames")
            os._exit(1)
        return
    changed = False
    # RNG calls this frame (for the project's battle / RNG pages): steps of the generator from last frame's state to this one
    cur_rng = rng()
    calls = rng_steps(st.get("rng_prev"), cur_rng)
    if calls:
        log(ev="rng_calls", frame=frame, turn=turn, calls=calls, state=cur_rng)
    st["rng_prev"] = cur_rng
    # BATTLE_SHOT_RANGE=lo-hi:step (frames since the state loaded): screenshots across a stretch (A18, the move camera)
    if SHOT_RANGE and SHOT_RANGE[0] <= frame <= SHOT_RANGE[1] and (frame - SHOT_RANGE[0]) % SHOT_RANGE[2] == 0:
        shot("range")
    # the acting Pokémon changing = the next action of the turn (the first one follows the move-order sort)
    att_ptr = u32(FLOOR + 0xA474)
    if att_ptr != st.get("att_ptr"):
        st["att_ptr"] = att_ptr
        a = mon(att_ptr)
        if a:
            log(ev="attacker", frame=frame, turn=turn, species=a["species"], record=a["record"], rng=cur_rng)
    # a move named in the message box = an action
    waza = u16(FLOOR + 0xA4BA)
    if waza != st["waza"]:
        st["waza"] = waza
        if waza:
            att, tgt = mon(u32(FLOOR + 0xA474)), mon(u32(FLOOR + 0xA478))
            log(ev="action", frame=frame, turn=turn, move=waza, rng=rng(),
                attacker=att and att["species"], target=tgt and tgt["species"])
            changed = True
    # HP per Pokémon (its record address), not per slot: the game re-sorts the field slots by turn order
    for i, m in enumerate(fl):
        if not m:
            continue
        key = m["record"]
        if st["hp"].get(key) is not None and m["hp"] != st["hp"][key]:
            log(ev="hp", frame=frame, turn=turn, slot=i, species=m["species"], record=key, before=st["hp"][key],
                after=m["hp"], damage=st["hp"][key] - m["hp"], rng=rng(),
                critical_msg=u32(FLOOR + 0xA4AC), effect_msg=u32(FLOOR + 0xA4B0))
            changed = True
        st["hp"][key] = m["hp"]
    if turn != st["turn"]:
        st["turn"] = turn
        st["menus_this_turn"] = 0
        snapshot_turn(turn)
        changed = True
    room = u32(idlog.ROOM_VAR)
    if st.get("room") is None:
        st["room"] = room
    if not enc or not live or room != st["room"] or frame > MAX_FRAMES:
        st["ended"], st["end_frame"] = True, frame
        moves_used(fl, "battle over", st["turn"])
        log(ev="battle_over", frame=frame, rng=rng(), field=fl, battle_field=enc, room=room,
            why="no battle" if not enc or not live else "room changed" if room != st["room"] else "frame cap")
        return
    ex = st.get("expect")
    if ex and frame >= ex["next"] and not st["queue"]:
        if ex["tries"] >= 10:
            log(ev="note", frame=frame, msg=f"gave up waiting for the {ex['what']} menu")
            st["expect"] = None
        else:
            ex["tries"] += 1
            ex["next"] = frame + 40
            log(ev="retry", frame=frame, waiting_for=ex["what"], tries=ex["tries"])
            shot("retry")
            press(["A"], wait_before=1)
    st["idle"] = 0 if (changed or st["queue"]) else st["idle"] + 1
    if st["idle"] > IDLE_FRAMES:  # battle messages ("gained EXP", "fainted") wait for A
        st["idle"] = 0
        log(ev="nudge", frame=frame)
        shot("nudge")
        press(["A"], wait_before=1)


event.on_frameadvance(on_frame)
event.on_codebreakpoint(on_code)
event.on_framedrawn(on_drawn)
