"""Compare the battle move camera's parameters between a Dolphin replay and a PC run (2026-10-21; snatcherLeo: "P01,
P02, and P03 all share battle camera angle stuff, so compare that").

    python -I tools/local/testing/camera_compare.py <Dolphin replay_K folder> <PC run log> [--all]

Both sides log the move camera's computed parameters (lbl_804673A0, WazaSequenceCameraParams in wazaSequenceCamera.c)
whenever they change: the Dolphin probe `camparams` (camparams.csv) and the PC's ORRE_CAMPARAMS_LOG=1 ("[camp] " lines,
the same columns). A row is one camera setup: the motion (which kind of shot, a random pick), then size and height
(the framed Pokemon's bounding box: deterministic), the rotation, height and distance ranges (from the motion and the
box), and the near / middle / far lengths (random within the ranges).

Rows are split into battles by room (the battle room's id while the battle runs) and by gaps of more than 3,000 frames,
and battles are paired in order. Per battle it prints how many setups each side made, how many are identical in order,
and two order-independent checks: the framing (the set of size / height pairs: did the camera frame the same Pokemon the
same way) and the ranges per motion (do the same motion and box give the same ranges). Those two hold even when the
battle's random numbers drift apart on the PC (snatcherLeo's option A) and the shots are picked differently, so they are the
test of the camera code itself; the in-order count is the test of the whole battle. --all lists every row.
"""
import argparse, csv, re

CAMP = re.compile(r"^\[camp\] (.*)$")
FIELDS = ["motion", "size", "height", "rot_min", "rot_max", "rot_base", "h_min", "h_max", "d_min", "d_max", "near",
          "mid", "far"]


def row_of(frame, room, vals):
    r = {"frame": int(frame), "room": int(room), "motion": int(vals[0])}
    for k, v in zip(FIELDS[1:], vals[1:]):
        r[k] = round(float(v), 3)
    return r


def load_dolphin(folder):
    rows = []
    with open(folder.rstrip("/\\") + "/camparams.csv", encoding="utf-8") as f:
        for r in csv.reader(f):
            if not r or r[0] == "frame" or r[0].startswith("#"):
                continue
            rows.append(row_of(r[0], r[1], r[2:]))
    return rows


def load_pc(log):
    rows = []
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = CAMP.match(line)
            if m:
                p = m.group(1).split(",")
                rows.append(row_of(p[0], p[1], p[2:]))
    return rows


def battles(rows):
    """Runs of non-zero setups in one room, split at gaps over 3,000 frames"""
    out, cur = [], []
    for r in rows:
        if all(r[k] == 0 for k in FIELDS[1:]):
            continue  # the cleared block before the first battle
        if cur and (r["room"] != cur[-1]["room"] or r["frame"] - cur[-1]["frame"] > 3000):
            out.append(cur)
            cur = []
        cur.append(r)
    if cur:
        out.append(cur)
    return out


def key(r, fields):
    return tuple(r[k] for k in fields)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("dolphin")
    ap.add_argument("pc_log")
    ap.add_argument("--all", action="store_true", help="list every setup on both sides")
    a = ap.parse_args()
    db, pb = battles(load_dolphin(a.dolphin)), battles(load_pc(a.pc_log))
    print(f"Dolphin: {len(db)} battles with move-camera setups; PC: {len(pb)}")
    shape = ["size", "height"]
    ranges = ["motion", "size", "height", "rot_min", "rot_max", "rot_base", "h_min", "h_max", "d_min", "d_max"]
    total_bad = 0
    # every range set Dolphin made anywhere in the sitting: the ranges come from the motion, the box and the move's own
    # camera entry, so a PC set the console never made in this sitting is "not seen", not proof of a bug
    seen = {key(r, ranges) for b in db for r in b}
    for k in range(max(len(db), len(pb))):
        d = db[k] if k < len(db) else []
        p = pb[k] if k < len(pb) else []
        same = 0
        for x, y in zip(d, p):
            if key(x, FIELDS) != key(y, FIELDS):
                break
            same += 1
        dframe, pframe = {key(r, shape) for r in d}, {key(r, shape) for r in p}
        prange = {key(r, ranges) for r in p}
        unseen = sorted(r for r in prange if r not in seen)
        room = (d or p)[0]["room"]
        print(f"\nbattle {k + 1} (room {room:#x}): setups Dolphin {len(d)}, PC {len(p)}; identical in order: {same}")
        print(f"  framing (size, height): {len(dframe & pframe)} shared, Dolphin only {sorted(dframe - pframe)}, "
              f"PC only {sorted(pframe - dframe)}")
        print(f"  PC range sets: {len(prange) - len(unseen)} of {len(prange)} also made in Dolphin's sitting"
              + (f"; not seen there: {unseen}" if unseen else ""))
        motions_d = [r["motion"] for r in d]
        motions_p = [r["motion"] for r in p]
        print(f"  motions (shot kinds, random picks) Dolphin {motions_d}\n                                    PC {motions_p}")
        if same < min(len(d), len(p)):
            x, y = d[same], p[same]
            diff = [f"{f} {x[f]} / {y[f]}" for f in FIELDS if x[f] != y[f]]
            print(f"  first different setup: #{same + 1} (Dolphin frame {x['frame']}, PC frame {y['frame']}): "
                  + "; ".join(diff))
        total_bad += len(pframe ^ dframe)
        if a.all:
            for i in range(max(len(d), len(p))):
                x = key(d[i], FIELDS) if i < len(d) else None
                y = key(p[i], FIELDS) if i < len(p) else None
                print(f"    {i + 1:3d} {'=' if x == y else '!'} D {x}\n          P {y}")
    print("\nverdict: " + ("the framing agrees in every battle; differences are in the order and random picks"
                           if total_bad == 0 else "the framing differs somewhere (see above)"))


if __name__ == "__main__":
    main()
