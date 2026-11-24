"""Check the data rename ledger (docs/data_renames.csv) against the tree (2026-11-08; the data-label step).

The function renames have tools/check_naming.py; data labels get this smaller checker:
  * every row has the ledger columns, a known tier (T1 / T2 / T3) and an evidence line of at most 200 characters;
  * new_name is a plain C identifier that is not a placeholder (`lbl_` / `fn_`), is unique in the ledger and in
    config/GC6E01/symbols.txt, and symbols.txt defines it at the row's address and section;
  * the old name no longer appears as a whole word in live files (src, include, port, tools, configure.py,
    config/GC6E01 except symbolmap, docs/engine), apart from NAME tags.
Exit status 1 on any error.

Usage: python -I tools/check_data_naming.py
"""
import csv
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEDGER = ROOT / "docs" / "data_renames.csv"
SYMBOLS = ROOT / "config" / "GC6E01" / "symbols.txt"
COLUMNS = ["address", "section", "old_name", "new_name", "tier", "evidence", "evidence_ref", "unit", "source_file",
           "batch_id", "commit", "date"]
LIVE = ["src", "include", "port", "tools", "configure.py", "config/GC6E01", "docs/engine", "docs/RULE_EXCEPTIONS.md"]
EXCLUDE = [":!config/GC6E01/symbolmap", ":!tools/symbolmap", ":!docs/data_renames.csv",
           ":!docs/engine/proposed-names.md"]  # the proposals log keeps each row's old name as history


def main():
    errors = []
    if not LEDGER.exists():
        print("check_data_naming: no ledger (nothing to check)")
        return 0
    with LEDGER.open(encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames != COLUMNS:
            errors.append(f"ledger columns {reader.fieldnames} != {COLUMNS}")
        rows = list(reader)
    syms = {}
    for line in SYMBOLS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(\S+) = (\.\w+):0x([0-9A-Fa-f]+);", line)
        if m:
            syms.setdefault(m.group(1), []).append((m.group(2), int(m.group(3), 16)))
    seen = set()
    for i, r in enumerate(rows, 2):
        loc = f"line {i} ({r.get('new_name')})"
        new, old = r.get("new_name", ""), r.get("old_name", "")
        if r.get("tier") not in ("T1", "T2", "T3"):
            errors.append(f"{loc}: tier '{r.get('tier')}'")
        if not r.get("evidence") or len(r["evidence"]) > 200:
            errors.append(f"{loc}: evidence missing or over 200 characters")
        if not re.fullmatch(r"[A-Za-z_]\w*", new) or re.match(r"(lbl|fn)_", new):
            errors.append(f"{loc}: '{new}' is not a valid non-placeholder name")
        if new in seen:
            errors.append(f"{loc}: '{new}' appears twice in the ledger")
        seen.add(new)
        try:
            addr = int(r.get("address", ""), 16)
        except ValueError:
            errors.append(f"{loc}: bad address '{r.get('address')}'")
            continue
        defs = syms.get(new, [])
        if len(defs) != 1:
            errors.append(f"{loc}: symbols.txt defines '{new}' {len(defs)} times")
        elif defs[0] != (r.get("section"), addr):
            errors.append(f"{loc}: symbols.txt has '{new}' at {defs[0]}, ledger says ({r.get('section')}, {addr:#x})")
        if old in syms:
            errors.append(f"{loc}: old name '{old}' is still in symbols.txt")
        hits = subprocess.run(["git", "grep", "-n", "-w", old, "--"] + LIVE + EXCLUDE, cwd=ROOT,
                              capture_output=True, text=True).stdout.splitlines()
        hits = [h for h in hits if "NAME-" not in h]
        for h in hits[:5]:
            errors.append(f"{loc}: old name still referenced: {h[:160]}")
    for e in errors:
        print("error:", e)
    print(f"check_data_naming: {len(rows)} row(s), {len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
