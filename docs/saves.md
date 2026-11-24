# Your saves in OrrePort: `.gci`, `.orresave` and GameCube copies

This page explains how OrrePort keeps your saves, what the two kinds of save file are, and how to take a save back
to a GameCube or Dolphin. For the exact file layout (for tool makers), see [orresave-format.md](orresave-format.md).

## The short version

- **Every save starts as a `.gci`:** a normal GameCube save, the same file Dolphin uses and a real GameCube can load.
- **A save becomes an `.orresave` only when it holds something a GameCube can't.** In this version that means one
  thing: a **name longer than the GameCube allows** (OrrePort allows 12 letters for your name, Rui's, nicknames and box
  names). The save's own name never decides it; only what's in it does.
- **Your progress is never locked in.** Even an `.orresave` keeps a GameCube-ready save inside, and "Make a GameCube
  copy…" gives you a `.gci` any GameCube or Dolphin can load.

## `.gci`: a GameCube save

- Exactly what a GameCube memory card holds for Colosseum.
- Plays in OrrePort, in Dolphin and on a real GameCube (with the usual tools for moving saves to a memory card).
- Can be played in **GameCube-accurate mode** ([accurate-mode.md](accurate-mode.md)).
- Tagged **GCI** in OrrePort's hub, with a GameCube memory card symbol.

## `.orresave`: a GameCube save plus OrrePort's extras

- The same GameCube save, plus a small set of **extras** that a GameCube save has no room for. Today that's only long
  names; later versions add more (expanded boxes, items above 99, modded Pokémon), each listed here when it arrives.
- Tagged **ORRESAVE** in the hub, with OrrePort's own file symbol.
- **The save inside always holds GameCube-safe names.** If you named your Pokémon "Sparkleberry" (12 letters), the
  save itself holds "Sparkleber", a 10-letter version the GameCube can show, and OrrePort shows the full name everywhere in the game.
- Plays only in OrrePort's normal mode. GameCube-accurate mode plays it through a GameCube copy (below).
- **Nothing in the game tells you a name is too long for a GameCube.** You just play; the hub's ORRESAVE tag and the
  GameCube copy window show what a copy would change.

## Making a GameCube copy

On the hub's **Saves** page, choose a save, then **Make a GameCube copy…**:

1. A window lists what changes in the copy, for example "Sparkleberry → Sparkleber" (the closest name a GameCube can
   show). If something can't be copied at all, the window says why and the button is off.
2. **Export to GCI…** asks where to put the file, with "<save name> - Colosseum.gci" filled in. It never replaces a
   file, and **your OrrePort save is never changed.**
3. Load the `.gci` in Dolphin or move it to a memory card as usual.

**Bringing a copy back:** import that `.gci` into OrrePort again and it's recognised as a GameCube copy of your save.
Imported as a new save, it gets its long names back.

## Sharing a save or moving it to another PC

- **A GCI save:** use **Export to GCI…** and share the `.gci`.
- **An ORRESAVE save:** use **Export to ORRESAVE…**. It packs everything into one `.orresave` file: the save, its long
  names, and its display name.
- **To bring a save in:** **Import a save…** on the Saves page, or drop a `.gci`, `.raw` memory card or `.orresave`
  file anywhere on the hub, or put it in the `Saves` folder before starting OrrePort. Each import becomes a new save;
  nothing is ever overwritten.
- **Checks on import:** the right game and region; a damaged save is refused; a save you already have asks "Import it
  again as a new save?"; an `.orresave` whose names don't belong to its save is refused, with **"Import just the
  save"** offered so no progress is lost.

## Earlier versions and Undo

OrrePort keeps the last **10 versions** of each save (when it changed), including its long names. On the Saves page,
**Restore from previous version…** goes back with one click, and **Undo** brings you back again.

## Not in this version

- Double-clicking an `.orresave` to open it (drop it on the hub instead).
- Exporting a plain GCI save as `.orresave` (it doesn't need one).
- Editing the GameCube version of a name by hand (planned with the Save Editor).
