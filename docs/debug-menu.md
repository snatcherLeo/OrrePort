# The debug menu

OrrePort has a debug menu for developers and testers. It is **off by default**, and normal play never needs it: the
quick cheats (instant win, infinite money and the like) live in the **Cheats** menu in the menu bar, where they are safe
to switch on and off.

## What it can do

- Jump to any room, by its number.
- Set or read the game's story flags.
- Give items and Pokémon, or set a whole team.
- Start a battle by its number, or win or lose the current one.
- Change how fast the game runs, step one frame at a time, take screenshots.
- Save and load checkpoints of the whole game.
- A command box for all of the above, typed (type `help` in it for the list).

## Before you turn it on

The debug menu does exactly what you ask, even when the game would never allow it. Jumping into a room in the middle of
an event, or setting story flags out of order, **can break your save for good**. So:

- **Copy your save first.** Your saves are in the `Pokemon Colosseum\Saves` folder inside the OrrePort folder (File › Open
  OrrePort folder opens it).
- Keep a separate save for experimenting.
- If you report a problem while debug tools were in use, say so in the report.

The debug menu is in English only, and it looks like a developer's tool, because it is one.

## How to turn it on

1. Close OrrePort.
2. Open the OrrePort folder, then `data\settings\`, and open `settings.ini` with a text editor (Notepad works). (In a
   copy you built from source, it's in the `user` folder instead.)
3. Find the line `debug_tools = 0` and change it to `debug_tools = 1`. If there is no such line, add
   `debug_tools = 1` on a line of its own.
4. Save the file and start OrrePort.

Now the debug menu opens with **F1** while you play, and from the menu bar: **Cheats › Debug menu…**, at the bottom of
the Cheats menu.

## How to turn it off

**Settings › General › Debug tools**: switch it off. The row only appears while debug tools are on. The change applies
the next time OrrePort starts. Or set `debug_tools = 0` in `settings.ini` again.

## For developers

- The environment variable `ORRE_DEBUG=1` turns the debug tools on for one run, whatever `settings.ini` says;
  `ORRE_DEBUG=0` keeps them off.
- Every button in the debug menu runs one of the typed commands, so the log shows exactly what was done.
- Automated test runs never read the saved setting, so recorded tests behave the same on every machine.
