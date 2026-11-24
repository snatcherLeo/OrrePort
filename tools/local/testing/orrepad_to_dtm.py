r"""Turn a PC input recording (.orrepad, docs/port/pad-recording.md) into a Dolphin movie (.dtm), for the reference playthrough's running
Dolphin check (the reference playthrough Part 1 item 4, 2026-11-01).

    python -I tools/local/testing/orrepad_to_dtm.py <recording.orrepad> <out.dtm> --offset N
        [--template <sitting.dtm>] [--frames F] [--rtc template|recording] [--compare <sitting.dtm>]

How the two sides line up (verified on Run 2's 40 console movies, 2026-11-01): Dolphin polls the controller twice per
field from power-on (every movie's poll count is twice its field count within 2), and the game reads once per field
(inputPadCallback keeps the latest poll; the PC's PADRead runs once per refresh). So each movie field gets two polls,
both the state the PC read in that field's game frame; a field the PC has no record for repeats the last state.

--offset N: the movie field where the PC's game frame 0 falls (it may be negative). The records' game frame counts from
the PC's own power-on, so this is the console's field minus the PC's frame at the same moment, best taken at the first
room entry (Dolphin's replay_events.jsonl against the PC log). *Corrected 2026-11-01:* it is not dtm_to_pad's "record 0
at power-on frame N": on P25 that number is 799, while the PC reached the same inputs 533 frames later (offset -533;
measured from the press timing, 159 of 159 console presses in order). Fields before it are a centred, connected pad
(the console movies' boot polls look the same).

One offset fits only the stretch between two room loads (P25, 2026-11-01): the PC's loads take another number of frames
than the console's (112 frames shorter at room 5, 85 more at room 15), and the movie is frame-exact.

--pc-log <run.log> --anchors <replay_events.jsonl> [--lag L] (the project's plan, 2026-11-01: the mirror of dtm_to_pad's
--window-frameat): each input is retimed from the last anchor before it, the PC log's room entries, message windows
and battle messages paired with the same ones in a Dolphin replay's events (the movie's field = that file's frame).
Live the reference playthrough sittings: a first Dolphin pass of the plain movie gives the console's anchor frames, the second plays the
retimed movie; iterate if a later room shifts (each pass fixes the rooms up to the first that drifted). Proof on P25
(verified on the bytes, against replay_3's events): 113 anchors, buttons equal on 99.4% of fields; every console
press reproduced within 2 fields, plus the PC rerun's one extra A press. --lag -1 is that sitting's best.

--template: the console movie whose header to copy (game ID, settings, disc MD5, RTC start); the frame, input and tick
counts are rewritten. Without it a header for GC6E01 with Run 2's settings is built (the recording profile's: single
core, custom RTC). --rtc: the movie's start time (Dolphin plays a movie on its recorded start time): `recording` takes
the .orrepad header's replay-clock start (OSTime ticks since 2000-01-01, 40.5 MHz), `template` keeps the template's
(Run 2: 0x386D4380, 2000-01-01). Default: the recording's when it has one.

--compare: the console movie of the same sitting; reports, per field from the offset on, whether the converted movie's
second poll (the one the game reads) equals the console's, and the first differences. On a PC replay of a Run 2
sitting this measures how far the pad script (with its resyncs) is from the recording.

Port 1 only (the .orrepad holds four ports; Colosseum's story mode reads port 1). Analog A / B pressure has no place in
a movie and is dropped.
"""
import argparse
import bisect
import collections
import difflib
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "port"))
from orrepad_to_pad import read_recording  # noqa: E402  (the project's reader, tools/local/port)

OS_TIMER_HZ = 40_500_000          # OSTime ticks per second (bus clock / 4)
UNIX_2000 = 946_684_800           # 2000-01-01 00:00:00 in Unix seconds (the GameCube's epoch)
CPU_HZ = 486_000_000              # the movie's tick count is CPU ticks
FIELD_HZ = 60_000 / 1001          # NTSC fields per second
BITS0 = (0x1000, 0x0100, 0x0200, 0x0400, 0x0800, 0x0010, 0x0008, 0x0004)  # START A B X Y Z UP DOWN (dtm_to_pad)
BITS1 = (0x0001, 0x0002, 0x0040, 0x0020)                                  # LEFT RIGHT L R
CONNECTED = 0x40                  # byte 1 bit 6: set on every poll of the console movies
NEUTRAL = bytes([0, CONNECTED, 0, 0, 0x80, 0x80, 0x80, 0x80])


def poll_bytes(button, sx, sy, cx, cy, lt, rt):
    """a PADStatus (s8 sticks, centre 0, Y up) as the movie's 8-byte controller state: buttons regrouped, sticks
    0..255 with 0x80 the centre and Y above 0x80 up, triggers as they are"""
    b0 = sum(1 << i for i, m in enumerate(BITS0) if button & m)
    b1 = sum(1 << i for i, m in enumerate(BITS1) if button & m) | CONNECTED
    return bytes([b0, b1, lt, rt, (sx + 128) & 255, (sy + 128) & 255, (cx + 128) & 255, (cy + 128) & 255])


def default_header():
    """a GC6E01 movie header with the recording profile's settings (sitting 36's header, 2026-10-23, field by
    field), for when no console movie is at hand"""
    h = bytearray(0x100)
    h[0:4] = b"DTM\x1a"
    h[4:10] = b"GC6E01"
    h[0x0B] = 1                                   # controllers: port 1
    h[0x51:0x54] = b"D3D"                         # video backend name (informational)
    struct.pack_into("<Q", h, 0x81, 0x386D4380)   # recording start: 2000-01-01, the profile's custom RTC
    # 0x89-0xEC as sitting 36 has them: "settings saved" (1), then skip idle, dual core (off), progressive, DSP HLE,
    # fast disc, CPU core, the EFB / XFB flags, memory cards, clear save, ..., and the Dolphin revision / DSP ROM
    # hashes. The disc MD5 (0x71) is left zero: give --template for a movie Dolphin checks against the disc.
    h[0x89:0xED] = bytes.fromhex(
        "0101000001000100010100000001010000000001000001010001000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000000000a22afcb10b97c251e17e46f2982d4e37729d32a10000000000000000")
    return h


PC_ENTER = re.compile(r"\[slice\] entered 0x([0-9A-Fa-f]+) frame=(\d+)")
PC_MSG = re.compile(r"\[ev\] frame=(\d+) msgwin type=(-?\d+)")


def replay_anchors(events):
    """a Dolphin replay's anchor events in time order: (frame, (kind, room / window type / message id))"""
    out = []
    for line in open(events, encoding="utf-8"):
        e = json.loads(line)
        if e.get("ev") in ("entered", "reentered"):
            out.append((e["f"], ("room", e["room"])))
        elif e.get("ev") == "msgwin" and not (OPENINGS_ONLY and e["type"] < 0):
            out.append((e["f"], ("window", e["type"])))
        elif e.get("ev") == "battle_msg":
            out.append((e["f"], ("battle message", e["id"])))
    return out


# --openings-only (2026-11-02, the reference playthrough P01): a window's closing (type -1) is caused by the press that dismisses it, so
# anchoring the presses after it on the previous pass's closing makes each pass later than the last (P01: the first
# window stays open 181 frames on the PC; passes 1-3 gave 181, 200, 239). Only events presses answer (rooms entered,
# windows opening, battle messages) anchor then.
OPENINGS_ONLY = False


def anchor_pairs(pc_log, events):
    """[(PC frame, console frame, kind)] for the room entries and message windows both sides have, in order: rooms
    paired on their room sequence, then windows room by room on their type sequence (replay_report's pairing), so a
    room or window one side lacks doesn't shift the rest. Pairs whose console frame goes backwards are dropped."""
    pr, pm, pb = [], [], []
    for line in open(pc_log, encoding="utf-8", errors="replace"):
        m = PC_ENTER.search(line)
        if m:
            pr.append((int(m.group(2)), int(m.group(1), 16)))
            continue
        m = PC_MSG.search(line)
        if m:
            if not (OPENINGS_ONLY and int(m.group(2)) < 0):
                pm.append((int(m.group(1)), int(m.group(2))))
            continue
        if line.startswith("[bmsg] "):   # frame,retrace,id (ORRE_BATTLE_LOG=1)
            f, _, mid = line[7:].strip().split(",")[:3]
            pb.append((int(f), int(mid)))
    dr, dm, db = [], [], []
    for line in open(events, encoding="utf-8"):
        e = json.loads(line)
        if e.get("ev") in ("entered", "reentered"):
            dr.append((e["f"], e["room"]))
        elif e.get("ev") == "msgwin" and not (OPENINGS_ONLY and e["type"] < 0):
            dm.append((e["f"], e["type"]))
        elif e.get("ev") == "battle_msg":
            db.append((e["f"], e["id"]))
    pairs = []
    rooms = []
    sm = difflib.SequenceMatcher(None, [r for _, r in pr], [r for _, r in dr], autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            rooms += list(zip(range(i1, i2), range(j1, j2)))
    for (i, j) in rooms:
        pairs.append((pr[i][0], dr[j][0], "room"))
    for n, (i, j) in enumerate(rooms):
        # the windows from this room entry to the next paired one, on each side
        p_hi = pr[rooms[n + 1][0]][0] if n + 1 < len(rooms) else 1 << 40
        d_hi = dr[rooms[n + 1][1]][0] if n + 1 < len(rooms) else 1 << 40
        # message windows (field text), then battle messages (a battle opens no message window)
        for kind, ps, ds in (("window", pm, dm), ("battle message", pb, db)):
            pk = [w for w in ps if pr[i][0] <= w[0] < p_hi]
            dk = [w for w in ds if dr[j][0] <= w[0] < d_hi]
            sw = difflib.SequenceMatcher(None, [t for _, t in pk], [t for _, t in dk], autojunk=False)
            for tag, i1, i2, j1, j2 in sw.get_opcodes():
                if tag == "equal":
                    pairs += [(p[0], d[0], kind) for p, d in zip(pk[i1:i2], dk[j1:j2])]
    pairs.sort()
    out = []
    for p in pairs:
        if not out or (p[0] > out[-1][0] and p[1] > out[-1][1]):
            out.append(p)
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("recording")
    ap.add_argument("out")
    ap.add_argument("--offset", type=int, default=0, help="movie field of the PC's game frame 0 (without anchors)")
    ap.add_argument("--pc-log", help="the PC run's log (its room entries and message windows: the PC's anchors)")
    ap.add_argument("--anchors", nargs="+", help="newest pass first; " +"a Dolphin replay's replay_events.jsonl of the same stretch (the console's frames "
                                      "for those anchors); with --pc-log, each input is retimed from the anchor before it")
    ap.add_argument("--lag", type=int, default=0, help="fields added to every anchored time (default 0)")
    ap.add_argument("--openings-only", action="store_true",
                    help="no window closings as anchors (they are the presses' own effect; see OPENINGS_ONLY)")
    ap.add_argument("--battle-anchors", help="replay_events.jsonl of a Dolphin pass with the PC's rolls injected: its "
                                             "battle messages pair with the PC log's own (see the code)")
    ap.add_argument("--template", help="a console movie whose header to copy (settings, disc MD5, RTC)")
    ap.add_argument("--frames", type=int, help="fields to write (default: the last record's frame + offset + 600)")
    ap.add_argument("--rtc", choices=("template", "recording"), help="the movie's start time (see above)")
    ap.add_argument("--compare", help="the sitting's console movie: compare the read polls field by field")
    a = ap.parse_args()
    global OPENINGS_ONLY
    OPENINGS_ONLY = a.openings_only

    stamp, records, epoch = read_recording(a.recording, with_epoch=True)
    if not records:
        sys.exit(f"{a.recording}: no records")
    # the state each game frame ends with (the last record read in it), port 1
    by_frame = {}
    for r in records:
        by_frame[r[2]] = NEUTRAL if r[10] != 0 else poll_bytes(*r[3:10])
    last_frame = max(by_frame)
    if a.pc_log or a.anchors:
        if not (a.pc_log and a.anchors):
            sys.exit("--pc-log and --anchors go together")
        # several files, newest pass first (the project's plan: pass N+1 retimes from pass N's own Dolphin playback). The last
        # (oldest) is the reference path: its anchors are paired with the PC log. Each newer pass is the playback of the
        # movie built from the passes before it, so where its anchor lands is where the game really was, against the
        # time that movie aimed for. Up to the first anchor that missed its aim (by more than 3 frames of jitter) the aim
        # held; that one takes the real time, and the miss carries to every later anchor (a pass's later anchors may
        # follow a wrong input: P25's second pass entered room 0x3A1 12 frames late, pressed early, and its next window
        # opened 40 frames late). Its events pair with the reference path's over their common prefix.
        pairs = anchor_pairs(a.pc_log, a.anchors[-1])
        ref = replay_anchors(a.anchors[-1])
        bases = [p[1] for p in pairs]   # each anchor's frame on the reference path
        for newer in reversed(a.anchors[:-1]):
            pn = replay_anchors(newer)
            real = {}
            for (fo, ko), (fn, kn) in zip(ref, pn):
                if ko != kn:
                    break
                real[fo] = fn
            merged, d, missed, held, taken = [], 0, None, True, 0
            for p, base in zip(pairs, bases):
                aim = p[1]
                n = real.get(base)
                # in a battle any frame counts: the random numbers advance every frame (idle blinks, the camera), so a
                # press a few frames off rolls other numbers (P25's third pass: its battle ran 4 frames early from
                # reference frame 4,521 and parted at 6,040, another battle message); a field window may wobble 3
                tol = 0 if p[2] == "battle message" else 3
                if missed is None and n is not None and abs(n - aim) > tol:
                    missed, d = base, n - aim
                    merged.append((p[0], n, p[2]))
                    taken += 1
                elif (missed is not None and held and n is not None
                      and abs(n - aim - d) <= (tol if p[2] == "battle message" or OPENINGS_ONLY else 20)):
                    # *Added 2026-11-02 (the reference playthrough P01):* after the first miss, the newer pass's times are taken while
                    # its shift moves by at most 20 frames from one anchor to the next (field presses have that
                    # slack: P01's pass 2 went +39, then +54 to +64 from 0x3A1 to room 0x2, then -311 where it parted;
                    # battle messages exact); a bigger jump stops it, and the last shift carries from there
                    merged.append((p[0], n, p[2]))
                    d = n - aim
                    taken += 1
                else:
                    if missed is not None and base != missed:
                        held = False
                    merged.append((p[0], aim + (d if missed is not None and base != missed else 0), p[2]))
            print(f"anchors from {newer}: " + (f"first miss at reference frame {missed} ({d:+d}); {taken} anchors "
                                               "taken at that shift, the rest carried"
                                               if missed is not None else "every anchor it reached was on time"))
            pairs = merged
        if a.battle_anchors and pairs:
            # with the PC's rolls injected (PLAYREC_ROLLINJECT) Dolphin's battle is the PC's, not the recording's: its
            # battle messages pair with the PC's own [bmsg] lines in order (their common prefix), and replace the
            # battle-message anchors. Anchors after the last of them move by the difference there (the PC's battle
            # can be longer or shorter than the recording's)
            pb = [(int(l[7:].split(",")[0]), int(l[7:].split(",")[2])) for l in open(a.pc_log, encoding="utf-8",
                  errors="replace") if l.startswith("[bmsg] ")]
            db = [(f, k[1]) for f, k in replay_anchors(a.battle_anchors) if k[0] == "battle message"]
            bp = []
            for (pf, pid), (df, did) in zip(pb, db):
                if pid != did:
                    break
                bp.append((pf, df + a.lag, "battle message"))
            if bp:
                # every reference anchor inside the PC's battle goes (its windows pair with the recording's battle,
                # which went otherwise: P25's live run, PC frame 4,440 paired with 5,179); after the PC's last battle
                # message the reference resumes, moved so that it continues from the last injected anchor
                lo, hi = pb[0][0], pb[-1][0]
                before = [p for p in pairs if p[0] < lo]
                after = [p for p in pairs if p[0] > hi]
                d = 0
                if after and bp[-1][0] == hi:
                    d = bp[-1][1] - (after[0][1] - (after[0][0] - hi))
                elif after:
                    # the injected pass stopped inside the battle: continue from its last anchor
                    d = bp[-1][1] + (after[0][0] - bp[-1][0]) - after[0][1]
                pairs = before + bp + [(p[0], p[1] + d, p[2]) for p in after]
                print(f"battle anchors from {a.battle_anchors}: {len(bp)} of the PC's {len(pb)} battle messages "
                      f"paired; later anchors moved {d:+d}")
        if not pairs:
            sys.exit("no anchor shared by the PC log and the replay events")
        pcs = [p[0] for p in pairs]

        def to_field(g):
            k = max(0, bisect.bisect_right(pcs, g) - 1)
            return pairs[k][1] + (g - pairs[k][0]) + a.lag
        kinds = collections.Counter(p[2] for p in pairs)
        print(f"anchors: {len(pairs)} ({kinds['room']} room entries, {kinds['window']} message windows, "
              f"{kinds['battle message']} battle messages); shift "
              f"{pairs[0][1] - pairs[0][0]:+d} at the first, {pairs[-1][1] - pairs[-1][0]:+d} at the last")
    else:
        def to_field(g):
            return g + a.offset
    # movie field -> the state from that field on. Fields only go forward: where a later anchor's shift is smaller, a
    # release could land on or before its press and the press would stay held (P25's first Dolphin pass, 2026-11-01:
    # A held from field 587, the game waited 1,569 frames for a new press in room 0x384), so each change goes at
    # least one field after the one before it
    # Only a new press is retimed at the anchor before it; a release or a stick-only change keeps its distance from the
    # change before it. The anchors are what the game showed (a window opening, a room), which is what a press answers;
    # a release only ends its press. Retiming a release too shortened the press (P25: the PC opens the first window 15
    # frames after the press, the console 27, so the next A shrank to 2 fields and the game missed it).
    at, prev, prev_g, prev_bits = {}, None, None, 0
    for g in sorted(by_frame):
        s = by_frame[g]
        bits = s[0] | s[1] << 8
        if prev is None or bits & ~prev_bits & ~(CONNECTED << 8):
            f = to_field(g)
        else:
            f = prev + (g - prev_g)
        if prev is not None and f <= prev:
            f = prev + 1
        at[f], prev, prev_g, prev_bits = s, f, g, bits
    fields = a.frames or (max(at) + 600)

    early = [f for f in at if f < 0]
    polls, cur = bytearray(), (at[max(early)] if early else NEUTRAL)
    for m in range(fields):
        cur = at.get(m, cur)
        polls += cur * 2

    if a.template:
        head = bytearray(open(a.template, "rb").read(0x100))
        if head[:4] != b"DTM\x1a":
            sys.exit(f"{a.template}: not a Dolphin movie")
    else:
        head = default_header()
    rtc = a.rtc or ("recording" if epoch else "template")
    if rtc == "recording":
        if not epoch:
            sys.exit(f"{a.recording}: no replay-clock start in its header (an ORREPAD1 file or a run off the replay "
                     "clock); use --rtc template")
        struct.pack_into("<Q", head, 0x81, UNIX_2000 + epoch // OS_TIMER_HZ)
    struct.pack_into("<QQQ", head, 0x0D, fields, len(polls) // 8, 0)   # fields, inputs, lag
    struct.pack_into("<I", head, 0x2D, 0)                              # re-records
    struct.pack_into("<Q", head, 0xED, int(fields / FIELD_HZ * CPU_HZ))  # CPU ticks (the progress display)
    with open(a.out, "wb") as f:
        f.write(head + polls)
    start = struct.unpack_from("<Q", head, 0x81)[0]
    print(f"{a.out}: {fields} fields, {len(polls) // 8} polls, game frame 0 at field {a.offset}, {len(records)} records "
          f"to frame {last_frame} (build '{stamp}'), start time {start:#x} ({rtc})")

    if a.compare:
        d = open(a.compare, "rb").read()
        con = [d[0x100 + 8 * i:0x108 + 8 * i] for i in range((len(d) - 0x100) // 8)]
        n = min(fields, len(con) // 2)
        same, diffs = 0, []
        for m in range(max(0, a.offset), n):
            ours, theirs = polls[16 * m + 8:16 * m + 16], con[2 * m + 1]
            if ours == theirs:
                same += 1
            elif len(diffs) < 10:
                diffs.append((m, m - a.offset, theirs.hex(), bytes(ours).hex()))
        total = max(1, n - max(0, a.offset))
        print(f"compare with {a.compare}: second poll equal on {same} of {total} fields ({100 * same / total:.1f}%) "
              f"from field {a.offset}")
        for m, g, t, o in diffs:
            print(f"  field {m} (game frame {g}): console {t}, converted {o}")


if __name__ == "__main__":
    main()
