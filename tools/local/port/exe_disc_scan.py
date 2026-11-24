"""The release check for the reference playthrough Part 1 item 2c (2026-11-01; docs/port/disc-data-loading.md, step 6): is any of
main.dol's game content inside the built exes? For each content table (snatcherLeo's categories: text, images, the save
banner and icon; the battle scripts are game logic in the decomp's own source, kept compiled by his decision of
2026-11-01, so they are not checked here), a 64-byte window from its middle is read from the local main.dol (by GameCube
address, through the DOL header) and searched in each exe. Prints FOUND / not found per table and exe (never bytes);
exit code 1 when anything is found. Needs the local disc files (orig/GC6E01/sys/main.dol); reads only.

    python -I tools/local/port/exe_disc_scan.py [exe ...]     (default: build-pc/firstlink/orrecolo.exe, build-pc/orreport.exe)
"""
import os
import re
import struct
import sys

MAIN = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
TABLES = {  # name: (GameCube address, size)
    "message bank (684 messages)": (0x802CF810, 74976),
    "menu parts texture 156x103": (0x803156E0, 65024),
    "window frame texture 186x87": (0x803254E0, 66304),
    "Nintendo logo screen 640x480": (0x803357E0, 153728),
}


def main():
    dol = open(os.path.join(MAIN, "orig", "GC6E01", "sys", "main.dol"), "rb").read()

    def at(addr, size):
        for i in range(18):
            off, a, n = (struct.unpack_from(">I", dol, base + 4 * i)[0] for base in (0, 0x48, 0x90))
            if n and a <= addr and addr + size <= a + n:
                return dol[off + addr - a: off + addr - a + size]
        return None

    def windows(blob):
        """four 64-byte windows spread over the table, each moved to the nearest spot with at least 24 different byte
        values (corrected 2026-11-01: a window from the middle of an image was a run of zeros, which any program
        contains, and gave false finds)"""
        out = []
        for q in (1, 3, 5, 7):
            start = len(blob) * q // 8
            for off in range(start, len(blob) - 64, 16):
                if len(set(blob[off:off + 64])) >= 24:
                    out.append(blob[off:off + 64])
                    break
        return out

    probes = {name: windows(at(addr, size) or b"") for name, (addr, size) in TABLES.items()}
    # the save banner and icon are C arrays in memcard.c (the decomp's copy of main.dol's bytes)
    memcard = open(os.path.join(MAIN, "src", "game", "memcard.c"), encoding="utf-8").read()
    for name, sym in (("save banner", "sMemcardBanner"), ("save icon", "sMemcardIcon")):
        m = re.search(sym + r"\[0x[0-9A-Fa-f]+\] = \{(.*?)\};", memcard, re.S)
        if m:
            probes[name] = windows(bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1))))
    exes = sys.argv[1:] or [os.path.join(MAIN, "build-pc", "firstlink", "orrecolo.exe"),
                            os.path.join(MAIN, "build-pc", "orreport.exe")]
    found = 0
    for exe in exes:
        data = open(exe, "rb").read()
        for name, wins in probes.items():
            hits = sum(1 for w in wins if w in data)
            state = "no usable windows" if not wins else "FOUND" if hits == len(wins) else \
                f"partly ({hits} of {len(wins)} windows)" if hits else "not found"
            found += hits > 0
            print(f"{os.path.basename(exe):14s} {name:30s} {state}")
    print("game content in the exe(s): " + (f"{found} table(s) found" if found else "none"))
    sys.exit(1 if found else 0)


if __name__ == "__main__":
    main()
