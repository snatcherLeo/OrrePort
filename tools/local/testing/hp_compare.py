"""Compare HP changes in battle between a Dolphin replay and a PC run (2026-10-20; snatcherLeo: match "whatever else
nitty gritty" in P01-P03).

    python -I tools/local/testing/hp_compare.py <Dolphin replay_K folder> <PC run log>

Dolphin: the "hp" events in replay_events.jsonl (species, was, now; one per battle slot whose HP changed). PC: the
"[battle] hp <species> <before> -> <after>" lines (pokemon_data.c, ORRE_BATTLE_LOG). The PC also prints the level-set
HP changes when a battle builds its Pokemon (before the first send-out), and Dolphin can report two slots swapping
places as a pair of changes; both are dropped before the two lists are aligned in order on (species, was, now).
Prints how many match in order and each place they part (a different damage roll, a missed or critical hit).
"""
import argparse, difflib, json, re

ROW = re.compile(r"^\[battle\] hp (\d+) (\d+) -> (\d+)")


def load_dolphin(folder):
    ev = []
    with open(folder.rstrip("/\\") + "/replay_events.jsonl", encoding="utf-8") as f:
        for line in f:
            e = json.loads(line)
            if e.get("ev") == "hp":
                ev.append((int(e["f"]), (int(e["species"]), int(e["was"]), int(e["now"]))))
    out, i = [], 0
    while i < len(ev):
        f, v = ev[i]
        # two slots of the same species trading HP values in one frame: a swap of places, not damage
        if i + 1 < len(ev) and ev[i + 1][0] == f and ev[i + 1][1] == (v[0], v[2], v[1]):
            i += 2
            continue
        out.append((f, v))
        i += 1
    return out


def load_pc(log):
    out, battle = [], False
    with open(log, encoding="utf-8", errors="replace") as f:
        for line in f:
            if line.startswith("[battle] mon "):
                battle = True  # the battle's Pokemon are built; HP changes from here on are the battle's
            elif line.startswith("[battle] start") or line.startswith("[battle] end"):
                battle = False  # building the next battle's Pokemon (level-set HP), or healing after one
            m = ROW.match(line)
            if m and battle:
                out.append((0, (int(m.group(1)), int(m.group(2)), int(m.group(3)))))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dolphin")
    ap.add_argument("pc_log")
    a = ap.parse_args()
    d, p = load_dolphin(a.dolphin), load_pc(a.pc_log)
    sm = difflib.SequenceMatcher(a=[v for _, v in d], b=[v for _, v in p], autojunk=False)
    matched = sum(b.size for b in sm.get_matching_blocks())
    print(f"HP changes: Dolphin {len(d)}, PC {len(p)}, matched in order {matched}")
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag == "equal":
            for _, v in d[i1:i2]:
                print(f"  = species {v[0]}: {v[1]} -> {v[2]}")
        else:
            print(f"  {tag}: Dolphin {[v for _, v in d[i1:i2]]}  PC {[v for _, v in p[j1:j2]]}")


if __name__ == "__main__":
    main()
