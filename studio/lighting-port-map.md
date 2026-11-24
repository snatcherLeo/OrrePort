# How Colosseum lights a battle model: a port map for Studio

*Copied into `studio/` 2026-11-19, as the reference for Studio's in-game lighting (release step 2). Private (Studio's planning). Scratchpad paths below (`scratchpad/...`, `allm/...`) were the research session's working files and are not kept.*

Research only (2026-11-19). Paths are relative to the repo root (`pkmn_colosseum`); line numbers are from
Studio's branch as of that date unless marked "the project's checkout". Labels: **verified** = read in code (or in data / a
real log), **likely** = strongly implied, **guess** = unconfirmed.

The short version: lighting is GX's fixed-function **per-vertex** lighting. HSD sets up two colour channels, **COLOR0**
(diffuse, lit by the "diffuse" lights) and **COLOR1** (specular, lit by the "specular" lights with a half-vector trick).
Both use a **white material register**, so the channels carry only the light; the material's own diffuse and specular
colours are multiplied in later, in the TEV (the per-pixel combiner), together with the textures. The lights come from
the battle room's map file (`scene_data` +0x08), and the move viewer uses the Phenac Colosseum arena's lights
(`M1_water_colo.dat`); the full list is in section 5.

---

## 1. Render mode -> GX channel setup

Code: `HSD_SetupChannelMode` in `src/hsd/state.c:88-173` (channel tables at `state.c:64-85`; a second, identical copy is in
`src/hsd/hsd_texp.c:656-743`). `HSD_SetupChannel` (`src/hsd/hsd_texp.c:761-845`) caches state and calls
`GXSetChanAmbColor`, `GXSetChanMatColor` and `GXSetChanCtrl`. It is called from `HSD_SetupRenderModeWithCustomPE` (`state.c:232-240`), the
last step of `HSD_MObjSetup` (`src/hsd/mobj.c:696-732`). Material colours are stored first by `HSD_SetMaterialColor`
(`state.c:248-259`: ambient, diffuse and specular as RGBA8, alpha clamped to 0..1 then ×255) and `HSD_SetMaterialShininess`
(`state.c:262`). **All verified.**

Render-mode bits (`include/hsd/hsd_mobj.h:50-88`): bits 0-1 the diffuse source (0 MAT0 = same as 1 MAT, 2 VTX, 3 BOTH),
bit 2 `RENDER_DIFFUSE` (lit), bit 3 `RENDER_SPECULAR`, bits 4-11 TEX0-7, bit 12 TOON, bits 13-14 the alpha source (0
COMPAT = "same as the diffuse source", 1 MAT, 2 VTX, 3 BOTH), bit 26 SHADOW, 27 ZMODE_ALWAYS, 29 NO_ZUPDATE, 30 XLU.

`HSD_Chan` field order (`include/hsd/hsd_tev.h:49-62`): chan, flags, amb_color, mat_color, enable, amb_src, mat_src,
light_mask, diff_fn, attn_fn. GX enums: amb/mat src 0 = REG, 1 = VTX; diff_fn 0 NONE, 1 SIGN, 2 CLAMP; attn_fn 0 SPEC, 1
SPOT, 2 NONE.

| Case | Channel | enable | amb src / colour | mat src / colour | light mask | diff fn | attn fn |
|---|---|---|---|---|---|---|---|
| `RENDER_SPECULAR` | COLOR1 (`spec_chan`) | on | REG, black (0,0,0,0) | REG, **white** | `HSD_LObjGetLightMaskSpecular()` | NONE | **SPEC** |
| `RENDER_DIFFUSE` | COLOR0 (`diffuse_chan`) | on | REG, **mat.ambient × ambient light colour / 255** (`HSD_MulColor`, `src/hsd/util.c:15`, integer per component), or black if there is no active ambient light with the DIFFUSE flag | **REG white** for MAT, **VTX** for VTX / BOTH (`mat_src = (color_mode >> 1) & 1`) | diffuse mask | **CLAMP** | **SPOT** |
| `RENDER_DIFFUSE`, alpha | ALPHA0, or ALPHA1 when the alpha source has the VTX bit (`alpha_chan`) | on only if the alpha mask is non-zero | REG, alpha = ambient light's alpha if it has the ALPHA flag (0x10), else 0 | REG; alpha 0xFF if lit, else = that ambient alpha | alpha mask | CLAMP | SPOT |
| `RENDER_DIFFUSE` + vertex alpha | ALPHA0 (`vtx_alpha_chan`) | off | - | **VTX** (raw vertex alpha) | - | - | - |
| not `RENDER_DIFFUSE` | COLOR0 (`unlit_color_chan`) | off | - | REG white, or VTX for VTX / BOTH | - | NONE | NONE |
| not `RENDER_DIFFUSE` | ALPHA0 (`unlit_alpha_chan`) | off | - | REG alpha 0xFF, or VTX if the alpha source has the VTX bit | - | NONE | NONE |

Channel count (`state.c:161-172`): 2 when specular or vertex alpha is used (the unused half, ALPHA1 or COLOR1, is
switched off through `fn_801B3AE8`, **likely** "disable channel"), otherwise 1.

**Where the material colours go (verified, `mobj.c:385-643`):** the material register of the lit channels is
white, so GX's `material × light` is just the light (or the vertex colour × light for VTX / BOTH). The material's
**diffuse** colour, **alpha** and **specular** colour become **TEV constants** (`HSD_TExpCnst`), multiplied in by the
TEV (section 4). The material's **ambient** colour only scales the ambient light (above).

**Two Colosseum quirks (verified):**
- `MObjLoad` ORs `RENDER_TOON` into **every** material (`mobj.c:341`; the texlog shows `mrm=0x00003015` with bit 12
  set), but the toon texture global `tobj_toon` is never assigned: only `mobj.c` touches it, and only to clear it
  (`MObjAmnesia`, `mobj.c:871`); the retail disassembly references it only from `mobj.s`. So the toon branches
  (`mobj.c:470-487, 552-558, 672-677, 719-724`) never run: **no toon ramp in Colosseum**.
- The specular colour stage happens only when the material has `RENDER_SPECULAR`; the lit specular channel is COLOR1.

## 2. HSD_LObj -> GX lights

Code: `src/hsd/lobj.c` (`HSD_LObjSetup` 652-748, `setup_*_lightobj` 524-649, `HSD_LObjSetupSpecular` 428-461,
`HSD_LObjSetupSpecularInit` 464-522, `LObjLoad` 1074-1118). Flag bits: `include/hsd/hsd_forward.h:183-198`.
GX initialisers: `src/dolphin/sdk_range_800BA1B4.c` (`GXInitLightSpot`, `GXInitLightDistAttn`),
`src/dolphin/gx/GX_fn_800BA424.c` (`GXInitLightDir` **stores the negated vector**), the shininess macro at `lobj.c:69-71`.
**All verified.**

**Flags:** bits 0-1 kind (0 ambient, 1 infinite, 2 point, 3 spot); bit 2 DIFFUSE, bit 3 SPECULAR, bit 4 ALPHA (a light
with none of the three is skipped); bit 5 HIDDEN; bit 6 RAW_PARAM (the attenuation block holds raw GX a0-a2 / k0-k2;
set when the description's attnflags bit 0 is set); bits 7-8 "needs re-upload" (diffuse / specular).

**Slot assignment (`HSD_LObjSetup`, called by `GSlightSetupLights(cobj)` before each model draw pass,
`src/game/gs_light.c:1084-1111`, `src/game/gs_model_main.c:144, 189`):**
1. Walk the current list (sorted by priority, ties in insertion order) and skip hidden lights and lights with no
   DIFFUSE / SPECULAR / ALPHA flag. Each non-ambient light takes the next GX light slot (`GX_LIGHT0`, bit 0x01, up to
   `GX_LIGHT7`, 0x80). The **first ambient light** takes the special index 8 (id 0x100) and no GX slot.
2. A light with DIFFUSE or ALPHA gets its diffuse light object (colour = light colour) and joins the **diffuse mask**
   (if DIFFUSE) and the **alpha mask** (if ALPHA); point and spot lights also join the "attn-func mask", which only
   the particle code reads (`src/game/psdisp.c:1556`, `ps_range_80168C64.c:1340`), not the model channels. A light with
   only SPECULAR uses its own slot as its specular slot.
3. Afterwards, every light with **both** DIFFUSE (or ALPHA) and SPECULAR gets a **second** GX slot for its specular
   object, taken from the slots left over, in active order, while fewer than 8 slots are used. That slot joins the
   **specular mask**. With 6 diffuse lights, only the first 2 get specular slots (exactly what the move viewer shows;
   section 5).
4. `HSD_LObjSetupSpecular` uploads each light (`GXLoadLightObjImm`).

**Space:** everything is in **view space**: positions go through the camera's view matrix (`PSMTXMultVec`),
directions through its rotation (`PSMTXMultVecSR`).

| Kind | Diffuse light object | Specular light object |
|---|---|---|
| **Infinite** (`setup_infinite_lightobj`, 575-591) | position = view × (world position × 2^20), a very far point, so the light comes **from the direction of its position as seen from the world origin** (its interest is not used for diffuse); attenuation a = (1,0,0), k = (1,0,0) (no falloff) | the same far position (used only for the back-face test); direction = half vector (below) |
| **Point** (`setup_point_lightobj`, 593-615) | position = view × world position; RAW: a = (1,0,0), k = (k0,k1,k2); else `GXInitLightDistAttn(ref_dist, ref_br, dist_func)` + `GXInitLightSpot(0, 0)` (a = (1,0,0)) | same position and distance attenuation, then overwritten by the shininess attenuation |
| **Spot** (`setup_spot_lightobj`, 617-649) | position as point; direction = view rotation of normalize(interest − position) (stored negated by `GXInitLightDir`); RAW: a0-a2, k0-k2 as given; else ref_dist clamped to ≥ 0.001, `GXInitLightDistAttn`, `GXInitLightSpot(cutoff, spot_func)` | as point |
| **Ambient** | not a GX light: its colour × the material ambient becomes COLOR0's ambient register (section 1); its alpha feeds the alpha channel if it has the ALPHA flag | - |

**GX attenuation helpers (verified, `sdk_range_800BA1B4.c`):**
- `GXInitLightDistAttn(d, br, fn)`: if d < 0 or br ∉ (0,1): off. Gentle: k = (1, (1−br)/(br·d), 0); medium:
  k = (1, 0.5(1−br)/(br·d), 0.5(1−br)/(br·d²)); steep: k = (1, 0, (1−br)/(br·d²)); off: (1,0,0).
- `GXInitLightSpot(cutoff°, fn)`: if cutoff ≤ 0 or > 90: off; c = cos(cutoff). Flat: a = (−1000c, 1000, 0); cos:
  a1 = 1/(1−c), a0 = −c·a1, a2 = 0; cos2: a2 = 1/(1−c), a1 = −c·a2, a0 = 0; sharp: d = 1/(1−c)², a = (c(c−2)d, 2d, −d);
  ring1: d as sharp, a2 = −4d, a0 = a2·c, a1 = 4(1+c)d; ring2: a0 = 1 − 2c²d, a1 = 4cd, a2 = −2d; off: (1,0,0).

**Specular (half-angle) setup (verified):**
- `setup_spec_lightobj` (549-573): colour = light colour; shininess 50 (default); `lvec` = view-space light
  position (point / spot) or view rotation of normalize(interest − position) (infinite; with no interest WObj this is
  normalize(−position)).
- `HSD_LObjSetupSpecularInit(pmtx)` (464-522) runs **per joint** that has `JOBJ_SPECULAR` (bit 16), from
  `HSD_JObjDispDObj` (`src/hsd/displayfunc.c:460-463`, not in the shadow pass): cdir = normalize(the joint's
  view-space translation) (eye → joint; (0,0,−1) if zero). Infinite: half = lvec + cdir; point / spot: ldir =
  normalize(jointPos − lightPos), half = ldir + cdir; normalized ((0,0,1) if degenerate). `GXInitLightDir` stores
  −half, which is the true Blinn half vector H = normalize(toLight + toEye). So H is **constant per joint** (joints
  without `JOBJ_SPECULAR` keep the last joint's H: an edge case).
- Shininess: `HSD_SetupChannelMode` calls `HSD_LObjSetupSpecular(light, colour, material shininess)` for every active
  light when the material has `RENDER_SPECULAR` (`state.c:108-118`), so the **material's** shininess s is used:
  `GXInitLightShininess` = attenuation a = (0, 0, 1), k = (s/2, 0, 1 − s/2) (`lobj.c:69-71`).

**Light-mask meaning:** the masks are **GX light-ID bit sets** (bit n = `GX_LIGHTn`): diffuse mask → COLOR0's light
mask, specular mask → COLOR1's, alpha mask → the lit alpha channel's.

## 3. GX's per-vertex lighting formula (for the shader)

From Dolphin's `VideoCommon/LightingShaderGen.cpp` and `VertexShaderGen.cpp` (read online 2026-11-19), which mirror
the hardware; **likely** exact for the integer steps. Everything below is in view space, **per vertex**; the result
is interpolated across the triangle (Gouraud), not computed per pixel.

    P = view-space position (position matrix applied, before projection)
    N = normalize(normal matrix × raw normal)        // Dolphin normalizes; real hardware: guess, likely not renormalized
    for each channel (COLOR0 rgb, ALPHA0 a, COLOR1 rgb, ...):
      mat  = enable-independent material: vertex colour (src VTX) or the register (src REG), in 0..255
      if !enable: out = mat
      lacc = ambient: vertex colour (amb src VTX) or the ambient register
      for each light i in the channel's mask:
        attn function:
          SPOT: v = Lpos_i − P; d = |v|; L = v / d
                cosA = max(0, dot(L, Ldir_i))                       // Ldir_i = the stored (negated) direction
                attn = max(0, a0 + a1·cosA + a2·cosA²) / (k0 + k1·d + k2·d²)
          SPEC: L = normalize(Lpos_i − P)
                x = dot(N, L) >= 0 ? max(0, dot(N, Ldir_i)) : 0      // Ldir_i = H, the half vector
                kk = (diff fn == NONE) ? k : normalize(k)             // k used as is for HSD's NONE
                attn = max(0, a0 + a1·x + a2·x²) / (kk0 + kk1·x + kk2·x²)
          NONE: attn = 1, L = normalize(Lpos_i − P)
        diffuse function:
          NONE:  lacc += round(attn · colour_i)
          SIGN:  lacc += round(attn · dot(L, N) · colour_i)
          CLAMP: lacc += round(attn · max(0, dot(L, N)) · colour_i)
      lacc = clamp(lacc, 0, 255)
      out  = (mat · (lacc + (lacc >> 7))) >> 8                        // ≈ mat × lacc / 255
    rasterized colour (RAS) = out / 255, interpolated

What HSD's setup turns this into (**verified** from sections 1-2; the formula **likely**):
- **COLOR0 (diffuse):** lit = clamp(ambReg + Σ_diffuse colour_i · attn_i · max(0, N·L_i)) × (white, or the vertex
  colour for VTX / BOTH). With ambReg = mat.ambient × ambientLight / 255. For infinite lights attn = 1 and L_i =
  view-rotated normalize(world position). Point lights: attn = 1 / (k0 + k1·d + k2·d²) (a = (1,0,0)); spot lights add
  the cone term.
- **COLOR1 (specular):** spec = clamp(Σ_specular colour_i · x²/(s/2 + (1 − s/2)·x²)) with x = max(0, N·H_i), and 0 when
  the vertex faces away from the light (N·L < 0). (k0 + k2·x² with k1 = 0 and a = (0,0,1): the "shininess" curve; at
  s = 50, x = 1 → 1, x = 0.98 → about 0.49.) Distance falloff is not applied (the shininess macro overwrites k).
- **Normals:** the normal matrix is the **inverse transpose of the model-view matrix** (`HSD_MtxInverseTranspose`
  of view × joint (rigid, `src/hsd/pobj.c:1347-1351`), of view × the blended envelope matrix (skinned,
  `pobj.c:1488-1492`)). It is loaded only when the joint has `JOBJ_LIGHTING` (bit 7, `pobj.c:1302, 1349, 1490`).
  Reflection / highlight texture coordinates get the same matrix as `GX_TEXMTX0` (`pobj.c:1353-1355`).
- **Studio shortcut (likely safe):** the view matrix is a rigid rotation + translation, so the dot products can be done
  in world space: diffuse L = normalize(lightWorldPos·2^20 − P) ≈ normalize(lightWorldPos) for infinite lights;
  spec H per joint = normalize(−dir(interest−position) + normalize(eye − jointWorldPos)). Do it in the vertex shader
  and interpolate, with 8-bit clamps, to match the game's per-vertex look.

## 4. Texture stages and the TEV order

Code: `MObjMakeTExp` (`src/hsd/mobj.c:385-643`), `TObjMakeTExp` (`src/hsd/tobj.c:1280-1405`), `MakeColorGenTExp`
(`tobj.c:986-1278`), volatile stages `HSD_TObjSetupVolatileTev` (`tobj.c:968-984`); compiled by `HSD_TExpCompile`
(`mobj.c:680`) into TEV stages. Battle models use GS's material class "GSmaterial MObj" (the texlog's `class=gs`),
whose `_matGSmatObjMakeTExp` (`src/game/gs_material_800DF498.c:275-306`) **calls HSD's `MObjMakeTExp` first** and
then optionally appends the shiny stages. **Verified.**

TEV arithmetic: each stage = d ± ((1−c)·a + c·b), clamped to 0..1 (every HSD stage here has clamp on). Below, "prev" =
the running colour (or alpha), T = the texture colour, Ta = its alpha.

**Colour ops (bits 16-19, `TObjMakeTExp`, verified):** 0 NONE and 6 PASS: prev; 1 ALPHA_MASK: lerp(prev, T, Ta);
2 RGB_MASK: lerp(prev, T, T) (per channel); 3 BLEND: lerp(prev, T, blending); 4 MODULATE: prev × T; 5 REPLACE: T;
7 ADD: prev + T; 8 SUB: prev − T.
**Alpha ops (bits 20-23):** 0 NONE and 5 PASS: prev; 1 ALPHA_MASK: lerp(prev, Ta, Ta); 2 BLEND: lerp(prev, Ta,
blending); 3 MODULATE: prev × Ta; 4 REPLACE: Ta; 6 ADD: prev + Ta; 7 SUB: prev − Ta. A stage that serves a second
lighting group (it has two lightmap bits) does not apply its alpha op again (`repeat`, `tobj.c:1286, 1352`).
A TObj TEV block (`tobj->tev`, Studio's `has_tev`) first replaces T / Ta with its own stage result (`MakeColorGenTExp`).

**Order of the expression for a lit material (`RENDER_DIFFUSE`), verified:**
1. **Base:** colour = material diffuse (konst), or 1 for DIFFUSE_VTX; alpha = material alpha, or 1 for vertex alpha.
2. **Diffuse group:** every TObj with lightmap bit 4 (DIFFUSE) **or bit 6 (AMBIENT)**, in list order, applies its
   colour / alpha op to that base. (Ambient-mapped textures are folded in with the diffuse ones; there is no separate
   ambient path.)
3. *(vertex alpha only)* a stage with RAS = COLOR1A1: alpha = alpha × (1 − ALPHA1), the lit alpha (0 when no alpha
   lights, so unchanged).
4. **Lighting:** colour = colour × RAS(COLOR0); alpha = alpha × (1 − RAS.a) for non-vertex alpha (ALPHA0 is 0 when no
   alpha lights, so unchanged) or alpha × RAS.a (ALPHA0 = vertex alpha) for vertex alpha.
5. **Specular** (`RENDER_SPECULAR`): spec = material specular (konst); every TObj with bit 5 (SPECULAR) applies its op
   to spec; spec = spec × RAS(COLOR1); colour = colour + spec (clamped).
6. **Ext group:** every TObj with bit 7 (EXT) applies its op to the lit colour (and alpha): reflection / env maps
   added or blended **after** lighting.
7. A final pass-through stage if needed.
8. **Volatile stages:** emboss bump (`SetupEmbossBumpTev`, `tobj.c:936-965`, for TObjs with bit 24): adds T(coord)
   then subtracts T(coord+1), coord+1 being GX's `GX_TG_BUMPn` shift toward the first light in the diffuse mask
   (`setupTextureCoordGenBump`, `tobj.c:862-884`); projected shadows (`TObjSetupTevModulateShadow`, `tobj.c:910-934`,
   for a TObj with bit 8 SHADOW, or `RENDER_SHADOW` with the global shadow list): colour × T.
9. **GS shiny stages** (only on materials with GS extensions): channel swap (ext bit 2, swap tables on every TEV
   stage) and modulation (ext bit 1): colour = 2 × modulate.rgb × colour (scale 2), alpha = modulate.a × alpha
   (scale 1; 0x7F means "unchanged" for colour; the alpha half is **guess**: check it against a shiny shot).

For an **unlit** material (no `RENDER_DIFFUSE`) the base is: MAT: konst diffuse; VTX: RAS(COLOR0) = raw vertex colour;
BOTH: konst × vertex colour; alpha the same way (`mobj.c:458-519`); then steps 2, 6, 7 (no lighting, no specular).

**The common case (one texture on the diffuse group, MODULATE, plus specular):**

    colour = clamp( clamp( matDiffuse × T ) × litDiffuse + matSpecular × litSpecular )
    alpha  = matAlpha × (alpha op with Ta)
    (then ext stages, then shiny modulation)

where litDiffuse / litSpecular are the interpolated per-vertex results of section 3. Survey note (Studio's design log): many
Pokémon use "BLEND with factor 1" on the first stage, which is just T (the texture becomes the diffuse colour before
lighting); second stages are mostly ext-channel reflection maps.

**Texture coordinates that depend on lights (verified, `tobj.c:636-860`):**
- **Highlight** (coordinate kind 2): normal-based texgen (`GX_TG_NRM`, `GX_TEXMTX0` = the inverse-transpose normal
  matrix, normalized), then a post-matrix built from the **first infinite light in the current list**
  (`HSD_LObjGetCurrentByType(LOBJ_INFINITE)`): ĥ = normalize(view-rotated normalize(interest − position) + (0,0,−1)),
  which is −H (H = the Blinn half vector for a viewer looking down −z). Then u = m00·(0.5 + 0.5·N·H) + m03 and
  v = m10·(0.5 + 0.5·N·H) + m13 (m = the stage's own texture matrix), so the texture is looked up along N·H.
  Without an infinite light, a zero matrix (u = v = 0).
- **Reflection** and **gradation**: normal-based, no lights. **Shadow**: position-based projection. **Backlight**:
  position-based, identity-like matrix. **Toon**: dead (section 1).

## 5. Where the lights come from

**In general (verified):** GS keeps a pool of lights (`GSlightInit`, 0x74 bytes each, `gs_light.c:1021-1035`). Each
model draw pass calls `GSlightSetupLights(cobj)` (`gs_light.c:1084-1111`), which chains every pool light that is
allocated **and active** in pool order and hands them to HSD (`HSD_LObjAddCurrentAll`, then `HSD_LObjSetup`).

- **Rooms, battles included:** a room's map archive (`scene_data` +0x08, a null-terminated list of light sets
  {`HSD_LightDesc*`, light-animation list}) is loaded with `GSlightLoad` (`gs_light.c:905-948`, called from
  `field_range_80114AE0.c:2153`) and switched on by `floorInitMap` (`src/game/floor.c:1060-1074`), which also starts
  light animation 0 at rate 0.5 if it has one. Scripts can toggle them (`std190_lightSetActive`,
  `src/game/gs_range_801C766C.c:1432-1441`). A battle runs in its arena's room (menu-rooms.md: "every battle: the arena
  record's room is pushed"), so **battle lighting = the arena map's light list** (likely; the move viewer below is the
  verified instance of the same path). No light comes from the Pokémon model itself.
- **Menus** build their own (`menuModelInit`, `src/game/gs_range_exact_menuModelInit.c`; `gs_range_80109C88.c:973-1094`):
  `GSlightCreate` makes a diffuse-only (flags 4) grey-128 ambient description, retyped with `GSlightSetType`
  (an ambient + two point lights from a float table at `lbl_80271F38`). Those point lights have no attenuation (their
  parameter block stays zero → `GXInitLightDistAttn` "off") and no specular flag. Not used by the battle or the viewer.

**The move viewer (room 0x3E7) (verified):** its archive `waza_viewer.fsys` holds the room module, the arena map
**`M1_water_colo.dat`** (the Phenac Colosseum stage) and its particle file. The logged lights
(`ORRE_WAZA_VIEWER_LIGHTS=1`, a real run's log in this scratchpad's `allm/exe/logs/orreport_20261119_152827.log`) match
that file's `scene_data` light list exactly (decoded from the disc copy with `scratchpad/scenelights.py`). Every light's
animation entry is empty (no colour or position keys), so the set is **static**:

| # (HSD active index) | Kind | Flags | Colour (RGB) | World position | Extra | GX slot(s) in the viewer |
|---|---|---|---|---|---|---|
| ambient (index 8) | ambient | 0x04 diffuse | 76, 76, 76 | - | - | none (feeds COLOR0's ambient: mat.ambient × 76/255; with the usual ambient 0x80 that is 38) |
| 0 | infinite | 0x0D diffuse + specular | 128, 128, 128 | (68.5441, 171.2361, −91.5254) | interest none | diffuse 0x01, specular 0x40 |
| 1 | infinite | 0x0D | 128, 128, 128 | (−88.5074, 171.2361, 98.6520) | interest none | diffuse 0x02, specular 0x80 |
| 2 | infinite | 0x0D | 51, 51, 51 | (266.6534, 171.2361, 302.9531) | | diffuse 0x04 (no specular slot left) |
| 3 | infinite | 0x0D | 51, 51, 51 | (−268.9288, 171.2361, −315.4289) | | diffuse 0x08 (no specular slot) |
| 4 | point | 0x0E diffuse + specular | 0, 0, 0 | (−15.1620, 291.7086, 13.6458) | ref_br 0.99, ref_dist 10000, steep | diffuse 0x10 (contributes nothing) |
| 5 | point | 0x0E | 87, 92, 92 | (2.8264, −62.5905, −3.0874) | ref_br 0.99, ref_dist 60, steep → k = (1, 0, 2.806e−6) | diffuse 0x20 (a fill light from **below** the floor) |

Masks in the log: diffuse 0x3F, specular 0xC0, alpha 0x00. The infinite lights' description pointer holds 16.0 (in
the shininess slot), which `LObjLoad` ignores; the specular objects use the **material's** shininess. The viewer's
camera (default preset): eye (−40, 44, 70), interest (−40, 14, 0) (or the logged aim), fov from the log; the model
at (−40, 0, 0) turned +90° about y. For the specular H, cdir is from the eye to each `JOBJ_SPECULAR` joint.

Worked example (likely, from the formulas): an upward normal gets 38 + 106.5 + 101.2 + 19.9 + 19.5 ≈ 285 → clamped to
255 (fully lit); a downward normal near the model gets about 38 + 87·0.83·0.98 ≈ 109 from the under-floor fill light.

For "in-game lighting" in Studio: use this table as the default (it is the move viewer's and the Phenac Colosseum
battle's lighting); other arenas would need their map's own list (same reader, different file).

## 6. What Studio has and what it would need

Checked in `studio/core/include/orre_core.h:204-268`, `studio/core/src/hsd_model.cpp`, `studio/win32/viewer.cpp`.

- **Normals: present** (`OrreVertex.nrm`, GX_VA_NRM = attribute 10, `hsd_model.cpp:555-557`), and posed with the skin
  matrix (`hsd_model.cpp:1862-1883`, with w = 0). The game uses the **inverse transpose** of view × skin matrix; the
  same as Studio's only for rotation + uniform scale. Worth renormalizing per vertex after skinning (likely what
  Dolphin's output looks like). **Gap: meshes whose normals come from GX_VA_NBT (attribute 25)** are sized but not
  stored (`hsd_model.cpp:525` lists 25; no branch keeps it), so their `nrm` stays zero (likely: check whether any
  Pokémon mesh has NBT and no NRM; bump-mapped meshes are the candidates).
- **Binormal / tangent: missing.** Needed for emboss bump (`GX_TG_BUMPn` shifts the second coordinate by the light
  direction projected on tangent and binormal) (likely).
- **Per-joint `JOBJ_LIGHTING` (bit 7) and `JOBJ_SPECULAR` (bit 16)**: needed to know which joints have a normal matrix
  and a specular half vector; check that Studio keeps the joint flags per mesh.
- **Materials: present** (diffuse, ambient, specular, alpha, shininess, render_mode, stage flags with the lightmap
  bits). The ambient colour is needed for the ambient register (it is read).
- **The lights:** a static table (section 5) for the move viewer / Phenac Colosseum; or a reader for any arena map's
  `scene_data` +0x08 (layout in `include/hsd/hsd_lobj.h`; the decoder logic in `scratchpad/scenelights.py`).
- **Per-vertex evaluation:** Studio's shader lights per pixel with one fixed light (`viewer.cpp:257-270`); the game is
  per vertex (Gouraud) with up to 8 lights and 8-bit clamps.
- **Toon ramp: not needed** (dead in Colosseum, section 1).
- **Shadows:** projected shadows only fall on receivers the game lists (the floor, `models-animation.md` "Shadows");
  a Pokémon model is not normally a receiver (likely), so a model-only view can skip them.
- **Shiny modulation:** the GS stage (section 4, item 9) applies after everything else.

## Open questions

1. The alpha half of GS's shiny modulation stage (scale 1 with modulate.a = 0x7F would halve alpha): check against a
   shiny shot (guess).
2. Real hardware vs Dolphin on normal renormalization (Dolphin normalizes; hardware likely doesn't). Only matters for
   scaled joints.
3. Which joints of a Pokémon carry `JOBJ_SPECULAR`; for joints without it the game reuses the previous joint's half
   vector (draw order dependent).
4. Battle arenas other than Phenac Colosseum: same mechanism (likely), different light lists; not decoded here.
5. Whether any Pokémon mesh stores its normal only in NBT (affects lit meshes with bump maps).
