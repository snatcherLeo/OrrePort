"""MusyX songs (arrangements, resource type 4 in bgm_archive.fsys) -> events -> standard MIDI
(docs/engine/music.md, "Song file format"). Read-only reader (no file output): a port candidate, see docs/tools.md
"Readers". Layout and event rules come from the decompiled sequencer: seqStartPlay (src/musyx/runtime/seq.c,
fn_801463C4), GenerateNextTrackEvent (seq_exact_80146E88.c), the event handler fn_801485FC and HandleTrackEvents
(seq_residual_801485FC.c), the types in include/musyx/runtime/seq_types.h."""
import struct

TICKS_PER_BEAT = 384  # seq called 5 x 256 units per 5 ms frame; ticks/s = bpm*1024*256000/40960000 = bpm*384/60
END_TIME = 0xFFFFFFFF


def _u32(b, o): return struct.unpack_from(">I", b, o)[0]


def _stream(b, off):
    """A pitch-bend / modulation stream (GetStreamValue): [(delta_ticks, delta_value), ...] up to 0x80 0x00."""
    out = []
    while off + 1 < len(b):
        b1, b2 = b[off], b[off + 1]
        if b1 == 0x80 and b2 == 0: break
        if b1 & 0x80: dt = ((b1 & 0x7F) << 8) | b2; off += 2
        else: dt = b1; off += 1
        b1 = b[off]
        if b1 & 0x80:
            v = ((b1 & 0x7F) << 8) | b[off + 1]; v |= (v & 0x4000) << 1; off += 2
            v = v - 0x10000 if v & 0x8000 else v
        else:
            v = b1 | ((b1 & 0x40) << 1); v = v - 0x100 if v & 0x80 else v; off += 1
        out.append((dt, v))
    return out


def parse_pattern(b, off):
    """One pattern (SEQ_PATTERN: header length, pitch-bend stream offset, modulation stream offset, then note data at
    +0x0C). Returns {'notes': [(time, key, velocity, length)], 'controls': [(time, key, velocity)], 'pitch': stream,
    'mod': stream, 'end': time}. Times are pattern-relative ticks."""
    _, pb, mod = struct.unpack_from(">III", b, off)
    p = off + 0x0C; t = 0; notes = []; ctrls = []
    while p + 4 <= len(b):
        dt, key, vel = struct.unpack_from(">HBB", b, p)
        if key == 0xFF and vel == 0xFF:  # end of pattern (its time is not added: the runtime checks before)
            break
        t += dt
        if key & 0x80: ctrls.append((t, key, vel)); p += 4
        elif key | vel == 0: p += 4  # time step only
        else: notes.append((t, key, vel, struct.unpack_from(">H", b, p + 4)[0])); p += 6
    return {"notes": notes, "controls": ctrls, "end": t,
            "pitch": _stream(b, pb) if pb else [], "mod": _stream(b, mod) if mod else []}


def parse_song(b):
    """Parse a song file. Returns a dict: header fields, tempo map, tracks (entries), patterns, channel per track."""
    t_tab, p_tab, tm_tab, m_track, info = struct.unpack_from(">IIIII", b, 0)
    s = {"track_table": t_tab, "pattern_table": p_tab, "channel_table": tm_tab, "tempo_track": m_track, "info": info,
         "bpm": (info & 0x0FFFFFFF) >> (10 if info & 0x40000000 else 0), "loop_tick": _u32(b, 0x14),
         "has_sections": bool(info & 0x80000000),
         "sections": None, "tempo": [], "tracks": {}, "patterns": {}}
    if s["has_sections"]: s["sections"] = list(b[_u32(b, 0x54):_u32(b, 0x54) + 64])  # tsTab (none on this disc)
    if m_track:
        o = m_track
        while o + 8 <= len(b):
            t, bpm = struct.unpack_from(">II", b, o); o += 8
            if t == END_TIME: break
            s["tempo"].append((t, bpm >> 10 if info & 0x40000000 else bpm))
    used = set()
    for i in range(64):
        to = _u32(b, t_tab + 4 * i)
        if not to: continue
        entries = []; o = to
        while o + 12 <= len(b):
            t, prg, vel, _, _, pat, tr, va = struct.unpack_from(">IBBBBHbb", b, o)
            if pat == 0xFFFF: entries.append({"time": t, "end": True}); break
            if pat == 0xFFFE:  # loop: jump back to entry index (u16 in the transpose / velocityAdd bytes)
                entries.append({"time": t, "loop_to": struct.unpack_from(">H", b, o + 10)[0]}); break
            entries.append({"time": t, "program": None if prg == 0xFF else prg, "volume": None if vel == 0xFF else vel,
                            "pattern": pat, "transpose": tr, "velocity_add": va}); used.add(pat)
            o += 12
        s["tracks"][i] = {"channel": b[tm_tab + i], "entries": entries}
    for pat in sorted(used):
        s["patterns"][pat] = parse_pattern(b, _u32(b, p_tab + 4 * pat))
    return s


def control_name(key, vel):
    """Meaning of a control event (key bit 7 set) as handled by fn_801485FC."""
    v = key & 0x7F
    if vel == 0: return ("program", v)
    if vel == 1: return ("ctrl_0x82", v)  # MusyX's controller 0x82 (fn_801603C0); not a MIDI controller
    if vel & 0x80:
        c = vel & 0x7F
        return {0x68: ("crossfade_sync", v), 0x69: ("page_select", v), 0x6A: ("page_select_hi", v),
                0x79: ("reset_controllers", v), 0x7B: ("all_notes_off", v)}.get(c, ("cc", c, v))
    return ("ignored", vel, v)


def song_events(s, loops=1):
    """Flatten a song into absolute-tick events per track, as the sequencer plays it (no section table):
    loops = how many times the 0xFFFE loop jumps are followed. Yields dicts sorted by tick:
    {tick, track, channel, kind, ...}. Kinds: note (key, velocity, length), program, volume, cc, pitch (14-bit value),
    mod (14-bit), loop_start / loop_end markers, and the others from control_name."""
    # HandleTrackEvents: a 0xFFFE entry jumps its track back to entry loop_to and stops it; once no track has events
    # left, song time jumps to loop_tick and every looping track restarts from its jump target (entry times are
    # absolute, so they replay unchanged). One pass therefore lasts from loop_tick to the last loop / end entry.
    ev = []
    tails = [tr["entries"][-1] for tr in s["tracks"].values() if tr["entries"]]
    loop_len = None
    if any("loop_to" in e for e in tails):
        loop_len = max(e["time"] for e in tails) - s["loop_tick"]
    for ti, tr in s["tracks"].items():
        ch = tr["channel"]; ents = tr["entries"]; k = 0; offset = 0; jumps = 0
        while k < len(ents):
            e = ents[k]
            if e.get("end"): break
            if "loop_to" in e:
                if jumps >= loops or not loop_len or loop_len <= 0: break
                offset += loop_len; jumps += 1; k = e["loop_to"]; continue
            base = e["time"] + offset
            if e["program"] is not None: ev.append({"tick": base, "track": ti, "channel": ch, "kind": "program", "value": e["program"]})
            if e["volume"] is not None: ev.append({"tick": base, "track": ti, "channel": ch, "kind": "cc", "cc": 7, "value": e["volume"]})
            p = s["patterns"][e["pattern"]]
            for t, key, vel, ln in p["notes"]:
                ev.append({"tick": base + t, "track": ti, "channel": ch, "kind": "note",
                           "key": max(0, min(127, key + e["transpose"])), "velocity": max(0, min(127, vel + e["velocity_add"])),
                           "length": ln})
            for t, key, vel in p["controls"]:
                c = control_name(key, vel)
                if c[0] == "cc": ev.append({"tick": base + t, "track": ti, "channel": ch, "kind": "cc", "cc": c[1], "value": c[2]})
                else: ev.append({"tick": base + t, "track": ti, "channel": ch, "kind": c[0], "value": c[-1]})
            for name, start in (("pitch", 0x2000), ("mod", 0)):
                t = 0; v = start
                for dt, dv in p[name]:
                    t += dt; v += dv
                    ev.append({"tick": base + t, "track": ti, "channel": ch, "kind": name, "value": v & 0x3FFF})
            k += 1
    ev.sort(key=lambda e: (e["tick"], e["track"]))
    return ev, loop_len


def _vlq(n):
    out = [n & 0x7F]; n >>= 7
    while n: out.append(0x80 | (n & 0x7F)); n >>= 7
    return bytes(reversed(out))


def to_midi(s, name="", loops=1, setup=None):
    """Standard MIDI file (type 1, 384 ticks per beat): track 0 = tempo map, loop markers, the song name and the MIDI
    setup; one MIDI track per MusyX track. setup: the song's MIDI setup from the project (16 x (program, volume, pan,
    reverb, chorus)), applied at tick 0 the way seqStartPlay applies it. MusyX program numbers are kept as they are
    (they are not General MIDI instruments)."""
    evs, loop_len = song_events(s, loops)
    tracks = []
    meta = [(0, b"\xFF\x03" + _vlq(len(name)) + name.encode())]
    if setup:
        used = {tr["channel"] & 15 for tr in s["tracks"].values()}
        for ch, (prg, vol, pan, rev, cho) in enumerate(setup):
            if ch not in used: continue
            meta += [(0, bytes([0xC0 | ch, prg & 0x7F])), (0, bytes([0xB0 | ch, 7, vol & 0x7F])),
                     (0, bytes([0xB0 | ch, 10, pan & 0x7F])), (0, bytes([0xB0 | ch, 91, rev & 0x7F])),
                     (0, bytes([0xB0 | ch, 93, cho & 0x7F]))]
    tempo = s["tempo"] or [(0, s["bpm"])]
    for t, bpm in tempo:
        us = int(round(60_000_000 / max(bpm, 1)))
        meta.append((t, b"\xFF\x51\x03" + us.to_bytes(3, "big")))
    if loop_len is not None:
        for j in range(loops + 1):
            lt = s["loop_tick"] + j * loop_len
            meta.append((lt, b"\xFF\x06" + _vlq(len(b"loop")) + b"loop"))
    tracks.append(meta)
    for ti in sorted(s["tracks"]):
        ch = s["tracks"][ti]["channel"] & 15; out = [(0, b"\xFF\x03" + _vlq(len(f"track {ti}")) + f"track {ti}".encode())]
        for e in evs:
            if e["track"] != ti: continue
            k = e["kind"]; t = e["tick"]
            if k == "note":
                out.append((t, bytes([0x90 | ch, e["key"], max(1, e["velocity"])])))
                out.append((t + max(1, e["length"]), bytes([0x80 | ch, e["key"], 0])))
            elif k == "program": out.append((t, bytes([0xC0 | ch, e["value"] & 0x7F])))
            elif k == "cc" and e["cc"] < 0x78: out.append((t, bytes([0xB0 | ch, e["cc"], e["value"] & 0x7F])))
            elif k == "pitch": out.append((t, bytes([0xE0 | ch, e["value"] & 0x7F, e["value"] >> 7 & 0x7F])))
            elif k == "mod": out.append((t, bytes([0xB0 | ch, 1, e["value"] >> 7 & 0x7F])))
            elif k == "all_notes_off": out.append((t, bytes([0xB0 | ch, 0x7B, 0])))
            elif k == "reset_controllers": out.append((t, bytes([0xB0 | ch, 0x79, 0])))
        tracks.append(out)
    data = b"MThd" + struct.pack(">IHHH", 6, 1, len(tracks), TICKS_PER_BEAT)
    for tr in tracks:
        tr.sort(key=lambda x: (x[0], 0 if x[1][0] & 0xF0 == 0x80 else 1))  # note-offs before note-ons at the same tick
        body = bytearray(); last = 0
        for t, m in tr: body += _vlq(t - last) + m; last = t
        body += b"\x00\xFF\x2F\x00"
        data += b"MTrk" + struct.pack(">I", len(body)) + bytes(body)
    return data


def summary(s):
    """Plain-text summary of a parsed song (header, tempo, per-track entries and pattern statistics)."""
    L = [f"bpm {s['bpm']} (info 0x{s['info']:08X}), loop tick {s['loop_tick']}, sections: {'yes' if s['has_sections'] else 'no'}",
         f"tempo changes: {len(s['tempo'])}" + (": " + ", ".join(f"{t}:{b}" for t, b in s["tempo"][:8]) if s["tempo"] else "")]
    for ti, tr in s["tracks"].items():
        ents = tr["entries"]; pats = [e for e in ents if "pattern" in e]
        nn = sum(len(s["patterns"][e["pattern"]]["notes"]) for e in pats)
        tail = ents[-1] if ents else {}
        how = f"loops to entry {tail['loop_to']} at tick {tail['time']}" if "loop_to" in tail else f"ends at tick {tail.get('time')}"
        progs = sorted({e["program"] for e in pats if e["program"] is not None})
        L.append(f"track {ti:2}: channel {tr['channel']:2}, {len(pats)} pattern entries, {nn} notes, programs {progs}, {how}")
    return "\n".join(L)
