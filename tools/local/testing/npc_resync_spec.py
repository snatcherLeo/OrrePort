"""Write pad_resync.py lines that make a room visit's wandering NPCs follow the console's route (2026-10-27).

    python -I tools/local/testing/npc_resync_spec.py <replay>/npc.csv ROOM:VISIT --shift N --anchor A
        [--slots 12,13] [--from F] [--to T]

From Dolphin's npc probe of one room visit (`PLAYREC_NPC=room,-1,frames,visit playrec.py replay N --probe npc`), one
`npc` placement each time a wandering person (state 4) starts a walk (its new target, the console's position) or
arrives (its pause), so its route follows the console's even where the PC's 30 fps frames run out of phase and its
own random picks differ. `--shift`: PC visit frame minus capture frame (find it from Wes's own positions, slot 0, in
the capture and the PC's `[ev] pos` log); `--anchor`: the PC visit frame of the visit's fade-in anchor (the pad's
`@0x3<room>` step; `[pad_script] fade-in finished ... at frame F`, minus the visit's entry frame). Prints spec lines on
stdout; append them to the sitting's `resyncs/<code>_<sitting>.txt`. Placements that would land before the anchor are
left out (pad_resync.py counts from it).
"""
import argparse
import csv


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("npc_csv")
    ap.add_argument("where", help="ROOM:VISIT, e.g. 0x3:4")
    ap.add_argument("--shift", type=int, required=True)
    ap.add_argument("--anchor", type=int, default=0)
    ap.add_argument("--absolute", type=int, default=None, metavar="ENTRY",
                    help="print '@PCFRAME' lines instead (pad_resync.py --log): ENTRY = the PC frame the visit starts "
                         "at in the reference run; for visits with no fade-in anchor")
    ap.add_argument("--slots", default="", help="only these slots (default: every person that wanders)")
    ap.add_argument("--from", dest="lo", type=int, default=0)
    ap.add_argument("--to", dest="hi", type=int, default=10 ** 9)
    a = ap.parse_args()
    only = {int(s) for s in a.slots.split(",") if s}
    last = {}
    out = []
    with open(a.npc_csv, newline="") as f:
        for r in csv.DictReader(f):
            if r["x"] == "":
                continue
            slot, frame = int(r["slot"]), int(r["room_frame"])
            if slot == 0 or (only and slot not in only) or r["state"] != "4":
                continue
            key = (r["sub"], r["tx"], r["tz"])
            prev = last.get(slot)
            last[slot] = key
            if prev is None or prev == key or not a.lo <= frame <= a.hi:
                continue
            offset = frame + a.shift - a.anchor
            if offset < 0:
                continue
            vals = [r["x"], r["y"], r["z"], r["tx"], r["ty"], r["tz"], r["pause"], r["yaw"], r["state"], r["sub"]]
            where = f"@{a.absolute + frame + a.shift}" if a.absolute is not None else f"{a.where} +{offset}"
            out.append((offset, f"{where} npc {slot} " + " ".join(f"{float(v):.7g}" for v in vals)
                        + f"  # capture frame {frame}, {'walks' if r['sub'] != '0' else 'stops'}"))
    for _, line in sorted(out):
        print(line)


if __name__ == "__main__":
    main()
