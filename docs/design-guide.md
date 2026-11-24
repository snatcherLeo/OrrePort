# OrrePort's design guide

This page says what OrrePort is meant to be, and which decisions rest with the project's maintainer, snatcherLeo,
rather than with pull requests. If you want to contribute, read this first, then [CONTRIBUTING.md](CONTRIBUTING.md).

## What OrrePort is

OrrePort is a native PC port of Pokémon Colosseum, built from the game's decompiled code, together with **OrrePort
Studio**, a tool for viewing the game's data and, later, making mods. You play it from your own copy of the game: no
game files come with it.

Three ideas shape everything in it:

1. **The original game first.** With nothing switched on, OrrePort plays like the GameCube version. Every enhancement
   is the player's choice, and **GameCube-accurate mode** keeps the timing, the random numbers and the button presses
   exactly as on a console ([accurate-mode.md](accurate-mode.md)).
2. **Native, offline and the player's own.** OrrePort is a desktop program made for each system it runs on, not a
   web page in a window. It works without the internet; nothing about you or your play is sent anywhere. Your saves,
   settings and mods are ordinary files in your own folder, and you can take them with you.
3. **Care in the details.** A feature isn't finished when it works: it has to feel right on screen, hold up over a
   whole playthrough, be translated, be tested against the original, and be written down.

## Decisions that rest with the maintainer

These are not open to pull requests. Suggestions are welcome as a **discussion** (see below), but the final call is
snatcherLeo's, so that OrrePort keeps one consistent look and direction:

- **The user interface:** the hub, the Settings pages, the screens and buttons OrrePort adds inside the game, and
  OrrePort Studio's layout and workflow.
- **The look:** themes, colours, fonts, icons, the logo and everything else in OrrePort's own visual style.
- **Naming:** what features, settings, menus, files and folders are called, in every language.
- **The file formats:** the `.orresave` save format ([orresave-format.md](orresave-format.md)), the `.orremod` mod
  format (published once mods load in the game), and the layout of the portable folder.
- **What is on by default,** and what GameCube-accurate mode allows.
- **The order of work:** what gets built next, and when releases go out.

## How changes to the game are made

OrrePort changes the original game only through small, named **hook points**: places in the game code where an
enhancement or a mod can step in. Every hook does exactly what the original game does unless something is switched
on, so the game itself stays the reference. Features live outside the game code and register for a hook; nothing is
patched in place.

Two lists keep the promises above honest, and every change that touches them adds its line:

- **The accurate-mode list:** for each change in how the game behaves, what it does in the normal mode and whether
  it is on, off or the same in GameCube-accurate mode, and why.
- **The GameCube copy's rules:** for each thing a save can hold that a GameCube save can't (a longer name, for
  example), what OrrePort keeps and what "Make a GameCube copy" writes instead, so any save can always go back to a
  console or Dolphin ([saves.md](saves.md)).

## Words, languages and platforms

- **Every word a player sees is translatable.** No text is written into the code: each line has a key in the language
  files, with a note on where it shows. Sentences are written whole, with placeholders, never glued together from
  pieces. OrrePort's official languages are English, German, French, Italian, Spanish and Japanese.
- **Windows first, then Linux and macOS.** Code that only one system needs stays in the platform layer, with a note on
  what the other systems would need instead.
- **Code says why.** Every piece of code carries a comment explaining why it exists, not only what it does.
