"""List unmatched functions ranked by how close they already are.

Usage: python tools/local/next_targets.py [--min 80] [--max 97] [--max-size 2000] [-n 25] [--unit gs_]
Reads build/GC6E01/report.json (run `ninja build/GC6E01/report.json` first).
Skips any function named in the project notes or in the upstream wall
ledger archive/previous_campaign/WALLS.md (whole-identifier match).
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "build" / "GC6E01" / "report.json"
ATTEMPTS = ROOT / "docs" / "notes" / "attempts.md"
WALLS = ROOT / "archive" / "previous_campaign" / "WALLS.md"


def skip_names():
    """Every identifier mentioned in the attempts log or the wall ledger."""
    names = set()
    for path in (ATTEMPTS, WALLS):
        if path.exists():
            names.update(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", path.read_text(encoding="utf-8")))
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--min", type=float, default=80.0, help="minimum fuzzy %%")
    ap.add_argument("--max", type=float, default=100.0, help="exclude functions at or above this fuzzy %%")
    ap.add_argument("--max-size", type=int, default=4000, help="max function size in bytes")
    ap.add_argument("-n", type=int, default=25, help="how many to show")
    ap.add_argument("--unit", default="", help="only units whose name contains this")
    args = ap.parse_args()

    if not REPORT.exists():
        raise SystemExit("No report yet. Run: ninja build/GC6E01/report.json")
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    tried = skip_names()

    rows = []
    for unit in report.get("units", []):
        name = unit.get("name", "")
        if args.unit and args.unit not in name:
            continue
        src = (unit.get("metadata") or {}).get("source_path", "")
        for fn in unit.get("functions", []) or []:
            pct = float(fn.get("fuzzy_match_percent", 0.0))
            size = int(fn.get("size", 0))
            fname = fn.get("name", "")
            if pct >= 100.0 or pct >= args.max or pct < args.min or size > args.max_size:
                continue
            if fname and fname in tried:
                continue
            rows.append((pct, size, fname, name, src))

    # Closest first; among equals, smaller functions first.
    rows.sort(key=lambda r: (-r[0], r[1]))
    print(f"{'fuzzy%':>7}  {'size':>5}  function            unit / source")
    for pct, size, fname, name, src in rows[: args.n]:
        print(f"{pct:7.2f}  {size:5d}  {fname:<18}  {name}  {src}")
    print(f"\n{len(rows)} candidates match these filters.")


if __name__ == "__main__":
    main()
