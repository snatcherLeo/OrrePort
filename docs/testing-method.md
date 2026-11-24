# How OrrePort is tested against the GameCube game

OrrePort promises that, in GameCube-accurate mode, the game plays **exactly** like the console: the same button
presses give the same random numbers, the same battles and the same rooms. This page says how that promise is checked,
and how you can run the same checks yourself. Claims are labelled **verified** (checked by a test or a Dolphin
comparison) or **likely** (not yet checked).

**In short:** a play session is recorded as button inputs. The same inputs are replayed in the PC port, and both sides
log every random-number draw and every room change. If the two logs are identical, the port did what the console did.

## The idea: the game is deterministic from its inputs

Pokémon Colosseum has **one random-number generator** for the whole game. Battles (accuracy, damage, critical hits,
the trainer AI's picks) draw from it, and so do idle eye blinks, particles and the battle camera. The generator is
seeded once from the console clock, about a second after power-on. After that, every number it gives depends only on
what the game did before: which frames ran and what the player pressed. (Verified: two Dolphin runs from the same clock
setting and the same inputs matched on every one of 20,279 frames compared.)

So if the PC port is fed the same inputs as a console and draws the same numbers in the same order, it ran the same
game logic. One wrong byte order, one missing argument or one extra frame anywhere shows up as a different draw, often
long before anything looks wrong on screen. That makes the random numbers a very sensitive test.

Two things are compared:

- **The rolls:** every random-number draw the battle logic makes, with the generator's state before and after it and
  the frame it happened on. Draws that create Pokémon (an opponent's team, a gift) are logged in their own list.
- **The room requests:** every time the game asks for a new room (a map, a menu screen, a battle), in order.

The rolls catch logic differences; the room requests catch a player who ended up somewhere else.

## Where recordings come from

Recordings come from real play. Nobody's recordings or saves ship with OrrePort; **contributors record their own**,
from their own copy of the game. Two kinds are used:

1. **Dolphin recordings.** A play session in Dolphin (the GameCube emulator), recorded as a Dolphin movie (`.dtm`,
   every controller poll) together with a log of what the game did (room entries, battles, story flags, the random
   state). The movie is turned into a PC pad script and replayed in the port. This direction checks the port against
   the console directly: Dolphin is the reference.
2. **OrrePort recordings.** A play session in OrrePort itself, in GameCube-accurate mode. OrrePort records every
   controller read the game makes (`.orrepad`) and every roll it draws (`.rolls`), on its own, with no setting to turn
   on. The recording is replayed in the port and must reproduce its own rolls exactly. It can also be converted into a
   Dolphin movie and played on the console side.

**The reference playthrough** is a complete play of the main story in OrrePort, in GameCube-accurate mode, recorded
this way. Its recordings are what the full replay check below replays.

### How a Dolphin recording becomes a PC replay

- `tools/local/testing/playrec.py` runs the recording in the scripting build of Dolphin (below). One command per
  recorded play session: `start` records, `replay N` replays session N in a scratch copy of its settings and memory
  card, `check N` compares the replay with the recording (inputs and the event log, first difference), and
  `topc N OUT.pad` writes the session's presses as a PC pad script.
- `tools/local/testing/dtm_to_pad.py <session folder> <out.pad>` does the conversion from the movie itself. Steps are
  keyed to **room entries** ("N frames after room R is entered"), not to absolute frames, so the PC's own loading times
  don't shift the inputs.
- The PC port plays the pad script with `ORRE_PAD_SCRIPT=@file:<path>`. Check the log's `[pad_script] N steps` line
  right after start: a missing `@file:` prefix is the usual mistake.

### Assisted and strict replays

Some differences between a PC and a console are about **when** inside a frame something finishes, not what the game
computes: a disc read, a copy into the second memory (ARAM) or a sound ending part-way through a screen refresh, or a
frame the console ran long. Each can move one event by one game frame, and from there the random numbers part.

- **Assisted replays** of Dolphin recordings may correct for that: for example, give each battle roll the console's
  generator state from just before it (kept in separate lists for battle rolls, the idle camera and the move camera,
  so a count difference in one can't shift the others). An assisted replay proves the **game logic** is the same.
- **Strict replays** correct nothing. The full replay check below is strict: OrrePort only **observes** its rolls
  (the roll record never changes the generator), so a match means the build really plays the recording the same way.

**The rule for a difference (verified as the working rule):** a difference may be called "timing" only when the
first differing draw is traced to one named event of those kinds, within one game frame of it. Story flags, room
order, battle results, damage, HP, the AI's choices and message order must be identical. Anything else is a bug until
traced.

## The full replay check

`tools/local/testing/full_replay_check.sh` replays every recording of the reference playthrough from **one frozen
build**, in each configuration you name, and compares each result with a **baseline**. It is how a build is cleared
before a release: every recording played the same way, on both graphics backends and in both modes, with nobody
watching.

### What you need

- A Windows PC with Git Bash or the MSYS2 shell (the scripts are POSIX shell), MSYS2's MinGW-w64 at
  `C:\msys64\mingw64` (its `bin` folder is put on `PATH`), and Python 3.
- Your own disc image of the US release at `orig/Pokemon Colosseum (USA).iso`. Every replay hard-links it into its own
  check folder, so no copy is made per run.
- A PC build (see the repository README): `build-pc/firstlink/orrecolo.exe`.
- Your recordings and baselines in the **work folder** (below).

### The work folder

Recordings and results live outside the repository, never in git. By default the work folder is `_run3` beside the
repository; set `ORRE_WORK` to use another. (`ORRE_REPO` overrides where the repository is; by default it is found
from the script's own place.)

```
<work folder>/
  <recording>/                      one folder per recorded play session
    <name>.orrepad                  the input recording
    <name>.rolls                    the rolls as recorded live
    <name>.log                      the live run's log (its "[room] request" lines are the room list)
    start_save.gci                  the save the session started from (optional)
  checks/
    <recording>_base2/              the baseline for that recording (replay.rolls, replay.rooms)
    <recording>_<pass>/             each replay's results
    full_<tag>.summary              the full check's summary
```

OrrePort writes the `.orrepad` and `.rolls` files for you, one pair per launch, in
`data\system\recordings\Pokemon Colosseum\` of the portable folder; its logs are in `data\system\logs\`. It keeps the
newest 20 sessions; set `recordings_keep = 0` in `settings.ini` to keep them all.

### Base references

A **baseline** is the replay result of a build that has been checked: its rolls and room requests for each recording,
kept so later builds can be compared with it. The script compares with folders named `<recording>_base2`.

- **Make your own:** replay each recording once with a build you trust, as pass `base2`:
  `sh tools/local/testing/run3_pc_replay.sh <recording> base2` (add `RUN_FROM=<frozen folder>` in front to use a frozen
  build). That writes `checks/<recording>_base2/`.
- **What makes a build trustworthy:** its replay reproduces the live recording's own rolls exactly (the replay script
  prints `rolls: ... IDENTICAL` and `room requests: ... IDENTICAL`), and, where a clean console comparison exists, it
  matches Dolphin too. (Verified for the project's current baseline: for example, a leaf move's turn took the same
  17,382 draws, in the same order and with the same states, on the console and on the PC.)
- When a fix changes the rolls on purpose (because the old build was wrong), the baseline is remade from the fixed
  build, and the reason is written down beside it.

### Running it

1. **Build**, then **freeze** the build once for the whole check:

   ```
   python -I tools/local/testing/freeze_build.py [<folder>]
   ```

   It copies the exe, its DLLs, the `ui` and `Fonts` folders and the pipeline cache seed into one folder (by default
   `<work folder>/frozen/<date>_<time>_<commit>/`), writes `FROZEN.txt` (commit, the exe's SHA-1, when) and prints the
   path. Why: the build folder is relinked in place, so a check running from it could change program halfway.

2. **Run the check:**

   ```
   sh tools/local/testing/full_replay_check.sh <frozen folder> <tag> <config> [<config> ...]
   ```

   - `<tag>` names this check (results go to `checks/<recording>_<tag>_<backend>_<mode>/`).
   - Each `<config>` is `<backend>:<mode>`: backend `vulkan` or `d3d12`; mode `acc` (GameCube-accurate mode, set
     through the save's own setting, as players get it) or `off` (normal mode, every setting at its default).
   - Example, all four combinations:
     `sh tools/local/testing/full_replay_check.sh "$FROZEN" r1 vulkan:acc d3d12:acc vulkan:off d3d12:off`
   - `SITS="<recording> <recording>"` in front limits the check to those recordings (by default, every recording
     with a baseline).

3. **Read the summary**, `checks/full_<tag>.summary`, one line per replay:

   ```
   <recording> <backend> <mode>: rolls same|DIFFER, rooms same|DIFFER vs base2; <seconds>s; last log line: ...
   ```

   The summary is written as each replay ends, so a stopped check keeps its results. Running the same tag again skips
   the replays that already finished and carries on.

### How each replay runs

`full_replay_check.sh` calls `tools/local/testing/run3_pc_replay.sh` once per recording and configuration. That script:

- makes a fresh check folder with the disc, the session's starting save and a `settings.ini` for the chosen backend
  and mode, and runs the frozen program with that folder as its OrrePort folder (`ORRE_ROOT`);
- converts the `.orrepad` to a pad script (`tools/local/port/orrepad_to_pad.py`) and starts the game clock at the
  recording's own start (`ORRE_VIRTUAL_EPOCH`), so the random seed is the same;
- replays at full speed with the window minimized (`ORRE_REPLAY_SPEED=max`, `ORRE_WINDOW=minimized`) and writes the
  replay's own roll record (`ORRE_ROLL_RECORD`), which only observes;
- compares the replay's rolls and room requests with the live recording's and prints the result.

`tools/local/testing/run3_runner.py` watches each replay: it stops the game 600 frames after the pad script's last
step (`RUN3_TAIL` changes that), and ends a run that makes no new frame for 60 seconds (`RUN3_STALL`), with the reason
in the log. A stuck run never needs someone to notice it. Replays run one at a time through the project's safe runner
(`tools/local/saferun.py`, a lock with time and memory caps), because comparison runs are timing-sensitive.

### What "same" means in each mode

- **Accurate mode (`acc`):** the rolls and room requests must be identical to the baseline, line for line. Anything
  else is a regression.
- **Normal mode (`off`):** differences are expected where a default convenience changes timing (fast loading, a
  shorter fade). Each difference has to be explained by such a feature; an unexplained one is a bug. This is also how
  the [accurate-mode list](accurate-mode.md)'s "likely" rows get checked: a feature stays "likely" until a replay with
  it on keeps the rolls.

## Testing an enhancement

Every change that could touch timing or the random numbers is checked the same way before it ships:

- Replay the recordings with the feature **off**: identical to the baseline (proves the hook's default is the
  original game).
- Replay with the feature **on**: identical rolls if it claims to change nothing in play (frame smoothing, for
  example: verified identical with 0, 1 and 3 extra pictures per frame); otherwise every difference explained, and the
  feature is off in accurate mode.
- Observe-only logs help find the first difference: `ORRE_RNG_COUNT=1` logs draws per room in 60-frame windows
  (`[rngcount]` lines), which can be compared between two runs line for line.

See [CONTRIBUTING.md](CONTRIBUTING.md): the determinism test passing is one of the rules every pull request meets.

## The scripting build of Dolphin

The Dolphin side uses **Felk's Dolphin fork with Python scripting**: [github.com/Felk/dolphin](https://github.com/Felk/dolphin).
Scripts run inside the emulator: they can read memory every frame, watch an address for writes, press controller
buttons for a frame, and save or load states. It is GPL-licensed like Dolphin, and OrrePort only **runs it as a
separate program**: nothing of it is copied into or linked with OrrePort. Download it from its own releases page.

### Setting it up

1. Download a **Python Scripting Preview** release from the fork's releases page and unpack it. The project's tools
   look for it at `_external/dolphin-scripting/app/Dolphin.exe` beside the repository.
2. Put an empty file named `portable.txt` beside `Dolphin.exe`, so its settings and memory cards stay in its own
   folder and never mix with your everyday Dolphin.
3. Settings that must stay fixed for every recording and replay (`playrec.py` checks them at start and reports any
   change):
   - **Dual Core off** (`CPUThread = False`): dual core makes timing vary between runs.
   - **Custom RTC on, with a fixed value.** The game seeds its random numbers from the clock; `0x386D4380`
     (2000-01-01 00:00:00, the GameCube's clock epoch) is the value the project uses.
   - The same CPU engine and the same audio mode (HLE or LLE) for the whole playthrough.
4. `python -I tools/local/testing/playrec.py setup` makes a separate settings folder for recording; `playrec.py
   settings` opens Dolphin on it to map your controller.
5. Record: `playrec.py start --section <name> [--note "..."]`. Combos while playing: **L + R + Z** places a mark (a
   numbered event and a save state, for "something odd here"); **L + R + START** ends the recording.

To start Dolphin with a script by hand: `Dolphin.exe -b -e "<disc image>" --script <script.py>` (`-m <movie.dtm>`
plays a movie).

### What the scripting interface can do, and its traps

| Need | How | Notes |
|---|---|---|
| Read memory | `memory.read_u8` ... `read_u64`, `read_f32` | field by field (no block read); fine every frame |
| Watch a variable | `memory.add_memcheck(addr)` with `event.memorybreakpoint()` | reports each write with its value; reliable (verified) |
| Stop at a function | code breakpoints listed in the game's settings file (`User/GameSettings/GC6E01.ini`), log-only | the script can't add them itself; use **log-only** breakpoints, because resuming from a breaking one in a callback puts Dolphin into single-step |
| Press buttons | `controller.set_gc_buttons(port, {...})` with `event.frameadvance()` | frame-exact |
| Save and load | `savestate.save_*` / `load_*` | slots, files or bytes |
| Screenshots | `event.framedrawn` | costs speed |

- **Registers read in a code-breakpoint callback are not reliable** (verified: the callback runs after the CPU has
  moved on). Read state from memory, or from a memory watch, which carries the written value.
- The code-breakpoint event can fire for unrelated instructions and **twice per real hit**: filter by address and drop
  duplicates.
- The game polls the controller twice per field in Dolphin but reads it once per frame; a movie keeps every poll. The
  converters take the poll the game actually reads.

### Going the other way: OrrePort recordings in Dolphin

`tools/local/testing/orrepad_to_dtm.py <recording.orrepad> <out.dtm> --offset N` turns an OrrePort recording into a
Dolphin movie. Because the PC loads rooms in a different number of frames than the console, one offset fits only the
stretch between two room loads; `--pc-log` with `--anchors` retimes each input from matched events (room entries,
message windows) in a first Dolphin pass. These replays line up presses by events, so they drift in the field
(likely), and battle rolls are given to Dolphin from the PC's roll record (`tools/local/testing/rolls_for_dolphin.py`).
They are useful to check what the console does with the same presses; they are assisted replays, not strict ones.

## What this does not cover yet

- **The clock seed in free play.** Dolphin's clock is known to handle some clock bits differently from real hardware
  (likely, from the RNG community); accurate mode aims to seed like a real console. Recorded replays avoid the
  question by starting from the recording's own clock.
- **Picture and sound.** The roll and room checks prove the logic. Pictures are checked with frame dumps
  (`ORRE_DUMP_FRAMES`) and side-by-side Dolphin frames, and sound with audio dumps; those comparisons need a person's
  eyes and ears for the final call.
- **Content nobody recorded.** A replay can only prove what a recording reached. Coverage is tracked against the
  game's story flags, rooms and battles; new recordings fill the gaps.
