# Attach points: how Colosseum puts move effects on any Pokémon, and what an imported model needs

*Written 2026-11-19, on snatcherLeo's word: "document the attach point stuff. We need to know EVERYTHING/enough
about these 3D models, so that, when we import 3D models from other Pokémon games, we can compare, and see how to
make those new 3D models still work within Colosseum's battle system for how it assigns moves to attach points".
Private (Studio's planning; the engine facts went to C for `docs/engine/models-animation.md` and `wzx.md`). The full
research, with code references and the survey's numbers, is `studio/attach-points-map.md`. Labels: verified (read in
code or data), likely, guess.*

## The idea in one paragraph

No move is made for a Pokémon. A move's effect file (WZX) names **an animation slot** ("ranged attack") and, per
effect, **an attach point** ("mouth", "body centre"). Every Pokémon's file (PKX) has its own answer to both: which of
its animations fills each slot, and which of its joints is each point, per slot. So Quilava using Thunderbolt plays
Quilava's own ranged-attack animation, with the sparks on Quilava's own joints (verified: the slot table and the
point lookup, `wazaSequenceEntryGetAttachPart`). The one exception is the per-Pokémon move versions (367 of them,
such as Pikachu's own Thunderbolt), which the game looks up first.

## The 16 points

Stored per animation slot, as 16 signed numbers at slot record +0x4C, each a joint numbered in depth-first order from
the root (root = 0, children before siblings), as `GSmodelGetPart` counts them (verified). The names come from which
moves use each point and from where the joints sit across all 420 Pokémon models (likely unless marked):

| # | Point | Typical use |
|---|---|---|
| 0 | root / ground (always joint 0; verified) | Rain Dance, Dig, ball send-out and return, stat rings |
| 1 | mouth | Fire Blast, Bubble, Blizzard, Yawn, the sleep "Z"s, roar camera |
| 2 | body centre / hit point | Thunderbolt, nearly every hit; the move camera's default target |
| 3 | tail tip (0 when there is no tail) | Poison Tail, Smeargle's Sketch |
| 4 / 5 | eyes (+X / -X) | Fake Tears, Lock-On, Detect, Foresight |
| 6 / 7 | hands or front limbs; 7 is the striking hand (+X / -X) | 7: every punch, claw and throw; 6: the Snag Ball throw, Outrage |
| 8 / 9 | a second pair of limbs (guess) | only Machamp's Revenge |
| 10 / 11 | a third pair of limb points (guess) | no move |
| 12 / 13 | feet (front feet on quadrupeds) | Blaze Kick, stomps, the Shadow / Hyper Mode aura |
| 14 / 15 | rear feet or heels | Grudge |
| 16 (not stored) | the bounding box's centre, added at run time (verified) | Transform, Substitute, purification |

Facts that matter for importers (verified from the data unless marked):
- Points 1, 2, 4 and 5 are always **bare locator joints** (no mesh), hung under the bone that moves them; the hands
  and feet mostly are too. An effect follows its joint every frame, so a locator on the right bone moves with every
  animation.
- A species without a feature points it at **0** (the root), never -1, in the slots it uses; -1 appears only in
  empty extra slots. A missing point (-1, or past the joint count) gets no fallback: the effect likely plays at the
  world origin, and the developers' move viewer prints "Invalid Null Joint data!".
- The table is **per slot**, and some motions really point a hand or foot at another joint (a punch with the other
  hand).
- Effects read only their own Pokémon's points: the attack half uses the user's, the hit half the target's.

## Two more header fields an imported model must set (verified; correct older guesses)

- **Size class** (PKX header +0x0C, -2 to 3): scales every move effect (x0.5, 0.75, 1, 1.333, 2, 3.25), sets the
  move camera's distance, and sizes the sleep and freeze loops. Choose it from the model's height (survey medians:
  -2 about 3.5 units tall, -1 about 7, 0 about 15, 1 about 19, 2 about 28 and up, 3 Wailord-size; likely).
- **Eye texture animations** (header 0x18 / 0x1C / 0x20): eyes closing for sleep, opening on waking, and the idle
  blink. -1 when the model has none (its eyes then stay open; nothing breaks).

## What an imported model needs (the importer checklist; likely, from the mechanics above)

1. A joint tree whose order is fixed when written: points are stored as walk positions, not names. Studio keeps its
   own name -> index map and recomputes the 16 numbers whenever it writes a model.
2. Joint 0 is the root, on the ground under the body.
3. Locator joints for at least: 0, 1, 2 (camera and most effects), 4 / 5 (eye moves), 7 and 6 (punches, claws,
   throws), 12 / 13 (feet). Then 3 (tail), 14 / 15. Points 8-11 are 0 unless the species has extra limbs.
4. Absent features point at 0, not -1.
5. The same table in every used slot, unless a motion should lead with another bone.
6. Colosseum's units and axes: +Y up, facing +Z; Pikachu about 3.8 units tall, Charizard about 17, Wailord about 98.
7. The size class to match, and a tight bounding box (stray far-off geometry moves the camera and point 16).
8. Optionally the three eye texture animations.

What goes wrong otherwise: fire from the tail, punches from a foot, effects frozen while the body moves (a locator
on a bone that doesn't animate), effects hidden inside the body, effects and camera the wrong size.

## In Studio (planned; matches the project plan's decided items)

- An attach-points view per slot: the 16 points as named dots on the model (the developers' viewer draws them as red
  dots), an "n of 16" check, warnings for -1 in a used slot or an index past the joint count, and "copy slot 0's
  table to the other slots".
- Studio names points; the PKX writer turns names into indices on export.
- The per-game importer checklists (snatcherLeo's own mapping work) list every Pokémon with its points in these names.

## Open questions (from the research)

1. What points 8-11 really are (check a few models in the viewer with the dots).
2. Whether +X is the model's left (likely).
3. Where an effect on a missing point really appears (likely the world origin).
4. Whether any vanilla move hits one of the 17 models with out-of-range indices (Beldum loses its mouth, for one).
5. Why the screen-distortion effect reads slot 0's point 2 instead of the current slot's.
