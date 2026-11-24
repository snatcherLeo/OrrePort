"""Seed / refresh the editable-areas catalog (docs/modding/editable-areas.md) from the coverage checklist (snatcherLeo, 2026-10-16).

The catalog lists every game system on the modding ladder (design notes 67 and 71):
  0 not yet decoded -> 1 decoded -> 2 documented -> 3 hookable -> 4 record schema -> 5 editor.
Levels 0-2 come from docs/engine/index.md's coverage rows (their status text); levels 3-5 are kept from the catalog's
own "Ladder" column once someone raises them by hand (this tool never lowers a hand-set level). Extra areas that exist
only in design notes so far are listed in EXTRA below.

Usage (repo root): python -I tools/local/editable_areas.py
"""
import re
from pathlib import Path

INDEX = Path("docs/engine/index.md")
OUT = Path("docs/modding/editable-areas.md")
LEVELS = ["0 not yet decoded", "1 decoded", "2 documented (partial)", "2 documented", "3 hookable", "4 record schema",
          "5 editor"]

# Areas from the design notes that have no coverage row yet: (area, design notes, where documented, level, docs status).
# The level and status are set here by hand (refreshed 2026-10-17, snatcherLeo's G6); a hand-raised 3-5 in the catalog wins.
P = "2 documented (partial)"
EXTRA = [
    ("Environment effects on maps: animated water (e.g. Phenac City), fog, lighting, skyboxes, ambient particles", "71",
     "gfx.md (map fog), floor-resources.md (room scene), particles.md; animated water and skyboxes not yet investigated",
     "1 decoded in part (map fog, room lighting records)", "partial"),
    ("Battle arenas as records: model, lighting, camera presets, music, crowd / ambience; custom arenas", "71",
     "[battle-arenas.md](../engine/battle-arenas.md) (all 50 arenas, the battle grid, the idle camera); lighting and crowd open", P, "partial"),
    ("Tournaments: rounds, trainers per round, rules (level caps, bans), rewards, arena, music, unlock flag; custom tournaments", "71",
     "[colosseum-challenges.md](../engine/colosseum-challenges.md) (the town Colosseums, Mt. Battle)", P, "partial"),
    ("Gift Pokémon (starters, Plusle, Ho-Oh, Pikachu / Celebi) and new gift events", "61",
     "[menu-rooms.md](../engine/menu-rooms.md), [new-game-flow.md](../engine/new-game-flow.md) (gift kinds 1-6; contents in code, so a hook is needed)", P, "partial"),
    ("Ability behaviour (building blocks: trigger, conditions, effects)", "62",
     "[battle-abilities.md](../engine/battle-abilities.md) (every battle ability located in the code); the building blocks are OrrePort's design", P, "partial"),
    ("Delta species (clones: independent copies by default, optional link; badge)", "69, inbox 2026-10-17",
     "an OrrePort feature on the species data (the species row above)", "n/a (OrrePort feature)", "design"),
    ("PDA pause menu entries", "64", "[pda.md](../engine/pda.md), [pause-menu.md](../engine/pause-menu.md)", P, "partial"),
    ("UI screens: layout records (PDA, summary, PC box, battle HUD, text boxes, title)", "66",
     "[menus.md](../engine/menus.md); per-screen positions not yet collected (many are in code)", "1 decoded in part (menu item table, window records)", "partial"),
    ("Room contents: props, doors / warps, triggers, NPC spots, camera zones", "67",
     "[rooms.md](../engine/rooms.md), [room-changes.md](../engine/room-changes.md), [npcs.md](../engine/npcs.md); trigger zones: "
     "[trainer-encounters.md](../engine/trainer-encounters.md), [player-movement.md](../engine/player-movement.md)", P, "partial"),
    ("Cutscenes (timeline: camera, characters, dialogue, music, sound, lighting)", "63",
     "[cutscenes.md](../engine/cutscenes.md) (38 scripts take the camera)", P, "partial"),
    ("Room cameras (zones and angles)", "63",
     "[animation-tracks.md](../engine/animation-tracks.md) (camera shot format, 126 shots), [field-camera.md](../engine/field-camera.md)", P, "partial"),
    ("Story flags with readable names; jump to story point", "63",
     "[flags.md](../engine/flags.md), [story-route.md](../engine/story-route.md) (the whole story route)", P, "partial"),
    ("Player avatar packages (models, animation slots, cutscene slot)", "36, 56",
     "[player-appearances.md](../engine/player-appearances.md) (18 places), [animation-catalog.md](../engine/animation-catalog.md) (\"Avatar packages\")", P, "partial"),
    ("Ambush events (trigger zone, entrance animation, battle)", "inbox 2026-10-17",
     "[trainer-encounters.md](../engine/trainer-encounters.md) (\"Ambushes\": 14 places)", P, "partial"),
    ("Item use from the bag; teaching TMs and HMs", "inbox 2026-10-17",
     "[bag-and-pc.md](../engine/bag-and-pc.md) (item limits, the use flow, TM teaching)", P, "partial"),
    ("Battle cameras (idle camera, move cameras)", "15, inbox 2026-10-17",
     "[battle-arenas.md](../engine/battle-arenas.md) (idle camera), [wzx.md](../engine/wzx.md) (move camera keys)", P, "partial"),
    ("OrrePort natives: new script natives that features register, so events can call them", "inbox 2026-10-17 (G2)",
     "an OrrePort feature on the script system; [script-vm.md](../engine/script-vm.md) (the game's 246 natives)", "n/a (OrrePort feature)", "design"),
]


def level_from_status(status):
    # ignore hidden history text (<!--h-->...<!--/h-->), e.g. 'was "not started"'
    s = re.sub(r"<!--h-->.*?<!--/h-->", "", status).lower()
    if "not started" in s:
        return LEVELS[0]
    if "complete" in s and "partial" not in s:
        return LEVELS[3]
    if "decoded" in s and "partial" not in s:
        return LEVELS[1]
    return LEVELS[2]


def index_rows():
    rows = []
    for line in INDEX.read_text(encoding="utf-8").splitlines():
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 5 or not line.startswith("| ") or cells[0] in ("Data", "System", "Name") or set(cells[0]) <= {"-", " "}:
            continue
        if cells[-1].lower() not in ("no", "yes") and not cells[-1].lower().startswith(("no", "yes")):
            continue  # only the coverage rows (their last column is "Mod-ready")
        page = re.sub(r"\]\((?!https?:|\.\./)", "](../engine/", cells[2])  # links are relative to docs/engine/
        rows.append((re.sub(r"\*\*", "", cells[0]), cells[1], page))
    return rows


def kept_levels():
    """Hand-set levels 3-5 from an existing catalog, by area name."""
    kept = {}
    if OUT.exists():
        for line in OUT.read_text(encoding="utf-8").splitlines():
            cells = [c.strip() for c in line.strip().strip("|").split("|")]
            if len(cells) >= 2 and cells[1][:1] in "345":
                kept[cells[0]] = cells[1]
    return kept


def main():
    kept = kept_levels()
    out = ["# Editable-areas catalog (the modding roadmap)", "",
           "*Private (OrrePort's mod system). Started 2026-10-16 (snatcherLeo, design note 71). One living list of every game "
           "system and how far it is on the modding ladder:* **0 not yet decoded → 1 decoded → 2 documented → 3 hookable "
           "→ 4 record schema → 5 editor** *(design note 67's recipe: decode → hook points → record schema → auto editor → "
           "custom view → a test mod). Levels 0-2 are refreshed from the coverage checklist in `docs/engine/index.md` by "
           "`python -I tools/local/editable_areas.py`; raise a row to 3-5 by hand when its hooks, schema or editor exist "
           "(the tool keeps those). Add a row whenever a new system turns up.*", "",
           "| Area | Ladder | Docs status (index.md) | Page / design notes |", "|---|---|---|---|"]
    for name, status, page in index_rows():
        lvl = kept.get(name, level_from_status(status))
        out.append(f"| {name} | {lvl} | {status} | {page} |")
    out += ["", "## Areas so far only in design notes (no coverage row yet)", "",
            "*Levels set in the tool's EXTRA list (refreshed 2026-10-17); none is past 2 yet, since no area has its modding "
            "hooks yet.*", "",
            "| Area | Ladder | Docs status | Design notes / where documented |", "|---|---|---|---|"]
    for name, notes, where, level, status in EXTRA:
        lvl = kept.get(name, level)
        out.append(f"| {name} | {lvl} | {status} | notes {notes}; {where} |")
    counts = {}
    for line in out:
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) >= 2 and cells[1][:1].isdigit():
            counts[cells[1][0]] = counts.get(cells[1][0], 0) + 1
    out += ["", "**Counts by level:** " + ", ".join(f"{k}: {counts[k]}" for k in sorted(counts)) + "."]
    OUT.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"{OUT}: {sum(counts.values())} areas; by level {dict(sorted(counts.items()))}")


if __name__ == "__main__":
    main()
