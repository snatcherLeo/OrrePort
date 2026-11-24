"""Tag a PC roll record's "create" lines with their battle, for the Dolphin replay's roll injection (2026-11-02).

    python -I tools/local/testing/rolls_for_dolphin.py <sitting.rolls> <pc run.log> <out.rolls> [--before F]

The PC's ORRE_ROLL_RECORD (pc_debug_game.c) writes "<kind> <battle> <index> <state before> <frame> <state after>"
per draw; "create" lines (draws inside pokemonCreate / pokemonCreateRndFit) are one sitting-wide list with battle 0.
Dolphin watches the RNG only inside battles, so it can inject only the creates that belong to a battle: an opponent
team is made in one burst between the battle's start and its start message (P25's live run: battle start, then 954
creation draws at frame 4,061, then message 30317 at 4,119; the console's P07: 52 frames before). This tool gives each
create line whose frame lies within --before frames (default 600) before the Nth start message ("[bmsg] f,r,30317" in
the PC log, as the battle numbering) battle N, numbered from 0 within that battle; the rest stay battle 0 (field
creations: gifts, Shadow Pokémon; not injected). Other lines pass through unchanged.
"""
import argparse
import collections
import re


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rolls")
    ap.add_argument("pclog")
    ap.add_argument("out")
    ap.add_argument("--before", type=int, default=600)
    a = ap.parse_args()
    starts = [int(m.group(1)) for m in (re.match(r"\[bmsg\] (\d+),\d+,30317\b", l)
                                         for l in open(a.pclog, encoding="utf-8", errors="replace")) if m]
    counts = collections.Counter()
    out = []
    for line in open(a.rolls, encoding="utf-8"):
        p = line.split()
        if line.startswith("#") or len(p) < 5 or p[0] != "create":
            out.append(line)
            continue
        f, n = int(p[4]), 0
        for k, s in enumerate(starts):
            if s - a.before <= f <= s:
                n = k + 1
                break
        p[1], p[2] = str(n), str(counts[n])
        counts[n] += 1
        out.append(" ".join(p) + "\n")
    with open(a.out, "w", encoding="utf-8") as fo:
        fo.writelines(out)
    print(f"{len(starts)} battle start messages in the log; create lines: " +
          ", ".join(f"battle {n}: {c}" for n, c in sorted(counts.items())))


if __name__ == "__main__":
    main()
