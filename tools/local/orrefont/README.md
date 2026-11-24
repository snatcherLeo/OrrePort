# OrreFont: building the font file

Builds **OrreFont Colosseum** (`port/ui/traced/fonts/OrreFont Colosseum.ttf`), the installable font of the game's
letters as OrrePort draws them: each letter traced sharp by the game's own tracer and written as smooth outlines.
Shipped in `Fonts\` next to the exe (`bin\Fonts\` in the portable layout) for players and modders to install, and for
Studio's previews later. The game's renderer never reads it: it traces the player's own disc at run time (part B,
`docs/port/font-renderer.md`). Built 2026-11-16 by the project's build helper; why it ships and why the name: the decisions log,
"OrreFont" (2026-11-15) and "The font files' names" (2026-11-16).

## Run it

```
python -I tools/local/orrefont/orrefont.py build --us "orig/Pokemon Colosseum (USA).iso" --jp "orig/Pokemon Colosseum (Japan).iso" --work <scratch folder>
```

Through `tools/local/saferun.py` like any heavy run (with MSYS2's `mingw64/bin` on PATH). It takes about two minutes
the first time (the glyph dump of both discs) and about 30 seconds after (the dump is reused from the work folder; the
trace itself takes under 10 s on 12 threads). `check --work <scratch>` re-runs only the checks.

Options: `--style regular|small|both` (default `regular`; `small` is the 18-px font, see "What it holds"),
`--keep-rare` (keep the 169 kanji the game draws from Noto, as the game's own picture traced), `--crop` (cut each letter
at its glyph box as the game's quad does), `--jobs N`, `--dest <folder>` (default `port/ui/traced/fonts/`),
`--no-check`.

**The work folder must be outside the repo** (the tool refuses one inside): it holds the glyph dump, the traces and
the specimen sheets, all made of the disc's letters. Everything in it regenerates from the discs with the command
above. The font itself is the only output that goes into the repo.

**Why `python -I` works:** fontTools (4.68.0; snatcherLeo approved it) and Pillow are in the user's site-packages, which `-I`
hides; `orrefont.py` adds that one folder to its path explicitly (`site.getusersitepackages()`), so the rest of the
isolation holds (no stray modules from the current folder or `PYTHON*` variables).

## How it works (verified: the build, the checks and the specimen, 2026-11-16)

1. **The glyph dump** (`dump` in `orrefont.py`; `glyphs.bin` and `glyphs.json` in the work folder): every glyph of both discs' fonts, read as the kanji review reads them
   (`tools/local/kanji_trace_check.gather`: the fonts in `main.dol`, then every font file of resource type 6), with
   each glyph's vertical offset: 5,751 glyphs on the US disc, 5,095 on the Japanese.
2. **The letters of a style:** the US disc's font for Latin letters and symbols, the Japanese disc's font of the same
   size for Japanese characters (the game's own split, `is_japanese` in `pc_text_atlas.cpp`: U+3000-30FF,
   U+3400-9FFF, U+FF00-FFEF), each falling back to the other disc. The space is not a glyph (the game never looks it
   up).
3. **The trace** (`orrefont_trace.cpp`, built by the script with MSYS2's g++): it `#include`s the game's own
   `port/src/pc_text_atlas.cpp`, as the kanji review's harness does, so the shapes are the exact code the game runs:
   `trace` for Latin letters at its 8x working detail, `trace_cjk` for Japanese characters at 16x, with the
   character passed (as `OrreHook_GlyphCode` does), so the review's valley gaps, the 22-px rule and the rare-kanji
   rule all apply. The Latin picture gets two blank pixels around it first, so its trace comes out whole (the tracer
   crops to the glyph's box, the game's quad); every letter is checked to give the game's own picture byte for byte
   inside the box (all 3,386 did).
4. **The outlines:** the 0/1 trace lightly blurred (0.8 working pixel) and contoured at half (marching squares), then
   corners found (a turn over 40 degrees within 3 working pixels), straight runs between corners made lines (within
   0.5 working pixel), the rest fitted with cubic Bezier curves to within 0.6 working pixel (Schneider's method,
   with one tangent at each split, so the curves join smoothly), and near-upright or near-level lines made exact.
   The cubics become TrueType's quadratic curves with fontTools' `cu2qu` (within one font unit). Why this way: the
   trace is a sharp 0/1 picture whose edges step by a working pixel; fitting smooth curves to its blurred contour keeps
   its shape to a few hundredths of a game pixel while dropping the steps (no lumpy edges).
5. **The font** (fontTools' `FontBuilder`, TrueType outlines): the metrics below; names; the generated-from-disc
   marker in a private table (`ORRE`) nothing reads, so the commit hook can refuse a copy anywhere but
   `port/ui/traced/fonts/`. Fixed date stamps: a rebuild from the same discs gives the same bytes (verified, `cmp`).
6. **The checks** (`check`): fontTools reads the file back and every table compiles again; no glyph runs the wrong way;
   the names hold none of the words the file must not use; then every letter is drawn by FreeType (through Pillow)
   at 8x and compared with the game's own trace at 8x (`trace8.bin`, what part B draws at a drawing scale of 8).
   Writes `specimen_<style>_set.png` (the font set in text) and `specimen_<style>_compare.png` (letters side by side
   with the game's trace, and over each other) to the work folder.

## What it holds (OrreFont Colosseum, verified 2026-11-16)

- **3,220 glyphs, about 4 MB:** Latin letters, digits, punctuation, the accented letters and symbols of the US
  dialogue font (font 0, 22 px; 172), the kana (163), CJK punctuation and full-width forms (82) and **2,800 kanji**
  from the Japanese disc's 22-px fonts (*corrected 2026-11-16:* "and `jis_1`" was wrong: `jis_1` is the **US** disc's
  kanji font, 21-px cell, `docs/engine/fonts.md`; the kanji here are the Japanese disc's own), plus the space, the no-break space and `.notdef`.
- **Left out on purpose: the 169 rare kanji** the game draws from Noto Sans CJK (the rare-kanji rule,
  font-renderer.md "The kanji"), so the file holds only the game's own design. `--keep-rare` keeps them as the game's
  own pictures traced (mostly grey blocks: graded C or D in the review).
- **The game's own code points,** as the game's text uses them (fonts.md "Special symbols"): `$` is the Pokedollar,
  `\` the yen sign (also reachable as U+00A5), `α` / `β` the e-Reader marks, `‰` two low dots, `‱` the speech bubble,
  `□` the boxed S, `＾` a small ring. So a modder's text previews exactly; someone typing `$` gets the Pokedollar.
- **Metrics:** one game pixel = 100 units, so the em (2,200) is the font's 22-pixel cell: at 22 px the font draws
  each letter at the game's own size. The baseline is the game's (the bottom of its 'H', 22 px below the line's top),
  the line is the game's (33 px: ascender 2,200, descender -1,100, no extra gap; `USE_TYPO_METRICS` on), every
  advance is the glyph's width in the game (the kanji 23 px, as on the Japanese disc), the space half the cell (11 px,
  as `msgSetChar`), and **no kerning** (the game has none: a letter advances by its own width; fonts.md).
- **Unhinted** (a `gasp` table asks for smoothing at every size): fine for large sizes and today's screens; very
  small sizes on Windows may look soft.
- **Names:** family "OrreFont Colosseum", style Regular, version 1.000, maker "OrrePort", a copyright line that
  makes no claim to the original designs and names no company, and a description saying the shapes were traced by
  OrrePort from a player's own copy. Embedding allowed (`fsType` 0).
- **How close (verified, every letter, FreeType at 8x against the game's trace at 8x):** edges within **0.02-0.03 of a
  game pixel** on median (Latin 0.030, kana 0.021, kanji 0.019), 99 % of letters within 0.10, the worst 0.108 (ー);
  mean picture difference 1-3 of 255.
- **The small style** (`--style small`, font 1, 18 px; built and checked, not shipped): 1,340 glyphs, 1.1 MB, edges
  within 0.05 px on median; but its Japanese side has only 916 kanji (the 18-px fonts' set, 38 of them the 22-px
  shape fitted in), so as a second family it would be a partial font. Not worth shipping now (the helper's advice to
  Main); the tool keeps the option.

## The licence line (decided 2026-11-16)

The font's name table carries a licence line (name ID 13, `LICENSE` in `orrefont.py`):

> Free to use in personal and fan projects, and to share unchanged. Not to be sold. Provided as is, without warranty
> of any kind.

**Why it's there** (snatcherLeo, 2026-11-16: "A, just include documentation on why it's in there in the first place"):
- **Without a line, people have to guess.** Someone who installs the font, or sees it in a mod, can't tell what
  they're allowed to do with it. One line answers that.
- **Why not the SIL Open Font License,** the usual licence for free fonts: it grants rights to the letter designs
  themselves, and those designs aren't OrrePort's to grant. OrrePort made the outlines by tracing; the letters' design
  belongs to the game's makers. The copyright line says so ("No claim is made to the original letter designs").
- **Why "not to be sold":** snatcherLeo's standing rule that nothing derived from the game is ever sold. His Patreon plan pays
  for his time and tools only, never for game content.
- **Why "share unchanged":** a modified copy passed around under the same name would be mistaken for OrrePort's own.
  Anyone changing the letters can make their own font from them, under another name.
- **Embedding** is allowed (`fsType` 0), so a mod's pictures or a document can use it.
- **For XD** (snatcherLeo, the same day: "We'd do the same for OrrePort XD, when the time came"): OrreFont XD gets the same
  licence line, for the same reasons.

## Platforms

The Python is the same everywhere. The tracer build uses MSYS2 MinGW and `build-pc/_deps` (SDL3, FreeType, libpng,
zlib) on Windows; on Linux or macOS any C++20 compiler with SDL3 and FreeType from their install (likely: `build_tracer`
would need their paths). The finished `.ttf` installs the same way on Windows, Linux and macOS.

## For XD (OrreFont XD)

The same tool builds OrreFont XD from XD's discs (decided names: xd-insights.md 3b). What carries over (likely):
everything here (the dump, the tracer harness, the vectoriser, the font writer, the checks). What would differ: XD's
glyphs must be found on its discs (`kanji_trace_check.gather` reads `main.dol` and type-6 font files; a guess that XD's
are in the same places), the rare-kanji rule's constants re-measured on XD's kanji font, and "used by the text" taken
from XD's Japanese text. The Latin letters would come out identical (XD's dialogue font is the same pictures, verified).
