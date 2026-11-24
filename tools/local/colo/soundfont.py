"""A SoundFont 2 (.sf2) for a MusyX song group, so the exported MIDI files play with the game's own samples in any
player (docs/engine/music.md, "SoundFont"). Read-only builder (returns bytes, no file output).

What it reproduces: each program's samples and key ranges (pages -> macro / keymap / layer -> key-split jumps ->
start-sample steps, as synth.c / synthmacros.c pick them), each sample's base note (sample directory info >> 24) and
loop. It also carries each macro's linear ADSR volume envelope (mcmdSetADSR mode 0) and its vibrato (mcmdVibrato: rate and
depth, on the modulation wheel when the macro asks for that). What it does not: the exact envelope curve shapes (MusyX's
are linear, SoundFont's decay is in decibels), vibrato phase, pitch sweeps, velocity / random splits
(the fall-through branch is used), volume and pan steps, reverb. So it is an approximation of the instruments, good for study."""
import math
import struct

from .musyx import parse_project, parse_pool, dsp_adpcm

KEY_SPLIT = 0x02  # jump to another macro when the note is >= arg (synthmacros.c)
JUMPS_IGNORED = (0x03, 0x0A, 0x13)  # velocity / modulation / random splits: the fall-through branch is taken
VIBRATO = 0x1C


def _vibrato(p0, p1):
    """mcmdVibrato (synthmacros.c; applied in synth.c): period = high u16 of the second word, in milliseconds when bit 0 of
    its byte 2 is set (every step on the disc is; tick mode is not converted here); depth = key range (byte 2 of the
    first word) semitones + cent range (byte 1) cents, peak deviation of a sine; bits 0-1 of byte 0 make the depth follow
    the modulation wheel. Returns (rate Hz, depth cents, follows mod wheel) or None (period 0 = vibrato off)."""
    period = p1 >> 16
    if not period or not (p1 >> 8) & 1: return None
    kr = ((p0 >> 8) & 0xFF) - (0x100 if p0 & 0x8000 else 0); cr = ((p0 >> 16) & 0xFF) - (0x100 if p0 & 0x800000 else 0)
    if kr > 0 and cr < 0: depth = (kr - 1) * 100 + 100 - cr  # the code's own rule for this sign mix
    else: depth = abs(kr) * 100 + abs(cr)
    return 1000.0 / period, depth, bool((p0 >> 24) & 3)


def _adsr(pool, curve_id):
    """A linear-mode ADSR curve from the pool (mcmdSetADSR mode 0, hwSetADSR, synthmacros.c / hardware.c): little-endian
    u16 attack, decay, sustain level, release; times likely in milliseconds; sustain level << 3 is out of 0x7FFF.
    Returns (attack s, decay s, sustain fraction, release s) or None."""
    cv = pool["curves"].get(curve_id)
    if not cv or len(cv) < 8: return None
    a, d, sl, r = struct.unpack_from("<4H", cv, 0)
    return a / 1000.0, d / 1000.0, min(0x7FFF, sl << 3) / 0x7FFF, r / 1000.0


def _zones(pool, obj, lo=0, hi=127, transpose=0, seen=None, adsr=None, vib=None):
    """[(lo, hi, sample id, transpose, adsr, vibrato)] that object `obj` plays for keys lo..hi (adsr / vibrato: what the
    macro set before starting the sample, or None)."""
    seen = set() if seen is None else seen
    key = (obj, lo, hi)
    if key in seen or lo > hi or obj == 0xFFFF: return []
    env = adsr; vb = vib
    seen.add(key); kind = obj & 0xC000; out = []
    if kind == 0:
        steps = pool["macros"].get(obj, [])
        for i, (p0, p1) in enumerate(steps):
            op = p0 & 0x7F
            if op == 0x0C and not p0 >> 24:  # SetADSR, linear mode
                env = _adsr(pool, (p0 >> 8) & 0xFFFF) or env
            elif op == VIBRATO:
                vb = _vibrato(p0, p1)
            elif op == KEY_SPLIT:
                k = (p0 >> 8) & 0xFF; target = (p0 >> 16) & 0xFFFF
                out += _zones(pool, target, max(lo, k), hi, transpose, seen, env, vb)
                hi = min(hi, k - 1)
                if lo > hi: break
            elif op == 0x10:
                # the music macros set their vibrato just after starting the sample, so look ahead in this macro too
                for q0, q1 in steps[i + 1:]:
                    if q0 & 0x7F == VIBRATO: vb = _vibrato(q0, q1); break
                    if q0 & 0x7F in (0x00, 0x01, 0x06, KEY_SPLIT) + JUMPS_IGNORED: break
                out.append((lo, hi, (p0 >> 8) & 0xFFFF, transpose, env, vb)); break
            elif op in (0x00, 0x01, 0x06): break
    elif kind == 0x4000:
        km = pool["keymaps"].get(obj, [])
        for k in range(lo, hi + 1):
            if k < len(km) and km[k][0] != 0xFFFF:
                out += _zones(pool, km[k][0], k, k, transpose + km[k][1], seen, adsr, vib)
    elif kind == 0x8000:
        for o, klo, khi, tr, vol, pr, pan in pool["layers"].get(obj, []):
            out += _zones(pool, o, max(lo, klo), min(hi, khi), transpose + tr, seen, adsr, vib)
    merged = []
    for z in sorted(out, key=lambda z: z[:4]):  # join neighbouring keys that play the same sample the same way (keymaps)
        if merged and merged[-1][2:] == z[2:] and merged[-1][1] + 1 == z[0]: merged[-1] = (merged[-1][0], z[1]) + z[2:]
        else: merged.append(z)
    return merged


def _samples(sdir, samp, wanted):
    """{id: (pcm list, rate, base note, loop (start, end) or None)} for the wanted DSP-ADPCM samples."""
    out = {}; i = 0
    while i + 0x20 <= len(sdir):
        sid = struct.unpack_from(">H", sdir, i)[0]
        if sid == 0xFFFF: break
        off, _, info, length, loop_off, loop_len, extra = struct.unpack_from(">IIIIIII", sdir, i + 4); i += 0x20
        if sid not in wanted or length >> 24 != 0 or extra + 0x28 > len(sdir): continue
        n = length & 0xFFFFFF
        coefs = [struct.unpack_from(">hh", sdir, extra + 8 + 4 * k) for k in range(8)]
        pcm = dsp_adpcm(samp[off:off + (n + 13) // 14 * 8], n, coefs)
        looped = loop_len and loop_off != 0xFFFFFFFF and loop_off + loop_len <= n  # 0xFFFFFFFF = no loop (sample 0x245)
        out[sid] = (pcm, info & 0xFFFF, info >> 24, (loop_off, loop_off + loop_len) if looped else None)
    return out


def _chunk(cid, data): return cid + struct.pack("<I", len(data)) + data + (b"\0" if len(data) & 1 else b"")
def _list(lid, data): return _chunk(b"LIST", lid + data)
def _name(s, n=20): return s.encode("ascii", "replace")[:n - 1].ljust(n, b"\0")


def build(proj, pool_bytes, sdir, samp, group_id=None, name="Colosseum"):
    """The .sf2 bytes for one song group (the first song group in the project when group_id is None). Returns
    (bytes, report dict)."""
    pool = parse_pool(pool_bytes)
    g = next(x for x in parse_project(proj) if x["type"] == 0 and (group_id is None or x["id"] == group_id))
    progs = [(0, p, o[0]) for p, o in sorted(g["normal_pages"].items())] + [(128, p, o[0]) for p, o in sorted(g["drum_pages"].items())]
    zones = {(b, p): _zones(pool, obj) for b, p, obj in progs}
    smp = _samples(sdir, samp, {z[2] for zs in zones.values() for z in zs})
    # sample data: each sample followed by 46 zero frames (SoundFont rule)
    smpl = bytearray(); shdr = bytearray(); index = {}
    for sid in sorted(smp):
        pcm, rate, root, loop = smp[sid]; start = len(smpl) // 2
        smpl += struct.pack(f"<{len(pcm)}h", *pcm) + b"\0" * 92
        ls, le = (start + loop[0], start + loop[1]) if loop else (start, start + len(pcm))
        shdr += _name(f"s{sid:04X}") + struct.pack("<IIIIIBbHH", start, start + len(pcm), ls, le, rate, min(127, root), 0, 0, 1)
        index[sid] = (len(index), root, loop is not None)
    shdr += _name("EOS") + b"\0" * 26
    inst = bytearray(); ibag = bytearray(); igen = bytearray(); imod = bytearray(); phdr = bytearray(); pbag = bytearray(); pgen = bytearray()
    for n_inst, (bank, prog, obj) in enumerate(progs):
        inst += _name(f"{'drum' if bank else 'prog'} {prog} {obj:#06x}") + struct.pack("<H", len(ibag) // 4)
        for lo, hi, sid, tr, env, vib in zones[(bank, prog)]:
            if sid not in index: continue
            si, root, looped = index[sid]
            ibag += struct.pack("<HH", len(igen) // 4, len(imod) // 10)
            igen += struct.pack("<HBB", 43, lo, hi)                                   # keyRange
            if env:  # volume envelope: times in timecents, sustain as attenuation in centibels (linear -> SF2's curves)
                tc = lambda sec: max(-12000, min(8000, int(round(1200 * math.log2(sec))))) if sec > 0 else -12000
                igen += struct.pack("<Hh", 34, tc(env[0])) + struct.pack("<Hh", 36, tc(env[1]))
                igen += struct.pack("<Hh", 37, 1440 if env[2] <= 0 else max(0, min(1440, int(round(-200 * math.log10(env[2]))))))
                igen += struct.pack("<Hh", 38, tc(env[3]))
            if vib:  # vibrato LFO: rate in absolute cents; depth as a generator, or on the modulation wheel (CC1)
                igen += struct.pack("<Hh", 24, max(-16000, min(4500, int(round(1200 * math.log2(vib[0] / 8.176))))))
                if vib[2]: imod += struct.pack("<HHhHH", 0x0081, 6, min(32767, vib[1]), 0, 0)  # CC1 -> vibLfoToPitch
                else: igen += struct.pack("<Hh", 6, min(32767, vib[1]))                      # vibLfoToPitch
            igen += struct.pack("<Hh", 58, max(0, min(127, root - tr)))              # overridingRootKey
            if looped: igen += struct.pack("<HH", 54, 1)                              # sampleModes: loop
            igen += struct.pack("<HH", 53, si)                                        # sampleID
        phdr += _name(f"{'Drums' if bank else 'Program'} {prog}") + struct.pack("<HHHIII", prog, bank, len(pbag) // 4, 0, 0, 0)
        pbag += struct.pack("<HH", len(pgen) // 4, 0)
        pgen += struct.pack("<HH", 41, n_inst)                                        # instrument
    inst += _name("EOI") + struct.pack("<H", len(ibag) // 4); ibag += struct.pack("<HH", len(igen) // 4, len(imod) // 10)
    igen += b"\0" * 4; imod += b"\0" * 10
    phdr += _name("EOP") + struct.pack("<HHHIII", 0, 0, len(pbag) // 4, 0, 0, 0); pbag += struct.pack("<HH", len(pgen) // 4, 0); pgen += b"\0" * 4
    info = _list(b"INFO", _chunk(b"ifil", struct.pack("<HH", 2, 1)) + _chunk(b"isng", b"EMU8000\0") + _chunk(b"INAM", name.encode() + b"\0")
                 + _chunk(b"ICMT", b"Built from the player's own disc by colo/soundfont.py; approximates MusyX instruments\0"))
    sdta = _list(b"sdta", _chunk(b"smpl", bytes(smpl)))
    pdta = _list(b"pdta", _chunk(b"phdr", bytes(phdr)) + _chunk(b"pbag", bytes(pbag)) + _chunk(b"pmod", b"\0" * 10)
                 + _chunk(b"pgen", bytes(pgen)) + _chunk(b"inst", bytes(inst)) + _chunk(b"ibag", bytes(ibag))
                 + _chunk(b"imod", bytes(imod)) + _chunk(b"igen", bytes(igen)) + _chunk(b"shdr", bytes(shdr)))
    body = b"sfbk" + info + sdta + pdta
    report = {"group": g["id"], "programs": len(progs), "samples": len(smp),
              "empty_programs": [f"{'drum' if b else 'prog'} {p}" for b, p, o in progs if not zones[(b, p)]]}
    return b"RIFF" + struct.pack("<I", len(body)) + body, report
