# tests/pc_hsd: HSD on the PC

A standalone test toward Milestone 1: load a model archive, relocate it with the disc marks
(`include/pc/disc_types.h`), build HSD's runtime objects with the matched `src/hsd` code and draw a frame through
Aurora. Separate from the GameCube build and from `orreport`; it reuses `port/` (B's platform layer and Aurora).

## Status (2026-10-15)

- Step 1, **load: works (verified).** `HSD_InitComponent` runs through the game's own code (heaps, GX, VI, object
  pools), and `HSD_JObjLoadJoint` builds Pikachu's model: 50 joints, 22 meshes, 22 materials, 15 textures, 27 shapes,
  the same counts an independent reader of the raw archive bytes gives.
- Step 2, **draw: Pikachu renders correctly (verified by screenshot, 2026-10-15).** Textures (eyes, cheeks,
  mouth, ear tips), lighting and shading all come out right, through the matched HSD code and Aurora:
  `pc_hsd_test.exe <Pikachu.dat> --frames 100000 --dist 12 --height 1.5`. Screenshots are taken from the window
  (PowerShell, DPI-aware) and stay in `build-pc/` (a render of disc data is not committed).
- Step 3, **a map: Outskirt Stand renders (verified by screenshot, 2026-10-15).** `S1_out.dat` (the room archive
  under `extracted/maps/S1_Outskirt_Stand/S1_out/map/model/`): 5 model sets, 192 joints, 146 meshes, 104 textures,
  147 display lists per frame, drawn with the archive's own camera and 5 lights (`--scene`): tiled ground, ramp,
  crates, pipes, baked shadows, all textured. Whether the view matches the game exactly is a visual check for snatcherLeo
  (Dolphin side by side). `--root N` draws one model set only.

Diagnostics (`gx_trace.c`): `--trace N` prints the material set-up for the first N meshes of the first frame;
`--white`, `--c0only`, `--rasonly`, `--notex`, `--nolight`, `--nosun`, `--nofog`, `--cullswap`, `--ambient N` are the
experiments that isolated the bugs below.

- Step 4, **animation: works (verified by screenshot, 2026-10-15).** `--anim N` attaches each model set's joint
  animation N and its material animation N (model set +4 / +8) and advances them every frame (`--animlen F` restarts
  every F frames). Pikachu has 15 of each; animation 3 visibly closes the eyes (a texture swap from the material
  animation) and moves the arms, and `PC_HSD_ANIMDBG=1` prints joint rotations / positions changing over the frames.
  So HSD's keyframe streams (fobj.c) and texture animation run correctly on the PC.

- Step 5, **every model and map: works (verified, 2026-10-15).** A sweep ran every Pokemon model (385 archives,
  with animation 0) and every map room archive (167, with its own camera, lights and animation 0) through the test for
  8 frames each: all load, animate and draw, with no crash, no assert and no missing function. (Scripts in the
  session's scratch folder; the logs stay in `build-pc/`.) Not checked: how each looks, only that each runs.

- **Second program, `pc_gfx_test`: the game's own graphics layer (verified, 2026-10-15).** It starts like the game's
  `main` (GSmem, OS heap, scratch slots), runs `GSgfxInit` with the game's arguments, then the game's frame (begin,
  playback, end, pacing). `--draw` records a coloured triangle through GSgfx's immediate mode (rendered correctly);
  `--model <archive>` loads a Pokemon with the game's `GSmodelLoad` (then `GSmodelSetVisibility`), lights it with two
  `GSlightCreate` lights (ambient + directional) and draws it with the game's `GSmodelDrawAll` through a camera from
  the game's pool, handed to HSD by `_cameraLoadCameraMatrix`: the whole model path is the game's code (verified by
  screenshot, the same picture as the HSD-only test). Built from HSD, the 53 graphics units, GSmem / scratch / math / GStexture, GSmodel / GSpart / GSmaterial,
  and the data files they read (`gfx_units.txt`, `gfx_support_units.txt`, `gfx_model_units.txt`,
  `gfx_data_units.txt`); particles and threads are stubbed, the debug log prints. Found with it: the cull-mode table and GSmtx's identity matrix were still raw
  big-endian data (both fixed / typed), and the project's math units called SDK vector functions without prototypes.
  Generated inputs: `pc_gfx_data.c` / `pc_gfx_stubs.c` from the link's undefined references (scratch script).
  `--room <map archive>` sets a room up the way `floorInitMap` does (minus the resource registry): every model set
  through `GSmodelLoad` (all made visible here), every light through `GSlightLoad` (active, animation 0 started), the
  room's camera through `fn_800D27FC`. Verified by screenshot on Outskirt Stand: sky dome with clouds, desert, mesas
  and pipes render through the game's code. The camera view is the description's resting pose (its animation is not
  started here), so the framing differs from the HSD-only test. A sweep of all 384 Pokemon models through
  `--model --anim 0` ran clean (load, animate, 8 frames). The same through `--room` for all 165 room archives: every one loads (with its
  camera), animates and runs 8 frames. `--texanim N` plays a model's texture animation through
  `GSmodelSetTexAnimIndex` / `StartTexAnimation` (Pikachu's face swaps, verified by screenshot).
  `--rtt` (with `--model`) draws the model into a 128x128 texture the way the field TV does it (`tvRender`) and shows
  the texture on a quad; the copy is read back through Aurora's readback (`PC_GFX_RTT_DUMP=<file>` writes its bytes).
  Options for the name-entry menu model's path (2026-10-17; all verified to still show the model):
  `PC_GFX_RTT_FMT=45` (RGBA8 copy, as the menu model; default 44 = RGB565), `PC_GFX_RTT_MODE=2` (HSD pixel format
  RGBA6_Z24 for the offscreen pass, as `menuModelRender`), `PC_GFX_RTT_BLEND=1` (the quad blended by the texture's
  alpha), `PC_GFX_RTT_RECT=1` (the quad as primitive 7 from two corners), `PC_GFX_RTT_RECT=2` (exactly
  `menuNameEntryBackDrawHumanModel`'s calls after `winSpriteDraw`'s `fn_800FE6D0` + `spriteSetEnv`; the sprite-env unit
  `gs_thread_hi_range_800FE35C.c` is in the unit list for this). `PC_GFX_PEDESC="12 hex bytes"` (with `--model`) applies
  an `HSD_PEDesc` to every material (the menu model's is `33 00 00 FF 01 04 05 0F 01 04 01 00`).
  Verified: 3,553 of 16,384 pixels drawn, and the dump decodes (RGB565, 4x4 tiles) to an upright, correctly coloured
  Pikachu.
  `--shadow` (with `--room` and `--model`) makes the model cast a shadow on the room's models the way the game sets it
  up (shadow flags 2, the room's first directional light, the receivers as 32-bit model words). Found and fixed through
  it: the receiver list read as native pointers, disc vertex positions read as native floats in the bound walk, and
  the bound walk's argument block one word too small on the PC. Verified: Pikachu's silhouette in the shadow texture
  (`PC_GFX_SHADOW_DUMP=<file>`), and the shadow changes 210 screen pixels near Pikachu.
  Without a display (window captures come back empty when the display is off) a run can still be checked:
  `PC_GFX_SCREEN_DUMP=<file>` writes frame 20 (or `PC_GFX_SCREEN_FRAME`) as GameCube RGB565 tiles, read back through
  Aurora. Used to confirm B's channel-colour fix: with the test's workaround gone the frame is byte-identical.
  `PC_GFX_BOUND=1` with `--model` prints the model's bounding box as `GSmodelRecalculateBound` builds it. Sweep over all
  385 Pokemon model files (2026-10-15): every box finite and non-empty, sizes from Caterpie (at most 2.3 units) to
  Wailord (139), no crash (verified).
  Two more sweeps over all 385 model files (2026-10-15, verified): `--room` Outskirt Stand + `--model` + `--shadow`
  renders every model's shadow slot without a crash, and `--anim-all` (the next animation every 4 frames, with the
  texture animation of the same index) runs every animation of every model (4 to 16 per model) without a crash.
  `--blend A B F` blends animations A and B by F the way the field walkers do (`GSmodelEnableAnimBlend`,
  `GSmodelSetAnimBlend`, `GSmodelSetBlendFactor`). Found with it: the blend code's view of a joint object padded the
  object header as 8 bytes, so on the PC it read and wrote the wrong fields and blended models drew nothing (fixed: the
  view starts with HSD_Obj on the PC). Verified on Pikachu: factor 0 is animation A's pose, 1 is B's, 0.5 in between.
  `--attach JOINT PATH` attaches a second model to a joint of the `--model` one the way move sequences and NPCs do
  (`GSmodelGetPart`, `GSmodelAttachToGSpart` selector 7, `GSpartFree`). Togepi on Pikachu's joints 0 / 10 / 20 lands
  at the root, below the body and on the head (tilted with it); on joint 20 during animation 3 it moves with Pikachu's
  sway across frames 10 / 20 / 30 (screen x 400 / 356 / 304 against the body's 336 / 312 / 284), so it follows the
  animated joint (verified on the PC; exact placement against Dolphin not checked).
  `--particles <bank.gpt1>` (with `--model`) loads a Pokemon's GPT1 particle bank and links it the way sequenceLoad
  does; `gsParticleInit` runs at start-up. Charizard's tail flame renders (verified, `PC_GFX_EYE="40 20 -60"` to see
  it past the body); about 180 live particles at steady state. `PC_GFX_PS_DEBUG=1` prints a live particle.
  `--emit-all` (with `--particles`) creates one generator for every command list of the bank, as a move sequence
  might. Sweep over all 865 GPT1 banks embedded in the move-effect (WZX) files (2026-10-15, verified): no crash,
  37,243 generators, 854 banks with live particles after 40 frames. Two PC bugs found on the way (generator.c's
  app-SRT view, the full-bank kill-the-oldest path). Synthetic: the moves' own timed sequences are not run yet.
  Visual: Thunderbolt's attack bank around Pikachu draws its sparks and lightning textures with additive blending
  (frame dump, all 30 emitters at once). The global status loops `koori_loop` / `nemuri_loop` (frozen / asleep,
  resource type 18) are GPT1 banks too and run.
  `--seq <file.pkx>` loads a Pokemon the way battle move sequences do (wazaViewer.c's order): the sequence pool for 8
  (`fn_801DAEF8`), a free sequence (`fn_801DB154`), the whole PKX in a GSmem block, then `sequenceLoad`. Needs the
  sequence units (wazaSequence*, sequence*, gs_res.c, gs_floor.c, the gs_range sequence helpers) in
  `gfx_model_units.txt`. Found on the way: the sequence pool header (`lbl_80467CC0`) and the shadow-receiver list
  (`lbl_80467C80`) used 8-byte pointers on the PC while every reader uses the GameCube offsets (fixed: 32-bit slots).
  Status 2026-10-15 (verified): Charizard.pkx loads through `sequenceLoad` (model, particle bank, the sequence's slot
  table) and draws (frame dump with `PC_GFX_EYE="0 4 45"`: battle scale is about 4x the plain `--model` view). Fixed on
  the way, all GameCube-identical: gs_floor.c's archive registration (HSD_Archive allocated at 0x60, 0x68 bytes on the
  PC; its messages and the "scene_data" name read past lbl_802717F0), the sequence record's model slot (+0x24, 32-bit)
  read as an 8-byte pointer, and the field-particle node accessors (fn_80118874-fn_80118FB0) using GameCube offsets on
  the PC's native node / bank structs.
  `--wzx <move.wzx>` (with `--seq`) attaches a move's effect sequence to that Pokemon and starts it the way
  wazaViewer.c does (`fn_801DDB4C`, then `fn_801DA9E8`; the test keys it as type 1, an attack, likely) and runs
  `fn_801DB088` every frame. Verified 2026-10-16: Pikachu's own Thunderbolt (`10manvolt_pikachu.wzx`) runs 150
  frames, Pikachu glows and the lightning backdrop and sparks draw (frame dumps 30 / 60, `PC_GFX_EYE="0 2 14"`;
  snatcherLeo: "visually, looks perfect"). The effect system is set up as main.c does (`fn_80130CE0(0x20)`) and its two
  GSgapp tasks run by hand each frame, so the move's colour-fade entry (WZX effect type 0) starts too (a bright flash
  over Pikachu at frame 30; its strength still to compare with Dolphin). The test's floor is an empty stand-in (no
  battle room: the room-tint step finds no models). The one entry still failing is the move's sound cue (audio is
  stubbed in the test). Needs the effect units (effect_visual*, tracefx*, gs_effect.c).
  `--seq2 <pkx> [--wzx2 <damage.wzx>] [--seq-dist F]`: a second Pokemon, the move's target. Both stand where the
  battle grid puts a single battle's Pokemon: x = +40 F turned -90 degrees and x = -40 F turned +90 degrees, F the
  size factor (default 1.0). Its WZX is keyed type 2 (damage, likely:
  the battle's fn_801DDD28 keys a move as (move, type)) and starts in the same frame as the attacker's. Verified
  2026-10-16: Pikachu's Thunderbolt (`10manvolt_attack.wzx`) on Charizard (`10manvolt_damage.wzx`): the beam reaches
  Charizard at frame 20, Charizard plays its hit animation by frame 60 (frame dumps, `PC_GFX_EYE="70 12 0"`); that run used a 10-unit spacing along z).
  With `--room <S1_out_bf.dat>` (Outskirt Stand's battle stage) both stand on the paved battle square (frame dump,
  `PC_GFX_EYE="0 45 170" PC_GFX_FAR=3000`: our own camera, since the room's is overhead; `PC_GFX_FAR` sets the far
  plane). Each half plays around its own Pokemon: move entries attach only to their owner's model parts
  (`fn_801D97F0`), so nothing crosses the arena by itself (likely by design; the battle camera ties the halves together).
  The room stands in for the battle's floor: its archive and models are registered in the resource registry under the
  game's floor ids (stand-ins for floorDataBiosGetCurrentPtr / GetGroupID / std093_getCurrentGroupID), so the move's
  room tint finds the real room models: during Thunderbolt the arena darkens (frame dumps 10 / 30; verified it runs,
  the amount still to compare with the original). Found on the way: the tint's colour step (`fn_8013B268`) walked
  the 4-byte model list as 8-byte pointers (fixed).
  `--battle-grid [--trainer0 PKX --trainer1 PKX]`: both Pokemon placed by the game's battle grid (battle_grid.c:
  battleGridAddTrainer / AddPokemon / battleGridUpdate; the grid and battle-camera units are in the test). Trainer
  battle models are PKX files like the Pokemon's (extracted/trainers); without them the trainer slots are empty
  records. Verified 2026-10-16 (frame dump, Outskirt Stand's battle stage): trainers at x = +/-65, Pokemon at +/-40,
  facing each other; distances 65 / 40. Found on the way: the grid's Pokemon-slot walk (4 copies) and the floor
  record's big-endian room id (4 places), fixed.
  `PC_GFX_IDLE_CAM=1` (with `--battle-grid --battle-cam`): the battle's idle camera (battleCameraStartRandom, as the
  battle main loop calls it) picks a grid member every ~200 frames: a close-up of Pikachu (frame 250), the trainer
  behind it (450). `PC_GFX_ROOM_CAM_HANDOFF=1` hands the room's authored camera to the controller (cameraSetGScamera, as
  floor.c does when a room loads); without it the controller keeps its start-up perspective and the sky lies beyond the
  far plane (it drew black; verified by toggling the hand-off: with it, sky and clouds draw).
  `PC_GFX_ROOM_EYE="x y z"` (with `--room`; optional `PC_GFX_ROOM_AT="x y z"`, default 50 10 70, and
  `PC_GFX_ROOM_FOV=deg`): the room's own camera placed by hand instead of running its authored animation, keeping the
  room's lights, to match a Dolphin view (visual check 18). Outskirt Stand's authored camera is perspective 30 degrees,
  near 0.1, far 32768 (printed).
  `PC_GFX_ROOM_ANIM="set:frame,..."` (with `--room`): start those room model sets' animation 0 at that frame, rate 0.5.
  With the Dolphin frames for S1_out (set 4 = 452, set 1 = 94) the sky and the swinging sign match Dolphin's
  s1_out_arrival frame (2026-10-16; set 4 is the sky dome / cloud ring, verified by moving its frame).
  `--room-game <ROOM.dat>`: the room loaded the way the game's floor loader does it (gs_floor.c `fn_8010147C`
  registers the archive, `fn_801013A0` loads each model into its own resource). Outskirt Stand's map loads (5 models)
  with the HSD heap intact (checked after every allocation and free, 2026-10-16).
  `--battle-cam`: the game's camera controller (camera.c units) started as main.c does (`cameraInit`, before the
  Pokemon load, which already calls it) and stepped every frame (`cameraUpdate`), so a move's own camera work drives
  the view. The test has no floor table (a zero count stands in). Verified 2026-10-16 (frame dumps, Thunderbolt on
  Outskirt Stand's battle stage): a close-up of Pikachu charging (frame 20), a front view as it leaps in the electric
  field (45), a side close-up as it fires (80); not yet compared with the original.
  With `--seq` the camera controller is always started (the move code calls into it); `--battle-cam` lets it drive the
  view. `--wzx2-delay N` starts the target's half N frames after the attacker's (the battle's real timing is not yet
  known; `PC_GFX_WAZA_TIMES=1` prints the game's time points for the attacker's move, e.g. Thunderbolt on Pikachu:
  types 4 / 1 / 2 = 108 / 190 / 247 frames). Petal Dance with a delay of 108 or 190 runs 300 frames without the
  same-frame crash; its leaf clones still run short of model slots (18-58 failures; a Dolphin check).
  `--wzx2-after`: the target's half starts when the attacker's ends (the game's own "still playing" check,
  fn_801DA94C), as the battle's waiting move type functions do. Thunderbolt: the attack half ends at frame 124 and
  the damage half starts then; Petal Dance runs 400 frames. With `--battle-cam` the camera then cuts to the target:
  Charizard struck by the lightning (frame 150), recovering (200). The whole move plays with the game's own sequencing
  and camera (frame dumps; not yet compared with the original).
  `PC_GFX_MODEL_COUNT=1` prints how many of GSmodel's 64 pool slots are in use every 20 frames. Petal Dance's attack
  half alone fills all 64 at frame 0 (three leaf source models, then up to 32 clones per leaf effect), so some leaf
  clones fail with the game's own pool size; likely the same in the original (the Dolphin check will tell). Fixed on the way: the WZX effect parser (tracefx.c fn_801364A8 and helpers) read the big-endian
  parameters natively (now TFX_* / DISC_*; its parsed record is native), the particle entries' node slot (+0x8C) and
  the camera's view of the sequence record (+0x24 / +0x2C) were read as 8-byte pointers.
  `PC_GFX_MODULATE="r g b a"` (with `--model`) tints every material through the game's material extension
  (`GSmodelEnableModulation`): Pikachu comes out red with "255 64 64 255" (verified). `PC_GFX_COLORSWAP="r g b a"`
  sets the colour-swap extension (`GSmodelEnableColorSwap`, a TEV swap table): "2 1 0 3" turns Pikachu cyan with blue
  cheeks (verified). `PC_GFX_ENVMAP="mode type blend"` (with `--rtt`) applies the third extension, an environment map
  (`GSmodelEnableEnvMap`), with a solid green GStexture: in mode 0 it multiplies, and Pikachu renders green (verified;
  this runs the GStexture -> image description -> HSD texture path with the project's GameCube-layout GStextureHandle). Do not
  use the RTT texture itself as the env map: it starts black and is rendered with the env map, so it stays black.
  `--backfb` (with `--model`) runs the back-framebuffer capture the fades use: it captures at frame 5, then hides the
  model and draws the captured frame as a half-size quad. Found with it: the game copies the EFB at the start of the
  next frame (GSgfxBackFBDoFrame, from fn_800D3410), which on the PC is a fresh render pass, so the capture was black;
  on the PC the copy now happens at the end of each screen frame (GSgfxBackFBCopyPC), the same image (verified).
  Also: textured GSgfx quads need flag 2 (`gfxSetFlags(3); gfxClearFlags(4)`, as the game's own quads do).
  Pitfall, fixed: a frame dump taken while Aurora was still compiling a new shader pipeline came out
  black (Aurora skips draws until the pipeline is ready); whenever a dump or readback is requested the test now calls
  `AuroraSetDrawsBlocking(true)`, and new configurations render on the first run (verified).
  `--dl` captures the triangle into a GSgfx display list (GSgfx writes the GX bytes itself) and draws the capture each
  frame: the frame is byte-identical to `--draw` (verified). Found with it: the capture wrote its 16- / 32-bit values
  natively, so on the PC Aurora read the vertex count 3 as 768 and crashed (fixed with GS_DL_BE16 / GS_DL_BE32).
  `--col <room .col> --at X Z ...` runs a room's collision file through the game's PC path (a layer pushed with
  `fn_8010D064`, the file made current with `fn_8010CFE4`, which converts it to native order once) and prints the floor
  query every walker uses (`GScolsys2WalkGetHeight`, plus `GScolsys2WalkGetLayer`) at each point. Checked against the extracted
  `.obj` (decoded by the Python tools, not the game's C): Outskirt Stand 200 / 200 random points give the same floor
  heights (verified). Sweep over all 124 rooms (2026-10-15), against a Python re-implementation of the game's query (grid cell, bounding box, one-winding edge test, plane height) on the original big-endian file: every one of the 122 rooms with a floor group matches at 200 random points each (24,400 queries, verified); the two colosseum rooms (M2_earth_colo, T1_ancient_colo) have no floor group. A plain brute-force reference disagrees in 34 rooms because the game accepts a floor triangle only in one winding: reversed triangles in the floor group never count. Pitfall found on the way: the push clears the
  current file, so the two calls must be separate statements (as arguments of one call GCC ran them right to left).

## Building

The HSD units are compiled with the same forced headers as the PC link: `port/src/sdk_pc_alias.h`,
`include/pc/game_pc_alias.h`, then `include/pc/prototypes.h`. src/hsd's own declarations of SDK functions are hidden on
the PC behind Aurora's, so a build without the alias headers fails (implicit declarations).
Re-verified 2026-10-15 19:30 after the PC prototype work: Pikachu and Outskirt Stand render as before.

Paths and tools as in `docs/port/setup.md`. Aurora's dependencies can come from another build folder that already
fetched them (no new downloads): pass `-DFETCHCONTENT_FULLY_DISCONNECTED=ON` and one
`-DFETCHCONTENT_SOURCE_DIR_<NAME>=<that build>/_deps/<name>-src` per dependency (abseil-cpp, dawn_prebuilt, fmt,
freetype, googletest, imgui, nod_prebuilt, png, sdl3_prebuilt, sqlite3, tracy, xxhash, zlib).

```
cmake -S tests/pc_hsd -B build-pc/hsdtest -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-pc/hsdtest --target pc_hsd_test
build-pc/hsdtest/pc_hsd_test.exe <model archive .dat>
```

Generated inputs (game data or derived from it, never committed; regenerate into `build-pc/generated/`):
- `pc_hsd_data.c`: `python -I tools/local/port/pcdata.py --need <HSD's undefined data list> --out build-pc/generated`,
  then renamed. The symbols `hsd_data_pc.c` defines are left out of the list.
- `pc_hsd_stubs.c`: `python -I tools/local/port/pcstubs.py <undefined functions> -o build-pc/generated/pc_hsd_stubs.c`
  (logging stubs: each reports its first call in `missing_functions.log`).

## Files

| File | What |
|---|---|
| `main.c` | the test: platform start (as `orreport`), the game's arena / heap set-up, HSD init, load, report |
| `sdk_forward.c` | *Not built since 2026-10-15 evening.* The test now force-includes `port/src/sdk_pc_alias.h` and `include/pc/game_pc_alias.h` (as the PC link does), which map the `fn_<address>` SDK names to Aurora's directly; compiled with them, these forwarders would call themselves (found as a hang in `GXInit`). Kept for reference |
| `platform_gaps.c` | SDK functions Aurora lacks or only declares (VI frame buffer / retrace, copy clamp, draw-done wait, fog range table, the game's panic) |
| `hsd_data_pc.c` | HSD data that must be real C objects on the PC (class-info tables, the state-callback table, native structs) |
| `hsd_units.txt` | the 40 `src/hsd` units the GameCube build compiles |

## What the PC hit (the list the first boot will hit too)

1. The game's `main` calls `OSInitAlloc` before anything allocates; without it every OS heap allocation fails (HSD's
   included). Any PC start-up must keep that order.
2. `HSD_InitComponent` gives HSD the whole remaining arena (as on the GameCube): later loads go through HSD's heap
   (`HSD_MemAlloc`), not `OSAllocFromArenaLo`.
3. The base object class (`ObjInfoInit_801AA568`) used the GameCube's literal sizes (0x3C / 8): fixed to `sizeof`.
4. Class-info tables and other pointer-holding data can't be generated as bytes: they need C definitions with their
   types (`hsd_data_pc.c`); `pcdata.py` lists them as unsure.
5. Aurora declares but does not implement `GXSetCopyClamp`, `VIGetNextField`, `VISetPreRetraceCallback`,
   `VISetPostRetraceCallback`, and lacks `GXWaitDrawDone`, `GXInitFogAdjTable`.
6. `include/crt/math_ppc.h`'s extern inlines are emitted in every unit on the PC (multiple definitions; linked with
   `--allow-multiple-definition` for now).
8. **`GXSetArray` (fn_800B84E0):** the game passes (attr, data, stride); Aurora's PC version also needs the array's
   size in bytes (it uploads that many bytes to the GPU) and its byte order. Disc arrays are big-endian; the size is
   unknown to the game. The test uses "to the end of the archive's data block"; a port needs a real rule (the index
   range of the meshes using the array, computed once at load). With the 3-argument call forwarded to the
   5-argument function, nothing was drawn at all. *Done 2026-10-15:* `array_sizes.c` parses every mesh's display
   list at load (big-endian GX commands) and records (highest index + 1) x stride per array; the map aborted inside
   Aurora with the earlier "to the end of the archive" rule (hundreds of MB uploaded per frame).
10. **Aurora bug: `GXSetChanMatColor` / `GXSetChanAmbColor`** write the whole register for `GX_COLORn` and
    `GX_ALPHAn` alike; on the GameCube `GX_COLORn` sets only the RGB and `GX_ALPHAn` only the alpha. HSD sets the
    alpha channel's material colour (alpha 0) after the colour channel's, so every lit material came out black. The
    test merges the parts (`gx_trace.c`); the fix belongs in Aurora (`extern/aurora/lib/dolphin/gx/GXLighting.cpp`).
9. Pokemon models are small: Pikachu is about 3 units tall in joint space, and the whole model fits a camera 40
   units away with a 30-degree field of view.
7. Functions the GameCube linker dead-strips but dead code still names (`hsdIsDescendantOf`, `hsdForgetClassLibrary`,
   ...) must exist for a PC link.
