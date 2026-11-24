r"""Assemble OrrePort's portable folder from the current PC build (2026-11-01; the reference playthrough Part 1 item 2, snatcherLeo's layout).

    python -I tools/local/port/make_portable.py <new empty folder> [--from build-pc/firstlink] [--mingw C:/msys64/mingw64/bin]

The folder gets:
    orrecolo.exe and the DLLs next to it in the build folder (SDL3, Dawn, nod, the shader compiler, png / zlib / zstd)
    the MinGW runtime the exe needs outside MSYS2: libstdc++-6.dll, libwinpthread-1.dll, libgcc_s_seh-1.dll
    initial_pipeline_cache.db with --seed (pipeline_seed.py's bundled GPU pipeline cache; *added 2026-11-03*), else the
        build folder's own launch list (*added 2026-11-11*)
    ui\ (the hub's pages and pictures; the script stops if any is missing; *added 2026-11-11*)
    Fonts\ (every shipped font with its OFL licence, the Noto set included; *moved out of ui\ 2026-11-16*)
    Colosseum\ROM\       (empty: the player's disc image goes here, any name; the exe finds it by game ID GC6E01)
    Colosseum\Saves\Slot 1\  (empty: the save card; backups go to Colosseum\Saves\Backups\ at every launch)
    README.txt            (what goes where)
Nothing from orig/, extracted/ or a disc is copied. *Updated 2026-11-02:* since Part 1 item 2c the exe takes the game's
data from the player's own disc (main.dol's tables, the room modules; exe_disc_scan.py finds none of the content
categories in it); it still carries the disc check's reference list (file names and SHA-1s, no game data). The folder
stays a private test build until snatcherLeo decides to share builds (README says so). Microsoft's Visual C++ runtime (MSVCP140 / VCRUNTIME140, used by SDL,
Dawn, nod and the shader compiler) is not copied: Windows has it system-wide when the official "Visual C++
Redistributable" is installed (this PC has it); on a fresh PC the README points the player to Microsoft's installer.

The folder must not exist or must be empty (the script never overwrites or deletes anything). Prints what it copied.
"""
import argparse
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MAIN = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
MINGW_RUNTIME = ("libstdc++-6.dll", "libwinpthread-1.dll", "libgcc_s_seh-1.dll")

README = """OrrePort (private test build {stamp})
=================================================

PRIVATE TEST BUILD: please don't share this folder yet.

1. Put your own Pokemon Colosseum disc image (USA) in Colosseum\\ROM\\
   Any file name works. Formats: ISO, GCM, CISO or RVZ. The program finds it by the game ID inside (GC6E01).
2. Start orrecolo.exe.
   Windows may say "Windows protected your PC" (the program is not signed): click More info, then Run anyway.
   The first start with a disc image checks every file on it against the original USA disc (a few seconds;
   later starts skip it). If the image is not an exact copy, a message says which files differ and why that
   matters. Dolphin can check a dump too: right-click the game, Properties, Verify, Verify Integrity.

Your save: Colosseum\\Saves\\Slot 1\\ (a Dolphin-style .gci file: it copies to and from Dolphin as it is).
Every start makes a backup first: Colosseum\\Saves\\Backups\\<date_time>\\Slot 1\\. Backups are never deleted
by the program.
Logs: logs\\ (send the newest one with any problem report).
Recordings: Colosseum\\Recordings\\ (every start records your button presses and the game's random numbers,
sitting_<date_time>.orrepad and .rolls; with a problem report, send the pair from that start too: it lets the
session be replayed exactly up to the problem). They hold no game data and nothing about you or your PC.

If Windows says MSVCP140.dll or VCRUNTIME140.dll is missing: install Microsoft's "Visual C++ Redistributable"
(x64) from Microsoft's website, then start again.
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", help="the new portable folder (must not exist, or be empty)")
    ap.add_argument("--from", dest="src", default=os.path.join(MAIN, "build-pc", "firstlink"),
                    help="the build folder with orrecolo.exe and its DLLs")
    ap.add_argument("--mingw", default="C:/msys64/mingw64/bin", help="MSYS2's mingw64 bin folder (the MinGW runtime)")
    ap.add_argument("--seed", help="a bundled GPU pipeline cache (pipeline_seed.py) to put next to the exe as "
                                   "initial_pipeline_cache.db (2026-11-03: removes first-use stalls)")
    a = ap.parse_args()

    out = os.path.abspath(a.out)
    if os.path.exists(out) and os.listdir(out):
        sys.exit(f"{out} exists and is not empty: give a new folder (nothing is overwritten)")
    exe = os.path.join(a.src, "orrecolo.exe")
    if not os.path.isfile(exe):
        sys.exit(f"no orrecolo.exe in {a.src}: build first (ninja -C build-pc, then port/tools/firstlink.py)")
    for name in MINGW_RUNTIME:
        if not os.path.isfile(os.path.join(a.mingw, name)):
            sys.exit(f"{name} not found in {a.mingw} (--mingw)")
    # *Added 2026-11-11:* the hub's and the Settings overlay's pages, pictures and fonts (ui\ next to the exe;
    # firstlink.py copies port/ui and the fonts into the build folder). The main build's first play folder was made
    # by hand without ui\fonts and the hub drew every label blank, so missing pages or fonts now stop the script
    # before anything is made.
    ui = os.path.join(a.src, "ui")
    # *Updated 2026-11-13:* Archivo and Roboto too: the in-game font renderer draws the game's small fonts with
    # them (without them the folder still runs, but fonts 1-4 fall back to the game's own letters)
    # *Moved 2026-11-16 (the Noto wiring):* every shipped font now sits in Fonts\ next to the exe (bin\Fonts\ in the full
    # layout; the decision "Fonts in the portable layout"), with the Noto set; the Noto files are checked too (without
    # them letters the game's fonts lack, emoji and kanji in names, are left out)
    font_dir = os.path.join(a.src, "Fonts")
    fonts = ("Inter-Variable.ttf", "Saira-Variable.ttf", "OFL-Inter.txt", "OFL-Saira.txt",
             "Archivo[wdth,wght].ttf", "Roboto[wdth,wght].ttf", "OFL-Archivo.txt", "OFL-Roboto.txt",
             "NotoSans[wdth,wght].ttf", "NotoSansCJK-Bold.ttc", "NotoSansSymbols[wght].ttf",
             "NotoSansSymbols2-Regular.ttf", "NotoSansThai[wdth,wght].ttf", "NotoSansDevanagari[wdth,wght].ttf",
             "NotoSansGeorgian[wdth,wght].ttf", "NotoSansArmenian[wdth,wght].ttf", "NotoSansArabic[wdth,wght].ttf",
             "NotoSansHebrew[wdth,wght].ttf", "NotoColorEmoji.ttf", "OFL-NotoSans.txt", "OFL-NotoSansCJK.txt",
             "OFL-NotoColorEmoji.txt")
    lacking = [f for f in fonts if not os.path.isfile(os.path.join(font_dir, f))]
    if not os.path.isfile(os.path.join(ui, "hub.rml")) or lacking:
        sys.exit(f"{a.src} lacks the hub's pages or fonts ({', '.join(lacking) or 'ui/hub.rml'}): run "
                 "port/tools/firstlink.py (it copies port/ui and decomp/_external/fonts) and try again")

    os.makedirs(os.path.join(out, "Colosseum", "ROM"), exist_ok=True)
    os.makedirs(os.path.join(out, "Colosseum", "Saves", "Slot 1"), exist_ok=True)
    os.makedirs(os.path.join(out, "Colosseum", "Recordings"), exist_ok=True)
    copied = []
    for name in ["orrecolo.exe"] + sorted(f for f in os.listdir(a.src) if f.lower().endswith(".dll")):
        shutil.copy2(os.path.join(a.src, name), os.path.join(out, name))
        copied.append(name)
    for name in MINGW_RUNTIME:
        shutil.copy2(os.path.join(a.mingw, name), os.path.join(out, name))
        copied.append(name + " (MinGW runtime)")
    # *Added 2026-11-13 (snatcherLeo: "Separate helper (1)"):* the one-click GameCube adapter setup, next to orrecolo.exe
    # in the 0.1 folder (bin\ in the full layout, orreport-plan section 8); its licences come with licenses\ below.
    # Windows only (a Linux / Mac folder gets its own way later, if it needs one).
    helper = "OrrePort Adapter Setup.exe"
    if os.path.isfile(os.path.join(a.src, helper)):
        shutil.copy2(os.path.join(a.src, helper), os.path.join(out, helper))
        copied.append(helper + " (the GameCube adapter setup; Settings > Controls)")
    else:
        print(f"note: no {helper} in {a.src}: the Controls row will say it is missing "
              "(build it: python -I tools/local/port/build_adapter_setup.py)")
    if a.seed:
        shutil.copy2(a.seed, os.path.join(out, "initial_pipeline_cache.db"))
        copied.append("initial_pipeline_cache.db (the bundled pipeline seed)")
    elif os.path.isfile(os.path.join(a.src, "initial_pipeline_cache.db")):
        # *Added 2026-11-11:* the build folder's launch list (the ubershader's 44 pipelines) by default
        shutil.copy2(os.path.join(a.src, "initial_pipeline_cache.db"), os.path.join(out, "initial_pipeline_cache.db"))
        copied.append("initial_pipeline_cache.db (the build folder's launch list)")
    # the build folder's ui\ may still hold an old fonts\ copy from before the move: left out of the portable folder
    shutil.copytree(ui, os.path.join(out, "ui"), ignore=lambda d, names: ["fonts"] if os.path.samefile(d, ui) else [])
    copied.append("ui\\ (the hub's pages and pictures)")
    shutil.copytree(font_dir, os.path.join(out, "Fonts"))
    copied.append("Fonts\\ (every shipped font with its OFL licence: Inter, Saira, Archivo, Roboto, the Noto set)")
    # *Added 2026-11-16 (the project's build helper):* OrreFont Colosseum, the installable font of the game's traced letters
    # (snatcherLeo: shipped in bin\Fonts\ "in case anyone wants to install on their computer"). firstlink.py copies it into
    # the build folder's Fonts\ with the rest, so the copy above carries it; only noted when missing, since the game
    # never reads it (tools/local/orrefont/README.md)
    if os.path.isfile(os.path.join(font_dir, "OrreFont Colosseum.ttf")):
        copied.append("Fonts\\OrreFont Colosseum.ttf (OrreFont, the installable font)")
    else:
        print("note: no OrreFont Colosseum.ttf in the build's Fonts\\: run port/tools/firstlink.py (it copies "
              "port/ui/traced/fonts/)")
    # *Added 2026-11-13:* the shipped libraries' licence texts (libusb's LGPL-2.1 so far; firstlink.py puts them
    # in the build folder's licenses\ next to libusb-1.0.dll, which the .dll loop above already copies). Why: the LGPL
    # asks that its text travels with the library; the folder is where every later shipped library's licence goes too.
    # Platforms: this script builds the Windows folder only; libusb is shipped on Windows (and likely macOS), not on
    # Linux, where the system provides it (firstlink.py copy_libusb says why).
    licenses = os.path.join(a.src, "licenses")
    if os.path.isdir(licenses):
        shutil.copytree(licenses, os.path.join(out, "licenses"))
        copied.append("licenses\\ (" + ", ".join(sorted(os.listdir(licenses))) + ")")
    if not os.path.isfile(os.path.join(a.src, "libusb-1.0.dll")):
        print("WARNING: no libusb-1.0.dll in the build folder: GameCube adapters won't be read (see firstlink.py)")

    stamp = "unknown"
    stamp_h = os.path.join(MAIN, "build-pc", "firstlink", "pc_build_stamp.h")
    if os.path.isfile(stamp_h):
        for line in open(stamp_h, encoding="utf-8", errors="replace"):
            if "ORRE_BUILD_STAMP" in line and '"' in line:
                stamp = line.split('"', 1)[1].rsplit('"', 1)[0]
    with open(os.path.join(out, "README.txt"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write(README.format(stamp=stamp))

    print(f"portable folder: {out}")
    for name in copied:
        print("  copied", name)
    print("  made Colosseum\\ROM\\, Colosseum\\Saves\\Slot 1\\, Colosseum\\Recordings\\, README.txt")
    print("PRIVATE TEST BUILD: not for sharing until snatcherLeo decides")


if __name__ == "__main__":
    main()
