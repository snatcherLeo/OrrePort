"""MusyX audio: GameCube DSP-ADPCM decoding (docs/engine/extractor.md, music.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct

def dsp_adpcm(data, nsamples, coefs):
    """GameCube DSP-ADPCM -> 16-bit PCM samples (8-byte frames: header byte + 14 signed nibbles)."""
    out = []; h1 = h2 = 0; p = 0
    while len(out) < nsamples and p < len(data):
        ps = data[p]; pred, scale = (ps >> 4) & 7, 1 << (ps & 15); c1, c2 = coefs[pred]
        for i in range(14):
            if len(out) >= nsamples: break
            byte = data[p + 1 + i // 2]; nib = (byte >> 4) if i % 2 == 0 else (byte & 15)
            if nib >= 8: nib -= 16
            v = ((nib * scale) << 11) + c1 * h1 + c2 * h2 + 1024 >> 11
            v = max(-32768, min(32767, v)); out.append(v); h2, h1 = h1, v
        p += 8
    return out



def sdir_samples(sdir, samp):
    """Yield one dict per DSP-ADPCM sample of a MusyX bank: id, offset, rate, sample count, loop, pcm.
    sdir = snd_<bank>_sdir entry (common.fsys); samp = sound/snd_<bank>.samp (SdirDataT, synthdata.c)."""
    i = 0
    while i + 0x20 <= len(sdir):
        sid = struct.unpack(">H", sdir[i:i + 2])[0]
        if sid == 0xFFFF: break
        off, _, info, length, loop_off, loop_len, extra = struct.unpack(">IIIIIII", sdir[i + 4:i + 0x20])
        i += 0x20
        rate = info & 0xFFFF; nsamp = length & 0xFFFFFF; fmt = length >> 24
        if fmt != 0 or not rate or extra + 0x28 > len(sdir): continue  # not DSP-ADPCM: skipped for now
        coefs = [struct.unpack(">hh", sdir[extra + 8 + 4 * k:extra + 12 + 4 * k]) for k in range(8)]
        yield {"id": sid, "offset": off, "rate": rate, "samples": nsamp, "loop": (loop_off, loop_len),
               "pcm": dsp_adpcm(samp[off:off + (nsamp + 13) // 14 * 8], nsamp, coefs)}


# --- Project (snd_<bank>_proj) and pool (snd_<bank>_pool): which macros, samples and programs a group has ---------
# Layouts from the decompiled runtime: GROUP_DATA (include/musyx/runtime/s_data_types.h, sndPushGroup / seqPlaySong in
# s_data.c), PAGE and MIDISETUP (seq.c, BuildTransTab / seqStartPlay), the pool entries (include/musyx/synthdata.h,
# GetPoolAddr in s_data.c), object ids by their top bits (synth.c: 0 = macro, 0x4000 = keymap, 0x8000 = layer) and
# the macro steps (synthmacros.c: 8 bytes, opcode = byte 3 & 0x7F; 0x10 = start sample, id in bytes 1-2).

def _id_list(b, o):
    """ScanIDList: u16 ids; an id with bit 15 set starts a range (id & 0x3FFF .. next u16); ends at 0xFFFF."""
    out = []
    while o + 2 <= len(b):
        v = struct.unpack_from(">H", b, o)[0]
        if v == 0xFFFF: break
        if v & 0x8000:
            hi = struct.unpack_from(">H", b, o + 2)[0]; out.extend(range(v & 0x3FFF, hi + 1)); o += 4
        else: out.append(v); o += 2
    return out


def _pages(b, o):
    """PAGE list (6 bytes: u16 object id, u8 priority, u8 max voices, u8 program, u8 reserved) up to program 0xFF.
    Returns {program: (object id, priority, max voices)}."""
    out = {}
    while o + 6 <= len(b):
        obj, prio, maxv, prog = struct.unpack_from(">HBBB", b, o)
        if prog == 0xFF: break
        out[prog] = (obj, prio, maxv); o += 6
    return out


def parse_project(b):
    """A MusyX project: list of groups (GROUP_DATA, linked by nextOff until 0xFFFFFFFF). Type 0 = song group (normal
    and drum program pages, MIDI setups per song id), type 1 = sound-effect group (FX table)."""
    groups = []; o = 0
    while o + 0x28 <= len(b):
        nxt, gid, gtype, mac, smp, cur, km, lay, a, bb, c = struct.unpack_from(">IHHIIIIIIII", b, o)
        g = {"id": gid, "type": gtype, "macros": _id_list(b, mac), "samples": _id_list(b, smp),
             "curves": _id_list(b, cur), "keymaps": _id_list(b, km), "layers": _id_list(b, lay)}
        if gtype == 0:
            g["normal_pages"] = _pages(b, a); g["drum_pages"] = _pages(b, bb); g["midi_setups"] = {}
            m = c
            while m + 4 <= len(b):
                sid = struct.unpack_from(">H", b, m)[0]
                if sid == 0xFFFF: break
                g["midi_setups"][sid] = [tuple(b[m + 4 + 5 * ch:m + 9 + 5 * ch]) for ch in range(16)]  # program, volume, pan, reverb, chorus
                m += 4 + 80
        else:
            n = struct.unpack_from(">H", b, a)[0]; g["fx_count"] = n
        groups.append(g)
        if nxt == 0xFFFFFFFF: break
        o = nxt
    return groups


def parse_pool(b):
    """A MusyX pool: header {macro, curve, keymap, layer offsets}; each list is entries {u32 next, u16 id, u16 0,
    data} until next = 0xFFFFFFFF. Returns {'macros': {id: [(para0, para1)]}, 'keymaps': {id: [(obj, transpose, pan,
    prio)] x128}, 'layers': {id: [(obj, keyLow, keyHigh, transpose, volume, prio, pan)]}, 'curves': {id: bytes}}."""
    mo, co, ko, lo = struct.unpack_from(">IIII", b, 0)
    def walk(o):
        while o and o + 8 <= len(b):
            nxt, i = struct.unpack_from(">IH", b, o)
            if nxt == 0xFFFFFFFF: break
            yield i, o + 8, o + nxt
            o += nxt
    macros = {i: [struct.unpack_from(">II", b, p) for p in range(s, e, 8)] for i, s, e in walk(mo)}
    keymaps = {i: [struct.unpack_from(">HbBh", b, s + 8 * k) for k in range(128)] for i, s, e in walk(ko)}
    layers = {}
    for i, s, e in walk(lo):
        n = struct.unpack_from(">I", b, s)[0]
        layers[i] = [struct.unpack_from(">HBBbBhB", b, s + 4 + 12 * k) for k in range(n)]
    curves = {i: b[s:e] for i, s, e in walk(co)}
    return {"macros": macros, "keymaps": keymaps, "layers": layers, "curves": curves}


MACRO_JUMPS = (0x02, 0x03, 0x13)  # conditional jumps to another macro (id in para0 >> 16): synthmacros.c


def object_samples(pool, obj, seen=None):
    """Sample ids an object (macro / keymap / layer id) can start: start-sample steps of the macros it reaches through
    keymaps, layers and the macro jump commands. A static over-approximation (every branch is followed)."""
    seen = set() if seen is None else seen
    if obj in seen or obj == 0xFFFF: return set()
    seen.add(obj); out = set()
    kind = obj & 0xC000
    if kind == 0:
        for p0, p1 in pool["macros"].get(obj, []):
            op = p0 & 0x7F
            if op == 0x10: out.add((p0 >> 8) & 0xFFFF)
            elif op in MACRO_JUMPS: out |= object_samples(pool, (p0 >> 16) & 0xFFFF, seen)
    elif kind == 0x4000:
        for k in pool["keymaps"].get(obj, []):
            out |= object_samples(pool, k[0], seen)
    elif kind == 0x8000:
        for l in pool["layers"].get(obj, []):
            out |= object_samples(pool, l[0], seen)
    return out


# Macro opcodes (byte 3 & 0x7F of a step) -> handler, from macHandleActive's switch (src/musyx/runtime/synthmacros.c).
# Names in lower case describe opcodes the switch handles inline.
MACRO_OPS = {
    0x00: "mcmdEndOfMacro", 0x01: "mcmdEndOfMacro (stop)", 0x02: "split key (jump if note >= arg)",
    0x03: "split velocity (jump if velocity >= arg)", 0x04: "mcmdWait", 0x05: "mcmdLoop", 0x06: "mcmdGoto",
    0x07: "mcmdWait (milliseconds)", 0x08: "mcmdPlayMacro", 0x09: "mcmdSendKeyOff",
    0x0A: "split modulation (jump if mod wheel >= arg)", 0x0B: "mcmdSetPianoPanning", 0x0C: "mcmdSetADSR",
    0x0D: "mcmdScaleVolume", 0x0E: "mcmdSetPanning", 0x0F: "mcmdEnvelope", 0x10: "mcmdStartSample",
    0x11: "hwBreak (stop sample)", 0x12: "key off state", 0x13: "split random (jump if random >= arg)",
    0x14: "mcmdFadeIn", 0x15: "mcmdSetSurroundPanning", 0x16: "mcmdSetADSRFromCtrl", 0x17: "mcmdRandomKey",
    0x18: "mcmdAddKey", 0x19: "mcmdSetKey", 0x1A: "mcmdLastKey", 0x1B: "mcmdPortamento", 0x1C: "mcmdVibrato",
    0x1D: "mcmdPitchSweep 1", 0x1E: "mcmdPitchSweep 2", 0x1F: "mcmdSetPitch", 0x20: "mcmdSetPitchADSR",
    0x21: "mcmdScaleVolumeDLS", 0x22: "vibrato mod-wheel scale", 0x23: "tremolo setup", 0x24: "mcmdReturn",
    0x25: "mcmdGosub", 0x28: "mcmdTrapEvent", 0x29: "mcmdUntrapEvent", 0x2A: "mcmdSendMessage", 0x2B: "varSet32",
    0x2C: "mcmdGetVID", 0x30: "mcmdAddAgeCounter", 0x31: "mcmdSetAgeCounter", 0x32: "set global variable",
    0x33: "pitch-bend key range", 0x34: "reverb volume scale / offset", 0x35: "voice flag 0x10000",
    0x36: "set priority", 0x37: "mcmdAddPriority", 0x38: "age speed", 0x39: "mcmdSetAgeCounterByVolume",
    0x40: "mcmdVolumeSelect", 0x41: "mcmdPanningSelect", 0x42: "mcmdPitchWheelSelect", 0x43: "mcmdModWheelSelect",
    0x44: "mcmdPedalSelect", 0x45: "mcmdPortamentoSelect", 0x46: "mcmdReverbSelect", 0x47: "mcmdSurroundPanningSelect",
    0x48: "mcmdDopplerSelect", 0x49: "mcmdTremoloSelect", 0x4A: "mcmdPreAuxASelect", 0x4B: "mcmdPreAuxBSelect",
    0x4C: "mcmdPostAuxBSelect", 0x4D: "mcmdAuxAFXSelect", 0x4E: "mcmdAuxBFXSelect", 0x50: "mcmdSetupLFO",
    0x58: "volume table / ITD mode", 0x59: "mcmdSetKeyGroup", 0x5A: "voice parameters (fn_801629A4 / fn_801629D0)",
    0x60: "variable add", 0x61: "variable subtract", 0x62: "variable multiply", 0x63: "variable divide",
    0x64: "variable calculation 4", 0x65: "varSet32", 0x70: "if variable equal", 0x71: "if variable less",
}


def bank_listing(proj, pool, sdir=None, name=""):
    """Readable text for one MusyX bank: groups (programs -> objects -> samples, MIDI setups, FX counts), keymaps,
    layers and every macro step with its opcode name and raw words."""
    groups = parse_project(proj); p = parse_pool(pool)
    L = [f"# MusyX bank {name}", "", f"{len(groups)} group(s); pool: {len(p['macros'])} macros, {len(p['keymaps'])} keymaps, "
         f"{len(p['layers'])} layers, {len(p['curves'])} curves", ""]
    kinds = {0: "macro", 0x4000: "keymap", 0x8000: "layer"}
    for g in groups:
        L.append(f"## Group {g['id']} ({'songs' if g['type'] == 0 else 'sound effects'})")
        L.append(f"uses {len(g['macros'])} macros, {len(g['samples'])} samples, {len(g['keymaps'])} keymaps, {len(g['layers'])} layers")
        if g["type"] == 0:
            for title, pages in (("Programs", g["normal_pages"]), ("Drum programs (channel 10)", g["drum_pages"])):
                L.append(f"### {title}")
                for prog, (obj, prio, mv) in sorted(pages.items()):
                    smp = sorted(object_samples(p, obj))
                    L.append(f"- {prog}: {kinds.get(obj & 0xC000, '?')} {obj:#06x}, priority {prio}, max voices {mv}, samples "
                             + (", ".join(f"{x:04X}" for x in smp) or "none"))
            L.append(f"### MIDI setups ({len(g['midi_setups'])}): song id: channel = program / volume / pan / reverb / chorus")
            for sid, chans in sorted(g["midi_setups"].items()):
                L.append(f"- {sid}: " + "; ".join(f"{c}={a}/{b}/{c2}/{d}/{e}" for c, (a, b, c2, d, e) in enumerate(chans)))
        else:
            L.append(f"sound-effect table: {g.get('fx_count', 0)} effects")
        L.append("")
    L.append("## Layers")
    for lid, items in sorted(p["layers"].items()):
        L.append(f"- {lid:#06x}: " + "; ".join(f"{o:#06x} keys {lo}-{hi} transpose {tr} volume {vol} pan {pan}" for o, lo, hi, tr, vol, pr, pan in items))
    L.append(""); L.append("## Keymaps")
    for kid, keys in sorted(p["keymaps"].items()):
        used = [(k, o, tr) for k, (o, tr, pan, pr) in enumerate(keys) if o != 0xFFFF]
        L.append(f"- {kid:#06x}: " + "; ".join(f"key {k} -> {o:#06x} ({tr:+d})" for k, o, tr in used))
    L.append(""); L.append("## Macros (step: opcode name, raw words)")
    for mid, steps in sorted(p["macros"].items()):
        L.append(f"### macro {mid:#06x}")
        for i, (p0, p1) in enumerate(steps):
            op = p0 & 0x7F
            extra = f" sample {(p0 >> 8) & 0xFFFF:04X}" if op == 0x10 else f" -> macro {(p0 >> 16) & 0xFFFF:#06x} step {p1 & 0xFFFF}" if op in (0x02, 0x03, 0x0A, 0x13) else ""
            L.append(f"  {i:3d}  {MACRO_OPS.get(op, f'op {op:#04x}')}{extra}  [{p0:08X} {p1:08X}]")
            if op in (0x00, 0x01): break
    return "\n".join(L)
