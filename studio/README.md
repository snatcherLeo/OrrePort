# OrrePort Studio

OrrePort Studio is OrrePort's companion app for looking inside Pokémon Colosseum. It reads your own copy of the game
(a disc image of the USA release) and shows what the game holds: every Pokémon with its data, pictures, 3D model,
animations, moves and cries. Over time it grows into a set of editors for making mods.

Studio never ships game files. Everything it shows is read from your own disc while it runs.

**Status:** a preview. The Pokémon browser works (its pages, the 3D view, animations, moves, multiple windows and tabs);
editing comes later.

## What's in this folder

| Folder | What it is | Platforms |
|---|---|---|
| `core/` | The reader core: a static library with a C API (`core/include/orre_core.h`, C++ inside). It reads the disc's file table, `.fsys` archives, the species, move, ability, type and item tables, the message banks, pictures, and the battle models with their animations. | every OS |
| `cli/` | `orrestudio-cli`, the core's command-line mode and test harness (`--disc <image> dump species 1`). | every OS |
| `tests/` | The golden tests: the core's output checked against the Python disc readers in `tools/local/colo/`, on your own disc. | every OS |
| `win32/` | The Windows app (`orrestudio.exe`): the windows and tabs, the list, the record pages, the 3D view, Settings, Report a problem. | Windows only for now |
| `diag/` | Shared helpers for diagnostics: the build stamp, system information, the action trail, a small zip writer. | every OS |
| `lang/` | Studio's words, one file per language (the same format as OrrePort's `port/ui/lang/`). Every word on screen has a key, so the app can be translated. | every OS |
| `shortcuts.md` | Every keyboard and mouse shortcut, with where it lives in the code and what Linux and macOS would use. | — |
| `*.md` (others) | Working notes on single topics: attachment points, the idle blink, effects, lighting, the Moves tab, multiple windows. | — |

Studio shares some code with OrrePort itself, compiled from `port/src/`: the updater, the report zip, the crash
handler, and the link that lets OrrePort and Studio find and start each other.

## Building

Studio builds on Windows with MSYS2's MinGW-w64 toolchain (GCC), CMake and Ninja. MSYS2's `mingw64\bin` must be on
`PATH`.

    cmake -S studio -B build-pc/studio -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=Release
    cmake --build build-pc/studio -j 8

The build uses the same prebuilt disc reader (nod) and graphics library (Dawn) as OrrePort's own build; if OrrePort is
already built in this checkout, point CMake at those with `-DFETCHCONTENT_SOURCE_DIR_NOD_PREBUILT=<path>` and
`-DFETCHCONTENT_SOURCE_DIR_DAWN_PREBUILT=<path>`.

The golden tests:

    python -I studio/tests/golden.py --disc "<disc image>" --cli build-pc/studio/orrestudio-cli.exe

Close Studio before rebuilding: Windows locks a running program.

## Running

In an OrrePort release, start **OrrePort Studio.exe** from the OrrePort folder, or open it from OrrePort's own menu.
Studio uses the disc OrrePort already has, and shares its language, theme and update settings.

For a development build:

    .\build-pc\studio\orrestudio.exe --disc "<disc image>"

Useful options: `--orreport <OrrePort folder>` (use that folder's settings and discs), `--theme light|dark`,
`--lang <code>` (or `pseudo`, to check that every layout survives longer words), `--no-3d`.

If something goes wrong, press **F8** (Report a problem) in Studio: it saves a small zip with your note, the log and
optional screenshots, ready to send. Logs are in the OrrePort folder's `data\system\logs\`.

## Platforms

Studio is written for Windows first. The reader core, the command-line tool and the tests are plain C++ and Python and
are meant for every OS. The app itself (`win32/`) uses Windows' own drawing and window APIs; Linux and macOS would get
their own app shells on top of the same core. The code notes, next to anything Windows-only, what the other systems
would need.
