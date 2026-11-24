# Hook points

OrrePort changes the original game only through small, named **hook points**. This page says what a hook point is,
how to add one, and lists every hook in the game code today.

## What a hook point is

A hook point is a named place inside the decompiled game code (`src/game`, `src/hsd`, `src/musyx`) where OrrePort can
step in. Each one is called `OrreHook_<What>`, where *What* names the idea (a fade's length, a move's damage), not the
game function it sits in.

- **The default is the original game.** A hook is a function pointer (or, for a few, a plain variable). When nothing
  is registered, it is empty, and the game does exactly what it does on the GameCube.
- **The feature lives outside the game code.** An enhancement, a cheat, a test tool or a mod loader is written in
  `port/`, and registers for the hook when it is switched on. The game code only holds the hook itself.
- **Nothing is patched in place.** With every feature off, OrrePort plays like the GameCube version.

There are two main kinds:

- **Decision hooks** return a value the game then uses: a fade's length, where a cursor starts, how much money a shop
  takes. With nothing registered, the game's own value is used.
- **Observation hooks** only report something (a room was entered, a flag changed, a message was shown). With nothing
  registered, nothing happens.

**What is not a hook:** fixes that make the PC do what the GameCube does (64-bit pointers, byte order of disc data,
real struct fields in place of raw offsets, a missing argument taken from the original program). Those make the port
correct; they don't add behaviour, so they stay as plain code.

## Hooks and GameCube-accurate mode

[GameCube-accurate mode](accurate-mode.md) promises the same timing, random numbers and button presses as a console.
Hooks keep that promise this way:

- **Decision hooks are empty in accurate mode** unless the change is look-only or sound-only (a font, a language, a
  volume): it draws no random numbers, changes no timing (no frame counts, load or fade lengths) and reads no
  different inputs. Each hook that stays on has its line in the accurate-mode list and is covered by the replay test.
- **Observation hooks may stay on** (read-only tools, such as an RNG viewer, are useful to RNG hunters). So an
  observation hook, and everything it calls, must never draw a random number, change the game's state or change its
  timing.
- **Checked, not assumed:** the determinism test replays recordings of real play with a hook on and off, and compares
  every random roll and room.

## Adding a hook

1. **Start with a discussion** if it is a new feature ([CONTRIBUTING.md](CONTRIBUTING.md)).
2. **Declare it in the hooks header,** `include/pc/orre_hooks.h`, with a comment saying where it is called, what it is
   given, what it returns, and the original behaviour when it is empty. Most hook pointers are defined in
   `port/src/pc_cheats.c`, the port library that is always linked into the game. (A few older hooks are still declared
   next to the code that uses them; new ones go in the header.)
3. **Call it from the game code** under `#ifdef TARGET_PC`, at the smallest place that works, and only read what it
   needs when a hook is registered. The GameCube build must stay byte-identical (its SHA-1 check passes).
4. **Write the feature in `port/`,** and register for the hook only while the feature is on. If another feature
   already uses the hook, chain to it (call the one that was there before) rather than replacing it.
5. **Say what it does in accurate mode:** off, the same, or on because it changes only the look or the sound. Add its
   line to the accurate-mode list ([accurate-mode.md](accurate-mode.md)).
6. **Add its row to the list below in the same change:** its name, what it lets you change, and its accurate-mode
   behaviour.
7. **Comment why** the hook exists, and note anything specific to one operating system.

## The hooks today

Names are as they appear in the code. Where a hook goes with a partner (a setter, a second hook that always comes
with it), both are in one row. "Test tools only" means only the testing and replay tools register for it; a player's
game never sets it.

### Testing and replays

| Hook | What it lets you change or see | Accurate mode |
|---|---|---|
| `OrreHook_WindowReady` | Reports which window just started reading buttons, so recorded button presses can be timed from when a menu opens. | Observation; test tools only |
| `pc_room_entered_hook` | Reports each room as it starts loading (room-keyed test scripts; later a story-progress meter). Older name; it follows the `OrreHook_` pattern in a later change (likely). | Observation |
| `OrreHook_FixedRngSeed`, `OrreHook_FixedRngSeedSet` | The random-number seed set once at boot (normally the clock), so a PC run can start from the same seed as a Dolphin run. | Test tools only; accurate mode keeps the clock seed |
| `OrreHook_RngFrameState` | Replaces the random generator's state before its next step, once, so a replay can follow a console's numbers frame by frame. | Test tools only |
| `OrreHook_RngBeforeDraw` | Lets a replay give each battle draw the console's own number, or record the PC's numbers for comparison. | Test tools only |
| `OrreHook_FrameRetraces`, `OrreHook_FrameStep`, `OrreHook_FrameSkipWait` | How long a game frame lasts and how far it steps, so a replay can copy the console's slow frames. | Test tools only |
| `OrreHook_TalkTarget` | Who the A button talks to, once, so test routes can run every character's talk script. | Test tools only |
| `OrreHook_SkipMovies` | A switch that skips movies (a debug tool). | Test tools only |
| `OrreHook_OldCryIndex` | Reads a cry index the older, wrong way, so recordings made before that fix still replay. | Test tools only |
| `OrreHook_FirstRoom` | The first room the game opens (the move viewer instead of the title, for reference shots). | Debug switch only |
| `OrreHook_WazaViewerThread`, `OrreHook_WazaViewerDraw` | Drives the game's own move viewer (its developer menu is missing from the retail disc) and draws its overlays. | Debug switch only |
| `OrreHook_Ui2DDraw` | Reports every 2D piece drawn (its place and size), for the widescreen groundwork. | Observation; test switch only |

### What happened in the game (the event log)

| Hook | What it reports | Accurate mode |
|---|---|---|
| `OrreHook_BattleMessage` | The battle message the text box was asked for. | Observation |
| `OrreHook_MessageMeasured` | The full text of a battle message, with names filled in. | Observation |
| `OrreHook_FlagChanged` | A story flag's number and its value before and after. Also read by the autosave. | Observation (the autosave itself is off) |
| `OrreHook_TalkStarted` | Who the player talks to. | Observation |
| `OrreHook_CharacterTalk` | Who says each line in the field, cutscenes included. | Observation |
| `OrreHook_HeroEventScript` | When a script for the player starts and ends, so a stuck script can be noticed. | Observation |

### Battle start

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_BattlePreloadPortraits` | Loads the lead Pokémon's portraits early, so the send-out doesn't wait (smooth battle start). | Off |
| `OrreHook_BattleStatusEarly` | Shows every HP box as soon as the last Pokémon has landed. Behind a test switch for now. | Off |
| `OrreHook_PortraitDeferRequest`, `OrreHook_PortraitRequestRefused`, `OrreHook_PortraitWaitRestart` | Keep an early portrait request from being lost while other files load. | Off (used only with smooth battle start) |

### Cheats

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_MoveDamage` | A damaging move's final damage (Instant win). | Off |
| `OrreHook_CatchCertain` | Makes a thrown ball always catch, as the Master Ball does (Instant catch). | Off |
| `OrreHook_MoneyTaken` | How much money is taken (Infinite money). | Off |
| `OrreHook_ItemUseKeeps` | Keeps an item when it is used (Items never run out). | Off |
| `OrreHook_ExpGained` | A Pokémon's EXP after every bonus (EXP ×2, ×5, ×10). | Off |

### Text, fonts and languages

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_GlyphFetch` | A letter's picture before it is drawn (open fonts, a test switch). | On (look only) |
| `OrreHook_GlyphCode` | Reports the letter about to be drawn, so the sharp-text renderer picks its tracer. | On (look only) |
| `OrreHook_GlyphMissing` | A letter the game's font lacks (normally a white box), drawn from another font instead. | On (look only) |
| `OrreHook_MessageText` | The text of any game message, in the chosen language (from the player's own European or Japanese disc). | On (look only) |
| `OrreHook_MessageExtra` | OrrePort's own messages, shown by the game's text code inside the game's screens. | On (look only) |
| `OrreHook_MessageForCopy` | Keeps species names in US English while the game copies or compares them, so saves stay correct in any language. | On |
| `OrreHook_MessageBox` | Re-wraps a dialogue line that would run past the box's edge. | Off |
| `OrreHook_MessageInsert` | A name or number inserted into a message (a test switch today; later, long names). | Off |
| `OrreHook_MessageFit`, `OrreHook_MessageFitRect`, `OrreHook_PrintPlace` | Shrinks or narrows a long name so it fits a fixed spot. | Off for the game's own prints; on for OrrePort's own Options row and, on the Options screen only, its values |
| `OrreHook_MenuNameCopy` | Reports where a nickname was copied for a menu, so its full long name can be shown. | Observation |
| `OrreHook_MenuExtra`, `OrreHook_MenuSprite` | Menu items and pictures past the game's tables, or another language's picture in place of a game one. | On (look only) |
| `OrreHook_ItemSpriteBox`, `OrreHook_CursorSpriteAt`, `OrreHook_StadiumHeader` | Where another language's word pictures, the menu glove and Battle Mode's stadium header sit, as that language's game places them. | On (look only) |
| `OrreHook_ResourceMissing` | A texture the game asks for that isn't loaded: a language's picture read from that language's disc. | On (look only) |
| `OrreHook_FsysCacheRoom`, `OrreHook_FsysCacheReserved` | Keeps the game's file cache making the US game's choices while a language's files are a different size, so loading times and random numbers stay the same. | On (keeps loading identical; checked by replay) |

### The naming screen

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_NameEntryInput` | The keyboard's input (OrrePort's keyboard: typing, pages, Shift). | Off: the game's own name screen |
| `OrreHook_NameEntryDrawGrid`, `OrreHook_NameEntryCursorBox`, `OrreHook_NameEntryDrawLabel`, `OrreHook_NameEntryPreview` | The letter grid, its cursor box, the button labels and the blinking preview letter. | Off |
| `OrreHook_NameEntryPiece`, `OrreHook_NameEntryDrawName` | The name field: twelve slots in place of the game's shorter field. | Off |
| `OrreHook_NameEntryCommit` | Reports the name stored, so the full long name can be kept beside the GameCube one. | Off |
| `OrreHook_NameEntryListBack`, `OrreHook_NameEntryCaller`, `OrreHook_NameMenuCanLeave`, `OrreHook_NameMenuLeft` | Lets B leave the player's name menu on New Game (with shorter fades back). | Off: B does nothing there, as on the console |

### Title, main menu, Options and memory card

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_NewGameInitialAnswer` | Where New Game's YES / NO cursor starts. | Off: the game's NO |
| `OrreHook_ContinueInitialAnswer`, `OrreHook_ContinueQuestion` | Where Continue's cursor starts, and the question shown after a save is picked from the list. | Off: the game's NO and its own question |
| `OrreHook_ContinueSpot` | Continue goes on from the exact spot a save was made with PC MENU (or an autosave). | Off: the save's own return point |
| `OrreHook_TopMenuOpen`, `OrreHook_TopMenuChoice` | OrrePort's LOAD entry on the main menu, and what choosing it does. | On, listing only saves set to accurate mode |
| `OrreHook_TopMenuHelp`, `OrreHook_TopMenuHelpDrawn` | The main menu's help lines (CONTINUE naming the loaded save), and the Accurate badge drawn beside CONTINUE's line. | The game's own lines; the badge shows only in accurate mode |
| `OrreHook_MenuDataExtra` | Whole menus OrrePort adds past the game's menu table. | Nothing registers yet |
| `OrreHook_OptionsScreen`, `OrreHook_OptionsKeys` | The Options screen's Language row: its layout, its keys, and saving the choice. | On (look only) |
| `OrreHook_OptionsCancelKeeps` | B keeps the Options screen's choices instead of putting them back. | Off: B cancels |
| `OrreHook_CardTaskStart`, `OrreHook_CardTaskEnd` | Before and after every memory-card task: puts back a save file deleted while the game ran, keeps the long-name file beside a save, ties autosaves to their save. | The parts that would change a save's contents are off (likely: each feature checks the mode itself) |
| `OrreHook_CardMessageSkip` | Skips "Your save file has been loaded!" where the next screen already says it. | Off |

### The pause menu, the PC and leaving rooms

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_FieldMenuOpen`, `OrreHook_FieldMenuItem`, `OrreHook_FieldMenuChoice`, `OrreHook_FieldMenuSpot`, `OrreHook_FieldMenuReopen` | PC MENU in the pause menu: its button, what it opens, where the player stood, and whether the pause menu opens again. | Off |
| `OrreHook_MenuBackKeys`, `OrreHook_MenuWrap` | Start closes the pause menu; its cursor wraps round. | Off |
| `OrreHook_FieldMenuUnduckMusic` | Keeps the field music down when leaving for the main menu. | Off |
| `OrreHook_PcMenuOpen`, `OrreHook_PcMenuChoice`, `OrreHook_PcMenuCursor`, `OrreHook_PcMenuRowPlace`, `OrreHook_PcMenuWheel` | Main Menu on the PC screen: its row, what it does, the wrapping cursor and the turning wheel of icons. | Off: the game's four rows |
| `OrreHook_PcMenuExit`, `OrreHook_FadeDuration`, `OrreHook_FadeHold`, `OrreHook_WindowCloseNow`, `OrreHook_PcMenuCloseTogether` | Leaving a PC screen faster: shorter fades, a screen held black, windows closed once the screen is black. | Off |
| `OrreHook_PcSaveStep` | Steps of the PC's Save, so OrrePort can warn before a save in a spot that could leave the player stuck, and save safely there. | Off |
| `OrreHook_RoomLeave` | A room change OrrePort asks for (back to the main menu); also the per-frame check for the autosave and the stuck-script watch. | Off |
| `OrreHook_RoomEntryDone` | Runs once a room's entry scripts have finished, so a person can be put back in the pose a saved spot needs. | Off |
| `OrreHook_AfterGappUpdate` | More work after each frame's update tasks (finishing file loads sooner after a PC screen). | Off |

### Bag, party and items

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_ItemUseAgain`, `OrreHook_PartyListHold` | Keep using an item: the party list stays open with the item still chosen. | Off |
| `OrreHook_PartyCursorStart` | Where the party list's cursor starts (the Pokémon last chosen). | Off |
| `OrreHook_TmSkipAsk`, `OrreHook_TmUsedCount`, `OrreHook_TmLabel` | Skipping a TM's question when trying again; TMs that aren't used up; a LEARNED label. | Off |
| `OrreHook_ScentListOpen`, `OrreHook_ScentUsed` | The scent list's order and its remembered choice. | Off |
| `OrreHook_BagOptions` | The bag item's options (no useless USE; READ, CHECK, TOSS where they help). | Off |
| `OrreHook_PauseMenuStep`, `OrreHook_PartySubmenuOpen`, `OrreHook_PartySubmenuChoice`, `OrreHook_RelivePickSlot` | PURIFY in the party menu in the Relic Forest, and the Time Flute there. | Off |
| `OrreHook_PartyPortraitWait`, `OrreHook_FaceDrawHold`, `OrreHook_FaceStandIn` | The party screen's portraits shown at once (Smooth Party UI). | Off |

### Controls and sound

| Hook | What it lets you change | Accurate mode |
|---|---|---|
| `OrreHook_PadSticks` | The sticks as the game reads them: the right stick works like the left one. | Off |
| `OrreHook_WalkStick` | The walking stick's values (today only a test log). | Off |
| `OrreHook_SoundVolumeScale` | The Music and Sound effects volume sliders. | On (sound only) |

### Frame smoothing

These hooks only report what is being drawn, so the renderer can make in-between pictures. They never change the
game.

| Hook | What it reports | Accurate mode |
|---|---|---|
| `OrreHook_CameraSet` | The camera the next draws use. | Observation; smoothing changes pictures only |
| `OrreHook_ModelDraw` | The model piece the next draws belong to. | Observation |
| `OrreHook_ParticleDraw` | The particle the next draws belong to. | Observation |
| `OrreHook_2DPiece` | Named 2D pieces (HP and EXP bars, menus, the screen fade). | Observation |

## Planned hooks

None of these exist yet:

- **`OrreHook_MoveCategory`:** a move's physical or special category, for a physical / special split rule set.
- **`OrreHook_FsysEntryLoaded`:** a file inside an archive, after it is read, for the mod system's file overrides.
- **`OrreHook_ModuleLinked`:** after a code module is loaded, for changing data tables in place.

A few older test switches inside the game code still read their setting directly. They are being turned into
observation hooks, with the tool itself moved to `port/`.
