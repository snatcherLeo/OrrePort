"""Record runtime ids (battles, rooms, menu rooms the game really started) in docs/testing/runtime-ids.csv (testing lane;
testing-plan.md 1a, "Resolving untraced rows").

    python -I tools/local/testing/idlog_apply.py --evidence "<what ran, e.g. Dolphin route to step 6>" FILE...

FILE is a Dolphin ids.jsonl (written by idlog.py inside Felk's fork) or a PC run log (build-pc/.../logs/*.log, the
`[battle] start N` and `[room] request 0xN` lines). New (kind, id) pairs are added with the evidence, the source file
name and today's date; pairs already recorded keep their first evidence (nothing is ever removed). The playability
generator (tools/local/playability.py) reads the file: an "untraced" battle, room or menu-room row whose id is listed
there becomes reachable, with this evidence. Ids only, no game data.
"""
import argparse, csv, datetime, json, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "..", "docs", "testing", "runtime-ids.csv")
COLUMNS = ["kind", "id", "evidence", "source", "date"]
PC_BATTLE = re.compile(r"^\[battle\] start (\d+)")
PC_ROOM = re.compile(r"^\[room\] request (0x[0-9a-fA-F]+|\d+)")


def read_ids(path):
    out = []
    with open(path, encoding="utf-8", errors="replace") as f:
        if path.endswith(".jsonl"):
            for line in f:
                try:
                    e = json.loads(line)
                except ValueError:
                    continue
                if e.get("kind") in ("battle", "room") and e.get("id"):
                    out.append((e["kind"], int(e["id"])))
        else:
            for line in f:
                m = PC_BATTLE.match(line)
                if m:
                    out.append(("battle", int(m.group(1))))
                m = PC_ROOM.match(line)
                if m:
                    out.append(("room", int(m.group(1), 0)))
    return [(k, i) for k, i in out if i]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--evidence", required=True, help="what ran (route, build, Dolphin or PC)")
    ap.add_argument("--out", default=OUT)
    ap.add_argument("files", nargs="+")
    a = ap.parse_args()
    rows = []
    if os.path.exists(a.out):
        with open(a.out, encoding="utf-8", newline="") as f:
            rows = list(csv.DictReader(f))
    have = {(r["kind"], int(r["id"])) for r in rows}
    today = datetime.date.today().isoformat()
    added = 0
    for path in a.files:
        for kind, ident in read_ids(path):
            if (kind, ident) in have:
                continue
            have.add((kind, ident))
            rows.append({"kind": kind, "id": ident, "evidence": a.evidence, "source": os.path.basename(path),
                         "date": today})
            added += 1
    rows.sort(key=lambda r: (r["kind"], int(r["id"])))
    with open(a.out, "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, COLUMNS)
        w.writeheader()
        w.writerows(rows)
    print(f"{a.out}: {added} new, {len(rows)} total")


if __name__ == "__main__":
    main()
