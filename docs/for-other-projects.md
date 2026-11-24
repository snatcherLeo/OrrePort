# For other port and decompilation teams

If you are decompiling a GameCube game, porting one to the PC, or working on Pokémon XD, this page is for you. It
says how OrrePort is put together, how it is tested, what you can reuse and under which licence, and how to credit
the work it builds on. Questions and comparisons between projects are always welcome. Claims are labelled
**verified** (checked by a test, the game's code or a Dolphin comparison) or **likely** (not yet checked).

## How the port is built

**The game's own code, compiled natively.** Pokémon Colosseum's code was decompiled to C that rebuilds the original
program byte for byte. OrrePort compiles that same C with GCC (MinGW-w64 on Windows) under a `TARGET_PC` switch, into a
64-bit PC program. It is not an emulator and not a static recompilation: the game runs as ordinary C.

The layers, bottom to top, are the same on both builds:

| Layer | Folder | What it is |
|---|---|---|
| Dolphin SDK | `src/dolphin` | Nintendo's GameCube system library; on the PC most of it is replaced by the hardware layer |
| HSD (sysdolphin) | `src/hsd` | HAL Laboratory's model, animation and scene library, the same one Super Smash Bros. Melee uses |
| MusyX | `src/musyx` | the audio library |
| Runtime | `src/crt`, `src/trk` | the compiler's C runtime and debugger stub |
| Game | `src/game` | the developer's own framework and the game itself |

**The hardware layer is Aurora.** Aurora reimplements the GameCube's graphics (GX) and input on modern APIs (WebGPU
through Dawn, with SDL3 for windows, input and sound). OrrePort carries its own copy in `extern/aurora` with additions
this game needs: main memory reserved at its GameCube address, disc and memory-card callbacks delivered on the game
thread, a hook that sees every controller read, frame smoothing and more. The rest of the platform layer is in
`port/src`: threads on fibres with the console's scheduling rules, 59.94 Hz pacing, disc reads from the player's
image, the memory card as `.gci` files, and a crash handler. Audio runs the matched MusyX code on a software mixer
(`extern/musyx-pc`).

**Data from the player's own disc.** The original program's data (tables, strings, pointer tables) is generated as C
from the player's disc image at build time, with typed fields converted to native order and pointers relocated.
Nothing generated from the disc is ever committed.

**Hook points with the original as the default.** Inside the game code, OrrePort never builds a feature. It adds
small named hook points (`OrreHook_<What>`, declared in `include/pc/orre_hooks.h`): each is a function pointer, and
when nothing is registered the game does exactly what the GameCube does. Features live in `port/` and register for a
hook. Port correctness fixes (64-bit pointers, byte order, missing arguments) are not hooks; they are plain fixes under
`TARGET_PC`.

**The GameCube build stays byte-identical.** Every PC change lives under `#ifdef TARGET_PC` or in PC-only files, so
the same source still builds the original program. Each change is checked against the original's SHA-1 and the
per-function match report. One caution worth passing on: in a dtk-based project the linked program always matches,
because unmatched functions fall back to the original assembly. Only the per-function report says whether a function
really matched.

**A step-by-step account.** The order of work (compile, link with logging stubs, generate data, first boot, then one
screen at a time), the bug families a 32-bit big-endian game hits on a 64-bit little-endian PC, and the checker for
each are written up as a playbook for other teams in the engine documentation repository,
[orre-engine-docs](https://github.com/snatcherLeo/orre-engine-docs). A few families worth knowing before you start
(all verified on this game):

- **32-bit pointer slots in GameCube-layout records**, read as 8-byte native pointers.
- **Big-endian data read natively**, including tables the decompiled source keeps at one width and the code reads at
  another.
- **Evaluation order:** the GameCube compiler evaluated `(f() & 0xFFFF) | (f() << 16)` right operand first, GCC left
  first; every Pokémon's personality value came out with its halves swapped.
- **Hardware that doesn't trap:** PowerPC returns a value for a division by zero and gives 0 for a shift by 32; x86
  traps or masks the count.
- **Values left in registers:** a function written `void` whose caller reads the result, or a call that passes an
  argument the prototype doesn't list. Never default such a value to 0: read the real one from the original assembly.

## How it is tested

OrrePort is tested by **determinism**: recorded play is replayed in the port, and every random-number draw and every
room change is compared with the recording and with a baseline. One random generator serves the whole game, so almost
any port bug shows up as a different draw. The method, the scripts and a how-to for the scripting build of Dolphin are
in [testing-method.md](testing-method.md). The method itself doesn't depend on Colosseum (likely): any game with a
single seeded generator can be checked the same way.

## What you can reuse, and under which licence

This is a summary, not legal advice. The licence files in the repository are what count.

- **OrrePort's own code is MIT:** the PC port (`port/`, `include/pc/`), the `TARGET_PC` fixes in the game code, the
  tools in `tools/local/` and the documentation pages. Use it, change it, ship it; keep the copyright and licence
  notice.
- **The decompiled base keeps its own licence and notice.** It is MIT too, with `LICENSE` and `NOTICE` kept exactly as
  they were. `NOTICE` says what the licence does **not** cover: the game itself, anything extracted from the disc, and
  the original compiler.
- **Every library keeps its own licence.** The vendored copies under `extern/` carry their licence files: Aurora (MIT),
  the MusyX PC backend (CC0), Monocypher (BSD-2-Clause or CC0), stb_image (public domain or MIT), libwebp
  (BSD-3-Clause) and libwdi (LGPL). The matched MusyX code in `src/musyx` records its origin in
  `src/musyx/LICENSE.AxioDL-musyx`. Libraries that ship with the app list their licences in its `licenses` folder.
- **No game data, ever.** The repository holds no disc files, no extracted assets, no saves and no recordings. Tools
  read the player's own disc image.
- **Tools that run separately keep their licence too.** Dolphin and its scripting build are GPL; OrrePort only runs
  them as separate programs for testing.

Things that are most likely useful outside this project (likely):

- **The `TARGET_PC` fix patterns** and their checkers: most are about the GameCube's compiler and data layout, not
  about this game.
- **The data generator** that turns the original program's data into C at build time.
- **The fibre-based thread layer** that keeps the SDK's scheduling rules on one host thread.
- **The input recording and roll record** (`.orrepad`, `.rolls`) and the replay scripts in `tools/local/testing/`.
- **The disc readers** in `tools/local/colo/` for the archive and data formats Genius Sonority's games share.

## The rename map

Many functions and data labels were renamed from their placeholder names (`fn_8001C3A4`, `lbl_8047ADB8`) to names that
say what they are, in batches, only where the evidence was certain. Every rename is recorded, so code written against
older names can be brought up to date.

- **The map:** `docs/function_renames.csv` (functions) and `docs/data_renames.csv` (data labels). Each row gives the
  address, the old name, the new name, the evidence and how certain it is (`docs/NAMING_SCHEME.md` explains the
  tiers). A name renamed twice has two rows; follow them old to new to newer. Placeholder names always contain the
  address, so nothing is lost.
- **Checking it:** `python tools/check_naming.py` validates the map against `config/GC6E01/symbols.txt` and the
  sources. `--remap-report IN OUT` rewrites an objdiff `report.json` from older names to the current ones, so
  progress reports from before a rename can be compared.
- **Bringing your code up to date:** `python -I tools/apply_rename_map.py <file or folder> ...` lists what would
  change; add `--apply` to write it. Old names map to their final new names (renames of renames are followed), whole
  words only, never inside string or character literals (comments are updated); Markdown and text files are updated
  too. It is rerunnable: a second run changes nothing. It's meant for your own code, not this repository's sources
  (a few headers keep old names on purpose).
- **Between two revisions:** `python -I tools/local/port/apply_renames.py <old revision> <new revision> [paths...]`
  builds the map by address from two revisions' `symbols.txt` instead. It only reports until you add `--write`, needs
  both revisions in the same git repository (fetch the older project as a remote first), and skips any rename that
  would change meaning (an old name that still names another address, or a new name that is already taken).

The game's own names are never changed by a rename: string literals and file names stay as they are, so the GameCube
build stays byte-identical.

## Who to credit, and how

OrrePort stands on other people's work. If you reuse something, please credit it the same way:

- **The Pokémon Colosseum decompilation** OrrePort is built on, and its contributors. `CREDITS.md` names them and links
  their repositories; `LICENSE` and `NOTICE` carry its terms.
- **The Super Smash Bros. Melee decompilation** ([doldecomp/melee](https://github.com/doldecomp/melee)): the HSD code
  follows its structure, names and inline helpers, because both games use the same library. It was used as a
  reference; no files were taken from it. `NOTICE` records the snapshot used. That project has no licence file, so
  treat its code as a reference too.
- **Aurora**, the GameCube graphics and input layer, and the **MusyX decompilation by AxioDL**.
- **dtk and objdiff**, the decompilation toolchain.
- **The Colosseum and XD research and modding community**, whose format notes and RNG findings this work checks
  against.
- **OrrePort itself:** "OrrePort (snatcherLeo and contributors), MIT". A link to the repository is enough.

When you credit, say what you took and from where, and keep each project's licence and notice intact. A line in your
README or credits file is plenty.

## Pokémon XD

Pokémon XD: Gale of Darkness is by the same developer and runs on the same engine layers: the Dolphin SDK, the HSD
library shared with Melee, MusyX audio and Genius Sonority's own framework (its memory manager, resource registry, room
loading, script engine and text system). Nothing below has been checked against XD's code yet, so it is all
**likely** unless it says otherwise.

**What should carry over unchanged (likely):**

- The **bug families** and their checkers: they come from the compiler, the SDK and HSD, which both games share.
- The **platform layer**: Aurora, the fibre thread layer, the 59.94 Hz pacing, the disc and card layers, the data
  generator, the crash handler, and frame smoothing, which lives entirely in Aurora and the port layer.
- The **testing method**: input recordings, roll records and strict replays. XD's port would record its own reference
  playthrough; a full Dolphin playthrough of the same route, with the same team and the same actions, would give an
  exact console reference for every scene.
- The **text system** and its font: the dialogue font on the two US discs is the same picture for every letter they
  share (verified with a one-off read-only comparison), so OrrePort's font work serves both games.
- The **archive format** and the way one large data module fills the game's table pointers at start-up.

**What to expect to differ (likely):**

- **The script engine** was rewritten: its values carry their own types, so the bridge to its native functions is
  different.
- **Data tables and the save layout:** entry sizes, the save's container and its cipher. Compare the table sizes
  between the games first.
- **The battle AI** has extensions, and XD has its own purification and menu flows.
- **The SDK and HSD revisions** are likely later ones. A quick way to tell an HSD revision: Colosseum's
  `SetupRigidModelMtx` lacks Melee's `HSD_PObjSetMtxMark` call.

**Practical advice for an XD port (likely):**

- Original source names survive in assert and error strings in Colosseum (verified). Scan XD's strings for names
  before guessing any.
- Port features that sit outside the game code (the hub, settings, the input recorder, frame smoothing) carry over
  as they are. Features that hook game code would need the same hook places found at XD's own addresses; the hook
  pattern (a function pointer whose default is the original behaviour) stays the same.
- Keep the GameCube build byte-identical from the first PC change on. It is the cheapest way to know a port fix didn't
  change the game.
