# OrrePort Studio: the Moves tab, a plan

**Status:** *version 1 built 2026-11-19* (snatcherLeo: "Build version 1 as planned"; how it was done is in Studio's design log,
"The Moves tab, version 1"). Changed from the plan while building: Plain stage frames the Pokémon to fill the stage at
the game's angle, and can be turned (the game's own distance shows every Pokémon tiny); moves aimed at the user take
their effect from the general table's part 3. Written first as a plan for snatcherLeo to read before anything was built
(2026-11-19). snatcherLeo, through C: "Send it to studio, see HOW
it COULD implement something like this for each Pokemon, and for special cases "unown"/pokemon with multiple forms, we
can figure that out afterwards". The mockup is `docs/design/mockups/25-pokemon-moves-tab.html` (mockup 25).

Labels: **verified** (from the game's code or data, in the engine docs), **likely**, **guess**.

## 1. The learnset and the move data: how they reach the page

Everything comes from the player's disc, through Studio's reader (the shared core). Nothing new on the disc is needed.

| What | Where on the disc | Studio today | To add |
|---|---|---|---|
| Level-up moves | species record +0xBA, 20 × {level, move} (verified) | read | none |
| TM / HM | species record +0x34, 58 bytes, one per machine (TM01-50, HM01-08); 0 = can't learn (verified, `include/game/pokemon_data.h`, `bag-and-pc.md`) | raw bytes only | decode; the machine → move table `item_waza_machine_no` in `main.dol` (0x1D0 bytes, 8-byte records; likely) |
| Egg moves | species record +0x74, 8 × u16 move ids (verified, same header) | raw bytes only | decode |
| Move tutors | none in Colosseum (verified); XD has 12 | none | only once XD is a source game |
| The moves themselves | the move table in `common_rel` (`common.fsys` 0x01C7), 358 records of 0x38 bytes: priority, PP, type, target, accuracy, power, effect (verified, `pokemon-data.md`) | not read | a move reader: `orre_move_get(move)` |
| Physical or special | Gen 3's split by type: the type table's category byte (`lbl_8035B500` +0x00: 1 physical, 2 special; verified, `battle-types.md`) | not read | read the byte (it's data, so a mod that moves a type moves its moves) |
| Names | move record +0x20, a message in `common_rel`'s banks (verified) | Studio reads messages already | none |
| Descriptions | move record +0x2C, the message bank in `main.dol` (verified) | not read | read that bank |

So the left side is about a day's core work: three small decoders and a move reader, each checked against the
Python extractor in the golden tests, as the species reader is.

## 2. The preview: playing a move on the Pokémon

**How the game does it** (verified, `models-animation.md`, `wzx.md`):
1. A move's visual is a **WZX sequence** (a timed list of model, particle, screen, sound and camera entries). It's found
   through tables in `main.dol`:
   - **First, the Pokémon's own version:** the per-Pokémon table (`lbl_80373750`, 367 records keyed by the model id
     and the move: Pikachu's Thunderbolt, Espeon's and Umbreon's Sand-Attack…).
   - **Otherwise, the general one:** the general table (`lbl_8036E150`: move id → attack, damage and third part).
2. The sequence's header says which **animation slot** the user plays. The PKX lists 17 slots per Pokémon (idle,
   ranged attack, body attack, strike, kick, take-off, dive, peck, hit, faint, the species' own moves…; slot meanings
   likely, counts verified), and each slot maps to one of the model's animations, with time points for the effects.
3. The game plays the attack half on the user and the damage half on the target.

**What the first version can do:**
- **The "own version" tag:** a lookup in the per-Pokémon table. Cheap, and exact.
- **The body animation:** the move's slot → the PKX slot record → the model's animation, played once on Studio's
  3D view, then back to idle. Studio already plays any animation. To add: reading the WZX header (the format is
  verified on all 1,098 files) and the PKX slot records.
- **The target's reaction** (a second model taking the hit, slot 8): possible, but it doubles the view's work.
  Later.

## 3. Particles: fade them in the first version

- **The format is fully documented**, all 975 banks parse (verified), and Studio already lists where an animation fires
  particles. But drawing them means porting the game's particle interpreter (a byte-code program per emitter), its
  random numbers, its textures and its drawing (`particles.md`). The docs call particles the hardest part of the
  renderer.
- **Recommendation:** fade the switch in version 1. Version 2 brings particles in steps: first the ambient ones (12
  species, the same machinery on a smaller scale), then the move effects.

## 4. Plain stage and Test in game: what the move-viewer work gives us

- **Plain stage: already possible.** Studio's `--game-camera` reproduces the developers' move viewer's camera
  exactly. The side-by-sides with the project's shots lined up pixel for pixel in place and size (verified, 2026-11-19).
  So "Plain stage" is that camera, fixed, on the viewer's flat grey backdrop (0x7F7F7F). I'd make it solid in
  version 1.
- **The game's lights** are logged too (4 infinite, 2 point, ambient 76), so Studio's lighting can copy them once it
  lights models at all. That's the same work that makes bump maps and shine show. It's a separate step, before or
  after the Moves tab.
- **Test in game: the faithful path is to let the game play it.** the project's OrrePort switches already boot straight into
  the move viewer with a chosen Pokémon (`ORRE_WAZA_VIEWER`, held frames, backdrop, aim, camera and light logs).
  - **What "Test in game" would do:** Studio starts OrrePort with those switches plus a move, and the real engine plays
    it with its own camera, lights, particles and timing. It's exact by construction, and nothing has to be
    re-implemented.
  - **To add (the project's side):** a switch to choose the move, and a battle-camera mode. 133 moves carry an authored
    camera; the others get the battle's random camera shot.
  - **The catch:** it opens the game in its own window rather than inside Studio, and needs the player's OrrePort
    set up. Fine for a "test" button.
  - **Recommendation:** fade it in version 1, and make it the first thing of version 2. It's cheaper than drawing
    particles in Studio, and it's the only fully faithful preview.

## 5. Solid now, faded for later

| Control | Version 1 | Why |
|---|---|---|
| Learnset (level-up, TM/HM, egg) with type, kind, power, accuracy, PP | solid | all data on the disc (section 1) |
| "Own version" tag | solid | one table lookup |
| Play (body animation) | solid | section 2 |
| Pause and step frame by frame | solid (moved up from "later") | Studio's view already holds frames; the speed choice stays later |
| Studio view / Plain stage | solid | section 4 |
| General move or own version line, link to the Move editor | solid (the link goes to the Move editor's "coming soon" page until it exists) | |
| Particles on / off | faded | section 3 |
| This move / Its four moves / All physical / All special | only "This move" in version 1 | see section 6 |
| Test in game | faded | section 4; first of version 2 |
| Frame rate, slow motion | faded | after the battle-speed work in OrrePort |
| Edit learnset, Tutor | faded (Tutor hidden until XD) | editing comes with mods |
| Make it your own (swap animation, particles, attach points, sounds, hit reaction) | faded | needs the WZX writer and the mod system |

## 6. What the mockup gets wrong or leaves open

- **"★ its current four moves":** a species has no current four. Only a single Pokémon (a party member, a trainer's
  Pokémon) does. Two options:
  - drop the ★ on the species page and show it later when Studio opens a trainer's or the player's Pokémon;
  - define it as "the four it would know at level N" (Gen 3's rule: its last four level-up moves), with a level field.

  I'd do the first. The "Its four moves" button follows the same choice.
- **"All physical / All special":** in Gen 3 the kind comes from the move's type, so these lists mix very different
  animations. As a playlist they'd mostly repeat the same two or three slots.
  - **Better:** the **Animations tab** shows each slot directly (all 17, played on the model).
  - **And:** the Moves tab plays "This move" only.
- **Shadow Rush** (move 356) appears in no learnset, as a Shadow Pokémon's move. Showing it belongs with the Shadow
  editor, not here.
- **Move kinds marked "—":** status moves have no kind in Gen 3. The column shows "status".
- **The two ends of a move:** the mockup shows only the user. A move in battle is a user half and a target half. A
  small "target" toggle could come with the target model later.

## 7. Forms (a note for later)

- **Unown's 28 letters, Castform's 4 weather looks:** each has its own model, but all share the same slot table
  (verified). So moves would animate the same on each, unless a per-Pokémon record names a form's model id (possible,
  since the records key on the model id; none checked yet).
- **Deoxys:** Colosseum has only the Normal Forme (verified); XD's Formes come later, with XD as a source game.
- **The plan:** pick the form in the Forms card, then the preview uses that form's model and its own records.
- *Checked 2026-11-19 (snatcherLeo asked how forms work on the Moves tab):* **forms do have their own move versions.** The
  per-Pokémon table names Castform's three weather models (0x19E-0x1A0) and its base model (385): the base form has
  its own Attract, the sunny form (0x19F) its own Sunny Day and Attract, the rainy form (0x19E) its own Rain Dance
  and Attract, the snowy form (0x1A0) its own Hail and Attract (verified from the table in `main.dol`). The learnset
  itself is the species' and the same for every form (one species record). So the Moves tab stays one screen: the
  learnset unchanged, a form picker beside the preview (in step with the Overview's Forms card), and the "own
  version" tags and the played effect following the picked form's model. Unown's letters: not checked yet (their
  models come from a separate table, not the model-id table the per-Pokémon records key on).

## 8. XD

XD's species and move values are the same as Colosseum's (verified by the project and 2026-11-19). The Moves tab's data works for both
unchanged. XD adds a 12-move tutor list and Shadow moves, shown once XD is added as a source game.

## 8b. snatcherLeo's idea for version 2: a stand-in target box (2026-11-19)

snatcherLeo: "are particles something you're going to add eventually for the move tab? ... especially for test in-game - we
can have a simple 3D box that we will use to demonstrate the attack hit of said attack. So, each move does a startup
animation with the perspective of the 3D Pokemon (if it'll hurt/inflict damage or a status condition on another
pokemon) and then the battle camera changes to the opposing Pokemon who will suffer said attack. For that opposing
Pokemon, so users can see their particles in action, it'll just be a box we can use, something simple that'll look
fine in both plain stage and in-game".

How it fits what the game does (labels as above):
- **Two halves:** a move's effect is a user half (part 1, or part 3 for moves aimed at the user) and a target half
  (part 2, `_damage`) in the general table (verified, `models-animation.md`). So "the user's startup, then the camera
  on the target" is the game's own split: the box plays part 2's particles where the target would.
- **Attach points:** effects fire at named points on a model (the PKX's attach records: mouth, hands, the body's
  middle; likely). A box needs those points defined on it (its middle, top, front...) so a move's hit lands
  somewhere sensible. Studio defines them; nothing from the disc is needed.
- **The camera:** 133 moves carry their own camera in the WZX; the rest use the battle's camera shots (verified count,
  `wzx.md`). Studio can follow the authored ones, and use one fixed "look at the target" shot for the others.
- **In Studio (Plain stage / Studio view):** the box is simple to draw. The real work is still the particles (porting
  the game's particle interpreter, section 3).
- **In Test in game:** the real engine needs a target model in the game's own format. A box would be a small model of
  OrrePort's own (a PKX with the attach points), loaded in place of the opposing Pokémon through the project's move switch
  (likely workable; to try). The game's particles and camera then play exactly.
- **Status moves** (Growl, Thunder Wave) also have a target half; moves aimed at the user (Growth, Protect) don't, so
  the camera stays on the user for those.

**The box's design (snatcherLeo, 2026-11-19, while release step 4 was being built):** "target box can be a random 3D box,
very simple, and will be fine in any background, will have a "hit animation", something very clear and simple. And an
idle pose. That's all we'd need." So, decided:
- a plain 3D box (Studio's own, nothing from the disc), in a neutral colour that reads on the Studio view and on the
  plain grey stage;
- two animations only: an **idle pose** (still, or a barely-there breathing) and a **hit animation** (a short, clear
  flinch: a quick push back and settle), played when the move's target half starts, as a Pokémon's slot 8 would be;
- attach points defined by Studio on the box (centre, top, front, the ground), so every point a target half asks for
  lands somewhere sensible (all 16 map onto those few);
- no faint, no other motions.

## 9. The order I'd build it in

1. **The core:** decode TM/HM, egg moves, the move table, the type kinds and the description bank (golden tests
   against the Python extractor).
2. **The left side:** the learnset table, sorted by level / machine / egg, with the move's details on hover or
   selection.
3. **The animations:** the WZX header and the PKX slot records in the core; Play / pause / step in the preview; the
   "own version" tag and line.
4. **Plain stage:** the move viewer's camera and backdrop.
5. **Version 2 (separate decisions):**
   - Test in game, with the project's move switch;
   - particles (ambient, then moves);
   - the target model;
   - lighting.

Each step is checked against the game's own data or the project's move-viewer shots, as the 3D view has been.
