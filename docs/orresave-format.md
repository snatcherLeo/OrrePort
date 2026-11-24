# The `.orresave` format (version 1)

For tool makers: how OrrePort stores a save that holds more than a GameCube can, and the packed `.orresave` file used
to share one. Players: see [saves.md](saves.md) instead.

## Principles

- **The `.gci` is always a valid GameCube save.** OrrePort never adds its own data inside it; extras live beside it.
- **Names in the `.gci` are always GameCube-safe:** at most 7 letters for the player and Rui, 10 for a nickname, 8 for
  a box name, using only letters the game's font has. The full name lives in the extras.
- **Progress goes in the save; settings go in the setup.** Progress is what the game and player made (the `.gci`, plus
  extras a GameCube can't hold). Settings are how to play it (display name, mods, accurate mode).
- **The contents decide** whether a save is an `.orresave`, never its file name.

## In OrrePort's save folder

Each save lives in its own folder, `Saves\Slot N\`:

| File | What it holds |
|---|---|
| `<name> - Colosseum.gci` | the GameCube save, read and written by the game itself |
| `extras.ini` | only when the save has extras: see below. **A slot with `extras.ini` is an ORRESAVE.** |
| `Backups\<date_time>\` | up to 10 earlier versions, each with its own `.gci` and `extras.ini` |

The slot also keeps a small file of local history for OrrePort's own use (its place in your list, Undo, and the exact
spot where you last saved from the pause menu, so Continue puts you back there); it never travels with the save. The
saved spot is tied to the `.gci`'s SHA-1, so if the save changes any other way it's dropped, and the game loads at the
last Pokémon Center, as on a console.

## `extras.ini` (version 3)

A plain UTF-8 text file of `key=value` lines; lines starting with `#` are comments.

| Key | Value |
|---|---|
| `version` | `3` |
| `gci` | the SHA-1 (40 hex digits) of the `.gci` these names belong to |
| `<id>.full` | the name as typed (UTF-8, up to 12 letters) |
| `<id>.gc` | the name the save itself holds (the GameCube-safe form) |
| `<id>.species` | nicknames only: the species name, so tools can label it without reading the save |

The ids:

| Id | Name |
|---|---|
| `hero` | the player |
| `partner` | Rui |
| `box1`, `box2`, `box3` | the three PC boxes' names |
| `nick.<16 hex digits>` | a Pokémon's nickname: its **personality value** (the Pokémon record's offset 0x04) followed by its **original trainer's ID** (offset 0x14), each written as its real 32-bit number in hex, 8 digits each. Reading the 4 bytes at each offset of the `.gci` record as **big-endian** (the save's own byte order) gives that number. The same key in the party, the PC boxes and the daycare. |

Only names that differ from their GameCube form are listed.

- **The fingerprint:** if the `.gci` changes without the extras (a crash between writes, or the save played in
  Dolphin), `gci` no longer matches and the names aren't used.
- **Written** whole after each successful in-game save, then moved over the old file. **Removed** when no long names
  remain, so the slot is a plain GCI save again.
- **Older versions** (early OrrePort test builds; a reader may meet them, and OrrePort writes version 3):
  - **version 1:** the same as version 3; take the keys as written;
  - **version 2:** each 8-digit half of a `nick.` key was written **byte-swapped**; swap each half's bytes back before
    use. For example, a Raikou with personality value `72723B52` and trainer ID `CB58D825` was stored under
    `nick.523b727225d858cb`.
- **Compatibility:** test builds older than version 3 support don't recognise a version 3 file's nickname keys.

## The packed `.orresave` file

Used only when a save leaves OrrePort (sharing, moving to another PC). File name: `<save name> - Colosseum.orresave`.
**It's a zip** with its entries stored uncompressed (the save is already encrypted and wouldn't shrink): rename it to
`.zip` and any tool shows what's inside.

| Entry | What it holds |
|---|---|
| `manifest.ini` | `version=1`; `game=Colosseum` (one extension for every game); `written_by=` the OrrePort version (for example `OrrePort 0.1`); `gci=` the SHA-1 of `save.gci` |
| `save.gci` | the GameCube save |
| `extras.ini` | as above; present only when the save has long names |
| `settings.ini` | a `#` comment line; `name=` the save's display name; `mods=` the mod list (empty until mods arrive) |

Not included: the save's local history (where it came from, Undo, its place in your list), which belongs to the PC it
was on.

## Importing rules

- `game` must be `Colosseum` (an XD file: "XD support comes later").
- `save.gci` goes through the normal checks: right game and region, not damaged.
- `extras.ini`'s `gci` must match the SHA-1 of `save.gci`. If it doesn't, OrrePort says the file was changed outside
  it, refuses it, and offers **"Import just the save"** (a plain GCI save; nothing lost).
- Each import becomes a new slot; nothing is overwritten. "Already here" means the same save and the same extras.

## Making a GameCube copy

Because the `.gci` already holds GameCube-safe names, a GameCube copy of an `.orresave` is its `.gci`, after a rule
check that lists every difference. The rules grow as OrrePort adds features; each feature that stores something a
GameCube can't hold adds its rule at the same time. Today the only rule is names.

## Versioning

`manifest.ini`'s `version` goes up whenever the layout changes; OrrePort reads older versions and upgrades them.
