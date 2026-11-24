"""Compare a PC battle run with the Dolphin battle reference, turn by turn (testing lane; the golden slice).

    python -I tools/local/testing/battle_compare.py <battle_ref folder> <PC run log | second battle_ref folder>

The reference is dolphin_battle_ref.py's output (ref.jsonl). The PC log needs these lines (proposed format; the PC
build prints them where it already prints `[battle] start N`):

    [battle] turn <n> rng=<hex u32> random=<u16>             at each turn-counter change (gFightFloor +0x10)
    [battle] mon <species> hp=<hp>/<max>                     right after each turn line, one per Pokémon on the field
    [battle] hp <species> <before> -> <after>                at each HP change of a Pokémon on the field

(the RNG state is *(u32*)*lbl_80478C94; the turn random gFightFloor +0xA4E4.) For the numbers to match, the PC run
must start the battle with the same RNG state as the reference ("start" event's rng; the debug toolkit's seed command
or a battle-start hook can set it) and make the same menu choices (the reference's choices.json).

Prints, in order: the battle number and start RNG on both sides, then per turn the RNG state, the turn random and
the HP of each Pokémon, then the sequence of HP changes; the first difference is marked FIRST DIFF and everything
after it is still listed (one difference usually shifts the RNG for the rest). Exit status 0 = identical, 1 = differs.
"""
import json, os, re, sys


def load_ref(folder):
    turns, hps, start = [], [], None
    with open(os.path.join(folder, "ref.jsonl"), encoding="utf-8") as f:
        for line in f:
            e = json.loads(line)
            if e["ev"] == "start":
                start = {"battle": e["battle"], "rng": e["rng"]}
            elif e["ev"] == "turn":
                mons = sorted((m["species"], m["hp"], m["max_hp"]) for m in e["field"] if m)
                turns.append({"turn": e["turn"], "rng": e["rng"], "random": e["turn_random"], "mons": mons})
            elif e["ev"] == "hp":
                hps.append((e["turn"], e["species"], e["before"], e["after"]))
    return start, turns, hps


TURN = re.compile(r"^\[battle\] turn (\d+) rng=(0x[0-9a-fA-F]+|\d+) random=(\d+)")
MON = re.compile(r"^\[battle\] mon (\d+) hp=(\d+)/(\d+)")
HP = re.compile(r"^\[battle\] hp (\d+) (\d+) -> (\d+)")
START = re.compile(r"^\[battle\] start (\d+)(?: rng=(0x[0-9a-fA-F]+|\d+))?")


def load_pc(path):
    turns, hps, start, cur = [], [], None, None
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = START.match(line)
            if m:
                start = {"battle": int(m.group(1)), "rng": int(m.group(2), 0) if m.group(2) else None}
                continue
            m = TURN.match(line)
            if m:
                cur = {"turn": int(m.group(1)), "rng": int(m.group(2), 0), "random": int(m.group(3)), "mons": []}
                turns.append(cur)
                continue
            m = MON.match(line)
            if m and cur is not None:
                cur["mons"].append((int(m.group(1)), int(m.group(2)), int(m.group(3))))
                cur["mons"].sort()
                continue
            m = HP.match(line)
            if m:
                hps.append((cur["turn"] if cur else 0, int(m.group(1)), int(m.group(2)), int(m.group(3))))
    return start, turns, hps


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    rs, rt, rh = load_ref(sys.argv[1])
    # the second side can be another Dolphin run (a battle_ref folder): the determinism check
    ps, pt, ph = load_ref(sys.argv[2]) if os.path.isdir(sys.argv[2]) else load_pc(sys.argv[2])
    first = [None]
    other = "other run" if os.path.isdir(sys.argv[2]) else "PC"

    def line(label, a, b):
        same = a == b
        mark = ""
        if not same and first[0] is None:
            first[0] = label
            mark = "   <-- FIRST DIFF"
        print(f"{'  ' if same else '! '}{label}: reference {a} | {other} {b}{mark}")

    line("battle", rs and rs["battle"], ps and ps["battle"])
    line("start rng", rs and rs["rng"], ps and ps["rng"])
    for i in range(max(len(rt), len(pt))):
        a = rt[i] if i < len(rt) else None
        b = pt[i] if i < len(pt) else None
        n = (a or b)["turn"]
        line(f"turn {n} rng", a and a["rng"], b and b["rng"])
        line(f"turn {n} random", a and a["random"], b and b["random"])
        line(f"turn {n} HP (species, hp, max)", a and a["mons"], b and b["mons"])
    for i in range(max(len(rh), len(ph))):
        a = rh[i] if i < len(rh) else None
        b = ph[i] if i < len(ph) else None
        line(f"HP change {i + 1} (turn, species, before, after)", a, b)
    print("identical" if first[0] is None else f"first difference: {first[0]}")
    sys.exit(0 if first[0] is None else 1)


if __name__ == "__main__":
    main()
