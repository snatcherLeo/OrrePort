"""HSD FObj keyframe tracks: the compressed animation curves inside every HSD animation (AObj -> FObj chain).
Format and semantics from Melee's sysdolphin `fobj.c` (same library; Colosseum's HSD_FObjLoadDesc and the
FObj interpreter are the same code). docs/engine/animation-tracks.md.

A track's data (`ad`, `length` bytes) is a byte stream of key groups:
  group header: low 4 bits = interpolation op, then a variable-length key count (pack info)
  per key: the op's values, then (except for SLP) a variable-length wait in frames before the next key.
Ops: 1 CON (constant / step), 2 LIN (linear), 3 SPL0 (spline, slope 0), 4 SPL (spline with slope),
5 SLP (slope only, changes the next key's incoming slope, no wait), 6 KEY (key value, held).
Values use the track's `frac_value` / `frac_slope` byte: high 3 bits = storage type (float, s16, u16, s8, u8),
low 5 bits = fixed-point shift (value = stored / 2**shift). Multi-byte values are little-endian."""
import struct

OPS = {1: "CON", 2: "LIN", 3: "SPL0", 4: "SPL", 5: "SLP", 6: "KEY"}
FRAC_TYPES = {0x00: "float", 0x20: "s16", 0x40: "u16", 0x60: "s8", 0x80: "u8"}


def frac_name(frac):
    t = FRAC_TYPES.get(frac & 0xE0, "?")
    return t if t == "float" else f"{t}/2^{frac & 0x1F}"


def _value(d, p, frac):
    """parseFloat: -> (value, new position)."""
    if frac == 0:
        return struct.unpack("<f", d[p:p + 4])[0], p + 4
    denom = 1 << (frac & 0x1F)
    kind = frac & 0xE0
    if kind == 0x60:
        return struct.unpack("b", d[p:p + 1])[0] / denom, p + 1
    if kind == 0x80:
        return d[p] / denom, p + 1
    if kind == 0x20:
        return struct.unpack("<h", d[p:p + 2])[0] / denom, p + 2
    if kind == 0x40:
        return struct.unpack("<H", d[p:p + 2])[0] / denom, p + 2
    return 0.0, p  # unknown storage type: the game returns 0 and does not advance


def _pack_info(d, p):
    """parsePackInfo: key count of a group (3 bits in the header byte, then 7 bits per extra byte)."""
    b = d[p]
    p += 1
    n = ((b >> 4) & 7) + 1
    shift = 3
    while b & 0x80:
        b = d[p]
        p += 1
        n += (b & 0x7F) << shift
        shift += 7
    return n, p


def _wait(d, p):
    """parseWait: 7 bits per byte, low first."""
    w = shift = 0
    while True:
        b = d[p]
        p += 1
        w |= (b & 0x7F) << shift
        shift += 7
        if not b & 0x80:
            return w, p


def decode(data, start_frame=0.0, frac_value=0, frac_slope=0):
    """Decode one track's byte stream -> list of keys
    {frame, op, value (None for SLP), slope (SPL / SLP only), wait}.
    `frame` is the animation frame where the key is reached: the track's clock starts at `start_frame` when the
    animation starts (HSD_FObjReqAnim: time = startframe + frame), so a key reached after waits totalling W
    falls on animation frame W - start_frame."""
    keys = []
    p, frame, left, op = 0, 0.0 - start_frame, 0, 0
    while p < len(data):
        if left == 0:
            op = data[p] & 0xF
            left, p = _pack_info(data, p)
        left -= 1
        key = dict(frame=frame, op=OPS.get(op, f"op{op}"), value=None, slope=None, wait=0)
        if op in (1, 2, 3, 6):
            key["value"], p = _value(data, p, frac_value)
        elif op == 4:
            key["value"], p = _value(data, p, frac_value)
            key["slope"], p = _value(data, p, frac_slope)
        elif op == 5:
            key["slope"], p = _value(data, p, frac_slope)
            keys.append(key)
            continue  # SLP: no wait, the next key follows at the same frame
        else:
            break  # op 0 / unknown: the game stops interpreting the track
        if p >= len(data):
            keys.append(key)
            break
        key["wait"], p = _wait(data, p)
        frame += key["wait"]
        keys.append(key)
    return keys


def text(keys, limit=None):
    """Compact one-line-per-key listing."""
    out = []
    for k in keys[:limit]:
        v = "" if k["value"] is None else f" {k['value']:.4g}"
        s = "" if k["slope"] is None else f" slope {k['slope']:.4g}"
        out.append(f"  frame {k['frame'] + 0.0:g}: {k['op']}{v}{s}")
    if limit and len(keys) > limit:
        out.append(f"  ... {len(keys) - limit} more keys")
    return "\n".join(out)
