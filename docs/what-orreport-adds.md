# What OrrePort adds

Everything OrrePort changes from the original Pokémon Colosseum, in one list. If it isn't here, the game plays exactly
as it does on a GameCube. This page is also the answer to "is this already in?": anything listed here is built in, so
there's no need to make a mod for it.

**How to read it:**

- **Always on:** part of OrrePort, with no switch.
- **Switch:** you can turn it off; where to find the switch is given.
- **Accurate mode:** what happens in GameCube-accurate mode, which plays exactly like the console. Most changes are
  off there; changes that only alter the look or the sound stay on. See [accurate mode](accurate-mode.md) for the
  full story.

This list is for **OrrePort Beta 0.1**. It grows with each update.

## Saving and your saves

| Change | | Accurate mode |
|---|---|---|
| **PC MENU anywhere:** the pause menu gets a PC MENU entry, so you can save, and use the PC, wherever you are | always on | off |
| **Autosave:** OrrePort keeps an extra save of its own while you play, once a room has settled, or after a few quiet minutes. It never writes over your own save and never saves in the middle of a scene; your own save is still what Continue loads. To play the autosave, choose **Play the autosave** on the hub | always on | off |
| **Saving in the middle of a scene is made safe:** a PC MENU save made where a scene could leave you stuck gives you a notice and keeps you safe | always on | off (there is no PC MENU) |
| **A stuck Continue offers a way out:** if continuing a save ever hangs, the game offers to go back to your autosave (when there's a newer one) and returns to the main menu to load it | always on | off (accurate mode has no autosave) |
| **Continuing a PC MENU save puts people back as the story left them** (someone lying on the floor stays there) | always on | off |
| **Fast loading and saving** | always on | the console's own timing |
| **The Saves page on the hub:** every save in one list, with Play, Rename, Duplicate, Delete (to the Recycle Bin) and Show in folder | always on | the same |
| **Earlier versions of every save:** the last 10 versions are kept, so you can go back, and undo going back | always on | the same |
| **Import any save:** drop a `.gci`, an `.orresave` or a whole memory card (`.raw`) onto the Saves page | always on | the same |
| **Make a GameCube copy:** a save with OrrePort's longer names can be copied as a `.gci` any GameCube or Dolphin loads | always on | the same |
| **LOAD on the game's main menu:** choose any of your saves from inside the game | always on | on, listing only the saves set to accurate mode |
| **The region of each save** shown in the game's save list (US, PAL or JPN) | always on | the same (look only) |
| **"Save file" instead of "memory card"** in the game's messages | always on | the same (look only) |

## Names and text

| Change | | Accurate mode |
|---|---|---|
| **Longer names:** up to 12 letters for you, Rui, nicknames and boxes. A GameCube-length version is kept too, so the save still works on a console | always on | off: the game's own lengths (7 for you and Rui, 10 for nicknames, 8 for boxes) |
| **OrrePort's naming keyboard:** type with your PC keyboard, click with the mouse, and the controller's X / B / START act as Shift, Delete and Enter | always on | off: the game's own name screen |
| **Typing with your system's input method** (Japanese, Chinese, Korean and others) | always on | off |
| **Long names fitted** into the spots that were made for short ones (the Continue screen, the trainer card) | always on | off (names stay short there) |
| **Dialogue re-wrapped** when a long name makes a line too wide | always on | off |
| **Sharper letters:** the game's own font redrawn at any resolution, with extra letters (and emoji in names) from an open font | always on | the same (look only) |
| **FRENZY PLANT** shown with its right name (the original shows SOLID PLANT) | always on | the same (look only) |

## Menus and controls

| Change | | Accurate mode |
|---|---|---|
| **Start closes the pause menu**, and its cursor wraps round from bottom to top | always on | off |
| **Main Menu on the PC screen:** go back to the title's main menu from the PC | always on | off |
| **The PC menu's cursor wraps round** | always on | off |
| **Leaving a PC screen is quicker** (shorter fades) | always on | off |
| **Continue and New Game start on YES.** New Game starts on YES only when the save has no story yet; a save with a story keeps NO, to protect it | always on | off |
| **One question after you load a save** ("Continue your story now?"), and Continue's help line names the save | always on | off |
| **B on your name menu** (New Game) goes back to the main menu. Rui still has to be named | always on | off |
| **B keeps your choices** on the Options screen (Sound, Rumble, Language), the way Done does | always on | off |
| **"Use" only where it can do something:** the bag and battle menus offer Use only for items that can be used there; READ on the Ein Files, CHECK on key items that can't be used, and TOSS on TMs and HMs | always on | off |
| **Keep using an item:** after you use or give an item from the bag, the party list stays open while you have more, and moves on to your next stack of the same item | always on | off |
| **TMs show LEARNED** on the party list for Pokémon that already know the move | always on | off |
| **Scents remember the last one:** the cursors stay where you left them, with Vivid Scent first | always on | off |
| **Relic Forest shortcuts:** PURIFY in the party menu and the Time Flute anywhere in the Relic Forest | always on | off |
| **The party screen shows every portrait at once**, instead of them popping in one by one | always on | off |
| **The right stick works like the left one** in story mode: walking, running, battles and menus (not in Battle Mode or Battle Now) | switch: Settings › Enhancements › Tweaks | off |
| **Infinite TMs:** teaching a TM keeps it, as HMs already do | switch: Settings › Enhancements › Tweaks (on to start) | off |
| **Your controller, your way:** button mapping, the stick's range and dead zone, and the official GameCube adapter | Settings › Controls | the GameCube's stick range, dead zone at most 10 |

## Speed

| Change | | Accurate mode |
|---|---|---|
| **Game speed and Battles speed**, from normal up to 5× | switch: Settings › Enhancements › Speed (normal to start) | the same |
| **A fast-forward button** (hold or press to switch; 2× to 5×) | switch: Settings › Enhancements › Speed | the same |
| **The speed badge** ("▶▶ 3×") shown for a moment when the speed changes | switch: Settings › Enhancements › Speed | the same |
| **Music when faster:** music at its normal tempo, or faster with the game | switch: Settings › Enhancements › Speed | the same |

## Look and sound

| Change | | Accurate mode |
|---|---|---|
| **Higher resolutions**, up to 8× the original (5K) | Settings › Graphics | the same |
| **Anti-aliasing and texture filtering** (up to 16× anisotropic) | Settings › Graphics | the same |
| **Frame smoothing:** smoother motion at 60, 120, 144, 165 or 240 frames a second, or your display's rate | Settings › Graphics › Frame rate (Off to start) | the same: checked to keep every random roll |
| **Vulkan, Direct3D 12 or Direct3D 11** | Settings › Graphics › Renderer (Vulkan to start) | the same |
| **Volume:** master, music and sound effects apart | Settings › Audio | the same |

Widescreen is coming in a later update.

## Languages

| Change | | Accurate mode |
|---|---|---|
| **Six languages:** English, German, French, Italian, Spanish and Japanese. English plays from your US disc; the others need your own European or Japanese disc as well | Settings › General › Language | the same (it plays as that language's console would) |
| **Everything in the game's language,** from that language's disc: the text, the words drawn as pictures, the credits, the title demo, Battle Mode's round titles, the title logo | always on | the same (look only) |
| **The Japanese release's own look** in Japanese (its title picture, logo and outfits) | always on | the same |
| **Pokémon that were never nicknamed** show their species name in the language you play, even in saves imported from another region | always on | the same |
| **OrrePort's own screens** (the hub, Settings, menus) in all six languages | always on | the same |

## Cheats

The **Cheats** menu in the menu bar: instant win, instant catch, infinite money, items never run out, EXP from off to
several times normal, and topping up a few useful items in the PC. **Every cheat is off until you turn it on**, and
each switch is noted in a log. Cheats work in accurate mode too: there, accurate means "as on a GameCube while no
cheat is on". A cheat changes how the game plays while it's on, so a run played with one won't match a console run.

## The OrrePort app

- **The hub:** add your disc (OrrePort checks it), choose a save, change settings, and press PLAY.
- **Settings during play:** the menu bar, or a quick Settings menu over the game, which can pause it.
- **Updates, only when you say so:** OrrePort tells you when a new version is out; nothing is downloaded or installed
  until you choose to, and each update is checked as genuine before it installs. Your saves and settings are never
  touched.
- **Report a problem:** press **F8** in the game, or use the hub's **Report a problem**. OrrePort makes a report
  with what's needed to find the bug, and opens its folder. If OrrePort closed unexpectedly, it offers to make a
  report the next time it starts.
- **One portable folder:** everything, saves included, stays in the folder you unzipped. Nothing is installed in
  Windows.
- **OrrePort Studio, an early preview:** the editor that comes with OrrePort. Its editors arrive in later updates.
- **The debug menu,** off by default, for testers and developers: see [the debug menu](debug-menu.md).

## Not in this version

Widescreen, mods, more boxes, items above 99 and the OrreBank come in later updates.
