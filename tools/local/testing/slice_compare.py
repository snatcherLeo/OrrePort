"""Compare a slice run's room entries on two sides: story flags at each room change (testing lane; golden slice).

    python -I tools/local/testing/slice_compare.py <Dolphin ids.jsonl> <PC log | second ids.jsonl> [--flags-dir DIR]

Dolphin side: the "entered" lines idlog.py writes (room, frame, RNG state, flags CRC) during dolphin_slice.py.
PC side: lines `[slice] entered 0x<room> frame=<n> rng=0x<hex> flags=0x<crc32>` (printed at pc_room_entered_hook; the
CRC is over the four story-flag classes in order, each u32 big-endian, as idlog.flags_crc). A second ids.jsonl works
too (Dolphin vs Dolphin: the determinism check).

The slice passes this check when both sides enter the same rooms in the same order with the same flags CRC at each.
Frame and RNG differences are listed but don't fail it (loading times differ; the RNG advances with time; battles are
compared separately with battle_compare.py after the seed step). With --flags-dir (the Dolphin folder holding
flags_entry_NNN_<room>.json), a flag mismatch also shows the Dolphin flags of that entry, to compare by hand with a PC
dump. Exit status 0 = same, 1 = differs.
"""
import argparse, json, os, re

PC_LINE = re.compile(r"\[slice\] entered (0x[0-9a-fA-F]+|\d+) frame=(\d+) rng=(0x[0-9a-fA-F]+|\d+) flags=(0x[0-9a-fA-F]+|\d+)")


def load(path):
    out = []
    with open(path, encoding="utf-8", errors="replace") as f:
        if path.endswith(".jsonl"):
            for line in f:
                try:
                    e = json.loads(line)
                except ValueError:
                    continue
                if e.get("kind") == "entered":
                    out.append({"room": e["id"], "frame": e["frame"], "rng": e["rng"], "flags": e["flags_crc"]})
        else:
            for line in f:
                m = PC_LINE.search(line)
                if m:
                    out.append({"room": int(m.group(1), 0), "frame": int(m.group(2)), "rng": int(m.group(3), 0),
                                "flags": int(m.group(4), 0)})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--flags-dir")
    a = ap.parse_args()
    A, B = load(a.a), load(a.b)
    other = "other run" if a.b.endswith(".jsonl") else "PC"
    first = None
    print(f"{len(A)} entries (reference) | {len(B)} entries ({other})")
    for i in range(max(len(A), len(B))):
        x = A[i] if i < len(A) else None
        y = B[i] if i < len(B) else None
        same_room = x and y and x["room"] == y["room"]
        same_flags = same_room and x["flags"] == y["flags"]
        mark = "  " if same_flags else "! "
        if not same_flags and first is None:
            first = i + 1
        fmt = lambda e: (f"room 0x{e['room']:X} frame {e['frame']} rng 0x{(e['rng'] or 0):08X} flags 0x{e['flags']:08X}"
                         if e else "(none)")
        info = ""
        if same_room:
            info = f"  [frames {y['frame'] - x['frame']:+d}, rng {'same' if x['rng'] == y['rng'] else 'differs'}]"
        print(f"{mark}{i + 1:3d}: reference {fmt(x)} | {other} {fmt(y)}{info}" + ("   <-- FIRST DIFF" if first == i + 1 and not same_flags else ""))
        if first == i + 1 and not same_flags and a.flags_dir and x:
            name = f"flags_entry_{i + 1:03d}_{x['room']:x}.json"
            p = os.path.join(a.flags_dir, name)
            if os.path.exists(p):
                print(f"     reference flags at this entry: {p}")
    print("same rooms and flags at every entry" if first is None else f"first difference at entry {first}")
    raise SystemExit(0 if first is None else 1)


if __name__ == "__main__":
    main()
