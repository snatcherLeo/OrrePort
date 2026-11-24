"""Runtime id log for Felk's Dolphin scripts (testing lane; testing-plan.md 1a, "Resolving untraced rows").

Not a script by itself: the Dolphin scripts in this folder import it and call it from their own handlers (Felk's fork
takes one callback per event), so every scripted run also logs which battles, rooms and menu rooms the game starts:

    import idlog
    idlog.open_log(OUT, run="willie_ref")        # writes <OUT>/ids.jsonl
    # in the script's code-breakpoint handler:   idlog.on_code(addr, frame)
    # in the script's frame handler:             idlog.on_frame(frame)

Sources (all log-only code breakpoints in the fork's User/GameSettings/GC6E01.ini, "$<addr> nl"):
- fieldStartEncount (0x801CA5C4) and fightStartEncount (0x8020DAD0): the battle number in r3 (fieldStartEncount is the
  scripts' entry; fightStartEncount the engine's; both are logged, a battle usually shows both);
- floorRequestChange (0x800FF58C): the requested room id in r3;
- the requested-room variable (lbl_80478B18), read every frame: catches menu rooms pushed without floorRequestChange.
One JSON line per id: {"kind": "battle" | "room", "id": n, "via": ..., "frame": f, "run": ...}. Read the lines with
tools/local/testing/idlog_apply.py, which records them in docs/testing/runtime-ids.csv.
"""
import json, os
from dolphin import registers, memory

FIELD_START, FIGHT_START, ROOM_REQ = 0x801CA5C4, 0x8020DAD0, 0x800FF58C
ROOM_VAR = 0x80478B18
ADDRS = {FIELD_START: ("battle", "fieldStartEncount"), FIGHT_START: ("battle", "fightStartEncount"),
         ROOM_REQ: ("room", "floorRequestChange")}

_f = None
_run = ""
_seen = set()
_last_room = None


def open_log(out_dir, run=""):
    global _f, _run, _out_dir
    _f = open(os.path.join(out_dir, "ids.jsonl"), "a", buffering=1)
    _run = run
    _out_dir = out_dir


def _write(kind, ident, via, frame):
    if _f:
        _f.write(json.dumps({"kind": kind, "id": ident, "via": via, "frame": frame, "run": _run}) + "\n")


def on_code(addr, frame):
    """Call from the script's on_codebreakpoint handler. The fork reports a hit twice and also reports unrelated
    addresses, so only known addresses count, once per frame."""
    if addr not in ADDRS or (frame, addr) in _seen:
        return
    _seen.add((frame, addr))
    kind, via = ADDRS[addr]
    try:
        r3 = registers.read_gpr(3) & 0xFFFFFFFF
    except Exception:
        return
    _write(kind, r3 & 0xFFFF if kind == "battle" else r3, via, frame)


def install(event, script_on_frame, out_dir, run=""):
    """attach() that never breaks the calling script: on any error the script's handler is registered alone and the
    error is written to <out_dir>/idlog_error.txt."""
    try:
        attach(event, script_on_frame, out_dir, run)
    except Exception:
        import traceback
        open(os.path.join(out_dir, "idlog_error.txt"), "w").write(traceback.format_exc())
        event.on_frameadvance(script_on_frame)


def attach(event, script_on_frame, out_dir, run=""):
    """For a route script: register its frame handler wrapped so the id log runs after it every frame. Use as
        try:
            import idlog; idlog.attach(event, on_frame, OUT, run="walk")
        except Exception:
            event.on_frameadvance(on_frame)
    (if this module can't be imported, the script runs as before)."""
    open_log(out_dir, run)
    count = [0]

    def wrapped():
        script_on_frame()
        count[0] += 1
        on_frame(count[0])
    event.on_frameadvance(wrapped)


FLOOR_BATTLE = 0x8046DD90 + 0x12   # gFightFloor's battle number (u16; fight_floor_bios.c)
_last_battle = None

# Room entries as the PC reports them (pc_room_entered_hook in gs_floor.c): the floor thread switches its context to a
# new room when it unloads one room for another, or pushes a menu room on top (lbl_8047ACC4 = context index,
# lbl_8047ACC8 = the current GSFloorContext*, +0x04 floorId). Not reported, as on the PC: the first room at boot (opened
# from idle) and returns to the room below a menu (the index going down).
CTX_INDEX, CTX_PTR, RNGPTR, FLAGS = 0x8047ACC4, 0x8047ACC8, 0x80478C94, 0x80478EEC
_ctx = None
_entries = 0
entered = []   # (room, frame) of every reported entry, for the slice runner's "@R+N" steps
# Reloads the context rule misses (2026-10-29; the project's P09 report, battle 158): after a battle room pushed on top
# of the field pops, the field's context can unload and load its room again. Its index went down and its floorId
# stays the same, so _check_entry sees nothing new, while the PC's hook fires (gs_floor.c fires it whenever the
# floor state machine leaves UNLOADING for LOADING with a room). P09's console read room 0x1D's file from disc at
# frame 208,689 with no "entered". Logged as "reentered" (not "entered"), so existing entry counts and the "@R+N"
# anchors stay the same: the floor state (lbl_8047ACD8) turning LOADING (1) with no entry logged since its last load.
FLOOR_STATE = 0x8047ACD8
reentered = []   # (room, frame)
_fstate = None
_entry_since_load = True
_out_dir = "."


def flag_words():
    """The four story-flag buffers (flags.md; {u32 count, u32 buffer} per class at lbl_80478EEC, or at the address
    that label holds: both are tried, as dolphin_flags.py does), as lists of u32."""
    def read(base):
        out = []
        for c in range(4):
            n, buf = memory.read_u32(base + 8 * c), memory.read_u32(base + 8 * c + 4)
            ok = buf and 0x80000000 <= buf < 0x81800000 and 0 < n <= 0x1000
            out.append([memory.read_u32(buf + 4 * i) for i in range(n)] if ok else [])
        return out
    words = read(FLAGS)
    if not any(words):
        p = memory.read_u32(FLAGS)
        if 0x80000000 <= p < 0x81800000:
            words = read(p)
    return words


def flags_crc(words):
    """CRC-32 of classes 0-3 in order, each word big-endian (the PC computes the same over its flag buffers)."""
    import zlib
    data = b"".join(w.to_bytes(4, "big") for cls in words for w in cls)
    return zlib.crc32(data) & 0xFFFFFFFF


def _check_entry(frame):
    global _ctx, _entries
    try:
        idx, ptr = memory.read_u32(CTX_INDEX), memory.read_u32(CTX_PTR)
        fid = memory.read_u32(ptr + 4) if 0x80000000 <= ptr < 0x81800000 else 0
    except Exception:
        return
    cur = (idx, ptr, fid)
    if cur == _ctx:
        return
    prev, _ctx = _ctx, cur
    if prev is None or not fid or prev[2] == 0 or idx < prev[0]:
        return                                   # boot room from idle, nothing loaded yet, or a pop back to a room
    room = memory.read_u32(ROOM_VAR)
    words = flag_words()
    p = memory.read_u32(RNGPTR)
    rng = memory.read_u32(p) if 0x80000000 <= p < 0x81800000 else None
    _entries += 1
    entered.append((room, frame))
    global _entry_since_load
    _entry_since_load = True
    _write_entry(room, frame, rng, flags_crc(words))
    if _f:
        import json as _j
        _j.dump(words, open(os.path.join(_out_dir, f"flags_entry_{_entries:03d}_{room:x}.json"), "w"))


def _check_reload(frame, room):
    """a load (floor state turning LOADING) that _check_entry didn't report: see FLOOR_STATE"""
    global _fstate, _entry_since_load
    try:
        state = memory.read_u32(FLOOR_STATE)
    except Exception:
        return
    if state == 1 and _fstate not in (None, 1):
        if not _entry_since_load and _entries and room != 0xFFFFFFFF:
            reentered.append((room, frame))
        _entry_since_load = False
    _fstate = state


def _write_entry(room, frame, rng, crc):
    if _f:
        _f.write(json.dumps({"kind": "entered", "id": room, "frame": frame, "rng": rng, "flags_crc": crc,
                             "n": _entries, "run": _run}) + "\n")


def on_frame(frame):
    """Call from the script's frame handler: logs every change of the requested-room variable (menu rooms too) and
    every battle that starts (the battle number in gFightFloor turning non-zero or changing). Polling is the main
    source: in Felk's fork a code breakpoint reports only its first hit (verified 2026-10-17), so the breakpoint
    hooks above catch only the first battle and room request of a run."""
    global _last_room, _last_battle
    try:
        room = memory.read_u32(ROOM_VAR)
        battle = memory.read_u16(FLOOR_BATTLE)
    except Exception:
        return
    _check_entry(frame)
    _check_reload(frame, room)
    if room != _last_room:
        if _last_room is not None and room and room != 0xFFFFFFFF:  # 0xFFFFFFFF = no room (during a battle)
            _write("room", room, "room variable", frame)
        _last_room = room
    if battle != _last_battle:
        # the value present when the script starts is a leftover of an earlier battle (the field keeps the last
        # battle's number), so only changes count (verified 2026-10-17: Wakin's 26 read at frame 1 after a load)
        if battle and _last_battle is not None:
            _write("battle", battle, "battle number", frame)
        _last_battle = battle
