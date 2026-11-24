"""PKX battle models: header, HSD model, ambient particle bank, animation slots, shiny colour block
(docs/engine/models-animation.md, textures.md). Read-only reader (no file output): a port candidate,
see docs/tools.md "Readers"."""
import struct


def parse_pkx(data):
    """Split a Colosseum PKX into its parts. Returns a dict:
    model (bytes, HSD .dat), gpt1 (bytes or None), slots (list of dicts), colour (dict or None), header."""
    model_size, gpt1_size, nslots, kind, mode, flags, ref0, ref1, ref2 = struct.unpack(">IIIiIIiii", data[:0x24])
    o = 0x40
    model = data[o:o + model_size]; o += (model_size + 0x1F) & ~0x1F
    gpt1 = None
    if gpt1_size:
        gpt1 = data[o:o + gpt1_size]; o += (gpt1_size + 0x1F) & ~0x1F
    slots = []
    for k in range(nslots):
        r = data[o + k * 0xD0:o + (k + 1) * 0xD0]
        npts, nanims, f08 = struct.unpack(">III", r[:12])
        slots.append({
            "time_points": list(struct.unpack(f">{min(npts, 16)}i", r[0x0C:0x0C + 4 * min(npts, 16)])),
            "body_map": list(struct.unpack(">16i", r[0x4C:0x8C])),
            "animations": [struct.unpack(">II", r[0x8C + 8 * j:0x94 + 8 * j]) for j in range(min(nanims, 8))],
        })
    o += nslots * 0xD0
    colour = None
    if len(data) - o >= 20:
        c = data[len(data) - 20:]
        colour = {"route": list(struct.unpack(">4I", c[:16])), "modulation_abgr": list(c[16:20])}
    header = {"model_size": model_size, "gpt1_size": gpt1_size, "slot_count": nslots, "kind": kind, "mode": mode,
              "flags": flags, "sub_anim_refs": [ref0, ref1, ref2]}
    return {"model": model, "gpt1": gpt1, "slots": slots, "colour": colour, "header": header}
