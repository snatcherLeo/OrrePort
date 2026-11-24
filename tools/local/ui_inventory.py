"""Fold the 2D inventory's "[ui2d]" log lines into one list of the game's 2D pieces (widescreen groundwork).

Why: every per-ratio UI row (port/ui/layout/<ratio>.ini, docs/port/widescreen-plan.md section 6) needs a piece id, and
the list of pieces comes from replays with the test switch ORRE_UI_INVENTORY=1 (port/src/pc_ui_inventory.c). This
script reads one or more runs' logs, names the code-drawn callers from the exe's symbol table, applies the plan's first
rule set plus a short table of callers whose kind the code shows (full-screen layers, 3D-anchored drawing), and writes:

  <out>/<name>.csv   one row per piece: id, kind, source, window, item, texture, primitive, crop, the union of its
                     rectangles (logical 640 x 480), where it was first seen, in how many places and frames
  <out>/<name>.txt   the summary printed to the terminal (counts by kind, the full-screen and code-drawn pieces)
  <out>/README.md    what the folder is (regenerable; ids and rectangles only)

The output goes under extracted/ (git-ignored, never committed): it holds only ids, rectangles and counts, no game
data, but it is derived from runs of the disc, so it stays with the other regenerable output.

Run:  python -I tools/local/ui_inventory.py --exe <the run's orrecolo.exe> [--name NAME] [--out DIR] LOG [LOG ...]
      a LOG may be written label=path (the label names the sitting in the list; default: the log's folder name)

The log line format (format 1) is documented in docs/port/setup.md, "The 2D inventory". Platform-neutral Python; nm
comes from MSYS2 (mingw64/bin) on Windows, binutils on Linux, and llvm-nm or nm on macOS (the exe's symbols, fixed base).
"""
import argparse
import bisect
import collections
import csv
import os
import re
import shutil
import subprocess
import sys

NEW_RE = re.compile(
    r"\[ui2d\] new n=(\d+) f=(\d+) room=0x([0-9A-Fa-f]+) battle=(\d+) src=(\w+) win=0x([0-9A-Fa-f]+) active=(\d) "
    r"item=(-?\d+) tex=0x([0-9A-Fa-f]+) prim=0x([0-9A-Fa-f]+) crop=(-?\d+),(-?\d+) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+) "
    r"rec=(-?\d+)(?: flags=(\d+))? caller=0x([0-9A-Fa-f]+)")
END_RE = re.compile(
    r"\[ui2d\] end n=(\d+) frames=(\d+) active_frames=(\d+) idle_frames=(\d+) place_frames=(\d+) "
    r"bbox=(-?\d+),(-?\d+),(-?\d+),(-?\d+) last=(\d+) kind=([\w-]+)")

# Callers whose kind the code shows (verified by reading each function; file named). The rule set can't see these:
# an ortho projection's box is always the whole screen, whatever is drawn in it.
CALLER_KIND = {
    "fadeDaemon": ("full-screen", "src/game/effect/fade.c: the screen fade, one 0..640 x 0..480 quad (verified)"),
    "menuOffScreenDraw": ("full-screen", "src/game/menu_offscreen.c: the pause backdrop, a capture drawn 0..640 (verified)"),
    "fadeEffectHookFunction_yoko_Init": ("full-screen", "src/game/effect/fade_effect.c: a fade effect's wipe (likely)"),
    "fadeEffectHookFunction_trainer": ("full-screen", "src/game/effect/fade_effect.c: the trainer swirl wipe (likely)"),
    "fn_801C680C": ("full-screen", "src/game/effect/fade_range_801C4CB8.c: a fade effect's textured quads (likely)"),
    "fn_801C6760": ("full-screen", "src/game/effect/fade_range_801C4CB8.c: a fade effect's untextured quads' set-up "
                    "(likely)"),
    "fn_801C6934": ("full-screen", "src/game/effect/fade_range_801C4CB8.c: a fade effect drawn in the 2D camera "
                    "(likely)"),
    "_blurDrawCapture": ("full-screen", "src/game/effect/effect_visual.c: a move's blur, a back-buffer capture drawn "
                         "back (likely)"),
    "wmapRideOverlay":("full-screen", "src/game/gs_title.c: the world map ride's back-buffer capture overlay (likely)"),
    "fn_80168284": ("full-screen", "src/game/gs_dvd_r47_prefix.c: the GSFilter list's colour quads, a screen tint "
                    "(likely; four indexed corners from a vertex array)"),
    "flareDraw": ("3d-anchored", "src/game/battle/battle_range_candidate_801ED780.c: a flare at a projected 3D point "
                  "(likely, from its name and file)"),
    "GSgfxDrawScene": ("setup", "src/game/gs_gfx_range_800D3FA4.c: each 2D layer's default projection before its "
                       "drawing plays back (verified); not a piece"),
}
# Callers that only repeat what the sprite reports already cover, or the request buffer's playback of a recorded ortho
# (the real caller was reported when it recorded, rec=1)
COVERED_ENV = {"menuDaemon", "winSpriteDraw"}
PLAYBACK = {"gfxPlayCommands"}


def load_symbols(exe):
    nm = shutil.which("nm") or r"C:\msys64\mingw64\bin\nm.exe"
    out = subprocess.run([nm, "-n", exe], capture_output=True, text=True, check=True).stdout
    addrs, names = [], []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "Tt":
            addrs.append(int(parts[0], 16))
            names.append(parts[2])
    return addrs, names


def name_of(symbols, addr):
    addrs, names = symbols
    if addr == 0 or not addrs:
        return ""
    i = bisect.bisect_right(addrs, addr) - 1
    return names[i] if i >= 0 else "0x%X" % addr


def piece_id(p):
    """The piece's stable id (the layout table's section name; docs/port/setup.md "The 2D inventory")."""
    if p["src"] == "sprite":
        where = "menu.%04x" % p["win"] if p["win"] else "sprite"
        if p["item"]:
            return "%s.item.%04x" % (where, p["item"] & 0xFFFF)
        if p["tex"]:
            return "%s.tex.%08x.%d.%d" % (where, p["tex"], p["crop"][0], p["crop"][1])
        return "%s.prim.%x" % (where, p["prim"])
    if p["src"] == "window":
        return "window.%04x" % p["win"]
    if p["src"] == "ortho":
        return "code.%s" % p["caller_name"]
    return "code2d.%s" % p["caller_name"]


def fold(logs, symbols):
    pieces = collections.OrderedDict()
    dropped = collections.Counter()
    for label, path in logs:
        live = {}
        with open(path, encoding="utf-8", errors="replace") as f:
            for line in f:
                if "[ui2d] " not in line:
                    continue
                m = NEW_RE.search(line)
                if m:
                    g = m.groups()
                    p = {"n": int(g[0]), "f": int(g[1]), "room": int(g[2], 16), "battle": int(g[3]), "src": g[4],
                         "win": int(g[5], 16), "active": int(g[6]), "item": int(g[7]), "tex": int(g[8], 16),
                         "prim": int(g[9], 16), "crop": (int(g[10]), int(g[11])),
                         "rect": (int(g[12]), int(g[13]), int(g[14]), int(g[15])), "rec": int(g[16]),
                         "flags": int(g[17] or 0), "caller": int(g[18], 16)}
                    p["caller_name"] = name_of(symbols, p["caller"])
                    if p["src"] == "env" and p["caller_name"] in COVERED_ENV:
                        dropped["2D camera set by %s (its sprites are listed)" % p["caller_name"]] += 1
                        continue
                    if p["src"] == "ortho" and p["caller_name"] in PLAYBACK:
                        dropped["ortho replayed by %s (listed from its recording)" % p["caller_name"]] += 1
                        continue
                    key = piece_id(p)
                    if p["src"] == "ortho":
                        key += "" if p["rect"] == (0, 0, 640, 480) else ".%d.%d.%d.%d" % p["rect"]
                    q = pieces.get(key)
                    if q is None:
                        x, y, w, h = p["rect"]
                        q = pieces[key] = {"id": key, "p": p, "first": "%s room 0x%X battle %d frame %d" % (
                            label, p["room"], p["battle"], p["f"]), "places": 0, "frames": 0, "kinds": collections.Counter(),
                            "bbox": [x, y, x + w, y + h], "sittings": set(), "active": 0}
                    q["places"] += 1
                    q["sittings"].add(label)
                    q["active"] |= p["active"]
                    live[p["n"]] = q
                    continue
                m = END_RE.search(line)
                if m:
                    g = m.groups()
                    q = live.pop(int(g[0]), None)
                    if q is None:
                        continue
                    q["frames"] += int(g[1])
                    q["kinds"][g[10]] += 1
                    b = q["bbox"]
                    b[0], b[1] = min(b[0], int(g[5])), min(b[1], int(g[6]))
                    b[2], b[3] = max(b[2], int(g[7])), max(b[3], int(g[8]))
        for q in live.values():  # the run ended (or was stopped) before the place's end line
            q["kinds"]["(no end line)"] += 1
    return pieces, dropped


def final_kind(q):
    p = q["p"]
    named = CALLER_KIND.get(p["caller_name"])
    if named:
        return named[0], named[1]
    kinds = q["kinds"]
    if kinds.get("full-screen"):
        return "full-screen", "covers 640 x 480 in some place"
    real = [(n, k) for k, n in kinds.items() if k != "(no end line)"]
    if not real:  # the run ended inside the place: the first rule set without the counts
        if p["src"] in ("sprite", "window") and p["win"]:
            return ("menu" if p["active"] else "window"), "no end line (the run stopped in its place)"
        return "code-drawn", "no end line (the run stopped in its place)"
    real.sort(reverse=True)
    return real[0][1], "most places"


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("logs", nargs="+", help="run logs ([label=]path)")
    ap.add_argument("--exe", required=True, help="the exe the runs used (names the callers)")
    ap.add_argument("--name", default="ui_inventory")
    ap.add_argument("--out", default=os.path.join("extracted", "ui_inventory"))
    a = ap.parse_args()
    logs = []
    for spec in a.logs:
        label, _, path = spec.rpartition("=") if "=" in spec else ("", "", spec)
        logs.append((label or os.path.basename(os.path.dirname(os.path.abspath(path))), path))
    symbols = load_symbols(a.exe)
    pieces, dropped = fold(logs, symbols)
    os.makedirs(a.out, exist_ok=True)
    rows = []
    for q in pieces.values():
        kind, why = final_kind(q)
        p = q["p"]
        rows.append({"id": q["id"], "kind": kind, "kind_why": why, "source": p["src"], "window": "0x%X" % p["win"],
                     "item": p["item"], "texture": "0x%08X" % p["tex"], "primitive": "0x%X" % p["prim"],
                     "crop": "%d,%d" % p["crop"], "flags": p["flags"], "bbox": "%d,%d,%d,%d" % tuple(q["bbox"]),
                     "caller": p["caller_name"], "first_seen": q["first"], "places": q["places"],
                     "frames": q["frames"], "sittings": " ".join(sorted(q["sittings"]))})
    csv_path = os.path.join(a.out, a.name + ".csv")
    with open(csv_path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else ["id"])
        w.writeheader()
        w.writerows(rows)
    by_kind = collections.Counter(r["kind"] for r in rows)
    windows = sorted({r["window"] for r in rows if r["source"] in ("sprite", "window") and r["window"] != "0x0"},
                     key=lambda s: int(s, 16))
    lines = ["2D inventory: %d logs (%s), %d distinct pieces, %d windows" % (
        len(logs), ", ".join(l for l, _ in logs), len(rows), len(windows)), "by kind:"]
    for k, n in by_kind.most_common():
        lines.append("  %-12s %d" % (k, n))
    lines.append("by source: " + ", ".join("%s %d" % kv for kv in collections.Counter(r["source"] for r in rows).most_common()))
    lines.append("full-screen pieces:")
    for r in rows:
        if r["kind"] == "full-screen":
            lines.append("  %-44s %-7s bbox %-18s %s (%s)" % (r["id"], r["source"], r["bbox"], r["kind_why"], r["first_seen"]))
    lines.append("code-drawn, 3D-anchored and setup pieces (no window):")
    for r in rows:
        if r["kind"] in ("code-drawn", "3d-anchored", "setup") or (
                r["source"] in ("ortho", "env") and r["kind"] != "full-screen"):
            lines.append("  %-44s %-12s %-7s bbox %-18s places %d (%s)" % (
                r["id"], r["kind"], r["source"], r["bbox"], r["places"], r["first_seen"]))
    callbacks = [r["id"] for r in rows if r["source"] == "window" and r["flags"] & 1]
    lines.append("whole windows: %d (%d with a draw callback: code-drawn content inside the window)" % (
        sum(1 for r in rows if r["source"] == "window"), len(callbacks)))
    lines.append("  with a draw callback: " + " ".join(callbacks))
    lines.append("left out (covered elsewhere): " + ("; ".join("%s x%d" % kv for kv in dropped.most_common()) or "none"))
    lines.append("windows seen: " + " ".join(windows))
    text = "\n".join(lines) + "\n"
    with open(os.path.join(a.out, a.name + ".txt"), "w", encoding="utf-8") as f:
        f.write(text)
    with open(os.path.join(a.out, "README.md"), "w", encoding="utf-8") as f:
        f.write("# ui_inventory (regenerable; never committed)\n\n"
                "The game's 2D pieces from replays run with `ORRE_UI_INVENTORY=1`, folded by "
                "`tools/local/ui_inventory.py`: ids, kinds, rectangles in logical 640 x 480 pixels and counts only. "
                "Each `<name>.csv` has one row per piece; `<name>.txt` is its summary. The ids are the sections of "
                "`port/ui/layout/<ratio>.ini`. Format and rules: `docs/port/setup.md`, \"The 2D inventory\"; "
                "`docs/port/widescreen-plan.md` section 6.\n")
    sys.stdout.write(text)
    print("written: %s (%d rows)" % (csv_path, len(rows)))


if __name__ == "__main__":
    main()
