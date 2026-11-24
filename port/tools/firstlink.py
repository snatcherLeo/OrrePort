"""The first link of the whole game for the PC (docs/port/first-link.md; the project's spec 2026-10-15).

Steps (each can be rerun; outputs go to build-pc/firstlink/ and build-pc/generated/, never committed):
  compile   every src/game and src/hsd unit of the GameCube build (`ninja -t commands all_source`), each with its
            own -D defines, plus -DTARGET_PC=1 and pc/prototypes.h (force-included), with GCC (-fpermissive). src/musyx comes from B's library (orre_musyx), the SDK, MSL
            and TRK from Aurora and the host's C library. The game's main() is compiled as orre_game_main.
            Units that don't compile are listed (compile_failures.txt) and left out.
  rename    the SDK address names in the objects -> real names (port/src/sdk_pc_alias.h, applied with objcopy)
  dedupe    one definition per symbol: the project's rule (pcunits.py: the unit whose split range holds the symbol's
            address owns it); every other copy is made local to its object (objcopy --localize-symbols; COFF has
            no ELF-style weak symbols). Symbols whose owner wasn't compiled keep their first copy.
  nodata    leave the generated data out, then link: the undefined data list is what pcdata.py has to make
  data      the project's generators: pcdata.py --need <undefined data> (pc_data_main.c) and pcreldata.py
            (pc_rel_modules.c), compiled.
  link      everything with port/src/pc_game_main.c and the platform libraries (the CMake build's link line for
            pc_audio_test, plus every Aurora library), into build-pc/firstlink/orrecolo.exe. The undefined symbols
            are written to undefined.txt (functions and data apart), the useful output of a first link.
  stubs     the project's pcstubs.py turns the functions still undefined (after a link with the data but without
            stubs) into logging stubs; then link again. nostubs / nodata leave those out of the next link.

Usage (pc-platform repo root, MSYS2 mingw64 and the CMake build-pc already configured and built):
  python -I port/tools/firstlink.py [compile] [dedupe] [data] [link] [stubs] [--jobs N] [--full]   (default: all)

Incremental since 2026-10-19 (snatcherLeo via Main: ~8 minutes for every one-line edit): compile redoes only the units whose
command, source, included headers (GCC's -MMD list) or symbols.txt changed; rename touches only those new objects;
the symbol lists (nm) are cached per object; and the default run links once when no object's or library's global
names, the generators, symbols.txt or pc_game_main.c changed since the last full build (link_inputs.sig), else it runs
the full data / stubs sequence. --full compiles every unit and redoes everything, as before.
"""
import argparse
import atexit
import concurrent.futures
import datetime
import hashlib
import json
import os
import threading
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(".")
OUT = Path("build-pc/firstlink")
OBJ = OUT / "obj"
GEN = Path("build-pc/generated")
FLAGS = ["-c", "-O1", "-g", "-w", "-fpermissive", "-fno-strict-aliasing", "-fwrapv", "-DTARGET_PC=1",
         # the project's decision (2026-10-15): the alias headers force-included before prototypes.h, as pc_probe.py --alias
         "-include", "port/src/sdk_pc_alias.h", "-include", "include/pc/game_pc_alias.h",
         "-include", "include/pc/prototypes.h",
         "-Iinclude", "-Ibuild/GC6E01/include", "-I.", "-Iextern/aurora/include", "-Iport/src"]
# ORRE_FIRSTLINK_CFLAGS: extra compile flags for every unit, for experiments (2026-10-19: "-mfma" to test whether
# fused multiply-adds, as the GameCube's compiler emits for a*b+c, make S02's NPC walking match Dolphin). A change
# recompiles every unit (the command is part of the incremental check).
FLAGS += os.environ.get("ORRE_FIRSTLINK_CFLAGS", "").split()

sys.path.insert(0, str(Path("tools/local/port").resolve()))

# SDK units the PC compiles from our own C instead of taking Aurora's (docs/port/first-link.md):
#  - OSAlloc: game code walks the heap's internals (gs_range_800E0DDC.c reads the heap array HeapArray), so the
#    heap must be the decompiled allocator's (2026-10-15); software over MEM1, not hardware;
#  - the THP movie player (reader / decoder threads, drawing); Aurora provides only the decoder (sdk-address-names.md)
SDK_UNITS = {
    "src/dolphin/sdk_range_8009A9D8.c", "src/dolphin/sdk_exact_8009ABD0.c", "src/dolphin/sdk_candidate_8009AC50.c",
    "src/dolphin/thp/THPPlayer.c", "src/dolphin/thp/THPRead.c", "src/dolphin/thp/THPDraw.c",
    "src/dolphin/thp/THPAudioDecode.c", "src/dolphin/thp/THPVideoDecode.c",
    "port/src/pc_sdk_gaps.c",  # OSAlloc's free-list insert, wcscpy (own types: no forced game headers)
}


# port/ files that need the game's headers: compiled with the game units (same flags)
PORT_GAME_SOURCES = ["port/src/pc_gx_arrays.c", "port/src/pc_sdk_gaps.c",
                     "src/game/pc_debug_game.c",  # the debug toolkit's game side (PC only, not in configure.py)
                     "src/game/pc_msgctrl_digit.c",  # _msgctrlMakeDigit written as C for the PC (numbers in text)
                     "src/game/pc_game_ui.c",  # game functions for OrrePort's parts of the game's screens (pc_game_ui.h)
                     # ORRE_WAZA_VIEWER: boot into the developers' move viewer for Studio's reference shots (2026-11-18)
                     "src/game/pc_waza_viewer.c",
                     # ORRE_BCAM_LOG: the battle camera's shots, logged for Studio's camera check (2026-11-19)
                     "src/game/pc_bcam_log.c"]

# ORRE_SAVE_TRACE_BUILD=1 (2026-10-18): a test build that records the width of every save-block access
# (port/src/pc_save_trace.c; the Dolphin-save converter's field table). Game units get the compiler's per-access
# calls; never use this folder's exe for playing. Build it in its own worktree.
SAVE_TRACE = os.environ.get("ORRE_SAVE_TRACE_BUILD") == "1"
SAVE_TRACE_FLAGS = ["-fsanitize=kernel-address", "--param", "asan-stack=0", "--param", "asan-globals=0",
                    "--param", "asan-instrumentation-with-call-threshold=0"]
if SAVE_TRACE:
    PORT_GAME_SOURCES.append("port/src/pc_save_trace.c")

# The hub's fonts (Inter, Saira; SIL Open Font License, which allows shipping them with the program alongside the
# licence text). Kept outside the repo in decomp/_external/fonts/<name>/; the hub and the Settings overlay read them
# from Fonts/ next to the exe (ui/fonts/ until 2026-11-16). Added 2026-11-11: the main build's first play folder
# had no fonts and the hub drew every label blank.
# Archivo and Roboto (added 2026-11-13): the in-game font renderer's part C draws the game's small fonts 1-4 with
# them (port/src/pc_font_render.cpp; snatcherLeo's option A). Variable fonts (width and weight axes), SIL OFL, shipped on every
# OS; the renderer finds them in Fonts/ next to the exe. Without them fonts 1-4 keep the game's own letters.
# Google's Noto set (added 2026-11-16, the project's helper; decided 2026-11-15, snatcherLeo: "Yes with emoji"): the last-resort
# fonts behind the game's part D and the hub's pages, the same on every OS (port/src/pc_font_files.cpp lists the chain
# and says why; docs/port/font-renderer.md "Start here"). SIL OFL 1.1, each with its OFL.txt, kept outside the repo in
# decomp/_external/fonts/noto-*/ like the others (sources in docs/licensing.md); about 53 MB (*widened 2026-11-16*,
# snatcherLeo: "Let's add all the missing noto faces now": one font for every living script Noto covers). Every noto-*
# folder there is shipped, one font file and its OFL.txt each, except noto-cjk-jp / -kr / -sc / -tc (the four
# separate region files: the kanji review's reference only; the game ships the one NotoSansCJK-Bold.ttc collection).
# Missing ones are only warned about: the renderer and the hub skip a font they can't open (a letter only that font has
# is then left out). The chain itself (which file, in which order) is port/src/pc_font_files.cpp.
# Moved 2026-11-16 with every shipped font from ui/fonts/ to Fonts/ next to the exe (app\Fonts\ in the portable layout:
# the decision "Fonts in the portable layout"); the build folder's old ui/fonts/ copy is no longer read.
NOTO_KEY_FILES = ["NotoSans[wdth,wght].ttf", "NotoSansCJK-Bold.ttc", "NotoColorEmoji.ttf"]


def downloaded(*parts):
    """Where a downloaded third-party file lives (2026-11-24, snatcherLeo's option C): deps/ inside the repo (git-ignored, filled
    by tools/fetch_deps.py with pinned checksums), else decomp/_external/ beside the repo (this PC's older place for the
    same files). The four small source libraries are vendored in extern/ instead and never looked for here."""
    for root in (Path("deps"), Path("..") / "_external"):
        if root.joinpath(*parts).exists():
            return root.joinpath(*parts)
    return Path("deps").joinpath(*parts)


def noto_fonts():
    """(folder, font file, licence name) for every shipped noto-* folder in the fonts folder (downloaded())"""
    out = []
    root = downloaded("fonts")
    for d in sorted(root.glob("noto-*")):
        if not d.is_dir() or d.name.startswith("noto-cjk-"):
            continue
        fonts = sorted(p.name for p in d.iterdir() if p.suffix.lower() in (".ttf", ".ttc", ".otf"))
        for f in fonts:
            out.append((d.name, f, "OFL-" + f.split("[")[0].split("-")[0].split(".")[0] + ".txt"))
    return out
UI_FONTS = [("inter", "Inter-Variable.ttf", "OFL-Inter.txt"), ("saira", "Saira-Variable.ttf", "OFL-Saira.txt"),
            ("archivo", "Archivo[wdth,wght].ttf", "OFL-Archivo.txt"), ("roboto", "Roboto[wdth,wght].ttf", "OFL-Roboto.txt")]


def copy_ui_fonts(dest):
    """Every shipped font with its licence into dest (Fonts/ next to the exe). A file already there with the same size
    is left alone, so a relink doesn't copy 53 MB each time. Platform-neutral: every OS's folder gets the same files."""
    import shutil
    src_root = downloaded("fonts")
    dest.mkdir(parents=True, exist_ok=True)
    missing = []
    noto = noto_fonts()
    noto_missing = [k for k in NOTO_KEY_FILES if k not in {f for _, f, _ in noto}]
    for fonts, lacking in ((UI_FONTS, missing), (noto, noto_missing)):
        for folder, font, licence in fonts:
            src = src_root / folder
            if (src / font).exists() and (src / "OFL.txt").exists():
                if not (dest / font).exists() or (dest / font).stat().st_size != (src / font).stat().st_size:
                    shutil.copyfile(src / font, dest / font)
                shutil.copyfile(src / "OFL.txt", dest / licence)
            elif not (dest / font).exists():
                lacking.append(str(src / font))
    if missing:
        print("WARNING: the hub's fonts are missing (" + ", ".join(missing) + "): the hub and the Settings overlay "
              "will show no text. Run python -I tools/fetch_deps.py (it downloads them into deps/fonts/).")
    if noto_missing:
        print("WARNING: Noto fallback fonts are missing (" + ", ".join(noto_missing) + "): letters only they have are "
              "left out. Run python -I tools/fetch_deps.py (it downloads them into deps/fonts/).")


def copy_orrefont(dest):
    """The OrreFont font files (port/ui/traced/fonts/*.ttf: "OrreFont Colosseum") into dest (Fonts/ next to the exe,
    app\\Fonts\\ in the portable layout). Added 2026-11-16 (the project's build helper). Why: snatcherLeo decided to ship OrreFont as
    an installable font ("just for fun ... in case anyone wants to install on their computer", 2026-11-15; named
    "OrreFont Colosseum" 2026-11-16). The game's renderer does not read it (it traces the player's disc at run time);
    it is there for players and modders to install, and for Studio's previews later. Made by
    tools/local/orrefont/orrefont.py and committed in the traced-shapes folder (the commit hook allows it there only).
    Platform-neutral: the same file on every OS (a TrueType font installs on Windows, Linux and macOS alike).
    *Removed 2026-11-23 (snatcherLeo: "maybe I should not include it within our final repo. Can you ensure it's taken out? I
    already made a copy"):* the font file is no longer in the repo or the build, so nothing is copied; nothing reads it
    (the game traces the player's disc; Studio never used it). tools/local/orrefont/orrefont.py can still make it."""
    import shutil
    src = Path("port") / "ui" / "traced" / "fonts"
    if not src.is_dir() or not any(src.glob("*.ttf")):
        return  # (no note: its absence is on purpose now)
    fonts = sorted(src.glob("*.ttf")) if src.is_dir() else []
    if not fonts:
        print("note: no OrreFont file in port/ui/traced/fonts/ (python -I tools/local/orrefont/orrefont.py build)")
    dest.mkdir(parents=True, exist_ok=True)
    for f in fonts:
        if not (dest / f.name).exists() or (dest / f.name).stat().st_size != f.stat().st_size \
                or (dest / f.name).stat().st_mtime < f.stat().st_mtime:
            shutil.copyfile(f, dest / f.name)


# libusb (LGPL-2.1, the official release's MinGW64 DLL, unchanged; docs/licensing.md): SDL loads it at run time to read
# Nintendo's GameCube adapter once the adapter has the WinUSB driver. Kept outside the repo in decomp/_external/libusb/
# (libusb-1.0.dll and COPYING, from release v1.0.30); copied next to the exe with its licence in licenses/. Added
# 2026-11-13, after snatcherLeo's adapter test ("It works").
# Why: Nintendo's adapter is not a standard game controller (it was made for the Wii U, which talks to it over raw
# USB), so SDL can only read it through libusb, and Windows has no libusb of its own: without this DLL next to the exe
# the adapter is never seen, even with the driver in place (verified on snatcherLeo's PC).
# Platforms: Windows only, both halves. Linux (likely): libusb comes with the system (libusb-1.0.so), so nothing is
# shipped and no driver is swapped; the player needs a one-time udev permission rule for USB 057E:0337 instead. macOS
# (likely): no driver step either, but libusb is not part of the OS, so a Mac build would ship libusb-1.0.dylib the
# same way this ships the DLL. The WinUSB driver itself is installed outside OrrePort (Zadig today, the planned
# one-click setup later; docs/port/controls.md "The GameCube adapter").
def copy_picture_licences(dest):
    """The disc art page's picture libraries' licences into licenses/ (2026-11-13; snatcherLeo approved both libraries).
    Why: libwebp (BSD-3) asks that its notice travels with any binary that contains it, and it is linked into the exe;
    stb_image (public domain / MIT) asks nothing, but the credit costs nothing. All three are vendored in extern/
    (*changed 2026-11-24*, snatcherLeo's option C; they were in decomp/_external/) and required by port/CMakeLists.txt, so a
    missing licence file is an error. Platform-neutral: every OS's folder carries the same texts."""
    import shutil
    ext = Path("extern")
    pairs = [(ext / "libwebp-1.5.0" / "COPYING", "libwebp-COPYING.txt"),
             (ext / "libwebp-1.5.0" / "PATENTS", "libwebp-PATENTS.txt"),
             (ext / "stb" / "LICENSE", "stb_image-LICENSE.txt"),
             # the updater's signature check (Monocypher 4.0.3, BSD-2 or CC-0; snatcherLeo approved it 2026-11-23): the BSD
             # option asks that its notice travels with the binary
             (ext / "monocypher-4.0.3" / "LICENCE.md", "Monocypher-LICENCE.txt")]
    (dest / "licenses").mkdir(parents=True, exist_ok=True)
    for src, name in pairs:
        if not src.exists():
            raise SystemExit(f"licences: {src} is missing (the vendored library's own licence must ship with it)")
        shutil.copyfile(src, dest / "licenses" / name)


def copy_library_licences(dest):
    """The licences of libraries built into the exe from the CMake build's fetched sources, into licenses/ (the project's build
    helper, 2026-11-16). Why: HarfBuzz (Old MIT) and SheenBidi (Apache 2.0), added for text shaping, ask that their
    licence travels with the binary; FreeType (built into the exe, the FTL chosen, whose credit line is also in the
    About window) and libpng (shipped as Aurora's DLL and read by FreeType for colour emoji) were noted as missing in
    docs/licensing.md; RmlUi's (MIT) covers the HarfBuzz font engine OrrePort compiles from RmlUi's sample. A missing
    file (a build before CMake fetched it) is only noted. Platform-neutral: every OS's folder carries the same texts."""
    import shutil
    deps = Path("build-pc") / "_deps"
    # HarfBuzz's src/ms-use/COPYING (Microsoft's MIT licence) covers the Universal Shaping Engine data its generated
    # tables come from, so it travels too
    pairs = [(deps / "orre_harfbuzz-src" / "COPYING", "HarfBuzz-COPYING.txt"),
             (deps / "orre_harfbuzz-src" / "src" / "ms-use" / "COPYING", "HarfBuzz-ms-use-COPYING.txt"),
             (deps / "orre_sheenbidi-src" / "LICENSE", "SheenBidi-LICENSE.txt"),
             (deps / "freetype-src" / "LICENSE.TXT", "FreeType-LICENSE.txt"),
             (deps / "freetype-src" / "docs" / "FTL.TXT", "FreeType-FTL.txt"),
             (deps / "png-src" / "LICENSE", "libpng-LICENSE.txt"),
             (deps / "rmlui-src" / "LICENSE.txt", "RmlUi-LICENSE.txt")]
    (dest / "licenses").mkdir(parents=True, exist_ok=True)
    for src, name in pairs:
        if src.exists():
            shutil.copyfile(src, dest / "licenses" / name)
        else:
            print(f"note: {src} missing: its licence isn't in licenses/ (run the CMake build first)")


def copy_adapter_setup(dest):
    """"OrrePort Adapter Setup.exe" (the one-click GameCube adapter setup, Settings > Controls, 2026-11-13, snatcherLeo:
    "Separate helper (1)") and its licences, from build-pc/adapter-setup/ when tools/local/port/build_adapter_setup.py
    has built it. Why separate: it contains libwdi (LGPL v3) and runs as administrator, neither of which belongs in the
    game's exe. Windows only; without it the Controls row says the program is missing."""
    import shutil
    src = Path("build-pc") / "adapter-setup" / "OrrePort Adapter Setup.exe"
    if not src.exists():
        print("note: OrrePort Adapter Setup.exe not built (python -I tools/local/port/build_adapter_setup.py)")
        return
    shutil.copyfile(src, dest / src.name)
    lic = dest / "licenses"
    lic.mkdir(parents=True, exist_ok=True)
    for s, name in [(Path("extern") / "libwdi-1.5.1" / "COPYING-LGPL", "libwdi-COPYING-LGPL-3.txt"),
                    (Path("extern") / "libwdi-1.5.1" / "COPYING", "libwdi-COPYING-GPL-3.txt"),
                    (downloaded("dl", "wdk8-redist", "x", "Windows Kits", "8.0", "redist", "wdf", "License.rtf"),
                     "Microsoft-WDF-1.11-redistributable-License.rtf"),
                    (src.parent / "libwdi-SOURCE.txt", "libwdi-SOURCE.txt")]:
        if s.exists():
            shutil.copyfile(s, lic / name)


def copy_libusb(dest):
    import shutil
    src = downloaded("libusb")
    if (src / "libusb-1.0.dll").exists() and (src / "COPYING").exists():
        shutil.copyfile(src / "libusb-1.0.dll", dest / "libusb-1.0.dll")
        (dest / "licenses").mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src / "COPYING", dest / "licenses" / "libusb-COPYING.txt")
    elif not (dest / "libusb-1.0.dll").exists():
        print("WARNING: libusb is missing (deps/libusb/libusb-1.0.dll and COPYING): GameCube adapters won't be read. "
              "Run python -I tools/fetch_deps.py (libusb's v1.0.30 release, checksum-pinned).")


def units():
    cmds = subprocess.run(["ninja", "-t", "commands", "all_source"], capture_output=True, text=True).stdout
    out, seen = [], set()
    for c in cmds.splitlines():
        if "mwcceppc" not in c:
            continue
        m = re.search(r" -c (\S+) -o \S+", c)
        if not m:
            continue
        src = m.group(1).replace("\\", "/")
        if not (src.startswith(("src/game/", "src/hsd/")) or src in SDK_UNITS) or src in seen:
            continue
        seen.add(src)
        defs = re.findall(r"(?<=\s)-D(\S+)", c)
        # a .c source is C on the PC even when its GameCube unit compiles it as C++ for the byte match
        # (menuPokemonChange_r51_8002DF10_suffix.c, Count-Sheep merge 2026-10-24): g++ would reject the C file's
        # implicit pointer conversions and the PC prototypes' unprototyped forms
        lang = "c++" if src.endswith((".cpp", ".cp")) or (re.search(r"-lang[ =]c\+\+", c) and not src.endswith(".c")) \
            else "c"
        out.append((src, defs, lang))
    for src in PORT_GAME_SOURCES:
        out.append((src, [], "c"))
    return out


def obj_path(src):
    if src.startswith("port/"):
        return OBJ / "port" / (Path(src).stem + ".o")
    return OBJ / (src[len("src/"):].rsplit(".", 1)[0] + ".o")


def compile_one(u):
    src, defs, lang = u
    obj = obj_path(src)
    obj.parent.mkdir(parents=True, exist_ok=True)
    extra = ["-Dmain=orre_game_main"] if src == "src/game/main.c" else []
    # our own SDK units keep their own declarations and types: no forced alias / prototype headers (the rename step
    # maps their address names to the real names afterwards)
    # and port/sdk_include first: Aurora's SDK structs where Aurora writes into them (DVDFileInfo, see its dvd/dvd.h)
    flags = ["-Iport/sdk_include"] + [f for f in FLAGS if f not in ("-include", "port/src/sdk_pc_alias.h",
                                                                     "include/pc/game_pc_alias.h",
                                                                     "include/pc/prototypes.h")] \
        if src in SDK_UNITS else FLAGS
    if SAVE_TRACE and src.startswith("src/game/"):
        flags = flags + SAVE_TRACE_FLAGS
    cmd = (["gcc"] + flags + ["-x", lang, "-std=gnu11" if lang == "c" else "-std=gnu++17"] + [f"-D{d}" for d in defs] +
           extra + [src, "-o", str(obj)])
    # Incremental (2026-10-19, snatcherLeo via Main: every relink recompiled all ~1,780 units, ~8 minutes for a one-line edit):
    # a unit is compiled again only when its command, its source, a header it includes (GCC's dependency list, -MMD),
    # or symbols.txt (the rename step's names) changed since its last compile. The unit's stamp (<obj>.cmd) holds the
    # command and is written after the compile; its time is the compile's. A failed unit is retried every run.
    stamp, deps = Path(str(obj) + ".cmd"), Path(str(obj) + ".d")
    key = "\n".join(cmd)
    if not FULL and up_to_date(obj, stamp, deps, key):
        return src, True, [], False
    stamp.unlink(missing_ok=True)
    r = subprocess.run(cmd + ["-MMD", "-MF", str(deps)], capture_output=True, text=True, errors="replace")
    errors = [l for l in r.stderr.splitlines() if ": error: " in l]
    if r.returncode == 0:
        stamp.write_text(key, encoding="utf-8")
    return src, r.returncode == 0, errors, True


FULL = False  # --full: compile every unit again (and rename every object), as before 2026-10-19
WINDOWED = False  # --windowed: no console window (the hub trial's player build)
_SYMBOLS_TIME = None


def up_to_date(obj, stamp, deps, key):
    global _SYMBOLS_TIME
    if not (obj.exists() and stamp.exists() and deps.exists()):
        return False
    try:
        if stamp.read_text(encoding="utf-8") != key:
            return False
        built = stamp.stat().st_mtime_ns
        if _SYMBOLS_TIME is None:
            _SYMBOLS_TIME = Path("config/GC6E01/symbols.txt").stat().st_mtime_ns
        if _SYMBOLS_TIME > built:
            return False
        text = deps.read_text(encoding="utf-8", errors="replace").replace("\\\n", " ")
        files = text.split(":", 1)[1].split() if ":" in text else []
        for f in files:
            if f == "\\":
                continue
            p = Path(f.replace("\\ ", " "))
            if not p.exists() or p.stat().st_mtime_ns > built:
                return False
    except OSError:
        return False
    return True


def step_compile(jobs):
    us = units()
    ok, failed, fresh = [], [], []
    last = OUT / "compiled.txt"
    compiled_before = set(last.read_text(encoding="utf-8").split()) if last.exists() else set()
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        for src, good, errors, compiled in ex.map(compile_one, us):
            (ok if good else failed).append((src, errors))
            if good and compiled:
                fresh.append(src)
    OUT.mkdir(parents=True, exist_ok=True)
    # the objects compiled this run: the rename step renames only these (renaming is not repeatable: some names chain,
    # e.g. an address name -> CARDOpen and CARDOpen -> CARDFastOpen, so a second pass would rename again)
    # (added to the list, not replacing it: a compile run whose rename never ran leaves objects still to rename)
    pending = OUT / "fresh.txt"
    waiting = set(pending.read_text(encoding="utf-8").split()) if pending.exists() else set()
    pending.write_text("".join(f"{s}\n" for s in sorted(waiting | set(fresh))), encoding="utf-8")
    # A unit that compiled last time and fails now is left out and its functions become stubs, which looks like a
    # working build (2026-10-17: a typo in floor_data.c stubbed the whole file; the game crashed at the first
    # room). Name every such unit loudly.
    previous = OUT / "compiled.txt"
    if previous.exists():
        before = set(previous.read_text(encoding="utf-8").split())
        for s, errors in sorted(failed):
            if str(s) in before:
                print(f"NEW COMPILE FAILURE (compiled last time, now left out as stubs): {s}")
                for e in errors[:3]:
                    print(f"    {e}")
    (OUT / "compiled.txt").write_text("".join(f"{s}\n" for s, _ in sorted(ok)), encoding="utf-8")
    with open(OUT / "compile_failures.txt", "w", encoding="utf-8") as f:
        for s, errors in sorted(failed):
            f.write(f"{s}: {len(errors)} errors\n")
            for e in errors[:5]:
                f.write(f"    {e}\n")
    print(f"compile: {len(us)} units, {len(ok)} compiled ({len(fresh)} this run, {len(ok) - len(fresh)} up to date), "
          f"{len(failed)} failed (compile_failures.txt)")
    # a unit that compiled before and is now left out may have owned symbols that other objects' copies were made local
    # for (dedupe): those copies would stay local and the names undefined. Rare: say so and rebuild from scratch.
    return bool(compiled_before - {s for s, _ in ok}) and not FULL


_NM_CACHE = None  # path -> [mtime_ns, size, names]: an object's global names until the file changes (2026-10-19)
_NM_LOCK = threading.Lock()


def _nm_cache():
    global _NM_CACHE
    if _NM_CACHE is None:
        try:
            _NM_CACHE = json.loads((OUT / "nm_cache.json").read_text(encoding="utf-8"))
        except (OSError, ValueError):
            _NM_CACHE = {}
        atexit.register(lambda: (OUT / "nm_cache.json").write_text(json.dumps(_NM_CACHE), encoding="utf-8")
                        if OUT.exists() else None)
    return _NM_CACHE


def defined_globals(obj):
    cache = _nm_cache()
    key = os.path.abspath(str(obj))
    try:
        st = os.stat(key)
    except OSError:
        return []
    with _NM_LOCK:
        hit = cache.get(key)
    if hit is not None and hit[0] == st.st_mtime_ns and hit[1] == st.st_size:
        return list(hit[2])
    r = subprocess.run(["nm", "-g", str(obj)], capture_output=True, text=True)
    out, undefined = [], []
    for line in r.stdout.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[1] in "TDBRCV":
            out.append(parts[2])
        elif len(parts) == 2 and parts[0] == "U":
            undefined.append(parts[1])
    if r.returncode == 0:
        with _NM_LOCK:
            cache[key] = [st.st_mtime_ns, st.st_size, out, sorted(set(undefined))]
    return out


def undefined_globals(obj):
    """the names an object (or library) uses but doesn't define (cached with defined_globals)"""
    defined_globals(obj)
    with _NM_LOCK:
        hit = _nm_cache().get(os.path.abspath(str(obj)))
    return list(hit[3]) if hit is not None and len(hit) > 3 else []


def link_inputs_signature():
    """Everything the generated data and stubs depend on (2026-10-19): each object's and library's global names
    (defined and used), the generators, symbols.txt and pc_game_main.c. The same signature as the last full build =
    the generated data and stubs still fit, so one link is enough."""
    objs = [str(obj_path(s)) for s in (OUT / "compiled.txt").read_text(encoding="utf-8").split() if s]
    libs = sorted(str(p) for p in Path("build-pc").glob("*.a")) + sorted(str(p) for p in Path("build-pc/aurora").glob("*.a"))
    h = hashlib.sha256()
    for o, names in sorted(all_defined_globals(objs + libs).items()):
        h.update(f"{os.path.basename(o)}|{','.join(sorted(names))}|{','.join(undefined_globals(o))}\n".encode())
    # + the generators' input lists (2026-10-25: an override added to pcdata_types.txt was ignored by the default run,
    # which linked the old generated data)
    for f in sorted(Path("tools/local/port").glob("*.py")) + sorted(Path("tools/local/port").glob("*.txt")) + [
            Path("config/GC6E01/symbols.txt"), Path("port/src/pc_game_main.c")]:
        if f.exists():
            h.update(f.name.encode() + hashlib.sha256(f.read_bytes()).digest())
    # + the generators' switches (2026-11-01: a release link after an ORRE_PCDATA_SELFTEST=1 build reused the
    # self-test data, since nothing above had changed)
    for v in ("ORRE_PCDATA_SELFTEST", "ORRE_REL_PREBUILT"):
        h.update(f"{v}={os.environ.get(v, '')}\n".encode())
    return h.hexdigest()


def all_defined_globals(objs, jobs=16):
    """defined_globals for many objects at once (in parallel; cached ones cost nothing)"""
    with concurrent.futures.ThreadPoolExecutor(min(16, jobs)) as ex:
        return dict(zip(objs, ex.map(defined_globals, objs)))


def step_rename():
    """The SDK address names -> real names (port/src/sdk_pc_alias.h's #define lines), applied to the compiled objects
    (objcopy --redefine-syms) rather than by force-including the header: a unit's own declaration of fn_800BD640
    would otherwise become a second, slightly different declaration of Aurora's GXSetViewportJitter."""
    # any game-side name (2026-10-18: since upstream's renames the game calls some SDK functions by real names that
    # the PC routes elsewhere: CARDOpen -> CARDFastOpen, GXSetArray -> the port's 3-argument wrapper, ...)
    pairs = re.findall(r"^#define ([A-Za-z_]\w*) ([A-Za-z_]\w*)", Path("port/src/sdk_pc_alias.h").read_text(encoding="utf-8"),
                       re.M)
    # Game code also calls functions by address name although symbols.txt names that address (fn_8001D834 ->
    # menuSubCalcColor): on the GameCube dtk's map joins them; here the same renaming does (the project's
    # include/pc/game_pc_alias.h has the same 800-odd pairs as #defines, which turns local declarations into clashes)
    # MusyX's API under its GameCube address names, which symbols.txt leaves unnamed: the game calls these, and the
    # port's orre_musyx library provides them under the real names (identified from src/musyx/musyx_range_80157280.c's
    # bodies and parameters; the first boots stubbed them, so sound init did nothing, 2026-10-16)
    pairs += [("fn_8015FE88", "sndInit"), ("fn_80159EF0", "sndPushGroup"), ("fn_8015A368", "sndSeqPlayEx"),
              ("fn_801631AC", "sndSetHooks"), ("fn_8015FFD4", "sndIsInstalled")]
    sdk = {a for a, b in pairs}
    named = {}
    for line in Path("config/GC6E01/symbols.txt").read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\S+) = \.\w+:0x([0-9A-F]{8}); // type:function", line)
        if m and not m.group(1).startswith("fn_"):
            named[f"fn_{m.group(2)}"] = m.group(1)
    # (2026-10-18) the port's kept wrappers keep their address names: upstream named these addresses after the real SDK
    # functions, which are Aurora's (the wrappers call them), so the symbols.txt pair would turn each wrapper into a
    # second definition of Aurora's function. The game's calls (now by the real name) are routed to the wrappers below.
    kept = {"fn_800B84E0": "GXSetArray",  # port/src/pc_gx_arrays.c: the game's 3-argument GXSetArray
            "fn_800B90A4": "GXSetDrawDoneCallback",  # port/src/pc_gx_hooks.c: draw done on the game thread
            "fn_800CAA3C": "wcscpy"}  # port/src/pc_sdk_gaps.c: the game's 16-bit copy (keeps libc's wcscpy untouched)
    taken = {b for a, b in pairs}
    for a, b in sorted(named.items()):
        if a not in sdk and a not in kept and b not in taken:  # objcopy refuses two names renamed to one; SDK first
            pairs.append((a, b))
            taken.add(b)
    lst = OUT / "sdk_names.redefine"
    lst.write_text("".join(f"{a} {b}\n" for a, b in pairs), encoding="utf-8")
    route = OUT / "kept_wrappers.redefine"
    route.write_text("".join(f"{b} {a}\n" for a, b in kept.items()), encoding="utf-8")
    # only the objects compiled this run (fresh.txt): the others were renamed when they were compiled, and renaming
    # twice is wrong (step_compile)
    fresh = OUT / "fresh.txt"
    listed = fresh if fresh.exists() else OUT / "compiled.txt"
    srcs = [x for x in listed.read_text(encoding="utf-8").split() if x]
    fresh.write_text("", encoding="utf-8")  # renamed now: a second rename step in the same build does nothing
    objs = [str(obj_path(x)) for x in srcs]
    game_objs = [str(obj_path(x)) for x in srcs if not x.startswith("port/")]  # never the wrappers' own files

    def rename(o):
        subprocess.run(["objcopy", f"--redefine-syms={lst}", o], check=True)
        if o in game_objs:
            subprocess.run(["objcopy", f"--redefine-syms={route}", o], check=True)
    with concurrent.futures.ThreadPoolExecutor(min(16, os.cpu_count() or 8)) as ex:  # at most 16 (snatcherLeo, 2026-10-16)
        list(ex.map(rename, objs))
    print(f"rename: {len(pairs)} address names -> real names (SDK + symbols.txt) in {len(objs)} objects; "
          f"{len(kept)} kept wrappers routed in {len(game_objs)} game objects")


def step_dedupe():
    import pcunits
    own = pcunits.owners()
    compiled = [l for l in (OUT / "compiled.txt").read_text(encoding="utf-8").split() if l]
    unit_of = {str(obj_path(s)): s[len("src/"):] for s in compiled}
    definers = defaultdict(list)
    for obj, names in all_defined_globals(list(unit_of)).items():
        for name in names:
            definers[name].append(obj)
    localize = defaultdict(list)
    dup = no_owner = 0
    for name, objs in definers.items():
        if len(objs) < 2 or name.startswith((".", "__real@", "__xmm@", "__imp_")):
            continue  # compiler-made per-object entries (MinGW's .refptr.X slots, float constants): COMDAT, keep
        dup += 1
        owner = own.get(name)
        keep = next((o for o in objs if unit_of[o] == owner), None)
        if keep is None:
            no_owner += 1
            keep = sorted(objs)[0]
        for o in objs:
            if o != keep:
                localize[o].append(name)
    for obj, names in localize.items():
        lst = Path(obj + ".localize")
        lst.write_text("\n".join(names) + "\n", encoding="utf-8")
        subprocess.run(["objcopy", f"--localize-symbols={lst}", obj], check=True)
    print(f"dedupe: {dup} symbols defined in more than one object ({no_owner} without a compiled owner: first copy "
          f"kept); {sum(len(v) for v in localize.values())} copies made local in {len(localize)} objects")


def step_data():
    GEN.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, "-I", "tools/local/port/dataaliases.py"], capture_output=True, text=True)
    need = OUT / "data_need.txt"  # from the last link without generated data (step nodata)
    args = ["--need", str(need)] if need.exists() and need.stat().st_size else []
    # --defined: what B's libraries define with real types (MusyX state and tables, the platform), so pcdata's
    # closure leaves them to those definitions
    lib_defined = set()
    for lib in ("liborre_musyx.a", "liborre_audio.a", "liborre_platform_vi.a", "liborre_platform.a",
                "liborre_gs_thread_host.a"):
        if (Path("build-pc") / lib).exists():
            lib_defined.update(defined_globals(Path("build-pc") / lib))
    (OUT / "lib_defined.txt").write_text("\n".join(sorted(lib_defined)) + "\n", encoding="utf-8")
    args += ["--defined", str(OUT / "lib_defined.txt")]
    # --link-defined: what the compiled game objects really define (after dedupe: only global copies count), so names
    # whose C exists only in gated blocks, unbuilt files or units that failed to compile get generated
    link_defined = set()
    for names in all_defined_globals([obj_path(x) for x in (OUT / "compiled.txt").read_text(encoding="utf-8").split()
                                      if x and obj_path(x).exists()]).values():
        link_defined.update(names)
    (OUT / "link_defined.txt").write_text("\n".join(sorted(link_defined)) + "\n", encoding="utf-8")
    args += ["--link-defined", str(OUT / "link_defined.txt")]
    # discmanifest.py (2026-11-02): the whole-disc check's reference list (pc_disc_check.c)
    for tool, extra in (("pcdata.py", args), ("pcreldata.py", []), ("nativethunks.py", []), ("discmanifest.py", [])):
        r = subprocess.run([sys.executable, "-I", f"tools/local/port/{tool}"] + extra, capture_output=True, text=True)
        print(f"data: {tool}: {'ok' if r.returncode == 0 else 'FAILED'} {r.stdout.strip().splitlines()[-1:]}")
        if r.returncode != 0:
            print(r.stderr[-1500:])
            # (2026-10-18) stop: linking on would reuse the previous run's generated data (stale, and silently)
            raise SystemExit(f"data: {tool} failed: not linking with stale generated data")
    for c in ("pc_data_main.c", "pc_rel_modules.c", "pc_native_thunks.c", "pc_disc_manifest.c"):
        if (GEN / c).exists():
            r = subprocess.run(["gcc", "-c", "-O1", "-w", "-fpermissive", "-DTARGET_PC=1", "-Iinclude", "-I.",
                                str(GEN / c), "-o", str(OUT / (c[:-2] + ".o"))], capture_output=True, text=True)
            print(f"data: compile {c}: {'ok' if r.returncode == 0 else 'FAILED'}")
            if r.returncode != 0:
                print("\n".join(l for l in r.stderr.splitlines() if "error" in l)[:2000])
    localize_generated_duplicates()
    make_placeholders()


def make_placeholders():
    """Data the game names but nobody can provide (not in symbols.txt, no bytes in main.dol: pcdata.py's
    "no original bytes found"): a zeroed, 32-byte aligned 256-byte block each, so the program links. Listed in
    placeholders.txt; every one is a game-code question (a wrong name or a missing symbol), not data."""
    report = GEN / "unsure_data.txt"
    need = OUT / "data_need.txt"
    wanted = set(need.read_text(encoding="utf-8").split()) if need.exists() else set()
    names = []
    if report.exists():
        for line in report.read_text(encoding="utf-8").splitlines():
            parts = line.split("\t")
            if (len(parts) >= 3 and "no original bytes" in parts[2] and parts[0] in wanted and
                    re.fullmatch(r"[A-Za-z_]\w*", parts[0])):
                names.append(parts[0])
    (OUT / "placeholders.txt").write_text("\n".join(names) + "\n", encoding="utf-8")
    src = OUT / "pc_placeholders.c"
    src.write_text("/* firstlink placeholders: names the game uses that no data source has (placeholders.txt) */\n" +
                   "".join(f"__attribute__((aligned(32))) unsigned char {n}[256];\n" for n in names), encoding="utf-8")
    subprocess.run(["gcc", "-c", "-O1", str(src), "-o", str(OUT / "pc_placeholders.o")], check=True)
    print(f"data: {len(names)} placeholder blocks for names with no data source (placeholders.txt)")


def localize_generated_duplicates():
    """pcdata.py's closure also generates variables that the port's own libraries (B's MusyX state and tables) or
    the game objects already define. The real definitions must win (two copies would split the state), so the
    generated copy is made local to pc_data_main.o; listed in data_duplicates.txt. TEMPORARY until pcdata.py can be
    told what the link already defines (asked of Main: a --defined option). Limit meanwhile: generated data that
    points at such a variable points at the local copy."""
    gen = OUT / "pc_data_main.o"
    if not gen.exists():
        return
    defined = set()
    objs = [obj_path(x) for x in (OUT / "compiled.txt").read_text(encoding="utf-8").split() if x]
    libs = [Path("build-pc") / l for l in ("liborre_musyx.a", "liborre_audio.a", "liborre_platform_vi.a",
                                           "liborre_platform.a")]
    for o in objs + libs:
        if o.exists():
            defined.update(defined_globals(o))
    dups = sorted(n for n in defined_globals(gen) if n in defined)
    (OUT / "data_duplicates.txt").write_text("\n".join(dups) + "\n", encoding="utf-8")
    if dups:
        lst = OUT / "data_duplicates.localize"
        lst.write_text("\n".join(dups) + "\n", encoding="utf-8")
        subprocess.run(["objcopy", f"--localize-symbols={lst}", str(gen)], check=True)
    print(f"data: {len(dups)} generated variables already defined elsewhere, made local (data_duplicates.txt)")


def write_build_stamp():
    """pc_build_stamp.h for pc_game_main.c: which commit and when, for the log's first line and the window title
    (snatcherLeo's request, 2026-10-17). '+changes' marks a build from a tree with uncommitted changes."""
    def git(*args):
        r = subprocess.run(["git", *args], capture_output=True, text=True)
        return r.stdout.strip() if r.returncode == 0 else ""
    commit = git("rev-parse", "--short=9", "HEAD") or "unknown"
    branch = git("rev-parse", "--abbrev-ref", "HEAD") or "?"
    dirty = " +changes" if git("status", "--porcelain", "--untracked-files=no") else ""
    when = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")
    # Which build is this, at a glance (snatcherLeo, 2026-10-17: lane test windows were confused with the build to play).
    # the project's checkout is the one to play; every other worktree's exe is a lane's test build.
    lanes = {"local/pc-hsd": "A", "local/pc-platform": "B", "local/docs-c": "C", "local/formats-d": "D"}
    if branch in lanes:
        label = f"TEST BUILD (lane {lanes[branch]}, not for playing)"
    elif Path.cwd().name == "pkmn_colosseum":
        label = "MAIN BUILD (play this one)"
    else:
        label = f"TEST BUILD ({Path.cwd().name}, not for playing)"
    stamp = f"OrrePort {label} - {commit}{dirty} ({branch}) built {when}"
    (OUT / "pc_build_stamp.h").write_text(
        "/* generated by port/tools/firstlink.py at link time; not in version control */\n"
        f"#define ORRE_BUILD_STAMP \"{stamp}\"\n", encoding="utf-8")
    print(f"build stamp: {stamp}")


def cmake_link_libraries():
    text = Path("build-pc/build.ninja").read_text(encoding="utf-8")
    block = text[text.index("build pc_audio_test.exe:"):]
    libs = re.search(r"\n  LINK_LIBRARIES = (.*)", block).group(1)
    flags = re.search(r"\n  LINK_FLAGS = (.*)", block).group(1)
    return flags.split(), libs.split()


def step_nodata():
    """Leave the generated data out of the next link, so its undefined list says exactly what pcdata must make"""
    for c in ("pc_data_main.o", "pc_rel_modules.o", "pc_placeholders.o", "pc_native_thunks.o"):
        (OUT / c).unlink(missing_ok=True)


def symbol_kinds():
    """name -> "object" / "function" from config/GC6E01/symbols.txt"""
    out = {}
    for line in Path("config/GC6E01/symbols.txt").read_text(encoding="utf-8").splitlines():
        m = re.match(r"(\S+) = \.\w+:0x[0-9A-F]+; // type:(\w+)", line)
        if m:
            out[m.group(1)] = m.group(2)
    return out


def link_aliases(undefined, defined):
    """Linker aliases (--defsym) for two kinds of undefined names the generators can't provide:
    - C++ units that call C functions without extern "C" ask for the mangled name (_Z12_threadSwitchv): alias it
      to the C name (the same function and arguments). TEMPORARY: the units need extern "C" (the project's code).
    - GameCube low-memory addresses below 0x80004000 (OS globals, boot labels): not in main.dol, but MEM1 is
      mapped at 0x80000000 on the PC too, so the symbol is that absolute address."""
    out = {}
    mangled = [n for n in undefined if n.startswith("_Z")]
    if mangled:
        r = subprocess.run(["c++filt"], input="\n".join(mangled), capture_output=True, text=True)
        for raw, plain in zip(mangled, r.stdout.splitlines()):
            base = plain.split("(", 1)[0].strip()
            if re.fullmatch(r"[A-Za-z_]\w*", base) and base in defined:
                out[raw] = base
    for n in undefined:
        m = re.fullmatch(r"lbl_(8000[0-3][0-9A-F]{3})", n)
        if m:
            out[n] = "0x" + m.group(1)
    # words inside larger symbols (the project's dataaliases.py: `lbl_8047B66C = lbl_8047B668 + 0x4`)
    # and (2026-10-18) the inner labels of the blocks pcdata.py keeps whole (pcblocks.txt)
    for table in (Path("build-pc/pc_data_aliases.txt"), GEN / "pc_block_aliases.txt"):
        if not table.exists():
            continue
        for line in table.read_text(encoding="utf-8").splitlines():
            m = re.fullmatch(r"(\w+) = (\w+) \+ (0x[0-9A-Fa-f]+)", line.strip())
            if m and m.group(1) in undefined and m.group(2) in defined:
                out[m.group(1)] = f"{m.group(2)}+{m.group(3)}"
    return out


def sound_split_wraps():
    """The MusyX calls the game exe is linked with --wrap for (port/src/pc_sound_split.wrap; the sound split prototype,
    2026-11-16). An empty list when the file is missing, so an older tree links as before."""
    f = Path("port/src/pc_sound_split.wrap")
    if not f.exists():
        return []
    return [l.strip() for l in f.read_text(encoding="utf-8").splitlines() if l.strip() and not l.startswith("#")]


def step_link():
    flags, libs = cmake_link_libraries()
    aurora = sorted(str(p.relative_to("build-pc")) for p in Path("build-pc/aurora").glob("*.a"))
    # the movie decoder must be there (2026-11-24): without libaurora_thp.a the link still succeeds, with a stand-in
    # THPVideoDecode, and the game shows no cutscenes; a fresh build had exactly that until port/CMakeLists.txt asked
    # for the library. Stop instead of linking a game without movies.
    if not (Path("build-pc") / "aurora" / "libaurora_thp.a").exists():
        raise SystemExit("link: build-pc/aurora/libaurora_thp.a is missing (Aurora's movie decoder): run the CMake "
                         "build first (cmake --build build-pc), which builds it")
    objs = [str(obj_path(s)) for s in (OUT / "compiled.txt").read_text(encoding="utf-8").split() if s]
    gen = [str(OUT / (c[:-2] + ".o")) for c in ("pc_data_main.c", "pc_rel_modules.c", "pc_missing_stubs.c",
                                                 "pc_placeholders.c", "pc_native_thunks.c", "pc_disc_manifest.c")
           if (OUT / (c[:-2] + ".o")).exists()]
    main_o = OUT / "pc_game_main.o"
    write_build_stamp()
    subprocess.run(["gcc", "-c", "-O1", "-g", "-I", str(OUT), "port/src/pc_game_main.c", "-o", str(main_o)], check=True)
    # The exe's icon and version details (4.1, 2026-11-21; port/src/pc_game_icon.rc says why): windres turns the .rc
    # into a COFF object that links like any other, so Explorer and shortcuts show the rust O for app\orrecolo.exe.
    # Windows only: Linux and macOS take their icons from a .desktop file / the .app bundle, not from the binary.
    res_o = []
    if sys.platform == "win32":
        res = OUT / "pc_game_icon.res.o"
        subprocess.run(["windres", "-I", "port/launcher", "port/src/pc_game_icon.rc", "-O", "coff", "-o", str(res)],
                       check=True)
        res_o = [str(res)]
    rsp = OUT / "link.rsp"
    # forward slashes: the linker reads backslashes in a response file as escapes
    rsp.write_text("\n".join(os.path.abspath(o).replace("\\", "/") for o in objs + gen + [str(main_o)] + res_o) + "\n",
                   encoding="utf-8")
    defined = set()
    host_libs = ["liborre_gs_thread_host.a"]  # B's GS-thread / asm-routine host versions (gs-fibres.md)
    for names in all_defined_globals([o for o in (objs + gen + [str(main_o)] +
                                                  [str(Path("build-pc") / l) for l in host_libs + libs if l.endswith(".a")] +
                                                  [str(Path("build-pc") / a) for a in aurora]) if Path(o).exists()]).values():
        defined.update(names)
    defsyms = {}
    # option 1 (port-gaps 22): the battle scripts' data at its GameCube address (pcdata.py, pcfixed.txt); MEM1 is
    # mapped at 0x80000000, below the heap arena, and pc_data_relocate copies the bytes there at startup
    fixed = GEN / "fixed_symbols.txt"
    if fixed.exists() and (OUT / "pc_data_main.o").exists():
        for line in fixed.read_text(encoding="utf-8").splitlines():
            w = line.split()
            if len(w) == 2 and not line.startswith("#") and w[0] not in defined:
                defsyms[w[0]] = w[1]
                defined.add(w[0])
    for attempt in range(3):
        # linked to a new file and swapped in afterwards (2026-10-19): ld removes its output first, so a link that
        # was stopped (or failed) left no orrecolo.exe at all for snatcherLeo's launcher or the other sessions' runs
        cmd = (["g++", "-o", str(Path("firstlink/orrecolo.new.exe")), f"@{os.path.abspath(rsp)}"] + flags + host_libs + libs +
               ["-Wl,--start-group"] + aurora + ["-Wl,--end-group"] +
               # the import libraries again after Aurora's: ld resolves a library only against references seen before
               # it, so Aurora's SDL calls (pad.cpp: SDL_ReadU8, the gamepad sensor / LED / power calls ...) were left
               # undefined and the stubs step shadowed them with logging stubs (verified 2026-10-16)
               [l for l in libs if l.endswith(".lib")] + ["-Wl,--no-demangle"] +
               # the sound split prototype (EXPERIMENTAL, 2026-11-16; port/src/pc_sound_split.c, sound-split.md): the
               # game's MusyX calls listed in pc_sound_split.wrap reach the split's wrappers, each of which calls the
               # real function first (with ORRE_SOUND_SPLIT off, only that). GNU ld's --wrap (MinGW, Linux)
               [f"-Wl,--wrap={w}" for w in sound_split_wraps()] +
               # --windowed (the hub trial, 2026-11-09): a windowed program, so no console window opens beside
               # the game for players (the log file still gets everything; ORRE_CONSOLE=1 opens a console for testers)
               (["-mwindows"] if WINDOWED else []))
        # the --defsym list in a response file of its own (2026-11-09): it is ~680 entries / 21 KB, which left
        # the command line just under Windows' 32 KB limit; the RmlUi trial's few extra libraries pushed it over
        # (WinError 206). One option per line; ld reads the same options from the file.
        defsym_rsp = OUT / "defsyms.rsp"
        defsym_rsp.write_text("".join(f"-Wl,--defsym,{k}={v}\n" for k, v in sorted(defsyms.items())), encoding="utf-8")
        cmd.append(f"@{os.path.abspath(defsym_rsp)}")
        r = subprocess.run(cmd, cwd="build-pc", capture_output=True, text=True, errors="replace")
        undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", r.stderr)))
        new = link_aliases(undef, defined)
        if not set(new) - set(defsyms):
            break
        defsyms.update(new)
    built = OUT / "orrecolo.new.exe"
    if r.returncode == 0 and built.exists():
        swapped = False
        try:
            os.replace(built, OUT / "orrecolo.exe")
            swapped = True
        except PermissionError:
            print("link: orrecolo.exe is in use (a game window or test run has it open); the new build is "
                  "build-pc/firstlink/orrecolo.new.exe. Close the game and run this step again to swap it in.")
        # 2026-11-11 (the hub's merge into the main build): the hub and the Settings overlay read their pages from
        # ui/ next to the exe (pc_hub.cpp ui_path, SDL_GetBasePath); without this copy the hub had no pages
        import shutil
        if os.path.isdir(os.path.join("port", "ui")):
            # the OrreFont files (port/ui/traced/fonts/) go to Fonts/ instead (copy_orrefont), not a second copy in ui/
            shutil.copytree(os.path.join("port", "ui"), OUT / "ui", dirs_exist_ok=True,
                            ignore=lambda d, names: ["fonts"] if Path(d).name == "traced" else [])
        copy_ui_fonts(OUT / "Fonts")
        copy_orrefont(OUT / "Fonts")
        copy_libusb(OUT)
        copy_adapter_setup(OUT)
        copy_picture_licences(OUT)
        copy_library_licences(OUT)
        # The stage step (4.1, 2026-11-21; docs/port/portable-layout.md part 6): snatcherLeo's play folder is the portable
        # folder beside the checkout (decomp\OrrePort\, his choice 2026-11-21), and a build refreshes only its app\
        # (and the two launchers beside it), never his saves or settings. Only when that folder's app\ exists (bin\
        # until layout A, 2026-11-22: a folder still on bin\ is left alone) and the link really swapped in a new exe;
        # port/tools/stage_bin.py refuses while app\'s game is running, so a game he's playing is never half-updated.
        # ORRE_NO_STAGE=1 skips it (a test build that must not reach his folder). Windows-only as written
        # (stage_bin.py says what Linux / macOS need).
        play_bin = Path("..") / "OrrePort" / "app"
        if swapped and play_bin.is_dir() and os.environ.get("ORRE_NO_STAGE") != "1":
            sys.path.insert(0, os.path.join("port", "tools"))
            import stage_bin
            stage_bin.stage(os.path.abspath(play_bin.parent), str(OUT), r"C:\msys64\mingw64\bin",
                            os.path.join("build-pc", "launcher"), quiet=True,
                            studio=os.path.join("build-pc", "studio"))
    (OUT / "link.txt").write_text(r.stdout + r.stderr, encoding="utf-8")
    (OUT / "defsyms.txt").write_text("".join(f"{k} = {v}\n" for k, v in sorted(defsyms.items())), encoding="utf-8")
    multi = sorted(set(re.findall(r"multiple definition of `([^']+)'", r.stderr)))
    # MinGW refers to some data through a ".refptr.NAME" slot: the symbol to generate is NAME
    undef = sorted(set(n[len(".refptr."):] if n.startswith(".refptr.") else n for n in undef))
    # data or function: symbols.txt's own type first (named variables like GSgfxVideoVsyncRate are objects; stubbing
    # one as a function made the game write into code), else the name's prefix
    kinds = symbol_kinds()
    data = [n for n in undef if kinds.get(n) == "object" or
            (n not in kinds and re.match(r"(lbl_|data_|jumptable_|@)", n))]
    funcs = [n for n in undef if n not in data]
    # a C++ caller's mangled name whose C function has no definition: stub the C name (the alias follows next link)
    if any(n.startswith("_Z") for n in funcs):
        r2 = subprocess.run(["c++filt"], input="\n".join(funcs), capture_output=True, text=True)
        funcs = sorted(set(p.split("(", 1)[0].strip() if n.startswith("_Z") else n
                           for n, p in zip(funcs, r2.stdout.splitlines())))
    (OUT / "undefined.txt").write_text("\n".join(undef) + "\n", encoding="utf-8")
    (OUT / "undefined_functions.txt").write_text("\n".join(funcs) + "\n", encoding="utf-8")
    (OUT / "undefined_data.txt").write_text("\n".join(data) + "\n", encoding="utf-8")
    (OUT / "multiple.txt").write_text("\n".join(multi) + "\n", encoding="utf-8")
    if not (OUT / "pc_missing_stubs.o").exists():  # without stubs: exactly the functions to stub
        (OUT / "stub_need.txt").write_text("\n".join(funcs) + "\n", encoding="utf-8")
    if not (OUT / "pc_data_main.o").exists():  # without generated data: exactly what pcdata.py must make
        (OUT / "data_need.txt").write_text("\n".join(data) + "\n", encoding="utf-8")
    print(f"link: {'LINKED' if r.returncode == 0 else 'failed'}: {len(undef)} undefined ({len(funcs)} functions, "
          f"{len(data)} data), {len(multi)} multiply defined (build-pc/firstlink/link.txt)")
    return r.returncode == 0


def step_stubs():
    """Logging stubs for the functions still undefined after a link with the generated data but without stubs"""
    need = OUT / "stub_need.txt"
    r = subprocess.run([sys.executable, "-I", "tools/local/port/pcstubs.py", str(need), "-o",
                        str(GEN / "pc_missing_stubs.c")], capture_output=True, text=True)
    print("stubs:", r.stdout.strip().splitlines()[-1:] if r.returncode == 0 else ("FAILED", r.stderr[-800:]))
    r = subprocess.run(["gcc", "-c", "-O1", "-w", "-fpermissive", "-DTARGET_PC=1", "-Iinclude", "-I.",
                        str(GEN / "pc_missing_stubs.c"), "-o", str(OUT / "pc_missing_stubs.o")],
                       capture_output=True, text=True)
    print(f"stubs: compile: {'ok' if r.returncode == 0 else 'FAILED'}")
    if r.returncode != 0:
        print("\n".join(l for l in r.stderr.splitlines() if "error" in l)[:2000])


def step_nostubs():
    (OUT / "pc_missing_stubs.o").unlink(missing_ok=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("steps", nargs="*")
    ap.add_argument("--jobs", type=int, default=min(16, os.cpu_count() or 8))  # at most 16 cores (snatcherLeo, 2026-10-16)
    ap.add_argument("--full", action="store_true", help="compile every unit and redo every step, as before 2026-10-19")
    ap.add_argument("--windowed", action="store_true",
                    help="link a windowed program (no console window; the hub trial's player build)")
    a = ap.parse_args()
    global FULL, WINDOWED
    FULL = a.full
    WINDOWED = a.windowed
    if not a.steps:
        return build_all(a.jobs)
    return run_steps(a.steps, a.jobs)


FULL_STEPS = ["nostubs", "nodata", "link", "data", "link", "stubs", "link"]


def build_all(jobs):
    """The default (no steps named): compile what changed, rename / dedupe the new objects, and then either one link
    (nothing the generated data and stubs depend on changed: link_inputs_signature) or the full data / stubs sequence.
    A small edit takes one unit's compile plus one link instead of ~1,780 compiles and three links."""
    global FULL
    t0 = datetime.datetime.now()
    if step_compile(jobs):
        print("compile: a unit that compiled last time is now left out: building everything again (--full)")
        FULL = True
        step_compile(jobs)
    step_rename()
    step_dedupe()
    sig_file = OUT / "link_inputs.sig"
    sig = link_inputs_signature()
    generated = all((OUT / o).exists() for o in ("pc_data_main.o", "pc_missing_stubs.o"))
    if not FULL and generated and sig_file.exists() and sig_file.read_text(encoding="utf-8") == sig:
        print("link inputs unchanged since the last full build: the generated data and stubs are reused, one link")
        rc = run_steps(["link"], jobs)
    else:
        sig_file.unlink(missing_ok=True)
        rc = run_steps(FULL_STEPS, jobs)
        if rc == 0 and _LAST_LINK_OK:
            sig_file.write_text(sig, encoding="utf-8")
    print(f"firstlink: done in {(datetime.datetime.now() - t0).total_seconds():.0f} s")
    return rc


_LAST_LINK_OK = False


def run_steps(steps, jobs):
    global _LAST_LINK_OK
    for s in steps:
        if s == "compile":
            step_compile(jobs)
        elif s == "rename":
            step_rename()
        elif s == "dedupe":
            step_dedupe()
        elif s == "data":
            step_data()
        elif s == "nodata":
            step_nodata()
        elif s == "link":
            _LAST_LINK_OK = step_link()
        elif s == "stubs":
            step_stubs()
        elif s == "nostubs":
            step_nostubs()
        else:
            print("unknown step", s)
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
