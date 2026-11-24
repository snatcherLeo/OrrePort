# Attach points in Colosseum's battle models: how they're stored, resolved and used

*Copied into `studio/` 2026-11-19: the research behind `studio/attach-points.md` (the importer-facing summary). Private (Studio's planning). Scratchpad paths in it were the research session's working files and are not kept.*

Research notes, 2026-11-19. Read-only: no repo edits. Labels: **verified** (read in code or data), **likely**,
**guess**. Code references are to Studio's branch as of that date (paths under `src/`). Data counts come from
the extracted disc files: 494 PKX files (420 Pokémon, counting the separate shiny copies, and 74 trainers) and all
1,098 WZX files, read with `tools/local/colo/pkx.py`, `wzx.py` and `hsdmodel.py`. The survey scripts are in the
same scratchpad folder as this file (`attach_survey.py`, `attach_analyse.py`, `wzx_attach_survey.py`,
`ptcl_tracks.py`).

---

## 0. Summary

- **What the 16 values are.** The 16 s32 at slot record +0x4C are the model's **attach-point table** for that
  animation slot: index *n* names one joint of the model by its position in a depth-first walk of the joint tree.
  **verified**
- **How the game turns one into a place.** A move entry names a selector (0..15, or 16 for "the body centre").
  The code reads `table[current slot].points[selector]` and passes it to `GSmodelGetPart`. That function walks
  the model's render joint tree in `HSD_JObjWalkTree` order (pre-order: the node, then its children; the root is
  0) and stops at that count. The part it returns is then **followed every frame** (or, with a per-entry flag,
  read once). The "position type" (1..7) chooses which of position, rotation and scale the effect takes from the
  joint. **verified**
- **The 16 points, by name:**

| Index | Best name | Label |
|---|---|---|
| 0 | root / ground point (the model origin, always joint 0) | verified (data: 0 in all 494) |
| 1 | mouth | likely (verified usage) |
| 2 | body centre / hit point (the camera target and the default "where it lands") | likely (verified usage) |
| 3 | tail tip | likely |
| 4 / 5 | eye, +X side / −X side | likely |
| 6 / 7 | hand or front limb, +X / −X (7 is the striking or throwing hand) | likely |
| 8 / 9 | second pair of hands or limbs, +X / −X | guess (one move uses them: Machamp's Revenge) |
| 10 / 11 | third pair of limb points, +X / −X | guess (no move uses them) |
| 12 / 13 | foot, or front foot or toe, +X / −X | likely |
| 14 / 15 | rear foot or heel, +X / −X | likely |
| (16) | not stored: the model's bounding-box centre (a joint added at run time) | verified |

- **Sides.** Models face +Z with +Y up (verified from data: the mouth sits at +Z in almost all models). So +X is
  the model's own **left** (likely, for a right-handed frame), and the odd indices are its right side. Index 7, the
  right hand, is the one the punch moves use.

---

## 1. What the 16 values at +0x4C are, and how an index becomes a joint

### Storage (verified)
- In the PKX file, each slot record is 0xD0 bytes and its 16 s32 at +0x4C are the attach points. The loader
  `wazaSequenceOwnerLoadResource` (0x801DD5E8, `src/game/sequence.c:781-818`) copies each record into a 0xD4-byte
  runtime record. The 16 words keep their offset (+0x4C). The runtime record adds one leading animation track and
  a loop counter at +0x90. It sits in the owner's slot table at `owner+0x2C`; the current slot is at
  `owner+0x32`.
- The other readers address the same 16 words:
  - `wazaSequenceEntryGetAttachPart`: `table[selector][0x13]`, that is +0x4C + 4×selector.
  - the move viewer: `entry + 0x4C + i*4`.
  - the camera: `targetParts[3]` at +0x4C.
  - the screen-effect start: `WazaEffectTblEntry.field_4C[]`.
  - the stop and aura particles: +0x50 and +0x54.

### Resolution chain (verified)
1. **WZX node header → runtime entry.** `wazaSequenceLoadEntryParams` (0x801DC46C,
   `src/game/wazaSequence.c:548-557`) copies the fields:

| WZX file offset | Runtime entry field |
|---|---|
| +0x08 ("attachment") | +0x20, the **attach selector** |
| +0x0C ("position type") | +0x28 |
| +0x60 (flags) | +0x1C |
| +0x64 (part index) | +0x24 |

2. **`wazaSequenceEntryGetAttachPart`** (0x801D97F0, `src/game/wazaSequenceEntry.c:1332-1388`):
   - **flags bit 0 set**, with a link key: the entry attaches to the linked *model entry* (a model embedded in the
     move). It uses the **raw joint index at +0x64** in that model, not the table.
   - **otherwise:** `owner = entry→sequence→owner`, and the table is the owner's **current slot** record.
     - selector 16: if `GSmodelCenterNull(model)` succeeds, the index is `GSmodelWalkJObjs(model) − 1`, the centre
       null joint just added at the end of the walk. If it fails, the index is `table[slot].points[2]`.
     - any other selector: the index is `table[slot].points[selector]`. There is no range check; the data uses only
       0..16.
   - The function calls `GSmodelGetPart` only when the index is ≥ 0; otherwise the part is NULL. A NULL part sets
     viewer error bit 2 when `GSgetVersionMinor()==2`.
3. **`GSmodelGetPart(model, n)`** (0x800EE150, `src/game/gs_part_exact_800EE150.c:42-73`):
   - It adds 1 to `n` when the model has flag 0x20000 ("a null root was added at load"), so file indices stay
     valid.
   - n = 0 is the render root.
   - Any other n starts `HSD_JObjWalkTree(root, counter)` and takes the joint where the running count equals n.
     The count is unsigned, so −1 never matches and gives NULL. An index past the joint count also gives NULL.
   - The result is a `GSpart` {model, joint, index}.
4. **Walk order.** `HSD_JObjWalkTree` (`src/hsd/jobj.c:313-328`) visits the node, then each child subtree in turn
   (child, then the child's `next` siblings). It is pre-order depth-first, and the root's own siblings are not
   walked. It does not enter the children of a joint flagged `JOBJ_INSTANCE`. This is the same numbering as
   `GSpartGetJObjIndex` and the particle hook. **verified**
5. `GSmodelWalkJObjs` (`src/game/gs_model_anim_suffix_800ED4D4.c:486-495`) returns the joint count, minus 1 when a
   null root was added. **verified**

### −1 means "none", and 0 means "the root" (verified from data)
- **Slot 0 never contains −1** in any of the 420 Pokémon models. Points a species doesn't have are set to **0**,
  the root on the ground:

| Point | Models with 0 (the root) |
|---|---|
| tail (3) | 138 |
| hands (6 / 7) | 81 |
| 8–11 | about 305 |
| front feet (12 / 13) | 34 |
| rear feet (14 / 15) | about 120 |

- **−1 appears only in the extra slots 11–16:**
  - An empty slot (no animation, track type 1) has all 15 points at −1 and index 0 at 0. That holds for 417 of 420
    models in slot 11, 298 in slot 12, 377 in slot 13, 397 in slot 14, 408 in slot 15, and 234 of the 263
    17-slot models in slot 16.
  - A filled extra slot sets its unused points (8–11, 14, 15) to −1 instead of 0.
- So both 0 and −1 mean "no such point". 0 fails gracefully (the effect goes to the ground point); −1 fails (no
  part). Index 0 is always the value 0.

### Why the table is per slot (verified from data; the reason is likely)
- Slots 1–10 always carry a full table. In most models it equals slot 0, but some motions point a point at a
  **different joint**. Changes from one real joint to another, across all slot records:

| Points | Records |
|---|---|
| hands (6 / 7) | 794 / 884 |
| feet (12 / 13) | 722 / 421 |
| mouth (1) | 105 |
| centre (2) | 31 |
| eyes (4 / 5) | about 54 |

- Examples:
  - Machamp's punch motions (slots 2, 3, 5 and 7) use different hand joints from its other motions.
  - Pikachu's slot 4 uses a different foot joint.
  - Charizard's slot 4 points both front feet at one joint.
- So yes, a motion can change which joint "the hand" or "the mouth" is. Likely reason: each slot was exported from
  its own animation scene, and the animator chose the bone that leads that motion.
- Slot 16, the airborne idle in 17-slot models, has a short table where a species fills it. For example,
  Charizard's slot 16 has only the mouth, centre and eyes.

### The PKX header's three s16 at 0x18 / 0x1C / 0x20 are not attach points (verified; corrects models-animation.md)
- The loader copies them to `owner+0x1A`, `+0x1E` and `+0x1C` (`sequence.c:716-718`), and **only
  texture-animation calls** read them (`src/game/gs_range_801DE698.c`):

| Header field | Read by | Meaning |
|---|---|---|
| 0x18 | `wazaSequenceOwnerTexAnimOn` (sleep starts) | **eyes-closed** texture animation |
| 0x1C | `wazaSequenceOwnerTexAnimOff` (wake) | **eyes-open** texture animation |
| 0x20 | the random idle blink (`GSrandf`) | **blink** texture animation |

- Data: Pokémon models use runs of three consecutive texture animations, for example (6, 7, 5) in 115 models and
  (7, 8, 6) in 65. 26 Pokémon models have (−1, −1, −1). Trainers have (−1, −1, 0): a blink only.
- *Correction for models-animation.md:* "three model-part indices" should read "texture-animation indices: sleep
  (eyes close), wake (eyes open), blink".

### The PKX header's +0x0C is the size class (verified; corrects "kind (?)")
- The loader stores it at `owner+0x10` (`sequence.c:723`). The values run from −2 to 3.
- `wazaSequenceGetEffectScale` (0x801D9950, `wazaSequenceEntry.c:1405-1514`) turns it into the **effect scale**
  (values read from main.dol):

| Size class | Effect scale |
|---|---|
| −2 | 0.5 |
| −1 | 0.75 |
| 0 | 1.0 |
| 1 | 1.3333 |
| 2 | 2.0 |
| 3 | 3.25 |

- A few move ids add their own factor on top (Bide, Psych Up and others).
- The same class drives:
  - the viewer's dot size;
  - the sleep and freeze particle scale;
  - the move camera's distance presets (`_wazaSequenceCameraCalculateParams`, 0x801D4FB8, `switch (mode)` at
    `wazaSequenceCamera.c:1559`);
  - the authored camera's scale (`wazaSequenceOwnerGetSizeScale`).

---

## 2. What each index means (likely; from usage and from joint positions)

### Two kinds of evidence
- **Usage:** which moves name each selector, from the owner-attached particle and model entries in all 1,098 WZX
  files.
- **Geometry:** where each index's joint sits in slot 0, in bind pose, normalised to the model's joint cloud: height
  from 0 (bottom) to 1, forward along Z, and side as x divided by half the width. The medians are over 420
  Pokémon models.

### Joint type (verified)
- Indices 1, 2, 4 and 5 point at **bare leaf joints with no mesh in all 420 Pokémon models**. These are dedicated
  locator joints, hung under the animated bone.
- Indices 6, 7 and 12–15 are leaves in about 80–90% of models.
- No attach point is a `JOBJ_PTCL` joint.

### The points

| Idx | Median height / forward / side | Moves that use it (examples) | Name |
|---|---|---|---|
| 0 | 0 / 0 / 0 (always joint 0) | Rain Dance, Dig, Dive, ball send-out / return, stat up / down rings, evolution: 61 particle entries, 175 model entries | **root / ground** |
| 1 | 0.66 / **+0.47** (front-most) / 0 | Bubble, Fire Blast, Blizzard, Poison Gas, Yawn, Lovely Kiss, Aeroblast, Charge Beam / Zap Cannon (`denjihou`), Shock Wave (`dengekiha`): 310 particle entries; the sleep "Z" loop (code); the move camera for 36 "breath / roar" files (Hyper Beam, Roar, Glare, Mean Look ...) | **mouth** |
| 2 | 0.53 / +0.34 / 0 | 633 particle entries, the most used: Thunderbolt, nearly every `_damage` half (hits land here); the freeze loop's fallback (code); the move camera's default target (1,053 of 1,091 files); the leaf effect's default; the distortion effect's centre (code: slot 0's index 2) | **body centre / hit point**: chest on bipeds, head on serpents (Gyarados, Steelix, Onix) |
| 3 | 0.48 / **−0.50** (rear-most) / 0 | Poison Tail; Wurmple's Poison Sting (`dokubari_kemusso`); Smeargle's Sketch (`sketch_doble`, its tail brush); Sand-Attack (`sunakake_sp3`), Flash (`flash_sp1`) | **tail tip** (0 when the species has no tail: 138 models) |
| 4 / 5 | **0.79** / +0.35 / **±0.24** | Fake Tears, Lock-On, Mind Reader, Detect, Foresight, Disable, Odor Sleuth, Snatch, Sky Attack's glint, Tickle | **eyes** (+X / −X) |
| 6 / 7 | 0.49 / +0.22 / **±0.69** | 7: Comet / Mach / Mega / Fire / Thunder / Ice / Focus / Shadow Punch, Dragon Claw, Slash, Cut, Crush Claw, Brick Break, Cross Chop, Needle Arm, the trainer's ball throw (`throw_*`); 6: the Snag Ball throw (trainer), Rage, Outrage, Dragon Rage, Revenge | **hands / front limbs** (+X / −X; on fliers the wing tips, e.g. Articuno) |
| 8 / 9 | 0.09 / +0.16 / ±0.37 (106 models use it) | only Machamp's Revenge (`revenge_kairiky`) | **second pair of hands or limbs** (guess) |
| 10 / 11 | 0.09–0.15 / +0.10 / ±0.30 (about 104 models) | no move | **third pair of limb points** (guess) |
| 12 / 13 | **0.02** (ground) / +0.05 / ±0.50 | Blaze Kick (12), Sand-Attack (13); Rage, Outrage, Dragon Rage, Revenge stomps; the Shadow and Hyper Mode aura | **feet** (biped toes, quadruped front feet) |
| 14 / 15 | 0.01 / **−0.13** / ±0.33 (about 297 models) | 15: Grudge (`onnen_sp1`) only | **rear feet / heels** |
| 16 | not stored | Transform, Substitute, Minimize, Growth, Castform's change, purification (`relive_*`), Snatch, Conversion 2 | **body centre (bounding box)** |

### Spot checks (verified from data)

| Model | Root | Mouth | Centre | Tail tip | Eyes | Hands | Feet |
|---|---|---|---|---|---|---|---|
| Charizard (63 joints) | j0 | j32 at the snout tip (y 11.6, z 10) | j17 on the chest (y 6.8) | j55 (z −16.8) | j31 / j33 (y 13.4, x ±0.7) | j44 / j50 (x ±5.5) | j7 / j14 toes, j8 / j15 heels |
| Pikachu | j0 | j25 | j31 | j48 (tail tip, high and behind) | j24 / j26 | j37 / j43 | j7 / j16; rear points = root |

---

## 3. How a WZX entry uses an attach point

### Fields (verified)
- **Attach selector** (file +0x08 → entry +0x20): 0..15 index the table, and 16 means the bounding-box centre.
- **Position type** (file +0x0C → entry +0x28): which parts of the joint's transform the effect takes. The
  `switch` in `modelUpdateAttachments` (`gs_model_anim_suffix_800ED4D4.c:174-201`) and in `gsParticleUpdate`
  (`field_range_80117E58.c:2569-2596`) maps them:

| Value | The effect takes |
|---|---|
| 0 | nothing (not attached) |
| 1 | position |
| 2 | rotation |
| 3 | scale |
| 4 | position and rotation |
| 5 | rotation and scale |
| 6 | position and scale |
| 7 | all three |

- **Flags** (file +0x60):

| Bit | Meaning |
|---|---|
| 0 | attach to a linked model entry's joint (file +0x64, a raw joint index in the move's own model) instead of the owner's table |
| 1 | **read once**: the effect takes the joint's transform at start and then stays put. Otherwise it follows **every frame**. |
| 2 | world placement: not attached; set on the battle grid, with bit 3 for the grid's normalised scale |

### Particles (`_wazaSequenceParticleEntryStart`, 0x801D8B38, `wazaSequenceEntry.c:794-1019`; verified)
1. Create the emitter from the entry's bank.
2. If flag 4 is set: world placement, done.
3. Otherwise get the part with `GetAttachPart`.
4. If the part exists and the position type is not 0, call `gsParticleLinkToJObj(node, part, type, followOnce, 1,
   linkChildGenerators)` (`field_range_80117E58.c:2729-2763`). This stores the model and the part index in the
   particle node (+0x48 / +0x4C, reset to identity).
5. Then set an offset by type, from the emitter's authored position scaled by the owner's size class (the exact
   arithmetic as decompiled multiplies by (scale − 1); likely), and for some types the model's rotation and scale.
6. Each frame, `gsParticleUpdate` re-reads the joint through `GSmodelGetPart` + `GSpartGetTransform`. It adds the
   stored offset (rotated by the joint) and applies the parts chosen by the position type. It stops after the
   first frame when "read once" is set.
7. A position type of 0 with a part found places the emitter **once** at the owner's root plus the offset, with no
   following.

### Models (`_wazaSequenceModelEntryStart`, 0x801D91EC, `wazaSequenceEntry.c:1031-1259`; verified)
- **The usual case:** `GSmodelAttachToGSpart(model, part, type, followOnce, 1)`
  (`gs_model_main_suffix_candidate_800E4AC0.c:14-55`). It records the target model and part index (GSmodel +0x118 /
  +0x11C) and the type, then sets model flag 0x80000 (follow), and 0x40000 as well for "read once". From then on
  the model's own position, rotation and scale act as a **local offset** from the joint. Each frame
  `modelUpdateAttachments` (`gs_model_anim_suffix_800ED4D4.c:146-212`) rebuilds the transform. Attached models are
  also animated at fractional frames (`modelProcessAt60fps`).
- **The reverse case:** when the entry's +0x94 is set, the **Pokémon is attached to the effect model's joint**
  (+0x98 names that joint). An option (+0x9C bit 0x10) first adds a null at the owner's bounding-box centre so it
  turns about its middle. Likely used for throws and carries (Seismic Toss, Fly and the like; which moves is a
  guess).

### Screen effects (`_wazaSequenceEffectEntryStart`, 0x801D87B0, `wazaSequenceEntry.c:573-780`; verified)
- The effect kind's parameters carry selectors into the same table (current slot):

| Kind | Selector source | Use |
|---|---|---|
| 1 (the loader calls it `boltEffectCreate`; `wzx.md` calls it "electron") | file parameter +0x48 | one point |
| 3 (lightning) | a selector at parsed +0x16 | one point |
| 4 (trail, tracefx) | two selectors | a trail from one point to another |
| 5 (leaf) | one selector (file parameter +0x30) | one point |
| 12 (distortion) | none | always reads **slot 0's index 2**, whatever the current slot |

- **On the disc:**
  - kind 5: 12 entries at index 2 and 2 at index 1 (Venusaur's Petal Dance);
  - kind 1: 2 entries in Thunder's damage half, at indices 0 and 2;
  - kind 12: 39 entries;
  - kinds 3 and 4: not used on the disc.

### Camera (verified)
- The WZX sequence header's +0x0C (format version 5) goes to `sequence+0x17`, default 2 (`wazaSequence.c:616-631`).
- `battleCameraStartWaza` (`wazaSequenceCamera.c:547-555`) and `battleCameraUpdateWaza` (0x801D2D28,
  `wazaSequenceCamera.c:257-278`) aim the camera at `points[that]`, which only reaches **0, 1 or 2** (a 3-entry
  view). Each frame the target follows that joint.
- On the disc: 1,053 files use 2, 36 use 1 (mouth) and 2 use 0 (Dig's and Dive's underground halves).
- The camera's distance and height presets come from the size class and the bounding box. Its scale comes from the
  size scale.

### User versus target (verified for the lookup; the split of halves comes from models-animation.md)
- Every lookup uses `entry → sequence → owner`, so an entry only ever reads **its own owner's** points.
- The attack half plays on the user's record and the damage half on each target's. So "the hit lands on the target's
  centre" is a damage-half entry at index 2 of the target.
- There is no cross-model reference to the other Pokémon's points. Effects that travel between the two are the
  move's own animated models or particles placed on the grid. Likely, not traced.

### Status loops (verified)
- **Freeze** (`koori_loop`, global resource 0x11EF2400): control value 7 runs `wazaSequenceOwnerSetStop`. It stops
  the animation and puts the loop on the **centre null, or index 2** (`wazaSequenceSys.c:153-215`).
- **Sleep** (`nemuri_loop`, 0x11EE2400): control value 0 runs `SetAura`. It plays the eyes-closed texture animation
  and puts the loop on **index 1**, the mouth (`wazaSequenceSys.c:69-123`). Here an index of 0 counts as "none".

### Usage counts (verified)

| | Particle entries attached to the owner | Model entries attached to the owner |
|---|---|---|
| Total | 1,167 | 427 |
| Position type 1 (position only) | 1,118 | 325 |
| Position type 6 (position + scale) | 46 | 45 |
| Position type 4 (position + rotation) | — | 39 |
| Read once | 347 | 218 |

- In addition, 423 particle entries and 77 model entries attach to a move model's own joint, and 45 entries are
  world-placed.

---

## 4. Missing points (−1, past the joint count) and the viewer's dots

### What a missing point does (verified)
- `GetAttachPart` returns NULL:
  - **particle:** not linked and not positioned. The emitter stays at the position it was created with (likely the
    bank's coordinates in world space, near the field origin), so the effect plays in the wrong place. It does not
    crash.
  - **model:** not attached. It is shown at its own transform (likely the origin).
  - **camera:** the per-frame retarget is skipped (`if (part != NULL)`).
- **There is no fallback** except selector 16 → index 2. Empty slots and their −1 tables are never consulted by
  vanilla moves: a move's slot is chosen by its WZX file, and the extra slots 12–15 are used only by species-specific
  variant moves (`_pikachu`, `_sp1` ...) for species that fill them.
- **Stale indices in the data (verified):** 17 models carry an index past their joint count, so the game resolves it
  to NULL. Examples:
  - Ekans and Gyarados: 6–11 in some slots;
  - Raichu, Zapdos, Porygon2, Seedot: 8–11;
  - Koffing: 13 and 15;
  - Beldum's model: most points, including the mouth.

  These look like tables left over from older rigs. Likely harmless where no move uses those points. Beldum's
  missing mouth is worth a check in the viewer.

### The viewer's dots (`wazaViewerDrawAttachPoints`, 0x801D7B94, `wazaViewer.c:1945-2104`; verified)
- When the debug display bit 0x80 is on (`ORRE_WAZA_VIEWER_DOTS=1` on the PC), it draws one point per index of the
  **current slot's** table at the joint's world position (`GSpartGetTransform`), or only the selected index
  (viewer work +0x18 ≥ 0).
- The dot's size scales with the size class and the camera distance.
- The colour passed is (255, 0, 0, 220), **red**. The function's comment says green, which is wrong;
  `docs/port/setup.md` already says red.
- **A missing point is drawn at (0, 0, 0)** and sets error bit 2. `wazaViewerDrawErrors` (`wazaViewer.c:1553`)
  prints "Invalid Null Joint data!" for that bit (the string at 0x80279488).
- The other attach error, bit 1 (an unknown position type), and bit 0x10 ("Attachment error!") are separate
  lines. Which string bit 1 prints is unsure: as decompiled, bits 1 and 2 share one message offset.
- **Quirk (verified):** the viewer calls `GSmodelGetPart` directly with no `≥ 0` check. On a model whose root null
  was added (flag 0x20000), −1 + 1 = 0 returns the root, so that dot sits on the root instead of raising the error.
  The game's own lookup is guarded.

---

## 5. Data survey (numbers)

### The models
- **Files:** 494 PKX (420 Pokémon including the shiny copies, 74 trainers). Pokémon: 263 have 17 slots and 157 have
  16. Trainers all have 17. The doc's disc-wide count is 527; some PKX share folders in `extracted/`.
- **Size class** (Pokémon), with the median joint-cloud height in units:

| Class | Models | Median height |
|---|---|---|
| −2 | 25 | 3.5 |
| −1 | 253 | 7.3 |
| 0 | 96 | 15.3 |
| 1 | 31 | 18.9 |
| 2 | 14 | 28 |
| 3 | 1 (Wailord) | 98 |

- All trainers are class 0.
- **Joints:** Pokémon models have 15–179 joints (median 67.5). The largest stored attach value is 170.
- **Slot 0 coverage:** 0 models with −1. The "absent = root" counts are listed in section 1. Indices 1, 2, 4 and 5
  are real joints in all 420 models.
- **Do slots differ from slot 0?**
  - Every model has at least one differing slot, because its empty extra slots are −1.
  - In slots 1–10, models with a differing table:

| Slot | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Models | 33 | 119 | 210 | 374 | 135 | 90 | 160 | 91 | 89 | 88 |

  - Most of those changes are hands (6 / 7) and feet (12 / 13).
- **Trainers:** usually only points 1 (head / mouth, height 0.94) and 2 (chest, 0.72), with 3–15 at −1 in 51 of
  74. Points 6 and 7 are added in slots 6 and 7 (the throw motions).

### Ambient particles (verified for all 12 species with a GPT1 bank)
- Their emitters are particle keys (FObj type 0x28) on **joints of their own**, for example:
  - Charizard's tail flame on j54, next to the tail-tip point j55;
  - Articuno on j46 and j53;
  - Torkoal on five joints, in animations 1, 3 and 5 only.
- No species uses `JOBJ_PTCL` joints. Only Koffing's gas vents coincide with attach points (7, 8 and 9).
- So ambient emitters and attach points are **independent systems** that both address joints by walk index.

### The moves
- WZX selector use and the camera targets are given in sections 2 and 3.

---

## 6. What an imported model needs to work with Colosseum's moves

### Must have
- **A joint tree whose depth-first order is fixed after export.** Indices are walk positions and are not stored by
  name. Any change to the hierarchy (adding or removing a joint above a point) renumbers everything after it.
  Studio must keep its own name → index map and recompute the 16 indices whenever it writes the model.
- **Joint 0 = the root, on the ground under the body.** Index 0 is always 0.
- **Locator joints** (bare leaves with no mesh, like the originals), each parented to the bone that animates it:

| Point | Parent bone | Where |
|---|---|---|
| 1 mouth | head or jaw | at the mouth opening |
| 2 centre / hit point | chest or spine | at the visual middle, where hits should land and the camera aims; for a long serpent, the head |
| 4 / 5 eyes | head | at the eyes |
| 6 / 7 hands | hand or wing-tip bones | the striking ends |
| 12 / 13 feet | foot or toe bones | on the ground |
| 14 / 15 rear feet or heels | rear foot or heel bones | on the ground |
| 3 tail tip | the last tail bone | the tail's end |

  Because effects follow the joint every frame, a locator on the right bone moves with every animation.
- **Absent features:** point at **0** (the root), not −1, so effects degrade to the ground instead of vanishing. The
  vanilla convention is 0 in slots 0–10 and −1 for unused extra points.
- **Indices 8–11:** 0 unless the species has extra limbs. Only Machamp's Revenge uses them.
- **Slots:** fill the same table into every used slot. Give a slot a different joint only when one of its motions
  should lead with another bone (a punch with the other hand). Empty slots: all −1 except index 0. A 16-slot model
  is valid (it has no airborne slot 16).
- **Minimum for "every move looks right":** 0, 1 and 2 (camera and most effects), 4 / 5 (eye moves), 7 (all punch,
  claw and throw moves), 6, and 12 / 13. Then 3 for the tail moves and 14 / 15.

### Scale and size (verified mechanics; the thresholds are likely, from the survey)
- **Units:** Colosseum units, about 1 unit ≈ 0.1 m of the species' in-game size (likely). Pikachu stands about 3.8
  units tall, Charizard about 17, Wailord about 98. Models face +Z with +Y up.
- **Size class** (PKX header +0x0C): choose it from the height using the survey's medians (−2: about 3.5, −1: about
  7, 0: about 15, 1: about 19, 2: about 28+, 3: Wailord-size).
  - It scales every move particle and model 0.5–3.25×.
  - It sets the move camera's distance presets, the sleep and freeze loops' scale, and the authored camera's scale.
  - A wrong class means effects that swamp or vanish on the model, and camera framing that is too close or too far.
- **Bounding box (GSbound):** built by the game from the vertices.
  - Selector 16 (the centre null) and the reverse attachment's centre use its middle.
  - The move camera clamps the height to a 6–20 window and uses the size for distance.
  - The shadow and the culling use it too.
  - So stray geometry far from the body (a huge hidden mesh, a far-off locator mesh) moves the centre and the camera
    framing. Locators themselves have no mesh, so they don't affect the box.
- **Texture-animation indices** (header 0x18 / 0x1C / 0x20): provide eyes-closed, eyes-open and blink texture
  animations, or use −1. Without them a sleeping Pokémon keeps its eyes open, and nothing breaks.

### What breaks if a point is wrong (likely)

| Problem | Result |
|---|---|
| Missing point (−1, or past the joint count) | the effect plays at the world origin (particles) or doesn't follow (models); the viewer shows "Invalid Null Joint data!" with the dot at 0,0,0 |
| Wrong joint | the effect comes out of the wrong place (fire from the tail, punches from the foot) |
| Joint on a bone that doesn't animate | the effect stays still while the body moves |
| Point inside the mesh | the effect is hidden |
| Wrong scale or size class | effects and camera are the wrong size |

### For Studio (likely)
- An "attach points" panel per slot: the 16 dots (as the developers' viewer does), named per the table above, with a
  "n of 16" check. Flag −1 in a used slot and any index past the joint count. Offer to copy slot 0's table to the
  other slots.
- Studio stores names and the game stores indices, so the PKX writer converts on export.

---

## Open questions
1. **Names of 8–11** (guess): their positions are mixed (near the front paws, or a second arm pair), and only
   Machamp's Revenge uses 8 / 9. Check a few models with snatcherLeo in the viewer, with dots on and one index selected.
2. **Side convention:** is +X the model's left? Likely, from right-handed axes and from 7 being the punching hand.
   Confirm on one model by eye.
3. **Where an unattached particle appears** (the emitter's created position when the part is NULL): likely the
   world origin. Check in the viewer with a deliberately broken index.
4. **Which string viewer error bit 1 prints:** as decompiled, bits 1 and 2 share "Invalid Null Joint data!". Bit
   0x10's "Attachment error!" setter was not found in the decompiled code.
5. **The particle offset formula** (offset × (scale − 1)) and which moves use the reverse attachment (+0x94).
6. **The 17 models with stale indices:** whether any vanilla move hits them (Beldum's mouth, for example). A sweep
   of each species' learnable moves would settle it.
7. **Why the distortion effect reads slot 0's index 2** instead of the current slot's: an original slip or
   intended. Harmless while slot tables agree on index 2 (31 records differ).
