# GameCube-accurate mode

GameCube-accurate mode makes OrrePort play **exactly like the GameCube version**: the same timing, the same random
numbers, the same button presses. It's for speedrunners, RNG routes, recordings, and anyone who wants the original
untouched.

**In short:** it's a setting of **each save**. Turn it on for a save on the **Saves** page while the game is closed;
that save then always plays in accurate mode and shows an **Accurate** badge. New saves start with the default you pick
in **Settings › General**.

## Two separate questions

OrrePort keeps two promises apart. They're connected, but not the same:

| | GameCube-accurate mode | `.gci` or `.orresave` |
|---|---|---|
| **About** | how the game **behaves** while you play | what your **save file** holds |
| **Protects** | the same button presses give the same results as a console | your save can go back to a GameCube or Dolphin |
| **Set by** | each save's own setting, changed on the Saves page while the game is closed | what's in the save ([saves.md](saves.md)) |

- **Accurate mode on:** plays `.gci` saves only. A save with long names plays through a GameCube copy (below).
- **Accurate mode off (normal mode):** plays both. A `.gci` stays a `.gci` until it gains something a GameCube can't
  hold, but its random rolls follow a different path than on a console, because normal mode's conveniences change
  timing.

## How to use it

- **Each save keeps its own setting.** A save set to accurate mode always opens in accurate mode; a normal save
  always opens normally. You never have to remember to switch it back.
- **Changing it for a save:** open **Saves** (the save card on the hub, or **All saves**), pick the save, and use the
  **GameCube-accurate mode** switch in its details. This works only while the game is closed, which is always the case
  on the hub.
- **The badge:** a save set to accurate mode shows **Accurate** next to its GCI tag, in the save list, in its details
  and on the hub's current-save card.
- **New saves:** **Settings › General** starts with **GameCube-accurate mode for new saves** (Off or On). Every new
  save, empty or imported, takes that setting; you can still change each one on the Saves page. A duplicate keeps the
  setting of the save it was copied from.
- **What it turns off:** the link under the switch (and in Settings › General) lists what accurate mode switches off,
  what it keeps at the GameCube's own way, and what still works. It's the same list as the table below.
- **A save with long names (`.orresave`):** turning accurate mode on doesn't change it. Instead OrrePort offers
  **Make a GameCube copy…**: you choose where to save the copy (a `.gci` any GameCube or Dolphin can load), and the
  same copy is added to your saves with accurate mode on, ready to play. Your original save stays exactly as it was.
  Pressing **Play** on an `.orresave` that is somehow set to accurate mode opens the same offer.
- **While the game runs:** Settings shows the General row greyed with "Close the game to change this". Options that
  accurate mode turns off show a lock and "Locked off because this save is in accurate mode". Options that only change
  the look or the sound (resolution, filtering, languages, fonts, volume) are never locked.
- **Switching saves inside the game:** the main menu's **LOAD** list shows only the saves that play in the same mode
  as the game you're in. To play a save of the other kind, choose it on the hub; the game starts in that save's mode.
- **Saves from earlier versions of OrrePort:** before accurate mode became a setting of each save, it was one switch
  for the whole program. The first time you start this version, every existing save takes the value that switch had,
  once, and keeps it from then on. An `.orresave` starts off (it could only play in accurate mode through a copy anyway).

## What accurate mode means

- **The original game, unchanged:** every OrrePort feature that would change timing, the random numbers, or the
  buttons a console route needs is **off**. Look and sound changes that keep the random numbers stay on (higher
  resolution, fonts, languages, the "save file" wording), because they change nothing in play.
- **Your save stays a pure GameCube save:** anything that would turn a save into an `.orresave` is off, including
  long names (the game's own name screen and limits: 7 letters for you and Rui, 10 for nicknames, 8 for boxes).
- **Never switched mid-game:** it's set per save, on the hub, before the game starts.
- **Checked, not assumed:** OrrePort's changes are tested by replaying recordings of real play and comparing every
  random roll and room against the original. A save set to accurate mode through the Saves page has been replayed
  that way: every random roll and every room matched the original recording.

## What's on and off

The full list grows with every OrrePort feature. A feature marked "likely" hasn't been through the replay check yet.

| OrrePort feature | Normal mode | Accurate mode |
|---|---|---|
| OrrePort's naming keyboard (12-letter names, typing, mouse) | on | **off**: the game's own name screen |
| Typing with your system's input method (Japanese, Chinese, Korean…) on the naming keyboard | on | **off**: the game's own name screen (time spent naming changes the random rolls in both modes, as on a console; likely until the replay check) |
| Long names fitted into fixed spots | on | off (names stay at console length) |
| Dialogue re-wrapping for long lines | on | **off**: the game's own layout |
| Continue and New Game start on YES | on (built in, no switch) | **off**: the game's NO |
| After loading a save from the list, one question ("Continue your story now?") and CONTINUE's help line naming the save | on | **off**: the game's own messages |
| B on the player's name menu (New Game) goes back to the main menu; Rui must still be named | on | **off**: B does nothing there, as on the console (likely until the replay check) |
| B keeps your choices on the Options screen (Sound, Rumble, Language) | on | **off**: B cancels, as on the console |
| PC MENU in the pause menu (save anywhere), Start closing the pause menu, and its cursor wrapping round | on | **off**: the game's own pause menu (B still closes it, as on the console) |
| Main Menu on the PC screen, and the PC menu's cursor wrapping round | on | **off**: the game's own four-button PC menu |
| Leaving a PC screen faster (shorter fades, the closing slide cut once the screen is black) | on | **off**: the game's own exit |
| Smooth Party UI | on (built in, no switch) | **off** |
| Fast loading and saving | on (built in, no switch) | **off**: console timing |
| Game speed and Battles speed | switchable | allowed; music and sound always follow the game's speed (likely) |
| The stick's range and dead zone (Settings › Controls) | adjustable | the GameCube's range; dead zone at most 10 |
| Cheats menu | every cheat off until switched on | **works too**: accurate mode means "as on a GameCube while no cheat is on". A cheat changes outcomes, and with them later random rolls; every result stays inside the game's limits, so the save stays a normal GameCube save |
| Animations don't affect randomness (planned) | on | **off** (one random sequence, as on the console) |
| LOAD button and save list on the main menu | on | on, listing only saves set to accurate mode (checked: it draws no random numbers) |
| Languages, the font renderer, "save file" wording | on | on (look only) |
| Letters for every language (OrrePort's shipped fonts), emoji in names | on | on (look only; names typed on the game's own screen can't hold emoji) |
| LOAD's word and the shortened Quit in the game's language (CHARGER · Retour, LADEN · Zurück, CARICA · INDIETRO, CARGAR · Volver, ロード · もどる) | on | on, the same (look only: made when the main menu opens; no random numbers, no change to button presses; likely until the replay check) |
| The credits, the title demo, Battle Mode's round titles and the title logo in the game's language (from that language's disc) | on | on, the same (look only, no random numbers; a video or round title of a different length can change frame counts while it plays, when no buttons are read; likely until the replay check) |
| Resolution, anti-aliasing, texture filtering, window and full screen, volume, button mapping | on | on (look and sound only) |
| Frame smoothing (Settings › Graphics, Frame rate) | off by default, your choice | on, the same: the replay check showed it keeps every random roll at every rate |
| Widescreen, texture and sound packs (planned) | on | allowed once the replay check shows they keep the random numbers |
| Read-only tools (RNG viewer, flag viewer, damage calculator) (planned) | on | on (they only read) |
| Behaviour-changing mods; more boxes; items above 99 | on | **off** |

Planned features join this table as they're built.
