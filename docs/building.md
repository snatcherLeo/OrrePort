# Building OrrePort from source

This page explains how to build OrrePort yourself: the PC game (`orrecolo.exe`) and the GameCube build that checks the
decompiled code against the original program. If you only want to play, download a release instead; you don't need
any of this.

Both builds need **your own copy of the game**. Nothing from the disc is in this repository, and nothing you extract
from it may ever be committed or shared.

## What you need

- **Windows 10 or 11, 64-bit.** Linux and macOS are planned but not supported yet (see the end of this page).
- **Your own disc image of Pokémon Colosseum, US release (game ID GC6E01).** ISO, and the usual Dolphin formats such
  as RVZ, work. Make it from a disc you own; OrrePort checks the game ID inside the image, so the file name doesn't
  matter. European and Japanese discs are only used later, for extra languages, never for building.
- **[MSYS2](https://www.msys2.org/)** with its MinGW-w64 tools. The PC build uses MSYS2's GCC, G++ and binutils
  (`gcc`, `g++`, `objcopy`, `nm`, `c++filt`, `windres`). From the "MSYS2 MINGW64" shell:

  ```
  pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
  ```

  This package list is likely complete; if `pacman` can't install CMake, use Kitware's portable CMake instead (see
  [Common problems](#common-problems)).
- **CMake 3.25 or newer** (MSYS2's package above, or Kitware's Windows zip).
- **Ninja** (MSYS2's package above, `pip install ninja`, or the release from GitHub). Both builds use it.
- **Python 3** from [python.org](https://www.python.org/downloads/), on your `PATH`.
- **Git** (the build stamps the commit it was made from into the game).
- **An internet connection for the first build.** The first CMake configure downloads the graphics, window and disc
  libraries (about 45 MB, most of them checked against pinned checksums), and the dependency script below downloads
  fonts and two Windows files.
- **Optional: FFmpeg**, only if you want the extractor to turn the game's movies into MP4 files for viewing. The build
  doesn't need it.

The commands below are for **Windows PowerShell**, run from the repository's top folder. Put MSYS2's MinGW tools on the
`PATH` first, in every new window (if you installed MSYS2 somewhere other than `C:\msys64`, change the path):

```
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
```

Check with `gcc --version`: it should name MinGW-w64.

## The libraries, and where they come from

- **In the repository, unchanged (`extern/`):** Aurora (the layer that stands in for the GameCube's graphics, input
  and disc), the MusyX audio library's PC backend, stb, libwebp, Monocypher and libwdi. Nothing to download.
- **Downloaded by CMake's first configure (into `build-pc/`):** Dawn (WebGPU), SDL3, nod (disc images) and the smaller
  libraries Aurora uses. Their versions are pinned by Aurora's CMake files.
- **Downloaded by `tools/fetch_deps.py` (into `deps/`):** the fonts OrrePort ships, the libusb DLL (for Nintendo's
  GameCube adapter) and Microsoft's driver files for the adapter setup. Each file comes from its official source at a
  fixed version and must match its pinned SHA-256 byte for byte, or it is refused and deleted. `deps/` is ignored by
  git.

## Building the PC game, step by step

The PC build compiles the same decompiled C as the GameCube build, so it **starts from the GameCube build**: it reads
the GameCube build's compile commands and the objects the GameCube build splits out of your disc's `main.dol`. Do the
steps in this order.

### 1. Set up the GameCube build

Follow [The GameCube build](#the-gamecube-build-matching-the-original) below up to and including `ninja`. You need it
once; later PC builds reuse it.

### 2. Download the third-party files

```
python -I tools/fetch_deps.py
```

It prints each file it fetches and ends with "N of N files in place". Run it again any time; files that are already
there and correct are skipped. `python -I tools/fetch_deps.py --check` only checks what is there (exit code 1 if
anything is missing or different).

### 3. Extract what the build needs from your disc

```
python -I tools/local/extract.py "<your disc image>" all
```

This reads your disc image and writes its contents, decoded where possible, into `extracted/` (ignored by git). The PC
build takes the game's room modules (`.rel` files) from there. `all` extracts everything, which takes a while (likely
several minutes) and needs about 3 GB of space; it is also a good way to look around the game's data. The extracted
files are yours to study and must never be committed or shared.

### 4. Configure the PC build

```
cmake -S port -B build-pc -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

The first time, this downloads Aurora's prebuilt and source dependencies into `build-pc/`. Later runs are quick.

### 5. Build the platform layer

```
cmake --build build-pc
```

This builds Aurora, the audio library, OrrePort's own PC code (the hub, settings, saves, controls) and a set of small
test programs. It must finish before the next step: the link needs Aurora's libraries, including its movie decoder.

### 6. Compile and link the game

```
python -I port/tools/firstlink.py
```

`firstlink.py` compiles every game and HSD unit of the decompiled code with GCC for the PC, gives each symbol one
definition, generates the game's data tables, and links everything with the platform layer into
`build-pc/firstlink/orrecolo.exe`. It ends with a line like `link: LINKED: ...` and `firstlink: done in N s`.

- **The first run compiles everything** (about 1,800 units) and links three times, so it takes a while. Later runs
  recompile only what changed and usually link once.
- **Options:** `--jobs N` sets how many compiles run at once (default: your core count, at most 16); `--full`
  rebuilds everything from scratch; `--windowed` links a program that opens no console window beside the game (the log
  file still gets everything).
- **Single steps** can be run by name (`compile`, `rename`, `dedupe`, `data`, `nodata`, `link`, `stubs`, `nostubs`),
  for example `python -I port/tools/firstlink.py link`. With no step named, it runs whatever is needed.
- **Units that don't compile** are listed in `build-pc/firstlink/compile_failures.txt` and left out; functions still
  missing at the end become logging stubs. The link's full output is in `build-pc/firstlink/link.txt`.

After every change to the PC code, repeat steps 5 and 6. After a change to the decompiled game code, step 6 is enough.

## Where the game lands

Everything the game needs to run ends up in **`build-pc/firstlink/`**:

- `orrecolo.exe`, the game and the OrrePort hub;
- the DLLs it loads (Dawn, SDL3, nod, libusb and others);
- `ui/` (the hub's and Settings' pages and the language files), `Fonts/` and `licenses/`.

When it runs, it also makes its own folders there: `logs/` (one log per run, the newest 20 kept), `user/` (your
`settings.ini` and controller profiles), `Colosseum/` (your saves and the `ROM` folder for your disc) and `cache/`.

## Running it

In PowerShell, with MSYS2's MinGW folder on the `PATH` (the game needs its C++ runtime DLLs):

```
$env:PATH = "C:\msys64\mingw64\bin;$env:PATH"
.\build-pc\firstlink\orrecolo.exe --disc "<your disc image>"
```

- **Without `--disc`**, OrrePort looks in `build-pc\firstlink\Colosseum\ROM\` for an image whose game ID is GC6E01. If
  there is none, the hub opens on **Add your disc**: drop the image onto the window, or browse for it.
- **Only one OrrePort runs per folder.** Starting a second copy brings the first window forward.
- **The menu:** press **Esc** on the keyboard or **View** on a controller for OrrePort's menu, and **F8** to save a
  problem report. All the settings and keys are on [controls-and-settings.md](controls-and-settings.md).

## The GameCube build (matching the original)

This is the decompilation itself: the C code is compiled with the original GameCube compilers and linked into a
`main.dol` and 25 room modules, and each must have exactly the same SHA-1 as the original. It uses the dtk-template
setup and runs on Windows with native tools (Python and Ninja; MSYS2 isn't needed for it). On Linux and macOS it
likely works too, through wibo, which the build downloads.

### 1. Put your game files in `orig/GC6E01/`

The build needs the original `main.dol` and the game's archives:

- **Extract the whole disc** into `orig/GC6E01/`, so that `orig/GC6E01/sys/main.dol` and `orig/GC6E01/files/` exist. In
  Dolphin: right-click the game › **Properties** › **Filesystem**, right-click the disc at the top › **Extract Entire
  Disc...**, and choose `orig/GC6E01/`.
- **Don't leave a disc image directly inside `orig/GC6E01/`.** dtk then reads every file from inside the image and
  can't find the room modules the build unpacks next to it (the build stops with `files/common_rel.rel not found`). If
  you keep the image in the repository at all, put it in a subfolder such as `orig/GC6E01/disc/`.

Everything under `orig/` is ignored by git.

The game keeps its room modules inside its own archives (`.fsys`), which dtk can't read, so the build unpacks the ones
it needs into `orig/GC6E01/files/` itself, with `tools/fsys_extract.py`.

### 2. Configure and build

```
python configure.py
ninja
```

The first `ninja` downloads the pinned build tools (dtk and the GameCube compilers) into `build/`, splits
`main.dol` and the room modules into objects, compiles the decompiled source and links. It ends by checking all 26
outputs against `config/GC6E01/build.sha1`. A build that ends without errors has matched the original byte for byte.

### 3. See how much is decompiled

```
ninja build/GC6E01/report.json
python configure.py progress
```

**A passing build doesn't mean every function is decompiled.** Where a function's C doesn't match yet, the build uses
the original code for it, so the final program always matches. The truth for each function is its
`fuzzy_match_percent` in `build/GC6E01/report.json`: a function counts as matched only at 100.0. `python
tools/decompctx.py -I include src/<path>.c` makes the context file for comparing a function on decomp.me.

## Common problems

- **`pacman` can't install CMake (or old mirrors).** Use Kitware's portable Windows zip from
  [cmake.org](https://cmake.org/download/) instead: unpack it anywhere and put its `bin` folder on the `PATH` after
  MSYS2's, for example `$env:PATH = "C:\msys64\mingw64\bin;<cmake folder>\bin;$env:PATH"`.
- **`gcc` isn't found, or the wrong one is.** MSYS2's `mingw64\bin` must be on the `PATH` in that window, before any
  other GCC. `gcc --version` should name MinGW-w64.
- **The game won't start: a DLL is missing** (`libstdc++-6.dll`, `libgcc_s_seh-1.dll`, `libwinpthread-1.dll`). Put
  MSYS2's `mingw64\bin` on the `PATH` before starting it.
- **`firstlink.py` stops: "libaurora_thp.a is missing".** Step 5 hasn't run, or failed. Run `cmake --build build-pc`
  first.
- **`firstlink.py` fails right away, or finds no units.** The GameCube build isn't set up: run `python configure.py`
  and `ninja` first.
- **"orrecolo.exe is in use".** The game (or a test run) is still open, so Windows locks the file. The new build is
  left as `build-pc/firstlink/orrecolo.new.exe`: close the game and run `python -I port/tools/firstlink.py link`.
- **"WARNING: the hub's fonts are missing", or the hub shows no text.** Run `python -I tools/fetch_deps.py`, then step
  6 again.
- **"WARNING: libusb is missing".** The same fix. Without libusb the game runs, but Nintendo's GameCube adapter can't
  be read.
- **The GameCube build stops with `files/common_rel.rel not found`.** A disc image sits directly in `orig/GC6E01/`:
  move it into a subfolder (above).
- **The disc is refused.** OrrePort plays only the US release (game ID GC6E01). The message names any image of
  another game or region it found.
- **The first start with Vulkan stutters once.** Vulkan builds its shader cache on the first run; it is smooth after
  that.
- **Black wedges or other picture faults with Direct3D 12 at high resolutions.** Use Vulkan (the default:
  **Settings › Graphics › Renderer**). OrrePort tries Vulkan first and falls back to Direct3D 12 on its own if Vulkan
  doesn't start.
- **A crash.** Read the newest file in `build-pc/firstlink/logs/`. Its first line names the build (commit, branch,
  date, and `+changes` if it was built with uncommitted changes); a crash report ends with the last room and game
  frame. Press **F8** in the game to save a report with the log, a picture and your note.

## Linux and macOS

Not supported yet; Windows comes first, then Linux and macOS. The code is written for that: everything specific to
Windows sits in the platform layer, with a note on what the other systems need. Some parts of today's build are
Windows-only: the icon step (`windres`), the adapter setup and its Microsoft driver files, and the shipped libusb DLL
(Linux likely uses the system's own libusb; macOS would ship its own).
