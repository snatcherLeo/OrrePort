# Contributing to OrrePort

Thank you for wanting to help. OrrePort welcomes pull requests in many areas; this page says where, and the rules
every pull request meets. Please read the [design guide](design-guide.md) first: it lists the decisions that rest with
the maintainer, snatcherLeo, and stay out of pull requests.

## Where pull requests are welcome

- **Fixes in the port:** crashes, wrong pictures, wrong sounds, anything that behaves differently from the GameCube
  version.
- **Graphics:** the renderer (Aurora) and its backends, and frame smoothing.
- **Performance:** loading, frame times, memory.
- **Enhancements behind hook points:** new options that leave the original game unchanged when they're off (start with
  a discussion; see below).
- **Tools:** the disc readers, the testing and replay tools, the build.
- **Documentation:** how the game works, how the port works, how to build.

**Bigger ideas start as a discussion,** before any code: a new feature, a new setting, anything that touches the
user interface or a file format. That saves you work on something that can't be merged as it is.

## The rules every pull request meets

1. **The original game stays the default.** A change in how the game behaves goes through a hook point that does
   exactly what the original does unless the player switches it on.
2. **GameCube-accurate mode is respected.** Say what your change does in accurate mode and why, and add its line to
   the accurate-mode list. A change that could alter the random numbers or the timing is off in accurate mode unless
   the determinism test proves it changes nothing.
3. **The determinism test passes.** The reference playthrough's recordings replay in the port with the same random
   numbers and the same rooms as on a console. Changes to the game code or the timing must keep that true.
4. **The GameCube build stays byte-identical.** Changes inside the decompiled game code must still build the original
   program exactly (its SHA-1 check passes).
5. **No game data.** Never commit files from the disc, files extracted or generated from it, saves or recordings.
   OrrePort reads everything from the player's own copy while it runs.
6. **Every word is translatable.** Text a player sees gets a key in the language files, never a string in the code.
7. **Code says why, and for which system.** Comment why the code exists; mark anything specific to one operating
   system and say what the others would need.
8. **Credit.** Say where an idea or a piece of code came from, and keep other projects' licences and credits intact.

## Names in the code

Many functions and variables have been renamed from their placeholder names (`fn_8001C3A4`) to names that say what
they do, in batches, as the evidence for each became certain. A map of every rename (old name, new name, address) and
a small script that applies it ship with the repo, so code written against older names can be brought up to date.
Placeholder names always contain the address, so nothing is lost.

## Reporting a problem

In OrrePort or OrrePort Studio, press **F8** (or use **Report a problem**). It saves a small zip with your note, the
logs and a screenshot, and copies it so you can paste it straight into a message. Share it on the OrrePort Discord.
Your Windows user name and folders are removed from it, and it never holds game files.

## Review and licence

- snatcherLeo reviews and merges every pull request; the main branch accepts changes only through pull requests.
- OrrePort's own code is under the MIT licence; by contributing, you agree your contribution is under it too. The
  decompiled base keeps its own licence and notice, and every library keeps its own.
