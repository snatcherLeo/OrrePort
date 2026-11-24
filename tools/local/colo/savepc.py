"""Move a Colosseum save between Dolphin / the GameCube and the PC port (2026-10-18).

    python -I tools/local/colo/savepc.py check <save.gci>                      which format each slot is in
    python -I tools/local/colo/savepc.py to-pc <dolphin.gci> <out.gci> [--report]
    python -I tools/local/colo/savepc.py to-gc <pc.gci> <out.gci> [--report]

Why a converter: the save block is one 0x1DFD0-byte array the game copies by value, and the PC's native code keeps
every multi-byte field in its own byte order (little-endian), so the same save is a different file on each side.
Two differences (both verified from the code, src/game/memcard.c):
1. The fields. Each 2- and 4-byte field is swapped. The field list (offset, width; no game data) is
   tools/local/port/save_fields.txt, made from the save-trace build (port/src/pc_save_trace.c: every access the
   game's code makes to the save block, with its width). Bytes no trace has seen are left as they are; --report
   lists the non-zero ones, which are the places a later trace still has to cover.
2. The seal. The PC runs the same cipher, but its SHA-1 loads each 64-byte block's words natively (SHA1_BLK0 has no
   byte swap), and the slot's counter and checksum words are native too. So "PC SHA-1" is SHA-1 with little-endian
   word loads; everything else (key = NOT digest, the 20-byte XOR chain, word 3 making the first eight words sum to
   zero) is the same.

Card file: the 0x6000-byte header area (banner, icons, comment) is copied unchanged; each valid slot is converted;
invalid slots are copied unchanged. Slot layout and cipher: savefile.py and docs/engine/save-data.md.
"""
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import savefile  # noqa: E402

FIELDS = Path(__file__).resolve().parents[1] / "port" / "save_fields.txt"
SLOTS_AT, SLOT_SIZE, NSLOTS = savefile.SLOTS_AT, savefile.SLOT_SIZE, savefile.NSLOTS
BLOCK_AT, BLOCK_SIZE = savefile.BLOCK_AT, savefile.BLOCK_SIZE
ENC_START, ENC_END, DIGEST_AT = savefile.ENC_START, savefile.ENC_END, savefile.DIGEST_AT
ICON_SUM_AT = 0xE90 * 4  # MemcardIconFile.checksum: after title 0x20, comment 0x20, banner 0x1800, icon 0x2200


def _rol(x, n):
    return ((x << n) | (x >> (32 - n))) & 0xFFFFFFFF


def sha1_pc(data):
    """SHA-1 as the PC build computes it: the standard padding and digest bytes, but each block's 16 words loaded
    little-endian (the GameCube's native big-endian load, run on a little-endian machine)."""
    h = [0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0]
    n = len(data) * 8
    msg = bytes(data) + b"\x80" + b"\x00" * ((55 - len(data)) % 64) + struct.pack(">Q", n)
    for i in range(0, len(msg), 64):
        w = list(struct.unpack("<16I", msg[i:i + 64]))
        for t in range(16, 80):
            w.append(_rol(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1))
        a, b, c, d, e = h
        for t in range(80):
            if t < 20:
                f, k = (b & c) | (~b & d), 0x5A827999
            elif t < 40:
                f, k = b ^ c ^ d, 0x6ED9EBA1
            elif t < 60:
                f, k = (b & c) | (b & d) | (c & d), 0x8F1BBCDC
            else:
                f, k = b ^ c ^ d, 0xCA62C1D6
            a, b, c, d, e = (_rol(a, 5) + (f & 0xFFFFFFFF) + e + k + w[t]) & 0xFFFFFFFF, a, _rol(b, 30), c, d
        h = [(x + y) & 0xFFFFFFFF for x, y in zip(h, (a, b, c, d, e))]
    return b"".join(struct.pack(">I", x) for x in h)


SIDES = {"gc": (hashlib.sha1, ">"), "pc": (lambda d: _Digest(sha1_pc(d)), "<")}


class _Digest:
    def __init__(self, d):
        self._d = d

    def digest(self):
        return self._d


def decrypt(slot, side):
    """(plain slot, valid) with that side's seal"""
    sha, _ = SIDES[side]
    s = bytearray(slot)
    s[0x0C:0x10] = bytes(4)
    digest = bytes(s[DIGEST_AT:DIGEST_AT + 20])
    key = bytes(b ^ 0xFF for b in digest)
    for o in range(ENC_START, ENC_END, 20):
        blk = bytes(s[o:o + 20])
        nxt = sha(blk).digest()
        s[o:o + 20] = bytes(a ^ b for a, b in zip(blk, key))
        key = nxt
    return bytes(s), sha(bytes(s[:ENC_END])).digest() == digest


def encrypt(plain, side):
    sha, order = SIDES[side]
    s = bytearray(plain)
    s[0x0C:0x10] = bytes(4)
    digest = sha(bytes(s[:ENC_END])).digest()
    s[DIGEST_AT:DIGEST_AT + 20] = digest
    key = bytes(b ^ 0xFF for b in digest)
    for o in range(ENC_START, ENC_END, 20):
        blk = bytes(a ^ b for a, b in zip(s[o:o + 20], key))
        s[o:o + 20] = blk
        key = sha(blk).digest()
    words = list(struct.unpack(order + "8I", s[:32]))
    words[3] = (-(sum(words) - words[3])) & 0xFFFFFFFF
    s[0x0C:0x10] = struct.pack(order + "I", words[3])
    return bytes(s)


KEEP = Path(__file__).resolve().parents[1] / "port" / "save_keep.txt"
LAYOUT = Path(__file__).resolve().parents[1] / "port" / "save_layout.txt"


def load_keep(path=KEEP):
    """Byte ranges never swapped (save_keep.txt, hand-kept with evidence): text the PC keeps in GameCube byte order.
    With path=LAYOUT: the fields known from the code (save_layout.txt, same syntax), as (offset, width)"""
    out = []
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            line = line.split("#")[0].strip()
            if not line:
                continue
            w = line.split()
            if w[0] == "mirror":  # mirror SRC SIZE DEST ... : applied by load_fields (traced fields), not a range here
                continue
            if w[0] == "records":  # records BASE STRIDE COUNT OFF:SIZE [OFF:SIZE ...]
                base, stride, count = int(w[1], 0), int(w[2], 0), int(w[3], 0)
                parts = [tuple(int(x, 0) for x in p.split(":")) for p in w[4:]]
                out += [(base + k * stride + o, s) for k in range(count) for o, s in parts]
            else:  # OFFSET SIZE
                out.append((int(w[0], 0), int(w[1], 0)))
    return out


def load_fields():
    out = {}
    for line in FIELDS.read_text(encoding="utf-8").splitlines():
        if line and not line.startswith("#"):
            off, width = line.split()[:2]
            out[int(off, 16)] = int(width)
    for off, width in load_keep(LAYOUT):  # fields known from the code, whether or not a trace saw them
        if width > 1:
            out[off] = max(width, out.get(off, 0))
    for lo, size in load_keep():
        for o in [o for o in out if lo <= o < lo + size]:
            del out[o]
    # save_layout.txt "mirror SRC SIZE DEST ...": a struct the game copies whole (memcpy / heroBiosCopy) into other
    # places of the block has the same fields there; the fields found in [SRC, SRC + SIZE) (after the keep list, so
    # text stays as bytes in the copies too) are repeated at each DEST (2026-10-30; section 14, port-gaps 42)
    if LAYOUT.exists():
        for line in LAYOUT.read_text(encoding="utf-8").splitlines():
            w = line.split("#")[0].split()
            if w and w[0] == "mirror":
                src, size = int(w[1], 0), int(w[2], 0)
                part = {o - src: wd for o, wd in out.items() if src <= o < src + size}
                for dest in (int(x, 0) for x in w[3:]):
                    for r, wd in part.items():
                        out[dest + r] = max(wd, out.get(dest + r, 0))
    return out


def swap_fields(block, fields):
    b = bytearray(block)
    for off, w in fields.items():
        if w > 1 and off + w <= len(b):
            b[off:off + w] = b[off:off + w][::-1]
    return bytes(b)


def slot_counter(plain, side):
    return struct.unpack((">" if side == "gc" else "<") + "I", plain[4:8])[0]


def convert(data, src, dst, report=False):
    gci_head = data[:savefile.GCI_HEADER] if len(data) == savefile.GCI_HEADER + SLOTS_AT + NSLOTS * SLOT_SIZE else b""
    card = bytearray(savefile.card_file(data))
    fields = load_fields()
    covered = bytearray(BLOCK_SIZE)
    for off, w in fields.items():
        covered[off:off + w] = b"\x01" * w
    done = 0
    for i in range(NSLOTS):
        at = SLOTS_AT + i * SLOT_SIZE
        plain, ok = decrypt(card[at:at + SLOT_SIZE], src)
        if not ok:
            # an empty slot still has a header the game checks at every card read (memcard.c fn_801CF9C8): bytes 0
            # and 1 = 1, its eight first words summing to zero natively, word 1 the save counter. A GameCube sum fails
            # on the PC and the whole card is refused (error 0x11, "The game data can't be saved"; verified
            # 2026-10-18), so the counter and the checksum word are redone in dst's order
            order_s, order_d = (">", "<") if dst == "pc" else ("<", ">")
            head = bytearray(card[at:at + 32])
            counter = struct.unpack(order_s + "I", head[4:8])[0]
            head[4:8] = struct.pack(order_d + "I", counter)
            head[0x0C:0x10] = bytes(4)
            words = struct.unpack(order_d + "8I", bytes(head))
            head[0x0C:0x10] = struct.pack(order_d + "I", (-sum(words)) & 0xFFFFFFFF)
            card[at:at + 32] = head
            if report:
                print(f"slot {i}: empty (not valid {src.upper()}); header re-sealed for {dst.upper()}")
            continue
        p = bytearray(plain)
        p[4:8] = p[4:8][::-1]  # the save counter, a native word outside the block's own fields
        block = p[BLOCK_AT:BLOCK_AT + BLOCK_SIZE]
        p[BLOCK_AT:BLOCK_AT + BLOCK_SIZE] = swap_fields(block, fields)
        card[at:at + SLOT_SIZE] = encrypt(bytes(p), dst)
        done += 1
        if report:
            loose = [o for o in range(BLOCK_SIZE) if not covered[o] and block[o] != 0]
            print(f"slot {i}: counter {slot_counter(plain, src)}; {len(fields)} fields swapped; "
                  f"{len(loose)} non-zero bytes outside every traced field")
            runs, start = [], None
            for o in loose + [None]:
                if start is not None and (o is None or o != prev + 1):
                    runs.append((start, prev))
                    start = None
                if o is not None and start is None:
                    start = o
                prev = o
            for a, b in runs[:25]:
                print(f"    block +0x{a:05X}..+0x{b:05X} ({b - a + 1} bytes)")
            if len(runs) > 25:
                print(f"    ... {len(runs) - 25} more ranges")
    if done == 0:
        raise SystemExit(f"no slot passes the {src.upper()} check: is this really a {src.upper()} save?")
    # the icon file at the start of the card (memcard.c MemcardIconFile: title, comment, banner, icon; all bytes) ends
    # in a checksum: the sum of its first 0xE90 native words. A GameCube sum fails on the PC and the game asks to
    # format the card ("The game data can't be saved", verified 2026-10-18), so it is recomputed in dst's order.
    order = "<" if dst == "pc" else ">"
    words = struct.unpack(order + f"{ICON_SUM_AT // 4}I", bytes(card[:ICON_SUM_AT]))
    card[ICON_SUM_AT:ICON_SUM_AT + 4] = struct.pack(order + "I", sum(words) & 0xFFFFFFFF)
    if report:
        print("icon file checksum recomputed for " + dst.upper())
    return gci_head + bytes(card), done


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    cmd, path = sys.argv[1], sys.argv[2]
    if cmd == "fields":  # fields [--fresh] TRACE... : (re)build save_fields.txt from save-trace files
        fresh = "--fresh" in sys.argv  # start over instead of merging with the table's current rows
        fields = load_fields() if FIELDS.exists() and not fresh else {}
        before = len(fields)
        write_only = 0
        for t in [a for a in sys.argv[2:] if a != "--fresh"]:
            for line in Path(t).read_text(encoding="utf-8").splitlines():
                if line.startswith("#") or not line.strip():
                    continue
                off, *parts = line.split()
                reads = [p[2:] for p in parts if p.startswith("r:")]
                if not any(p.startswith(("r:", "w:")) for p in parts):
                    reads = parts  # the first traces (2026-10-18) did not keep reads and writes apart
                if not reads:
                    write_only += 1  # only written: a new game's clearing says nothing about the layout
                    continue
                w = max(int(x) for x in reads[0].split(","))
                if w > 1:  # width 1 alone needs no swap (and byte copies of whole records mark every byte 1)
                    fields[int(off, 16)] = max(w, fields.get(int(off, 16), 0))
        if write_only:
            print(f"{write_only} offsets only written, never read: left out")
        bad = [o for o, w in fields.items() if any(o + k in fields for k in range(1, w))]
        FIELDS.write_text("# Save block fields wider than a byte (savepc.py fields, from save-trace runs; never edit by "
                          "hand).\n# offset in the save block, width in bytes. Offsets and widths only, no game data.\n"
                          + "".join(f"0x{o:05X} {fields[o]}\n" for o in sorted(fields)), encoding="utf-8")
        print(f"{FIELDS}: {len(fields)} fields ({len(fields) - before} new); overlapping: {len(bad)}")
        return
    if cmd == "ctable":  # ctable OUT.c : the port's built-in field table (port/src/pc_save_fields.c)
        fields = load_fields()
        offs = sorted(o for o, w in fields.items() if w in (2, 4))
        body = [
            "/* GENERATED by tools/local/colo/savepc.py ctable from tools/local/port/save_fields.txt and save_keep.txt;",
            " * never edit by hand (regenerate after new save-trace runs). Offsets and widths only, no game data.",
            " *",
            " * The save block's multi-byte fields: the card file keeps them in GameCube byte order, the game's code on the",
            " * PC reads them natively, so memcard.c swaps them after loading and before saving (2026-10-18: console and",
            " * Dolphin saves load on the PC, and the PC's saves load in Dolphin). Names stay as bytes (save_keep.txt). */",
            "#include <stdint.h>",
            "",
            f"static const uint32_t s_offsets[{len(offs)}] = {{",
        ]
        for i in range(0, len(offs), 10):
            body.append("    " + ", ".join(f"0x{o:05X}" for o in offs[i:i + 10]) + ",")
        body += ["};", f"static const uint8_t s_widths[{len(offs)}] = {{"]
        for i in range(0, len(offs), 30):
            body.append("    " + ", ".join(str(fields[o]) for o in offs[i:i + 30]) + ",")
        body += ["};", "",
                 "/* Swap every field of a 0x1DFD0-byte save block between GameCube and PC byte order (its own inverse) */",
                 "void pc_save_swap_fields(uint8_t* block) {",
                 f"    for (int i = 0; i < {len(offs)}; i++) {{",
                 "        uint8_t* p = block + s_offsets[i];",
                 "        uint8_t t;",
                 "        if (s_widths[i] == 2) {",
                 "            t = p[0]; p[0] = p[1]; p[1] = t;",
                 "        } else {",
                 "            t = p[0]; p[0] = p[3]; p[3] = t;",
                 "            t = p[1]; p[1] = p[2]; p[2] = t;",
                 "        }",
                 "    }",
                 "}", ""]
        Path(sys.argv[2]).write_text("\n".join(body), encoding="utf-8")
        print(f"{sys.argv[2]}: {len(offs)} fields")
        return
    data = Path(path).read_bytes()
    if cmd == "check":
        card = savefile.card_file(data)
        for i in range(NSLOTS):
            slot = card[SLOTS_AT + i * SLOT_SIZE:SLOTS_AT + (i + 1) * SLOT_SIZE]
            res = []
            for side in ("gc", "pc"):
                plain, ok = decrypt(slot, side)
                if ok:
                    res.append(f"{side.upper()} format, save counter {slot_counter(plain, side)}")
            print(f"slot {i}: " + ("; ".join(res) if res else "not valid in either format (empty?)"))
        return
    if cmd not in ("to-pc", "to-gc") or len(sys.argv) < 4:
        raise SystemExit(__doc__)
    src, dst = ("gc", "pc") if cmd == "to-pc" else ("pc", "gc")
    out, n = convert(data, src, dst, "--report" in sys.argv)
    Path(sys.argv[3]).write_bytes(out)
    print(f"{sys.argv[3]}: {n} slot(s) converted {src.upper()} -> {dst.upper()}")


if __name__ == "__main__":
    main()
