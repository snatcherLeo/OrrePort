# Controls and settings

This page lists what every OrrePort setting does, its default, and whether it works in **GameCube-accurate mode**, plus
the controller and keyboard controls. It's for players who want to know what a switch does, and for modders and tool
makers who want to know where it is kept.

For what accurate mode is and the full list of what it turns off, see [accurate-mode.md](accurate-mode.md). In short:
anything that changes how the game plays is off in an accurate-mode save; anything that only changes the look or the
sound keeps working.

## Opening Settings

- **In the hub** (before a game starts): the Settings page fills the hub's middle, with the categories in a column on
  the left. **L / R** switch category; **B** goes back.
- **While you play:** press the **menu shortcut** (**Esc** on the keyboard, **View** on a controller, **Z + L** on a
  GameCube controller; you can change it). It opens a side panel over the game with **Back to the game**, **Open full
  Settings** and **Quit to hub**. **F2** also opens and closes it, and so does **File › Settings…** in the menu bar.
- **Changes apply at once**, except where a setting says "Applies the next time OrrePort starts".
- **The game never sees the menu shortcut.** Its buttons are held back from the game while you use them.

The categories, in the order they appear: **General, Appearance, Graphics, Enhancements, Audio, Controls, About.**

## General

| Setting | What it does | Default | In accurate mode |
|---|---|---|---|
| **GameCube-accurate mode for new saves** | The mode every new save starts in (each save keeps its own; change it per save on the **Saves** page). While a game runs, the row shows that save's mode, greyed: "Close the game to change this". | Off | Set in the hub only |
| **What it turns off** | Opens the list of what accurate mode turns off, what works as on the GameCube, and what still works. | | |
| **Ask how to play imported saves** | On: importing a save asks whether to play it Enhanced or GameCube-accurate. Off: imports take the default above. | On | Works (hub only) |
| **Window** | Windowed or Fullscreen. **F11** (or **Alt+Enter**) does the same. | Windowed (likely) | Works |
| **Language** | The language of OrrePort's own screens: **Follow Windows**, or a language by name. Applies the next time OrrePort starts. | Follow Windows | Works |
| **More languages** | The game's own text in other languages: add your own European or Japanese disc (the official translations). Done from the hub's main page, under your US disc. | | Works |
| **Pause the game while this menu is open** | On: the game stops behind the menu. Off: it keeps running, as in Dolphin. | Off | Works |
| **Menu hint at start** | Shows how to open the menu for a few seconds when the game starts. | On | Works |
| **In-game menu** | What the menu shortcut opens while you play: **Side panel** or **Full window**. The hub always opens the full window. | Side panel | Works |
| **Ask before Quit to hub** | Quit to hub asks first, since progress since your last save is lost. | On | Works |
| **Report a problem…** | Saves a report to share: your note, the recording, the save, the log and a picture. **F8** does the same from anywhere in the game. | | Works |
| **Debug tools** | The developers' debug menu (**F1**). The row only appears once debug tools are on; see [debug-menu.md](debug-menu.md). Applies the next time OrrePort starts. | Off | Not locked, but the debug menu can change your save in ways the game never could |

## Appearance

Look only: everything here works in accurate mode.

| Setting | What it does | Default |
|---|---|---|
| **Theme** | Light, dark, or the same as Windows. | Follow Windows |
| **Window icon** | The icon in the title bar's top-left corner: the **OrrePort logo**, or **Espeon and Umbreon** from your game's own save icon. Listed once you've added your disc. The taskbar always shows OrrePort's icon. | OrrePort logo |
| **Animations** | On: the hub's disc moves. | On |
| **Change disc art** | The picture on the hub's disc: OrrePort's, the game's banner, or your own. Opens in the hub, once a disc is added. | |
| **Always show the menu bar** | Off: **File**, **Cheats** and **Help** appear only when the mouse is at the top of the window. | Off |
| **Menu background** | In game: **See-through** (the game shows through the menu) or **Solid**. | See-through |

## Graphics

All of these change only the picture, so all of them work in accurate mode.

| Setting | What it does | Default |
|---|---|---|
| **Renderer** | **Vulkan** or **Direct3D 12**. Vulkan suits most PCs; if Vulkan doesn't start, OrrePort falls back to Direct3D 12 on its own. Applies the next time OrrePort starts. | Vulkan |
| **Internal resolution** | How sharply the game is drawn: **Match the window**, **Native** (640 × 480, the GameCube's), or **2×** to **8×** (8× is 5120 × 3840, for 5K). Higher needs a stronger graphics card. | Match the window |
| **Frame rate** | Frame smoothing: extra pictures between the game's own frames, for smoother motion. **Off (original)** (the game's own rate), **Match** (your display's rate, shown as "Match (120 Hz)"; marked Recommended), or a fixed **60, 120, 144, 165** or **240 FPS**. Smoothest at your display's rate or half of it. The picture runs up to one game frame behind. A rate above your display's runs at your display's rate. | Off (original) |
| **V-Sync** | On: each picture waits for your monitor. Steadier, with a little more delay. | Off |
| **Anti-aliasing** | Smooths jagged edges: **None** or **4× MSAA**. Applies the next time OrrePort starts. | None |
| **Texture filtering** | Keeps floors and walls sharp at a low angle: **None** (the game's own filtering) or **2×** to **16× Anisotropic**. Applies the next time OrrePort starts. | None |

The frame-rate smoothing changes only what is drawn between the game's frames, never the game itself: recordings of
real play replay with the same random rolls with it off and at every rate.

## Enhancements

### Speed

Speeding up runs more of the game's own steps per second, so every step is the one a console would run. With **Music
when faster** on **Normal tempo**, the game's own sound engine also stays on the console's schedule, so waits for a
sound to end take the same number of steps. That's why the speed settings **work in accurate mode**: recordings of
real play replay with the same random rolls at 3× as at normal speed.

| Setting | What it does | Default |
|---|---|---|
| **Fast-forward** | How fast the game runs while you hold the fast-forward button: **2×, 3×, 4×** or **5×**. | 3× |
| **Fast-forward button** | Opens Controls on the **Fast-forward** row, to choose the button or key. | **LB** (**L1** on PlayStation), **Tab** on the keyboard, none on a GameCube controller |
| **Fast-forward works** | **Hold**: fast while held. **Press to switch**: one press on, one press off. | Hold |
| **Speed badge** | Shows the speed in a corner for about a second when fast-forward starts or the game gets faster. | Show |
| **Game speed** | How fast the whole game runs, without holding anything: **Normal, 1.5×, 2×, 3×, 4×** or **5×**. | Normal |
| **Battles** | Battles can run at a speed of their own: **Same as Game speed**, **Normal**, or **1.5×** to **5×**. | Same as Game speed |
| **Music when faster** | **Normal tempo**: the music keeps its pace, and nothing waits on sounds (Recommended). **Faster too**: music and sound speed up with the game, higher in pitch. | Normal tempo |

The fast-forward button overrides the other speeds while it's held. **Faster too** in accurate mode is likely safe as
well, but hasn't had its own replay check yet.

### Tweaks

Small changes to how the game plays. With a tweak off, that part plays as on the GameCube. **Every tweak is off in
accurate mode**: the row shows a lock and "Off in GameCube-accurate mode".

| Setting | What it does | Default |
|---|---|---|
| **Infinite TMs** | Teaching a TM keeps it, as HMs already do. | On |
| **The right stick works like the left one** | In story mode the right stick (C-Stick) does everything the left stick does: walking, running, battles and every menu. Battle Mode and the P★DA's Pokémon pictures keep its own use. | On |

**Built-in improvements.** Some of OrrePort's changes have no switch: they are always on in normal play and always off
in accurate mode. They include Continue and New Game starting on YES, the party screen showing every portrait at once,
the party list staying open while you use an item, the LEARNED label on a TM's party list, the scents' order and memory,
the Relic Forest shortcuts, fast loading and saving, PC MENU in the pause menu, OrrePort's naming keyboard and
re-wrapped dialogue. The full list, with what each does in accurate mode, is in [accurate-mode.md](accurate-mode.md)
and in the game under **Settings › General › What it turns off**.

## Audio

Sound only: all three work in accurate mode.

| Setting | What it does | Default |
|---|---|---|
| **Master volume** | Everything you hear. | 100 |
| **Music** | The game's songs. | 100 |
| **Sound effects** | Moves, menus and footsteps. | 100 |

The game's own **Options** screen (Sound: stereo or mono, Rumble, and OrrePort's **Language** row for the game's text)
works as on the GameCube and is saved in your save, not in OrrePort's settings.

## Controls

| Setting | What it does |
|---|---|
| **Set up controller** | Opens the controller setup screen: change any button, the sticks, the dead zone, rumble, and OrrePort's shortcuts (below). |
| **Set up GameCube adapter** | Shown only while Nintendo's GameCube adapter is plugged in and Windows can't read it yet. One click sets it up (Windows asks first); afterwards the adapter's controller becomes Player 1. |

### The default controls

**Controllers** use the buttons by **position**, like a GameCube controller, not by the letter printed on them:

| GameCube | Xbox-style | PlayStation | GameCube controller (adapter) |
|---|---|---|---|
| A | A (bottom) | ✕ (bottom) | A |
| B | X (left) | ○ (right) | B |
| X | B (right) | □ (left) | X |
| Y | Y (top) | △ (top) | Y |
| Z | RB | R1 | Z |
| START | Menu | Options | START |
| L / R | LT / RT | L2 / R2 | L / R |
| Control stick, C-Stick, D-pad | left stick, right stick, D-pad | the same | the same |
| OrrePort menu | View | Create | Z + L |
| Fast-forward | LB | L1 | none |

**The keyboard** plays as Player 1, together with a controller:

| GameCube | Key |
|---|---|
| Control stick | W A S D |
| A / B / X / Y | J / K / L / I |
| L / R | Q / E |
| Z | Left Shift |
| START | Enter |
| D-pad | the arrow keys |
| C-Stick | T F G H |
| Walk (half tilt) | Left Ctrl |
| OrrePort menu | Esc |
| Fast-forward | Tab |

**Keys OrrePort keeps for itself** (they can't be given to the game): **F2** Settings, **F8** Report a problem, **F11**
fullscreen (**Alt+Enter** too). With debug tools on: **F1** the debug menu, **F5** / **F9** save and load a checkpoint,
**F12** or **Print Screen** a screenshot, **Pause** stops the game and **.** (full stop) steps one frame
([debug-menu.md](debug-menu.md)).

**The menu bar** appears when the mouse is at the top of the window: **File** (Fullscreen, Open OrrePort folder,
Settings…, Quit to hub), **Cheats** (below) and **Help** (Report a problem…, About OrrePort).

### The controller setup screen

- **Device:** every connected controller and the keyboard. Choosing a controller makes it Player 1's. Button names
  follow the device (Xbox, PlayStation, GameCube adapter, Steam Deck).
- **Profile:** your setups under a name, separate lists for controllers and the keyboard. **Default** (or **GameCube
  Controller** on a GameCube controller) is the built-in one and can't be renamed or deleted; **New profile…**,
  **Rename** and **Delete** manage the rest. Each controller model and the keyboard remember their profile.
- **Buttons:** select one, then press the new button or key. If it already did something else, the two swap.
- **Sticks:** **Up / Down / Left / Right** for each stick (select a row, then push the stick that way; shown as
  Dolphin names them, such as "Left Y+"), **Calibrate** (roll the stick round its edge until **Finish Calibration**
  appears), **Range**, **Dead zone**, **Walk** and its **Speed** (50 % by default), and **Snap to 8 directions**.
- **Triggers:** a controller's analog trigger gives the game the squeeze as it is, and the digital L / R click past
  90 % of its travel, as Dolphin does by default.
- **Rumble:** its strength (a GameCube controller's motor has one strength: **Test** buzzes it). Turning rumble off is
  the game's own Options choice.
- **Shortcuts:** **Open OrrePort menu**, the **Fast-forward** button, and a **Quit to hub** shortcut (none by default).
  The menu and Quit to hub shortcuts can be one button or two held together. The game never sees a shortcut's buttons.
  A fast-forward button that is also a game button asks first; "Use it anyway" takes that button away from the game.
- **Reset** puts the device back to its defaults. **Leaving after a change** asks to save it to the profile or discard
  it; nothing is written if nothing changed.

**In accurate mode** your button mapping works as normal. The sticks keep the GameCube's own range (locked), and the
dead zone can be at most 10, because the game itself ignores tilts within ±10: a dead zone of 10 or less changes nothing
the game sees, while a larger one would change walking speed and so the timing. The right stick's tweak is off.

## Cheats

The **Cheats** menu is in the menu bar. Each cheat works at once, switches off any time, and every switch is written to
a log (and named in problem reports).

- **Instant win** (one attack knocks out), **Instant catch**, **Infinite money**, **Items never run out**.
- **EXP:** Normal, ×2, ×5 or ×10.
- **Put in the PC** (**changes your save**): tops up Master Balls, Rare Candies, Max Revives, Max Elixirs, Full
  Restores or Full Heals to 99 in the PC's Item Storage.
- **Debug menu…** at the bottom, only while debug tools are on.

All cheats are off until you turn one on. They can be used in accurate mode too, but while one is on, the game plays
differently from a GameCube.

## About

| Setting | What it does | Default |
|---|---|---|
| **OrrePort** *version* | The version (the hub and OrrePort Studio share one), with **Check for updates**, then **Download** and **Restart now** when an update is there. | |
| **Download updates automatically** | Off: you're asked first. Either way, an update installs only when you restart. | Off |
| **Check for updates** | Off: OrrePort stays fully offline and never asks for updates. | On |
| **Go back to** *version* | Shown once an older version is kept: OrrePort and Studio both go back to it. Your saves and mods aren't touched. | |
| **Logs folder** | Opens the folder where OrrePort and Studio keep a log of each run. | |

## For modders: where settings are kept

Settings live in **`settings.ini`**, a plain text file in the OrrePort folder (**File › Open OrrePort folder**):
`data\settings\settings.ini` in a release, `user\settings.ini` next to the program in a build from source. Edit it only
while OrrePort is closed. Controller profiles are small text files in the `profiles\controller\` and
`profiles\keyboard\` folders beside it; each save's own accurate-mode setting is kept with that save, not here.

Some keys and their rows:

| Key | Row | Values |
|---|---|---|
| `accurate_new_saves` | GameCube-accurate mode for new saves | 0 off, 1 on |
| `import_ask` | Ask how to play imported saves | 0 / 1 |
| `language` | Language | `auto`, or a language file's code |
| `overlay_pause` | Pause the game while this menu is open | 0 / 1 (likely) |
| `ui_menu_hint` | Menu hint at start | 0 / 1 |
| `menu_full` | In-game menu | 0 side panel, 1 full window (likely) |
| `quit_hub_ask` | Ask before Quit to hub | 0 / 1 |
| `debug_tools` | Debug tools | 0 / 1 |
| `ui_animations`, `menu_bar_always`, `ui_menu_solid`, `ui_window_icon` | Animations, Always show the menu bar, Menu background, Window icon | 0 / 1 |
| `gpu_backend` | Renderer | `auto`, `vulkan`, `d3d12`; `d3d11` also exists, has no row and is untested |
| `render_scale` | Internal resolution | 0 match the window, 1 native, 2 to 8 |
| `frame_smoothing` | Frame rate | `off`, `match`, or a number (60, 120, 144, 165, 240) |
| `vsync`, `msaa`, `anisotropy` | V-Sync, Anti-aliasing, Texture filtering | 0 / 1; 1 or 4; 1 (none), 2, 4, 8, 16 (likely) |
| `speed_ff` | Fast-forward | 0 2×, 1 3×, 2 5×, 3 4× |
| `speed_ff_mode` | Fast-forward works | 0 Hold, 1 Press to switch |
| `speed_badge` | Speed badge | 1 Show, 0 Hide |
| `speed_game` | Game speed | 0 Normal, 1 1.5×, 2 2×, 3 3×, 4 5×, 5 4× |
| `speed_battle` | Battles | 0 Same as Game speed, 1 Normal, 2 1.5×, 3 2×, 4 3×, 5 5×, 6 4× |
| `speed_sound` | Music when faster | 0 Normal tempo, 1 Faster too |
| `infinite_tms`, `cstick_walk` | The two tweaks | 0 / 1 |
| `menu_pad`, `menu_keys` | The menu shortcut (controller, keyboard) | empty for the default |
| `ff_pad`, `ff_keys` | The fast-forward button (controller, keyboard) | empty for the default, `none` for no button |
| `walk_key`, `walk_pct` | The keyboard's walk key and the walk speed | a key code; 10 to 90 (default 50) |
| `update_check`, `update_auto_download` | Check for updates, Download updates automatically | 0 / 1 |

The speed codes aren't in the order the rows show them: 4× was added after 5×, so it took the next free code. A key
that isn't in the file takes its default.
