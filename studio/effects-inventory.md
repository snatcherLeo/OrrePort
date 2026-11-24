# The Pokémon's own effects: inventory

*Started 2026-11-19, on snatcherLeo's word: "Make note of the pokemon that have particles (Vaporeon, Charizard, etc).
I'll eventually have it so that, particles like that, can be given to any 3D model users want, but for now, we just keep
inventory."* Private (Studio's planning). Regenerate the numbers with `orrestudio-cli survey` (columns `gpt1`,
`ptcl_joints`, `billboard`), `orrestudio-cli ptclkeys <species>` and `orrestudio-cli texlog <species>` (lines marked `glow`).

## Why this list exists

Some battle models carry effects of their own beside the mesh: a **particle bank** (the game's GPT1 data inside the
Pokémon's PKX file; emitters started by animation keys on a joint, then run by HSD's particle code) or a **glow card**
(a camera-facing part drawn see-through without writing depth, such as the sparkle on Espeon's forehead gem; not a
particle, but Studio's Particles switch hides it too, snatcherLeo, 2026-11-19). snatcherLeo's later plan: let modders give effects like
these to any model. So this list records which species have them, what each does, which joint carries it and which
animations start it. That is what a "give this effect to another model" feature needs to copy: the bank, the emitters,
the joint to attach to and the animation keys.

## Particle banks (12 species, all verified from the disc)

Counts from `ptclkeys`: **idle keys** = emitter starts in the idle animation (what Studio's Overview shows);
**animations** = how many of the model's animations start any emitter; **emitters** = distinct emitter records used.
"What it is" is what Studio draws (verified where marked "seen", against the game's move-viewer shots or snatcherLeo's eyes),
otherwise likely from the picture and the joint.

| Species | Bank (bytes) | Joints | Idle keys | Animations | Emitters | What it is |
|---|---|---|---|---|---|---|
| Charmander | 4,004 | 48 | 1 | 8 of 11 | 17 | tail flame (seen; checked against the game 2026-11-19) |
| Charmeleon | 4,960 | 22 | 1 | 8 of 11 | 24 | tail flame (likely) |
| Charizard | 5,356 | 54, 56 | 1 | 10 of 13 | 27 | tail flame (seen) |
| Gastly | 3,728 | 16 | 1 | 5 of 8 | 12 | its gas, with its two glow cards (below) (likely) |
| Koffing | 2,408 | 18 joints (7-41, odd) | 10 | 5 of 8 | 2 | gas from the vents round its body (likely; live counts match the game, verified) |
| Weezing | 2,540 | 15 joints | 15 | 7 of 10 | 3 | gas from both heads' vents (likely; counts verified) |
| Magmar | 4,696 | 91 | 1 | 8 of 11 | 22 | flames (likely; counts verified) |
| Vaporeon | 2,508 | 62 | 1 | 7 of 10 | 3 | sparkle (seen; counts within the game's range, verified) |
| Articuno | 8,028 | 46, 53 | 4 | 10 of 13 | 15 | icy sparkle (likely; count verified 30.1 vs the game's 30.4) |
| Moltres | 3,680 | 67, 73, 81 | 4 | 10 of 13 | 24 | its fire (likely; count verified 95 vs 83-96) |
| Magcargo | 772 | 70 | 1 | 4 of 7 | 1 | likely its smoke or heat; also has a billboard part (not a glow card) |
| Torkoal | 10,428 | 46, 60, 62, 64, 66 | 0 | 5 of 6 | 12 | smoke from its shell (likely); **not drawn yet**: its emitters use the point keys (HSD track type 0x2A) Studio doesn't run yet, and none start in the idle |

No species uses a static particle joint (`ptcl_joints` is 0 everywhere, verified): every effect starts from an animation key.

## Glow cards (5 species, verified from the disc; pictures checked in 2026-11-19)

| Species | Cards | What it is |
|---|---|---|
| Gastly | 2 | the purple gas haze round its body |
| Chinchou | 2 | the light round its two lures |
| Lanturn | 2 | the light round its two lures |
| Espeon | 1 | the sparkle on its forehead gem |
| Wailmer | 1 | not visible on the idle's first frame (likely its spout or a shine; to look at) |

Rule (Studio's reader, `OrreMesh.glow`): a part on a billboard joint (the joint flags 0xE00 / 0x2000) whose material is
drawn blended without depth writes (`RENDER_XLU` and `RENDER_NO_ZUPDATE`). 13 other species have billboard parts that
are not glow cards (Sandslash, Grimer, Muk, Ampharos, Jumpluff, Magcargo, Stantler, Sceptile, Breloom, Skitty, Delcatty,
Grumpig, Anorith): those are drawn as ordinary parts and the switch leaves them alone.

## For the "give an effect to any model" feature (later; notes, not a design)

- A particle effect is: the bank's emitter records and their pictures, the joint it's attached to and the animation keys
  that start it (frame, emitter id). To move it to another model: copy the emitters and pictures into that model's bank,
  pick a joint on the new skeleton, and add keys to the chosen animations (likely a per-slot "start at frame N" list).
- A glow card is a mesh part with its own material on a billboard joint: moving it means adding a joint (or picking one)
  and a small textured quad.
- The game draws a model's own particles through its bank only (`gsParticlePtclHook` looks the emitter up in the bank of
  the model that owns the joint; likely, from the particle-hook log's "the model's particle bank" lines), so an effect given to another model has to travel with that
  model's file (or a mod's added bank), not stay in the original's.
