"""Colosseum saved game (.gci from Dolphin, or the raw card file): decrypt the save slots and read the story flags.
Read-only; never writes a save. Layout and cipher: docs/engine/save-data.md "Card file layout, checksum and encryption"
(field_range_801CB180.c); flag storage: docs/engine/extractor.md (flags_decoded.csv) and docs/engine/flags.md.

Card file: 0x6000-byte header area, then 3 slots of 0x1E000 bytes. Slot: +0x04 u32 save counter (highest valid slot
wins), +0x08 the save block (0x1DFD0 bytes), +0x18 .. +0x1DFD8 encrypted, +0x1DFEC SHA-1 digest of the plain slot
0 .. 0x1DFD8. Decrypt: key = NOT digest; per 20-byte block: next = SHA1(encrypted block), block ^= key, key = next.

Story flags: classes 1 / 2 / 3 live in save sections 4 / 5 / 6 (block +0x1BDBC / +0x1BDDC / +0x1BE5C); each flag's
bit position is its class's running sum of widths in id order (GSflagInitBitPos), bits counted from the low end of
big-endian u32 words, values crossing a word take their upper bits from the next word (_flagGet)."""
import hashlib
import struct

GCI_HEADER = 0x40
SLOTS_AT, SLOT_SIZE, NSLOTS = 0x6000, 0x1E000, 3
BLOCK_AT, BLOCK_SIZE = 0x08, 0x1DFD0
ENC_START, ENC_END, DIGEST_AT = 0x18, 0x1DFD8, 0x1DFEC
FLAG_SECTIONS = {1: (0x1BDBC, 8), 2: (0x1BDDC, 0x20), 3: (0x1BE5C, 0x180)}  # class -> (block offset, words)


def card_file(data):
    """The card file bytes: strips a Dolphin .gci header (0x40 bytes starting with the game code) when present."""
    if len(data) == GCI_HEADER + SLOTS_AT + NSLOTS * SLOT_SIZE: return data[GCI_HEADER:]
    if len(data) == SLOTS_AT + NSLOTS * SLOT_SIZE: return data
    raise ValueError(f"unexpected save size {len(data):#x}")


def decrypt_slot(slot):
    """(plain slot bytes, valid) for one encrypted 0x1E000-byte slot."""
    s = bytearray(slot)
    s[0x0C:0x10] = bytes(4)  # the game zeroes word 3 before checking (fn_801CF7E4)
    digest = bytes(s[DIGEST_AT:DIGEST_AT + 20])
    key = bytes(b ^ 0xFF for b in digest)
    for o in range(ENC_START, ENC_END, 20):
        blk = bytes(s[o:o + 20])
        nxt = hashlib.sha1(blk).digest()
        s[o:o + len(blk)] = bytes(a ^ b for a, b in zip(blk, key))
        key = nxt
    return bytes(s), hashlib.sha1(bytes(s[:ENC_END])).digest() == digest


def slots(data):
    """[dict(index, counter, valid, block)] for the three slots of a save file."""
    card = card_file(data); out = []
    for i in range(NSLOTS):
        plain, ok = decrypt_slot(card[SLOTS_AT + i * SLOT_SIZE:SLOTS_AT + (i + 1) * SLOT_SIZE])
        out.append(dict(index=i, counter=struct.unpack(">I", plain[4:8])[0], valid=ok,
                        block=plain[BLOCK_AT:BLOCK_AT + BLOCK_SIZE]))
    return out


def current_block(data):
    """The save block the game would load: the valid slot with the highest save counter (None if none is valid)."""
    good = [s for s in slots(data) if s["valid"]]
    return max(good, key=lambda s: s["counter"])["block"] if good else None


def flag_layout(definitions):
    """[(class, width, bit position)] per flag id from the 8-byte definitions (common_rel lbl_80478F9C)."""
    nxt = {1: 0, 2: 0, 3: 0}; out = []
    for e in definitions:
        cls, w = e[0] >> 6, e[0] & 0x3F
        w = 32 if w > 32 else (1 if w == 0 else w)
        out.append((cls, w, nxt.get(cls)))
        if cls in nxt: nxt[cls] += w
    return out


def flag_values(block, definitions):
    """{flag id: value} for every flag of class 1-3 in a save block (class 0 flags have no storage: left out)."""
    words = {c: struct.unpack(f">{n}I", block[o:o + 4 * n]) for c, (o, n) in FLAG_SECTIONS.items()}
    out = {}
    for i, (cls, w, pos) in enumerate(flag_layout(definitions)):
        if cls not in words: continue
        buf, wi, bp = words[cls], pos >> 5, pos & 31
        v = buf[wi] >> bp
        if w > 1 and bp and wi + 1 < len(buf): v |= buf[wi + 1] << (32 - bp)
        out[i] = v & ((1 << w) - 1)
    return out


def player_name(block):
    """The player's name from section 2 (block +0x70, 0x2C bytes of UTF-16BE, zero-terminated)."""
    raw = block[0x70:0x70 + 0x2C]
    return raw.decode("utf-16-be", "replace").split("\0")[0]


def story_progress(block, definitions, chain):
    """Story-progress meter inputs from a save block: flags set (of the stored ones), and the furthest story-chain
    position whose flag is set (chain = flag ids in story order, from the flag config's linked list)."""
    vals = flag_values(block, definitions)
    pos = {f: k for k, f in enumerate(chain)}
    setf = [f for f, v in vals.items() if v]
    reach = max((pos[f] for f in setf if f in pos), default=-1)
    return dict(stored=len(vals), set=len(setf), furthest_chain_position=reach, chain_length=len(chain),
                furthest_flag=chain[reach] if reach >= 0 else None)


if __name__ == "__main__":
    # python -I tools/local/colo/savefile.py <save .gci or card file> <disc image>   (read-only)
    import os, sys
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from colo.disc import Disc
    from colo.tables import common_rel, table_rows
    from colo import world
    data = open(sys.argv[1], "rb").read()
    rel = common_rel(Disc(sys.argv[2]))
    defs = table_rows(rel, 0x111e04, 0x14a6c0, 8)
    cfg = world.common_table(rel, 0x80478F98); cur = struct.unpack(">h", rel[cfg + 4:cfg + 6])[0]; chain = []
    while 0 <= cur < len(defs) and len(chain) <= len(defs): chain.append(cur); cur = struct.unpack(">h", defs[cur][6:8])[0]
    for s in slots(data): print(f"slot {s['index']}: save counter {s['counter']}, {'valid' if s['valid'] else 'INVALID (digest mismatch)'}")
    block = current_block(data)
    if block is None: print("no valid slot"); sys.exit(1)
    p = story_progress(block, defs, chain)
    print(f"player: {player_name(block)}")
    print(f"story flags set: {p['set']} of {p['stored']} stored ({len(defs)} defined)")
    print(f"furthest story-chain flag set: position {p['furthest_chain_position']} of {p['chain_length']} "
          f"({100 * (p['furthest_chain_position'] + 1) / p['chain_length']:.1f}%), flag {p['furthest_flag']}")
