"""Write a blank, formatted GameCube memory card image (.raw) for Dolphin (2026-11-24).

    python -I tools/local/testing/blank_gc_card.py <out.raw> [--blocks 251]

Why: XD's Dolphin sittings record on a fixed card image (playrec.py xd_card_use: with Dolphin's card folder, the card is
rebuilt every run and replays that start from a save state parted from the recording at the first save). Dolphin
makes a card image only when a game first writes to it, which is too late (the movie starts before that), so the image
is made here once, before the first sitting. Fixed serial and format time: the same file every time, so the recording
and every replay see one card.

The layout, as Dolphin's own format writes it (Source/Core/Core/HW/GCMemcard/GCMemcard.h; verified only by Dolphin
loading the result, see the tools catalog):
  block 0  header: serial[12], format time u64, SRAM bias u32, SRAM language u32, unknown u32, device id u16,
           size in megabits u16, encoding u16, 0xFF fill to 0x1FC, checksum u16, inverse checksum u16, 0xFF to 0x2000
  block 1, 2  directory and its backup: 127 empty entries (0x40 bytes of 0xFF each), 0xFF fill, update counter u16,
           checksum u16, inverse u16 (the checksums over the first 0x1FFC bytes)
  block 3, 4  block map and its backup: checksum u16, inverse u16, update counter u16, free blocks u16, last allocated
           u16 (4), then 0xFFB zero entries (the checksums over bytes 4..0x2000)
  the rest  0xFF (unused data blocks)
A checksum is the 16-bit sum of the big-endian words; the inverse sums each word xor 0xFFFF; 0xFFFF becomes 0 in both.
All numbers are big-endian. Platforms: plain Python, any OS.
"""
import argparse
import struct

BLOCK = 0x2000


def checksums(data):
    c = i = 0
    for k in range(0, len(data), 2):
        w = (data[k] << 8) | data[k + 1]
        c = (c + w) & 0xFFFF
        i = (i + (w ^ 0xFFFF)) & 0xFFFF
    return (0 if c == 0xFFFF else c), (0 if i == 0xFFFF else i)


def blank_card(blocks_usable=251):
    total = blocks_usable + 5                       # 5 system blocks: header, 2 directories, 2 block maps
    size_mbit = total * BLOCK * 8 // (1024 * 1024)  # 256 blocks = 16 Mbit (the 251-block card)
    hdr = bytearray(b"\xff" * BLOCK)
    hdr[0:0x26] = (b"ORREPORT-XD\0"                  # serial (12 bytes, fixed)
                   + struct.pack(">QIIIHHH", 0, 0, 0, 0, 0, size_mbit, 0))
    c, i = checksums(hdr[0:0x1FC])
    hdr[0x1FC:0x200] = struct.pack(">HH", c, i)
    dirb = bytearray(b"\xff" * BLOCK)
    dirb[0x1FFA:0x1FFC] = struct.pack(">H", 0)       # update counter
    c, i = checksums(dirb[0:0x1FFC])
    dirb[0x1FFC:0x2000] = struct.pack(">HH", c, i)
    bat = bytearray(BLOCK)
    bat[4:10] = struct.pack(">HHH", 0, blocks_usable, 4)   # update counter, free blocks, last allocated
    c, i = checksums(bat[4:BLOCK])
    bat[0:4] = struct.pack(">HH", c, i)
    return bytes(hdr) + bytes(dirb) * 2 + bytes(bat) * 2 + b"\xff" * (BLOCK * blocks_usable)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("out")
    ap.add_argument("--blocks", type=int, default=251, choices=(59, 123, 251, 507, 1019, 2043))
    a = ap.parse_args()
    data = blank_card(a.blocks)
    with open(a.out, "xb") as f:                    # "x": never overwrites an existing card
        f.write(data)
    print(f"blank card: {a.out} ({a.blocks} blocks, {len(data)} bytes)")


if __name__ == "__main__":
    main()
