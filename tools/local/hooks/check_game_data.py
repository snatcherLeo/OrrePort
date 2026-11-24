"""Block game data from the disc from ever being committed or pushed (snatcherLeo, 2026-10-15).

Game data from snatcherLeo's disc (extracted files, the PC build's generated data definitions) lives only in git-ignored
folders. This check runs from the git hooks in this folder (pre-commit, pre-push; activate once per clone with
`git config core.hooksPath tools/local/hooks`, shared by all worktrees) and fails if anything about to be committed or
pushed:
  - is under `extracted/`, `build-pc/`, `orig/` or `checkpoints/` (even when forced past .gitignore with `git add -f`), or
  - contains the marker line every generated-from-disc file starts with (see MARKER), wherever it was copied to.
Usage: python -I tools/local/hooks/check_game_data.py --staged          (pre-commit)
       python -I tools/local/hooks/check_game_data.py --range A..B      (pre-push: the commits being pushed)
"""
import subprocess
import sys

# Built at run time so this file itself doesn't contain the marker text.
MARKER = "ORREPORT-" + "GENERATED-FROM-DISC"
BLOCKED_DIRS = ("extracted/", "build-pc/", "orig/", "checkpoints/")  # checkpoints: debug copies of the save block (2026-10-17)
# Binaries and disc images, wherever they are: a PC build links data generated from the disc, and the compiled
# file has no marker line to find (snatcherLeo, 2026-10-16: no builds leave this PC until the exe loads its data from the
# player's disc). Object files and libraries too, for the same reason.
BLOCKED_EXTS = (".exe", ".dll", ".o", ".obj", ".a", ".lib", ".elf", ".dol", ".rel", ".iso", ".rvz", ".gcm",
                ".ciso", ".nkit", ".gci", ".raw", ".sav", ".fsys")
# The one exception (snatcherLeo, 2026-11-04: "keep these saves on Github ... just private one"): the reference playthrough's sitting saves in
# recordings/run3/ (their start and end memory-card files), kept in the private repo; at the end of the reference playthrough's Vulkan run
# snatcherLeo reviews whether to remove them (recordings/run3/README.md). Only .gci, only there; the marker check still applies.
ALLOWED_SAVE_DIR = "recordings/run3/"
# The second exception (snatcherLeo, 2026-11-14: "Kanji traced from the disc can ship ... We ship the traces", "Same with
# european langauge stuff"; the project rules "Game data never enters git"): OrrePort's own traced letter shapes, smooth outlines
# traced from the discs' lettering (first: the PC screen's Main Menu wheel labels, tools/local/pc_menu_pictures.py,
# 2026-11-15). Each such PNG carries the marker (it was made from disc pictures), and it is allowed in this one
# folder only, PNG only; anywhere else the marker still blocks it. Never the disc's own pictures or bitmaps.
ALLOWED_TRACE_DIR = "port/ui/traced/"
# Within it, the OrreFont font files (added 2026-11-16, the project's build helper; snatcherLeo: the installable "OrreFont Colosseum"
# ships in bin\Fonts\, a lawyer's read cleared it): the same traced outlines written as a TrueType file by
# tools/local/orrefont/orrefont.py, which puts the marker in a private table of the font. Allowed as .ttf in
# port/ui/traced/fonts/ only; a .ttf with the marker anywhere else is still refused.
ALLOWED_TRACE_FONT_DIR = "port/ui/traced/fonts/"


def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, errors="replace").stdout


def check(paths, show):
    bad = []
    for p in paths:
        if p.startswith(BLOCKED_DIRS):
            bad.append(f"{p}: in a folder that holds game data ({p.split('/')[0]}/)")
            continue
        if p.lower().endswith(BLOCKED_EXTS) and not (p.startswith(ALLOWED_SAVE_DIR) and p.lower().endswith(".gci")):
            bad.append(f"{p}: a binary, build output, save or disc file (never committed)")
            continue
        if p.startswith(ALLOWED_TRACE_DIR) and p.lower().endswith(".png"):
            continue  # OrrePort's traced outlines (the second exception, above)
        if p.startswith(ALLOWED_TRACE_FONT_DIR) and p.lower().endswith(".ttf"):
            continue  # the OrreFont files, the same outlines as a font (above)
        content = show(p)
        if content is not None and MARKER in content:
            bad.append(f"{p}: generated from disc data (marker line found)")
    return bad


def main():
    if "--staged" in sys.argv:
        paths = [p for p in git("diff", "--cached", "--name-only", "--diff-filter=ACMR").splitlines() if p]
        bad = check(paths, lambda p: git("show", f":{p}"))
    elif "--range" in sys.argv:
        rng = sys.argv[sys.argv.index("--range") + 1]
        tip = rng.split("..")[-1]
        paths = [p for p in git("diff", "--name-only", "--diff-filter=ACMR", rng).splitlines() if p]
        bad = check(paths, lambda p: git("show", f"{tip}:{p}"))
    else:
        print(__doc__)
        return 2
    if bad:
        print("BLOCKED: game data must never be committed or pushed (the project rules, the project notes):")
        for b in bad:
            print("  " + b)
        print("Unstage these files (git restore --staged <file>) and keep them in a git-ignored folder.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
