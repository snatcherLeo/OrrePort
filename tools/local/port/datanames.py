"""Data-label names for the port tools (2026-11-08; the readability renames' data step).

The port tools find data symbols in the C sources by their dtk placeholder form, `lbl_<8 hex digits>`. Once a data
label gets a real name (the data rename ledger, docs/data_renames.csv), a tool that only knows the placeholder form
would stop seeing it. This module gives every tool one pattern that matches both: the placeholder form and every
renamed data label in the ledger. With an empty ledger the pattern is exactly the old `lbl_[0-9A-Fa-f]{8}`, so the
tools behave as before.

Use (from a tool in tools/local/port or port/tools):
    sys.path.insert(0, str(Path(__file__).resolve().parent))   # or the path to tools/local/port
    from datanames import DATA_NAME, renamed_to_old
    re.compile(r"\\b(" + DATA_NAME + r")\\b")

The ledger's columns follow docs/function_renames.csv plus the symbol's section:
    address,section,old_name,new_name,tier,evidence,evidence_ref,unit,source_file,batch_id,commit,date
"""
import csv
import re
from pathlib import Path

LEDGER = Path(__file__).resolve().parents[3] / "docs" / "data_renames.csv"
PLACEHOLDER = r"lbl_[0-9A-Fa-f]{8}"


def _rows():
    if not LEDGER.exists():
        return []
    with LEDGER.open(encoding="utf-8", newline="") as f:
        return [r for r in csv.DictReader(f) if r.get("new_name")]


def renamed_to_old():
    """{current name: the dtk placeholder it replaced} for every renamed data label (a name renamed twice maps to the
    first placeholder)."""
    rows = _rows()
    first = {}
    for r in rows:
        first[r["new_name"]] = first.get(r["old_name"], r["old_name"])
    # drop intermediate names that were renamed again
    return {n: o for n, o in first.items() if n not in {r["old_name"] for r in rows}}


def data_name_pattern():
    """A regex fragment (no capture group) matching a placeholder or any renamed data label, longest names first."""
    names = sorted(renamed_to_old(), key=len, reverse=True)
    return "(?:" + PLACEHOLDER + "".join("|" + re.escape(n) for n in names) + ")"


DATA_NAME = data_name_pattern()
_OLD = renamed_to_old()


def sort_key(name):
    """Sort key for generated output: a renamed data label sorts where its placeholder did, so a rename never changes
    the order a generator writes things in (pcdata's definitions and its start-up fill calls; added 2026-11-08 after
    data batch 2 reordered pc_disc_fill and the PC exe stopped being instruction-for-instruction the reference playthrough's)."""
    return _OLD.get(name, name)
